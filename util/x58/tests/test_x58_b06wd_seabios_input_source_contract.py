#!/usr/bin/env python3

import os
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
KCONFIG = (BOARD / "Kconfig").read_text()
MAKEFILE = (BOARD / "Makefile.mk").read_text()
PCI = (BOARD / "b06vn_pci.c").read_text()
INPUT = (BOARD / "b06wd_input.c").read_text()
INPUT_ASL = (BOARD / "b06wd_input.asl").read_text()
DSDT = (BOARD / "dsdt.asl").read_text()
ACPI = (BOARD / "acpi_tables.c").read_text()
BOOTBLOCK = (BOARD / "bootblock.c").read_text()
ROMSTAGE = (BOARD / "romstage.c").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
BASE_CONFIG = (ROOT / "configs/x58-pro-e-b06wc.config").read_text()
IMAGE_CONFIG = (ROOT / "configs/x58-pro-e-b06wd.config").read_text()
BUILDER = (ROOT / "scripts/build_x58_b06_sata_successor.sh").read_text()
WRAPPER = ROOT / "scripts/build_x58_b06wd.sh"

SYMBOL = "CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT"
ID = "X58PROE-B06WD-SEABIOS-INPUT-20260906"
BASE_ID = "X58PROE-B06WC-INTEGRATED-PLATFORM-20260906"


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


class B06WDSeaBiosInputSourceContractTests(unittest.TestCase):
    def test_variant_is_a_separate_b06wc_successor(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06WD_SEABIOS_INPUT",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        compact = " ".join(option.split())
        for fragment in (
            "depends on X58_PRO_E_B06WC_INTEGRATED_PLATFORM",
            "depends on PAYLOAD_SEABIOS",
            "select DRIVERS_PS2_KEYBOARD",
            "software-only SATA policy mismatch measured by B06WC-HW-01",
            "LPC_EN.KBC (2001 -> 2401)",
            "PNP0303, ports 60/64, IRQ1",
            "adds no USB GPIO, over-current, PORTSC or controller write",
        ):
            self.assertIn(fragment, compact)

        devicetree = between(KCONFIG, "config DEVICETREE", "config X58_PRO_E_B06M")
        self.assertLess(devicetree.index(SYMBOL.removeprefix("CONFIG_")),
                        devicetree.index("X58_PRO_E_B06WC_INTEGRATED_PLATFORM"))
        payload = between(KCONFIG, "config PAYLOAD_CONFIGFILE", "config MAINBOARD_PART_NUMBER")
        self.assertLess(payload.index(SYMBOL.removeprefix("CONFIG_")),
                        payload.index("X58_PRO_E_B06WC_INTEGRATED_PLATFORM"))

    def test_config_is_only_the_intended_b06wc_delta(self) -> None:
        base = config_contract(BASE_CONFIG)
        image = config_contract(IMAGE_CONFIG)
        allowed = {
            SYMBOL,
            "CONFIG_DRIVERS_PS2_KEYBOARD",
            "CONFIG_SEABIOS_PS2_TIMEOUT",
            "CONFIG_MAINBOARD_PART_NUMBER",
            "CONFIG_LOCALVERSION",
        }
        self.assertEqual(
            {key: value for key, value in base.items() if key not in allowed},
            {key: value for key, value in image.items() if key not in allowed},
        )
        self.assertEqual(image[SYMBOL], f"{SYMBOL}=y")
        self.assertEqual(image["CONFIG_DRIVERS_PS2_KEYBOARD"],
                         "CONFIG_DRIVERS_PS2_KEYBOARD=y")
        self.assertEqual(image["CONFIG_SEABIOS_PS2_TIMEOUT"],
                         "CONFIG_SEABIOS_PS2_TIMEOUT=1000")
        self.assertEqual(image["CONFIG_LOCALVERSION"],
                         'CONFIG_LOCALVERSION="x58-pro-e-b06wd"')

    def test_identity_is_distinct_in_every_executable_stage(self) -> None:
        for source, expected_count in (
            (BOOTBLOCK, 1),
            (ROMSTAGE, 2),
            (RAMMON, 1),
            (PCI, 1),
        ):
            self.assertEqual(source.count(ID), expected_count)
            self.assertLess(source.index(ID), source.index(BASE_ID))
        self.assertLess(MAINBOARD.index(SYMBOL),
                        MAINBOARD.index("CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM"))
        self.assertIn("experimental B06WD SeaBIOS input platform", MAINBOARD)

    def test_sata_policy_fix_is_software_only_and_precedes_b06vv(self) -> None:
        prestate = function_text(PCI, "b06wd_sata_policy_prestate_is_exact")
        for exact in (
            "sata->command == (PCI_COMMAND_IO | PCI_COMMAND_MEMORY)",
            "snapshot->f2_command == 0",
            "snapshot->f2_pcs == 0",
            "snapshot->f2_sclkcg == 0",
            "b06vs_abar_resource_is_exact",
            "snapshot->f5_id == 0xffffffffu",
        ):
            self.assertIn(exact, prestate)

        fix = function_text(PCI, "b06wd_correct_sata_policy_once")
        self.assertEqual(fix.count("sata->command &= ~PCI_COMMAND_IO;"), 1)
        self.assertNotIn("pci_io_write_config", fix)
        self.assertIn("b06vv_snapshot_is_exact(&after, sata, abar, 0)", fix)
        self.assertIn("HARDWARE_CMD=0000 HARDWARE_WRITE=0", fix)

        assigned = function_text(PCI, "b06vn_resources_assigned")
        order = (
            "b06wd_correct_sata_policy_once();",
            "b06vv_program_pcs_once();",
            "b06vv_program_sclk_once();",
            "b06vw_program_mmio_once();",
        )
        positions = [assigned.index(token) for token in order]
        self.assertEqual(positions, sorted(positions))

    def test_kbc_decode_is_exact_and_keyboard_probe_is_bounded_library_path(self) -> None:
        for token in (
            "B06WD_LPC_EN_PRE == 0x2001",
            "KBC_LPC_EN == 0x0400",
            "B06WD_LPC_EN_KBC == 0x2401",
            "snapshot->did0 == 0x05",
            "snapshot->did1 == 0x41",
            "snapshot->vid0 == 0x19",
            "snapshot->vid1 == 0x34",
            "snapshot->enabled == 0x01",
            "snapshot->io0_lo == 0x60",
            "snapshot->irq1 == 0x01",
            "snapshot->irq2 == 0x0c",
            "snapshot->mode == 0x83",
        ):
            self.assertIn(token, INPUT)

        prepare = function_text(INPUT, "b06wd_prepare_input")
        self.assertEqual(prepare.count("pci_io_write_config16("), 1)
        self.assertIn("lpc_en != B06WD_LPC_EN_PRE", prepare)
        self.assertIn("status_before != 0xff", prepare)
        self.assertIn("status_after == 0xff", prepare)
        self.assertIn("pc_keyboard_init(NO_AUX_DEVICE);", prepare)
        self.assertIn("inspect keyboard ACK/BAT log", prepare)
        self.assertLess(prepare.index("lpc_en != B06WD_LPC_EN_PRE"),
                        prepare.index("sio = b06wd_read_sio_snapshot();"))
        self.assertLess(prepare.index("pci_io_write_config16("),
                        prepare.index("pc_keyboard_init(NO_AUX_DEVICE);"))

        enabled = function_text(PCI, "b06vn_resources_enabled")
        order = (
            "b06vw_verify_final_once();",
            "b06vq_log_usb_runtime_once();",
            "b06wd_prepare_input();",
            "b06wc_usb_admit();",
        )
        positions = [enabled.index(token) for token in order]
        self.assertEqual(positions, sorted(positions))

    def test_acpi_and_seabios_receive_matching_keyboard_contract(self) -> None:
        self.assertIn('#include "b06wd_input.asl"', DSDT)
        self.assertIn("#if CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT", DSDT)
        for token in (
            'EisaId ("PNP0303")',
            "0x0060, 0x0060",
            "0x0064, 0x0064",
            "IRQ (Edge, ActiveHigh, Exclusive) {1}",
        ):
            self.assertIn(token, INPUT_ASL)
        self.assertNotIn("PNP0F13", INPUT_ASL)
        fadt = function_text(ACPI, "acpi_fill_fadt")
        self.assertLess(fadt.index("fadt->iapc_boot_arch = ACPI_FADT_LEGACY_DEVICES;"),
                        fadt.index("b06wd_enable_fadt_8042(fadt);"))
        helper = function_text(INPUT, "b06wd_enable_fadt_8042")
        self.assertIn("!b06wd_input_ready", helper)
        self.assertIn("fadt->iapc_boot_arch |= ACPI_FADT_8042;", helper)

    def test_build_path_keeps_usb_trace_and_adds_ps2_spinup(self) -> None:
        self.assertIn(
            "ramstage-$(CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT) += b06wd_input.c",
            MAKEFILE,
        )
        self.assertTrue(WRAPPER.is_file())
        self.assertTrue(os.access(WRAPPER, os.X_OK))
        self.assertRegex(WRAPPER.read_text(),
                         r'exec .*build_x58_b06_sata_successor\.sh" b06wd\s*$')
        for token in (
            "b06wc|b06wd",
            f'build_id="{ID}"',
            "CONFIG_DRIVERS_PS2_KEYBOARD=y",
            "CONFIG_SEABIOS_PS2_TIMEOUT=1000",
            "CONFIG_USB_UHCI=y",
            "CONFIG_USB_EHCI=y",
            "CONFIG_USB_KEYBOARD=y",
            "CONFIG_PS2PORT=y",
            "etc/usb-time-sigatt",
            "etc/ps2-keyboard-spinup",
        ):
            self.assertIn(token, BUILDER)


if __name__ == "__main__":
    unittest.main()
