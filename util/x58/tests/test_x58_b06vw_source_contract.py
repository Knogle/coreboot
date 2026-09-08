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
DEVICETREE = (BOARD / "devicetree_b06vw.cb").read_text()
BASE_CONFIG = (ROOT / "configs/x58-pro-e-b06vv.config").read_text()
IMAGE_CONFIG = (ROOT / "configs/x58-pro-e-b06vw.config").read_text()
BUILDER = (ROOT / "scripts/build_x58_b06_sata_successor.sh").read_text()
WRAPPER = ROOT / "scripts/build_x58_b06vw.sh"

SYMBOL = "CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO"
ID = "X58PROE-B06VW-ICH10-AHCI-MMIO-20260906"
BASE_ID = "X58PROE-B06VV-ICH10-PCS-SCLK-20260906"


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


class B06VWMinimalAhciMmioSourceContractTests(unittest.TestCase):
    def test_variant_is_default_off_and_leaves_port_ownership_to_payload(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VW_ICH10_AHCI_MMIO",
            "config X58_PRO_E_B06VX_AHCI_USB_TRACE",
        )
        compact = " ".join(option.split())
        for fragment in (
            "depends on X58_PRO_E_B06VV_ICH10_PCS_SCLK",
            "default n",
            "dynamically assigned and",
            "stored 2-KiB ABAR",
            "no I/O decode or bus",
            "mastering. Enable only PCI_COMMAND.MEM",
            "Enable only PCI_COMMAND.MEM",
            "set only GHC.AE",
            "writing only PI's low byte to 3f",
            "does not choose an ABAR address",
            "set BME, set GHC.HR or GHC.IE",
            "CLB,",
            "FB or PxCMD",
            "COMRESET/IDENTIFY",
        ):
            self.assertIn(fragment, compact)
        self.assertNotIn("select SOUTHBRIDGE", option)

    def test_identity_config_and_topology_are_distinct_successors(self) -> None:
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON, PCI):
            self.assertLess(source.index(ID), source.index(BASE_ID))
        self.assertLess(MAINBOARD.index(SYMBOL), MAINBOARD.index("B06VV_ICH10_PCS_SCLK"))
        self.assertIn('default "devicetree_b06vw.cb"', KCONFIG)
        self.assertEqual(DEVICETREE.count("device pci"), 13)
        self.assertIn("device pci 1f.2 mandatory ops b06vn_endpoint_ops", DEVICETREE)
        self.assertNotRegex(DEVICETREE, r"device pci 1f\.5\b")

        base = config_contract(BASE_CONFIG)
        image = config_contract(IMAGE_CONFIG)
        allowed = {SYMBOL, "CONFIG_MAINBOARD_PART_NUMBER", "CONFIG_LOCALVERSION"}
        self.assertEqual(
            {k: v for k, v in base.items() if k not in allowed},
            {k: v for k, v in image.items() if k not in allowed},
        )
        self.assertEqual(image[SYMBOL], f"{SYMBOL}=y")
        self.assertEqual(image["CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT"],
                         "CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT=y")

    def test_global_tuples_are_exact_and_minimal(self) -> None:
        for name, value in {
            "B06VW_AHCI_CAP": "0x00u",
            "B06VW_AHCI_GHC": "0x04u",
            "B06VW_AHCI_PI": "0x0cu",
            "B06VW_AHCI_VS": "0x10u",
            "B06VW_AHCI_CAP_TARGET": "0xff22ffc5u",
            "B06VW_AHCI_PI_TARGET": "0x0000003fu",
            "B06VW_AHCI_VS_TARGET": "0x00010200u",
        }.items():
            self.assertRegex(PCI, rf"#define {name}\s+{value}")
        reset = function_text(PCI, "b06vw_global_reset_is_exact")
        target = function_text(PCI, "b06vw_global_target_is_exact")
        self.assertIn("snapshot->ghc == 0 && snapshot->pi == 0", reset)
        self.assertIn("snapshot->ghc == B06VW_AHCI_GHC_AE", target)
        self.assertIn("snapshot->pi == B06VW_AHCI_PI_TARGET", target)
        for body in (reset, target):
            self.assertIn("snapshot->cap == B06VW_AHCI_CAP_TARGET", body)
            self.assertIn("snapshot->vs == B06VW_AHCI_VS_TARGET", body)

    def test_dynamic_resource_and_mem_only_decode_are_gated_before_mmio(self) -> None:
        config_gate = function_text(PCI, "b06vw_config_is_exact")
        self.assertIn("b06vv_snapshot_is_exact", config_gate)
        self.assertIn("I82801JX_SATA_PCS_ALL_PORTS_ENABLED", config_gate)
        self.assertIn("I82801JX_SATA_SCLKCG_FIELD1_REQUIRED", config_gate)
        program = function_text(PCI, "b06vw_program_mmio_once")
        for fragment in (
            "probe_resource(sata, I82801JX_SATA_ABAR)",
            "b06vw_config_is_exact(&config, sata, abar, 0)",
            "base = (uintptr_t)abar->base;",
            "pci_io_write_config16(B06VR_SATA1_DEV, PCI_COMMAND,",
            "PCI_COMMAND_MEMORY);",
            "b06vw_config_is_exact(&config, sata, abar,",
            "ABAR is accessed only after its resource and MEM-only decode pass",
        ):
            self.assertIn(fragment, program)
        self.assertLess(program.index("pci_io_write_config16"),
                        program.index("b06vw_read_global(base)"))
        self.assertNotRegex(program, r"(?:0xfec|0xfed|0xd[0-9a-f]{7})[0-9a-f]*u?")

    def test_ae_then_pi_write_scope_is_exact(self) -> None:
        program = function_text(PCI, "b06vw_program_mmio_once")
        self.assertEqual(program.count("write32p("), 1)
        self.assertEqual(program.count("write8p("), 1)
        self.assertIn("write32p(base + B06VW_AHCI_GHC, B06VW_AHCI_GHC_AE);", program)
        self.assertIn("write8p(base + B06VW_AHCI_PI,", program)
        self.assertLess(program.index("write32p("), program.index("write8p("))
        self.assertEqual(program.count("pci_io_write_config16("), 1)
        for forbidden in (
            "B06VW_AHCI_CAP,", "B06VW_AHCI_VS,", "GHC_HR", "GHC_IE",
            "COMRESET(", "IDENTIFY(", "PCI_COMMAND_MASTER);",
        ):
            self.assertNotIn(forbidden, program)
        self.assertIn("BME/HR/IE/CAP/VS/CLB/FB/PxCMD/COMRESET/IDENTIFY untouched", program)

    def test_hooks_preserve_vv_order_and_reverify_before_payload(self) -> None:
        assigned = function_text(PCI, "b06vn_resources_assigned")
        enabled = function_text(PCI, "b06vn_resources_enabled")
        self.assertLess(assigned.index("b06vv_program_pcs_once();"),
                        assigned.index("b06vv_program_sclk_once();"))
        self.assertLess(assigned.index("b06vv_program_sclk_once();"),
                        assigned.index("b06vw_program_mmio_once();"))
        self.assertIn("b06vw_verify_final_once();", enabled)
        self.assertLess(enabled.index("b06vw_verify_final_once();"),
                        enabled.index("b06vq_log_usb_runtime_once();"))
        final = function_text(PCI, "b06vw_verify_final_once")
        self.assertIn("b06vw_config_is_exact", final)
        self.assertIn("b06vw_global_target_is_exact", final)
        self.assertIn("b06vw_final_verified = true;", final)
        self.assertEqual(final.count("post_code(POST_B06VW_READY);"), 1)
        self.assertNotRegex(final, r"\b(?:write8p|write16p|write32p|pci_io_write_config)\s*\(")

    def test_post_codes_and_release_builder_are_explicit(self) -> None:
        for name, value in {
            "POST_B06VW_MEM_BEGIN": "0x68",
            "POST_B06VW_MEM_OK": "0x69",
            "POST_B06VW_AE_BEGIN": "0x6a",
            "POST_B06VW_AE_OK": "0x6b",
            "POST_B06VW_PI_BEGIN": "0x6c",
            "POST_B06VW_PI_OK": "0x6d",
            "POST_B06VW_READY": "0x6e",
            "POST_B06VW_FAIL": "0x6f",
        }.items():
            self.assertRegex(PCI, rf"#define {name}\s+{value}")
        self.assertTrue(WRAPPER.is_file())
        self.assertIn("b06vw", WRAPPER.read_text())
        for fragment in (
            "SOURCE_DATE_EPOCH",
            "env -u DEBUG make -C",
            "cmp -s -- \"${first_rom}\" \"${second_rom}\"",
            "CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO=y",
            "CONFIG_SOUTHBRIDGE_INTEL_I82801JX=n",
        ):
            self.assertIn(fragment, BUILDER)


if __name__ == "__main__":
    unittest.main()
