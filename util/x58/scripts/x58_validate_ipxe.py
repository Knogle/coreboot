#!/usr/bin/env python3
"""Validate the built legacy PCI option ROM without assuming a historic hash."""

import argparse
import hashlib
from pathlib import Path
import struct


def validate(data):
    if len(data) < 512 or len(data) > 512 * 255 or data[:2] != b"\x55\xaa":
        raise ValueError("invalid legacy option-ROM signature or size")
    if data[2] * 512 != len(data) or sum(data) & 255:
        raise ValueError("option-ROM length or checksum mismatch")
    pcir = struct.unpack_from("<H", data, 0x18)[0]
    if pcir < 0x1a or pcir + 0x18 > len(data) or data[pcir:pcir + 4] != b"PCIR":
        raise ValueError("invalid PCI data structure")
    vendor, device = struct.unpack_from("<HH", data, pcir + 4)
    if (vendor, device) != (0x10ec, 0x8168):
        raise ValueError("option ROM does not target the RTL8168")
    if struct.unpack_from("<H", data, pcir + 0x10)[0] * 512 != len(data):
        raise ValueError("PCI image length mismatch")
    if data[pcir + 0x14] != 0 or data[pcir + 0x15] != 0x80:
        raise ValueError("expected a single final legacy x86 image")
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    args = parser.parse_args()
    print(validate(args.rom.read_bytes()))


if __name__ == "__main__":
    main()
