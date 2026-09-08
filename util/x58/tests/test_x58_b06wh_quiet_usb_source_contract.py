#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
KCONFIG = (BOARD / "Kconfig").read_text()
MAKEFILE = (BOARD / "Makefile.mk").read_text()
BOOTBLOCK = (BOARD / "bootblock.c").read_text()
ROMSTAGE = (BOARD / "romstage.c").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
PCI = (BOARD / "b06vn_pci.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
HANDOFF = (BOARD / "b06v6_handoff.c").read_text()
USB_SOURCE = (BOARD / "b06wg_usb_auto.c").read_text()
USB_HEADER = (BOARD / "b06wg_usb_auto.h").read_text()
WG_SEABIOS = (BOARD / "config_seabios_b06wg_usbtrace8").read_text()
WH_SEABIOS = (BOARD / "config_seabios_b06wh_usbtrace6").read_text()
WG_CONFIG = (ROOT / "configs/x58-pro-e-b06wg.config").read_text()
WH_CONFIG = (ROOT / "configs/x58-pro-e-b06wh.config").read_text()
BUILDER = (ROOT / "scripts/build_x58_b06_sata_successor.sh").read_text()
WRAPPER = ROOT / "scripts/build_x58_b06wh.sh"

WG_SYMBOL = "CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS"
WH_SYMBOL = "CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS"
LAB_SYMBOL = "CONFIG_X58_PRO_E_B06WF_USB_LAB"
WG_ID = "X58PROE-B06WG-AUTO-USB-SEABIOS-20260907"
WH_ID = "X58PROE-B06WH-AUTO-USB-SEABIOS-20260907"


def between(source: str, first: str, last: str) -> str:
    begin = source.index(first)
    return source[begin : source.index(last, begin)]


def config_contract(source: str) -> dict[str, str]:
    result = {}
    for raw_line in source.splitlines():
        line = raw_line.strip()
        if line.startswith("CONFIG_") and "=" in line:
            result[line.split("=", 1)[0]] = line
        elif line.startswith("# CONFIG_") and line.endswith(" is not set"):
            result[line[2:].split(" ", 1)[0]] = line
    return result


class B06WHQuietUsbSourceContractTests(unittest.TestCase):
    def test_variant_is_default_off_and_reuses_only_b06wg_engine(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06WH_QUIET_USB_SEABIOS",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        compact = " ".join(option.split())
        for fragment in (
            "depends on X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX",
            "depends on PAYLOAD_SEABIOS",
            "depends on !X58_PRO_E_B06WF_USB_LAB",
            "depends on !X58_PRO_E_B06WG_AUTO_USB_SEABIOS",
            "default n",
            "sole functional delta",
            "global debug level 6 instead of 8",
            "CONFIG_DEBUG_USB_TRACE remains enabled",
            "without changing a hardware operation, gate, delay, POST code",
        ):
            self.assertIn(fragment, compact)
        self.assertIn(
            "ramstage-$(CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS) += "
            "b06wg_usb_auto.c",
            MAKEFILE,
        )
        self.assertFalse((BOARD / "b06wh_usb_auto.c").exists())
        self.assertIn('#include "b06wg_usb_auto.h"', USB_SOURCE)

    def test_image_config_diff_is_only_selection_identity_and_debug_level(self) -> None:
        wg = config_contract(WG_CONFIG)
        wh = config_contract(WH_CONFIG)
        allowed = {
            WG_SYMBOL,
            WH_SYMBOL,
            "CONFIG_MAINBOARD_PART_NUMBER",
            "CONFIG_LOCALVERSION",
            "CONFIG_PAYLOAD_CONFIGFILE",
            "CONFIG_SEABIOS_DEBUG_LEVEL",
        }
        self.assertEqual(
            {key: value for key, value in wg.items() if key not in allowed},
            {key: value for key, value in wh.items() if key not in allowed},
        )
        self.assertEqual(wh[WG_SYMBOL], f"# {WG_SYMBOL} is not set")
        self.assertEqual(wh[WH_SYMBOL], f"{WH_SYMBOL}=y")
        self.assertEqual(wh[LAB_SYMBOL], f"# {LAB_SYMBOL} is not set")
        self.assertEqual(
            wh["CONFIG_MAINBOARD_PART_NUMBER"],
            'CONFIG_MAINBOARD_PART_NUMBER="X58 Pro-E B06WH automatic GPIO57 USB SeaBIOS"',
        )
        self.assertEqual(
            wh["CONFIG_LOCALVERSION"],
            'CONFIG_LOCALVERSION="x58-pro-e-b06wh"',
        )
        self.assertEqual(
            wh["CONFIG_PAYLOAD_CONFIGFILE"],
            'CONFIG_PAYLOAD_CONFIGFILE="$(top)/src/mainboard/'
            '$(MAINBOARDDIR)/config_seabios_b06wh_usbtrace6"',
        )
        self.assertEqual(
            wh["CONFIG_SEABIOS_DEBUG_LEVEL"],
            "CONFIG_SEABIOS_DEBUG_LEVEL=6",
        )

    def test_seabios_config_has_exactly_one_functional_delta(self) -> None:
        wg = config_contract(WG_SEABIOS)
        wh = config_contract(WH_SEABIOS)
        self.assertEqual(set(wg), set(wh))
        changed = {key for key in wg if wg[key] != wh[key]}
        self.assertEqual(changed, {"CONFIG_DEBUG_LEVEL"})
        self.assertEqual(wg["CONFIG_DEBUG_LEVEL"], "CONFIG_DEBUG_LEVEL=8")
        self.assertEqual(wh["CONFIG_DEBUG_LEVEL"], "CONFIG_DEBUG_LEVEL=6")
        self.assertEqual(
            wh["CONFIG_DEBUG_USB_TRACE"], "CONFIG_DEBUG_USB_TRACE=y"
        )
        for setting in (
            "CONFIG_USB=y",
            "CONFIG_USB_UHCI=y",
            "CONFIG_USB_EHCI=y",
            "CONFIG_USB_MSC=y",
            "CONFIG_USB_HUB=y",
            "CONFIG_USB_KEYBOARD=y",
            "CONFIG_KEYBOARD=y",
            "# CONFIG_THREADS is not set",
        ):
            self.assertIn(setting, WH_SEABIOS)

    def test_b06wg_fast_postmem_and_usb_operations_are_shared(self) -> None:
        combined = (
            "#if CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS || \\\n"
            "\tCONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS"
        )
        self.assertEqual(HANDOFF.count(combined), 3)
        for token in (
            "b06wg_clear_window_once_sparse",
            "b06v6_transactional_smoke",
            "B06WG_CLEAR_READBACK_COUNT 9u",
        ):
            self.assertIn(token, HANDOFF)
        for token in (
            "b06wg_automatic_release",
            "B06WG_GPIO57_LOW_SETTLE_US 0x00010000u",
            "b06wg_all_oca_clear",
            "BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_BOOT, BS_ON_ENTRY",
        ):
            self.assertIn(token, USB_SOURCE)

    def test_identity_is_distinct_in_every_executable_stage(self) -> None:
        self.assertIn(f'#define B06WG_BUILD_ID "{WH_ID}"', USB_HEADER)
        self.assertIn('#define B06WG_STAGE_ID "B06WH-AUTO-GPIO57-USB1"', USB_HEADER)
        for source, expected_count in (
            (BOOTBLOCK, 1),
            (ROMSTAGE, 2),
            (RAMMON, 1),
            (PCI, 1),
        ):
            self.assertEqual(source.count(WH_ID), expected_count)
            self.assertEqual(source.count(WG_ID), expected_count)
            self.assertLess(source.index(WH_ID), source.index(WG_ID))
        ops = MAINBOARD[MAINBOARD.index("struct chip_operations") :]
        self.assertLess(ops.index(WH_SYMBOL), ops.index(WG_SYMBOL))
        self.assertIn("experimental B06WH automatic GPIO57 USB SeaBIOS", ops)

    def test_kconfig_and_builder_select_quiet_payload(self) -> None:
        self.assertIn(
            'config_seabios_b06wh_usbtrace6" if '
            "X58_PRO_E_B06WH_QUIET_USB_SEABIOS && PAYLOAD_SEABIOS",
            KCONFIG,
        )
        self.assertIn(
            'default "X58 Pro-E B06WH automatic GPIO57 USB SeaBIOS" if '
            "X58_PRO_E_B06WH_QUIET_USB_SEABIOS",
            KCONFIG,
        )
        self.assertTrue(WRAPPER.exists())
        self.assertTrue(WRAPPER.stat().st_mode & 0o111)
        self.assertIn("b06wh", WRAPPER.read_text())
        for token in (
            "b06wh)",
            f'build_id="{WH_ID}"',
            "CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS=y",
            "CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS=n",
            "CONFIG_DEBUG_USB_TRACE=y",
            "CONFIG_DEBUG_LEVEL=6",
            "CONFIG_USB_KEYBOARD=y",
        ):
            self.assertIn(token, BUILDER)

    def test_builder_accepts_hidden_b06wh_symbol_for_b06wg(self) -> None:
        wg_dotconfig_checks = between(
            BUILDER,
            'if [[ "${variant}" == b06wg ]]; then',
            'if [[ "${variant}" == b06wh ]]; then',
        )
        self.assertNotIn(
            "# CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS is not set",
            wg_dotconfig_checks,
        )
        self.assertIn(
            "CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS=n",
            BUILDER,
        )


if __name__ == "__main__":
    unittest.main()
