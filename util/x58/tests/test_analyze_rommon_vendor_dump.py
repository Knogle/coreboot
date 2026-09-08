#!/usr/bin/env python3

import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

import analyze_rommon_vendor_dump as analyzer  # noqa: E402


class AnalyzeRommonVendorDumpTests(unittest.TestCase):
    def test_legacy_vendor_records_remain_supported(self) -> None:
        raw = b"\n".join((
            b"[VENDOR] CSI 0000:00010203",
            b"[VENDOR] CSI 0004:0405",
        ))

        data, records, duplicates = analyzer.parse_dump(raw, "CSI")

        self.assertEqual(data, bytes(range(6)))
        self.assertEqual(records, 2)
        self.assertEqual(duplicates, 0)

    def test_automatic_raminit_records_are_reconstructed(self) -> None:
        raw = b"\r\n".join((
            b"[RAMINIT] CSI[0000]=00 01 02 03",
            b"[RAMINIT] CSI[0004]=04 05",
            b"[RAMINIT] WORK[0000]=aa bb",
        ))

        data, records, duplicates = analyzer.parse_dump(raw, "CSI")

        self.assertEqual(data, bytes(range(6)))
        self.assertEqual(records, 2)
        self.assertEqual(duplicates, 0)

    def test_matching_mixed_format_overlap_is_counted(self) -> None:
        raw = b"\n".join((
            b"[VENDOR] WORK 0000:aabb",
            b"[RAMINIT] WORK[0000]=aa bb cc",
        ))

        data, records, duplicates = analyzer.parse_dump(raw, "WORK")

        self.assertEqual(data, bytes.fromhex("aabbcc"))
        self.assertEqual(records, 2)
        self.assertEqual(duplicates, 2)

    def test_mixed_format_conflict_is_rejected(self) -> None:
        raw = b"\n".join((
            b"[VENDOR] CSI 0000:0001",
            b"[RAMINIT] CSI[0001]=ff",
        ))

        with self.assertRaisesRegex(ValueError, "conflicting byte at 0x1"):
            analyzer.parse_dump(raw, "CSI")

    def test_automatic_gap_is_rejected(self) -> None:
        raw = b"\n".join((
            b"[RAMINIT] WORK[0000]=00",
            b"[RAMINIT] WORK[0002]=02",
        ))

        with self.assertRaisesRegex(ValueError, "1 missing bytes: 0x1"):
            analyzer.parse_dump(raw, "WORK")

    def test_malformed_automatic_record_is_not_silently_ignored(self) -> None:
        raw = b"\n".join((
            b"[RAMINIT] CSI[0000]=00 01",
            b"[RAMINIT] CSI[0000]=00 gg",
        ))

        with self.assertRaisesRegex(
            ValueError, r"malformed \[RAMINIT\] CSI record at line 2"
        ):
            analyzer.parse_dump(raw, "CSI")


if __name__ == "__main__":
    unittest.main()
