from __future__ import annotations

from pathlib import Path
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

from uefi_report_matrix import compare_summaries, parse_report, report_summary


REPORT = """\
        Type         |        Subtype        |   Base   |   Size   |  CRC32   |   Name
 Volume              | FFSv2                 | 00000000 | 00001000 | 12345678 | -- 7A9354D9-0468-444A-81CE-0BF617D890DF
 File                | PEI module            | 00000048 | 00000100 | ABCDEF01 | --- 63C0690C-5D9E-4EE3-840E-FAE0E76E291A | MemoryInit
 Section             | PEI dependency       | 00000060 | 00000010 | 11111111 | ---- PEI dependency section
 Section             | PE32 image            | 00000070 | 000000D0 | 22222222 | ---- PE32 image section
 File                | DXE driver            |   N/A    | 00000080 | 33333333 | --- D1DA2E54-B233-4BCA-BB1D-A09363338484
"""


class UefiReportMatrixTests(unittest.TestCase):
    def test_rows_and_names_are_parsed(self) -> None:
        rows = parse_report(REPORT)
        self.assertEqual(len(rows), 5)
        self.assertEqual(rows[1].guid, "63C0690C-5D9E-4EE3-840E-FAE0E76E291A")
        self.assertEqual(rows[1].name, "MemoryInit")
        self.assertIsNone(rows[4].base)
        self.assertEqual(rows[2].depth, 4)

    def test_summary_and_comparison(self) -> None:
        first = report_summary("first", Path("first.report"), parse_report(REPORT))
        second_text = REPORT.replace(
            "D1DA2E54-B233-4BCA-BB1D-A09363338484",
            "AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE",
        )
        second = report_summary(
            "second", Path("second.report"), parse_report(second_text)
        )
        self.assertEqual(first["file_occurrences"], 2)
        self.assertEqual(first["unique_file_guids"], 2)
        comparison = compare_summaries([first, second])[0]
        self.assertEqual(comparison["shared_unique_guids"], 1)
        self.assertEqual(comparison["left_only"], 1)
        self.assertEqual(comparison["right_only"], 1)


if __name__ == "__main__":
    unittest.main()
