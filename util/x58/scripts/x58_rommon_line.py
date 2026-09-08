#!/usr/bin/env python3
"""Send bounded line commands to the X58 CAR or DRAM ROMMON."""

from __future__ import annotations

import argparse
import os
import pathlib
import re
import select
import sys
import termios
import time


PROMPT = re.compile(rb"(?:rommon(?:\[[A-Z]+\])?|x58-dram|x58-usb)> $")
PROMPT_MODES = {
    "any": PROMPT,
    "rommon": re.compile(rb"rommon(?:\[[A-Z]+\])?> $"),
    "dram": re.compile(rb"x58-dram> $"),
    "usb": re.compile(rb"x58-usb> $"),
}


def configure(fd: int) -> None:
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0
    attrs[4] = termios.B115200
    attrs[5] = termios.B115200
    attrs[6][termios.VMIN] = 0
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)


def prepare_command_channel(fd: int, preserve_input: bool = False) -> None:
    """Discard stale command output before transmitting a new command.

    A prior writer can leave bytes in the tty/USB transmit path when it is
    interrupted.  TCIFLUSH alone does not remove those bytes.  Normal command
    sessions therefore clear both directions; callers which deliberately
    retain boot output clear only the transmit queue.
    """

    selector = termios.TCOFLUSH if preserve_input else termios.TCIOFLUSH
    termios.tcflush(fd, selector)


def receive_prompt(fd: int, timeout: float, prompt: re.Pattern[bytes] = PROMPT) -> bytes:
    deadline = time.monotonic() + timeout
    data = bytearray()
    while time.monotonic() < deadline:
        ready, _, _ = select.select((fd,), (), (), 0.05)
        if not ready:
            continue
        try:
            chunk = os.read(fd, 4096)
        except BlockingIOError:
            continue
        if chunk:
            data.extend(chunk)
            if len(data) > 1024 * 1024:
                raise RuntimeError("response exceeded 1 MiB")
            if prompt.search(data):
                return bytes(data)
    raise TimeoutError(f"prompt timeout; partial response: {bytes(data[-256:])!r}")


def send_command(
    fd: int,
    command: str,
    byte_delay: float,
    timeout: float,
    prompt: re.Pattern[bytes] = PROMPT,
) -> bytes:
    wire = command.encode("ascii") + b"\r"
    for value in wire:
        os.write(fd, bytes((value,)))
        if byte_delay:
            time.sleep(byte_delay)
    termios.tcdrain(fd)
    return receive_prompt(fd, timeout, prompt)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", nargs="+")
    parser.add_argument("--device", default="/dev/ttyUSB1")
    parser.add_argument("--timeout", type=float, default=3.0)
    parser.add_argument("--byte-delay", type=float, default=0.001)
    parser.add_argument(
        "--keep-input",
        action="store_true",
        help=(
            "preserve queued received bytes; stale pending transmit bytes "
            "are still discarded before the first command"
        ),
    )
    parser.add_argument(
        "--expect-prompt",
        choices=tuple(PROMPT_MODES),
        default="any",
        help="prompt class required after each command (use usb for B06WF)",
    )
    parser.add_argument(
        "--log",
        type=pathlib.Path,
        help="create an immutable transcript and fsync after each response",
    )
    args = parser.parse_args()

    log = None
    if args.log is not None:
        try:
            log = args.log.open("xb")
        except OSError as error:
            print(f"error: cannot create transcript: {error}", file=sys.stderr)
            return 1

    fd = None
    try:
        fd = os.open(args.device, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        configure(fd)
        prepare_command_channel(fd, preserve_input=args.keep_input)
        for command in args.command:
            response = send_command(
                fd, command, args.byte_delay, args.timeout,
                PROMPT_MODES[args.expect_prompt],
            )
            if log is not None:
                log.write(response)
                log.flush()
                os.fsync(log.fileno())
            sys.stdout.buffer.write(response)
            sys.stdout.buffer.flush()
    except (OSError, RuntimeError, TimeoutError, UnicodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    finally:
        if fd is not None:
            os.close(fd)
        if log is not None:
            log.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
