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
PCI_SOURCE = (BOARD / "b06vn_pci.c").read_text()
VO_DEVTREE = (BOARD / "devicetree_b06vo.cb").read_text()
VO_CONFIG = (ROOT / "configs/x58-pro-e-b06vo.config").read_text()
VO_BUILD = (ROOT / "scripts/build_x58_b06vo.sh").read_text()

VO_SYMBOL = "CONFIG_X58_PRO_E_B06VO_IOHBUSNO_ROUTE"
VN_SYMBOL = "CONFIG_X58_PRO_E_B06VN_IOU0_PCIE_PHYS_VBIOS"
VO_ID = "X58PROE-B06VO-IOHBUSNO-ROUTE-PROBE-20260906"
VN_ID = "X58PROE-B06VN-IOU0-HD5450-PHYSVBIOS-20260906"


def between(source: str, first: str, last: str) -> str:
    begin = source.index(first)
    end = source.index(last, begin)
    return source[begin:end]


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


class B06VOSourceContractTests(unittest.TestCase):
    def test_separate_default_off_successor_has_one_hypothesis(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VO_IOHBUSNO_ROUTE",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        for fragment in (
            "depends on X58_PRO_E_B06VN_IOU0_PCIE_PHYS_VBIOS",
            "default n",
            "0xe00a010a",
            "Accept only 0x0000",
            "01:0d.0 and 02:0d.0",
            "0, 1, 10 and 100 milliseconds",
            "no bridge-control reset",
            "GPU, interrupt, or payload write",
        ):
            self.assertIn(fragment, option)
        self.assertNotIn("default y", option)

    def test_tree_and_identity_priorities_precede_b06vn(self) -> None:
        tree = between(KCONFIG, "config DEVICETREE", "config X58_PRO_E_B06M")
        self.assertLess(tree.index("devicetree_b06vo.cb"), tree.index("devicetree_b06vn.cb"))
        pci_lines = re.findall(
            r"^\s*device pci ([0-9a-f]{2}\.[0-7]) mandatory ops (\w+)",
            VO_DEVTREE,
            re.MULTILINE,
        )
        self.assertEqual(len(pci_lines), 14)
        self.assertEqual([ops for _, ops in pci_lines].count("b06vn_root_port_ops"), 2)
        self.assertEqual([ops for _, ops in pci_lines].count("b06vn_endpoint_ops"), 12)
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON, MAINBOARD, PCI_SOURCE):
            self.assertIn(VO_SYMBOL if source == MAINBOARD else VO_ID, source)
            self.assertIn(VN_SYMBOL if source == MAINBOARD else VN_ID, source)
            self.assertLess(
                source.index(VO_SYMBOL if source == MAINBOARD else VO_ID),
                source.index(VN_SYMBOL if source == MAINBOARD else VN_ID),
            )

    def test_config_selects_b06vo_without_changing_payload_policy(self) -> None:
        for fragment in (
            "CONFIG_X58_PRO_E_B06VN_IOU0_PCIE_PHYS_VBIOS=y",
            "CONFIG_X58_PRO_E_B06VO_IOHBUSNO_ROUTE=y",
            'CONFIG_MAINBOARD_PART_NUMBER="X58 Pro-E B06VO IOHBUSNO routing probe"',
            "CONFIG_SEABIOS_HARDWARE_IRQ=y",
            'CONFIG_PXE_ROM_ID="10ec,8168"',
            "# CONFIG_VGA_ROM_RUN is not set",
            "CONFIG_NO_GFX_INIT=y",
            'CONFIG_LOCALVERSION="x58-pro-e-b06vo"',
        ):
            self.assertIn(fragment, VO_CONFIG)

        for fragment in (
            "configs/x58-pro-e-b06vo.config",
            "msi-x58-pro-e-b06vo-coreboot-base-4MiB.rom",
            "CONFIG_X58_PRO_E_B06VO_IOHBUSNO_ROUTE=y",
            "tests.test_x58_b06vo_source_contract",
            "X58_B06VO_SOURCE_DATE_EPOCH",
        ):
            self.assertIn(fragment, VO_BUILD)

    def test_iohbusno_exact_preflight_write_and_readback(self) -> None:
        route = function_text(PCI_SOURCE, "b06vo_program_ioh_bus_number_once")
        for fragment in (
            "B06VO_IOHBUSNO",
            "before != 0x0000 && before != B06VO_IOHBUSNO_BUS0_VALID",
            "before == 0x0000",
            "write16p(address, B06VO_IOHBUSNO_BUS0_VALID)",
            "after != B06VO_IOHBUSNO_BUS0_VALID",
            'b06vo_log_ioh_routing("PRE")',
            'b06vo_log_ioh_routing("POST")',
            "POST_B06VO_IOHBUSNO_FAIL",
            "POST_B06VO_IOHBUSNO_OK",
        ):
            self.assertIn(fragment, route)
        self.assertEqual(route.count("write16p("), 1)
        self.assertNotIn("pci_io_write", route)
        self.assertNotIn("pci_write_config", route)

        domain = function_text(PCI_SOURCE, "b06vn_domain_scan_bus")
        ordered = [
            "b06vn_raw_root_preflight();",
            "b06vo_program_ioh_bus_number_once();",
            "post_code(POST_B06VN_ROOTS_SAFE);",
            "b06vn_start_iou0_once();",
            "pci_host_bridge_scan_bus(dev);",
        ]
        positions = [domain.index(item) for item in ordered]
        self.assertEqual(positions, sorted(positions))

    def test_alias_and_documented_bus_ranges_are_read_only_telemetry(self) -> None:
        for fragment in (
            "0xe00a010aULL",
            "0xe0168000ULL",
            "0xe0268000ULL",
            "0xe0100000ULL",
            "#define B06VO_LCFGBUS_BASE\t0x11c",
            "#define B06VO_LCFGBUS_LIMIT\t0x11d",
            "#define B06VO_GCFGBUS_BASE\t0x134",
            "#define B06VO_GCFGBUS_LIMIT\t0x135",
        ):
            self.assertIn(fragment, PCI_SOURCE)
        telemetry = function_text(PCI_SOURCE, "b06vo_log_ioh_routing")
        self.assertEqual(telemetry.count("read8p("), 4)
        self.assertEqual(telemetry.count("b06vo_ecam_read_id("), 2)
        self.assertNotRegex(telemetry, r"\bwrite(?:8|16|32)p\s*\(")
        self.assertNotIn("pci_io_write", telemetry)

    def test_root03_paired_probe_precedes_unchanged_scanner(self) -> None:
        probe = function_text(PCI_SOURCE, "b06vo_scan_bus_with_endpoint_probe")
        for fragment in (
            "0, 1000, 9000, 90000",
            "PCI_DEV(bus->secondary, B06VO_PEG_ENDPOINT_DEV, 0)",
            "pci_io_read_config32(",
            "b06vo_ecam_read_id(bus->secondary",
            "CF8=%08x ECAM=%08x MATCH=%u",
            "pci_scan_bus(bus, min_devfn, max_devfn);",
        ):
            self.assertIn(fragment, probe)
        self.assertLess(probe.index("PEG_PROBE"), probe.index("pci_scan_bus("))
        self.assertNotIn("pci_io_write", probe)
        self.assertNotIn("write16p", probe)
        bridge = function_text(PCI_SOURCE, "b06vn_scan_bridge")
        self.assertIn(
            "do_pci_scan_bridge(dev, b06vo_scan_bus_with_endpoint_probe);",
            bridge,
        )

    def test_no_second_hardware_hypothesis_is_present(self) -> None:
        for forbidden in (
            "PCI_BRIDGE_CONTROL_RESET",
            "PCI_EXP_LNKCTL_RL",
            "pciexp_retrain_link",
            "CONFIG_SEABIOS_HARDWARE_IRQ=n",
        ):
            self.assertNotIn(forbidden, PCI_SOURCE + VO_CONFIG)


if __name__ == "__main__":
    unittest.main()
