#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
ACPI_C = (COREBOOT / "src/acpi/acpi.c").read_text()
ACPI_H = (COREBOOT / "src/include/acpi/acpi.h").read_text()
ACPI_KCONFIG = (COREBOOT / "src/acpi/Kconfig").read_text()


def function_text(source: str, name: str) -> str:
    definition = re.search(
        rf"\b{re.escape(name)}\s*\([^;{{}}]*\)\s*\{{", source, re.DOTALL
    )
    if definition is None:
        raise ValueError(f"no definition for {name}")
    begin = source.rfind("\n", 0, definition.start()) + 1
    depth = 0
    for offset in range(definition.end() - 1, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[begin : offset + 1]
    raise ValueError(f"unterminated function {name}")


class AcpiCustomMcfgSourceContractTests(unittest.TestCase):
    def test_kconfig_is_table_publication_only(self) -> None:
        block = ACPI_KCONFIG[
            ACPI_KCONFIG.index("config ACPI_CUSTOM_MCFG") :
            ACPI_KCONFIG.index("config ACPI_NO_CUSTOM_MADT")
        ]
        self.assertIn("PCI MMCONFIG", block)
        self.assertIn("ACPI table contents only", block)
        self.assertIn("does not select", block)
        self.assertNotIn("select ECAM_MMCONF_SUPPORT", block)

    def test_public_board_filler_signature(self) -> None:
        signature = "unsigned long acpi_fill_mcfg(unsigned long current);"
        self.assertEqual(ACPI_H.count(signature), 1)

    def test_custom_path_does_not_depend_on_ecam_access(self) -> None:
        create = function_text(ACPI_C, "acpi_create_mcfg")
        ordered = (
            "if (CONFIG(ACPI_CUSTOM_MCFG))",
            "current = acpi_fill_mcfg(current);",
            "else if (CONFIG(ECAM_MMCONF_SUPPORT))",
            "current = acpi_fill_mcfg_from_config(current);",
        )
        positions = [create.index(token) for token in ordered]
        self.assertEqual(positions, sorted(positions))

    def test_default_ecam_behavior_remains_in_separate_filler(self) -> None:
        default = function_text(ACPI_C, "acpi_fill_mcfg_from_config")
        for token in (
            "PCI_SEGMENT_GROUP_COUNT",
            "CONFIG_ECAM_MMCONF_BASE_ADDRESS",
            "PCI_PER_SEGMENT_GROUP_ECAM_SIZE",
            "PCI_BUSES_PER_SEGMENT_GROUP - 1",
        ):
            self.assertIn(token, default)


if __name__ == "__main__":
    unittest.main()
