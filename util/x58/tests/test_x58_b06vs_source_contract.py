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
ICH10_KCONFIG = (ICH10 / "Kconfig").read_text()
ICH10_MAKE = (ICH10 / "Makefile.mk").read_text()
ICH10_HEADER = (ICH10 / "i82801jx.h").read_text()
ICH10_HELPER = (ICH10 / "sata_port_enable.c").read_text()
VR_CONFIG = (ROOT / "configs/x58-pro-e-b06vr.config").read_text()
VS_CONFIG = (ROOT / "configs/x58-pro-e-b06vs.config").read_text()
VS_BUILD_PATH = ROOT / "scripts/build_x58_b06vs.sh"

VS_SYMBOL = "CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS"
VR_SYMBOL = "CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP"
VS_ID = "X58PROE-B06VS-ICH10-AHCI-PORTS-20260906"
VR_ID = "X58PROE-B06VR-ICH10-AHCI-MAP-20260906"


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


class B06VSSourceContractTests(unittest.TestCase):
    def test_successor_is_default_off_and_fail_closed(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VS_ICH10_AHCI_PORTS",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        for fragment in (
            "depends on X58_PRO_E_B06VR_ICH10_AHCI_MAP",
            "select SOUTHBRIDGE_INTEL_I82801JX_SATA_PORT_ENABLE",
            "default n",
            "After normal PCI enumeration, BAR sizing, allocation",
            "8086:3a22/010601",
            "D31:F5 absent",
            "MAP 0060",
            "2-KiB, 32-bit",
            "non-prefetchable",
            "PCS[5:0] to 0x3f",
            "8-bit low-byte masked read-modify-write",
            "bits 7:6 and reserved bit 14 must remain zero",
            "reserved bit 14 must remain zero",
            "OOB Retry Mode bit 15 is deliberately preserved",
            "does not change ORM",
            "presence fields",
            "presence fields, SATA",
            "clocks, PI or any AHCI MMIO",
            "broad ICH10",
            "driver.",
        ):
            self.assertIn(fragment, option)

    def test_identity_precedence_and_b06vr_tree_reuse(self) -> None:
        tree = between(KCONFIG, "config DEVICETREE", "config X58_PRO_E_B06M")
        payload = between(
            KCONFIG, "config PAYLOAD_CONFIGFILE", "config MAINBOARD_PART_NUMBER"
        )
        part = between(KCONFIG, "config MAINBOARD_PART_NUMBER", "endif")
        for source in (tree, payload, part):
            self.assertLess(source.index("B06VS"), source.index("B06VR"))
        self.assertIn(
            'default "devicetree_b06vr.cb" if X58_PRO_E_B06VS_ICH10_AHCI_PORTS',
            tree,
        )
        self.assertFalse((BOARD / "devicetree_b06vs.cb").exists())

        for source in (BOOTBLOCK, ROMSTAGE, RAMMON, MAINBOARD, PCI_SOURCE):
            vs_marker = VS_SYMBOL if source == MAINBOARD else VS_ID
            vr_marker = VR_SYMBOL if source == MAINBOARD else VR_ID
            self.assertIn(vs_marker, source)
            self.assertLess(source.index(vs_marker), source.index(vr_marker))

    def test_config_is_b06vr_plus_identity_and_one_option(self) -> None:
        vr = config_contract(VR_CONFIG)
        vs = config_contract(VS_CONFIG)
        allowed = {VS_SYMBOL, "CONFIG_MAINBOARD_PART_NUMBER", "CONFIG_LOCALVERSION"}
        self.assertEqual(
            {key: value for key, value in vr.items() if key not in allowed},
            {key: value for key, value in vs.items() if key not in allowed},
        )
        self.assertEqual(vs[VS_SYMBOL], f"{VS_SYMBOL}=y")
        self.assertEqual(
            vs["CONFIG_MAINBOARD_PART_NUMBER"],
            'CONFIG_MAINBOARD_PART_NUMBER="X58 Pro-E B06VS ICH10 AHCI ports"',
        )
        self.assertEqual(
            vs["CONFIG_LOCALVERSION"], 'CONFIG_LOCALVERSION="x58-pro-e-b06vs"'
        )

    def test_reusable_helper_is_low_byte_only(self) -> None:
        option = between(
            ICH10_KCONFIG,
            "config SOUTHBRIDGE_INTEL_I82801JX_SATA_PORT_ENABLE",
            "\nconfig SOUTHBRIDGE_INTEL_I82801JX\n",
        )
        for fragment in (
            "changes only the six port-",
            "enable fields in the low byte",
            "8-bit masked read-modify-write",
            "preserving the reserved low-byte fields",
            "never writing the",
            "read-only presence fields",
            "ICH10 section 14.1.31",
            "does not program SATA clocks",
            "AHCI MMIO registers",
        ):
            self.assertIn(fragment, option)
        self.assertIn(
            "ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_PORT_ENABLE) += sata_port_enable.c",
            ICH10_MAKE,
        )
        for fragment in (
            "#define I82801JX_SATA_PCS\t",
            "I82801JX_SATA_PCS_PORT_ENABLE_MASK",
            "I82801JX_SATA_PCS_ALL_PORTS_ENABLED",
            "I82801JX_SATA_PCS_LOW_NON_PORT_MASK",
            "I82801JX_SATA_PCS_PRESENCE_MASK",
            "I82801JX_SATA_PCS_RESERVED_14",
            "I82801JX_SATA_PCS_OOB_RETRY_MODE",
            "void i82801jx_sata_enable_all_ports(pci_devfn_t sata);",
        ):
            self.assertIn(fragment, ICH10_HEADER)

        helper = function_text(ICH10_HELPER, "i82801jx_sata_enable_all_ports")
        self.assertEqual(helper.count("pci_io_read_config8("), 1)
        self.assertEqual(helper.count("pci_io_write_config8("), 1)
        self.assertNotRegex(helper, r"pci_io_(?:read|write)_config(?:16|32)\s*\(")
        self.assertIn("~I82801JX_SATA_PCS_PORT_ENABLE_MASK", helper)
        self.assertIn("I82801JX_SATA_PCS_ALL_PORTS_ENABLED", helper)
        self.assertNotIn("I82801JX_SATA_PCS_PRESENCE_MASK", helper)
        self.assertNotIn("I82801JX_SATA_PCS_HIGH_POLICY_MASK", helper)

    def test_exact_identity_decode_and_abar_resource_gate(self) -> None:
        state = function_text(PCI_SOURCE, "b06vs_sata_state_is_exact")
        for fragment in (
            "B06VR_SATA1_AHCI_ID",
            "B06VR_AHCI_CLASS_PI",
            "B06VR_SATA2_DEV, PCI_VENDOR_ID) ==",
            "0xffffffffu",
            "I82801JX_SATA_MAP_AHCI_D31F2_VALUE",
            "PCI_COMMAND_IO | PCI_COMMAND_MEMORY",
            "PCI_COMMAND_MASTER",
            "b06vs_abar_resource_is_exact(abar, raw_bar)",
        ):
            self.assertIn(fragment, state)

        abar = function_text(PCI_SOURCE, "b06vs_abar_resource_is_exact")
        for fragment in (
            "abar->size != B06VS_ABAR_SIZE",
            "abar->gran != B06VS_ABAR_GRANULARITY",
            "abar->align < B06VS_ABAR_GRANULARITY",
            "abar->base & (B06VS_ABAR_SIZE - 1)",
            "IORESOURCE_MEM | IORESOURCE_ASSIGNED",
            "IORESOURCE_STORED",
            "abar->flags != required",
            "abar->base >= B06VN_PCI_MMIO_BASE",
            "top <= B06VN_PCI_MMIO_TOP",
            "resource_end(abar) == top - 1",
            "PCI_BASE_ADDRESS_MEM_ATTR_MASK",
            "PCI_BASE_ADDRESS_SPACE_MEMORY",
            "abar->base",
        ):
            self.assertIn(fragment, abar)

    def test_exact_pcs_prestate_and_selected_readback(self) -> None:
        program = function_text(PCI_SOURCE, "b06vs_program_ports_once")
        for fragment in (
            "b06vs_port_enable_attempted",
            'b06vs_log_sata_state("PRE", sata, abar)',
            "!b06vs_sata_state_is_exact(sata, abar)",
            "before_low != 0",
            "before_low != I82801JX_SATA_PCS_ALL_PORTS_ENABLED",
            "I82801JX_SATA_PCS_LOW_NON_PORT_MASK",
            "POST_B06VS_PORTS_BEGIN",
            "i82801jx_sata_enable_all_ports(B06VR_SATA1_DEV);",
            'b06vs_log_sata_state("POST", sata, abar)',
            "(uint8_t)after != I82801JX_SATA_PCS_ALL_PORTS_ENABLED",
            "I82801JX_SATA_PCS_RESERVED_14",
            "I82801JX_SATA_PCS_OOB_RETRY_MODE",
            "POST_B06VS_PORTS_FAIL",
            "POST_B06VS_PORTS_OK",
            "presence ignored",
            "clocks/PI/AHCI MMIO untouched",
            "b06vs_log_presence_samples();",
        ):
            self.assertIn(fragment, program)
        self.assertEqual(program.count("i82801jx_sata_enable_all_ports("), 1)
        self.assertNotRegex(
            program, r"\b(?:pci_io_write_config(?:8|16|32)|write(?:8|16|32)p)\s*\("
        )

    def test_presence_samples_are_bounded_read_only_telemetry(self) -> None:
        sample = function_text(PCI_SOURCE, "b06vs_log_presence_samples")
        for fragment in (
            "0, 1000, 9000, 90000, 400000",
            "B06VS_PCS_SAMPLE_COUNT",
            "pci_io_read_config16",
            "I82801JX_SATA_PCS_PRESENCE_MASK",
            '"PRESENCE=%02x',
        ):
            self.assertIn(fragment, sample)
        self.assertNotRegex(sample, r"\b(?:pci_io_write|write(?:8|16|32)p)\s*\(")
        self.assertNotIn("die_with_post_code", sample)

    def test_hook_runs_only_after_complete_allocation_audit(self) -> None:
        assigned = function_text(PCI_SOURCE, "b06vn_resources_assigned")
        ordered = (
            "b06vn_collect_and_audit_resources(leaves);",
            "b06vn_require_no_leaf_overlap(leaves, leaf_count);",
            "post_code(POST_B06VN_ALLOC_OK);",
            "b06vs_program_ports_once();",
        )
        positions = [assigned.index(fragment) for fragment in ordered]
        self.assertEqual(positions, sorted(positions))
        self.assertNotIn("b06vs_program_ports_once();", function_text(
            PCI_SOURCE, "b06vn_domain_scan_bus"
        ))
        self.assertNotIn("b06vs_program_ports_once();", function_text(
            PCI_SOURCE, "b06vn_resources_enabled"
        ))

    def test_builder_contract(self) -> None:
        self.assertTrue(VS_BUILD_PATH.exists())
        builder = VS_BUILD_PATH.read_text()
        for fragment in (
            "configs/x58-pro-e-b06vs.config",
            "CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS=y",
            "CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_PORT_ENABLE=y",
            'CONFIG_DEVICETREE="devicetree_b06vr.cb"',
            'cd -- "${repo_root}"',
            "unset PYTHONOPTIMIZE",
            "CONFIG_USB_MSC=y",
            "CONFIG_PMM=y",
            "CONFIG_PCIBIOS=y",
            "CONFIG_PNPBIOS=y",
            "tests.test_x58_b06vq_source_contract",
            "tests.test_x58_b06vr_source_contract",
            "tests.test_x58_b06vs_source_contract",
            "msi-x58-pro-e-b06vs-coreboot-base-4MiB.rom",
            "msi-x58-pro-e-b06vs-deterministic-w25q128-16MiB.rom",
            'cmp -s -- "${first_rom}" "${second_rom}"',
            'int.from_bytes(data, "little") == 1',
        ):
            self.assertIn(fragment, builder)


if __name__ == "__main__":
    unittest.main()
