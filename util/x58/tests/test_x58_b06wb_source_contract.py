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
BASE_TREE = (BOARD / "devicetree_b06wa.cb").read_text()
IMAGE_TREE = (BOARD / "devicetree_b06wb.cb").read_text()
BASE_CONFIG = (ROOT / "configs/x58-pro-e-b06wa.config").read_text()
IMAGE_CONFIG = (ROOT / "configs/x58-pro-e-b06wb.config").read_text()
BUILDER = (ROOT / "scripts/build_x58_b06_sata_successor.sh").read_text()
WRAPPER = ROOT / "scripts/build_x58_b06wb.sh"

SYMBOL = "CONFIG_X58_PRO_E_B06WB_TCO_HALT"
ID = "X58PROE-B06WB-ICH10-TCO-HALT-20260906"
BASE_ID = "X58PROE-B06WA-ICH10-HPET-DECODE-20260906"


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


class B06WBTcoHaltSourceContractTests(unittest.TestCase):
    def test_variant_is_separate_build_only_b06wa_successor(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06WB_TCO_HALT",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        compact = " ".join(option.split())
        for fragment in (
            "depends on X58_PRO_E_B06WA_HPET_DECODE",
            "default n",
            "complete TCO1_CNT 0000 or already halted 0800",
            "one 16-bit masked read-modify-write",
            "set only TCO_TMR_HLT bit 11",
            "require complete-word 0800 readback",
            "Log GCS, TCO_RLD, TCO1_STS, TCO2_STS, TCO1_CNT, TCO2_CNT and TCO_TMR",
            "does not write GCS.NR or TCO_LOCK",
            "PM/SMI/GPE, PIC/ELCR, IOAPIC, HPET, USB, SATA or ACPI policy",
            "B06VY, B06VZ and B06WA each have a recorded hardware PASS",
        ):
            self.assertIn(fragment, compact)
        self.assertNotIn("select SOUTHBRIDGE", option)

    def test_identity_topology_and_config_are_one_delta(self) -> None:
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON, PCI):
            self.assertLess(source.index(ID), source.index(BASE_ID))
        self.assertLess(MAINBOARD.index(SYMBOL), MAINBOARD.index("B06WA_HPET_DECODE"))
        self.assertIn('default "devicetree_b06wb.cb"', KCONFIG)
        self.assertEqual(IMAGE_TREE.count("device pci"), 13)
        self.assertEqual(
            re.sub(r"# B06W[AB][^\n]*", "", IMAGE_TREE),
            re.sub(r"# B06W[AB][^\n]*", "", BASE_TREE),
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
            image["CONFIG_X58_PRO_E_B06WA_HPET_DECODE"],
            "CONFIG_X58_PRO_E_B06WA_HPET_DECODE=y",
        )
        self.assertEqual(
            image["CONFIG_MAINBOARD_PART_NUMBER"],
            base["CONFIG_MAINBOARD_PART_NUMBER"],
        )
        self.assertEqual(image["CONFIG_LOCALVERSION"], base["CONFIG_LOCALVERSION"])

    def test_exact_decode_and_control_gates_precede_the_only_write(self) -> None:
        definitions = {
            "B06WB_PMBASE_REG": "0x40u",
            "B06WB_ACPI_CNTL_REG": "0x44u",
            "B06WB_PMBASE_ENABLED": "0x00000501u",
            "B06WB_ACPI_DECODE_ENABLED": "0x80u",
            "B06WB_TCO_BASE": "0x0560u",
            "B06WB_TCO_RLD": "0x00u",
            "B06WB_TCO1_STS": "0x04u",
            "B06WB_TCO2_STS": "0x06u",
            "B06WB_TCO1_CNT": "0x08u",
            "B06WB_TCO2_CNT": "0x0au",
            "B06WB_TCO_TMR": "0x12u",
            "B06WB_TCO1_CNT_HLT": "BIT(11)",
            "B06WB_TCO1_CNT_LOCK": "BIT(12)",
            "B06WB_TCO1_CNT_PRE": "0x0000u",
            "B06WB_TCO1_CNT_TARGET": "0x0800u",
        }
        for name, value in definitions.items():
            start = PCI.index(f"#define {name}")
            self.assertIn(value, PCI[start : start + 180])
        self.assertIn("B06WB_TCO1_CNT_TARGET == B06WB_TCO1_CNT_HLT", PCI)
        self.assertIn("!(B06WB_TCO1_CNT_TARGET & B06WB_TCO1_CNT_LOCK)", PCI)

        init = function_text(PCI, "b06wb_halt_tco_once")
        write = "outw(target, B06WB_TCO_BASE + B06WB_TCO1_CNT);"
        self.assertEqual(init.count("outw("), 1)
        self.assertIn(write, init)
        for gate in (
            "b06wb_tco_attempted",
            "b06vqi_baseline_attempted || b06vu_ahci_route_attempted",
            "lpc_id != B06VY_LPC_ID || rcba != B06VY_RCBA_ENABLED",
            "pmbase != B06WB_PMBASE_ENABLED",
            "acpi_cntl != B06WB_ACPI_DECODE_ENABLED",
            "before.tco1_cnt != B06WB_TCO1_CNT_PRE",
            "before.tco1_cnt != B06WB_TCO1_CNT_TARGET",
            "target != B06WB_TCO1_CNT_TARGET",
        ):
            self.assertIn(gate, init)
            self.assertLess(init.index(gate), init.index(write))
        self.assertIn("before.tco1_cnt & (uint16_t)~B06WB_TCO1_CNT_HLT", init)
        self.assertIn("|\n\t\t\tB06WB_TCO1_CNT_HLT", init)

    def test_complete_telemetry_and_read_only_state_are_preserved(self) -> None:
        snapshot = function_text(PCI, "b06wb_read_tco_snapshot")
        for register in (
            "GCS",
            "B06WB_TCO_RLD",
            "B06WB_TCO1_STS",
            "B06WB_TCO2_STS",
            "B06WB_TCO1_CNT",
            "B06WB_TCO2_CNT",
            "B06WB_TCO_TMR",
        ):
            self.assertIn(register, snapshot)
        self.assertEqual(snapshot.count("inw("), 6)

        log = function_text(PCI, "b06wb_log_tco_snapshot")
        for label in (
            "GCS=%08x",
            "TCO_RLD=%04x",
            "TCO1_STS=%04x",
            "TCO2_STS=%04x",
            "TCO1_CNT=%04x",
            "TCO2_CNT=%04x",
            "TCO_TMR=%04x",
        ):
            self.assertIn(label, log)

        init = function_text(PCI, "b06wb_halt_tco_once")
        for fragment in (
            'b06wb_log_tco_snapshot("PRE", &before, false)',
            'b06wb_log_tco_snapshot("POST", &after, control_write)',
            "after.tco1_cnt != B06WB_TCO1_CNT_TARGET",
            "after.tco1_sts != before.tco1_sts",
            "after.tco2_sts != before.tco2_sts",
            "after.gcs != before.gcs",
            "after.tco2_cnt != before.tco2_cnt",
            "after.tco_tmr != before.tco_tmr",
            'b06wb_tco_fail("FINAL_STATE_RECHECK")',
            "STATUS_PRESERVED=1",
            "GCS_WRITE=0",
            "STATUS_WRITE=0",
            "RELOAD_WRITE=0",
            "TIMER_WRITE=0",
            "TCO_LOCK_WRITE=0",
        ):
            self.assertIn(fragment, init)
        self.assertLess(
            init.index('b06wb_tco_fail("FINAL_STATE_RECHECK")'),
            init.index("b06wb_tco_ready = true"),
        )

    def test_tco_stage_is_earliest_and_resources_are_unchanged(self) -> None:
        scan = function_text(PCI, "b06vn_domain_scan_bus")
        for later in (
            "b06vqi_program_baseline_once();",
            "b06vu_program_ahci_route_once();",
            "b06vy_decode_and_mask_ioapic_once();",
            "b06wa_decode_and_gate_hpet_once();",
            "b06vn_raw_root_preflight();",
        ):
            self.assertLess(scan.index("b06wb_halt_tco_once();"), scan.index(later))
        self.assertLess(
            scan.index("b06vn_require_platform_state"),
            scan.index("b06wb_halt_tco_once();"),
        )
        self.assertLess(
            scan.index("b06vn_require_static_topology"),
            scan.index("b06wb_halt_tco_once();"),
        )

        reader = function_text(PCI, "b06wb_read_resources")
        self.assertIn("!b06wb_tco_ready", reader)
        self.assertEqual(reader.count("b06wa_read_resources(dev);"), 1)
        self.assertNotIn("mmio_range", reader)
        self.assertNotIn("io_range", reader)
        self.assertIn("resources 0..15 unchanged", reader)
        ops = between(PCI, "static struct device_operations b06vn_domain_ops", "static struct device_operations b06vn_cpu_cluster_ops")
        self.assertLess(ops.index(SYMBOL), ops.index("B06WA_HPET_DECODE"))
        self.assertIn(".read_resources = b06wb_read_resources", ops)

    def test_scope_excludes_broad_watchdog_and_unrelated_hardware(self) -> None:
        names = (
            "b06wb_read_tco_snapshot",
            "b06wb_log_tco_snapshot",
            "b06wb_tco_fail",
            "b06wb_halt_tco_once",
            "b06wb_read_resources",
        )
        combined = "\n".join(function_text(PCI, name) for name in names)
        self.assertEqual(combined.count("outw("), 1)
        self.assertNotRegex(
            combined,
            r"write(?:8|16|32|64)p|pci_(?:io_)?write_config|out[bl]|wrmsr|"
            r"watchdog_off|setup_i8259|setup_ioapic|b06vy_ioapic_write_exact\s*\(",
        )
        for forbidden in (
            "TCO1_STS_TIMEOUT",
            "TCO2_STS_SECOND_TO",
            "GCS.NR",
            "TCO_LOCK =",
            "PM1_",
            "SMI_EN",
            "GPE0_",
            "ELCR",
            "PIRQA_ROUT",
            "HPET_",
            "USB",
            "SATA",
            "acpi_write",
        ):
            self.assertNotIn(forbidden, combined)

    def test_builder_is_deterministic_and_enforces_isolation(self) -> None:
        self.assertTrue(WRAPPER.is_file())
        self.assertTrue(os.access(WRAPPER, os.X_OK))
        self.assertIn("b06wb", WRAPPER.read_text())
        for fragment in (
            "b06vv|b06vw|b06vx|b06vy|b06vz|b06wa|b06wb",
            'build_id="X58PROE-B06WB-ICH10-TCO-HALT-20260906"',
            "B06WB is BUILD-ONLY",
            "recorded B06VY, B06VZ and B06WA hardware PASSes",
            "CONFIG_X58_PRO_E_B06WA_HPET_DECODE=y",
            "CONFIG_X58_PRO_E_B06WB_TCO_HALT=y",
            "CONFIG_HAVE_ACPI_TABLES=n",
            "CONFIG_IOAPIC=n",
            "CONFIG_SOUTHBRIDGE_INTEL_COMMON_HPET=n",
            "CONFIG_SOUTHBRIDGE_INTEL_COMMON_WATCHDOG=n",
            "CONFIG_USE_WATCHDOG_ON_BOOT=n",
            "CONFIG_SOUTHBRIDGE_INTEL_I82801JX=n",
            'cmp -s -- "${first_rom}" "${second_rom}"',
            "python3 -m unittest discover -s tests -v",
        ):
            self.assertIn(fragment, BUILDER)
        self.assertEqual(BUILDER.count("'CONFIG_X58_PRO_E_B06WB_TCO_HALT=n'"), 6)


if __name__ == "__main__":
    unittest.main()
