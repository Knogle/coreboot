#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
SOURCE = (BOARD / "b06wc_usb_admit.c").read_text()
HEADER = (BOARD / "b06wc_usb_admit.h").read_text()


class B06WCUsbAdmitSourceContractTests(unittest.TestCase):
    def test_is_an_isolated_public_stage(self) -> None:
        self.assertIn("void b06wc_usb_admit(void);", HEADER)
        self.assertIn("void b06wc_usb_admit(void)", SOURCE)
        self.assertIn('"B06WC-USBADMIT1"', SOURCE)
        self.assertIn("POST_B06WC_BEGIN\t0x83u", SOURCE)
        self.assertIn("POST_B06WC_READY\t0x84u", SOURCE)
        self.assertIn("POST_B06WC_FAIL\t\t0x85u", SOURCE)

    def test_has_no_usb_gpio_portsc_or_oc_mutation_primitive(self) -> None:
        forbidden = (
            r"\bpci_(?:io_)?write_config(?:8|16|32)\b",
            r"\bwrite(?:8|16|32)p?\b",
            r"\bout[bwl]\b",
            r"\b(?:set|clear|clrset)bits(?:8|16|32)\b",
            r"\bi82801jx_ehci_init\b",
        )
        for pattern in forbidden:
            self.assertIsNone(re.search(pattern, SOURCE), pattern)
        self.assertIn("READ_ONLY=1", SOURCE)
        self.assertIn("MUTATIONS=0", SOURCE)

    def test_structural_contract_is_fail_closed(self) -> None:
        for token in (
            "lpc_id != B06WC_LPC_ID",
            "rcba != (CONFIG_FIXED_RCBA_MMIO_BASE | 1u)",
            "pmbase != B06WC_PMBASE_ENABLED",
            "gpiobase != B06WC_GPIOBASE_ENABLED",
            "fd & B06WC_USB_FD_DISABLE_MASK",
            "cg & B06WC_USB_CG_DISABLE",
            "ppo & B06WC_USB_PPO_MASK",
            "map & B06WC_USB_MAP_MODE",
            "gpio1 & B06WC_USB_GPIO1_OC_MASK",
            "I82801JX_EHCI_FCREG_REQUIRED_MASK",
            "B06WC_EHCI_HCSPARAMS",
            'b06wc_fail("EHCI-BAR"',
            'b06wc_fail("UHCI-BAR"',
            "die_with_post_code(POST_B06WC_FAIL",
        ):
            self.assertIn(token, SOURCE)

    def test_all_eight_expected_functions_are_identified(self) -> None:
        ids = (
            "0x3a3c8086u", "0x3a3a8086u",
            "0x3a378086u", "0x3a388086u", "0x3a398086u",
            "0x3a348086u", "0x3a358086u", "0x3a368086u",
        )
        for expected_id in ids:
            self.assertEqual(SOURCE.count(expected_id), 1)
        self.assertIn("B06WC_EHCI_CLASS\t0x0c0320u", SOURCE)
        self.assertIn("B06WC_UHCI_CLASS\t0x0c0300u", SOURCE)

    def test_port_electrical_bits_are_classification_only(self) -> None:
        for token in (
            "B06WC_EHCI_PORT_CCS", "B06WC_EHCI_PORT_PE",
            "B06WC_EHCI_PORT_OCA", "B06WC_EHCI_PORT_OCC",
            "B06WC_UHCI_PORT_CCS", "B06WC_UHCI_PORT_PE",
            "B06WC_UHCI_PORT_OCA", "B06WC_UHCI_PORT_OCC",
            "CLASS=%s", '"CONNECT_OC"', '"CONNECT"', '"OC_NO_CONNECT"',
            '"IDLE_NO_CONNECT"',
        ):
            self.assertIn(token, SOURCE)
        electrical = SOURCE[SOURCE.index("/* PORTSC OCA/OCC/CCS/PE") :]
        self.assertNotIn("b06wc_fail(", electrical)
        self.assertNotIn("die_with_post_code", electrical)

    def test_decode_gates_precede_dependent_accesses(self) -> None:
        gate = SOURCE.index("if (gpiobase != B06WC_GPIOBASE_ENABLED")
        rcba_access = SOURCE.index(
            "fd = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_FD)"
        )
        gpio_access = SOURCE.index("gpio1 = inl(DEFAULT_GPIOBASE")
        self.assertLess(gate, rcba_access)
        self.assertLess(gate, gpio_access)
        ehci_bar_gate = SOURCE.index('b06wc_fail("EHCI-BAR"')
        ehci_mmio = SOURCE.index("if (read8p(base)")
        self.assertLess(ehci_bar_gate, ehci_mmio)


if __name__ == "__main__":
    unittest.main()
