#!/usr/bin/env python3

import os
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
KCONFIG = (BOARD / "Kconfig").read_text()
SOURCE = (BOARD / "acpi_tables.c").read_text()
HEADER = (BOARD / "b06wc_acpi.h").read_text()
BOOTBLOCK = (BOARD / "bootblock.c").read_text()
ROMSTAGE = (BOARD / "romstage.c").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
PCI = (BOARD / "b06vn_pci.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
BASE_CONFIG = (ROOT / "configs/x58-pro-e-b06wd.config").read_text()
IMAGE_CONFIG = (ROOT / "configs/x58-pro-e-b06we.config").read_text()
BUILDER = (ROOT / "scripts/build_x58_b06_sata_successor.sh").read_text()
WRAPPER = ROOT / "scripts/build_x58_b06we.sh"

SYMBOL = "CONFIG_X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX"
ID = "X58PROE-B06WE-ACPI-SAD-BDF-FIX-20260907"
BASE_ID = "X58PROE-B06WD-SEABIOS-INPUT-20260906"


def between(source: str, first: str, last: str) -> str:
    begin = source.index(first)
    return source[begin : source.index(last, begin)]


def function_text(source: str, name: str) -> str:
    definition = re.search(
        rf"\b{re.escape(name)}\s*\([^;{{}}]*\)\s*\{{", source, re.DOTALL
    )
    if definition is None:
        raise ValueError(name)
    begin = source.rfind("\n", 0, definition.start()) + 1
    depth = 0
    for offset in range(definition.end() - 1, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[begin : offset + 1]
    raise ValueError(name)


def config_contract(source: str) -> dict[str, str]:
    result = {}
    for raw_line in source.splitlines():
        line = raw_line.strip()
        if line.startswith("CONFIG_") and "=" in line:
            result[line.split("=", 1)[0]] = line
        elif line.startswith("# CONFIG_") and line.endswith(" is not set"):
            result[line[2:].split(" ", 1)[0]] = line
    return result


class B06WEAcpiSadBdfSourceContractTests(unittest.TestCase):
    def test_variant_is_a_separate_b06wd_successor(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        compact = " ".join(option.split())
        for fragment in (
            "depends on X58_PRO_E_B06WD_SEABIOS_INPUT",
            "ff:00.1 instead of the absent 00:00.1",
            "Require the measured SAD identity 8086:2d81",
            "adds no PCI, MMIO, USB, PS/2 or power-policy write",
            "keeps the existing fail-closed ACPI behavior",
        ):
            self.assertIn(fragment, compact)

        for first, last in (
            ("config DEVICETREE", "config X58_PRO_E_B06M"),
            ("config PAYLOAD_CONFIGFILE", "config MAINBOARD_PART_NUMBER"),
        ):
            section = between(KCONFIG, first, last)
            self.assertLess(
                section.index(SYMBOL.removeprefix("CONFIG_")),
                section.index("X58_PRO_E_B06WD_SEABIOS_INPUT"),
            )
        part_number = KCONFIG[KCONFIG.index("config MAINBOARD_PART_NUMBER") :]
        self.assertLess(
            part_number.index(SYMBOL.removeprefix("CONFIG_")),
            part_number.index("X58_PRO_E_B06WD_SEABIOS_INPUT"),
        )

    def test_config_changes_only_identity_and_new_gate_selection(self) -> None:
        base = config_contract(BASE_CONFIG)
        image = config_contract(IMAGE_CONFIG)
        allowed = {SYMBOL, "CONFIG_MAINBOARD_PART_NUMBER", "CONFIG_LOCALVERSION"}
        self.assertEqual(
            {key: value for key, value in base.items() if key not in allowed},
            {key: value for key, value in image.items() if key not in allowed},
        )
        self.assertEqual(image[SYMBOL], f"{SYMBOL}=y")
        self.assertEqual(image["CONFIG_LOCALVERSION"],
                         'CONFIG_LOCALVERSION="x58-pro-e-b06we"')

    def test_identity_is_distinct_in_every_executable_stage(self) -> None:
        for source, expected_count in (
            (BOOTBLOCK, 1),
            (ROMSTAGE, 2),
            (RAMMON, 1),
            (PCI, 1),
        ):
            self.assertEqual(source.count(ID), expected_count)
            self.assertLess(source.index(ID), source.index(BASE_ID))
        ops = MAINBOARD[MAINBOARD.index("struct chip_operations") :]
        self.assertLess(ops.index(SYMBOL), ops.index(
            "CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT"))
        self.assertIn("experimental B06WE ACPI SAD BDF fix", MAINBOARD)

    def test_corrected_gate_uses_real_sad_and_keeps_old_variant_frozen(self) -> None:
        gate = function_text(SOURCE, "b06wc_acpi_gate_pciexbar")
        corrected = between(
            gate,
            "#if CONFIG_X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX",
            "#else",
        )
        legacy = between(gate, "#else", "#endif")
        self.assertIn("PCI_DEV(0xff, 0, 1)", corrected)
        self.assertIn("pci_io_read_config32(sad, 0x00)", corrected)
        self.assertIn("PCI_DEV(0, 0, 1)", legacy)

        for token in (
            "B06WC_ACPI_SAD_ID\t\t0x2d818086",
            "B06WE_ACPI_POST_MCFG_GATE_BEGIN\t0x8e",
            "B06WE_ACPI_POST_MCFG_GATE_READY\t0x8f",
        ):
            self.assertIn(token, HEADER)
        for token in (
            "SAD_BDF=ff:00.1 SAD=%08x",
            "sad_id != B06WC_ACPI_SAD_ID",
            "low != B06WC_ACPI_PCIEXBAR_VALUE",
            "host_id != B06WC_ACPI_ECAM_HOST_ID",
            "post_code(B06WE_ACPI_POST_MCFG_GATE_BEGIN);",
            "post_code(B06WE_ACPI_POST_MCFG_GATE_READY);",
        ):
            self.assertIn(token, gate)
        self.assertLess(gate.index("B06WE_ACPI_POST_MCFG_GATE_BEGIN"),
                        gate.index("sad_id != B06WC_ACPI_SAD_ID"))
        self.assertLess(gate.index("sad_id != B06WC_ACPI_SAD_ID"),
                        gate.index("B06WE_ACPI_POST_MCFG_GATE_READY"))
        for write in ("pci_io_write_config", "write8p(", "write16p(", "write32p("):
            self.assertNotIn(write, gate)

    def test_release_builder_and_payload_diagnostics_are_inherited(self) -> None:
        self.assertTrue(WRAPPER.is_file())
        self.assertTrue(os.access(WRAPPER, os.X_OK))
        self.assertRegex(
            WRAPPER.read_text(),
            r'exec .*build_x58_b06_sata_successor\.sh" b06we\s*$',
        )
        for token in (
            "b06wd|b06we",
            f'build_id="{ID}"',
            "CONFIG_X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX=y",
            "CONFIG_DRIVERS_PS2_KEYBOARD=y",
            "CONFIG_USB_UHCI=y",
            "CONFIG_USB_EHCI=y",
            "CONFIG_USB_KEYBOARD=y",
            "etc/usb-time-sigatt",
            "etc/ps2-keyboard-spinup",
        ):
            self.assertIn(token, BUILDER)


if __name__ == "__main__":
    unittest.main()
