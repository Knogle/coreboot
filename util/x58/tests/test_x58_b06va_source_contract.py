#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
KCONFIG = (BOARD / "Kconfig").read_text()
ROMSTAGE = (BOARD / "romstage.c").read_text()
VENDOR_INIT = (BOARD / "vendor_init.c").read_text()
VENDOR_HEADER = (BOARD / "vendor_init.h").read_text()


def between(source: str, first: str, last: str) -> str:
    begin = source.index(first)
    end = source.index(last, begin)
    return source[begin:end]


class B06VASourceContractTests(unittest.TestCase):
    def test_config_is_explicit_default_off_and_requires_b06v9(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VA_HIGH_QPI_A0_SET",
            "config X58_PRO_E_BRINGUP_STAGE",
        )

        self.assertIn(
            "depends on X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS", option
        )
        self.assertIn("default n", option)
        self.assertIn("never invokes MINIT", option)
        self.assertIn(
            "never\n\t  hands off to postcar, ramstage, or a payload", option
        )

    def test_a0_predicate_is_the_exact_two_value_set(self) -> None:
        predicate = between(
            VENDOR_HEADER,
            "static inline bool x58_vendor_high_qpi_cpu_a0_exact(",
            "enum x58_vendor_status",
        )
        normalized = " ".join(predicate.split())
        self.assertIn(
            "return value == 0x00017000u || value == 0x00017600u;",
            normalized,
        )

        literals = re.findall(r"0x[0-9a-fA-F]+u?", predicate)
        accepted = {int(literal.rstrip("uU"), 16) for literal in literals}
        self.assertEqual(accepted, {0x00017000, 0x00017600})
        self.assertNotIn(0x00017200, accepted)
        self.assertNotIn(0x00017400, accepted)
        self.assertNotIn("&", predicate)
        self.assertNotIn("<", predicate)
        self.assertNotIn(">", predicate)

    def test_both_high_qpi_gates_share_the_new_predicate(self) -> None:
        vendor_gate = between(
            VENDOR_INIT,
            "static bool b06v8_high_qpi_platform_exact(void)",
            "static bool pe32_header_valid",
        )
        tuple_gate = between(
            ROMSTAGE,
            "static bool b06v8_high_qpi_tuple_exact(",
            "static bool b06v8_print_csi_state(",
        )

        self.assertEqual(
            vendor_gate.count("x58_vendor_high_qpi_cpu_a0_exact("), 1
        )
        self.assertEqual(
            tuple_gate.count("x58_vendor_high_qpi_cpu_a0_exact("), 1
        )
        for gate in (vendor_gate, tuple_gate):
            self.assertIn(
                "#if CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET", gate
            )
            self.assertIn("#else", gate)
            self.assertIn("#endif", gate)

    def test_b06v9_single_value_gates_remain_exact_when_b06va_is_off(self) -> None:
        vendor_gate = between(
            VENDOR_INIT,
            "static bool b06v8_high_qpi_platform_exact(void)",
            "static bool pe32_header_valid",
        )
        tuple_gate = between(
            ROMSTAGE,
            "static bool b06v8_high_qpi_tuple_exact(",
            "static bool b06v8_print_csi_state(",
        )
        vendor_switch = between(
            vendor_gate,
            "#if CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET",
            "#endif",
        )
        tuple_switch = between(
            tuple_gate,
            "#if CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET",
            "#endif",
        )

        self.assertIn(
            "pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0xa0) ==\n"
            "\t\t\t0x00017600u &&",
            vendor_switch,
        )
        self.assertIn("tuple->cpu_a0 == 0x00017600 &&", tuple_switch)
        self.assertNotIn("0x00017000", vendor_switch)
        self.assertNotIn("0x00017000", tuple_switch)
        self.assertNotIn("0x00017200", vendor_switch + tuple_switch)
        self.assertNotIn("0x00017400", vendor_switch + tuple_switch)

    def test_selector_and_post13_precede_csi_arm_and_call(self) -> None:
        probe = between(
            ROMSTAGE,
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
            "static enum b06v6_auto_result __maybe_unused b06v6_auto_handoff(",
        )
        pass3_preselection = between(
            probe,
            "if (phase == B06V8_PHASE_PASS3) {",
            "if (!b06j_run_spd(state))",
        )

        selector = probe.index("x58_vendor_b06v8_select_high_qpi_csi(")
        selector_result_gate = probe.index(
            "if (call_status != X58_VENDOR_OK)", selector
        )
        post13 = probe.index(
            "outb(POST_B06VA_HIGH_PROFILE_READY, CONFIG_POST_IO_PORT)",
            selector_result_gate,
        )
        arm = probe.index("x58_vendor_arm_csi_wrapper(", post13)
        call = probe.index("x58_vendor_call_csi_wrapper(", arm)

        self.assertEqual(
            probe.count("x58_vendor_b06v8_select_high_qpi_csi("), 1
        )
        self.assertEqual(
            probe.count(
                "outb(POST_B06VA_HIGH_PROFILE_READY, CONFIG_POST_IO_PORT)"
            ),
            1,
        )
        self.assertLess(selector, selector_result_gate)
        self.assertLess(selector_result_gate, post13)
        self.assertLess(post13, arm)
        self.assertLess(arm, call)
        self.assertIn(
            "#if CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET\n"
            "\t\toutb(POST_B06VA_HIGH_PROFILE_READY, CONFIG_POST_IO_PORT);\n"
            "#endif",
            pass3_preselection,
        )
        self.assertIn("POST_B06VA_HIGH_PROFILE_READY\t0x13", ROMSTAGE)

    def test_automatic_probe_remains_minit_free_and_terminal(self) -> None:
        probe = between(
            ROMSTAGE,
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
            "static enum b06v6_auto_result __maybe_unused b06v6_auto_handoff(",
        )

        self.assertNotIn("x58_vendor_arm_minit", probe)
        self.assertNotIn("x58_vendor_call_minit", probe)
        self.assertNotIn("x58_vendor_accept_csi_result", probe)
        self.assertNotIn("x58_b06v6_handoff_record_success", probe)
        self.assertIn(
            "[QPI] PASS3_TERMINAL; no CSI acceptance, MINIT, caller reset, "
            "postcar or ramstage",
            probe,
        )
        terminal_message = probe.index("[QPI] PASS3_TERMINAL")
        terminal_post = probe.index(
            "outb(POST_B06V8_TERMINAL, CONFIG_POST_IO_PORT)",
            terminal_message,
        )
        terminal_fallback = probe.index(
            'return b06v6_fallback("B06V8_PASS3_TERMINAL")',
            terminal_post,
        )
        self.assertLess(terminal_message, terminal_post)
        self.assertLess(terminal_post, terminal_fallback)


if __name__ == "__main__":
    unittest.main()
