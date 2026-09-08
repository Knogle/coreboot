#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
SOURCE = (BOARD / "acpi_tables.c").read_text()
HEADER = (BOARD / "b06wc_acpi.h").read_text()
DSDT = (BOARD / "dsdt.asl").read_text()
KCONFIG = (BOARD / "Kconfig").read_text()


def function_text(source: str, name: str) -> str:
    definition = re.search(
        rf"\b{re.escape(name)}\s*\([^;{{}}]*\)\s*\{{", source, re.DOTALL
    )
    if definition is None:
        raise ValueError(f"no definition for {name}")
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


class X58B06WCAcpiSourceContractTests(unittest.TestCase):
    def test_tables_are_scoped_only_to_b06wc(self) -> None:
        for token in (
            "select HAVE_ACPI_TABLES if X58_PRO_E_B06WC_INTEGRATED_PLATFORM",
            "select ACPI_CUSTOM_MADT if X58_PRO_E_B06WC_INTEGRATED_PLATFORM",
            "select ACPI_CUSTOM_MCFG if X58_PRO_E_B06WC_INTEGRATED_PLATFORM",
        ):
            self.assertIn(token, KCONFIG)
        self.assertIn("CONFIG(NO_ECAM_MMCONF_SUPPORT)", SOURCE)
        self.assertIn("CONFIG(NO_SMM)", SOURCE)
        self.assertIn("!CONFIG(SMP) && CONFIG_MAX_CPUS == 1", SOURCE)

    def test_exact_mcfg_is_runtime_gated(self) -> None:
        for token in (
            "B06WC_ACPI_PCIEXBAR_VALUE\t0xe0000001",
            "B06WC_ACPI_ECAM_BASE\t\t0xe0000000",
            "B06WC_ACPI_ECAM_SIZE\t\t0x10000000",
            "B06WC_ACPI_ECAM_LIMIT\t\t0xefffffff",
            "B06WC_ACPI_ECAM_HOST_ID\t\t0x34058086",
            "B06WC_ACPI_PCI_BUS_START\t0x00",
            "B06WC_ACPI_PCI_BUS_END\t\t0xff",
        ):
            self.assertIn(token, HEADER)
        gate = function_text(SOURCE, "b06wc_acpi_gate_pciexbar")
        self.assertIn("pci_io_read_config32", gate)
        self.assertIn("host_id != B06WC_ACPI_ECAM_HOST_ID", gate)
        fill = function_text(SOURCE, "acpi_fill_mcfg")
        ordered = (
            "b06wc_acpi_gate_pciexbar();",
            "mmconfig->base_address = B06WC_ACPI_ECAM_BASE;",
            "mmconfig->pci_segment_group_number = B06WC_ACPI_PCI_SEGMENT;",
            "mmconfig->start_bus_number = B06WC_ACPI_PCI_BUS_START;",
            "mmconfig->end_bus_number = B06WC_ACPI_PCI_BUS_END;",
        )
        positions = [fill.index(token) for token in ordered]
        self.assertEqual(positions, sorted(positions))

    def test_fadt_is_no_smm_table_only_policy(self) -> None:
        fill = function_text(SOURCE, "acpi_fill_fadt")
        for token in (
            "fadt->sci_int = B06WC_ACPI_SCI_IRQ;",
            "fadt->pm1a_evt_blk = B06WC_ACPI_PMBASE + B06WC_ACPI_PM1_STS;",
            "fadt->pm1a_cnt_blk = B06WC_ACPI_PMBASE + B06WC_ACPI_PM1_CNT;",
            "fadt->pm_tmr_blk = B06WC_ACPI_PMBASE + B06WC_ACPI_PM_TMR;",
            "fadt->pm1_evt_len = 4;",
            "fadt->pm1_cnt_len = 2;",
            "fadt->pm_tmr_len = 4;",
            "fadt->smi_cmd = 0;",
            "fadt->acpi_enable = 0;",
            "fadt->acpi_disable = 0;",
            "fadt->iapc_boot_arch = ACPI_FADT_LEGACY_DEVICES;",
            "ACPI_FADT_WBINVD | ACPI_FADT_C1_SUPPORTED |",
            "ACPI_FADT_POWER_BUTTON | ACPI_FADT_SLEEP_BUTTON |",
            "ACPI_FADT_FIXED_RTC;",
            "fadt->reset_value = 0;",
        ):
            self.assertIn(token, fill)
        for claim in (
            "ACPI_FADT_8042",
            "ACPI_FADT_RESET_REGISTER",
            "ACPI_FADT_S4_RTC_WAKE",
            "ACPI_FADT_C2_MP_SUPPORTED",
        ):
            self.assertNotIn(claim, fill)

    def test_pm_gate_is_read_only_and_requires_all_sources_quiet(self) -> None:
        gate = function_text(SOURCE, "b06wc_acpi_gate_pm")
        for token in (
            "id != B06WC_ACPI_LPC_ID",
            "pmbase != B06WC_ACPI_PMBASE_DECODED",
            "acpi_cntl != B06WC_ACPI_ACPI_CNTL_DECODED",
            "pm1_en != 0",
            "gpe0_en_lo != 0",
            "gpe0_en_hi != 0",
            "smi_en != 0",
            "alt_gp_smi_en != 0",
            "uprwc != 0",
            "gpio_rout != 0",
            "pm1_cnt != B06WC_ACPI_PM1_CNT_TARGET",
        ):
            self.assertIn(token, gate)
        ready = function_text(SOURCE, "b06wc_acpi_require_mode_ready")
        self.assertIn("!b06wc_acpi_mode_ready()", ready)
        self.assertIn("b06wc_acpi_require_mode_ready();", gate)
        self.assertIn(
            "inw(B06WC_ACPI_PMBASE + B06WC_ACPI_ALT_GP_SMI_EN)", gate
        )
        self.assertNotIn(
            "inl(B06WC_ACPI_PMBASE + B06WC_ACPI_ALT_GP_SMI_EN)", gate
        )
        for write in ("outb(", "outw(", "outl(", "write8p(", "write16p(", "write32p("):
            self.assertNotIn(write, gate)

    def test_madt_is_bsp_only_and_sci9_only(self) -> None:
        fill = function_text(SOURCE, "acpi_fill_madt")
        for token in (
            "acpi_create_madt_one_lapic(current, 0, 0)",
            "ioapic->ioapic_id = B06WC_ACPI_IOAPIC_ID;",
            "ioapic->ioapic_addr = B06WC_ACPI_IOAPIC_BASE;",
            "ioapic->gsi_base = B06WC_ACPI_IOAPIC_GSI_BASE;",
            "B06WC_ACPI_SCI_IRQ, B06WC_ACPI_SCI_IRQ",
            "MP_IRQ_TRIGGER_LEVEL | MP_IRQ_POLARITY_HIGH",
            "b06wc_acpi_gate_pm();",
        ):
            self.assertIn(token, fill)
        self.assertNotIn("acpi_create_madt_lapic_nmis", fill)
        self.assertNotIn("MP_BUS_ISA, 0, 2", fill)

    def test_each_runtime_table_callback_is_latch_gated(self) -> None:
        for callback in ("acpi_fill_fadt", "acpi_fill_madt", "acpi_fill_mcfg"):
            self.assertIn(
                "b06wc_acpi_gate_pm();", function_text(SOURCE, callback)
            )

    def test_ioapic_selector_is_restored(self) -> None:
        gate = function_text(SOURCE, "b06wc_acpi_gate_ioapic")
        select_version = gate.index(
            "write32p(B06WC_ACPI_IOAPIC_BASE, B06WC_ACPI_IOAPIC_REG_VERSION);"
        )
        restore_id = gate.index(
            "write32p(B06WC_ACPI_IOAPIC_BASE, B06WC_ACPI_IOAPIC_REG_ID);"
        )
        final_check = gate.index("read32p(B06WC_ACPI_IOAPIC_BASE) !=")
        self.assertLess(select_version, restore_id)
        self.assertLess(restore_id, final_check)

    def test_dsdt_is_minimal_and_reserves_custom_mcfg_window(self) -> None:
        for token in (
            '#include "b06wc_acpi.h"',
            'EisaId ("PNP0A08")',
            'EisaId ("PNP0A03")',
            "Method (_CBA, 0, NotSerialized)",
            'EisaId ("PNP0C02")',
            "Memory32Fixed (ReadWrite, B06WC_ACPI_ECAM_BASE",
            "DWordIO (ResourceProducer",
            "DWordMemory (ResourceProducer",
        ):
            self.assertIn(token, DSDT)
        for forbidden in (
            "_S3",
            "_S4",
            "_S5",
            "_PRW",
            "_WAK",
            "_PTS",
            "_PRT",
            "PNP0303",
            "PNP0F13",
            "PNP0103",
        ):
            self.assertNotIn(forbidden, DSDT)


if __name__ == "__main__":
    unittest.main()
