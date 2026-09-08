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
ICH_KCONFIG = (ICH10 / "Kconfig").read_text()
ICH_MAKEFILE = (ICH10 / "Makefile.mk").read_text()
ICH_HEADER = (ICH10 / "i82801jx.h").read_text()
ICH_DRIVER = (ICH10 / "i82801jx.c").read_text()
HELPER = (ICH10 / "required_fields.c").read_text()
BASE_CONFIG = (ROOT / "configs/x58-pro-e-b06vq.config").read_text()
IMAGE_CONFIG = (ROOT / "configs/x58-pro-e-b06vq-ichbase1.config").read_text()
BUILDER = (ROOT / "scripts/build_x58_b06vq_ichbase1.sh").read_text()

SYMBOL = "CONFIG_X58_PRO_E_B06VQ_ICHBASE1"
ID = "X58PROE-B06VQ-ICHBASE1-20260906"
BASE_ID = "X58PROE-B06VQ-ICH10-EHCI-INIT-20260906"


def between(source: str, first: str, last: str) -> str:
    begin = source.index(first)
    return source[begin : source.index(last, begin)]


def function_text(source: str, name: str) -> str:
    definition = re.search(
        rf"\b{re.escape(name)}\s*\([^;{{}}]*\)\s*\{{", source, re.DOTALL
    )
    if definition is None:
        raise ValueError(f"no definition for function {name}")
    begin = source.rfind("\n", 0, definition.start()) + 1
    depth = 0
    for offset in range(definition.end() - 1, len(source)):
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


class B06VQICHBase1SourceContractTests(unittest.TestCase):
    def test_variant_is_default_off_and_isolated(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VQ_ICHBASE1",
            "config X58_PRO_E_B06VR_ICH10_AHCI_MAP",
        )
        for fragment in (
            "depends on X58_PRO_E_B06VQ_ICH10_EHCI_INIT",
            "depends on !X58_PRO_E_B06VQ_USB_TRACE1",
            "depends on !X58_PRO_E_B06VQ_PLATRO1",
            "depends on !X58_PRO_E_B06VR_ICH10_AHCI_MAP",
            "select SOUTHBRIDGE_INTEL_I82801JX_REQUIRED_FIELDS",
            "default n",
            "Mixed",
            "pre-state is terminal before any register write",
            "full historical",
            "ICH10 device model remains disabled",
        ):
            self.assertIn(fragment, option)
        self.assertNotIn("select SOUTHBRIDGE_INTEL_I82801JX\n", option)

    def test_identity_precedes_all_inherited_identifiers(self) -> None:
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON, PCI_SOURCE):
            self.assertIn(ID, source)
            self.assertIn(BASE_ID, source)
            self.assertLess(source.index(ID), source.index(BASE_ID))
        self.assertLess(MAINBOARD.index(SYMBOL), MAINBOARD.index("PLATRO1"))
        for section in (
            between(KCONFIG, "config DEVICETREE", "config X58_PRO_E_B06M"),
            between(KCONFIG, "config PAYLOAD_CONFIGFILE", "config MAINBOARD_PART_NUMBER"),
            between(KCONFIG, "config MAINBOARD_PART_NUMBER", "endif"),
        ):
            self.assertLess(section.index("B06VQ_ICHBASE1"), section.index("B06VQ_PLATRO1"))

    def test_outer_config_diff_is_only_variant_identity(self) -> None:
        base = config_contract(BASE_CONFIG)
        image = config_contract(IMAGE_CONFIG)
        allowed = {
            SYMBOL,
            "CONFIG_X58_PRO_E_B06VQ_PLATRO1",
            "CONFIG_X58_PRO_E_B06VQ_USB_TRACE1",
            "CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP",
            "CONFIG_MAINBOARD_PART_NUMBER",
            "CONFIG_LOCALVERSION",
        }
        self.assertEqual(
            {key: value for key, value in base.items() if key not in allowed},
            {key: value for key, value in image.items() if key not in allowed},
        )
        self.assertEqual(image[SYMBOL], f"{SYMBOL}=y")
        for key in (
            "CONFIG_X58_PRO_E_B06VQ_PLATRO1",
            "CONFIG_X58_PRO_E_B06VQ_USB_TRACE1",
            "CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP",
        ):
            self.assertEqual(image[key], f"# {key} is not set")

    def test_reusable_helper_is_exactly_six_ordered_updates(self) -> None:
        calls = re.findall(r"\b(RCBA32_(?:AND_OR|OR))\((RCBA_[A-Z0-9]+)", HELPER)
        self.assertEqual(
            calls,
            [
                ("RCBA32_AND_OR", "RCBA_CIR8"),
                ("RCBA32_OR", "RCBA_FD"),
                ("RCBA32_AND_OR", "RCBA_CIR9"),
                ("RCBA32_AND_OR", "RCBA_CIR7"),
                ("RCBA32_AND_OR", "RCBA_CIR13"),
                ("RCBA32_OR", "RCBA_CIR10"),
            ],
        )
        for forbidden in ("GCS", "RCBA_CIR5", "RCBA_FDSW", "RCBA_RPFN", "RCBA_MAP"):
            self.assertNotIn(f"({forbidden},", HELPER)
        self.assertEqual(HELPER.count("RCBA32_"), 6)
        body = function_text(HELPER, "i82801jx_program_required_fields")
        calls_in_body = set(re.findall(r"\b([A-Za-z_]\w*)\s*\(", body))
        self.assertEqual(
            calls_in_body,
            {"i82801jx_program_required_fields", "RCBA32_AND_OR", "RCBA32_OR"},
        )
        self.assertNotRegex(
            body,
            r"\b(?:RCBA(?:8|16|32|64)|write(?:8|16|32|64)?p|out[blw]|"
            r"wrmsr|pci_(?:io_)?write_config(?:8|16|32)|reset|halt)\s*\(",
        )

    def test_masks_and_complete_tuples_are_literal(self) -> None:
        expected_offsets = {
            "RCBA_CIR13": "0x0f20",
            "RCBA_CIR7": "0x2034",
            "RCBA_FD": "0x3418",
            "RCBA_CIR8": "0x3430",
            "RCBA_CIR9": "0x350c",
            "RCBA_CIR10": "0x352c",
        }
        for name, value in expected_offsets.items():
            self.assertRegex(ICH_HEADER, rf"#define {name}\s+{value}")
        expected_header = {
            "I82801JX_CIR8_FIELD_1_0_MASK": "0x00000003u",
            "I82801JX_CIR8_FIELD_1_0_REQUIRED": "0x00000002u",
            "I82801JX_FD_REQUIRED_BIT_0": "0x00000001u",
            "I82801JX_CIR9_FIELD_27_26_MASK": "0x0c000000u",
            "I82801JX_CIR9_FIELD_27_26_REQUIRED": "0x08000000u",
            "I82801JX_CIR7_FIELD_19_16_MASK": "0x000f0000u",
            "I82801JX_CIR7_FIELD_19_16_REQUIRED": "0x00050000u",
            "I82801JX_CIR13_FIELD_19_16_MASK": "0x000f0000u",
            "I82801JX_CIR13_FIELD_19_16_REQUIRED": "0x00050000u",
            "I82801JX_CIR10_REQUIRED_BITS_17_16": "0x00030000u",
        }
        for name, value in expected_header.items():
            self.assertRegex(ICH_HEADER, rf"#define {name}\s+{value}")
        expected_board = {
            "B06VQI_CIR8_PRE": "0x00000000u",
            "B06VQI_FD_PRE": "0x00000000u",
            "B06VQI_CIR9_PRE": "0x00000020u",
            "B06VQI_CIR7_PRE": "0xb2b477ccu",
            "B06VQI_CIR13_PRE": "0xb2b477ccu",
            "B06VQI_CIR10_PRE": "0x0008c008u",
            "B06VQI_CIR8_TARGET": "0x00000002u",
            "B06VQI_FD_TARGET": "0x00000001u",
            "B06VQI_CIR9_TARGET": "0x08000020u",
            "B06VQI_CIR7_TARGET": "0xb2b577ccu",
            "B06VQI_CIR13_TARGET": "0xb2b577ccu",
            "B06VQI_CIR10_TARGET": "0x000bc008u",
        }
        for name, value in expected_board.items():
            self.assertRegex(PCI_SOURCE, rf"#define {name}\s+{value}")

    def test_snapshot_rcba_mmio_is_strictly_gated(self) -> None:
        snapshot = function_text(PCI_SOURCE, "b06vqi_read_required_snapshot")
        identity_gate = snapshot.index("if (snapshot.lpc_id != B06VQI_LPC_ID")
        early_return = snapshot.index("return snapshot;", identity_gate)
        first_mmio = snapshot.index("read8p(CONFIG_FIXED_RCBA_MMIO_BASE")
        self.assertLess(identity_gate, early_return)
        self.assertLess(early_return, first_mmio)
        self.assertIn("snapshot.rcba != B06VQI_RCBA_ENABLED", snapshot)
        self.assertIn("snapshot.rcba_mmio_read = true", snapshot)

    def test_board_gate_rejects_mixed_state_before_helper(self) -> None:
        program = function_text(PCI_SOURCE, "b06vqi_program_baseline_once")
        for fragment in (
            "b06vqi_baseline_attempted = true;",
            "b06vqi_snapshot_has_fixed_gates(&before)",
            "b06vqi_snapshot_is_pre(&before)",
            "b06vqi_snapshot_is_target(&before)",
            "rejected identity/RCBA/FDSW or mixed six-field prestate",
            "b06vqi_snapshot_has_fixed_gates(&after)",
            "six-field complete target readback failed",
            "GCS/CIR5/FDSW/hide/lock/RPFN/MAP/PMIR/IRQ untouched",
        ):
            self.assertIn(fragment, program)
        self.assertLess(
            program.index("b06vqi_baseline_attempted = true;"),
            program.index("before = b06vqi_read_required_snapshot();"),
        )
        self.assertLess(
            program.index("mixed six-field prestate"),
            program.index("i82801jx_program_required_fields();"),
        )
        self.assertEqual(program.count("i82801jx_program_required_fields();"), 1)
        self.assertEqual(program.count("post_code(POST_B06VQI_BASE_BEGIN)"), 1)
        self.assertEqual(program.count("post_code(POST_B06VQI_BASE_OK)"), 1)

    def test_hook_is_after_raw_preflight_and_before_all_consumers(self) -> None:
        scan = function_text(PCI_SOURCE, "b06vn_domain_scan_bus")
        raw_preflight = scan.index("b06vn_raw_root_preflight();")
        call = scan.index("b06vqi_program_baseline_once();", raw_preflight)
        self.assertLess(raw_preflight, call)
        for later in (
            "b06vo_program_ioh_bus_number_once();",
            "b06vq_program_ehci_once();",
            "b06vn_start_iou0_once();",
            "pci_host_bridge_scan_bus(dev);",
        ):
            self.assertLess(call, scan.index(later))
        legacy = between(
            scan,
            "#if CONFIG_X58_PRO_E_B06VQ_ICHBASE1 &&",
            "#endif",
        )
        self.assertEqual(legacy.count("b06vqi_program_baseline_once();"), 1)

    def test_full_driver_uses_helper_after_separate_gcs(self) -> None:
        settings = function_text(ICH_DRIVER, "i82801jx_early_settings")
        self.assertLess(settings.index("RCBA32(GCS)"), settings.index("i82801jx_program_required_fields();"))
        for register in ("RCBA_CIR8", "RCBA_FD", "RCBA_CIR9", "RCBA_CIR7", "RCBA_CIR13", "RCBA_CIR10"):
            self.assertNotIn(register, settings)
        self.assertIn("select SOUTHBRIDGE_INTEL_I82801JX_REQUIRED_FIELDS", ICH_KCONFIG)
        self.assertIn("ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_REQUIRED_FIELDS) += required_fields.c", ICH_MAKEFILE)

    def test_no_adjacent_platform_experiment_is_selected(self) -> None:
        for line in (
            "# CONFIG_X58_PRO_E_B06VQ_PLATRO1 is not set",
            "# CONFIG_X58_PRO_E_B06VQ_USB_TRACE1 is not set",
            "# CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP is not set",
            "CONFIG_NO_SMM=y",
        ):
            self.assertIn(line, IMAGE_CONFIG)
        self.assertNotIn("CONFIG_SOUTHBRIDGE_INTEL_I82801JX=y", IMAGE_CONFIG)

    def test_builder_pins_isolation_payload_and_reproducibility(self) -> None:
        for fragment in (
            "SOURCE_DATE_EPOCH",
            "clean builds are not byte-identical",
            "verify_ipxe_source_and_rom",
            "verify_seabios_source",
            "CBFS iPXE ROM differs from the pinned source artifact",
            "'CONFIG_X58_PRO_E_B06VQ_ICHBASE1=y'",
            "'CONFIG_SOUTHBRIDGE_INTEL_I82801JX_REQUIRED_FIELDS=y'",
            "'CONFIG_SOUTHBRIDGE_INTEL_I82801JX=n'",
            "'CONFIG_X58_PRO_E_B06VQ_PLATRO1=n'",
            "'CONFIG_X58_PRO_E_B06VQ_USB_TRACE1=n'",
            "'CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP=n'",
            "'CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS=n'",
            "'CONFIG_X58_PRO_E_B06VT_ICH10_SATA_CLOCK=n'",
            "'CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_AHCI_MAP=n'",
            "'CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_PORT_ENABLE=n'",
            "'CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_CLOCK_FIELD=n'",
            "'CONFIG_HAVE_ACPI_TABLES=n'",
            "'CONFIG_SMP=n'",
        ):
            self.assertIn(fragment, BUILDER)


if __name__ == "__main__":
    unittest.main()
