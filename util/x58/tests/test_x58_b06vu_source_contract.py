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
DEVICETREE = (BOARD / "devicetree_b06vu.cb").read_text()
ICH_KCONFIG = (ICH10 / "Kconfig").read_text()
ICH_MAKEFILE = (ICH10 / "Makefile.mk").read_text()
ICH_HEADER = (ICH10 / "i82801jx.h").read_text()
MAP_HELPER = (ICH10 / "sata_ahci_map.c").read_text()
FD_HELPER = (ICH10 / "sata2_disable.c").read_text()
BASE_CONFIG = (ROOT / "configs/x58-pro-e-b06vq-ichbase1.config").read_text()
IMAGE_CONFIG = (ROOT / "configs/x58-pro-e-b06vu.config").read_text()
BUILDER_PATH = ROOT / "scripts/build_x58_b06vu.sh"

SYMBOL = "CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE"
ID = "X58PROE-B06VU-ICH10-AHCI-ROUTE-20260906"
BASE_ID = "X58PROE-B06VQ-ICHBASE1-20260906"


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


class B06VUCorrectedSataRouteSourceContractTests(unittest.TestCase):
    def test_variant_is_new_default_off_fail_closed_composition(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VU_ICH10_AHCI_ROUTE",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        for fragment in (
            "depends on X58_PRO_E_B06VQ_ICHBASE1",
            "select SOUTHBRIDGE_INTEL_I82801JX_SATA_AHCI_MAP",
            "select SOUTHBRIDGE_INTEL_I82801JX_SATA2_DISABLE",
            "default n",
            "exact measured quiescent",
            "intermediate state in which D31:F5 is still visible",
            "RCBA FD_SAD2 bit 25",
            "complete FD readback is",
            "FDSW remains unlocked",
            "D31:F5 is absent",
            "does not lock FDSW",
            "program SATA clocks, access AHCI",
            "MMIO, issue COMRESET/OOB",
            "fails closed",
        ):
            self.assertIn(fragment, option)
        self.assertNotIn("select SOUTHBRIDGE_INTEL_I82801JX\n", option)

    def test_identity_and_board_selection_precede_inherited_variants(self) -> None:
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON, PCI_SOURCE):
            self.assertIn(ID, source)
            self.assertIn(BASE_ID, source)
            self.assertLess(source.index(ID), source.index(BASE_ID))
        self.assertLess(MAINBOARD.index(SYMBOL), MAINBOARD.index("B06VQ_ICHBASE1"))
        for section in (
            between(KCONFIG, "config DEVICETREE", "config X58_PRO_E_B06M"),
            between(KCONFIG, "config PAYLOAD_CONFIGFILE", "config MAINBOARD_PART_NUMBER"),
            between(KCONFIG, "config MAINBOARD_PART_NUMBER", "endif"),
        ):
            self.assertLess(
                section.index("B06VU_ICH10_AHCI_ROUTE"),
                section.index("B06VQ_ICHBASE1"),
            )
        self.assertIn('default "devicetree_b06vu.cb"', KCONFIG)

    def test_config_only_adds_the_named_successor_and_explicit_exclusions(self) -> None:
        base = config_contract(BASE_CONFIG)
        image = config_contract(IMAGE_CONFIG)
        allowed = {
            SYMBOL,
            "CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS",
            "CONFIG_X58_PRO_E_B06VT_ICH10_SATA_CLOCK",
            "CONFIG_MAINBOARD_PART_NUMBER",
            "CONFIG_LOCALVERSION",
        }
        self.assertEqual(
            {key: value for key, value in base.items() if key not in allowed},
            {key: value for key, value in image.items() if key not in allowed},
        )
        self.assertEqual(image[SYMBOL], f"{SYMBOL}=y")
        for key in (
            "CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP",
            "CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS",
            "CONFIG_X58_PRO_E_B06VT_ICH10_SATA_CLOCK",
            "CONFIG_X58_PRO_E_B06VQ_PLATRO1",
            "CONFIG_X58_PRO_E_B06VQ_USB_TRACE1",
        ):
            self.assertEqual(image[key], f"# {key} is not set")

    def test_devicetree_exposes_only_the_post_route_sata_function(self) -> None:
        self.assertEqual(DEVICETREE.count("device pci"), 13)
        self.assertIn("device pci 1f.2 mandatory ops b06vn_endpoint_ops", DEVICETREE)
        self.assertNotRegex(DEVICETREE, r"device pci 1f\.5\b")
        self.assertIn("MAP first, then separately hides", DEVICETREE)

    def test_map_and_fd_helpers_are_separate_and_minimal(self) -> None:
        map_body = function_text(MAP_HELPER, "i82801jx_sata_select_ahci")
        self.assertEqual(map_body.count("pci_io_write_config16("), 1)
        self.assertEqual(map_body.count("pci_io_write_config32("), 1)
        self.assertIn("I82801JX_SATA_MAP_AHCI_D31F2_VALUE", map_body)
        self.assertIn("I82801JX_SATA_ABAR, 0", map_body)
        for forbidden in ("RCBA_FD", "RCBA_FDSW", "FD_SAD2", "SATA_PCS", "SATA_SCLKCG"):
            self.assertNotIn(forbidden, MAP_HELPER)

        fd_body = function_text(FD_HELPER, "i82801jx_disable_sata2")
        self.assertEqual(fd_body.count("RCBA32_OR("), 1)
        self.assertIn("RCBA32_OR(RCBA_FD, FD_SAD2);", fd_body)
        for forbidden in ("RCBA_FDSW", "SATA_MAP", "pci_io_write", "RCBA32_AND", "RCBA32("):
            self.assertNotIn(forbidden, fd_body)

        self.assertIn("void i82801jx_disable_sata2(void);", ICH_HEADER)
        self.assertIn(
            "ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA2_DISABLE) += sata2_disable.c",
            ICH_MAKEFILE,
        )
        helper_option = between(
            ICH_KCONFIG,
            "config SOUTHBRIDGE_INTEL_I82801JX_SATA2_DISABLE",
            "config SOUTHBRIDGE_INTEL_I82801JX_SATA_PORT_ENABLE",
        )
        for fragment in ("def_bool n", "sets only RCBA Function Disable", "neither", "FDSW", "locks"):
            self.assertIn(fragment, helper_option)

    def test_exact_measured_reset_and_target_tuples_are_literal(self) -> None:
        expected = {
            "B06VU_SATA1_IDE_CLASSREV": "0x01018a00u",
            "B06VU_SATA2_IDE_CLASSREV": "0x01018500u",
            "B06VU_SATA1_AHCI_CLASSREV": "0x01060100u",
            "B06VU_RESET_COMMAND": "0x0000u",
            "B06VU_RESET_HEADER": "0x00u",
            "B06VU_RESET_BAR5": "0x00000001u",
            "B06VU_RESET_MAP": "0x0000u",
            "B06VU_RESET_PCS": "0x0000u",
            "B06VU_RESET_SCLKCG": "0x00000000u",
            "B06VU_FD_BASELINE": "0x00000001u",
            "B06VU_FD_SATA2_DISABLED": "0x02000001u",
        }
        for name, value in expected.items():
            self.assertRegex(PCI_SOURCE, rf"#define {name}\s+{value}")
        self.assertIn("FD_SAD2 == 0x02000000", PCI_SOURCE)
        self.assertIn("B06VQI_FD_TARGET | FD_SAD2", PCI_SOURCE)

    def test_snapshot_is_complete_and_rcba_mmio_is_identity_gated(self) -> None:
        snapshot = function_text(PCI_SOURCE, "b06vu_read_sata_snapshot")
        gate = snapshot.index("if (snapshot.lpc_id != B06VQI_LPC_ID")
        early_return = snapshot.index("return snapshot;", gate)
        first_mmio = snapshot.index("read8p(CONFIG_FIXED_RCBA_MMIO_BASE")
        self.assertLess(gate, early_return)
        self.assertLess(early_return, first_mmio)
        for field in (
            "fdsw", "fd", "f2_id", "f2_classrev", "f2_command", "f2_header",
            "f2_bar5", "f2_map", "f2_pcs", "f2_sclkcg", "f5_id",
            "f5_classrev", "f5_command", "f5_header", "f5_bar5", "f5_map",
        ):
            self.assertIn(f"snapshot.{field} =", snapshot)
        log = function_text(PCI_SOURCE, "b06vu_log_sata_snapshot")
        for label in (
            "LPC_ID", "RCBA", "FDSW", "FD", "F2_ID", "F2_CLASSREV",
            "F2_CMD", "F2_HDR", "F2_BAR5", "F2_MAP", "F2_PCS",
            "F2_SCLKCG", "F5_ID", "F5_CLASSREV", "F5_CMD", "F5_HDR",
            "F5_BAR5", "F5_MAP",
        ):
            self.assertIn(label, log)

    def test_reset_intermediate_and_final_gates_are_exact(self) -> None:
        reset = function_text(PCI_SOURCE, "b06vu_raw_reset_topology_is_exact")
        for fragment in (
            "b06vu_fixed_gates_are_exact(snapshot)",
            "snapshot->fd == B06VU_FD_BASELINE",
            "b06vu_f2_reset_state_is_exact(snapshot)",
            "b06vu_f5_reset_state_is_exact(snapshot)",
        ):
            self.assertIn(fragment, reset)

        mapped = function_text(PCI_SOURCE, "b06vu_mapped_before_hide_is_exact")
        for fragment in (
            "snapshot->fd == B06VU_FD_BASELINE",
            "b06vu_f2_ahci_state_is_exact(snapshot)",
            "b06vu_f5_reset_state_is_exact(snapshot)",
        ):
            self.assertIn(fragment, mapped)

        final = function_text(PCI_SOURCE, "b06vu_final_route_is_exact")
        for fragment in (
            "snapshot->fd == B06VU_FD_SATA2_DISABLED",
            "b06vu_f2_ahci_state_is_exact(snapshot)",
            "snapshot->f5_id == 0xffffffffu",
            "snapshot->f5_classrev == 0xffffffffu",
            "snapshot->f5_command == 0xffffu",
            "snapshot->f5_header == 0xffu",
            "snapshot->f5_bar5 == 0xffffffffu",
            "snapshot->f5_map == 0xffffu",
        ):
            self.assertIn(fragment, final)

    def test_program_is_one_attempt_and_preserves_proven_causal_order(self) -> None:
        program = function_text(PCI_SOURCE, "b06vu_program_ahci_route_once")
        for fragment in (
            "b06vu_ahci_route_attempted = true;",
            'b06vu_log_sata_snapshot("RESET_PRE"',
            "b06vu_raw_reset_topology_is_exact(&snapshot)",
            "exact dual-IDE reset topology PASS after ICHBASE1",
            "MAP_STAGE begin",
            'b06vu_log_sata_snapshot("MAP_POST"',
            "b06vu_mapped_before_hide_is_exact(&snapshot)",
            "F5 remains visible as measured",
            "FD_SAD2_STAGE begin",
            'b06vu_log_sata_snapshot("FD_SAD2_POST"',
            "b06vu_final_route_is_exact(&snapshot)",
            "FD=02000001 FDSW=00 F5=absent",
            "no FDSW lock, PCS/SCLKCG/AHCI MMIO write",
        ):
            self.assertIn(fragment, program)
        self.assertEqual(program.count("i82801jx_sata_select_ahci(B06VR_SATA1_DEV);"), 1)
        self.assertEqual(program.count("i82801jx_disable_sata2();"), 1)
        self.assertEqual(program.count("post_code(POST_B06VU_RESET_TOPOLOGY_OK);"), 1)
        self.assertEqual(program.count("post_code(POST_B06VU_MAP_BEGIN);"), 1)
        self.assertEqual(program.count("post_code(POST_B06VU_MAP_OK);"), 1)
        self.assertEqual(program.count("post_code(POST_B06VU_FD_SAD2_BEGIN);"), 1)
        self.assertEqual(program.count("post_code(POST_B06VU_FD_SAD2_OK);"), 1)
        self.assertLess(program.index("RESET_PRE"), program.index("MAP_STAGE begin"))
        self.assertLess(program.index("MAP_STAGE begin"), program.index("FD_SAD2_STAGE begin"))
        self.assertNotRegex(
            program,
            r"\b(?:pci_io_write_config(?:8|16|32)|RCBA(?:8|16|32)|write(?:8|16|32|64)?p)\s*\(",
        )

    def test_baseline_and_route_complete_before_normal_pci_scan(self) -> None:
        scan = function_text(PCI_SOURCE, "b06vn_domain_scan_bus")
        variant = between(
            scan,
            "#if CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE",
            "#else",
        )
        baseline = variant.index("b06vqi_program_baseline_once();")
        route = variant.index("b06vu_program_ahci_route_once();")
        self.assertLess(baseline, route)
        self.assertEqual(variant.count("b06vqi_program_baseline_once();"), 1)
        self.assertEqual(variant.count("b06vu_program_ahci_route_once();"), 1)
        self.assertLess(scan.index("b06vu_program_ahci_route_once();"), scan.index("b06vn_raw_root_preflight();"))
        for consumer in (
            "b06vo_program_ioh_bus_number_once();",
            "b06vq_program_ehci_once();",
            "b06vn_start_iou0_once();",
            "pci_host_bridge_scan_bus(dev);",
        ):
            self.assertLess(scan.index("b06vu_program_ahci_route_once();"), scan.index(consumer))

    def test_post_codes_make_each_stage_and_failure_visible(self) -> None:
        expected = {
            "POST_B06VU_RESET_TOPOLOGY_OK": "0x58",
            "POST_B06VU_MAP_BEGIN": "0x59",
            "POST_B06VU_MAP_OK": "0x5a",
            "POST_B06VU_FD_SAD2_BEGIN": "0x5b",
            "POST_B06VU_FD_SAD2_OK": "0x5c",
            "POST_B06VU_ROUTE_FAIL": "0x5d",
        }
        for name, value in expected.items():
            self.assertRegex(PCI_SOURCE, rf"#define {name}\s+{value}")

    def test_builder_has_release_isolation_and_reproducibility_contract(self) -> None:
        self.assertTrue(BUILDER_PATH.is_file())
        builder = BUILDER_PATH.read_text()
        for fragment in (
            "SOURCE_DATE_EPOCH",
            "clean builds are not byte-identical",
            "install_exact",
            "refusing to replace differing artifact",
            "verify_ipxe_source_and_rom",
            "verify_seabios_source",
            "msi-x58-pro-e-b06vu-coreboot-base-4MiB.rom",
            "b06vu",
            "'CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE=y'",
            "'CONFIG_X58_PRO_E_B06VQ_ICHBASE1=y'",
            "'CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP=n'",
            "'CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_AHCI_MAP=y'",
            "'CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA2_DISABLE=y'",
            "'CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_PORT_ENABLE=n'",
            "'CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_CLOCK_FIELD=n'",
            "'CONFIG_SOUTHBRIDGE_INTEL_I82801JX=n'",
        ):
            self.assertIn(fragment, builder)


if __name__ == "__main__":
    unittest.main()
