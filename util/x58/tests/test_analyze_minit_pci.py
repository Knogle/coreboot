# SPDX-License-Identifier: GPL-2.0-only
"""Synthetic PE/parser fixtures; no firmware bytes or disassembly are embedded."""

import importlib.util
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch


SCRIPT = Path(__file__).parents[1] / "scripts" / "analyze_minit_pci.py"
SPEC = importlib.util.spec_from_file_location("analyze_minit_pci", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def synthetic_disassembly(*instructions):
    """Hand-written semantic cases with synthetic addresses and dummy byte cells.

    The parser ignores opcode values; 90 is intentionally not an instruction
    encoding of the assembly column. Nothing here is an executable ROM excerpt.
    """
    return "\n".join(
        f"{0x2000 + index * 0x10:08x}: 90  {instruction}"
        for index, instruction in enumerate(instructions)
    )


class AnalyzeMinitPciTests(unittest.TestCase):
    def setUp(self):
        wrappers = {
            0x8000: MODULE.Wrapper("read", 4),
            0x8100: MODULE.Wrapper("write", 4),
        }
        self.profile = patch.dict(MODULE.WRAPPERS, wrappers, clear=True)
        self.profile.start()
        self.addCleanup(self.profile.stop)

    def test_extracts_text_from_raw_pe_without_normalizing_it(self):
        payload = bytearray(0x204)
        struct.pack_into("<I", payload, 0x3C, 0x80)
        payload[0x80:0x84] = b"PE\0\0"
        struct.pack_into("<H", payload, 0x86, 1)
        struct.pack_into("<H", payload, 0x94, 0)
        payload[0x98:0xA0] = b".text\0\0\0"
        struct.pack_into("<I", payload, 0xA0, 4)
        struct.pack_into("<I", payload, 0xA8, 4)
        struct.pack_into("<I", payload, 0xAC, 0x200)
        payload[0x200:0x204] = b"code"
        self.assertEqual(MODULE.pe_section_payload(bytes(payload), b".text"), b"code")

    def test_decodes_literal_channel_write(self):
        disassembly = synthetic_disassembly(
            "push 0x12345", "push 0x50", "push 0x0", "push 0x5",
            "push ebx", "push edi", "call 0x8100",
        )
        accesses = MODULE.parse_disassembly(disassembly)
        self.assertEqual(len(accesses), 1)
        access = accesses[0]
        self.assertEqual(access["operation"], "write")
        self.assertEqual(access["width_bytes"], 4)
        self.assertEqual(access["device"], 5)
        self.assertEqual(access["function"], 0)
        self.assertEqual(access["offset"], "0x50")
        self.assertEqual(access["role"], "channel1_control")
        self.assertEqual(access["register"], "MC_CHANNEL_DIMM_RESET_CMD")
        self.assertEqual(access["arguments"]["value"]["value"], "0x12345")
        self.assertFalse(access["arguments"]["bus"]["literal"])

    def test_retains_dynamic_target_without_guessing(self):
        disassembly = synthetic_disassembly(
            "push ecx", "push eax", "push DWORD PTR [ebp+0x14]",
            "push edi", "push ebx", "call 0x8000",
        )
        access = MODULE.parse_disassembly(disassembly)[0]
        self.assertIsNone(access["device"])
        self.assertIsNone(access["function"])
        self.assertIsNone(access["offset"])
        self.assertFalse(access["decoded_uncore_target"])
        self.assertIsNone(access["register"])

    def test_control_flow_boundary_discards_stale_pushes(self):
        disassembly = synthetic_disassembly(
            "push 0x50", "jmp 0x9000", "nop", "call 0x8000",
        )
        access = MODULE.parse_disassembly(disassembly)[0]
        self.assertFalse(access["arguments_complete"])
        self.assertEqual(access["arguments"], {})


if __name__ == "__main__":
    unittest.main()
