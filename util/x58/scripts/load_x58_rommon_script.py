#!/usr/bin/env python3
"""Validate and load one sealed X58 ROMMON script over a local UART."""

from __future__ import annotations

import argparse
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
    rb"TXN_VALID=([01]{2})"
)


class LoaderError(RuntimeError):
    """A fail-closed loader or target-validation failure."""


def parse_expected_digest(value: str) -> int:
    try:
        return script_format.parse_hex(value)
    except script_format.ScriptError as error:
        raise argparse.ArgumentTypeError(str(error)) from error


def require_clean_transaction(response: bytes) -> None:
    matches = list(STATUS_RE.finditer(response))
    if not matches:
        raise LoaderError("ROMMON did not return a parseable script status")
    if matches[-1].group(4) != b"00":
        raise LoaderError("ROMMON has a retained transaction; inspect and discard it first")


def require_loaded_status(response: bytes, count: int, digest: int) -> None:
    matches = list(STATUS_RE.finditer(response))
    if not matches:
        raise LoaderError("ROMMON did not return a parseable final script status")
    match = matches[-1]
    if int(match.group(1), 16) != count:
        raise LoaderError("ROMMON operation count differs from validated source")
    if int(match.group(2), 16) != digest:
        raise LoaderError("ROMMON program digest differs from validated source")
    if match.group(3) != b"01" or match.group(4) != b"00":
        raise LoaderError("ROMMON script is not sealed or acquired a transaction")


def checked_send(fd: int, command: str, byte_delay: float,
                 timeout: float) -> bytes:
    response = send_command(fd, command, byte_delay, timeout)
    if b"\r\nERR " in response or response.startswith(b"ERR "):
        raise LoaderError(f"ROMMON rejected command: {command}")
    return response


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("--expected-fnv", required=True,
                        type=parse_expected_digest)
    parser.add_argument("--device", default="/dev/ttyUSB1")
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--byte-delay", type=float, default=0.001)
    parser.add_argument("--lock-file", type=pathlib.Path,
                        default=pathlib.Path("/tmp/x58-uart.lock"))
    parser.add_argument("--log", type=pathlib.Path)
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

    transcript = bytearray()
    lock_fd = os.open(args.lock_file, os.O_RDWR | os.O_CREAT, 0o600)
    serial_fd = -1
    try:
        fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        serial_fd = os.open(args.device,
                            os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        fcntl.ioctl(serial_fd, getattr(termios, "TIOCEXCL", 0x540C))
        configure(serial_fd)
        prepare_command_channel(serial_fd)

        response = checked_send(serial_fd, "script status",
                                args.byte_delay, args.timeout)
        transcript.extend(response)
        require_clean_transaction(response)

        commands = ["script clear"]
        commands.extend(script_format.operation_command(op)
                        for op in operations)
        commands.extend(("script seal", "script status", "script list"))
        for command in commands:
            response = checked_send(serial_fd, command,
                                    args.byte_delay, args.timeout)
            transcript.extend(response)

        require_loaded_status(transcript, len(operations), digest)
    except (BlockingIOError, LoaderError, OSError, TimeoutError) as error:
        print(f"error: {error}", file=sys.stderr)
        return_code = 1
    else:
        print(f"LOADED ops={len(operations):02x} program_fnv={digest:08x}",
              file=sys.stderr)
        return_code = 0
    finally:
        if serial_fd >= 0:
            os.close(serial_fd)
        os.close(lock_fd)
        if args.log is not None:
            args.log.write_bytes(transcript)
        sys.stdout.buffer.write(transcript)
        sys.stdout.buffer.flush()
    return return_code


if __name__ == "__main__":
    raise SystemExit(main())
