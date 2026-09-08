#!/usr/bin/env python3
"""Place a smaller firmware image at the top of a larger erased SPI image."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def integer(value: str) -> int:
    return int(value, 0)


def sha256(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="source firmware image")
    parser.add_argument("output", type=Path, help="generated full-chip image")
    parser.add_argument(
        "--flash-size",
        type=integer,
        required=True,
        help="target flash capacity in bytes (decimal or 0x-prefixed)",
    )
    parser.add_argument(
        "--expected-sha256",
        help="refuse the input unless its SHA-256 matches this value",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="replace an existing output file",
    )
    args = parser.parse_args()

    source = args.input.read_bytes()
    source_hash = sha256(source)
    expected_hash = args.expected_sha256.lower() if args.expected_sha256 else None

    if expected_hash and source_hash != expected_hash:
        parser.error(
            f"input SHA-256 is {source_hash}, expected {expected_hash}"
        )
    if args.flash_size <= 0 or args.flash_size & (args.flash_size - 1):
        parser.error("--flash-size must be a positive power of two")
    if len(source) > args.flash_size:
        parser.error("input is larger than the target flash")
    if args.output.exists() and not args.force:
        parser.error(f"output already exists: {args.output} (use --force to replace)")

    source_offset = args.flash_size - len(source)
    output = bytes([0xFF]) * source_offset + source
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(output)
    written = args.output.read_bytes()
    if written != output:
        parser.error("output readback differs from generated image")

    result = {
        "input": str(args.input),
        "input_size": len(source),
        "input_sha256": source_hash,
        "output": str(args.output),
        "output_size": len(written),
        "output_sha256": sha256(written),
        "fill_byte": "0xff",
        "source_offset": f"0x{source_offset:08x}",
        "source_end": f"0x{len(written) - 1:08x}",
    }
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
