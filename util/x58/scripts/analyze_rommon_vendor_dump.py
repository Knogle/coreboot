#!/usr/bin/env python3
"""Reconstruct and validate a B06 ROMMON vendor-state hex dump.

The ROMMON and automatic MINIT observation path emit lines such as::

    [VENDOR] WORK 02a0:0053000a...
    [RAMINIT] WORK[02a0]=00 53 00 0a ...

Commands may be split over several serial transactions.  This tool rejects
gaps, overlaps with conflicting data, malformed records, and unexpected sizes
before reporting hashes suitable for an immutable bring-up record.
"""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path


VENDOR_LINE_RE = re.compile(
    rb"^\[VENDOR\]\s+(CSI|POLICY|WORK)\s+([0-9a-fA-F]{4,8}):"
    rb"([0-9a-fA-F]+)\s*$"
)
RAMINIT_LINE_RE = re.compile(
    rb"^\[RAMINIT\]\s+(CSI|WORK)\[([0-9a-fA-F]{4,8})\]="
    rb"([0-9a-fA-F]{2}(?:[ \t]+[0-9a-fA-F]{2})*)[ \t]*$"
)
RAMINIT_PREFIX_RE = re.compile(rb"^\[RAMINIT\]\s+(CSI|WORK)\[")


def fnv1a32(data: bytes) -> int:
    value = 0x811C9DC5
    for byte in data:
        value ^= byte
        value = (value * 0x01000193) & 0xFFFFFFFF
    return value


def parse_dump(raw: bytes, kind: str) -> tuple[bytes, int, int]:
    records: dict[int, int] = {}
    matched_lines = 0
    duplicate_bytes = 0

    for line_number, line in enumerate(
        raw.replace(b"\r", b"").splitlines(), start=1
    ):
        match = VENDOR_LINE_RE.fullmatch(line)
        spaced_hex = False
        if match is None:
            match = RAMINIT_LINE_RE.fullmatch(line)
            spaced_hex = match is not None
            if match is None:
                prefix = RAMINIT_PREFIX_RE.match(line)
                if (
                    prefix is not None
                    and prefix.group(1).decode("ascii") == kind
                ):
                    raise ValueError(
                        f"malformed [RAMINIT] {kind} record at line "
                        f"{line_number}"
                    )
        if match is None or match.group(1).decode("ascii") != kind:
            continue
        matched_lines += 1
        offset = int(match.group(2), 16)
        hex_data = match.group(3)
        if spaced_hex:
            hex_data = b"".join(hex_data.split())
        if len(hex_data) % 2:
            raise ValueError(f"odd hex length at offset 0x{offset:x}")
        data = bytes.fromhex(hex_data.decode("ascii"))
        for index, value in enumerate(data, offset):
            old = records.get(index)
            if old is not None:
                if old != value:
                    raise ValueError(
                        f"conflicting byte at 0x{index:x}: "
                        f"0x{old:02x} versus 0x{value:02x}"
                    )
                duplicate_bytes += 1
            records[index] = value

    if not records:
        raise ValueError(f"no {kind} dump records found")
    if min(records) != 0:
        raise ValueError(f"dump starts at 0x{min(records):x}, not zero")

    end = max(records) + 1
    missing = [index for index in range(end) if index not in records]
    if missing:
        preview = ", ".join(f"0x{index:x}" for index in missing[:8])
        suffix = " ..." if len(missing) > 8 else ""
        raise ValueError(f"{len(missing)} missing bytes: {preview}{suffix}")

    return bytes(records[index] for index in range(end)), matched_lines, duplicate_bytes


def parse_int(value: str) -> int:
    return int(value, 0)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("dump", type=Path)
    parser.add_argument("--kind", choices=("CSI", "POLICY", "WORK"), required=True)
    parser.add_argument("--size", type=parse_int)
    parser.add_argument("--expect-fnv", type=parse_int)
    parser.add_argument("--write-bin", type=Path)
    args = parser.parse_args()

    try:
        data, line_count, duplicate_bytes = parse_dump(
            args.dump.read_bytes(), args.kind
        )
    except ValueError as error:
        parser.error(str(error))

    if args.size is not None and len(data) != args.size:
        parser.error(f"size 0x{len(data):x}, expected 0x{args.size:x}")

    fnv = fnv1a32(data)
    if args.expect_fnv is not None and fnv != args.expect_fnv:
        parser.error(f"FNV-1a {fnv:08x}, expected {args.expect_fnv:08x}")

    if args.write_bin:
        args.write_bin.write_bytes(data)

    print(f"kind: {args.kind}")
    print(f"records: {line_count}")
    print(f"duplicate bytes: {duplicate_bytes}")
    print(f"coverage: 0x0000..0x{len(data) - 1:04x} (0x{len(data):x} bytes)")
    print(f"FNV-1a-32: {fnv:08x}")
    print(f"SHA-256: {hashlib.sha256(data).hexdigest()}")
    if args.write_bin:
        print(f"binary: {args.write_bin}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
