from __future__ import annotations

from io import StringIO
from pathlib import Path
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

from decode_seabios_usbtrace import parse_usbtrace


QEMU_TRACE = """unrelated SeaBIOS output
[USBTRACE] BEGIN count=4 dropped=0
[USBTRACE] 00 EVT=01 TYPE=0 BDF=ff:1f.7 PORT=255 RET=0 A=00000064 B=00000000
[USBTRACE] 01 EVT=10 TYPE=3 BDF=00:04.0 PORT=255 RET=32 A=00000006 B=00006880
[USBTRACE] 02 EVT=39 TYPE=3 BDF=00:04.0 PORT=0 RET=0 A=00000008 B=00000007
[USBTRACE] 03 EVT=02 TYPE=0 BDF=ff:1f.7 PORT=255 RET=0 A=00000064 B=00000000
[USBTRACE] END
more unrelated output
"""


class DecodeSeabiosUsbTraceTests(unittest.TestCase):
    def test_valid_trace_and_sentinels(self) -> None:
        blocks = parse_usbtrace(StringIO(QEMU_TRACE))
        self.assertEqual(len(blocks), 1)
        block = blocks[0]
        self.assertTrue(block.valid)
        self.assertEqual(block.reported_count, 4)
        self.assertEqual(block.events[0].event_name, "setup_begin")
        self.assertIsNone(block.events[0].bdf)
        self.assertIsNone(block.events[0].port)
        self.assertEqual(block.events[1].controller_name, "ehci")
        self.assertEqual(block.events[1].bdf, "00:04.0")
        self.assertEqual(block.events[2].event_name, "hid_interrupt_pipe")
        self.assertEqual(block.events[2].value_b, 7)

    def test_drop_and_count_mismatch_are_invalid(self) -> None:
        text = QEMU_TRACE.replace("count=4 dropped=0", "count=5 dropped=2")
        block = parse_usbtrace(StringIO(text))[0]
        self.assertFalse(block.valid)
        self.assertIn("firmware dropped 2 event(s)", block.errors)
        self.assertTrue(any("reported count 5" in error for error in block.errors))

    def test_noncontiguous_and_malformed_entries_are_invalid(self) -> None:
        text = QEMU_TRACE.replace("[USBTRACE] 02 EVT=39", "[USBTRACE] 08 EVT=39")
        text = text.replace(
            "[USBTRACE] 03 EVT=02",
            "[USBTRACE] malformed\n[USBTRACE] 03 EVT=02",
        )
        block = parse_usbtrace(StringIO(text))[0]
        self.assertFalse(block.valid)
        self.assertTrue(any("non-contiguous" in error for error in block.errors))
        self.assertTrue(any("malformed trace line" in error for error in block.errors))

    def test_incomplete_block_is_retained(self) -> None:
        block = parse_usbtrace(StringIO(QEMU_TRACE.replace("[USBTRACE] END\n", "")))[0]
        self.assertFalse(block.complete)
        self.assertFalse(block.valid)
        self.assertIn("end of input before END", block.errors)

    def test_multiple_blocks(self) -> None:
        blocks = parse_usbtrace(StringIO(QEMU_TRACE + QEMU_TRACE))
        self.assertEqual(len(blocks), 2)
        self.assertTrue(all(block.valid for block in blocks))


if __name__ == "__main__":
    unittest.main()
