#!/usr/bin/env python3

import unittest
from pathlib import Path
from x58_test_paths import COREBOOT_ROOT as COREBOOT


ROOT = Path(__file__).resolve().parents[1]
ROMSTAGE = (COREBOOT / "src/mainboard/msi/x58_pro_e/romstage.c").read_text()
VENDOR_INIT = (COREBOOT / "src/mainboard/msi/x58_pro_e/vendor_init.c").read_text()


def between(source: str, first: str, last: str) -> str:
    begin = source.index(first)
    end = source.index(last, begin)
    return source[begin:end]


class B06V8SourceContractTests(unittest.TestCase):
    def test_board_unique_post_codes_remain_pinned(self) -> None:
        for definition in (
            "POST_B06V8_PASS2_ACCEPTED\t0xca",
            "POST_B06V8_OUTER_RESET_ARMED\t0xcb",
            "POST_B06V8_CSI_PASS3_ARMED\t0xcf",
            "POST_B06V8_CSI_PASS3_RETURN\t0xd9",
            "POST_B06V8_TERMINAL\t\t0xda",
            "POST_B06V8_IOH_SYRE\t\t0xfe",
        ):
            self.assertIn(definition, ROMSTAGE)

    def test_wrapper_hash_and_branch_digest_ranges_are_pinned(self) -> None:
        self.assertIn("X58_VENDOR_CSI_WRAPPER_FNV1A\t0x986153c5u", VENDOR_INIT)
        signature = between(
            VENDOR_INIT,
            "#elif CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS\n\t\t&& signature_digest_valid",
            "#endif",
        )
        self.assertIn("signature_digest_valid(0xfffc0bbeu, 16, 0x51beeb35u)", signature)
        self.assertIn("signature_digest_valid(0xfffc0510u, 10, 0xa246696cu)", signature)
        self.assertNotIn("0x9321081du", signature)

    def test_high_profile_gates_decode_before_ecam(self) -> None:
        function = between(
            VENDOR_INIT,
            "static bool b06v8_high_qpi_platform_exact(void)",
            "static bool pe32_header_valid",
        )
        sad_gate = function.index("X58_VENDOR_SAD_ID")
        low_gate = function.index("pciexbar_low != X58_VENDOR_PCIEXBAR_LO")
        first_ecam = function.index("fixed_read32(0xe0000000u)")
        self.assertLess(sad_gate, first_ecam)
        self.assertLess(low_gate, first_ecam)
        self.assertIn("fixed_read32(0xe0000008u) != 0x06000013u", function)

    def test_cmos_inputs_and_pass2_state_pairs_are_exact(self) -> None:
        self.assertIn("B06V8_CMOS_COLD_AUTHORIZATION\t0x2c", ROMSTAGE)
        for expected in (
            "B06V8_EXT80_EXPECTED\t\t0xb3",
            "B06V8_EXT81_EXPECTED\t\t0x19",
            "B06V8_EXT82_EXPECTED\t\t0xd7",
            "B06V8_EXT88_EXPECTED\t\t0x67",
            "B06V8_EXT89_EXPECTED\t\t0xcb",
        ):
            self.assertIn(expected, ROMSTAGE)
        probe = between(
            ROMSTAGE,
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
            "static enum b06v6_auto_result __maybe_unused b06v6_auto_handoff(",
        )
        self.assertIn(
            "cmos_guard != B06V8_CMOS_COLD_AUTHORIZATION", probe
        )
        for condition in (
            "csi_state[0x1c] != 0",
            "csi_state[0x1d] != 1",
            "csi_state[0x70] != 0",
            "csi_state[0x71] != 1",
            "csi_state[0xcd] != 0",
            "csi_state[0xce] != 1",
            "csi_state[0x121] != 0",
            "csi_state[0x122] != 1",
        ):
            self.assertIn(condition, probe)

    def test_cold_signature_is_captured_before_spd_and_owned_values_reject(self) -> None:
        startup = between(
            ROMSTAGE,
            "/* Program the controller decode only; do not access SMBus status/data. */",
            "outb(POST_B04_UART_SENT, CONFIG_POST_IO_PORT);",
        )
        decode = startup.index("smbus_enable_iobar(CONFIG_FIXED_SMBUS_IO_BASE)")
        capture = startup.index("b06v6_capture_i801_signature(&entry_signature)")
        consume = startup.index(
            "b06v6_set_i801_signature(&b06v8_consumed_signature)"
        )
        report = startup.index("report_ich10(")
        self.assertLess(decode, capture)
        self.assertLess(capture, consume)
        self.assertLess(consume, report)
        self.assertLess(capture, report)
        self.assertIn("#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS", startup)
        self.assertIn("b06v8_pass2_signature", startup)
        self.assertIn("b06v8_pass3_signature", startup)
        self.assertIn('b06v8_phase_guard_stop("B06V8_ENTRY_PHASE_CONSUME")', startup)

        probe = between(
            ROMSTAGE,
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
            "static enum b06v6_auto_result __maybe_unused b06v6_auto_handoff(",
        )
        cold = between(
            probe,
            "if (smbus_entry == B06V3_SMBUS_ENTRY_COLD) {",
            "} else {",
        )
        for signature in (
            "b06v8_pass2_signature",
            "b06v8_consumed_signature",
            "b06v8_pass3_signature",
        ):
            self.assertIn(signature, cold)
        self.assertIn("B06V8_COLD_RETAINED_PHASE_SIGNATURE", cold)
        self.assertLess(
            probe.index("B06V8_COLD_RETAINED_PHASE_SIGNATURE"),
            probe.index("b06j_run_spd(state)"),
        )
        entry_print = between(
            probe,
            '!b04_uart_puts(" I801_SIG=")',
            '!b04_uart_puts(" CMOS0E=")',
        )
        self.assertNotIn('b04_uart_puts("NA")', entry_print)
        for field in ("control", "command", "xmit_address", "data0", "data1"):
            self.assertIn(f"entry_signature->{field}", entry_print)

    def test_phase_markers_are_committed_after_uart_and_before_action(self) -> None:
        probe = between(
            ROMSTAGE,
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
            "static enum b06v6_auto_result __maybe_unused b06v6_auto_handoff(",
        )
        announce = probe.index("[QPI] CSI_PASS=1")
        flush = probe.index("b04_uart_wait_for", announce)
        cmos_commit = probe.index(
            "b06v6_write_cmos_diagnostic(B06V6_CMOS_GUARD_IN_PROGRESS)", flush
        )
        phase_commit = probe.index("b06v6_set_i801_signature(next_signature)", cmos_commit)
        csi_call = probe.index("x58_vendor_call_csi_wrapper(&result)", phase_commit)
        self.assertLess(announce, flush)
        self.assertLess(flush, cmos_commit)
        self.assertLess(cmos_commit, phase_commit)
        self.assertLess(phase_commit, csi_call)

        pass2_message = probe.index("[QPI] PASS2_ACCEPTED")
        pass2_flush = probe.index("b04_uart_wait_for", pass2_message)
        pass3_commit = probe.index(
            "b06v6_set_i801_signature(&b06v8_pass3_signature)", pass2_flush
        )
        reset = probe.index("b06v8_issue_outer_syre_reset()", pass3_commit)
        self.assertLess(pass2_message, pass2_flush)
        self.assertLess(pass2_flush, pass3_commit)
        self.assertLess(pass3_commit, reset)

        recovery = between(
            ROMSTAGE,
            'if (b06j_streq(argv[0], "autoguard")',
            'if (b06j_streq(argv[0], "reset")',
        )
        self.assertIn("B06V8_CMOS_COLD_AUTHORIZATION", recovery)
        self.assertIn("B06V8_CMOS_PHASE_FAILED", recovery)
        self.assertIn("0xec/0xed->0x2c", recovery)

    def test_pre_outer_tuple_and_reset_edge_are_complete(self) -> None:
        entry_gate = between(
            ROMSTAGE,
            "static bool b06v8_slow_qpi_entry_tuple_exact(",
            "static bool b06v8_high_qpi_tuple_exact",
        )
        for condition in (
            "tuple->cpu_50 == 0x160c0110",
            "tuple->cpu_54 == 0x00000010",
            "tuple->cpu_6c == 0x0000a020",
            "tuple->cpu_80 == 0x030f0f03",
            "tuple->cpu_94 == 0x00000102",
            "tuple->cpu_9c == 0x00000502",
            "tuple->cpu_a0 == 0x00000c00",
            "tuple->cpu_a4 == 0x001d2c03",
            "tuple->ioh_82c == 0x00006020",
            "tuple->ioh_840 == 0x030f0f03",
            "tuple->ioh_854 == 0x00000102",
            "tuple->ioh_85c == 0x00000002",
            "tuple->ioh_864 == 0x00322808",
            "0x00000200 : 0x00000600",
        ):
            self.assertIn(condition, entry_gate)

        tuple_gate = between(
            ROMSTAGE,
            "static bool b06v8_pre_outer_reset_tuple_exact(",
            "static bool b06v8_high_qpi_tuple_exact",
        )
        for condition in (
            "tuple->cpu_9c == 0x00000502",
            "tuple->cpu_a0 == 0x00000c00",
            "tuple->cpu_a4 == 0x00322808",
            "tuple->ioh_82c == 0x004060a0",
            "tuple->ioh_sr0_7c == 0",
            "tuple->ioh_sr1_80 == 0",
            "tuple->ioh_syre_cc == 0x00000600",
        ):
            self.assertIn(condition, tuple_gate)

        reset = between(
            ROMSTAGE,
            "static void __noreturn b06v8_issue_outer_syre_reset(void)",
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
        )
        sequence = (
            "if (syre != 0x00000600)",
            "syre & ~B06V8_IOH_SYRE_REQUEST",
            "if (syre != 0x00000200)",
            "B06V8_IOH_SR_DEV, 0x7c, 0, 4",
            "B06V8_IOH_SR_DEV, 0x80, 0, 4",
            "outb(POST_B06V8_OUTER_RESET_ARMED",
            "outb(POST_B06V8_IOH_SYRE",
            "syre | B06V8_IOH_SYRE_REQUEST",
            'asm volatile("cli"',
        )
        positions = [reset.index(item) for item in sequence]
        self.assertEqual(positions, sorted(positions))
        for reason in (
            "B06V8_SYRE_PRE_EDGE_GATE",
            "B06V8_SYRE_CLEAR_READBACK",
            "B06V8_SR_CLEAR_READBACK",
            "B06V8_SYRE_FINAL_PRE_EDGE_GATE",
        ):
            self.assertIn(f'b06v8_phase_guard_stop("{reason}")', reset)

        final_reread = (
            "syre = b06j_pci_read(B06V8_IOH_SYRE_DEV, 0xcc, 4);"
        )
        self.assertEqual(reset.count(final_reread), 3)
        self.assertGreater(
            reset.rindex(final_reread),
            reset.index('b06v8_phase_guard_stop("B06V8_SR_CLEAR_READBACK")'),
        )
        self.assertLess(
            reset.index('b06v8_phase_guard_stop("B06V8_SYRE_FINAL_PRE_EDGE_GATE")'),
            reset.index("outb(POST_B06V8_OUTER_RESET_ARMED"),
        )
        after_final_set = reset[reset.index("syre | B06V8_IOH_SYRE_REQUEST") :]
        self.assertNotIn("b06v8_phase_guard_stop", after_final_set)
        self.assertNotIn("b06v6_write_cmos_diagnostic", after_final_set)
        self.assertNotIn("b04_uart_", after_final_set)
        self.assertNotIn("0xcf9", reset.lower())

        fail_stop = between(
            ROMSTAGE,
            "static void __noreturn b06v8_phase_guard_stop(",
            "static enum b06v6_auto_result b06v6_fallback(",
        )
        self.assertIn("B06V8_CMOS_PHASE_FAILED", fail_stop)
        self.assertIn("DO NOT WARM RESET", fail_stop)
        self.assertIn("stop_with_post(POST_B06V6_GUARD_STOP)", fail_stop)

    def test_probe_is_terminal_and_never_calls_minit_or_handoff(self) -> None:
        probe = between(
            ROMSTAGE,
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
            "static enum b06v6_auto_result __maybe_unused b06v6_auto_handoff(",
        )
        self.assertIn("B06V8_PASS3_TERMINAL", probe)
        self.assertNotIn("x58_vendor_call_minit", probe)
        self.assertNotIn("x58_b06v6_handoff_record_success", probe)
        dispatch = between(
            ROMSTAGE,
            "#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS\n\tauto_result",
            "if (auto_result == B06V6_AUTO_UART_ERROR)",
        )
        self.assertIn("b06v8_high_qpi_probe", dispatch)
        self.assertIn("#else", dispatch)
        self.assertIn("b06v6_auto_handoff", dispatch)


if __name__ == "__main__":
    unittest.main()
