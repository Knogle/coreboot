#!/usr/bin/env python3
"""Run a loaded X58 ROMMON script and dispose its transaction safely.

The executor sends no command after a prompt timeout.  Successful programs
containing only reversible writes are rolled back before their trace is
collected; read-only programs are discarded directly.  Every response is
flushed to an immutable host transcript as it arrives.
"""

from __future__ import annotations

import argparse
import dataclasses
import fcntl
import os
import pathlib
import re
import sys
import termios

import x58_rommon_script as script_format
from x58_rommon_line import configure, prepare_command_channel, send_command


STATUS_RE = re.compile(
    rb"\[SCRIPT\] FORMAT=01 OPS=([0-9a-f]{2}) "
    rb"PROGRAM_FNV=([0-9a-f]{8}) SEALED=([01]{2}) "
    rb"TXN_VALID=([01]{2}) TRACE=([0-9a-f]{2}) "
    rb"TXN_FNV=([0-9a-f]{8}) LAST=([a-z0-9_-]+) "
    rb"MUT=([0-9a-f]{2}) NONREV=([0-9a-f]{2}) "
    rb"RB_AVAILABLE=([01]{2}) RB_RESULT=([a-z0-9_-]+) "
    rb"RB_ATTEMPTED=([01]{2}) RB_TRIES=([0-9a-f]{8}) "
    rb"ROLLED_BACK=([01]{2})"
)


class ExecutorError(RuntimeError):
    """A fail-closed executor or target-validation failure."""


@dataclasses.dataclass(frozen=True)
class Status:
    op_count: int
    program_digest: int
    sealed: bool
    transaction_valid: bool
    trace_count: int
    transaction_digest: int
    last: str
    mutations: int
    nonreversible: int
    rollback_available: bool
    rollback_result: str
    rollback_attempted: bool
    rollback_tries: int
    rolled_back: bool


def parse_expected_digest(value: str) -> int:
    try:
        return script_format.parse_hex(value)
    except script_format.ScriptError as error:
        raise argparse.ArgumentTypeError(str(error)) from error


def parse_last_status(response: bytes) -> Status:
    matches = list(STATUS_RE.finditer(response))
    if not matches:
        raise ExecutorError("ROMMON did not return a complete script status")
    fields = matches[-1].groups()
    return Status(
        op_count=int(fields[0], 16),
        program_digest=int(fields[1], 16),
        sealed=fields[2] == b"01",
        transaction_valid=fields[3] == b"01",
        trace_count=int(fields[4], 16),
        transaction_digest=int(fields[5], 16),
        last=fields[6].decode("ascii"),
        mutations=int(fields[7], 16),
        nonreversible=int(fields[8], 16),
        rollback_available=fields[9] == b"01",
        rollback_result=fields[10].decode("ascii"),
        rollback_attempted=fields[11] == b"01",
        rollback_tries=int(fields[12], 16),
        rolled_back=fields[13] == b"01",
    )


def require_loaded_clean(status: Status, op_count: int, digest: int) -> None:
    if status.op_count != op_count or status.program_digest != digest:
        raise ExecutorError("loaded ROMMON program differs from validated source")
    if not status.sealed:
        raise ExecutorError("loaded ROMMON program is not sealed")
    if status.transaction_valid:
        raise ExecutorError("ROMMON has a retained transaction")


def require_success(status: Status, op_count: int, digest: int,
                    mutation_count: int) -> None:
    if status.op_count != op_count or status.program_digest != digest:
        raise ExecutorError("completed ROMMON program identity changed")
    if not status.sealed or not status.transaction_valid:
        raise ExecutorError("successful ROMMON run did not retain a transaction")
    if status.trace_count != op_count or status.last != "ok":
        raise ExecutorError("ROMMON run did not complete every operation")
    if status.mutations != mutation_count or status.nonreversible != 0:
        raise ExecutorError("ROMMON mutation accounting differs from source")
    if mutation_count and not status.rollback_available:
        raise ExecutorError("reversible writes completed without rollback data")
    if not mutation_count and status.rollback_available:
        raise ExecutorError("read-only program unexpectedly offers rollback")


def require_rolled_back(status: Status, op_count: int, digest: int,
                        mutation_count: int) -> None:
    if status.op_count != op_count or status.program_digest != digest:
        raise ExecutorError("ROMMON program identity changed during rollback")
    if not status.transaction_valid or status.trace_count != op_count:
        raise ExecutorError("ROMMON discarded evidence during rollback")
    if status.mutations != mutation_count or status.nonreversible != 0:
        raise ExecutorError("ROMMON rollback mutation accounting changed")
    if status.rollback_available or not status.rollback_attempted:
        raise ExecutorError("ROMMON rollback did not consume rollback data")
    if status.rollback_result != "ok" or not status.rolled_back:
        raise ExecutorError("ROMMON rollback readback failed")


def require_discarded(status: Status, op_count: int, digest: int) -> None:
    if status.op_count != op_count or status.program_digest != digest:
        raise ExecutorError("ROMMON program identity changed during discard")
    if status.transaction_valid or status.trace_count != 0:
        raise ExecutorError("ROMMON transaction was not fully discarded")


def checked_send(fd: int, command: str, byte_delay: float,
                 timeout: float) -> bytes:
    response = send_command(fd, command, byte_delay, timeout)
    if b"\r\nERR " in response or response.startswith(b"ERR "):
        raise ExecutorError(f"ROMMON rejected command: {command}")
    return response


class Transcript:
    def __init__(self, path: pathlib.Path):
        self._stream = path.open("xb")

    def append(self, response: bytes) -> None:
        self._stream.write(response)
        self._stream.flush()
        os.fsync(self._stream.fileno())
        sys.stdout.buffer.write(response)
        sys.stdout.buffer.flush()

    def close(self) -> None:
        self._stream.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("--expected-fnv", required=True,
                        type=parse_expected_digest)
    parser.add_argument("--device", default="/dev/ttyUSB1")
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--byte-delay", type=float, default=0.001)
    parser.add_argument("--lock-file", type=pathlib.Path,
                        default=pathlib.Path("/tmp/x58-uart.lock"))
    parser.add_argument("--log", required=True, type=pathlib.Path)
    args = parser.parse_args()

    try:
        operations = script_format.parse_script(args.source)
    except (OSError, script_format.ScriptError) as error:
        parser.error(str(error))
    digest = script_format.program_digest(operations)
    if digest != args.expected_fnv:
        parser.error(
            f"validated digest {digest:08x} differs from --expected-fnv "
            f"{args.expected_fnv:08x}"
        )
    mutation_count = sum(
        op.kind in (script_format.KIND["write"], script_format.KIND["mask"])
        for op in operations
    )
    if any(
        op.kind in (script_format.KIND["write"], script_format.KIND["mask"])
        and not op.flags & script_format.REVERSIBLE
        for op in operations
    ):
        parser.error("executor requires every mutation to be reversible")

    try:
        transcript = Transcript(args.log)
    except OSError as error:
        print(f"error: cannot create transcript: {error}", file=sys.stderr)
        return 1

    lock_fd = -1
    serial_fd = -1
    run_started = False
    disposed = False
    try:
        lock_fd = os.open(args.lock_file, os.O_RDWR | os.O_CREAT, 0o600)
        fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        serial_fd = os.open(args.device,
                            os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        fcntl.ioctl(serial_fd, getattr(termios, "TIOCEXCL", 0x540C))
        configure(serial_fd)
        prepare_command_channel(serial_fd)

        response = checked_send(serial_fd, "script status",
                                args.byte_delay, args.timeout)
        transcript.append(response)
        require_loaded_clean(parse_last_status(response), len(operations),
                             digest)

        response = checked_send(serial_fd, "unlock SCRIPT",
                                args.byte_delay, args.timeout)
        transcript.append(response)
        if b"[SCRIPT] ARMED" not in response:
            raise ExecutorError("ROMMON did not arm script execution")

        run_started = True
        response = checked_send(serial_fd,
                                f"script run {digest:08x} auto",
                                args.byte_delay, args.timeout)
        transcript.append(response)
        status = parse_last_status(response)
        require_success(status, len(operations), digest, mutation_count)

        if mutation_count:
            response = checked_send(serial_fd, "unlock SCRIPT",
                                    args.byte_delay, args.timeout)
            transcript.append(response)
            response = checked_send(
                serial_fd,
                f"script rollback {status.transaction_digest:08x}",
                args.byte_delay,
                args.timeout,
            )
            transcript.append(response)
            status = parse_last_status(response)
            require_rolled_back(status, len(operations), digest,
                                mutation_count)

        response = checked_send(serial_fd,
                                f"script trace 00 {len(operations):02x}",
                                args.byte_delay, args.timeout)
        transcript.append(response)

        response = checked_send(serial_fd, "unlock SCRIPT",
                                args.byte_delay, args.timeout)
        transcript.append(response)
        response = checked_send(
            serial_fd,
            f"script discard {status.transaction_digest:08x}",
            args.byte_delay,
            args.timeout,
        )
        transcript.append(response)
        require_discarded(parse_last_status(response), len(operations),
                          digest)
        disposed = True
    except (BlockingIOError, ExecutorError, OSError, TimeoutError) as error:
        print(f"error: {error}", file=sys.stderr)
        if run_started and not disposed:
            print("error: TARGET TRANSACTION MAY REQUIRE MANUAL ROLLBACK OR "
                  "COLD RESET; no further UART command was sent after the "
                  "failure", file=sys.stderr)
        return_code = 1
    else:
        print(f"EXECUTED ops={len(operations):02x} "
              f"program_fnv={digest:08x} mutations={mutation_count:02x} "
              "disposition=rolled-back-and-discarded"
              if mutation_count else
              f"EXECUTED ops={len(operations):02x} "
              f"program_fnv={digest:08x} mutations=00 "
              "disposition=discarded",
              file=sys.stderr)
        return_code = 0
    finally:
        if serial_fd >= 0:
            os.close(serial_fd)
        if lock_fd >= 0:
            os.close(lock_fd)
        transcript.close()
    return return_code


if __name__ == "__main__":
    raise SystemExit(main())
