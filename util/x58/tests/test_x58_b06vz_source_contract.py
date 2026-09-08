#!/usr/bin/env python3

import os
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
BASE_TREE = (BOARD / "devicetree_b06vy.cb").read_text()
IMAGE_TREE = (BOARD / "devicetree_b06vz.cb").read_text()
BASE_CONFIG = (ROOT / "configs/x58-pro-e-b06vy.config").read_text()
IMAGE_CONFIG = (ROOT / "configs/x58-pro-e-b06vz.config").read_text()
BUILDER = (ROOT / "scripts/build_x58_b06_sata_successor.sh").read_text()
WRAPPER = ROOT / "scripts/build_x58_b06vz.sh"

SYMBOL = "CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES"
ID = "X58PROE-B06VZ-FIXED-RESOURCES-20260906"
BASE_ID = "X58PROE-B06VY-ICH10-IOAPIC-MASK-20260906"


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


class B06VZFixedResourcesSourceContractTests(unittest.TestCase):
    def test_variant_is_separate_build_only_b06vy_successor(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VZ_FIXED_RESOURCES",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        compact = " ".join(option.split())
        for fragment in (
            "depends on X58_PRO_E_B06VY_IOAPIC_MASK",
            "default n",
            "indices 8 through 14",
            "all resources 0 through 14 have exact bounds and flags",
            "pairwise disjoint",
            "1000-ffff",
            "c0000000-dfffffff",
            "build-only successor",
            "no additional PCI, port-I/O, MMIO or MSR programming",
            "no HPET resource, decode or table",
            "no ACPI, SMBIOS, interrupt, SCI, USB or 8042 policy",
            "Do not flash it until B06VY has a recorded hardware PASS",
        ):
            self.assertIn(fragment, compact)
        self.assertNotIn("select SOUTHBRIDGE", option)

    def test_identity_topology_and_config_are_one_delta(self) -> None:
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON, PCI):
            self.assertLess(source.index(ID), source.index(BASE_ID))
        self.assertLess(MAINBOARD.index(SYMBOL), MAINBOARD.index("B06VY_IOAPIC_MASK"))
        self.assertIn('default "devicetree_b06vz.cb"', KCONFIG)
        self.assertEqual(IMAGE_TREE.count("device pci"), 13)
        self.assertEqual(
            re.sub(r"# B06VZ[^\n]*", "", IMAGE_TREE),
            re.sub(r"# B06VY[^\n]*", "", BASE_TREE),
        )

        base = config_contract(BASE_CONFIG)
        image = config_contract(IMAGE_CONFIG)
        allowed = {SYMBOL}
        self.assertEqual(
            {key: value for key, value in base.items() if key not in allowed},
            {key: value for key, value in image.items() if key not in allowed},
        )
        self.assertEqual(image[SYMBOL], f"{SYMBOL}=y")
        self.assertEqual(
            image["CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK"],
            "CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK=y",
        )
        # These feed generic SMBIOS Type 0/1/2 strings.  A separately named
        # artifact must not accidentally turn a resource-only stage into an
        # SMBIOS-content delta.
        self.assertEqual(
            image["CONFIG_MAINBOARD_PART_NUMBER"],
            base["CONFIG_MAINBOARD_PART_NUMBER"],
        )
        self.assertEqual(image["CONFIG_LOCALVERSION"], base["CONFIG_LOCALVERSION"])
        part_number_block = KCONFIG[KCONFIG.index("config MAINBOARD_PART_NUMBER") :]
        self.assertNotIn("B06VZ fixed platform resources", part_number_block)

    def test_fixed_resource_indices_bases_and_sizes_are_exact(self) -> None:
        definitions = {
            "B06VZ_SMBUS_RESOURCE_INDEX": "8u",
            "B06VZ_PM_RESOURCE_INDEX": "9u",
            "B06VZ_GPIO_RESOURCE_INDEX": "10u",
            "B06VZ_IOAPIC_RESOURCE_INDEX": "11u",
            "B06VZ_RCBA_RESOURCE_INDEX": "12u",
            "B06VZ_LAPIC_RESOURCE_INDEX": "13u",
            "B06VZ_ROM_RESOURCE_INDEX": "14u",
            "B06VZ_DOMAIN_RESOURCE_COUNT": "15u",
            "B06VZ_SMBUS_IO_BASE": "0x0400u",
            "B06VZ_SMBUS_IO_SIZE": "0x0020u",
            "B06VZ_PM_IO_BASE": "0x0500u",
            "B06VZ_PM_IO_SIZE": "0x0080u",
            "B06VZ_GPIO_IO_BASE": "0x0580u",
            "B06VZ_GPIO_IO_SIZE": "0x0040u",
            "B06VZ_IOAPIC_BASE": "0xfec00000ULL",
            "B06VZ_IOAPIC_SIZE": "0x00001000ULL",
            "B06VZ_RCBA_BASE": "0xfed1c000ULL",
            "B06VZ_RCBA_SIZE": "0x00004000ULL",
            "B06VZ_LAPIC_BASE": "0xfee00000ULL",
            "B06VZ_LAPIC_SIZE": "0x00001000ULL",
            "B06VZ_ROM_BASE": "0xff000000ULL",
            "B06VZ_ROM_SIZE": "0x01000000ULL",
        }
        for name, value in definitions.items():
            self.assertRegex(PCI, rf"#define {name}\s+{value}")

        resources = function_text(PCI, "b06vz_read_resources")
        self.assertIn("!b06vy_ioapic_attempted", resources)
        self.assertLess(
            resources.index("b06vn_read_resources(dev)"),
            resources.index("fixed_io_range_reserved"),
        )
        self.assertEqual(resources.count("fixed_io_range_reserved("), 3)
        self.assertEqual(resources.count("mmio_range("), 4)
        for name in (
            "B06VZ_SMBUS_RESOURCE_INDEX",
            "B06VZ_PM_RESOURCE_INDEX",
            "B06VZ_GPIO_RESOURCE_INDEX",
            "B06VZ_IOAPIC_RESOURCE_INDEX",
            "B06VZ_RCBA_RESOURCE_INDEX",
            "B06VZ_LAPIC_RESOURCE_INDEX",
            "B06VZ_ROM_RESOURCE_INDEX",
        ):
            self.assertEqual(resources.count(name), 1)

    def test_resource_contract_is_exact_and_covers_zero_through_fourteen(self) -> None:
        contracts = between(
            PCI,
            "b06vz_domain_resource_contracts[] = {",
            "_Static_assert(ARRAY_SIZE(b06vz_domain_resource_contracts)",
        )
        entries = re.findall(r"^\s*\{\s*[^\n]+", contracts, re.MULTILINE)
        self.assertEqual(len(entries), 15)
        for index in range(8):
            self.assertRegex(contracts, rf"\{{ {index},")
        for flags in (
            "B06VZ_RAM_FLAGS",
            "B06VZ_RESERVED_RAM_FLAGS",
            "B06VZ_MMIO_FLAGS",
            "B06VZ_FIXED_IO_FLAGS",
            "B06VZ_IO_WINDOW_FLAGS",
            "B06VZ_MEM_WINDOW_FLAGS",
        ):
            self.assertIn(flags, contracts)

        bounds = function_text(PCI, "b06vz_domain_resource_bounds_are_exact")
        exact = function_text(PCI, "b06vz_require_exact_domain_resources")
        for fragment in (
            "resource->base != expected->base",
            "resource->index != expected->index",
            "resource->size == expected->top - expected->base",
            "resource_end(resource) == expected->top - 1",
            "resource->size == 0",
            "resource->limit == expected->top - 1",
        ):
            self.assertIn(fragment, bounds)
        for fragment in (
            "actual_count != B06VZ_DOMAIN_RESOURCE_COUNT",
            "resource->flags != expected->flags",
            "left_type == right_type",
            "left->base < right->top",
            "right->base < left->top",
        ):
            self.assertIn(fragment, exact)

    def test_allocator_apertures_and_leaf_non_overlap_are_preserved(self) -> None:
        for fragment in (
            "B06VZ_GPIO_IO_BASE + B06VZ_GPIO_IO_SIZE <=\n\tB06VN_PCI_IO_BASE",
            "B06VN_ECAM_TOP <= B06VZ_IOAPIC_BASE",
            "B06VZ_ROM_TOP == B06VN_HIGH_RAM_BASE",
        ):
            self.assertIn(fragment, PCI)
        fixed_leaf = function_text(PCI, "b06vz_require_no_fixed_leaf_overlap")
        for fragment in (
            "i = B06VZ_FIRST_FIXED_RESOURCE_INDEX",
            "fixed_type == leaf_type",
            "fixed->base < leaf_top",
            "leaf->base < fixed->top",
        ):
            self.assertIn(fragment, fixed_leaf)
        assigned = function_text(PCI, "b06vn_resources_assigned")
        self.assertLess(
            assigned.index("b06vz_require_exact_domain_resources();"),
            assigned.index("b06vn_collect_and_audit_resources(leaves)"),
        )
        self.assertLess(
            assigned.index("b06vn_require_no_leaf_overlap(leaves, leaf_count);"),
            assigned.index("b06vz_require_no_fixed_leaf_overlap(leaves, leaf_count);"),
        )

    def test_vz_delta_contains_no_platform_write_or_table_path(self) -> None:
        names = (
            "b06vz_read_resources",
            "b06vz_domain_resource_bounds_are_exact",
            "b06vz_require_exact_domain_resources",
            "b06vz_require_no_fixed_leaf_overlap",
        )
        combined = "\n".join(function_text(PCI, name) for name in names)
        self.assertNotRegex(
            combined,
            r"\b(?:pci_(?:(?:io_)?write|update|or|and)_config(?:8|16|32)|"
            r"write(?:8|16|32|64)?p|out[blw]|wrmsr|"
            r"(?:setbits|clrbits|clrsetbits)(?:8|16|32|64)?|"
            r"setup_ioapic|register_new_ioapic|ioapic_set_max_vectors|"
            r"acpi_[A-Za-z0-9_]*|smbios_[A-Za-z0-9_]*)\s*\(",
        )
        for forbidden in (
            "HPET",
            "HPTC",
            "SCI",
            "PIRQ",
            "USB",
            "8042",
        ):
            self.assertNotIn(forbidden, combined)

    def test_builder_is_deterministic_and_enforces_isolation(self) -> None:
        self.assertTrue(WRAPPER.is_file())
        self.assertTrue(os.access(WRAPPER, os.X_OK))
        self.assertIn("b06vz", WRAPPER.read_text())
        for fragment in (
            "b06vv|b06vw|b06vx|b06vy|b06vz",
            'build_id="X58PROE-B06VZ-FIXED-RESOURCES-20260906"',
            "B06VZ is BUILD-ONLY",
            "recorded B06VY hardware PASS",
            "CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES=y",
            "CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK=y",
            "CONFIG_HAVE_ACPI_TABLES=n",
            "CONFIG_SOUTHBRIDGE_INTEL_I82801JX=n",
            'cmp -s -- "${first_rom}" "${second_rom}"',
        ):
            self.assertIn(fragment, BUILDER)


if __name__ == "__main__":
    unittest.main()
