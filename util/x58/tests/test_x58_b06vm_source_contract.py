#!/usr/bin/env python3

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
MEMMAP = (BOARD / "fail_closed_memmap.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
VL_DEVTREE = (BOARD / "devicetree.cb").read_text()
VM_DEVTREE = (BOARD / "devicetree_b06vm.cb").read_text()
PCI_SOURCE = (BOARD / "b06vm_pci.c").read_text()
CPU_MAKEFILE = (COREBOOT / "src/cpu/intel/model_206cx/Makefile.mk").read_text()
INTEL_TIMEBASE = (COREBOOT / "src/cpu/intel/common/fsb.c").read_text()

VM_SYMBOL = "CONFIG_X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE"
VL_SYMBOL = "CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS"
VM_ID = "X58PROE-B06VM-AUTO-PCI-VGA-IPXE-20260905"
VL_ID = "X58PROE-B06VL-BROAD-HARD-GATE-SEABIOS-20260905"


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


class B06VMSourceContractTests(unittest.TestCase):
    def test_option_is_separate_default_off_and_has_bus_master_contract(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        for line in (
            "depends on X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS",
            "depends on PCI_ALLOW_BUS_MASTER",
            "depends on PCI_SET_BUS_MASTER_PCI_BRIDGES",
            "depends on !PCI_ALLOW_BUS_MASTER_ANY_DEVICE",
            "select HAVE_CONFIGURABLE_RAMSTAGE",
            "select CONFIGURABLE_RAMSTAGE",
            "select MINIMAL_PCI_SCANNING",
            "select CPU_INTEL_COMMON",
            "select CPU_INTEL_COMMON_TIMEBASE",
            "select UDELAY_TSC",
            "default n",
        ):
            self.assertIn(line, option)
        self.assertNotIn("default y", option)
        self.assertIn("00:0d.* through 00:16.* remains", option)
        self.assertIn("only the two selected PCI bridges receive BME", option)

    def test_b06vm_has_a_real_scoped_westmere_tsc_delay_provider(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        self.assertIn("model 0x2c's fixed 133-MHz BCLK", option)
        self.assertIn("nominal ratio 18 rounds to the expected", option)
        self.assertIn("2400 MHz", option)

        timebase_case = between(INTEL_TIMEBASE, "case 0x25:", "case 0x2a:")
        self.assertIn("case 0x2c:", timebase_case)
        self.assertIn("Westmere-EP/Gulftown BCLK fixed at 133MHz", timebase_case)
        self.assertIn("*fsb = 133;", timebase_case)
        self.assertIn("rdmsr(MSR_PLATFORM_INFO).lo >> 8", timebase_case)
        self.assertEqual(100 * round((18 * 133) / 100), 2400)

        self.assertIn("ifneq ($(CONFIG_CPU_INTEL_COMMON_TIMEBASE),y)", CPU_MAKEFILE)
        self.assertIn("ramstage-y += timebase_stub.c", CPU_MAKEFILE)
        self.assertLess(
            CPU_MAKEFILE.index("ifneq ($(CONFIG_CPU_INTEL_COMMON_TIMEBASE),y)"),
            CPU_MAKEFILE.index("ramstage-y += timebase_stub.c"),
        )

    def test_b06vm_uses_an_alternate_tree_and_b06vl_tree_is_unchanged(self) -> None:
        tree_default = between(KCONFIG, "config DEVICETREE", "config X58_PRO_E_B06M")
        self.assertIn(
            'default "devicetree_b06vm.cb" if '
            "X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE",
            tree_default,
        )
        self.assertLess(tree_default.index("devicetree_b06vm.cb"), tree_default.index('"devicetree.cb"'))
        self.assertEqual(
            VL_DEVTREE,
            "chip mainboard/msi/x58_pro_e\n"
            "\tdevice cpu_cluster 0 on end\n"
            "\tdevice domain 0 on end\n"
            "end\n",
        )

    def test_static_tree_contains_only_the_exact_fourteen_mandatory_functions(self) -> None:
        pci_lines = re.findall(r"^\s*device pci ([0-9a-f]{2}\.[0-7]) mandatory ops (\w+)", VM_DEVTREE, re.MULTILINE)
        self.assertEqual(len(pci_lines), 14)
        self.assertEqual(
            [bdf for bdf, _ in pci_lines],
            [
                "03.0", "00.0", "1a.0", "1a.1", "1a.2", "1a.7",
                "1c.4", "00.0", "1d.0", "1d.1", "1d.2", "1d.7",
                "1f.2", "1f.5",
            ],
        )
        self.assertEqual(
            [ops for _, ops in pci_lines].count("b06vm_root_port_ops"), 2
        )
        self.assertEqual(
            [ops for _, ops in pci_lines].count("b06vm_endpoint_ops"), 12
        )
        for forbidden in ("0d.", "0e.", "0f.", "10.", "11.", "12.", "13.", "14.", "15.", "16."):
            self.assertNotIn(f"device pci {forbidden}", VM_DEVTREE)

    def test_exact_observed_identity_allowlist_precedes_bar_sizing(self) -> None:
        for pci_id in (
            "0x340a8086u", "0x0f0010deu", "0x3a378086u", "0x3a388086u",
            "0x3a398086u", "0x3a3c8086u", "0x3a488086u", "0x816810ecu",
            "0x3a348086u", "0x3a358086u", "0x3a368086u", "0x3a3a8086u",
            "0x3a208086u", "0x3a268086u",
        ):
            self.assertIn(pci_id, PCI_SOURCE)
        for pci_class in (
            "PCI_CLASS_BRIDGE_PCI", "PCI_CLASS_DISPLAY_VGA",
            "PCI_CLASS_SERIAL_USB", "PCI_CLASS_NETWORK_ETHERNET",
            "PCI_CLASS_STORAGE_IDE",
        ):
            self.assertIn(pci_class, PCI_SOURCE)

        self.assertIn(".enable = b06vm_probe_gate", PCI_SOURCE)
        probe_gate = function_text(PCI_SOURCE, "b06vm_probe_gate")
        self.assertLess(
            probe_gate.index("b06vm_require_identity"),
            probe_gate.index("b06vm_clear_command"),
        )
        raw_gate = function_text(PCI_SOURCE, "b06vm_raw_root_preflight")
        self.assertLess(
            raw_gate.index("Validate the complete root allowlist"),
            raw_gate.index("Only after every root identity passed"),
        )

    def test_scan_is_minimal_and_uses_only_standard_selected_paths(self) -> None:
        self.assertIn("_Static_assert(CONFIG(MINIMAL_PCI_SCANNING)", PCI_SOURCE)
        self.assertIn("pci_host_bridge_scan_bus(dev);", PCI_SOURCE)
        self.assertIn("pci_scan_bridge(dev);", PCI_SOURCE)
        self.assertIn(".read_resources = pci_bus_read_resources", PCI_SOURCE)
        self.assertIn(".set_resources = pci_dev_set_resources", PCI_SOURCE)
        self.assertIn(".enable_resources = pci_bus_enable_resources", PCI_SOURCE)
        self.assertIn(".read_resources = pci_dev_read_resources", PCI_SOURCE)
        self.assertIn(".enable_resources = pci_dev_enable_resources", PCI_SOURCE)
        self.assertNotIn("pci_domain_read_resources", PCI_SOURCE)
        self.assertNotIn("pciexp_scan_bridge", PCI_SOURCE)
        self.assertNotIn("default_pciexp_ops_bus", PCI_SOURCE)

    def test_platform_and_handoff_gates_are_exact(self) -> None:
        for value in (
            "0x2d818086u", "0xe0000001u", "0x34058086u", "0x06000013u",
            "0x342e8086u", "0xbc000000u", "0x3c000000u", "0x00000001u",
            "0x00000bc3u", "0x00000fc0u", "0x000013c3u", "0x000013c0u",
        ):
            self.assertIn(value, PCI_SOURCE)
        handoff = function_text(PCI_SOURCE, "b06vm_verified_handoff")
        self.assertIn("x58_b06v6_handoff_is_valid(handoff)", handoff)
        self.assertIn("X58_B06VF_HANDOFF_LOWMEM_SMOKED", handoff)
        self.assertIn("X58_B06VL_HANDOFF_BROAD_POST_MINIT", handoff)

    def test_memory_map_and_allocator_apertures_are_exact_and_disjoint(self) -> None:
        resources = function_text(PCI_SOURCE, "b06vm_read_resources")
        for fragment in (
            "ram_range(dev, 0, 0x00000000, 0x000a0000)",
            "mmio_range(dev, 1, 0x000a0000, 0x00020000)",
            "reserved_ram_range(dev, 2, 0x000c0000, 0x00040000)",
            "ram_from_to(dev, 3, B06VM_LOW_RAM_BASE, B06VM_LOW_RAM_TOP)",
            "ram_from_to(dev, 4, B06VM_HIGH_RAM_BASE, B06VM_HIGH_RAM_TOP)",
            "domain_io_window_from_to(dev, 5, B06VM_PCI_IO_BASE",
            "domain_mem_window_from_to(dev, 6, B06VM_PCI_MMIO_BASE",
            "mmio_from_to(dev, 7, B06VM_ECAM_BASE, B06VM_ECAM_TOP)",
        ):
            self.assertIn(fragment, resources)
        for define in (
            "#define B06VM_LOW_RAM_TOP\t0xc0000000ULL",
            "#define B06VM_HIGH_RAM_BASE\t0x100000000ULL",
            "#define B06VM_HIGH_RAM_TOP\t0x140000000ULL",
            "#define B06VM_PCI_IO_BASE\t0x1000u",
            "#define B06VM_PCI_IO_TOP\t0x10000u",
            "#define B06VM_PCI_MMIO_BASE\t0xc0000000ULL",
            "#define B06VM_PCI_MMIO_TOP\t0xe0000000ULL",
        ):
            self.assertIn(define, PCI_SOURCE)

    def test_postcar_caches_full_low_ram_but_keeps_vga_hole_uc(self) -> None:
        for fragment in (
            "#define B06VM_LOW_RAM0_BASE\t0x00000000u",
            "#define B06VM_LOW_RAM0_SIZE\t0x80000000u",
            "#define B06VM_LOW_RAM1_BASE\t0x80000000u",
            "#define B06VM_LOW_RAM1_SIZE\t0x40000000u",
            "#define B06VM_VGA_HOLE_BASE\t0x000a0000u",
            "#define B06VM_VGA_HOLE_SIZE\t0x00020000u",
            "B06VM_LOW_RAM0_SIZE, MTRR_TYPE_WRBACK",
            "B06VM_LOW_RAM1_SIZE, MTRR_TYPE_WRBACK",
            "B06VM_VGA_HOLE_SIZE, MTRR_TYPE_UNCACHEABLE",
        ):
            self.assertIn(fragment, MEMMAP)

        # The common postcar path appends the 256-KiB ROM WP entry after
        # these three board entries.  Ramstage must reject any other layout.
        for fragment in (
            "#define B06VM_POSTCAR_MTRR0_BASE_LO\t0x00000006u",
            "#define B06VM_POSTCAR_MTRR0_MASK_LO\t0x80000800u",
            "#define B06VM_POSTCAR_MTRR1_BASE_LO\t0x80000006u",
            "#define B06VM_POSTCAR_MTRR1_MASK_LO\t0xc0000800u",
            "#define B06VM_POSTCAR_MTRR2_BASE_LO\t0x000a0000u",
            "#define B06VM_POSTCAR_MTRR2_MASK_LO\t0xfffe0800u",
            "B06VM_POSTCAR_MTRR3_BASE_LO",
            "B06VM_POSTCAR_MTRR3_MASK_LO",
            "ARRAY_SIZE(b06v6_expected_mtrr_base_lo)",
        ):
            self.assertIn(fragment, RAMMON)

    def test_allocation_and_enable_audits_fail_closed(self) -> None:
        allocated = function_text(PCI_SOURCE, "b06vm_require_allocated_resource")
        for check in (
            "IORESOURCE_ASSIGNED", "IORESOURCE_STORED", "IORESOURCE_ABOVE_4G",
            "resource_end(resource) != top - 1", "B06VM_PCI_IO_BASE",
            "B06VM_PCI_MMIO_BASE",
        ):
            self.assertIn(check, allocated)
        self.assertNotIn("resource->limit != top - 1", allocated)
        self.assertIn("b06vm_require_no_leaf_overlap(leaves, leaf_count)", PCI_SOURCE)
        self.assertIn(
            "BOOT_STATE_INIT_ENTRY(BS_DEV_RESOURCES, BS_ON_EXIT,",
            PCI_SOURCE,
        )
        self.assertIn(
            "BOOT_STATE_INIT_ENTRY(BS_DEV_ENABLE, BS_ON_EXIT,",
            PCI_SOURCE,
        )
        enabled = function_text(PCI_SOURCE, "b06vm_resources_enabled")
        self.assertIn("expected->bridge ?", enabled)
        self.assertIn("PCI_COMMAND_MASTER : 0", enabled)
        self.assertIn("dev->command & PCI_COMMAND_MASTER", enabled)

    def test_identity_and_build_wiring_precede_b06vl(self) -> None:
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON):
            self.assertIn(VM_ID, source)
            self.assertIn(VL_ID, source)
            self.assertLess(source.index(VM_ID), source.index(VL_ID))
        self.assertLess(MAINBOARD.index(VM_SYMBOL), MAINBOARD.index(VL_SYMBOL))
        self.assertIn(".enable_dev = x58_b06vm_enable_dev", MAINBOARD)
        self.assertIn(
            "ramstage-$(CONFIG_X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE) += b06vm_pci.c",
            MAKEFILE,
        )

    def test_bad_checksum_gt630_has_board_local_seabios_override(self) -> None:
        """The measured card ROM sums to 0xff and SeaBIOS would reject it."""
        for fragment in (
            "ifeq ($(CONFIG_X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE),y)",
            "b06vm_seabios_optionrom_checksum",
            "remove -n etc/optionroms-checksum",
            "add-int -i 0 -n etc/optionroms-checksum",
            "remove -n etc/pci-optionrom-exec",
            "add-int -i 1 -n etc/pci-optionrom-exec",
        ):
            self.assertIn(fragment, MAKEFILE)


if __name__ == "__main__":
    unittest.main()
