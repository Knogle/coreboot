#!/usr/bin/env python3
"""Regression tests for the B06V1 host-side script compiler."""

from __future__ import annotations

import pathlib
import tempfile
import unittest

import x58_rommon_script as tool


class ScriptToolTests(unittest.TestCase):
    def parse(self, text: str) -> list[tool.Operation]:
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "test.xrs"
            path.write_text(text, encoding="utf-8")
            return tool.parse_script(path)

    def test_digest_matches_c_engine_vector(self) -> None:
        operations = self.parse(
            "read mem 1000 l\n"
            "mask mem 1004 l 0000ff00 00000012 rev\n"
            "assert mem 1004 l 000000ff 00000012\n"
        )
        self.assertEqual(tool.program_digest(operations), 0x2BF2E012)

    def test_canonical_commands(self) -> None:
        operations = self.parse(
            "write msr 1a0 q 00000001:00000002 rev\n"
            "poll pci ff060060 l 00800000 00000000 00000010\n"
            "delay 20\n"
        )
        self.assertEqual(
            [tool.operation_command(op) for op in operations],
            [
                "script add write msr 000001a0 q 00000001:00000002 rev",
                "script add poll pci ff060060 l 00800000 00000000 00000010",
                "script add delay 00000020",
            ],
        )

    def test_mask_overlap_replaces_field(self) -> None:
        operations = self.parse("mask mem 1000 l f 5 rev\n")
        self.assertEqual(operations[0].mask.lo, 0xF)
        self.assertEqual(operations[0].value.lo, 0x5)

    def test_expected_outside_mask_rejected(self) -> None:
        with self.assertRaisesRegex(tool.ScriptError, "outside"):
            self.parse("assert mem 1000 l 1 2\n")

    def test_target_poll_and_count_bounds(self) -> None:
        with self.assertRaisesRegex(tool.ScriptError, "target"):
            self.parse("read pci ff200000 l\n")
        with self.assertRaisesRegex(tool.ScriptError, "poll limit"):
            self.parse("poll mem 1000 l 1 1 0\n")
        with self.assertRaisesRegex(tool.ScriptError, "more than"):
            self.parse("\n".join(["read mem 1000 l"] * 33))

    def test_nonreversible_detected(self) -> None:
        operations = self.parse("write io 80 b aa nr\n")
        self.assertFalse(all(
            op.kind not in (tool.KIND["write"], tool.KIND["mask"]) or
            op.flags & tool.REVERSIBLE for op in operations
        ))

    def test_numeric_error_has_source_line(self) -> None:
        with self.assertRaisesRegex(tool.ScriptError, r"line 2: not hexadecimal"):
            self.parse("read mem 1000 l\nread mem nope l\n")


if __name__ == "__main__":
    unittest.main()
