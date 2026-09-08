#!/usr/bin/env python3
"""Read-only, dependency-free firmware image inventory.

The scanner deliberately reports structural facts only.  It does not extract
modules, assign semantics to unknown fields, or modify its input.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
from typing import Any


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def u64(data: bytes, offset: int) -> int:
    return struct.unpack_from("<Q", data, offset)[0]


def hex0(value: int) -> str:
    return f"0x{value:x}"


def scan_microcode(data: bytes) -> list[dict[str, Any]]:
    found: list[dict[str, Any]] = []
    # Capsule/FFS framing can place an update at only dword alignment.  Validate
    # the full dword checksum to reject accidental header-like byte patterns.
    for offset in range(0, len(data) - 48 + 1, 4):
        if u32(data, offset) != 1 or u32(data, offset + 20) != 1:
            continue
        data_size = u32(data, offset + 28)
        total_size = u32(data, offset + 32)
        if data_size == 0:
            data_size = 2000
        if total_size == 0:
            total_size = 2048
        if total_size < 48 or total_size % 4 or offset + total_size > len(data):
            continue
        words = struct.unpack_from(f"<{total_size // 4}I", data, offset)
        if sum(words) & 0xFFFFFFFF:
            continue
        found.append(
            {
                "offset": hex0(offset),
                "revision": hex0(u32(data, offset + 4)),
                "date_raw": hex0(u32(data, offset + 8)),
                "processor_signature": hex0(u32(data, offset + 12)),
                "processor_flags": hex0(u32(data, offset + 24)),
                "data_size": data_size,
                "total_size": total_size,
                "sha256": hashlib.sha256(data[offset : offset + total_size]).hexdigest(),
            }
        )
    return found


def scan_firmware_volumes(data: bytes) -> list[dict[str, Any]]:
    found: list[dict[str, Any]] = []
    start = 0
    while True:
        signature = data.find(b"_FVH", start)
        if signature < 0:
            break
        start = signature + 1
        base = signature - 0x28
        if base < 0 or base + 0x38 > len(data):
            continue
        length = u64(data, base + 0x20)
        header_length = u16(data, base + 0x30)
        revision = data[base + 0x37]
        if length < header_length or header_length < 0x38 or base + length > len(data):
            continue
        found.append(
            {
                "offset": hex0(base),
                "length": hex0(length),
                "header_length": hex0(header_length),
                "revision": revision,
            }
        )
    return found


def scan_pe_images(data: bytes) -> list[dict[str, Any]]:
    found: list[dict[str, Any]] = []
    start = 0
    while True:
        offset = data.find(b"MZ", start)
        if offset < 0:
            break
        start = offset + 1
        if offset + 0x40 > len(data):
            continue
        pe_offset = u32(data, offset + 0x3C)
        # A huge e_lfanew almost certainly belongs to an accidental MZ match.
        if pe_offset < 0x40 or pe_offset > 0x100000:
            continue
        nt = offset + pe_offset
        if nt + 24 > len(data) or data[nt : nt + 4] != b"PE\0\0":
            continue
        machine = u16(data, nt + 4)
        sections = u16(data, nt + 6)
        optional_size = u16(data, nt + 20)
        optional = nt + 24
        if optional + optional_size > len(data) or optional_size < 0x60:
            continue
        magic = u16(data, optional)
        if magic == 0x10B:
            image_base = u32(data, optional + 28)
        elif magic == 0x20B:
            image_base = u64(data, optional + 24)
        else:
            continue
        found.append(
            {
                "offset": hex0(offset),
                "machine": hex0(machine),
                "sections": sections,
                "pe_magic": hex0(magic),
                "entry_rva": hex0(u32(data, optional + 16)),
                "image_base": hex0(image_base),
                "size_of_image": hex0(u32(data, optional + 56)),
            }
        )
    return found


def inventory(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    return {
        "path": str(path),
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "microcode_updates": scan_microcode(data),
        "firmware_volumes": scan_firmware_volumes(data),
        "pe_images": scan_pe_images(data),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("images", nargs="+", type=Path)
    args = parser.parse_args()
    missing = [str(path) for path in args.images if not path.is_file()]
    if missing:
        parser.error("not a regular file: " + ", ".join(missing))
    print(json.dumps([inventory(path) for path in args.images], indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
