#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
SOURCE = (BOARD / "b06wg_usb_auto.c").read_text()
HEADER = (BOARD / "b06wg_usb_auto.h").read_text()
KCONFIG = (BOARD / "Kconfig").read_text()
MAKEFILE = (BOARD / "Makefile.mk").read_text()
BOOTBLOCK = (BOARD / "bootblock.c").read_text()
ROMSTAGE = (BOARD / "romstage.c").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
PCI = (BOARD / "b06vn_pci.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
HANDOFF = (BOARD / "b06v6_handoff.c").read_text()
SEABIOS_CONFIG = (BOARD / "config_seabios_b06wg_usbtrace8").read_text()
HISTORICAL_SEABIOS_CONFIG = (BOARD / "config_seabios_b06vq_usbtrace1").read_text()
BASE_CONFIG = (ROOT / "configs/x58-pro-e-b06we.config").read_text()
IMAGE_CONFIG = (ROOT / "configs/x58-pro-e-b06wg.config").read_text()
BUILDER = (ROOT / "scripts/build_x58_b06_sata_successor.sh").read_text()
WRAPPER = ROOT / "scripts/build_x58_b06wg.sh"

SYMBOL = "CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS"
BASE_SYMBOL = "CONFIG_X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX"
LAB_SYMBOL = "CONFIG_X58_PRO_E_B06WF_USB_LAB"
BUILD_ID = "X58PROE-B06WG-AUTO-USB-SEABIOS-20260907"
BASE_ID = "X58PROE-B06WE-ACPI-SAD-BDF-FIX-20260907"


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


class B06WGUsbAutoSourceContractTests(unittest.TestCase):
    def test_variant_is_default_off_and_separate_from_manual_lab(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06WG_AUTO_USB_SEABIOS",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        compact = " ".join(option.split())
        for fragment in (
            "depends on X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX",
            "depends on PAYLOAD_SEABIOS",
            "depends on !X58_PRO_E_B06WF_USB_LAB",
            "default n",
            "Sticky OCC/OCI",
            "never writes PCI configuration",
            "nine sparse readbacks per window",
            "ASSUMED_STABLE",
            "Historical images retain their exhaustive memory tests unchanged",
            "socketed-flash and one-second AC recovery path",
        ):
            self.assertIn(fragment, compact)
        self.assertIn(
            "ramstage-$(CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS) += "
            "b06wg_usb_auto.c",
            MAKEFILE,
        )
        self.assertNotIn(SYMBOL, (BOARD / "b06wf_usb_lab.c").read_text())

    def test_config_is_only_b06we_plus_identity_and_auto_selection(self) -> None:
        base = config_contract(BASE_CONFIG)
        image = config_contract(IMAGE_CONFIG)
        allowed = {
            SYMBOL,
            LAB_SYMBOL,
            "CONFIG_MAINBOARD_PART_NUMBER",
            "CONFIG_LOCALVERSION",
            "CONFIG_PAYLOAD_CONFIGFILE",
            "CONFIG_SEABIOS_DEBUG_LEVEL",
        }
        self.assertEqual(
            {key: value for key, value in base.items() if key not in allowed},
            {key: value for key, value in image.items() if key not in allowed},
        )
        self.assertEqual(image[SYMBOL], f"{SYMBOL}=y")
        self.assertEqual(image[LAB_SYMBOL], f"# {LAB_SYMBOL} is not set")
        self.assertEqual(
            image["CONFIG_LOCALVERSION"],
            'CONFIG_LOCALVERSION="x58-pro-e-b06wg"',
        )
        self.assertEqual(
            image["CONFIG_PAYLOAD_CONFIGFILE"],
            'CONFIG_PAYLOAD_CONFIGFILE="$(top)/src/mainboard/'
            '$(MAINBOARDDIR)/config_seabios_b06wg_usbtrace8"',
        )
        self.assertEqual(
            image["CONFIG_SEABIOS_DEBUG_LEVEL"],
            "CONFIG_SEABIOS_DEBUG_LEVEL=8",
        )

    def test_identity_is_distinct_in_every_executable_stage(self) -> None:
        self.assertIn(f'#define B06WG_BUILD_ID "{BUILD_ID}"', HEADER)
        for source, expected_count in (
            (BOOTBLOCK, 1),
            (ROMSTAGE, 2),
            (RAMMON, 1),
            (PCI, 1),
        ):
            self.assertEqual(source.count(BUILD_ID), expected_count)
            self.assertLess(source.index(BUILD_ID), source.index(BASE_ID))
        ops = MAINBOARD[MAINBOARD.index("struct chip_operations") :]
        self.assertLess(ops.index(SYMBOL), ops.index(LAB_SYMBOL))
        self.assertIn("experimental B06WG automatic GPIO57 USB SeaBIOS", ops)

    def test_callbacks_are_late_and_payload_is_fail_closed(self) -> None:
        self.assertIn(
            "BOOT_STATE_INIT_ENTRY(BS_WRITE_TABLES, BS_ON_ENTRY,\n"
            "\tb06wg_automatic_release, NULL);",
            SOURCE,
        )
        self.assertIn(
            "BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_BOOT, BS_ON_ENTRY,\n"
            "\tb06wg_payload_gate, NULL);",
            SOURCE,
        )
        release = function_text(SOURCE, "b06wg_automatic_release")
        final = function_text(SOURCE, "b06wg_payload_gate")
        self.assertLess(
            release.index('"PRECISE-B06WD-BASELINE"'),
            release.index("b06wg_gpio2_msb_write_target(GP_IO_SEL2"),
        )
        self.assertLess(
            release.index("b06wg_all_oca_clear"),
            release.index("b06wg_released = true"),
        )
        self.assertIn('b06wg_fail("release-callback-not-complete", false)', final)
        self.assertLess(
            final.index('"FINAL-PRE-PAYLOAD-GATE"'),
            final.index("b06wg_all_oca_clear"),
        )
        self.assertNotIn("b06wg_automatic_release", RAMMON)

    def test_only_gpio57_is_written_and_usb_controllers_are_read_only(self) -> None:
        writer = function_text(SOURCE, "b06wg_gpio2_msb_write_target")
        self.assertEqual(SOURCE.count("outb("), 1)
        self.assertIn("outb(target, port);", writer)
        self.assertIn("(before ^ after) & ~mask", writer)
        for forbidden in (
            "outw(",
            "outl(",
            "write8p(",
            "write16p(",
            "write32p(",
            "pci_io_write_config",
        ):
            self.assertNotIn(forbidden, SOURCE)
        self.assertNotIn("strcmp(", SOURCE)
        release = function_text(SOURCE, "b06wg_automatic_release")
        self.assertEqual(release.count("B06WG_GPIO57_BYTE"), 2)
        self.assertNotIn("B06WG_GPIO56", SOURCE)
        self.assertIn("SeaBIOS owns HC reset/enumeration/HID", release)

    def test_gpio57_order_timing_readback_and_rollback(self) -> None:
        release = function_text(SOURCE, "b06wg_automatic_release")
        direction = release.index("b06wg_gpio2_msb_write_target(GP_IO_SEL2")
        delay = release.index("udelay(B06WG_GPIO57_LOW_SETTLE_US)")
        level = release.index("b06wg_gpio2_msb_write_target(GP_LVL2")
        self.assertLess(direction, delay)
        self.assertLess(delay, level)
        self.assertIn("B06WG_GPIO57_LOW_SETTLE_US 0x00010000u", SOURCE)
        for settle in ("udelay(10000);", "udelay(90000);", "udelay(400000);"):
            self.assertIn(settle, release)

        rollback = function_text(SOURCE, "b06wg_rollback_gpio57")
        first_write = rollback.index("b06wg_gpio2_msb_write_target")
        for gate in (
            "lpc_id != B06WG_LPC_ID",
            "gpiobase != B06WG_GPIOBASE_EXPECTED",
            "gpio_cntl != B06WG_GPIO_CNTL_EXPECTED",
            "GP_IO_USE_SEL2) != B06WG_GPIO_USE2_EXPECTED",
            "direction & ~B06WG_GPIO57",
        ):
            self.assertLess(rollback.index(gate), first_write)
        self.assertLess(
            rollback.index("b06wg_gpio2_msb_write_target(GP_LVL2"),
            rollback.index("b06wg_gpio2_msb_write_target(GP_IO_SEL2"),
        )

    def test_only_live_oca_is_a_port_admission_or_release_gate(self) -> None:
        active = function_text(SOURCE, "b06wg_all_oca_active")
        clear = function_text(SOURCE, "b06wg_all_oca_clear")
        for gate in (active, clear):
            self.assertIn("B06WG_EHCI_PORT_OCA", gate)
            self.assertIn("B06WG_UHCI_PORT_OCA", gate)
            for telemetry in (
                "B06WG_EHCI_PORT_OCC",
                "B06WG_UHCI_PORT_OCI",
                "B06WG_EHCI_PORT_CCS",
                "B06WG_UHCI_PORT_CCS",
                "B06WG_UHCI_PORT_LSDA",
            ):
                self.assertNotIn(telemetry, gate)
        self.assertNotIn("ports_are_exact_baseline", SOURCE)
        self.assertIn(
            "OCC/OCI and connect/speed/change fields are asynchronous telemetry",
            SOURCE,
        )

    def test_seabios_has_complete_uhci_ehci_and_keyboard_path(self) -> None:
        for setting in (
            "CONFIG_USB=y",
            "CONFIG_USB_UHCI=y",
            "CONFIG_USB_EHCI=y",
            "CONFIG_USB_HUB=y",
            "CONFIG_USB_KEYBOARD=y",
            "CONFIG_KEYBOARD=y",
            "CONFIG_PS2PORT=y",
            "CONFIG_DEBUG_USB_TRACE=y",
            "# CONFIG_THREADS is not set",
        ):
            self.assertIn(setting, SEABIOS_CONFIG)
        self.assertIn("CONFIG_DEBUG_LEVEL=8", SEABIOS_CONFIG)
        self.assertNotIn("CONFIG_DEBUG_LEVEL=9", SEABIOS_CONFIG)
        self.assertIn("CONFIG_DEBUG_LEVEL=9", HISTORICAL_SEABIOS_CONFIG)
        self.assertIn(
            'config_seabios_b06wg_usbtrace8" if '
            "X58_PRO_E_B06WG_AUTO_USB_SEABIOS && PAYLOAD_SEABIOS",
            KCONFIG,
        )
        source_root = COREBOOT / "payloads/external/SeaBIOS/seabios/src"
        if not (source_root / "hw/usb.c").is_file():
            self.skipTest("optional pinned SeaBIOS checkout has not been prepared")
        usb = (source_root / "hw/usb.c").read_text()
        ehci = (source_root / "hw/usb-ehci.c").read_text()
        uhci = (source_root / "hw/usb-uhci.c").read_text()
        hid = (source_root / "hw/usb-hid.c").read_text()
        for token, body in (
            ("USB_REQ_SET_ADDRESS", usb),
            ("usb_hid_setup(usbdev)", usb),
            ("ehci_controller_setup", ehci),
            ("uhci_controller_setup", uhci),
            ("usb_kbd_setup", hid),
        ):
            self.assertIn(token, body)

    def test_fast_postmem_keeps_gates_alias_and_deterministic_clear(self) -> None:
        postmem = function_text(HANDOFF, "platform_romstage_post_mem")
        exact_gate = postmem.index("x58_b06v6_raminit_result_is_exact")
        mtrr_gate = postmem.index("b06v6_mtrr_state_is_exact", exact_gate)
        fast_begin = postmem.index(
            "#if CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS", mtrr_gate
        )
        fast_end = postmem.index("#else", fast_begin)
        lowmem_fast = postmem[fast_begin:fast_end]
        alias = postmem.index("b06v6_transactional_smoke", fast_end)
        high_fast_begin = postmem.index(
            "#if CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS", alias
        )
        high_fast_end = postmem.index("#else", high_fast_begin)
        high_fast = postmem[high_fast_begin:high_fast_end]

        self.assertLess(exact_gate, mtrr_gate)
        self.assertLess(mtrr_gate, fast_begin)
        self.assertIn("EXPERIMENTAL B06WG FAST_POSTMEM", lowmem_fast)
        self.assertIn("RAM/QPI ASSUMED_STABLE", lowmem_fast)
        lowmem = postmem.index(
            "b06wg_clear_window_once_sparse(X58_B06VF_LOWMEM_BASE"
        )
        cbmem = postmem.index(
            "b06wg_clear_window_once_sparse(X58_B06V6_CBMEM_BASE"
        )
        objects = postmem.index(
            "b06wg_clear_window_once_sparse(X58_B06V6_OBJECT_BASE"
        )
        self.assertEqual(
            (lowmem, alias, cbmem, objects),
            tuple(sorted((alias, lowmem, cbmem, objects))),
        )
        self.assertNotIn("b06v6_destructive_window_test", lowmem_fast)
        self.assertNotIn("b06v6_destructive_window_test", high_fast)
        self.assertIn("b06v6_destructive_window_test", postmem[fast_end:])

        helper = function_text(HANDOFF, "b06wg_clear_window_once_sparse")
        self.assertIn("B06WG_CLEAR_READBACK_COUNT 9u", HANDOFF)
        self.assertEqual(helper.count("write32p("), 1)
        self.assertEqual(helper.count("read32p("), 1)
        self.assertIn("for (uintptr_t address = base; address < end; address += 4)", helper)
        self.assertIn("for (size_t i = 0; i < ARRAY_SIZE(readback_offsets); i++)", helper)
        self.assertIn("write32p(address, 0);", helper)
        self.assertNotIn("b06v6_window_pattern", helper)

    def test_build_scripts_select_and_verify_b06wg(self) -> None:
        self.assertTrue(WRAPPER.exists())
        self.assertTrue(WRAPPER.stat().st_mode & 0o111)
        self.assertIn("b06wg", WRAPPER.read_text())
        for token in (
            'b06wg)',
            f'build_id="{BUILD_ID}"',
            "CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS=y",
            "CONFIG_X58_PRO_E_B06WF_USB_LAB=n",
            "CONFIG_DEBUG_USB_TRACE=y",
            "CONFIG_USB_KEYBOARD=y",
        ):
            self.assertIn(token, BUILDER)


if __name__ == "__main__":
    unittest.main()
