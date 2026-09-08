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
PCI_SOURCE = (BOARD / "b06vn_pci.c").read_text()
VR_TREE = (BOARD / "devicetree_b06vr.cb").read_text()
ICH10_KCONFIG = (ICH10 / "Kconfig").read_text()
ICH10_MAKE = (ICH10 / "Makefile.mk").read_text()
ICH10_HEADER = (ICH10 / "i82801jx.h").read_text()
ICH10_HELPER = (ICH10 / "sata_ahci_map.c").read_text()
VQ_CONFIG = (ROOT / "configs/x58-pro-e-b06vq.config").read_text()
VR_CONFIG = (ROOT / "configs/x58-pro-e-b06vr.config").read_text()
VR_BUILD_PATH = ROOT / "scripts/build_x58_b06vr.sh"

VR_SYMBOL = "CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP"
VQ_SYMBOL = "CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT"
VR_ID = "X58PROE-B06VR-ICH10-AHCI-MAP-20260906"
VQ_ID = "X58PROE-B06VQ-ICH10-EHCI-INIT-20260906"


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


def config_contract(source: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for raw_line in source.splitlines():
        line = raw_line.strip()
        if line.startswith("CONFIG_") and "=" in line:
            result[line.split("=", 1)[0]] = line
        elif line.startswith("# CONFIG_") and line.endswith(" is not set"):
            result[line[2:].split(" ", 1)[0]] = line
    return result


class B06VRSourceContractTests(unittest.TestCase):
    def test_successor_is_default_off_and_one_mode_transition(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VR_ICH10_AHCI_MAP",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        for fragment in (
            "depends on X58_PRO_E_B06VQ_ICH10_EHCI_INIT",
            "select SOUTHBRIDGE_INTEL_I82801JX_SATA_AHCI_MAP",
            "default n",
            "MAP bits 7:5 to 011b",
            "mandatory BAR5 clear",
            "does not enable ports",
            "program SATA clocks",
            "access AHCI MMIO",
            "broad ICH10 driver",
        ):
            self.assertIn(fragment, option)

    def test_identity_and_tree_precede_b06vq(self) -> None:
        tree = between(KCONFIG, "config DEVICETREE", "config X58_PRO_E_B06M")
        payload = between(KCONFIG, "config PAYLOAD_CONFIGFILE", "config MAINBOARD_PART_NUMBER")
        part = between(KCONFIG, "config MAINBOARD_PART_NUMBER", "endif")
        for source in (tree, payload, part):
            # Match exact inherited stage symbols. A diagnostic derivative
            # such as B06VQ_USB_TRACE1 otherwise collides with a short
            # human-label search even though the actual precedence is sound.
            self.assertLess(
                source.index("X58_PRO_E_B06VR_ICH10_AHCI_MAP"),
                source.index("X58_PRO_E_B06VQ_ICH10_EHCI_INIT"),
            )
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON, MAINBOARD, PCI_SOURCE):
            vr_marker = VR_SYMBOL if source == MAINBOARD else VR_ID
            vq_marker = VQ_SYMBOL if source == MAINBOARD else VQ_ID
            self.assertIn(vr_marker, source)
            self.assertLess(source.index(vr_marker), source.index(vq_marker))

        pci_lines = re.findall(
            r"^\s*device pci ([0-9a-f]{2}\.[0-7]) mandatory ops (\w+)",
            VR_TREE,
            re.MULTILINE,
        )
        self.assertEqual(len(pci_lines), 13)
        self.assertIn(("1f.2", "b06vn_endpoint_ops"), pci_lines)
        self.assertNotIn(("1f.5", "b06vn_endpoint_ops"), pci_lines)

    def test_config_is_b06vq_plus_identity_and_one_option(self) -> None:
        vq = config_contract(VQ_CONFIG)
        vr = config_contract(VR_CONFIG)
        allowed = {VR_SYMBOL, "CONFIG_MAINBOARD_PART_NUMBER", "CONFIG_LOCALVERSION"}
        self.assertEqual(
            {key: value for key, value in vq.items() if key not in allowed},
            {key: value for key, value in vr.items() if key not in allowed},
        )
        self.assertEqual(vr[VR_SYMBOL], f"{VR_SYMBOL}=y")
        self.assertEqual(
            vr["CONFIG_MAINBOARD_PART_NUMBER"],
            'CONFIG_MAINBOARD_PART_NUMBER="X58 Pro-E B06VR ICH10 AHCI map"',
        )
        self.assertEqual(
            vr["CONFIG_LOCALVERSION"], 'CONFIG_LOCALVERSION="x58-pro-e-b06vr"'
        )

    def test_reusable_helper_is_narrow_and_atomic(self) -> None:
        option = between(
            ICH10_KCONFIG,
            "config SOUTHBRIDGE_INTEL_I82801JX_SATA_AHCI_MAP",
            "\nconfig SOUTHBRIDGE_INTEL_I82801JX\n",
        )
        for fragment in (
            "clears BAR5 after the component type",
            "ICH10 section 14.1.16",
            "does not enable ports",
            "program SATA",
            "AHCI MMIO registers",
        ):
            self.assertIn(fragment, option)
        self.assertIn(
            "ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_AHCI_MAP) += sata_ahci_map.c",
            ICH10_MAKE,
        )
        for fragment in (
            "#define I82801JX_SATA_MAP\t",
            "I82801JX_SATA_MAP_AHCI_D31F2_MASK",
            "I82801JX_SATA_MAP_AHCI_D31F2_VALUE",
            "#define I82801JX_SATA_ABAR",
            "void i82801jx_sata_select_ahci(pci_devfn_t sata);",
        ):
            self.assertIn(fragment, ICH10_HEADER)

        helper = function_text(ICH10_HELPER, "i82801jx_sata_select_ahci")
        self.assertEqual(helper.count("pci_io_read_config16("), 1)
        self.assertEqual(helper.count("pci_io_write_config16("), 1)
        self.assertEqual(helper.count("pci_io_write_config32("), 1)
        self.assertIn("~I82801JX_SATA_MAP_AHCI_D31F2_MASK", helper)
        self.assertIn("I82801JX_SATA_MAP_AHCI_D31F2_VALUE", helper)
        self.assertIn("I82801JX_SATA_ABAR, 0", helper)
        self.assertLess(
            helper.index("pci_io_write_config16("),
            helper.index("pci_io_write_config32("),
        )

    def test_exact_cold_or_retained_prestate_and_poststate(self) -> None:
        cold = function_text(PCI_SOURCE, "b06vr_sata_legacy_prestate")
        retained = function_text(PCI_SOURCE, "b06vr_sata_ahci_state")
        self.assertIn("return map == 0", cold)
        for fragment in (
            "B06VR_SATA1_IDE_ID",
            "B06VR_SATA2_IDE_ID",
            "B06VR_IDE_CLASS",
            "b06vr_sata_is_quiescent(B06VR_SATA1_DEV)",
            "b06vr_sata_is_quiescent(B06VR_SATA2_DEV)",
        ):
            self.assertIn(fragment, cold)
        for fragment in (
            "map == I82801JX_SATA_MAP_AHCI_D31F2_VALUE",
            "B06VR_SATA1_AHCI_ID",
            "B06VR_AHCI_CLASS_PI",
            "B06VR_SATA2_DEV, PCI_VENDOR_ID) == 0xffffffffu",
        ):
            self.assertIn(fragment, retained)

    def test_one_shot_exact_gate_precedes_normal_root_preflight(self) -> None:
        program = function_text(PCI_SOURCE, "b06vr_program_ahci_map_once")
        for fragment in (
            "b06vr_ahci_map_attempted",
            'b06vr_log_sata_state("PRE")',
            "!b06vr_sata_legacy_prestate(before)",
            "!b06vr_sata_ahci_state(before)",
            "POST_B06VR_AHCI_BEGIN",
            "i82801jx_sata_select_ahci(B06VR_SATA1_DEV);",
            'b06vr_log_sata_state("POST")',
            "after != target || !b06vr_sata_ahci_state(after)",
            "I82801JX_SATA_ABAR) != 0",
            "POST_B06VR_AHCI_FAIL",
            "POST_B06VR_AHCI_OK",
            "ports/clocks/AHCI MMIO untouched",
            "BAR5 cleared per ICH10 section 14.1.16",
        ):
            self.assertIn(fragment, program)
        self.assertEqual(program.count("i82801jx_sata_select_ahci("), 1)
        self.assertNotRegex(program, r"\b(?:pci_io_write|write[0-9]*p|out[blw]|wrmsr)\s*\(")

        scan = function_text(PCI_SOURCE, "b06vn_domain_scan_bus")
        ordered = (
            "b06vn_require_static_topology();",
            "b06vr_program_ahci_map_once();",
            "b06vn_raw_root_preflight();",
            "b06vo_program_ioh_bus_number_once();",
            "b06vq_program_ehci_once();",
            "b06vn_start_iou0_once();",
            "pci_host_bridge_scan_bus(dev);",
        )
        positions = [scan.index(fragment) for fragment in ordered]
        self.assertEqual(positions, sorted(positions))

    def test_post_map_allowlist_is_ahci_only(self) -> None:
        table = between(
            PCI_SOURCE,
            "static const struct b06vn_expected_pci b06vn_expected[]",
            "static const uint32_t b06vn_sad_rules",
        )
        conditional = between(
            table,
            "#if CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP",
            "#else",
        )
        for fragment in (
            "B06VR_SATA1_AHCI_ID",
            "PCI_CLASS_STORAGE_SATA",
            '"ICH10R SATA AHCI"',
        ):
            self.assertIn(fragment, conditional)
        self.assertNotIn("B06VR_SATA2_IDE_ID", conditional)

    def test_builder_contract(self) -> None:
        self.assertTrue(VR_BUILD_PATH.exists())
        builder = VR_BUILD_PATH.read_text()
        for fragment in (
            "configs/x58-pro-e-b06vr.config",
            "CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP=y",
            "CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_AHCI_MAP=y",
            'cd -- "${repo_root}"',
            "unset PYTHONOPTIMIZE",
            "CONFIG_USB_MSC=y",
            "CONFIG_PMM=y",
            "CONFIG_PCIBIOS=y",
            "CONFIG_PNPBIOS=y",
            "tests.test_x58_b06vr_source_contract",
            "msi-x58-pro-e-b06vr-coreboot-base-4MiB.rom",
            "msi-x58-pro-e-b06vr-deterministic-w25q128-16MiB.rom",
            'cmp -s -- "${first_rom}" "${second_rom}"',
            'int.from_bytes(data, "little") == 1',
        ):
            self.assertIn(fragment, builder)


if __name__ == "__main__":
    unittest.main()
