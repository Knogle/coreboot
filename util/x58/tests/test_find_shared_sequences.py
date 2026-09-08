from __future__ import annotations

from pathlib import Path
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

from find_shared_sequences import find_matches


class SharedSequenceTests(unittest.TestCase):
    def test_one_to_many_matches_are_preserved(self) -> None:
        anchor = b"unique-sequence-0123456789"
        first = b"AA" + anchor + b"BB"
        second = b"CC" + anchor + b"DD" + anchor + b"EE"
        matches = find_matches(first, second, 12)
        anchors = {(left, right, length) for left, right, length in matches if length >= len(anchor)}
        self.assertIn((2, 2, len(anchor)), anchors)
        self.assertIn((2, 2 + len(anchor) + 2, len(anchor)), anchors)

    def test_padding_is_not_reported(self) -> None:
        first = b"A" * 128
        second = b"A" * 128
        self.assertEqual(find_matches(first, second, 16), [])

    def test_long_match_is_reported_once_per_diagonal(self) -> None:
        anchor = bytes(range(1, 100))
        matches = find_matches(b"x" + anchor + b"y", b"z" + anchor + b"w", 16)
        self.assertEqual(matches, [(1, 1, len(anchor))])


if __name__ == "__main__":
    unittest.main()
