#!/usr/bin/env python3
"""Extract Intel X58 ACPI raw sections using an existing UEFIExtract report.

Firmware remains local and unmodified. The Intel type-2 compression sections
used here contain a four-byte FV-section-header copy followed by LZMA-alone.
Only Python's host decompressor runs; no firmware module is executed.
Output files are created exclusively.
The JSON manifest records original compressed-section offsets and separate
decompressed offsets, not invented physical addresses for compressed objects.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import lzma
from pathlib import Path
import struct
import sys
import uuid
import zlib

from analyze_msi_acpi import inventory
from uefi_report_matrix import parse_report


ACPI_GUID = uuid.UUID("7e374e25-8e01-4fee-87f2-390c23c606cd")
DXE_GUID = "4A538818-5AE0-4EB2-B2EB-488B23657022"
MAX_OUTPUT = 32 * 1024 * 1024


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def decompress(data: bytes, expected_size: int) -> bytes:
    if not 0 < expected_size <= MAX_OUTPUT:
        raise ValueError("unreasonable decompressed size")
    decoder = lzma.LZMADecompressor(format=lzma.FORMAT_ALONE, memlimit=64 * 1024 * 1024)
    result = decoder.decompress(data[4:], max_length=MAX_OUTPUT + 1)
    if not decoder.eof or len(result) != expected_size:
        raise ValueError("Intel LZMA length/termination mismatch")
    if result[:4] != data[:4] or result[3:4] != b"\x17" or result[44:48] != b"_FVH":
        raise ValueError("Intel LZMA did not reproduce the expected FV section")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True,
                        help="new private output directory beneath blobs-local")
    args = parser.parse_args()
    if "blobs-local" not in args.output.resolve().parts:
        parser.error("proprietary extracted output must remain beneath blobs-local")
    image = args.image.read_bytes()
    rows = parse_report(args.report.read_text())
    args.output.mkdir(parents=True, exist_ok=False)
    candidates = []
    for i, row in enumerate(rows):
        if row.item_type != "File" or row.guid != DXE_GUID:
            continue
        following = rows[i + 1]
        if following.subtype == "Compressed" and following.base is not None:
            candidates.append(following)
    if not candidates:
        raise ValueError("no original-offset compressed DXE FV found in report")
    records = []
    for copy, row in enumerate(candidates):
        assert row.base is not None and row.size is not None
        section = image[row.base:row.base + row.size]
        if len(section) != row.size or int.from_bytes(section[:3], "little") != row.size:
            raise ValueError("report/source section-size mismatch")
        if section[3] != 1 or section[8] != 2:
            raise ValueError("not an Intel type-2 compressed section")
        if f"{zlib.crc32(section):08X}" != row.crc32:
            raise ValueError("report/source compressed CRC32 mismatch")
        expanded = decompress(section[9:], struct.unpack_from("<I", section, 4)[0])
        # A driver can embed the GUID as a constant; require a file header.
        matches, cursor = [], 0
        while (cursor := expanded.find(ACPI_GUID.bytes_le, cursor)) >= 0:
            size = int.from_bytes(expanded[cursor + 20:cursor + 23], "little")
            if (expanded[cursor + 18:cursor + 19] == b"\x02" and
                    24 <= size and cursor + size <= len(expanded) and
                    expanded[cursor + 27:cursor + 28] == b"\x19"):
                matches.append(cursor)
            cursor += 1
        if len(matches) != 1:
            raise ValueError("ACPI freeform FFS header missing or ambiguous")
        start = matches[0]
        size = int.from_bytes(expanded[start + 20:start + 23], "little")
        if expanded[start + 18] != 2 or start + size > len(expanded):
            raise ValueError("invalid ACPI freeform FFS boundary")
        ffs = expanded[start:start + size]
        entry = {
            "copy": copy, "compressed_section_image_offset": hex(row.base),
            "compressed_section_size": len(section),
            "compressed_section_sha256": sha(section), "algorithm": "Intel type 2 / LZMA-alone",
            "decompressed_sha256": sha(expanded), "decompressed_size": len(expanded),
            "ffs_guid": str(ACPI_GUID), "ffs_decompressed_offset": hex(start),
            "ffs_size": len(ffs), "ffs_sha256": sha(ffs), "raw_sections": [],
        }
        offset, index = 24, 0
        while offset + 4 <= len(ffs):
            length = int.from_bytes(ffs[offset:offset + 3], "little")
            if length < 4 or offset + length > len(ffs):
                raise ValueError("invalid raw section boundary")
            if ffs[offset + 3] != 0x19:
                raise ValueError("unexpected non-raw ACPI section")
            path = args.output / f"copy{copy}-{index:02d}.raw"
            with path.open("xb") as stream:
                stream.write(ffs[offset + 4:offset + length])
            record = inventory(path)
            record.update({"section_ffs_offset": hex(offset),
                           "body_ffs_offset": hex(offset + 4),
                           "body_decompressed_offset": hex(start + offset + 4),
                           "section_crc32": f"{zlib.crc32(ffs[offset:offset + length]):08X}"})
            entry["raw_sections"].append(record)
            index += 1
            offset = (offset + length + 3) & ~3
        records.append(entry)
    manifest = {
        "image": str(args.image), "image_size": len(image), "image_sha256": sha(image),
        "report": str(args.report), "report_sha256": sha(args.report.read_bytes()),
        "python_version": sys.version,
        "script_sha256": sha(Path(__file__).read_bytes()),
        "copies": records,
    }
    with (args.output / "manifest.json").open("x") as stream:
        json.dump(manifest, stream, indent=2, sort_keys=True)
        stream.write("\n")
    print(json.dumps({"manifest": str(args.output / "manifest.json"),
                      "copies": len(records), "raw_sections": [len(x["raw_sections"]) for x in records]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
