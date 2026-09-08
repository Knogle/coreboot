#!/usr/bin/env python3
"""Source contract for the B06WC no-SMM, quiet ACPI-mode transition."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
SOURCE = (COREBOOT / "src/mainboard/msi/x58_pro_e/b06vn_pci.c").read_text()
HEADER = (COREBOOT / "src/mainboard/msi/x58_pro_e/b06vn_pci.h").read_text()


def function_text(source: str, name: str) -> str:
    """Return one C function, using brace depth rather than a loose regex."""
    match = re.search(rf"\b{name}\s*\([^;]*?\)\s*\{{", source, re.S)
    if match is None:
        raise AssertionError(f"function {name} not found")

    start = match.start()
    brace = source.find("{", match.start())
    depth = 0
    for offset in range(brace, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[start : offset + 1]
    raise AssertionError(f"unterminated function {name}")


class B06WCAcpiModeTransitionSourceContractTests(unittest.TestCase):
    def test_exact_state_and_post_contract(self) -> None:
        for fragment in (
            "#define B06WC_PM1_CNT_DWORD_PRE\t\t0x00000000u",
            "#define B06WC_PM1_CNT_DWORD_TARGET\t0x00000001u",
            "#define B06WC_PM1_CNT_TARGET\t\tSCI_EN",
            "#define POST_B06WC_ACPI_BEGIN\t0x7f",
            "#define POST_B06WC_ACPI_STATUS\t0x80",
            "#define POST_B06WC_ACPI_READY\t0x81",
            "#define POST_B06WC_ACPI_FAIL\t0x82",
        ):
            self.assertIn(fragment, SOURCE)

    def test_preconditions_are_fail_closed_before_first_mutation(self) -> None:
        body = function_text(SOURCE, "b06wc_enter_quiet_acpi_mode_once")
        first_write = body.index("outw(PRBTNOR_STS")
        for fragment in (
            'b06wc_acpi_mode_fail("SECOND_ATTEMPT")',
            'b06wc_acpi_mode_fail("STAGE_ORDER")',
            'b06wc_acpi_mode_fail("LPC_PM_DECODE_IDENTITY")',
            'b06wc_acpi_mode_fail("CPU_INTERRUPTS_ENABLED")',
            'b06wc_acpi_mode_fail("PIC_ELCR_TARGET_CHANGED")',
            'b06wc_acpi_mode_fail("SCI_DELIVERY_NOT_MASKED")',
            'b06wc_acpi_mode_fail("EVENT_ENABLE_OR_GPIO_ROUTE_ACTIVE")',
            'b06wc_acpi_mode_fail("PM1_CNT_OUTSIDE_EXACT_ALLOWLIST")',
            "post_code(POST_B06WC_ACPI_BEGIN);",
        ):
            self.assertLess(body.index(fragment), first_write, fragment)

        self.assertIn("eflags & X86_EFLAGS_IF", body[:first_write])
        self.assertIn("pmbase != B06WB_PMBASE_ENABLED", body[:first_write])
        self.assertIn("acpi_cntl != B06WB_ACPI_DECODE_ENABLED", body[:first_write])

    def test_only_two_direct_pm_writes_and_w1c_is_narrow(self) -> None:
        body = function_text(SOURCE, "b06wc_enter_quiet_acpi_mode_once")

        self.assertEqual(body.count("outw("), 2)
        self.assertIn(
            "outw(PRBTNOR_STS, DEFAULT_PMBASE + PM1_STS);", body
        )
        self.assertNotIn("outw(pm1_sts", body)
        self.assertIn("outw(target, DEFAULT_PMBASE + PM1_CNT);", body)
        for forbidden in ("outb(", "outl(", "pci_io_write_config", "write32p("):
            self.assertNotIn(forbidden, body)

        status_write = body.index("outw(PRBTNOR_STS")
        status_readback = body.index('"PRBTNOR_STS_DID_NOT_CLEAR"')
        control_write = body.index("outw(target, DEFAULT_PMBASE + PM1_CNT)")
        self.assertLess(status_write, status_readback)
        self.assertLess(status_readback, control_write)

    def test_sci_write_preserves_reserved_upper_word_and_requires_exact_value(self) -> None:
        body = function_text(SOURCE, "b06wc_enter_quiet_acpi_mode_once")
        for fragment in (
            "pm1_cnt = inl(DEFAULT_PMBASE + PM1_CNT);",
            "pm1_cnt == B06WC_PM1_CNT_DWORD_PRE",
            "target != B06WC_PM1_CNT_TARGET",
            "outw(target, DEFAULT_PMBASE + PM1_CNT);",
            "inl(DEFAULT_PMBASE + PM1_CNT) != B06WC_PM1_CNT_DWORD_TARGET",
            'b06wc_acpi_mode_fail("SCI_EN_EXACT_READBACK")',
        ):
            self.assertIn(fragment, body)
        self.assertNotIn("outl(target, DEFAULT_PMBASE + PM1_CNT)", body)

    def test_all_event_enables_and_gpio_routes_are_zero(self) -> None:
        body = function_text(SOURCE, "b06wc_acpi_enables_are_quiescent")
        for fragment in (
            "inw(DEFAULT_PMBASE + PM1_EN) == 0",
            "inl(DEFAULT_PMBASE + GPE0_EN) == 0",
            "inl(DEFAULT_PMBASE + GPE0_EN + 4) == 0",
            "inl(DEFAULT_PMBASE + SMI_EN) == 0",
            "inw(DEFAULT_PMBASE + ALT_GP_SMI_EN) == 0",
            "inw(DEFAULT_PMBASE + B06WC_UPRWC) == 0",
            "pci_io_read_config32(B06VY_LPC_DEV, D31F0_GPIO_ROUT) == 0",
        ):
            self.assertIn(fragment, body)

    def test_both_irq9_delivery_paths_stay_masked(self) -> None:
        body = function_text(SOURCE, "b06wc_acpi_irq9_is_masked")
        for fragment in (
            "inb(SLAVE_PIC_OCW1) & BIT(IRQ_9 - 8)",
            "b06vy_ioapic_read(B06WC_IOAPIC_SCI_LOW_REG, &low)",
            "b06vy_ioapic_read(B06WC_IOAPIC_SCI_HIGH_REG, &high)",
            "low == B06VY_IOAPIC_LOW_TARGET",
            "high == B06VY_IOAPIC_HIGH_TARGET",
            "b06vy_ioapic_select(B06VY_IOAPIC_ID_REG)",
        ):
            self.assertIn(fragment, body)

        # IOREGSEL is an address latch.  Restore it even if either indirect
        # read fails, so the fail-closed path does not leave a surprising
        # selector behind.
        self.assertIn("readable = b06vy_ioapic_read", body)
        self.assertIn("restored = b06vy_ioapic_select(B06VY_IOAPIC_ID_REG);", body)
        self.assertLess(
            body.index("readable = b06vy_ioapic_read"),
            body.index("restored = b06vy_ioapic_select"),
        )
        self.assertLess(
            body.index("restored = b06vy_ioapic_select"),
            body.index("return readable && restored"),
        )

        transition = function_text(SOURCE, "b06wc_enter_quiet_acpi_mode_once")
        first_control_write = transition.index(
            "outw(target, DEFAULT_PMBASE + PM1_CNT)"
        )
        self.assertIn("!b06wc_acpi_irq9_is_masked()", transition[:first_control_write])
        self.assertIn("!b06wc_acpi_irq9_is_masked()", transition[first_control_write:])

    def test_ready_is_published_only_after_final_checks(self) -> None:
        body = function_text(SOURCE, "b06wc_enter_quiet_acpi_mode_once")
        ready = body.index("b06wc_acpi_mode_is_ready = true;")
        for fragment in (
            'b06wc_acpi_mode_fail("FINAL_QUIESCENT_STATE_RECHECK")',
            'b06wc_acpi_mode_fail("FINAL_CPU_INTERRUPTS_ENABLED")',
            'b06wc_acpi_mode_fail("FINAL_DECODE_IDENTITY_RECHECK")',
        ):
            self.assertLess(body.index(fragment), ready)
        self.assertLess(ready, body.index("post_code(POST_B06WC_ACPI_READY);"))

        accessor = function_text(SOURCE, "b06wc_acpi_mode_ready")
        self.assertIn("return b06wc_acpi_mode_is_ready;", accessor)
        self.assertIn("bool b06wc_acpi_mode_ready(void);", HEADER)

    def test_transition_runs_after_ioapic_and_hpet_before_pci_scan(self) -> None:
        scan = function_text(SOURCE, "b06vn_domain_scan_bus")
        ioapic = scan.index("b06vy_decode_and_mask_ioapic_once();")
        hpet = scan.index("b06wa_decode_and_gate_hpet_once();")
        acpi = scan.index("b06wc_enter_quiet_acpi_mode_once();")
        preflight = scan.index("b06vn_raw_root_preflight();")
        self.assertLess(ioapic, hpet)
        self.assertLess(hpet, acpi)
        self.assertLess(acpi, preflight)

    def test_no_interrupt_enable_or_acpi_apm_command(self) -> None:
        body = function_text(SOURCE, "b06wc_enter_quiet_acpi_mode_once")
        for forbidden in ("sti", "enable_interrupts", "APM_CNT", "SMI_CMD"):
            self.assertNotIn(forbidden, body)


if __name__ == "__main__":
    unittest.main()
