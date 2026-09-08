#!/usr/bin/env python3

import hashlib
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
KCONFIG = (BOARD / "Kconfig").read_text()
MAKEFILE = (BOARD / "Makefile.mk").read_text()
BOOTBLOCK = (BOARD / "bootblock.c").read_text()
ROMSTAGE = (BOARD / "romstage.c").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
VN_DEVTREE = (BOARD / "devicetree_b06vn.cb").read_text()
VN_SOURCE = (BOARD / "b06vn_pci.c").read_text()
VN_HEADER = (BOARD / "b06vn_pci.h").read_text()
VM_SOURCE_PATH = BOARD / "b06vm_pci.c"
VM_HEADER_PATH = BOARD / "b06vm_pci.h"
VN_CONFIG = (ROOT / "configs/x58-pro-e-b06vn.config").read_text()
VN_BUILD = (ROOT / "scripts/build_x58_b06vn.sh").read_text()

VN_SYMBOL = "CONFIG_X58_PRO_E_B06VN_IOU0_PCIE_PHYS_VBIOS"
VM_SYMBOL = "CONFIG_X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE"
VN_ID = "X58PROE-B06VN-IOU0-HD5450-PHYSVBIOS-20260906"
VM_ID = "X58PROE-B06VM-AUTO-PCI-VGA-IPXE-20260905"


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


class B06VNSourceContractTests(unittest.TestCase):
    def test_option_is_separate_default_off_and_describes_one_hypothesis(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VN_IOU0_PCIE_PHYS_VBIOS",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        for fragment in (
            "depends on X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE",
            "default n",
            "0xe0018190",
            "only when Link Status reports",
            "Ports 00:01.0 and 00:07.0 receive",
            "No Link Control retrain",
            "checksum validation retained",
        ):
            self.assertIn(fragment, option)
        self.assertNotIn("default y", option)

    def test_alternate_tree_precedes_b06vm_and_keeps_fourteen_nodes(self) -> None:
        tree = between(KCONFIG, "config DEVICETREE", "config X58_PRO_E_B06M")
        self.assertIn('default "devicetree_b06vn.cb" if', tree)
        self.assertLess(tree.index("devicetree_b06vn.cb"), tree.index("devicetree_b06vm.cb"))
        pci_lines = re.findall(
            r"^\s*device pci ([0-9a-f]{2}\.[0-7]) mandatory ops (\w+)",
            VN_DEVTREE,
            re.MULTILINE,
        )
        self.assertEqual(len(pci_lines), 14)
        self.assertEqual([ops for _, ops in pci_lines].count("b06vn_root_port_ops"), 2)
        self.assertEqual([ops for _, ops in pci_lines].count("b06vn_endpoint_ops"), 12)
        self.assertIn("AMD VGA, exact device ID discovered at runtime", VN_DEVTREE)

    def test_b06vn_and_b06vm_pci_objects_are_mutually_exclusive(self) -> None:
        branch = between(
            MAKEFILE,
            "ifeq ($(CONFIG_X58_PRO_E_B06VN_IOU0_PCIE_PHYS_VBIOS),y)",
            "# The exact GT630",
        )
        self.assertIn("ramstage-y += b06vn_pci.c", branch)
        self.assertIn("else", branch)
        self.assertIn(
            "ramstage-$(CONFIG_X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE) += b06vm_pci.c",
            branch,
        )
        self.assertEqual(MAKEFILE.count("ramstage-y += b06vn_pci.c"), 1)

    def test_exact_ecam_address_and_single_iou0_start_write(self) -> None:
        for fragment in (
            "0xe0008190ULL",
            "0xe0018190ULL",
            "0xe0038190ULL",
            "#define B06VN_IOU0_X16_START\t0x0000000cu",
            "#define B06VN_PCIE_PRTX_BIF_CTRL 0x190",
        ):
            self.assertIn(fragment, VN_SOURCE)
        start = function_text(VN_SOURCE, "b06vn_start_iou0_once")
        self.assertEqual(start.count("write16p("), 1)
        self.assertIn("B06VN_IOU0_X16_DEV", start)
        self.assertIn("B06VN_PCIE_PRTX_BIF_CTRL", start)
        self.assertIn("B06VN_IOU0_X16_START", start)
        self.assertNotIn("pci_io_write", start)
        # The second source-level write belongs to the separately gated B06VO
        # successor.  A B06VN build preprocesses that entire block away.
        self.assertEqual(VN_SOURCE.count("write16p("), 2)
        self.assertLess(
            VN_SOURCE.index("#if CONFIG_X58_PRO_E_B06VO_IOHBUSNO_ROUTE"),
            VN_SOURCE.index("static void b06vo_program_ioh_bus_number_once"),
        )

    def test_link_start_is_gated_once_and_poll_is_bounded(self) -> None:
        start = function_text(VN_SOURCE, "b06vn_start_iou0_once")
        for fragment in (
            "b06vn_iou0_start_attempted",
            "B06VN_IOU0_X16_ID",
            "B06VN_LNKSTA_DLL_ACTIVE",
            "polls < B06VN_LINK_POLL_COUNT",
            "udelay(B06VN_LINK_POLL_US)",
            "PCI_EXP_LNKSTA_LT",
            "continuing to serial payload fallback",
        ):
            self.assertIn(fragment, start)
        self.assertIn(
            "B06VN_LINK_POLL_US * B06VN_LINK_POLL_COUNT == 1000000",
            VN_SOURCE,
        )
        self.assertNotIn("pciexp_retrain_link", VN_SOURCE)
        self.assertNotIn("PCI_EXP_LNKCTL_RL", VN_SOURCE)
        self.assertNotIn("wait_us(", VN_SOURCE)

    def test_only_iou0_is_mutated_and_other_ports_are_telemetry(self) -> None:
        start = function_text(VN_SOURCE, "b06vn_start_iou0_once")
        for dev in (
            "B06VN_IOU2_X4_DEV",
            "B06VN_IOU0_X16_DEV",
            "B06VN_AUX_X16_DEV",
        ):
            self.assertIn(f'b06vn_log_iou("PRE", {dev})', start)
            self.assertIn(f'b06vn_log_iou("POST", {dev})', start)
        write_call = re.search(r"write16p\((.*?)\);", start, re.DOTALL)
        self.assertIsNotNone(write_call)
        self.assertIn("B06VN_IOU0_X16_DEV", write_call.group(1))
        self.assertNotIn("B06VN_IOU2_X4_DEV", write_call.group(1))
        self.assertNotIn("B06VN_AUX_X16_DEV", write_call.group(1))

    def test_gpu_identity_is_measured_not_guessed(self) -> None:
        for fragment in (
            "B06VN_AMD_VENDOR_ID\t0x1002u",
            "PCI_CLASS_DISPLAY_VGA",
            "PCI_HEADER_TYPE_NORMAL",
            "runtime_optional",
            "(id & 0xffffu) == expected->id",
            "AMD_VGA_PRESENT_PHYSICAL_VBIOS",
        ):
            self.assertIn(fragment, VN_SOURCE)
        for guessed_id in ("0x68f9", "0x68e1", "0x68f8"):
            self.assertNotIn(guessed_id, VN_SOURCE.lower())

    def test_absent_gpu_is_fail_soft_but_required_devices_remain_strict(self) -> None:
        topology = function_text(VN_SOURCE, "b06vn_require_enumerated_topology")
        resources = function_text(VN_SOURCE, "b06vn_collect_and_audit_resources")
        enabled = function_text(VN_SOURCE, "b06vn_resources_enabled")
        for function in (topology, resources, enabled):
            self.assertIn("b06vn_optional_device_absent", function)
            self.assertIn("continue;", function)
        self.assertIn("ABSENT_SERIAL_SEABIOS_FALLBACK", VN_SOURCE)
        for required_id in (
            "0x816810ecu",
            "0x3a378086u",
            "0x3a3c8086u",
            "0x3a208086u",
            "0x3a268086u",
        ):
            self.assertIn(required_id, VN_SOURCE)

    def test_gpu_identity_gate_precedes_bar_sizing(self) -> None:
        self.assertIn(".enable = b06vn_probe_gate", VN_SOURCE)
        probe = function_text(VN_SOURCE, "b06vn_probe_gate")
        self.assertLess(probe.index("b06vn_require_identity"), probe.index("b06vn_clear_command"))
        self.assertIn(".read_resources = pci_dev_read_resources", VN_SOURCE)

    def test_physical_vbios_and_ipxe_cbfs_policy_are_explicit(self) -> None:
        vn_branch = between(
            MAKEFILE,
            "ifeq ($(CONFIG_X58_PRO_E_B06VN_IOU0_PCIE_PHYS_VBIOS),y)",
            "else ifeq ($(CONFIG_X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE),y)",
        )
        self.assertIn("add-int -i 1 -n etc/optionroms-checksum", vn_branch)
        self.assertIn("add-int -i 1 -n etc/pci-optionrom-exec", vn_branch)
        self.assertNotIn("add-int -i 0", vn_branch)
        for fragment in (
            "# CONFIG_VGA_ROM_RUN is not set",
            "CONFIG_NO_GFX_INIT=y",
            'CONFIG_PXE_ROM_ID="10ec,8168"',
            "No AMD option ROM is embedded in CBFS",
        ):
            self.assertIn(fragment, VN_CONFIG)
        self.assertIn("must not embed an AMD VGA option ROM", VN_BUILD)
        self.assertIn("int.from_bytes(data, \"little\") == 1", VN_BUILD)

    def test_build_identity_wiring_precedes_b06vm(self) -> None:
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON):
            self.assertIn(VN_ID, source)
            self.assertIn(VM_ID, source)
            self.assertLess(source.index(VN_ID), source.index(VM_ID))
        self.assertLess(MAINBOARD.index(VN_SYMBOL), MAINBOARD.index(VM_SYMBOL))
        self.assertIn(".enable_dev = x58_b06vn_enable_dev", MAINBOARD)
        self.assertIn("void x58_b06vn_enable_dev", VN_HEADER)

    def test_b06vm_pci_sources_are_unchanged(self) -> None:
        self.assertEqual(
            hashlib.sha256(VM_SOURCE_PATH.read_bytes()).hexdigest(),
            "7e1067753bb89312f31b7854d4a2e39fe7e54d0cc9cb4a747dd09afd36f2d9d3",
        )
        self.assertEqual(
            hashlib.sha256(VM_HEADER_PATH.read_bytes()).hexdigest(),
            "470e6d3a0df973ae2b43f82f732d5235dcf33f3c50623ec4154ef3748de00cb5",
        )


if __name__ == "__main__":
    unittest.main()
