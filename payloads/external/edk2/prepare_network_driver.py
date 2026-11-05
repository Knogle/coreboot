#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Validate a separately supplied x64 EFI network driver before staging it."""

import argparse
import hashlib
import os
from pathlib import Path
import re
import struct
import tempfile


def validate_efi_driver(data):
    if not 0x100 <= len(data) <= 16 * 1024 * 1024 or data[:2] != b"MZ":
        raise ValueError("expected a bounded PE/COFF EFI image")
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    if pe < 0x40 or pe + 24 > len(data) or data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("invalid PE header")
    machine, sections = struct.unpack_from("<HH", data, pe + 4)
    optional_size = struct.unpack_from("<H", data, pe + 20)[0]
    optional = pe + 24
    table = optional + optional_size
    if machine != 0x8664 or not 1 <= sections <= 96:
        raise ValueError("expected an x64 EFI driver")
    if optional_size < 160 or table + 40 * sections > len(data):
        raise ValueError("truncated PE headers")
    if struct.unpack_from("<H", data, optional)[0] != 0x20b:
        raise ValueError("expected a PE32+ image")
    if struct.unpack_from("<H", data, optional + 68)[0] != 11:
        raise ValueError("expected an EFI boot services driver")
    entry = struct.unpack_from("<I", data, optional + 16)[0]
    image_size = struct.unpack_from("<I", data, optional + 56)[0]
    directories = struct.unpack_from("<I", data, optional + 108)[0]
    if not 0 < entry < image_size or directories < 6:
        raise ValueError("invalid driver entry or data directories")
    imports = struct.unpack_from("<II", data, optional + 120)
    relocations = struct.unpack_from("<II", data, optional + 152)
    if imports != (0, 0):
        raise ValueError("EFI driver must not require an import loader")
    if not relocations[0] or not relocations[1] or sum(relocations) > image_size:
        raise ValueError("driver has no bounded base relocation directory")
    executable_entry = False
    for index in range(sections):
        offset = table + 40 * index
        virtual_size, address, raw_size, raw_offset = struct.unpack_from(
            "<IIII", data, offset + 8)
        flags = struct.unpack_from("<I", data, offset + 36)[0]
        if address + max(virtual_size, raw_size) > image_size:
            raise ValueError("section exceeds image bounds")
        if raw_size and raw_offset + raw_size > len(data):
            raise ValueError("truncated section data")
        if address <= entry < address + max(virtual_size, raw_size):
            executable_entry |= bool(flags & 0x20000000)
    if not executable_entry:
        raise ValueError("entry point is outside executable sections")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--sha256", default="")
    args = parser.parse_args()
    try:
        data = args.input.read_bytes()
        validate_efi_driver(data)
        digest = hashlib.sha256(data).hexdigest()
        if args.sha256 and (not re.fullmatch(r"[0-9a-fA-F]{64}", args.sha256)
                            or digest != args.sha256.lower()):
            raise ValueError("network driver SHA-256 does not match configuration")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=args.output.parent,
                                         prefix=".network-driver-", delete=False) as out:
            temporary = Path(out.name)
            try:
                out.write(data)
                out.flush()
                os.fsync(out.fileno())
            except BaseException:
                temporary.unlink(missing_ok=True)
                raise
        try:
            os.replace(temporary, args.output)
        finally:
            temporary.unlink(missing_ok=True)
        print(f"Intel Gigabit UNDI: {len(data)} bytes, SHA-256 {digest}")
    except (OSError, ValueError, struct.error) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
