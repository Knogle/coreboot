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
ROUTE = (BOARD / "b06wi_irq_route.c").read_text()
ROUTE_HEADER = (BOARD / "b06wi_irq_route.h").read_text()
PRT = (BOARD / "b06wi_prt.asl").read_text()
ACPI = (BOARD / "acpi_tables.c").read_text()
ACPI_HEADER = (BOARD / "b06wc_acpi.h").read_text()
DSDT = (BOARD / "dsdt.asl").read_text()
BOOTBLOCK = (BOARD / "bootblock.c").read_text()
ROMSTAGE = (BOARD / "romstage.c").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
PCI = (BOARD / "b06vn_pci.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
WH_CONFIG = (ROOT / "configs/x58-pro-e-b06wh.config").read_text()
WI_CONFIG = (ROOT / "configs/x58-pro-e-b06wi.config").read_text()
BUILDER = (ROOT / "scripts/build_x58_b06_sata_successor.sh").read_text()
WRAPPER = ROOT / "scripts/build_x58_b06wi.sh"

WH_SYMBOL = "CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS"
WI_SYMBOL = "CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI"
WI_ID = "X58PROE-B06WI-VENDOR-IRQ-ACPI-20260907"
WH_ID = "X58PROE-B06WH-AUTO-USB-SEABIOS-20260907"


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


class B06WIVendorIrqAcpiSourceContractTests(unittest.TestCase):
    def test_config_is_exact_b06wh_inheritance_plus_new_selector(self) -> None:
        wh = config_contract(WH_CONFIG)
        wi = config_contract(WI_CONFIG)
        allowed = {WI_SYMBOL, "CONFIG_MAINBOARD_PART_NUMBER", "CONFIG_LOCALVERSION"}
        self.assertEqual(
            {key: value for key, value in wh.items() if key not in allowed},
            {key: value for key, value in wi.items() if key not in allowed},
        )
        self.assertEqual(wi[WH_SYMBOL], f"{WH_SYMBOL}=y")
        self.assertEqual(wi[WI_SYMBOL], f"{WI_SYMBOL}=y")
        self.assertEqual(
            wi["CONFIG_PAYLOAD_CONFIGFILE"],
            'CONFIG_PAYLOAD_CONFIGFILE="$(top)/src/mainboard/'
            '$(MAINBOARDDIR)/config_seabios_b06wh_usbtrace6"',
        )
        self.assertEqual(wi["CONFIG_SEABIOS_DEBUG_LEVEL"],
                         "CONFIG_SEABIOS_DEBUG_LEVEL=6")
        self.assertEqual(
            wi["CONFIG_SEABIOS_REVISION_ID"],
            'CONFIG_SEABIOS_REVISION_ID="5497f43189374647b3b0f282aa497e71c891f3c5"',
        )
        self.assertEqual(wi["CONFIG_LOCALVERSION"],
                         'CONFIG_LOCALVERSION="x58-pro-e-b06wi"')

    def test_variant_is_default_off_and_keeps_b06wh_payload(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06WI_VENDOR_IRQ_ACPI",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        compact = " ".join(option.split())
        for fragment in (
            "depends on X58_PRO_E_B06WH_QUIET_USB_SEABIOS",
            "depends on PAYLOAD_SEABIOS",
            "default n",
            "Every write is read back",
            "attempts rollback and blocks boot",
            "leaves every IOAPIC redirection entry masked",
            "writes no INT_LINE, GPIO or GPIO level register",
            "suppresses the unproved B06WD 8042/PNP0303 advertisement",
        ):
            self.assertIn(fragment, compact)
        self.assertIn(
            "ramstage-$(CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI) += "
            "b06wi_irq_route.c",
            MAKEFILE,
        )

    def test_identity_is_distinct_in_every_executable_stage(self) -> None:
        self.assertIn(f'#define B06WI_BUILD_ID "{WI_ID}"', ROUTE_HEADER)
        for source, expected_count in (
            (BOOTBLOCK, 1),
            (ROMSTAGE, 2),
            (RAMMON, 1),
        ):
            self.assertEqual(source.count(WI_ID), expected_count)
            self.assertLess(source.index(WI_ID), source.index(WH_ID))
        self.assertIn('#include "b06wi_irq_route.h"', PCI)
        self.assertIn("#define B06VN_BUILD_ID B06WI_BUILD_ID", PCI)
        ops = MAINBOARD[MAINBOARD.index("struct chip_operations") :]
        self.assertLess(ops.index(WI_SYMBOL), ops.index(WH_SYMBOL))
        self.assertIn("experimental B06WI exact IRQ/ACPI", ops)

    def test_route_writer_is_exact_gated_and_forbidden_domains_are_read_only(self) -> None:
        for token in (
            '{ D31IR, 0x3210u, 0xf0ffu, 0x0032u, 0x0232u, "D31IR" }',
            '{ D29IR, 0x3210u, 0xf0ffu, 0x0037u, 0x0237u, "D29IR" }',
            '{ D28IR, 0x3210u, 0x00ffu, 0x0001u, 0x3201u, "D28IR" }',
            '{ D27IR, 0x3210u, 0x000fu, 0x0006u, 0x3216u, "D27IR" }',
            '{ D26IR, 0x3210u, 0x00f0u, 0x0050u, 0x3250u, "D26IR" }',
            "B06WI_D26IP_F2_MASK\t0x00000f00u",
            "B06WI_D26IP_F2_PIN_D\t0x00000400u",
            "B06WI_IOAPIC_ID_TARGET\t0x01000000u",
            "B06WI_IOAPIC_REDIR_COUNT 24u",
            'b06wi_fail("RCBA_ROUTE_PRE", false)',
            'b06wi_fail("PCI_PIN_PRE", false)',
            'b06wi_fail("WRITE_READBACK", true)',
            'b06wi_fail("IOAPIC_POST", true)',
        ):
            self.assertIn(token, ROUTE)
        self.assertNotIn("pci_io_write_config", ROUTE)
        self.assertNotIn("outb(", ROUTE)
        self.assertIn(
            "PIRQ_WRITE=0 GPIO_WRITE=0 INT_LINE_WRITE=0 RTE_WRITE=0",
            ROUTE,
        )

    def test_route_runs_once_before_normal_pci_enumeration(self) -> None:
        enable = function_text(PCI, "b06vn_domain_scan_bus")
        call = enable.index("b06wi_program_vendor_irq_once();")
        scan = enable.index("pci_host_bridge_scan_bus(dev);")
        self.assertLess(call, scan)
        self.assertIn("static bool b06wi_attempted;", ROUTE)
        self.assertIn('b06wi_fail("SECOND_ATTEMPT", false)', ROUTE)
        self.assertIn("bool b06wi_vendor_irq_ready(void)", ROUTE)

    def test_acpi_contract_matches_programmed_irq_state(self) -> None:
        for token in (
            "fadt->gpe0_blk = B06WC_ACPI_PMBASE + B06WC_ACPI_GPE0_STS;",
            "fadt->gpe0_blk_len = B06WC_ACPI_GPE0_BLK_LEN;",
            "fadt->smi_cmd = 0;",
            "B06WC_ACPI_IOAPIC_ID_VALUE\t0x01000000",
            "B06WC_ACPI_IOAPIC_ID\t\t1",
            "0, 2, MP_IRQ_TRIGGER_EDGE | MP_IRQ_POLARITY_HIGH",
            'b06wc_acpi_fail("B06WI_IRQ_ROUTE_NOT_READY")',
        ):
            self.assertIn(token, ACPI + ACPI_HEADER)
        self.assertIn("CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT &&", ACPI)
        self.assertIn("!CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI", ACPI)
        self.assertIn('#include "b06wi_prt.asl"', DSDT)
        self.assertIn("!CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI", DSDT)

    def test_prt_contains_exact_root_and_bridge_direct_gsi_maps(self) -> None:
        for token in (
            "Package () { 0x001fffff, Zero, Zero, 18 }",
            "Package () { 0x001dffff, Zero, Zero, 23 }",
            "Package () { 0x001cffff, Zero, Zero, 17 }",
            "Package () { 0x001bffff, Zero, Zero, 22 }",
            "Package () { 0x001affff, One,  Zero, 21 }",
            "Device (NPE3)",
            "Name (_ADR, 0x00030000)",
            "Device (P0P8)",
            "Name (_ADR, 0x001c0004)",
        ):
            self.assertIn(token, PRT)
        self.assertEqual(PRT.count("Method (_PRT"), 1)
        self.assertEqual(PRT.count("Name (_PRT"), 2)

    def test_builder_wrapper_and_determinism_contract(self) -> None:
        self.assertTrue(WRAPPER.is_file())
        self.assertTrue(os.access(WRAPPER, os.X_OK))
        self.assertRegex(
            WRAPPER.read_text(),
            r'exec .*build_x58_b06_sata_successor\.sh" b06wi\s*$',
        )
        for token in (
            "b06wh|b06wi",
            f'build_id="{WI_ID}"',
            "CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI=y",
            "CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS=y",
            "CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS=n",
            'config_seabios_b06wh_usbtrace6"',
            "CONFIG_DEBUG_USB_TRACE=y",
            "CONFIG_DEBUG_LEVEL=6",
            "CONFIG_USB_KEYBOARD=y",
            "etc/usb-time-sigatt",
            "etc/ps2-keyboard-spinup",
            'build_once "${first_rom}"',
            'build_once "${second_rom}"',
            'cmp -s -- "${first_rom}" "${second_rom}"',
            'cmp -s -- "${first_ipxe}"',
            "verify-composite",
            "place_firmware_at_flash_top.py",
        ):
            self.assertIn(token, BUILDER)


if __name__ == "__main__":
    unittest.main()
