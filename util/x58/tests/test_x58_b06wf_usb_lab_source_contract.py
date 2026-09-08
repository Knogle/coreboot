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
SOURCE = (BOARD / "b06wf_usb_lab.c").read_text()
HEADER = (BOARD / "b06wf_usb_lab.h").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
BOOTBLOCK = (BOARD / "bootblock.c").read_text()
ROMSTAGE = (BOARD / "romstage.c").read_text()
PCI = (BOARD / "b06vn_pci.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
USB_ADMISSION = (BOARD / "b06wc_usb_admit.c").read_text()
BASE_CONFIG = (ROOT / "configs/x58-pro-e-b06we.config").read_text()
IMAGE_CONFIG = (ROOT / "configs/x58-pro-e-b06wf.config").read_text()
BUILDER = (ROOT / "scripts/build_x58_b06_sata_successor.sh").read_text()
WRAPPER = ROOT / "scripts/build_x58_b06wf.sh"

SYMBOL = "CONFIG_X58_PRO_E_B06WF_USB_LAB"
ID = "X58PROE-B06WF-LATE-USB-LAB-20260907"
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


class B06WFUsbLabSourceContractTests(unittest.TestCase):
    def test_variant_is_default_off_and_separate_from_b06we(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06WF_USB_LAB",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        compact = " ".join(option.split())
        for fragment in (
            "depends on X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX",
            "default n",
            "after normal device initialization and before ACPI tables",
            "performs no automatic hardware mutation",
            "exposes no generic register writer",
            "socketed-flash recovery path",
        ):
            self.assertIn(fragment, compact)

        for first, last in (
            ("config DEVICETREE", "config X58_PRO_E_B06M"),
            ("config PAYLOAD_CONFIGFILE", "config MAINBOARD_PART_NUMBER"),
        ):
            section = between(KCONFIG, first, last)
            self.assertLess(
                section.index(SYMBOL.removeprefix("CONFIG_")),
                section.index("X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX"),
            )
        part_number = KCONFIG[KCONFIG.index("config MAINBOARD_PART_NUMBER") :]
        self.assertLess(
            part_number.index(SYMBOL.removeprefix("CONFIG_")),
            part_number.index("X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX"),
        )
        self.assertIn(
            "ramstage-$(CONFIG_X58_PRO_E_B06WF_USB_LAB) += b06wf_usb_lab.c",
            MAKEFILE,
        )

    def test_config_changes_only_identity_and_new_lab_selection(self) -> None:
        base = config_contract(BASE_CONFIG)
        image = config_contract(IMAGE_CONFIG)
        allowed = {SYMBOL, "CONFIG_MAINBOARD_PART_NUMBER", "CONFIG_LOCALVERSION"}
        self.assertEqual(
            {key: value for key, value in base.items() if key not in allowed},
            {key: value for key, value in image.items() if key not in allowed},
        )
        self.assertEqual(image[SYMBOL], f"{SYMBOL}=y")
        self.assertEqual(
            image["CONFIG_LOCALVERSION"], 'CONFIG_LOCALVERSION="x58-pro-e-b06wf"'
        )

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
            "CONFIG_X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX"))
        self.assertIn("experimental B06WF late USB diagnostic lab", MAINBOARD)

    def test_monitor_is_late_and_has_no_automatic_mutation(self) -> None:
        monitor = function_text(RAMMON, "b06wf_late_usb_monitor")
        begin = function_text(SOURCE, "b06wf_usb_lab_begin")
        self.assertIn(
            "BOOT_STATE_INIT_ENTRY(BS_WRITE_TABLES, BS_ON_ENTRY,\n"
            "\tb06wf_late_usb_monitor, NULL);",
            RAMMON,
        )
        self.assertNotIn("BOOT_STATE_INIT_ENTRY(BS_DEV_ENABLE", RAMMON)
        self.assertIn("BOOT_STATE_INIT_ENTRY(BS_PRE_DEVICE, BS_ON_ENTRY", RAMMON)
        self.assertLess(monitor.index("b06wf_usb_lab_begin();"),
                        monitor.index('printk(BIOS_INFO, "x58-usb> ");'))
        self.assertIn('!strcmp(argv[0], "continue")', monitor)
        self.assertIn("b06wf_usb_lab_can_continue()", monitor)
        self.assertIn("b06wf_usb_lab_lock();", monitor)
        self.assertIn("b06wf_usb_lab_command(argc, argv)", monitor)
        self.assertIn("b06v6_command(argc, argv)", monitor)
        self.assertIn("AUTO_WRITES=0", begin)
        for writer in ("outl(", "outw(", "write32p(", "pci_io_write_config"):
            self.assertNotIn(writer, begin)

    def test_preflight_covers_exact_global_and_controller_state(self) -> None:
        for token in (
            "B06WF_LPC_ID\t\t0x3a168086u",
            "B06WF_FD_EXPECTED\t0x02000001u",
            "B06WF_CG_EXPECTED\t0x00000000u",
            "B06WF_PPO_EXPECTED\t0x0000u",
            "B06WF_MAP_EXPECTED\t0x00000000u",
            "B06WF_UPRWC_EXPECTED\t0x0000u",
            "B06WF_GPIO_USE1_EXPECTED\t0x197e75ffu",
            "B06WF_GPIO_USE2_EXPECTED\t0x030300ffu",
            "B06WF_GPIO_DIR2_EXPECTED\t0x0f55fff0u",
            "B06WF_GPIO_LVL2_EXPECTED\t0x15ff00d3u",
            "B06WF_GPIO_LVL2_STABLE_MASK 0x0002000fu",
            "B06WF_GPIO57\t\tBIT(25)",
            "B06WF_EHCI_LEGACY_CTL_EXPECTED 0xc0040000u",
            "B06WF_EHCIIR2_EXPECTED\t0x2002170au",
            "B06WF_EHCI_USBCMD_EXPECTED 0x00080000u",
            "B06WF_EHCI_USBSTS_EXPECTED 0x00001004u",
            "B06WF_EHCI_PORT_EXPECTED\t0x00003030u",
            "B06WF_UHCI_PORT_EXPECTED\t0x0c80u",
            "PCI_DEV(0, 0x1a, 7), 0x3a3c8086u",
            "PCI_DEV(0, 0x1d, 7), 0x3a3a8086u",
            "PCI_DEV(0, 0x1a, 0), 0x3a378086u",
            "PCI_DEV(0, 0x1d, 2), 0x3a368086u",
        ):
            self.assertIn(token, SOURCE)

        global_snapshot = function_text(SOURCE, "b06wf_global_snapshot")
        decode_gate = global_snapshot.index("snapshot->lpc_id != B06WF_LPC_ID")
        self.assertLess(decode_gate, global_snapshot.index(
            "read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_FD)"))
        self.assertLess(decode_gate, global_snapshot.index(
            "inl(DEFAULT_GPIOBASE + GP_IO_USE_SEL)"))
        self.assertIn("snapshot->gpio_lvl2 & B06WF_GPIO_LVL2_STABLE_MASK",
                      global_snapshot)
        self.assertIn("snapshot->gpio_lvl2 & B06WF_GPIO57",
                      global_snapshot)
        self.assertNotIn("snapshot->gpio_lvl2 == B06WF_GPIO_LVL2_EXPECTED",
                         global_snapshot)

        collect = function_text(SOURCE, "b06wf_collect")
        self.assertLess(collect.index("b06wf_global_snapshot"),
                        collect.index("b06wf_ehci_snapshot"))
        self.assertLess(collect.index("b06wf_ehci_snapshot"),
                        collect.index("b06wf_uhci_snapshot"))
        for name in ("b06wf_ehci_snapshot", "b06wf_uhci_snapshot"):
            body = function_text(SOURCE, name)
            self.assertLess(body.index("id != desc->id"), body.index("base ="))
            self.assertIn("snapshot->command != PCI_COMMAND_", body)

    def test_gpio56_probes_are_separately_armed_bounded_and_rolled_back(self) -> None:
        command = function_text(SOURCE, "b06wf_usb_lab_command")
        arm = function_text(SOURCE, "b06wf_arm")
        probe = function_text(SOURCE, "b06wf_gpio56_test")
        restore = function_text(SOURCE, "b06wf_restore_gpio")
        for token in (
            '"GPIO56-HIGH"',
            '"GPIO56-LOW"',
            '!strcmp(argv[2], "high")',
            '!strcmp(argv[2], "low")',
        ):
            self.assertIn(token, command)
        self.assertLess(arm.index("b06wf_usb_lab_lock();"), arm.index("*arm = true;"))
        self.assertIn("lab.gpio_high_armed = false;", probe)
        self.assertIn("lab.gpio_low_armed = false;", probe)
        self.assertIn("b06wf_ports_are_exact_oc_reset(&before)", probe)
        self.assertIn("b06wf_gpio2_msb_write_target(GP_IO_SEL2", probe)
        self.assertIn("B06WF_GPIO56_BYTE, false", probe)
        self.assertIn("b06wf_gpio2_msb_write_target(GP_LVL2", probe)
        for delay in ("udelay(10000);", "udelay(90000);", "udelay(400000);"):
            self.assertIn(delay, probe)
        self.assertIn('b06wf_sample_ports("GPIO56-T+500MS"', probe)
        self.assertIn("b06wf_restore_gpio(&before, level_written)", probe)
        self.assertLess(
            restore.index("b06wf_gpio2_msb_write_target(GP_LVL2"),
            restore.index("b06wf_gpio2_msb_write_target(GP_IO_SEL2"),
        )
        self.assertIn("Attempt input restoration even when level readback", restore)
        self.assertIn("!level_exact || !direction_exact", restore)
        self.assertIn("B06WF_GPIO_LVL2_STABLE_MASK | B06WF_GPIO56", restore)
        self.assertIn("lab.rollback_failed = true;", restore)

    def test_gpio57_pwrgd_uses_masked_vendor_order_and_persists(self) -> None:
        command = function_text(SOURCE, "b06wf_usb_lab_command")
        probe = function_text(SOURCE, "b06wf_gpio57_pwrgd_test")
        restore = function_text(SOURCE, "b06wf_restore_gpio57")
        all_oca = function_text(SOURCE, "b06wf_all_usb_oca_clear")
        continuation = function_text(SOURCE, "b06wf_usb_lab_can_continue")

        for token in (
            '"GPIO57-PWRGD"',
            '!strcmp(argv[2], "pwrgd")',
            "b06wf_gpio57_pwrgd_test();",
        ):
            self.assertIn(token, command)
        for token in (
            "B06WF_GPIO57_XRS_DELAY_PAUSES 0x00010000u",
            "B06WF_GPIO57_LOW_SETTLE_US 0x00010000u",
            "uncalibrated PAUSE iterations (different units)",
            "lab.gpio57_armed = false;",
            "b06wf_ports_are_exact_oc_reset(&before)",
            'b06wf_sample_ports("GPIO57-LOW-T+65536US"',
            'b06wf_sample_ports("GPIO57-HIGH-T+500MS"',
            "b06wf_all_usb_oca_clear(&before)",
            "lab.gpio57_persistent = true;",
            "POST_B06WF_PWRGD_READY",
        ):
            self.assertIn(token, SOURCE if token.startswith("B06WF_GPIO57_") else probe)

        direction = probe.index("b06wf_gpio2_msb_write_target(GP_IO_SEL2")
        settle = probe.index("udelay(B06WF_GPIO57_LOW_SETTLE_US)")
        high = probe.index("b06wf_gpio2_msb_write_target(GP_LVL2")
        persistent = probe.index("lab.gpio57_persistent = true;")
        self.assertLess(direction, settle)
        self.assertLess(settle, high)
        self.assertLess(high, persistent)
        self.assertLess(
            restore.index("b06wf_gpio2_msb_write_target(GP_LVL2"),
            restore.index("b06wf_gpio2_msb_write_target(GP_IO_SEL2"),
        )
        self.assertIn("Attempt input restoration even when level readback", restore)
        self.assertIn("!level_exact || !direction_exact", restore)
        self.assertIn("B06WF_EHCI_PORT_OCA", all_oca)
        self.assertIn("B06WF_UHCI_PORT_OCA", all_oca)
        self.assertNotIn("B06WF_EHCI_PORT_OCC", all_oca)
        self.assertNotIn("B06WF_UHCI_PORT_OCI", all_oca)
        self.assertEqual(probe.count("b06wf_gpio2_msb_write_target("), 2)
        self.assertNotIn("write32p(", probe)
        self.assertNotIn("outw(", probe)
        self.assertNotIn("B06WF_EHCI_PORTSC +", probe)
        self.assertNotIn("B06WF_UHCI_PORTSC1 +", probe)
        self.assertIn("lab.gpio57_persistent && !b06wf_all_usb_oca_clear", continuation)
        self.assertLess(continuation.index('b06wf_collect(&current, "CONTINUE-GATE")'),
                        continuation.index("b06wf_all_usb_oca_clear(&current)"))
        self.assertIn("GPIO57_PERSIST=%u", continuation)
        self.assertNotIn("GPIO59", SOURCE)
        self.assertNotIn("OC0#", SOURCE)

    def test_owner_probe_writes_only_configflag_and_restores_it(self) -> None:
        owner = function_text(SOURCE, "b06wf_owner_test")
        restore = function_text(SOURCE, "b06wf_restore_configflags")
        self.assertIn("lab.owner_armed = false;", owner)
        self.assertIn("b06wf_ports_are_exact_oc_reset(&before)", owner)
        self.assertIn("CONFIGFLAG 0->1->0; no PORTSC write", owner)
        self.assertIn("B06WF_EHCI_CONFIGFLAG, 1", owner)
        self.assertIn("before.ehci[i].configflag", owner)
        self.assertIn("CONFIGFLAG-one-readback", owner)
        self.assertIn("CONFIGFLAG-zero-readback", owner)
        self.assertIn("b06wf_restore_configflags(&before)", owner)
        self.assertNotIn("B06WF_EHCI_PORTSC +", owner)
        self.assertNotIn("outw(", owner)
        self.assertIn("lab.rollback_failed = true;", restore)

    def test_oc_ack_is_fixed_w1c_nonreversible_and_reset_only(self) -> None:
        ack = function_text(SOURCE, "b06wf_oc_ack_test")
        continuation = function_text(SOURCE, "b06wf_usb_lab_can_continue")
        for token in (
            "B06WF_EHCI_OC_ACK_WRITE\t0x00003020u",
            "B06WF_EHCI_OC_ACK_EXPECTED 0x00003010u",
            "B06WF_UHCI_OC_ACK_WRITE\t0x0800u",
            "B06WF_UHCI_OC_ACK_EXPECTED 0x0480u",
        ):
            self.assertIn(token, SOURCE)
        self.assertIn("lab.oc_ack_armed = false;", ack)
        self.assertIn("b06wf_ports_are_exact_oc_reset(&before)", ack)
        self.assertLess(
            ack.index("lab.nonreversible_dirty = true;"),
            ack.index("write32p("),
        )
        self.assertIn("B06WF_EHCI_OC_ACK_WRITE", ack)
        self.assertIn("B06WF_UHCI_OC_ACK_WRITE", ack)
        self.assertNotRegex(
            ack,
            r"(?:write32p|outw)\s*\([^;]*(?:read32p|inw)\s*\(",
        )
        self.assertIn("lab.nonreversible_dirty", continuation)
        self.assertIn("reset required", SOURCE)

    def test_failures_latch_and_continue_requires_exact_rollback(self) -> None:
        fail = function_text(SOURCE, "b06wf_fail")
        continuation = function_text(SOURCE, "b06wf_usb_lab_can_continue")
        self.assertIn("lab.fault_latched = true;", fail)
        for token in (
            "!lab.baseline_valid",
            "lab.mutation_active",
            "lab.fault_latched",
            "lab.rollback_failed",
            "lab.nonreversible_dirty",
            'b06wf_collect(&current, "CONTINUE-GATE")',
            "b06wf_controls_equal(&current, &lab.baseline)",
        ):
            self.assertIn(token, continuation)
        self.assertLess(continuation.index("b06wf_usb_lab_lock();"),
                        continuation.index("!lab.baseline_valid"))
        self.assertIn("POST_B06WF_CONTINUE", continuation)

    def test_no_generic_register_writer_and_old_admission_remains_read_only(self) -> None:
        for forbidden in (
            "pci_io_write_config",
            "write8p(",
            "write16p(",
            "RCBA32(",
            "RCBA16(",
            '"mw"',
            '"mmio-write"',
            '"pci-write"',
            '"io-write"',
        ):
            self.assertNotIn(forbidden, SOURCE)
        self.assertEqual(SOURCE.count("write32p("), 4)
        self.assertEqual(SOURCE.count("outl("), 0)
        self.assertEqual(SOURCE.count("outw("), 1)
        self.assertEqual(SOURCE.count("outb("), 1)
        self.assertNotIn("pci_write_config", SOURCE)
        gpio_writer = function_text(SOURCE, "b06wf_gpio2_msb_write_target")
        self.assertIn("outb(target, port);", gpio_writer)
        self.assertIn("!!(inb(port) & mask) == set", gpio_writer)
        self.assertIn("No generic write command exists", SOURCE)
        for writer in (
            "pci_io_write_config",
            "write8p(",
            "write16p(",
            "write32p(",
            "outb(",
            "outw(",
            "outl(",
        ):
            self.assertNotIn(writer, USB_ADMISSION)

        monitor = function_text(RAMMON, "b06wf_late_usb_monitor")
        self.assertIn("b06v6_command(argc, argv);", monitor)
        self.assertIn('!strcmp(argv[1], "RESET")', RAMMON)
        self.assertIn("b06v6_cf9_reset", RAMMON)

    def test_header_builder_and_wrapper_contract(self) -> None:
        for name in (
            "b06wf_usb_lab_begin",
            "b06wf_usb_lab_help",
            "b06wf_usb_lab_lock",
            "b06wf_usb_lab_command",
            "b06wf_usb_lab_can_continue",
        ):
            self.assertIn(name, HEADER)
        self.assertTrue(WRAPPER.is_file())
        self.assertTrue(os.access(WRAPPER, os.X_OK))
        self.assertRegex(
            WRAPPER.read_text(),
            r'exec .*build_x58_b06_sata_successor\.sh" b06wf\s*$',
        )
        for token in (
            "b06we|b06wf",
            f'build_id="{ID}"',
            "CONFIG_X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX=y",
            "CONFIG_X58_PRO_E_B06WF_USB_LAB=y",
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
