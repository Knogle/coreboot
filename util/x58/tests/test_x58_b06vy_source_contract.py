#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
KCONFIG = (BOARD / "Kconfig").read_text()
BOOTBLOCK = (BOARD / "bootblock.c").read_text()
ROMSTAGE = (BOARD / "romstage.c").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
PCI = (BOARD / "b06vn_pci.c").read_text()
BASE_TREE = (BOARD / "devicetree_b06vx.cb").read_text()
IMAGE_TREE = (BOARD / "devicetree_b06vy.cb").read_text()
BASE_CONFIG = (ROOT / "configs/x58-pro-e-b06vx.config").read_text()
IMAGE_CONFIG = (ROOT / "configs/x58-pro-e-b06vy.config").read_text()
BUILDER = (ROOT / "scripts/build_x58_b06_sata_successor.sh").read_text()
WRAPPER = ROOT / "scripts/build_x58_b06vy.sh"

SYMBOL = "CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK"
ID = "X58PROE-B06VY-ICH10-IOAPIC-MASK-20260906"
BASE_ID = "X58PROE-B06VX-AHCI-USBTRACE-20260906"


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


class B06VYIoapicMaskSourceContractTests(unittest.TestCase):
    def test_variant_is_separate_default_off_b06vx_successor(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VY_IOAPIC_MASK",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        for fragment in (
            "depends on X58_PRO_E_B06VX_AHCI_USB_TRACE",
            "default n",
            "maximum redirection entry 17h",
            "24 initial redirection entries to be masked",
            "high 00000000 and low 00010000",
            "IOREGSEL",
            "does not install an IOAPIC ExtINT route",
            "IOAPIC MRE lock",
            "route PIRQ or SCI",
            "enable HPET",
            "emit ACPI",
            "add fixed resources",
            "enable the broad historical ICH10 driver",
        ):
            self.assertIn(fragment, option)
        self.assertNotIn("select SOUTHBRIDGE_INTEL_I82801JX", option)

    def test_identity_and_topology_precede_inherited_b06vx(self) -> None:
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON, PCI):
            self.assertLess(source.index(ID), source.index(BASE_ID))
        self.assertLess(MAINBOARD.index(SYMBOL), MAINBOARD.index("B06VX_AHCI_USB_TRACE"))
        self.assertIn('default "devicetree_b06vy.cb"', KCONFIG)
        self.assertEqual(IMAGE_TREE.count("device pci"), 13)
        self.assertEqual(
            re.sub(r"# B06VY[^\n]*", "", IMAGE_TREE),
            re.sub(r"# B06VX[^\n]*", "", BASE_TREE),
        )

    def test_outer_config_only_selects_the_new_identity(self) -> None:
        base = config_contract(BASE_CONFIG)
        image = config_contract(IMAGE_CONFIG)
        allowed = {SYMBOL, "CONFIG_MAINBOARD_PART_NUMBER", "CONFIG_LOCALVERSION"}
        self.assertEqual(
            {key: value for key, value in base.items() if key not in allowed},
            {key: value for key, value in image.items() if key not in allowed},
        )
        self.assertEqual(image[SYMBOL], f"{SYMBOL}=y")
        self.assertEqual(image["CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE"],
                         "CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE=y")

    def test_exact_decode_identity_version_and_count_gates(self) -> None:
        for fragment in (
            "#define B06VY_LPC_ID\t\t0x3a168086u",
            "#define B06VY_OIC_DISABLED\t0x00u",
            "#define B06VY_OIC_ENABLED\t0x03u",
            "#define B06VY_IOAPIC_ID_TARGET\t0x00000000u",
            "#define B06VY_IOAPIC_VERSION_TARGET 0x00170020u",
            "#define B06VY_IOAPIC_MAX_REDIR\t0x17u",
            "#define B06VY_IOAPIC_REDIR_COUNT 24u",
            "#define B06VY_IOAPIC_LOW_TARGET\t0x00010000u",
            "#define B06VY_IOAPIC_HIGH_TARGET 0x00000000u",
        ):
            self.assertIn(fragment, PCI)
        body = function_text(PCI, "b06vy_decode_and_mask_ioapic_once")
        self.assertIn("lpc_id != B06VY_LPC_ID", body)
        self.assertIn("rcba != B06VY_RCBA_ENABLED", body)
        self.assertIn("oic != B06VY_OIC_DISABLED && oic != B06VY_OIC_ENABLED", body)
        self.assertIn("ioapic_id != B06VY_IOAPIC_ID_TARGET", body)
        self.assertIn("ioapic_version != B06VY_IOAPIC_VERSION_TARGET", body)

    def test_every_entry_must_start_masked_then_gets_exact_pair_readback(self) -> None:
        snapshot = function_text(PCI, "b06vy_redirection_snapshot")
        body = function_text(PCI, "b06vy_decode_and_mask_ioapic_once")
        writer = function_text(PCI, "b06vy_ioapic_write_exact")
        self.assertIn("entry < B06VY_IOAPIC_REDIR_COUNT", snapshot)
        self.assertIn("all_masked &=", snapshot)
        self.assertIn("PRE_ENTRY_UNMASKED", body)
        self.assertIn("ENTRY_CHANGED_OR_UNMASKED", body)
        self.assertLess(body.index("B06VY_IOAPIC_HIGH_TARGET"),
                        body.index("B06VY_IOAPIC_LOW_TARGET"))
        self.assertIn("POST_ENTRY_NONCANONICAL", body)
        self.assertIn("read32p(B06VY_IOAPIC_BASE + B06VY_IOWIN) == value", writer)
        self.assertIn("FINAL_SELECTOR_RESTORE", body)
        self.assertIn("b06vy_ioapic_select(B06VY_IOAPIC_ID_REG)", body)
        for marker in (
            "IOAPIC_EXTINT_ROUTE=0",
            "MRE_LOCK_WRITE=0",
            "PIRQ_WRITE=0",
            "SCI_WRITE=0",
        ):
            self.assertIn(marker, body)

    def test_hook_precedes_root_preflight_and_adds_no_routing_or_resources(self) -> None:
        scan = function_text(PCI, "b06vn_domain_scan_bus")
        resources = function_text(PCI, "b06vn_read_resources")
        self.assertLess(scan.index("b06vu_program_ahci_route_once"),
                        scan.index("b06vy_decode_and_mask_ioapic_once"))
        self.assertLess(scan.index("b06vy_decode_and_mask_ioapic_once"),
                        scan.index("b06vn_raw_root_preflight"))
        self.assertNotIn(SYMBOL, resources)
        self.assertEqual(resources.count("\tram_range("), 1)
        self.assertEqual(resources.count("\tmmio_range("), 1)
        self.assertNotRegex(
            function_text(PCI, "b06vy_decode_and_mask_ioapic_once"),
            r"\b(?:setup_ioapic|register_new_ioapic|ioapic_set_max_vectors|acpi_|hpet_)\w*\s*\(",
        )

    def test_builder_and_wrapper_pin_the_inherited_trace_payload(self) -> None:
        self.assertTrue(WRAPPER.is_file())
        self.assertIn("b06vy", WRAPPER.read_text())
        for fragment in (
            "b06vv|b06vw|b06vx|b06vy",
            'build_id="X58PROE-B06VY-ICH10-IOAPIC-MASK-20260906"',
            "CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK=y",
            "CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE=y",
            "CONFIG_HAVE_ACPI_TABLES=n",
            "CONFIG_SOUTHBRIDGE_INTEL_I82801JX=n",
            "CONFIG_DEBUG_USB_TRACE=y",
        ):
            self.assertIn(fragment, BUILDER)


if __name__ == "__main__":
    unittest.main()
