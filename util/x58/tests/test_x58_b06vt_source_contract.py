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
ICH10_HELPER = (ICH10 / "sata_clock_field.c").read_text()
VS_CONFIG = (ROOT / "configs/x58-pro-e-b06vs.config").read_text()
VT_CONFIG = (ROOT / "configs/x58-pro-e-b06vt.config").read_text()
VT_BUILD_PATH = ROOT / "scripts/build_x58_b06vt.sh"

VT_SYMBOL = "CONFIG_X58_PRO_E_B06VT_ICH10_SATA_CLOCK"
VS_SYMBOL = "CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS"
VT_ID = "X58PROE-B06VT-ICH10-SATA-CLOCK-20260906"
VS_ID = "X58PROE-B06VS-ICH10-AHCI-PORTS-20260906"


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


class B06VTSourceContractTests(unittest.TestCase):
    def test_successor_is_default_off_and_one_field_only(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VT_ICH10_SATA_CLOCK",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        for fragment in (
            "depends on X58_PRO_E_B06VS_ICH10_AHCI_PORTS",
            "select SOUTHBRIDGE_INTEL_I82801JX_SATA_CLOCK_FIELD",
            "default n",
            "offset 0x94",
            "0x00000000",
            "0x00000193",
            "port-clock-disable bits",
            "bits 8:0",
            "32-bit masked read-modify-write",
            "exact complete-dword readback",
            "does not set",
            "reserved bit 30",
            "does not gate a port clock",
            "access AHCI MMIO",
            "issue COMRESET/OOB",
            "touch a disk",
        ):
            self.assertIn(fragment, option)

    def test_identity_precedence_and_tree_reuse(self) -> None:
        tree = between(KCONFIG, "config DEVICETREE", "config X58_PRO_E_B06M")
        payload = between(
            KCONFIG, "config PAYLOAD_CONFIGFILE", "config MAINBOARD_PART_NUMBER"
        )
        part = between(KCONFIG, "config MAINBOARD_PART_NUMBER", "endif")
        for source in (tree, payload, part):
            self.assertLess(source.index("B06VT"), source.index("B06VS"))
        self.assertIn(
            'default "devicetree_b06vr.cb" if X58_PRO_E_B06VT_ICH10_SATA_CLOCK',
            tree,
        )
        self.assertFalse((BOARD / "devicetree_b06vt.cb").exists())

        for source in (BOOTBLOCK, ROMSTAGE, RAMMON, MAINBOARD, PCI_SOURCE):
            vt_marker = VT_SYMBOL if source == MAINBOARD else VT_ID
            vs_marker = VS_SYMBOL if source == MAINBOARD else VS_ID
            self.assertIn(vt_marker, source)
            self.assertLess(source.index(vt_marker), source.index(vs_marker))

    def test_config_is_b06vs_plus_identity_and_one_option(self) -> None:
        vs = config_contract(VS_CONFIG)
        vt = config_contract(VT_CONFIG)
        allowed = {VT_SYMBOL, "CONFIG_MAINBOARD_PART_NUMBER", "CONFIG_LOCALVERSION"}
        self.assertEqual(
            {key: value for key, value in vs.items() if key not in allowed},
            {key: value for key, value in vt.items() if key not in allowed},
        )
        self.assertEqual(vt[VT_SYMBOL], f"{VT_SYMBOL}=y")
        self.assertEqual(
            vt["CONFIG_MAINBOARD_PART_NUMBER"],
            'CONFIG_MAINBOARD_PART_NUMBER="X58 Pro-E B06VT ICH10 SATA clock"',
        )
        self.assertEqual(
            vt["CONFIG_LOCALVERSION"], 'CONFIG_LOCALVERSION="x58-pro-e-b06vt"'
        )

    def test_reusable_helper_changes_only_field1(self) -> None:
        option = between(
            ICH10_KCONFIG,
            "config SOUTHBRIDGE_INTEL_I82801JX_SATA_CLOCK_FIELD",
            "\nconfig SOUTHBRIDGE_INTEL_I82801JX\n",
        )
        for fragment in (
            "changes only Field 1, bits 8:0",
            "required value",
            "0x193",
            "32-bit masked read-modify-write",
            "preserves",
            "port-clock-disable",
            "does not enable clock request",
            "AHCI MMIO",
            "issue OOB",
        ):
            self.assertIn(fragment, option)
        self.assertIn(
            "ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_CLOCK_FIELD) += sata_clock_field.c",
            ICH10_MAKE,
        )
        broad = between(
            ICH10_KCONFIG,
            "config SOUTHBRIDGE_INTEL_I82801JX\n",
            "\nif SOUTHBRIDGE_INTEL_I82801JX",
        )
        self.assertNotIn("select SOUTHBRIDGE_INTEL_I82801JX_SATA_CLOCK_FIELD", broad)
        for fragment in (
            "#define I82801JX_SATA_SCLKCG\t",
            "I82801JX_SATA_SCLKCG_FIELD1_MASK",
            "I82801JX_SATA_SCLKCG_FIELD1_REQUIRED",
            "I82801JX_SATA_SCLKCG_RESERVED_23_9",
            "I82801JX_SATA_SCLKCG_PORT_DISABLE_MASK",
            "I82801JX_SATA_SCLKCG_RESERVED_31_30",
            "void i82801jx_sata_program_clock_field(pci_devfn_t sata);",
        ):
            self.assertIn(fragment, ICH10_HEADER)

        helper = function_text(
            ICH10_HELPER, "i82801jx_sata_program_clock_field"
        )
        self.assertEqual(helper.count("pci_io_read_config32("), 1)
        self.assertEqual(helper.count("pci_io_write_config32("), 1)
        self.assertNotRegex(helper, r"pci_io_(?:read|write)_config(?:8|16)\s*\(")
        self.assertIn("~I82801JX_SATA_SCLKCG_FIELD1_MASK", helper)
        self.assertIn("I82801JX_SATA_SCLKCG_FIELD1_REQUIRED", helper)
        self.assertNotIn("PORT_DISABLE_MASK", helper)
        self.assertNotIn("RESERVED_31_30", helper)

    def test_exact_prestate_and_complete_readback(self) -> None:
        program = function_text(PCI_SOURCE, "b06vt_program_clock_once")
        for fragment in (
            "b06vt_clock_attempted",
            'b06vt_log_clock_state("PRE", sata, abar)',
            "!b06vs_sata_state_is_exact(sata, abar)",
            "(uint8_t)pcs_before != I82801JX_SATA_PCS_ALL_PORTS_ENABLED",
            "I82801JX_SATA_PCS_RESERVED_14",
            "before != 0",
            "before != I82801JX_SATA_SCLKCG_FIELD1_REQUIRED",
            "I82801JX_SATA_SCLKCG_RESERVED_23_9",
            "I82801JX_SATA_SCLKCG_PORT_DISABLE_MASK",
            "I82801JX_SATA_SCLKCG_RESERVED_31_30",
            "POST_B06VT_CLOCK_BEGIN",
            "i82801jx_sata_program_clock_field(B06VR_SATA1_DEV);",
            'b06vt_log_clock_state("POST", sata, abar)',
            "after != I82801JX_SATA_SCLKCG_FIELD1_REQUIRED",
            "POST_B06VT_CLOCK_FAIL",
            "POST_B06VT_CLOCK_OK",
            "AHCI MMIO untouched",
        ):
            self.assertIn(fragment, program)
        self.assertEqual(program.count("i82801jx_sata_program_clock_field("), 1)
        self.assertNotRegex(
            program, r"\b(?:pci_io_write_config(?:8|16|32)|write(?:8|16|32)p)\s*\("
        )

    def test_clock_follows_ports_in_same_audited_hook(self) -> None:
        assigned = function_text(PCI_SOURCE, "b06vn_resources_assigned")
        ordered = (
            "b06vn_collect_and_audit_resources(leaves);",
            "b06vn_require_no_leaf_overlap(leaves, leaf_count);",
            "post_code(POST_B06VN_ALLOC_OK);",
            "b06vs_program_ports_once();",
            "b06vt_program_clock_once();",
        )
        positions = [assigned.index(fragment) for fragment in ordered]
        self.assertEqual(positions, sorted(positions))
        for name in ("b06vn_domain_scan_bus", "b06vn_resources_enabled"):
            self.assertNotIn("b06vt_program_clock_once();", function_text(
                PCI_SOURCE, name
            ))
        self.assertEqual(PCI_SOURCE.count("b06vt_program_clock_once();"), 1)
        self.assertIn(
            "BOOT_STATE_INIT_ENTRY(BS_DEV_RESOURCES, BS_ON_EXIT,\n"
            "\tb06vn_resources_assigned, NULL);",
            PCI_SOURCE,
        )

    def test_post_codes_are_stable_and_distinct(self) -> None:
        for name, value in (
            ("POST_B06VT_CLOCK_BEGIN", "0x50"),
            ("POST_B06VT_CLOCK_OK", "0x52"),
            ("POST_B06VT_CLOCK_FAIL", "0x53"),
        ):
            self.assertEqual(
                len(re.findall(rf"^#define {name}\s+{value}$", PCI_SOURCE, re.M)),
                1,
            )

    def test_builder_contract(self) -> None:
        self.assertTrue(VT_BUILD_PATH.exists())
        builder = VT_BUILD_PATH.read_text()
        for fragment in (
            "configs/x58-pro-e-b06vt.config",
            "CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS=y",
            "CONFIG_X58_PRO_E_B06VT_ICH10_SATA_CLOCK=y",
            "CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_CLOCK_FIELD=y",
            'CONFIG_DEVICETREE="devicetree_b06vr.cb"',
            'cd -- "${repo_root}"',
            "unset PYTHONOPTIMIZE",
            "CONFIG_USB_MSC=y",
            "CONFIG_PMM=y",
            "CONFIG_PCIBIOS=y",
            "CONFIG_PNPBIOS=y",
            "tests.test_x58_b06vs_source_contract",
            "tests.test_x58_b06vt_source_contract",
            "msi-x58-pro-e-b06vt-coreboot-base-4MiB.rom",
            "msi-x58-pro-e-b06vt-deterministic-w25q128-16MiB.rom",
            'cmp -s -- "${first_rom}" "${second_rom}"',
            'int.from_bytes(data, "little") == 1',
        ):
            self.assertIn(fragment, builder)


if __name__ == "__main__":
    unittest.main()
