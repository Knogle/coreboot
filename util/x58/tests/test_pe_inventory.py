from __future__ import annotations

import hashlib
from pathlib import Path
import struct
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

from pe_inventory import (
    PeFormatError,
    canonical_bytes,
    canonical_te_bytes,
    parse_pe,
    parse_te,
    scan_pe_images,
)


def synthetic_pe(image_base: int) -> bytes:
    data = bytearray(0x600)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", data, 0x84, 0x14C, 2, 0x12345678, 0, 0, 0xE0, 0x210E)
    optional = 0x98
    struct.pack_into("<H", data, optional, 0x10B)
    struct.pack_into("<I", data, optional + 16, 0x1000)
    struct.pack_into("<I", data, optional + 20, 0x1000)
    struct.pack_into("<I", data, optional + 24, 0x2000)
    struct.pack_into("<I", data, optional + 28, image_base)
    struct.pack_into("<II", data, optional + 32, 0x1000, 0x200)
    struct.pack_into("<I", data, optional + 56, 0x3000)
    struct.pack_into("<I", data, optional + 60, 0x200)
    struct.pack_into("<I", data, optional + 64, 0xDEADBEEF)
    struct.pack_into("<H", data, optional + 68, 3)
    struct.pack_into("<I", data, optional + 92, 16)
    struct.pack_into("<II", data, optional + 96 + 8 * 5, 0x2000, 12)

    section_table = optional + 0xE0
    data[section_table : section_table + 8] = b".text\0\0\0"
    struct.pack_into("<IIII", data, section_table + 8, 0x100, 0x1000, 0x200, 0x200)
    struct.pack_into("<I", data, section_table + 36, 0x60000020)
    second = section_table + 40
    data[second : second + 8] = b".reloc\0\0"
    struct.pack_into("<IIII", data, second + 8, 0x100, 0x2000, 0x200, 0x400)
    struct.pack_into("<I", data, second + 36, 0x42000040)

    struct.pack_into("<I", data, 0x220, image_base + 0x1234)
    struct.pack_into("<IIHH", data, 0x400, 0x1000, 12, 0x3020, 0)
    return bytes(data)


def synthetic_te(image_base: int) -> bytes:
    data = bytearray(0x400)
    stripped_size = 0x120
    data[:2] = b"VZ"
    struct.pack_into("<HBBHIIQ", data, 2, 0x14C, 2, 11, stripped_size, 0x1000, 0x1000, image_base)
    struct.pack_into("<II", data, 24, 0x2000, 12)
    first = 40
    data[first : first + 8] = b".text\0\0\0"
    struct.pack_into("<IIII", data, first + 8, 0x100, 0x1000, 0x200, 0x200 + stripped_size - 40)
    struct.pack_into("<I", data, first + 36, 0x60000020)
    second = first + 40
    data[second : second + 8] = b".reloc\0\0"
    struct.pack_into("<IIII", data, second + 8, 0x100, 0x2000, 0x100, 0x300 + stripped_size - 40)
    struct.pack_into("<I", data, second + 36, 0x42000040)
    struct.pack_into("<I", data, 0x220, image_base + 0x1234)
    struct.pack_into("<IIHH", data, 0x300, 0x1000, 12, 0x3020, 0)
    return bytes(data)


class PeInventoryTests(unittest.TestCase):
    def test_parse_and_scan_embedded_image(self) -> None:
        embedded = b"prefix" + synthetic_pe(0x10000000) + b"suffix"
        images = scan_pe_images(embedded)
        self.assertEqual(len(images), 1)
        self.assertEqual(images[0].source_offset, 6)
        self.assertEqual(images[0].image_base, 0x10000000)
        self.assertEqual(images[0].sections[0].name, ".text")

    def test_relocation_normalization_matches_rebased_images(self) -> None:
        first = parse_pe(synthetic_pe(0x10000000))
        second = parse_pe(synthetic_pe(0x20000000))
        first_bytes, first_count, first_unsupported = canonical_bytes(first)
        second_bytes, second_count, second_unsupported = canonical_bytes(second)
        self.assertEqual(hashlib.sha256(first_bytes).digest(), hashlib.sha256(second_bytes).digest())
        self.assertEqual(first_count, 1)
        self.assertEqual(second_count, 1)
        self.assertEqual(first_unsupported, ())
        self.assertEqual(second_unsupported, ())

    def test_truncated_section_is_rejected(self) -> None:
        truncated = synthetic_pe(0x10000000)[:-1]
        with self.assertRaises(PeFormatError):
            parse_pe(truncated)

    def test_te_relocation_normalization_matches_rebased_images(self) -> None:
        first = parse_te(synthetic_te(0x10000000))
        second = parse_te(synthetic_te(0x20000000))
        first_bytes, first_count, first_unsupported = canonical_te_bytes(first)
        second_bytes, second_count, second_unsupported = canonical_te_bytes(second)
        self.assertEqual(hashlib.sha256(first_bytes).digest(), hashlib.sha256(second_bytes).digest())
        self.assertEqual(first_count, 1)
        self.assertEqual(second_count, 1)
        self.assertEqual(first_unsupported, ())
        self.assertEqual(second_unsupported, ())


if __name__ == "__main__":
    unittest.main()
