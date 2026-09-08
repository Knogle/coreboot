# SPDX-License-Identifier: GPL-2.0-only
"""Synthetic parser tests; these instruction descriptions are not ROM extracts."""

import importlib.util
from pathlib import Path
import sys
import unittest
from unittest.mock import patch


SCRIPTS = Path(__file__).parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS))
SCRIPT = SCRIPTS / "analyze_csi_pci.py"
SPEC = importlib.util.spec_from_file_location("analyze_csi_pci", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def synthetic_disassembly(*instructions):
    """Only the assembly column matters; 90 is a parser-only placeholder byte.

    Deliberately spaced synthetic addresses and non-encoding byte columns keep
    these hand-written fixtures distinct from executable/vendor instruction data.
    """
    return "\n".join(
        f"{0x1000 + index * 0x10:08x}: 90  {instruction}"
        for index, instruction in enumerate(instructions)
    )


class AnalyzeCsiPciTests(unittest.TestCase):
    def setUp(self):
        wrappers = {
            0x7000: MODULE.analyzer.Wrapper("read", 4),
            0x7100: MODULE.analyzer.Wrapper("write", 4),
        }
        self.profile = patch.dict(MODULE.CSI_WRAPPERS, wrappers, clear=True)
        self.profile.start()
        self.addCleanup(self.profile.stop)

    def test_propagates_callee_saved_offset_across_helper_call(self):
        disassembly = synthetic_disassembly(
            "mov ebx,0x94",
            "push ebx", "push 0x0", "push 0x6", "push 0x2", "push edi",
            "call 0x7000",
            "push 0x12345678", "push ebx", "push 0x0", "push 0x6",
            "push 0x2", "push DWORD PTR [ebp+0x10]", "call 0x7100",
        )
        accesses = MODULE.parse_csi_disassembly(disassembly)
        self.assertEqual(len(accesses), 2)
        self.assertEqual(accesses[0]["operation"], "read")
        access = accesses[1]
        self.assertEqual(access["operation"], "write")
        self.assertEqual(access["device"], 6)
        self.assertEqual(access["function"], 0)
        self.assertEqual(access["offset"], "0x94")
        self.assertEqual(access["arguments"]["value"]["value"], "0x12345678")
        self.assertTrue(access["arguments"]["offset"]["constant_propagated"])

    def test_jump_discards_register_constant(self):
        disassembly = synthetic_disassembly(
            "mov ebx,0x94", "jmp 0x9000", "nop",
            "push 0x55", "push ebx", "push 0x0", "push 0x6",
            "push 0x2", "push edi", "call 0x7100",
        )
        access = MODULE.parse_csi_disassembly(disassembly)[0]
        self.assertIsNone(access["offset"])
        self.assertFalse(access["arguments"]["offset"]["constant_propagated"])

    def test_partial_register_write_discards_parent_constant(self):
        disassembly = synthetic_disassembly(
            "xor ecx,ecx", "mov cl,BYTE PTR [edi+0x12]", "mov ebp,0x88",
            "push 0x22", "push ebp", "push 0x0", "push 0x0",
            "push ecx", "push esi", "call 0x7100",
        )
        access = MODULE.parse_csi_disassembly(disassembly)[0]
        self.assertEqual(access["device"], 0)
        self.assertEqual(access["function"], 0)
        self.assertEqual(access["offset"], "0x88")
        self.assertNotIn("value", access["arguments"]["bus"])


if __name__ == "__main__":
    unittest.main()
