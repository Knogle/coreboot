#!/usr/bin/env python3
"""Emit MSI MINIT PHY descriptors or read them from a live X58 scan chain.

The live mode only selects a channel and issues scan-chain read commands.  It
never writes PHY payload data.  Descriptor input is one ``index start width``
triple per line, with hexadecimal starts accepted.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import struct
import sys


DESCRIPTOR_COUNT = 0x23A
DESCRIPTOR_VA = 0xFFFD9C11
DATA_VA = 0xFFFD9220
DATA_FILE_OFFSET = 0x17420
CHAIN_BASE = 0x162A
LAST_FIELD_BIT = 0x1603
SELECT_OFFSET = 0x5C
COMMAND_OFFSET = 0xF8
DATA_OFFSET = 0xFC
CHANNEL_MASK = 3 << 25
ANY_BUSY = (1 << 31) | (1 << 30)
READ_BUSY = 1 << 31
POLL_LIMIT = 100_000


def read32(fd: int, offset: int) -> int:
    payload = os.pread(fd, 4, offset)
    if len(payload) != 4:
        raise RuntimeError(f"short PCI configuration read at 0x{offset:x}")
    return struct.unpack("<I", payload)[0]


def write32(fd: int, offset: int, value: int) -> None:
    if os.pwrite(fd, struct.pack("<I", value), offset) != 4:
        raise RuntimeError(f"short PCI configuration write at 0x{offset:x}")


def wait_clear(fd: int, mask: int) -> int:
    for polls in range(1, POLL_LIMIT + 1):
        value = read32(fd, COMMAND_OFFSET)
        if not value & mask:
            return polls
    raise RuntimeError(f"PHY scan busy timeout: mask=0x{mask:08x}")


def emit_descriptors(path: Path) -> None:
    payload = path.read_bytes()
    table = DATA_FILE_OFFSET + DESCRIPTOR_VA - DATA_VA
    end = table + DESCRIPTOR_COUNT * 3
    if end > len(payload):
        raise RuntimeError("MINIT image is too short for the descriptor table")
    for index in range(DESCRIPTOR_COUNT):
        start, width = struct.unpack_from("<HB", payload, table + index * 3)
        if width < 2 or width > 30 or start + width - 1 > LAST_FIELD_BIT:
            raise RuntimeError(
                f"invalid descriptor {index}: start=0x{start:x} width={width}"
            )
        print(f"{index} 0x{start:04x} {width}")


def parse_descriptors() -> list[tuple[int, int, int]]:
    descriptors = []
    for line_number, line in enumerate(sys.stdin, 1):
        stripped = line.partition("#")[0].strip()
        if not stripped:
            continue
        fields = stripped.split()
        if len(fields) != 3:
            raise RuntimeError(f"descriptor input line {line_number} is malformed")
        index, start, width = (int(field, 0) for field in fields)
        if width < 2 or width > 30 or start + width - 1 > LAST_FIELD_BIT:
            raise RuntimeError(f"invalid descriptor on input line {line_number}")
        descriptors.append((index, start, width))
    if not descriptors:
        raise RuntimeError("no descriptors supplied on stdin")
    return descriptors


def dump(config: Path, channel: int) -> None:
    descriptors = parse_descriptors()
    fd = os.open(config, os.O_RDWR | os.O_CLOEXEC)
    original_select = None
    try:
        wait_clear(fd, ANY_BUSY)
        original_select = read32(fd, SELECT_OFFSET)
        selected = (original_select & ~CHANNEL_MASK) | channel << 25
        write32(fd, SELECT_OFFSET, selected)
        if read32(fd, SELECT_OFFSET) != selected:
            raise RuntimeError("PHY channel selector readback mismatch")
        for index, start, width in descriptors:
            field_end = start + width - 1
            command = READ_BUSY | (CHAIN_BASE - field_end)
            write32(fd, COMMAND_OFFSET, command)
            polls = wait_clear(fd, READ_BUSY)
            raw = read32(fd, DATA_OFFSET)
            value = raw & ((1 << (width - 2)) - 1)
            print(
                json.dumps(
                    {
                        "index": index,
                        "start": start,
                        "width": width,
                        "value": value,
                        "raw": raw,
                        "polls": polls,
                    },
                    separators=(",", ":"),
                ),
                flush=True,
            )
    finally:
        if original_select is not None:
            wait_clear(fd, ANY_BUSY)
            write32(fd, SELECT_OFFSET, original_select)
        os.close(fd)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--emit-descriptors", type=Path, metavar="MINIT_PE")
    mode.add_argument("--dump", type=Path, metavar="PCI_CONFIG")
    parser.add_argument("--channel", type=int, choices=range(3), default=2)
    args = parser.parse_args()
    try:
        if args.emit_descriptors:
            emit_descriptors(args.emit_descriptors)
        else:
            dump(args.dump, args.channel)
    except (OSError, RuntimeError, ValueError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
