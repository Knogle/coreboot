#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
PCI = (COREBOOT / "src/mainboard/msi/x58_pro_e/b06vn_pci.c").read_text()
I8259 = (COREBOOT / "src/drivers/pc80/pc/i8259.c").read_text()


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


class B06WCPicElcrSourceContractTests(unittest.TestCase):
    def test_exact_admitted_and_target_tuples(self) -> None:
        for token in (
            "B06WC_PIC_MASTER_MASK_RESET\t0x00u",
            "B06WC_PIC_SLAVE_MASK_RESET\t0x00u",
            "B06WC_ELCR1_RESET\t\t0x00u",
            "B06WC_ELCR2_RESET\t\t0x00u",
            "B06WC_PIC_MASTER_MASK_TARGET\t0xfbu",
            "B06WC_PIC_SLAVE_MASK_TARGET\t0xffu",
            "B06WC_ELCR1_TARGET\t\t0x00u",
            "B06WC_ELCR2_TARGET\t\tBIT(IRQ_9 - 8)",
        ):
            self.assertIn(token, PCI)

    def test_standard_i8259_semantics_match_target(self) -> None:
        setup = function_text(I8259, "setup_i8259")
        writes = (
            "outb(ICW_SELECT|IC4, MASTER_PIC_ICW1);",
            "outb(ICW_SELECT|IC4, SLAVE_PIC_ICW1);",
            "outb(INT_VECTOR_MASTER | IRQ0, MASTER_PIC_ICW2);",
            "outb(INT_VECTOR_SLAVE  | IRQ8, SLAVE_PIC_ICW2);",
            "outb(CASCADED_PIC, MASTER_PIC_ICW3);",
            "outb(SLAVE_ID, SLAVE_PIC_ICW3);",
            "outb(MICROPROCESSOR_MODE, MASTER_PIC_ICW2);",
            "outb(MICROPROCESSOR_MODE, SLAVE_PIC_ICW2);",
            "outb(ALL_IRQS, SLAVE_PIC_OCW1);",
            "outb(ALL_IRQS & ~IRQ2, MASTER_PIC_OCW1);",
        )
        positions = [setup.index(write) for write in writes]
        self.assertEqual(positions, sorted(positions))

    def test_stage_is_single_shot_ordered_and_fail_closed(self) -> None:
        init = function_text(PCI, "b06wc_initialize_pic_once")
        for token in (
            "if (b06wc_pic_attempted)",
            'b06wc_pic_fail("SECOND_ATTEMPT")',
            "b06wc_pic_attempted = true;",
            "!b06wb_tco_ready",
            "b06vqi_baseline_attempted",
            "b06vu_ahci_route_attempted",
            "b06vy_ioapic_attempted",
            "b06wa_hpet_attempted",
            'b06wc_pic_fail("STAGE_ORDER")',
            "eflags_before & X86_EFLAGS_IF",
            'b06wc_pic_fail("CPU_INTERRUPTS_ENABLED")',
            'b06wc_pic_fail("PRE_TUPLE_OUTSIDE_ALLOWLIST")',
            'b06wc_pic_fail("TARGET_READBACK")',
            'b06wc_pic_fail("FINAL_STATE_RECHECK")',
        ):
            self.assertIn(token, init)
        self.assertIn("die_with_post_code(POST_B06WC_PIC_FAIL", PCI)

    def test_mutation_and_readback_order_is_exact(self) -> None:
        init = function_text(PCI, "b06wc_initialize_pic_once")
        ordered = (
            "post_code(POST_B06WC_PIC_BEGIN);",
            "setup_i8259();",
            "i8259_configure_irq_trigger(IRQ_9, IRQ_LEVEL_TRIGGERED);",
            "master_after = inb(MASTER_PIC_OCW1);",
            "slave_after = inb(SLAVE_PIC_OCW1);",
            "elcr1_after = inb(ELCR1);",
            "elcr2_after = inb(ELCR2);",
            "b06wc_pic_ready = true;",
            "post_code(POST_B06WC_PIC_READY);",
        )
        positions = [init.index(item) for item in ordered]
        self.assertEqual(positions, sorted(positions))
        self.assertNotIn("sti(", init)
        self.assertNotIn("asm(\"sti", init)

    def test_pic_precedes_later_platform_writes_and_usb_admission(self) -> None:
        scan = function_text(PCI, "b06vn_domain_scan_bus")
        ordered_scan = (
            "b06wb_halt_tco_once();",
            "b06wc_initialize_pic_once();",
            "b06vqi_program_baseline_once();",
            "b06vu_program_ahci_route_once();",
            "b06vy_decode_and_mask_ioapic_once();",
            "b06wa_decode_and_gate_hpet_once();",
            "b06vn_raw_root_preflight();",
        )
        positions = [scan.index(item) for item in ordered_scan]
        self.assertEqual(positions, sorted(positions))

        enabled = function_text(PCI, "b06vn_resources_enabled")
        self.assertLess(enabled.index("if (!b06wc_pic_ready)"),
                        enabled.index("b06wc_usb_admit();"))

    def test_final_tuple_is_reread_before_ready(self) -> None:
        init = function_text(PCI, "b06wc_initialize_pic_once")
        final = init.index("/* Re-read every readable byte")
        ready = init.index("b06wc_pic_ready = true;")
        self.assertLess(final, ready)
        for expression in (
            "inb(MASTER_PIC_OCW1) != B06WC_PIC_MASTER_MASK_TARGET",
            "inb(SLAVE_PIC_OCW1) != B06WC_PIC_SLAVE_MASK_TARGET",
            "inb(ELCR1) != B06WC_ELCR1_TARGET",
            "inb(ELCR2) != B06WC_ELCR2_TARGET",
        ):
            self.assertIn(expression, init[final:ready])


if __name__ == "__main__":
    unittest.main()
