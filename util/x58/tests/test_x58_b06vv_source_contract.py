#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
ICH10 = COREBOOT / "src/southbridge/intel/i82801jx"
KCONFIG = (BOARD / "Kconfig").read_text()
BOOTBLOCK = (BOARD / "bootblock.c").read_text()
ROMSTAGE = (BOARD / "romstage.c").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
PCI = (BOARD / "b06vn_pci.c").read_text()
DEVICETREE = (BOARD / "devicetree_b06vv.cb").read_text()
PORT_HELPER = (ICH10 / "sata_port_enable.c").read_text()
CLOCK_HELPER = (ICH10 / "sata_clock_field.c").read_text()
BASE_CONFIG = (ROOT / "configs/x58-pro-e-b06vu.config").read_text()
IMAGE_CONFIG = (ROOT / "configs/x58-pro-e-b06vv.config").read_text()
BUILDER = (ROOT / "scripts/build_x58_b06_sata_successor.sh").read_text()
WRAPPER = ROOT / "scripts/build_x58_b06vv.sh"

SYMBOL = "CONFIG_X58_PRO_E_B06VV_ICH10_PCS_SCLK"
ID = "X58PROE-B06VV-ICH10-PCS-SCLK-20260906"
BASE_ID = "X58PROE-B06VU-ICH10-AHCI-ROUTE-20260906"


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


class B06VVCorrectedPcsClockSourceContractTests(unittest.TestCase):
    def test_variant_is_default_off_and_composes_b06vu(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VV_ICH10_PCS_SCLK",
            "config X58_PRO_E_B06VW_ICH10_AHCI_MMIO",
        )
        for fragment in (
            "depends on X58_PRO_E_B06VU_ICH10_AHCI_ROUTE",
            "select SOUTHBRIDGE_INTEL_I82801JX_SATA_PORT_ENABLE",
            "select SOUTHBRIDGE_INTEL_I82801JX_SATA_CLOCK_FIELD",
            "default n",
            "PCS[5:0] from exact 00 to 3f",
            "SCLKCG[8:0] from exact",
            "Each stage is attempted once",
            "FD=02000001/FDSW=00/F5-absent",
            "enable PCI decode or bus mastering",
            "access ABAR, set GHC or PI",
        ):
            self.assertIn(fragment, option)
        self.assertNotIn("select SOUTHBRIDGE_INTEL_I82801JX\n", option)

    def test_identity_and_topology_are_distinct(self) -> None:
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON, PCI):
            self.assertLess(source.index(ID), source.index(BASE_ID))
        self.assertLess(MAINBOARD.index(SYMBOL), MAINBOARD.index("B06VU_ICH10_AHCI_ROUTE"))
        self.assertIn('default "devicetree_b06vv.cb"', KCONFIG)
        self.assertEqual(DEVICETREE.count("device pci"), 13)
        self.assertIn("device pci 1f.2 mandatory ops b06vn_endpoint_ops", DEVICETREE)
        self.assertNotRegex(DEVICETREE, r"device pci 1f\.5\b")

    def test_config_is_b06vu_plus_only_the_named_successor(self) -> None:
        base = config_contract(BASE_CONFIG)
        image = config_contract(IMAGE_CONFIG)
        allowed = {
            SYMBOL,
            "CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO",
            "CONFIG_MAINBOARD_PART_NUMBER",
            "CONFIG_LOCALVERSION",
        }
        self.assertEqual(
            {k: v for k, v in base.items() if k not in allowed},
            {k: v for k, v in image.items() if k not in allowed},
        )
        self.assertEqual(image[SYMBOL], f"{SYMBOL}=y")
        self.assertEqual(
            image["CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO"],
            "# CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO is not set",
        )
        self.assertEqual(image["CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT"],
                         "CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT=y")

    def test_helpers_have_exact_selected_field_write_scope(self) -> None:
        ports = function_text(PORT_HELPER, "i82801jx_sata_enable_all_ports")
        self.assertEqual(ports.count("pci_io_write_config8("), 1)
        self.assertIn("I82801JX_SATA_PCS_PORT_ENABLE_MASK", ports)
        for forbidden in ("write_config16", "write_config32", "SATA_MAP", "SATA_ABAR"):
            self.assertNotIn(forbidden, ports)
        clock = function_text(CLOCK_HELPER, "i82801jx_sata_program_clock_field")
        self.assertEqual(clock.count("pci_io_write_config32("), 1)
        self.assertIn("I82801JX_SATA_SCLKCG_FIELD1_MASK", clock)
        self.assertIn("I82801JX_SATA_SCLKCG_FIELD1_REQUIRED", clock)
        for forbidden in ("SATA_PCS", "SATA_MAP", "SATA_ABAR", "RCBA"):
            self.assertNotIn(forbidden, clock)

    def test_pcs_and_clock_are_ordered_one_attempt_fail_closed(self) -> None:
        pcs = function_text(PCI, "b06vv_program_pcs_once")
        clock = function_text(PCI, "b06vv_program_sclk_once")
        assigned = function_text(PCI, "b06vn_resources_assigned")
        for body, attempted, begin, ok in (
            (pcs, "b06vv_pcs_attempted = true;", "POST_B06VV_PCS_BEGIN", "POST_B06VV_PCS_OK"),
            (clock, "b06vv_sclk_attempted = true;", "POST_B06VV_SCLK_BEGIN", "POST_B06VV_SCLK_OK"),
        ):
            self.assertIn(attempted, body)
            self.assertEqual(body.count(f"post_code({begin});"), 1)
            self.assertEqual(body.count(f"post_code({ok});"), 1)
            self.assertIn("die_with_post_code(POST_B06VV_FAIL", body)
            self.assertNotRegex(body, r"\b(?:write8p|write16p|write32p|pci_io_write_config)\s*\(")
        self.assertEqual(pcs.count("i82801jx_sata_enable_all_ports("), 1)
        self.assertEqual(clock.count("i82801jx_sata_program_clock_field("), 1)
        self.assertIn("b06vv_pcs_attempted", clock)
        self.assertLess(assigned.index("b06vv_program_pcs_once();"),
                        assigned.index("b06vv_program_sclk_once();"))
        self.assertIn(
            "#if CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO\n\tb06vw_program_mmio_once();\n#endif",
            assigned,
        )

    def test_full_route_resource_and_no_mmio_contract(self) -> None:
        gate = function_text(PCI, "b06vv_snapshot_is_exact")
        for fragment in (
            "b06vu_fixed_gates_are_exact(snapshot)",
            "snapshot->fd == B06VU_FD_SATA2_DISABLED",
            "(sata->command & decode) == PCI_COMMAND_MEMORY",
            "snapshot->f2_command == expected_command",
            "b06vs_abar_resource_is_exact(abar, snapshot->f2_bar5)",
            "snapshot->f5_id == 0xffffffffu",
        ):
            self.assertIn(fragment, gate)
        pcs = function_text(PCI, "b06vv_program_pcs_once")
        clock = function_text(PCI, "b06vv_program_sclk_once")
        for forbidden in ("B06VW_AHCI", "read32p(", "write32p(", "PCI_COMMAND_MEMORY);"):
            self.assertNotIn(forbidden, pcs + clock)

    def test_post_codes_and_release_builder_are_explicit(self) -> None:
        for name, value in {
            "POST_B06VV_PCS_BEGIN": "0x5e",
            "POST_B06VV_PCS_OK": "0x5f",
            "POST_B06VV_SCLK_BEGIN": "0x60",
            "POST_B06VV_SCLK_OK": "0x61",
            "POST_B06VV_READY": "0x62",
            "POST_B06VV_FAIL": "0x63",
        }.items():
            self.assertRegex(PCI, rf"#define {name}\s+{value}")
        self.assertTrue(WRAPPER.is_file())
        self.assertIn("b06vv", WRAPPER.read_text())
        for fragment in (
            "{b06vv|b06vw|b06vx}",
            "env -u DEBUG make -C",
            "clean builds are not byte-identical",
            "CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT=y",
            "CONFIG_SOUTHBRIDGE_INTEL_I82801JX=n",
            "msi-x58-pro-e-${variant}-deterministic-w25q128-16MiB.rom",
        ):
            self.assertIn(fragment, BUILDER)


if __name__ == "__main__":
    unittest.main()
