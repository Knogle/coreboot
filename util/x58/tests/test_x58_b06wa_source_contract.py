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
BASE_TREE = (BOARD / "devicetree_b06vz.cb").read_text()
IMAGE_TREE = (BOARD / "devicetree_b06wa.cb").read_text()
BASE_CONFIG = (ROOT / "configs/x58-pro-e-b06vz.config").read_text()
IMAGE_CONFIG = (ROOT / "configs/x58-pro-e-b06wa.config").read_text()
BUILDER = (ROOT / "scripts/build_x58_b06_sata_successor.sh").read_text()
WRAPPER = ROOT / "scripts/build_x58_b06wa.sh"

SYMBOL = "CONFIG_X58_PRO_E_B06WA_HPET_DECODE"
ID = "X58PROE-B06WA-ICH10-HPET-DECODE-20260906"
BASE_ID = "X58PROE-B06VZ-FIXED-RESOURCES-20260906"


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


class B06WAHpetDecodeSourceContractTests(unittest.TestCase):
    def test_variant_is_separate_build_only_b06vz_successor(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06WA_HPET_DECODE",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        compact = " ".join(option.split())
        for fragment in (
            "depends on X58_PRO_E_B06VZ_FIXED_RESOURCES",
            "default n",
            "HPTC allowlist gates",
            "00000000 to exact 00000080",
            "0429b17f:8086a301",
            "timer-0 interrupt enable clear",
            "unchanged 64-bit main counter",
            "resource index 15, fed00000/400",
            "does not start or write the counter",
            "SCI, SMI or ACPI",
            "IOAPIC-entry, USB, PIC or LAPIC programming",
            "Do not flash it until B06VZ has a recorded hardware PASS",
        ):
            self.assertIn(fragment, compact)
        self.assertNotIn("select SOUTHBRIDGE", option)

    def test_identity_topology_and_config_are_one_delta(self) -> None:
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON, PCI):
            self.assertLess(source.index(ID), source.index(BASE_ID))
        self.assertLess(MAINBOARD.index(SYMBOL), MAINBOARD.index("B06VZ_FIXED_RESOURCES"))
        self.assertIn('default "devicetree_b06wa.cb"', KCONFIG)
        self.assertEqual(IMAGE_TREE.count("device pci"), 13)
        self.assertEqual(
            re.sub(r"# B06WA[^\n]*", "", IMAGE_TREE),
            re.sub(r"# B06VZ[^\n]*", "", BASE_TREE),
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
            image["CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES"],
            "CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES=y",
        )
        self.assertEqual(
            image["CONFIG_MAINBOARD_PART_NUMBER"],
            base["CONFIG_MAINBOARD_PART_NUMBER"],
        )
        self.assertEqual(image["CONFIG_LOCALVERSION"], base["CONFIG_LOCALVERSION"])

    def test_hptc_preflight_is_exact_and_precedes_the_only_write(self) -> None:
        definitions = {
            "B06WA_HPTC_ADDRESS_SELECT_MASK": "GENMASK(1, 0)",
            "B06WA_HPTC_DECODE_ENABLE": "BIT(7)",
            "B06WA_HPTC_DISABLED": "0x00000000u",
            "B06WA_HPET_BASE": "0xfed00000ULL",
            "B06WA_HPET_SIZE": "0x00000400ULL",
            "B06WA_HPET_CAP_ID_LOW_TARGET": "0x8086a301u",
            "B06WA_HPET_CAP_ID_HIGH_TARGET": "0x0429b17fu",
            "B06WA_HPET_TIMER_INTERRUPT_ENABLE": "BIT(2)",
            "B06WA_HPET_COUNTER_STABILITY_DELAY_US": "4096u",
        }
        for name, value in definitions.items():
            self.assertIn(f"#define {name}", PCI)
            self.assertIn(value, PCI[PCI.index(f"#define {name}") :][:160])

        init = function_text(PCI, "b06wa_decode_and_gate_hpet_once")
        write = "write32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_HPTC, target);"
        self.assertEqual(init.count("write32p("), 1)
        self.assertIn(write, init)
        for gate in (
            "b06wa_hpet_attempted",
            "!b06vy_ioapic_attempted",
            "lpc_id != B06VY_LPC_ID",
            "rcba != B06VY_RCBA_ENABLED",
            "OIC) != B06VY_OIC_ENABLED",
            "hptc != B06WA_HPTC_DISABLED && hptc != B06WA_HPTC_FED00000",
            "DISABLED_APERTURE_NOT_OPEN_BUS",
            "target != B06WA_HPTC_FED00000",
        ):
            self.assertIn(gate, init)
            self.assertLess(init.index(gate), init.index(write))
        self.assertIn("(hptc & ~B06WA_HPTC_CONTROL_MASK)", init)
        self.assertIn("| B06WA_HPTC_FED00000", init)

    def test_live_hpet_gate_is_quiescent_and_read_only(self) -> None:
        init = function_text(PCI, "b06wa_decode_and_gate_hpet_once")
        for fragment in (
            "hptc != B06WA_HPTC_FED00000",
            "cap_low != B06WA_HPET_CAP_ID_LOW_TARGET",
            "cap_high != B06WA_HPET_CAP_ID_HIGH_TARGET",
            "config_low != 0 || config_high != 0",
            "interrupt_status_low != 0 || interrupt_status_high != 0",
            "timer0_config_low & B06WA_HPET_TIMER_INTERRUPT_ENABLE",
            "counter_first.low != counter_second.low",
            "counter_first.high != counter_second.high",
            "udelay(B06WA_HPET_COUNTER_STABILITY_DELAY_US)",
            "STOPPED=1",
            "TIMER0_IRQ=0",
            "ROUTE_WRITE=0",
            "COUNTER_WRITE=0",
            "ACPI_TABLE=0",
        ):
            self.assertIn(fragment, init)
        self.assertEqual(init.count("write32p("), 1)
        for final_reread in (
            "B06WA_HPET_GENERAL_CONFIG_LOW",
            "B06WA_HPET_GENERAL_CONFIG_HIGH",
            "B06WA_HPET_GENERAL_INTERRUPT_STATUS_LOW",
            "B06WA_HPET_GENERAL_INTERRUPT_STATUS_HIGH",
            "B06WA_HPET_TIMER0_CONFIG_LOW",
        ):
            self.assertGreaterEqual(init.count(final_reread), 2)
        self.assertLess(
            init.index('b06wa_hpet_fail("FINAL_STATE_RECHECK")'),
            init.index("b06wa_hpet_ready = true"),
        )
        self.assertNotRegex(
            init,
            r"write(?:8|16|64)p|pci_(?:io_)?write_config|out[blw]|wrmsr|"
            r"setup_ioapic|b06vy_ioapic_(?:select|read|write_exact)\s*\(",
        )

    def test_hpet_resource_extends_vz_contract_exactly(self) -> None:
        for fragment in (
            "#define B06WA_HPET_RESOURCE_INDEX 15u",
            "#define B06WA_DOMAIN_RESOURCE_COUNT 16u",
            "B06WA_HPET_RESOURCE_INDEX == B06VZ_DOMAIN_RESOURCE_COUNT",
            "B06VZ_IOAPIC_BASE + B06VZ_IOAPIC_SIZE <= B06WA_HPET_BASE",
            "B06WA_HPET_BASE + B06WA_HPET_SIZE <= B06VZ_RCBA_BASE",
            "b06wa_hpet_resource_contract",
        ):
            self.assertIn(fragment, PCI)

        reader = function_text(PCI, "b06wa_read_resources")
        self.assertIn("!b06wa_hpet_ready", reader)
        self.assertLess(
            reader.index("b06vz_read_resources(dev);"), reader.index("mmio_range(")
        )
        self.assertEqual(reader.count("mmio_range("), 1)
        self.assertIn("B06WA_HPET_RESOURCE_INDEX", reader)
        self.assertIn("B06WA_HPET_BASE, B06WA_HPET_SIZE", reader)

        exact = function_text(PCI, "b06wa_require_exact_hpet_resource")
        for fragment in (
            "actual_count != B06WA_DOMAIN_RESOURCE_COUNT",
            "resource->flags != b06wa_hpet_resource_contract.flags",
            "b06vz_domain_resource_bounds_are_exact",
            "other->flags & IORESOURCE_TYPE_MASK",
            "b06wa_hpet_resource_contract.base < other->top",
            "other->base < b06wa_hpet_resource_contract.top",
        ):
            self.assertIn(fragment, exact)
        leaf = function_text(PCI, "b06wa_require_no_hpet_leaf_overlap")
        self.assertIn("IORESOURCE_MEM", leaf)
        self.assertIn("b06wa_hpet_resource_contract.base < leaf_top", leaf)
        self.assertIn("leaf->base < b06wa_hpet_resource_contract.top", leaf)

    def test_order_and_scope_exclude_tables_routing_and_other_writes(self) -> None:
        scan = function_text(PCI, "b06vn_domain_scan_bus")
        self.assertLess(
            scan.index("b06vy_decode_and_mask_ioapic_once();"),
            scan.index("b06wa_decode_and_gate_hpet_once();"),
        )
        self.assertLess(
            scan.index("b06wa_decode_and_gate_hpet_once();"),
            scan.index("b06vn_raw_root_preflight();"),
        )
        assigned = function_text(PCI, "b06vn_resources_assigned")
        self.assertLess(
            assigned.index("b06vz_require_exact_domain_resources();"),
            assigned.index("b06wa_require_exact_hpet_resource();"),
        )
        self.assertLess(
            assigned.index("b06vz_require_no_fixed_leaf_overlap"),
            assigned.index("b06wa_require_no_hpet_leaf_overlap"),
        )

        names = (
            "b06wa_read_hpet_counter",
            "b06wa_hpet_fail",
            "b06wa_decode_and_gate_hpet_once",
            "b06wa_read_resources",
            "b06wa_require_exact_hpet_resource",
            "b06wa_require_no_hpet_leaf_overlap",
        )
        combined = "\n".join(function_text(PCI, name) for name in names)
        for forbidden in (
            "acpi_write",
            "smbios_",
            "setup_ioapic",
            "register_new_ioapic",
            "PIRQA_ROUT",
            "LAPIC_",
            "USB",
            "8042",
        ):
            self.assertNotIn(forbidden, combined)
        self.assertNotIn("HPET_MIN_TICKS", combined)

    def test_builder_is_deterministic_and_enforces_isolation(self) -> None:
        self.assertTrue(WRAPPER.is_file())
        self.assertTrue(os.access(WRAPPER, os.X_OK))
        self.assertIn("b06wa", WRAPPER.read_text())
        for fragment in (
            "b06vv|b06vw|b06vx|b06vy|b06vz|b06wa",
            'build_id="X58PROE-B06WA-ICH10-HPET-DECODE-20260906"',
            "B06WA is BUILD-ONLY",
            "recorded B06VZ hardware PASS",
            "CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES=y",
            "CONFIG_X58_PRO_E_B06WA_HPET_DECODE=y",
            "CONFIG_HAVE_ACPI_TABLES=n",
            "CONFIG_IOAPIC=n",
            "CONFIG_SOUTHBRIDGE_INTEL_COMMON_HPET=n",
            "CONFIG_SOUTHBRIDGE_INTEL_I82801JX=n",
            'cmp -s -- "${first_rom}" "${second_rom}"',
        ):
            self.assertIn(fragment, BUILDER)


if __name__ == "__main__":
    unittest.main()
