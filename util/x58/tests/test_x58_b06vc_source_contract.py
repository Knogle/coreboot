#!/usr/bin/env python3

import hashlib
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
BOOTBLOCK = (BOARD / "bootblock.c").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
DEFCONFIG = (ROOT / "configs/x58-pro-e-b06vc.config").read_text()


def between(source: str, first: str, last: str) -> str:
    begin = source.index(first)
    end = source.index(last, begin)
    return source[begin:end]


class B06VCSourceContractTests(unittest.TestCase):
    def test_option_is_default_off_and_narrowly_inherits_b06vb(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        self.assertIn(
            "depends on X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE", option
        )
        self.assertIn("default n", option)
        self.assertIn("uses neither a mask nor a", option)
        self.assertIn("numeric range", option)
        self.assertIn("never writes this register", option)
        self.assertIn("post-CSI B06VB observation gate remains unchanged", option)
        self.assertIn("never accesses ordinary DRAM", option)
        self.assertIn("never hands off to postcar", option)

    def test_defconfig_selects_complete_chain_and_unique_identity(self) -> None:
        for symbol in (
            "CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET=y",
            "CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE=y",
            "CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT=y",
        ):
            self.assertIn(symbol, DEFCONFIG)
        self.assertIn(
            'CONFIG_MAINBOARD_PART_NUMBER="X58 Pro-E B06VC High-QPI A0 '
            'four-set MINIT ROMMON"',
            DEFCONFIG,
        )
        self.assertIn('CONFIG_LOCALVERSION="x58-pro-e-b06vc"', DEFCONFIG)
        self.assertIn("CONFIG_PAYLOAD_NONE=y", DEFCONFIG)

    def test_pre_pass3_a0_predicate_is_exactly_four_values(self) -> None:
        predicate = between(
            VENDOR_HEADER,
            "static inline bool x58_vendor_b06vc_high_qpi_cpu_a0_exact(",
            "#endif",
        )
        body = between(predicate, "{", "}")
        normalized = " ".join(body.split())
        self.assertIn(
            "return value == 0x00017000u || value == 0x00017600u || "
            "value == 0x00017800u || value == 0x00017a00u;",
            normalized,
        )
        literals = re.findall(r"0x[0-9a-fA-F]+u?", body)
        accepted = {int(literal.rstrip("uU"), 16) for literal in literals}
        self.assertEqual(
            accepted, {0x00017000, 0x00017600, 0x00017800, 0x00017A00}
        )
        self.assertEqual(len(literals), 4)
        self.assertNotIn("&", body)
        self.assertNotIn("<", body)
        self.assertNotIn(">", body)

    def test_only_two_pre_pass3_gates_use_the_four_value_predicate(self) -> None:
        vendor_gate = between(
            VENDOR_INIT,
            "static bool b06v8_high_qpi_platform_exact(void)",
            "static bool b06vb_high_qpi_post_csi_platform_exact(void)",
        )
        tuple_gate = between(
            ROMSTAGE,
            "static bool b06v8_high_qpi_tuple_exact(",
            "struct b06vb_stage_fingerprints",
        )
        for gate in (vendor_gate, tuple_gate):
            self.assertEqual(
                gate.count("x58_vendor_b06vc_high_qpi_cpu_a0_exact("), 1
            )
            self.assertEqual(gate.count("x58_vendor_high_qpi_cpu_a0_exact("), 1)
            self.assertIn(
                "#if CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT", gate
            )
            self.assertIn(
                "#else\n#if CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET", gate
            )
            self.assertIn("0x00017600", gate)
        self.assertEqual(
            (VENDOR_INIT + ROMSTAGE).count(
                "x58_vendor_b06vc_high_qpi_cpu_a0_exact("
            ),
            3,
        )

    def test_pass3_telemetry_prints_the_complete_exact_set(self) -> None:
        probe = between(
            ROMSTAGE,
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
            "static enum b06v6_auto_result __maybe_unused b06v6_auto_handoff(",
        )
        preselect = between(
            probe,
            "if (phase == B06V8_PHASE_PASS3) {",
            "call_status = x58_vendor_b06v8_select_high_qpi_csi(",
        )
        self.assertIn(
            "EXACT_SET=00017000|00017600|00017800|00017a00 MATCH=",
            preselect,
        )
        self.assertIn("x58_vendor_b06vc_high_qpi_cpu_a0_exact(cpu_a0)", preselect)
        self.assertIn("EXACT_SET=00017000|00017600 MATCH=", preselect)

    def test_post_csi_gate_remains_the_exact_b06vb_observation(self) -> None:
        tuple_gate = between(
            ROMSTAGE,
            "static bool b06vb_post_csi_tuple_exact(",
            "static bool b06vb_csi_state_exact(",
        )
        state_gate = between(
            ROMSTAGE,
            "static bool b06vb_csi_state_exact(",
            "static bool b06v8_print_csi_state(",
        )
        vendor_gate = between(
            VENDOR_INIT,
            "static bool b06vb_high_qpi_post_csi_platform_exact(",
            "static bool b06vb_high_qpi_minit_is_authorized(",
        )
        helper = between(
            ROMSTAGE,
            "static enum b06v6_auto_result b06vb_high_qpi_minit_observe(",
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
        )
        self.assertIn("tuple->cpu_a0 == 0x00017000", tuple_gate)
        self.assertIn("0x00017000u", vendor_gate)
        for widened in ("0x00017600", "0x00017800", "0x00017a00"):
            self.assertNotIn(widened, tuple_gate + vendor_gate)
        self.assertNotIn("x58_vendor_b06vc_high_qpi_cpu_a0_exact", tuple_gate)
        self.assertNotIn("x58_vendor_b06vc_high_qpi_cpu_a0_exact", vendor_gate)
        self.assertIn("B06VB_CSI_OBSERVATION_FNV\t0x8b38506a", ROMSTAGE)
        self.assertIn("X58_VENDOR_B06VB_CSI_FNV1A\t0x8b38506au", VENDOR_INIT)
        self.assertIn("csi_result->eax != 0", helper)
        self.assertIn("csi_result->ebx != 0", helper)
        self.assertIn("csi_result->ecx != 0x11", helper)
        self.assertIn("state[0x2a6] == 0x0c", state_gate)

    def test_b06vb_policy_minit_guard_and_terminal_path_are_inherited(self) -> None:
        helper = between(
            ROMSTAGE,
            "static enum b06v6_auto_result b06vb_high_qpi_minit_observe(",
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
        )
        self.assertIn("B06VB_POLICY_BASE_FNV\t\t0xbc4268bf", ROMSTAGE)
        self.assertIn("ARRAY_SIZE(b06v6_candidate_edits)", helper)
        self.assertIn("X58_B06V6_EXPECTED_POLICY_FNV", helper)
        self.assertIn("EXPECT_CONSUMED_08:42:bd:7a:85_MATCH=", helper)
        self.assertIn(
            "b06v6_set_i801_signature(&b06vb_minit_in_progress_signature)",
            helper,
        )
        self.assertIn("x58_vendor_call_minit", helper)
        self.assertIn("DRAM_ACCESSES=00", helper)
        self.assertIn("B06VB_MINIT_OBSERVATION_TERMINAL", helper)
        self.assertNotIn("b06v5_policy_force_cold", helper)
        self.assertNotIn("x58_b06v6_handoff_record_success", helper)
        self.assertNotIn("B06V6_AUTO_READY", helper)

    def test_manual_vendor_calls_stay_locked_but_policy_dump_is_read_only(self) -> None:
        lock = between(
            ROMSTAGE,
            'if (b06j_streq(argv[0], "vprep") ||',
            'if (b06j_streq(argv[0], "vprep") && argc == 2)',
        )
        for command in ('"vprep"', '"vcsi"', '"vaccept"', '"vminit"'):
            self.assertIn(command, lock)
        self.assertIn('b06j_streq(argv[0], "vpolicy")', lock)
        self.assertIn('argc == 4 && b06j_streq(argv[1], "dump")', lock)
        self.assertIn(
            'b06j_streq(argv[0], "vpolicy") && argc == 4', ROMSTAGE
        )
        self.assertIn('b06j_streq(argv[1], "dump")', ROMSTAGE)
        self.assertIn("B06VC is terminal: manual vendor calls remain disabled", lock)

    def test_identity_precedes_b06vb_in_every_stage(self) -> None:
        identity = "X58PROE-B06VC-HIGHQPI-A0-4SET-MINIT-ROMMON-20260904"
        for source in (BOOTBLOCK, ROMSTAGE):
            self.assertIn(identity, source)
            self.assertLess(source.index(identity), source.index("X58PROE-B06VB"))
        self.assertIn("X58PROE-B06VC-UNEXPECTED-RAMSTAGE-20260904", RAMMON)
        self.assertLess(
            RAMMON.index("CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT"),
            RAMMON.index("CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE"),
        )
        self.assertIn("experimental B06VC High-QPI A0 four-set", MAINBOARD)
        self.assertLess(
            MAINBOARD.index("CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT"),
            MAINBOARD.index("CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE"),
        )

    def test_legacy_b06va_and_b06vb_configs_remain_byte_exact(self) -> None:
        expected = {
            "x58-pro-e-b06va.config":
                "93c2e18fa306926526561acf72a7af87b35a27ce65313cc45cf4f01b78e995c9",
            "x58-pro-e-b06vb.config":
                "b62323678ff895e81e0059422ddc8c357268f5890680ae118ddd10c77019096c",
        }
        for name, digest in expected.items():
            data = (ROOT / "configs" / name).read_bytes()
            self.assertEqual(hashlib.sha256(data).hexdigest(), digest)
            self.assertNotIn(
                b"CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT", data
            )
        legacy_predicate = between(
            VENDOR_HEADER,
            "static inline bool x58_vendor_high_qpi_cpu_a0_exact(",
            "#endif",
        )
        self.assertIn(
            "return value == 0x00017000u || value == 0x00017600u;",
            " ".join(legacy_predicate.split()),
        )
        self.assertNotIn("0x00017800", legacy_predicate)
        self.assertNotIn("0x00017a00", legacy_predicate)


if __name__ == "__main__":
    unittest.main()
