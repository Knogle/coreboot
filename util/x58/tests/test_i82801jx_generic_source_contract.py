#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
ICH10 = COREBOOT / "src/southbridge/intel/i82801jx"
HEADER = (ICH10 / "i82801jx.h").read_text()
EARLY_INIT = (ICH10 / "early_init.c").read_text()


def macro_value(source: str, name: str) -> int:
    match = re.search(
        rf"^#define\s+{re.escape(name)}\s+(0x[0-9a-fA-F]+)\b",
        source,
        re.MULTILINE,
    )
    if match is None:
        raise ValueError(f"no hexadecimal definition for {name}")
    return int(match.group(1), 16)


def function_text(source: str, name: str) -> str:
    definition = re.search(
        rf"\b{re.escape(name)}\s*\([^;{{}}]*\)\s*\{{", source, re.DOTALL
    )
    if definition is None:
        raise ValueError(f"no definition for function {name}")
    begin = source.rfind("\n", 0, definition.start()) + 1
    body = definition.end() - 1
    depth = 0
    for offset in range(body, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[begin : offset + 1]
    raise ValueError(f"unterminated function {name}")


class I82801JxGenericSourceContractTests(unittest.TestCase):
    def test_device_interrupt_pin_offsets_do_not_alias(self) -> None:
        self.assertEqual(macro_value(HEADER, "D26IP"), 0x3114)
        self.assertEqual(macro_value(HEADER, "D25IP"), 0x3118)
        self.assertEqual(
            macro_value(HEADER, "D25IP") - macro_value(HEADER, "D26IP"), 4
        )

    def test_pmir_is_accessed_as_its_documented_32_bit_register(self) -> None:
        self.assertEqual(macro_value(HEADER, "D31F0_PMIR"), 0xAC)
        function = function_text(EARLY_INIT, "i82801jx_early_init")
        for fragment in (
            "u32 pmir = pci_read_config32(d31f0, D31F0_PMIR);",
            "PMIR_CF9LOCK | PMIR_FIELD_2 | PMIR_CF9GR | PMIR_FIELD_0",
            "pci_write_config32(d31f0, D31F0_PMIR, pmir);",
        ):
            self.assertIn(fragment, function)
        self.assertNotRegex(
            function,
            r"pci_(?:read|write)_config8\s*\(\s*d31f0\s*,\s*(?:0xac|D31F0_PMIR)",
        )


if __name__ == "__main__":
    unittest.main()
