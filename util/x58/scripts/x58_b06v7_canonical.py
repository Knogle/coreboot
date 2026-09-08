#!/usr/bin/env python3
"""Compute the vendor-free B06V7 Slow-QPI canonical digests.

The input files are user-supplied ROMMON snapshots.  This tool does not
contain, extract, or redistribute vendor firmware bytes.  Canonicalization
only changes a temporary copy used for hashing; raw evidence is also reported.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
from typing import Iterable


CSI_SIZE = 0x304
CSI_DYNAMIC_OFFSET = 0x2A6
CSI_DYNAMIC_VALUES = frozenset((0x08, 0x0C))
CSI_CANONICAL_FNV = 0x8403AC98

WORKSPACE_SIZE = 0x2BCC
WORKSPACE_DYNAMIC_RANGES = (
    (0x132C, 0x136D),
    (0x13BE, 0x13FD),
    (0x1859, 0x185C),
    (0x18E7, 0x18E9),
    (0x1AA4, 0x1AA7),
    (0x23A6, 0x23A6),
    (0x2411, 0x2434),
    (0x24A1, 0x24C4),
    (0x251F, 0x252E),
)
WORKSPACE_CANONICAL_FNV = 0x7A34F363


def fnv1a32(data: bytes | bytearray) -> int:
    digest = 0x811C9DC5
    for value in data:
        digest ^= value
        digest = (digest * 0x01000193) & 0xFFFFFFFF
    return digest


def canonical_digest(data: bytes, ranges: Iterable[tuple[int, int]]) -> int:
    canonical = bytearray(data)
    previous_last = -1
    for first, last in ranges:
        if first < 0 or last < first or first <= previous_last or last >= len(data):
            raise ValueError(f"invalid canonical range 0x{first:x}..0x{last:x}")
        canonical[first : last + 1] = bytes(last - first + 1)
        previous_last = last
    return fnv1a32(canonical)


def analyze_csi(data: bytes) -> tuple[int, int, int, bool]:
    if len(data) != CSI_SIZE:
        raise ValueError(f"CSI size 0x{len(data):x}, expected 0x{CSI_SIZE:x}")
    raw = fnv1a32(data)
    dynamic = data[CSI_DYNAMIC_OFFSET]
    canonical = canonical_digest(data, ((CSI_DYNAMIC_OFFSET, CSI_DYNAMIC_OFFSET),))
    accepted = dynamic in CSI_DYNAMIC_VALUES and canonical == CSI_CANONICAL_FNV
    return raw, dynamic, canonical, accepted


def analyze_workspace(data: bytes) -> tuple[int, int, bool]:
    if len(data) != WORKSPACE_SIZE:
        raise ValueError(
            f"workspace size 0x{len(data):x}, expected 0x{WORKSPACE_SIZE:x}"
        )
    raw = fnv1a32(data)
    canonical = canonical_digest(data, WORKSPACE_DYNAMIC_RANGES)
    return raw, canonical, canonical == WORKSPACE_CANONICAL_FNV


def print_csi(path: Path) -> bool:
    data = path.read_bytes()
    raw, dynamic, canonical, accepted = analyze_csi(data)
    print(f"CSI path: {path}")
    print(f"CSI SHA-256: {hashlib.sha256(data).hexdigest()}")
    print(f"CSI raw FNV-1a-32: {raw:08x}")
    print(f"CSI byte 0x{CSI_DYNAMIC_OFFSET:03x}: {dynamic:02x}")
    print(f"CSI canonical FNV-1a-32: {canonical:08x}")
    print(f"CSI Slow-QPI gate: {'PASS' if accepted else 'FAIL'}")
    return accepted


def print_workspace(path: Path) -> bool:
    data = path.read_bytes()
    raw, canonical, accepted = analyze_workspace(data)
    print(f"workspace path: {path}")
    print(f"workspace SHA-256: {hashlib.sha256(data).hexdigest()}")
    print(f"workspace raw FNV-1a-32: {raw:08x}")
    print(f"workspace canonical FNV-1a-32: {canonical:08x}")
    print(f"workspace gate: {'PASS' if accepted else 'FAIL'}")
    return accepted


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--csi", type=Path, help="raw 0x304-byte CSI snapshot")
    parser.add_argument(
        "--workspace", type=Path, help="raw 0x2bcc-byte MINIT workspace snapshot"
    )
    parser.add_argument(
        "--require-gate",
        action="store_true",
        help="return failure unless every supplied snapshot passes its B06V7 gate",
    )
    args = parser.parse_args()
    if args.csi is None and args.workspace is None:
        parser.error("supply --csi and/or --workspace")

    try:
        results = []
        if args.csi is not None:
            results.append(print_csi(args.csi))
        if args.workspace is not None:
            results.append(print_workspace(args.workspace))
    except (OSError, ValueError) as error:
        parser.error(str(error))

    return 1 if args.require_gate and not all(results) else 0


if __name__ == "__main__":
    raise SystemExit(main())
