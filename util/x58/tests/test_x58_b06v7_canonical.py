#!/usr/bin/env python3

import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

import x58_b06v7_canonical as canonical  # noqa: E402


class B06V7CanonicalTests(unittest.TestCase):
    def test_csi_canonicalization_is_inclusive_and_single_byte(self) -> None:
        original = bytes((index * 37 + 11) & 0xFF for index in range(canonical.CSI_SIZE))
        changed = bytearray(original)
        changed[canonical.CSI_DYNAMIC_OFFSET] ^= 0x5A
        self.assertEqual(
            canonical.canonical_digest(
                original,
                ((canonical.CSI_DYNAMIC_OFFSET, canonical.CSI_DYNAMIC_OFFSET),),
            ),
            canonical.canonical_digest(
                bytes(changed),
                ((canonical.CSI_DYNAMIC_OFFSET, canonical.CSI_DYNAMIC_OFFSET),),
            ),
        )
        changed[canonical.CSI_DYNAMIC_OFFSET + 1] ^= 0x01
        self.assertNotEqual(
            canonical.canonical_digest(
                original,
                ((canonical.CSI_DYNAMIC_OFFSET, canonical.CSI_DYNAMIC_OFFSET),),
            ),
            canonical.canonical_digest(
                bytes(changed),
                ((canonical.CSI_DYNAMIC_OFFSET, canonical.CSI_DYNAMIC_OFFSET),),
            ),
        )

    def test_workspace_ranges_zero_first_and_last_bytes(self) -> None:
        original = bytes((index * 19 + 7) & 0xFF for index in range(canonical.WORKSPACE_SIZE))
        changed = bytearray(original)
        for first, last in canonical.WORKSPACE_DYNAMIC_RANGES:
            changed[first] ^= 0xA5
            changed[last] ^= 0x5A
        self.assertEqual(
            canonical.canonical_digest(original, canonical.WORKSPACE_DYNAMIC_RANGES),
            canonical.canonical_digest(
                bytes(changed), canonical.WORKSPACE_DYNAMIC_RANGES
            ),
        )
        changed[canonical.WORKSPACE_DYNAMIC_RANGES[0][1] + 1] ^= 0x01
        self.assertNotEqual(
            canonical.canonical_digest(original, canonical.WORKSPACE_DYNAMIC_RANGES),
            canonical.canonical_digest(
                bytes(changed), canonical.WORKSPACE_DYNAMIC_RANGES
            ),
        )

    def test_dynamic_byte_allowlist_is_exact(self) -> None:
        self.assertEqual(canonical.CSI_DYNAMIC_VALUES, frozenset((0x08, 0x0C)))
        self.assertNotIn(0x00, canonical.CSI_DYNAMIC_VALUES)
        self.assertNotIn(0x04, canonical.CSI_DYNAMIC_VALUES)

    def test_size_and_range_fail_closed(self) -> None:
        with self.assertRaises(ValueError):
            canonical.analyze_csi(bytes(canonical.CSI_SIZE - 1))
        with self.assertRaises(ValueError):
            canonical.analyze_workspace(bytes(canonical.WORKSPACE_SIZE + 1))
        with self.assertRaises(ValueError):
            canonical.canonical_digest(b"abc", ((1, 3),))
        with self.assertRaises(ValueError):
            canonical.canonical_digest(b"abc", ((1, 1), (1, 2)))


if __name__ == "__main__":
    unittest.main()
