#!/usr/bin/env python3

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
KCONFIG = (BOARD / "Kconfig").read_text()
ROMSTAGE = (BOARD / "romstage.c").read_text()
VENDOR_INIT = (BOARD / "vendor_init.c").read_text()
VENDOR_HEADER = (BOARD / "vendor_init.h").read_text()
BOOTBLOCK = (BOARD / "bootblock.c").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
DEFCONFIG = (ROOT / "configs/x58-pro-e-b06vb.config").read_text()


def between(source: str, first: str, last: str) -> str:
    begin = source.index(first)
    end = source.index(last, begin)
    return source[begin:end]


class B06VBSourceContractTests(unittest.TestCase):
    def test_option_is_default_off_and_narrowly_inherits_b06va(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        self.assertIn("depends on X58_PRO_E_B06VA_HIGH_QPI_A0_SET", option)
        self.assertIn("default n", option)
        self.assertIn("never accesses ordinary DRAM", option)
        self.assertIn("never declares training successful", option)
        self.assertIn("never hands off to postcar", option)
        self.assertIn(
            "CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE=y", DEFCONFIG
        )

    def test_exact_pass3_observation_is_required(self) -> None:
        state_gate = between(
            ROMSTAGE,
            "static bool b06vb_csi_state_exact(",
            "static bool b06v8_print_csi_state(",
        )
        helper = between(
            ROMSTAGE,
            "static enum b06v6_auto_result b06vb_high_qpi_minit_observe(",
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
        )
        self.assertIn("B06VB_CSI_OBSERVATION_FNV\t0x8b38506a", ROMSTAGE)
        for literal in (
            "csi_result->eax != 0",
            "csi_result->ebx != 0",
            "csi_result->ecx != 0x11",
            "state[0x06] == 0x01",
            "state[0x07] == 0x01",
            "state[0x2a6] == 0x0c",
            "state[0x2ed] == 0x01",
            "state[0x2ef] == 0x00",
            "state[0x301] == 0x00",
            "state[0x1c] == 0x00",
            "state[0x1d] == 0x01",
            "state[0x70] == 0x00",
            "state[0x71] == 0x01",
            "state[0xcd] == 0x00",
            "state[0xce] == 0x01",
            "state[0x121] == 0x00",
            "state[0x122] == 0x01",
            "state[0x030] == 0x00",
            "state[0x031] == 0x00",
            "state[0x1db] == 0x01",
            "state[0x206] == 0x12",
            "state[0x275] == 0x01",
            "state[0x285] == 0x0e",
            "state[0x2f8] == 0x11",
            "state[0x2f9] == 0x00",
            "state[0x2fc] == 0x00",
            "state[0x302] == 0x01",
        ):
            self.assertIn(literal, state_gate + helper)
        self.assertIn("HIGH_CSI_OBSERVATION=ACCEPTED_FOR_ONE_MINIT_CALL", helper)
        self.assertIn("not semantic CSI success", helper)

    def test_final_endpoint_is_exact_but_pointer_residues_are_telemetry(self) -> None:
        tuple_gate = between(
            ROMSTAGE,
            "static bool b06vb_post_csi_tuple_exact(",
            "static bool b06vb_csi_state_exact(",
        )
        stage_print = between(
            ROMSTAGE,
            "static bool b06vb_print_stage_fingerprints(",
            "static bool b06vb_post_csi_tuple_exact(",
        )
        vendor_gate = between(
            VENDOR_INIT,
            "static bool b06vb_high_qpi_post_csi_platform_exact(",
            "static bool b06vb_high_qpi_minit_is_authorized(",
        )
        for value in (
            "0x0040a0a8",
            "0x00b00502",
            "0x00017000",
            "0x86000000",
            "0x0616fc00",
            "0xea000000",
        ):
            self.assertIn(value, tuple_gate)
            self.assertIn(value + "u", vendor_gate)
        self.assertIn("0x0a000006", tuple_gate)
        self.assertIn("0x00000006", tuple_gate)
        self.assertIn("X58_VENDOR_MEMORY_CLOCK_STATE", vendor_gate)
        self.assertIn("X58_VENDOR_MEMORY_RATIO_STATE", vendor_gate)
        self.assertIn("B06VB_CSI_CONTEXT_DELTA", stage_print)
        self.assertIn("CPU80/D0 READ_ONLY_NOT_GATES", stage_print)
        self.assertNotIn("stage->cpu_context_80", tuple_gate)
        self.assertNotIn("stage->cpu_stage_d0", tuple_gate)
        self.assertNotIn("X58_VENDOR_QPI_LINK_DEV, 0x80", vendor_gate)
        self.assertNotIn("X58_VENDOR_QPI_LINK_DEV, 0xd0", vendor_gate)

    def test_vendor_gate_switches_on_return_then_requires_authorization(self) -> None:
        platform_gate = between(
            VENDOR_INIT,
            "static enum x58_vendor_status csi_platform_gate(void)",
            "static enum x58_vendor_status pre_pciexbar_platform_gate(void)",
        )
        authorize = between(
            VENDOR_INIT,
            "enum x58_vendor_status x58_vendor_b06vb_authorize_high_qpi_minit(",
            "enum x58_vendor_status x58_vendor_arm_csi_wrapper(",
        )
        minit_path = between(
            VENDOR_INIT,
            "enum x58_vendor_status x58_vendor_install_confirmed_minit_policy(",
            "const void *x58_vendor_csi_state_snapshot(",
        )
        self.assertIn("if (runtime.csi_returned)", platform_gate)
        self.assertIn("b06vb_high_qpi_post_csi_platform_exact()", platform_gate)
        self.assertIn("!runtime.csi_result_accepted", authorize)
        self.assertIn("b06vb_high_qpi_post_csi_platform_exact()", authorize)
        self.assertIn("runtime.b06vb_high_qpi_minit_authorized = true", authorize)
        self.assertEqual(
            minit_path.count("!b06vb_high_qpi_minit_is_authorized()"), 3
        )
        self.assertIn("x58_vendor_b06vb_authorize_high_qpi_minit", VENDOR_HEADER)

    def test_policy_is_direct_seven_edit_path_without_force_cold(self) -> None:
        helper = between(
            ROMSTAGE,
            "static enum b06v6_auto_result b06vb_high_qpi_minit_observe(",
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
        )
        self.assertIn("B06VB_POLICY_BASE_FNV\t\t0xbc4268bf", ROMSTAGE)
        self.assertIn("ARRAY_SIZE(b06v6_candidate_edits)", helper)
        self.assertIn("X58_B06V6_EXPECTED_POLICY_FNV", helper)
        self.assertIn("SEVEN_EDIT_FNV=", helper)
        self.assertIn("FORCE_COLD_HELPER=00", helper)
        self.assertNotIn("b06v5_policy_force_cold", helper)

    def test_unique_guard_is_committed_immediately_before_d3(self) -> None:
        helper = between(
            ROMSTAGE,
            "static enum b06v6_auto_result b06vb_high_qpi_minit_observe(",
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
        )
        signature = between(
            ROMSTAGE,
            "static const struct b06v6_i801_signature b06vb_minit_in_progress_signature",
            "/* Neutral offset names are intentional",
        )
        for value in ("0x64", "0x9b", "0x5c", "0xa3"):
            self.assertIn(value, signature)
        self.assertIn("EXPECT_CONSUMED_08:42:bd:7a:85_MATCH=", helper)
        self.assertIn("B06VB_PRE_MINIT_PERSISTENCE_GATE", helper)
        commit = helper.index(
            "b06v6_set_i801_signature(&b06vb_minit_in_progress_signature)"
        )
        d3 = helper.index("outb(POST_B06V0_MINIT_CALL", commit)
        call = helper.index("x58_vendor_call_minit", d3)
        d4 = helper.index("outb(POST_B06V0_MINIT_RETURN", call)
        self.assertLess(commit, d3)
        self.assertLess(d3, call)
        self.assertLess(call, d4)
        self.assertNotIn("b04_uart", helper[commit:d3])
        self.assertNotIn("b06v6_clear_signature", helper)
        self.assertNotIn("B06V6_CMOS_DIAGNOSTIC_OBSERVED", helper)

    def test_return_is_observational_terminal_and_never_hands_off(self) -> None:
        helper = between(
            ROMSTAGE,
            "static enum b06v6_auto_result b06vb_high_qpi_minit_observe(",
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
        )
        self.assertIn("FULL_PATH_RETURN_DRAM_UNTESTED", helper)
        self.assertIn("RETURNED_UNKNOWN_OR_INCOMPLETE_DRAM_UNTESTED", helper)
        self.assertIn("b06vb_print_complete_buffer(\"CSI\"", helper)
        self.assertIn("b06vb_print_complete_buffer(\"WORK\"", helper)
        self.assertIn("DRAM_ACCESSES=00", helper)
        self.assertIn("B06VB_MINIT_OBSERVATION_TERMINAL", helper)
        self.assertNotIn("x58_b06v6_handoff_record_success", helper)
        self.assertNotIn("B06V6_AUTO_READY", helper)

    def test_rommon_keeps_only_read_only_vendor_inspection_available(self) -> None:
        lock = between(
            ROMSTAGE,
            "if (b06j_streq(argv[0], \"vprep\") ||",
            "if (b06j_streq(argv[0], \"vprep\") && argc == 2)",
        )
        self.assertIn('b06j_streq(argv[0], "vpolicy")', lock)
        self.assertIn('argc == 4 && b06j_streq(argv[1], "dump")', lock)
        for command in ('"vprep"', '"vcsi"', '"vaccept"', '"vminit"'):
            self.assertIn(command, lock)
        self.assertIn(
            'b06j_streq(argv[0], "vpolicy") && argc == 4', ROMSTAGE
        )
        self.assertIn('b06j_streq(argv[1], "dump")', ROMSTAGE)

    def test_identity_precedes_inherited_b06va_identity_everywhere(self) -> None:
        identity = "X58PROE-B06VB-HIGHQPI-MINIT-OBSERVE-ROMMON-20260904"
        for source in (BOOTBLOCK, ROMSTAGE):
            self.assertIn(identity, source)
            self.assertLess(source.index(identity), source.index("X58PROE-B06VA"))
        self.assertIn("X58PROE-B06VB-UNEXPECTED-RAMSTAGE-20260904", RAMMON)
        self.assertLess(
            RAMMON.index("CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE"),
            RAMMON.index("CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET"),
        )
        self.assertIn("experimental B06VB High-QPI MINIT observation", MAINBOARD)


if __name__ == "__main__":
    unittest.main()
