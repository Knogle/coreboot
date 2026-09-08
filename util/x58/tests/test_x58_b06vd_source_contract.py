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
DEFCONFIG = (ROOT / "configs/x58-pro-e-b06vd.config").read_text()


def between(source: str, first: str, last: str) -> str:
    begin = source.index(first)
    end = source.index(last, begin)
    return source[begin:end]


def paired_digest_contract(byte_2a6: int, raw: int, canonical: int) -> bool:
    if canonical != 0x908DABB6:
        return False
    return ((byte_2a6 == 0x08 and raw == 0x03E3D24E) or
            (byte_2a6 == 0x0C and raw == 0x8B38506A))


class B06VDSourceContractTests(unittest.TestCase):
    def test_option_is_default_off_and_narrowly_inherits_b06vc(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        self.assertIn(
            "depends on X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT", option
        )
        self.assertIn("default n", option)
        self.assertIn("uses neither a mask nor a", option)
        self.assertIn("numeric range", option)
        self.assertIn("never writes this register", option)
        self.assertIn("0x08/0x03e3d24e", option)
        self.assertIn("0x0c/0x8b38506a", option)
        self.assertIn("canonical FNV-1a 0x908dabb6", option)
        self.assertIn("raw CSI buffer is never changed", option)
        self.assertIn("never accesses ordinary DRAM", option)
        self.assertIn("never\n\t  hands off to postcar", option)

    def test_defconfig_selects_complete_chain_and_terminal_car(self) -> None:
        for symbol in (
            "CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET=y",
            "CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE=y",
            "CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT=y",
            "CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT=y",
        ):
            self.assertIn(symbol, DEFCONFIG)
        self.assertIn(
            'CONFIG_MAINBOARD_PART_NUMBER="X58 Pro-E B06VD High-QPI '
            'CSI2A6 MINIT ROMMON"',
            DEFCONFIG,
        )
        self.assertIn('CONFIG_LOCALVERSION="x58-pro-e-b06vd"', DEFCONFIG)
        self.assertIn("CONFIG_PAYLOAD_NONE=y", DEFCONFIG)

    def test_pre_pass3_a0_predicate_is_exactly_five_values(self) -> None:
        predicate = between(
            VENDOR_HEADER,
            "static inline bool x58_vendor_b06vd_high_qpi_cpu_a0_exact(",
            "#endif",
        )
        body = between(predicate, "{", "}")
        literals = re.findall(r"0x[0-9a-fA-F]+u?", body)
        accepted = {int(literal.rstrip("uU"), 16) for literal in literals}
        self.assertEqual(
            accepted,
            {0x00017000, 0x00017600, 0x00017800, 0x00017A00, 0x00017C00},
        )
        self.assertEqual(len(literals), 5)
        self.assertNotIn("&", body)
        self.assertNotIn("<", body)
        self.assertNotIn(">", body)

    def test_five_value_predicate_is_used_only_by_two_pre_csi_gates(self) -> None:
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
                gate.count("x58_vendor_b06vd_high_qpi_cpu_a0_exact("), 1
            )
        self.assertEqual(
            (VENDOR_INIT + ROMSTAGE).count(
                "x58_vendor_b06vd_high_qpi_cpu_a0_exact("
            ),
            2,
        )
        probe = between(
            ROMSTAGE,
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
            "static enum b06v6_auto_result __maybe_unused b06v6_auto_handoff(",
        )
        self.assertIn(
            "EXACT_SET=00017000|00017600|00017800|00017a00|00017c00; "
            "DUAL_GATE_FOLLOWS",
            probe,
        )

    def test_raw_and_canonical_digest_pairs_fail_closed(self) -> None:
        self.assertTrue(paired_digest_contract(0x08, 0x03E3D24E, 0x908DABB6))
        self.assertTrue(paired_digest_contract(0x0C, 0x8B38506A, 0x908DABB6))
        for byte_2a6, raw, canonical in (
            (0x08, 0x8B38506A, 0x908DABB6),
            (0x0C, 0x03E3D24E, 0x908DABB6),
            (0x08, 0x03E3D24E, 0x908DABB7),
            (0x0C, 0x8B38506A, 0x00000000),
            (0x0A, 0x03E3D24E, 0x908DABB6),
            (0x08, 0x00000000, 0x908DABB6),
        ):
            self.assertFalse(paired_digest_contract(byte_2a6, raw, canonical))

        selector = between(
            ROMSTAGE,
            "static bool b06vd_select_expected_csi_raw_digest(",
            "static bool b06vb_csi_state_exact(",
        )
        self.assertIn("canonical_digest != B06VD_CSI_CANONICAL_FNV", selector)
        self.assertIn("observed_raw_digest == B06VD_CSI_RAW_08_FNV", selector)
        self.assertIn("observed_raw_digest == B06VD_CSI_RAW_0C_FNV", selector)
        self.assertIn("*expected_raw_digest = B06VD_CSI_RAW_08_FNV", selector)
        self.assertIn("*expected_raw_digest = B06VD_CSI_RAW_0C_FNV", selector)
        self.assertNotIn("*expected_raw_digest = observed_raw_digest", selector)

    def test_canonicalization_changes_only_hash_stream_on_both_sides(self) -> None:
        rom_digest = between(
            ROMSTAGE,
            "static uint32_t b06vd_csi_canonical_digest(",
            "static bool b06vd_select_expected_csi_raw_digest(",
        )
        vendor_digest = between(
            VENDOR_INIT,
            "static uint32_t b06vd_csi_canonical_digest(",
            "static bool b06vd_csi_digest_pair_exact(",
        )
        for implementation, offset_name in (
            (rom_digest, "B06VD_CSI_DYNAMIC_OFFSET"),
            (vendor_digest, "X58_VENDOR_B06VD_CSI_DYNAMIC_OFFSET"),
        ):
            self.assertIn(f"offset == {offset_name} ?", implementation)
            self.assertIn("0 : state[offset]", implementation)
            self.assertIn("digest ^= value", implementation)
            self.assertNotRegex(implementation, r"state\s*\[[^]]+\]\s*=")

        vendor_gate = between(
            VENDOR_INIT,
            "static bool b06vb_high_qpi_post_csi_platform_exact(",
            "static bool b06vb_high_qpi_minit_is_authorized(",
        )
        self.assertIn("b06vd_csi_canonical_digest(state)", vendor_gate)
        self.assertIn("b06vd_csi_digest_pair_exact(", vendor_gate)
        self.assertIn("runtime.csi_state_digest != raw_digest", vendor_gate)

    def test_expected_raw_digest_is_selected_before_accept(self) -> None:
        helper = between(
            ROMSTAGE,
            "static enum b06v6_auto_result b06vb_high_qpi_minit_observe(",
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
        )
        select = helper.index("b06vd_select_expected_csi_raw_digest(")
        pair_gate = helper.index("!csi_digest_pair_exact", select)
        accept = helper.index("x58_vendor_accept_csi_result(", pair_gate)
        self.assertLess(select, pair_gate)
        self.assertLess(pair_gate, accept)
        self.assertIn("expected_csi_raw_fnv,", helper[accept:accept + 300])
        self.assertNotIn("csi_raw_fnv,\n\t\tX58_VENDOR_EXPERIMENT_CONFIRMATION", helper)
        self.assertIn("csi_info->csi_state_digest != csi_raw_fnv", helper)
        self.assertIn("info.csi_state_digest != expected_csi_raw_fnv", helper)

    def test_abi_sparse_state_and_platform_endpoint_remain_strict(self) -> None:
        helper = between(
            ROMSTAGE,
            "static enum b06v6_auto_result b06vb_high_qpi_minit_observe(",
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
        )
        tuple_gate = between(
            ROMSTAGE,
            "static bool b06vb_post_csi_tuple_exact(",
            "static uint32_t b06vd_csi_canonical_digest(",
        )
        vendor_gate = between(
            VENDOR_INIT,
            "static bool b06vb_high_qpi_post_csi_platform_exact(",
            "static bool b06vb_high_qpi_minit_is_authorized(",
        )
        for check in (
            "csi_result->eax != 0",
            "csi_result->ebx != 0",
            "csi_result->ecx != 0x11",
            "!csi_info->canaries_valid",
            "!b06v8_result_is_self_consistent",
            "!b06vb_csi_state_exact",
        ):
            self.assertIn(check, helper)
        for value in (
            "0x0040a0a8",
            "0x00b00502",
            "0x00017000",
            "0x86000000",
            "0x0616fc00",
            "0xea000000",
            "0x0a000006",
            "0x00000006",
        ):
            self.assertIn(value, tuple_gate)
        for value in (
            "0x0040a0a8u",
            "0x00b00502u",
            "0x00017000u",
            "0x86000000u",
            "0x0616fc00u",
            "0xea000000u",
        ):
            self.assertIn(value, vendor_gate)
        self.assertNotIn("x58_vendor_b06vd_high_qpi_cpu_a0_exact", tuple_gate)
        self.assertNotIn("x58_vendor_b06vd_high_qpi_cpu_a0_exact", vendor_gate)

        def endpoint(a0: int, reg_9c: int) -> bool:
            return a0 == 0x00017000 and reg_9c == 0x00B00502

        self.assertTrue(endpoint(0x00017000, 0x00B00502))
        self.assertFalse(endpoint(0x00017A00, 0x00A00502))

    def test_minit_guard_manual_lock_and_terminal_path_are_unchanged(self) -> None:
        helper = between(
            ROMSTAGE,
            "static enum b06v6_auto_result b06vb_high_qpi_minit_observe(",
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
        )
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
        self.assertIn("DRAM_ACCESSES=00", helper)
        self.assertIn("B06VB_MINIT_OBSERVATION_TERMINAL", helper)
        self.assertNotIn("x58_b06v6_handoff_record_success", helper)
        self.assertNotIn("B06V6_AUTO_READY", helper)

        lock = between(
            ROMSTAGE,
            'if (b06j_streq(argv[0], "vprep") ||',
            'if (b06j_streq(argv[0], "vprep") && argc == 2)',
        )
        for command in ('"vprep"', '"vcsi"', '"vaccept"', '"vminit"'):
            self.assertIn(command, lock)
        self.assertIn('argc == 4 && b06j_streq(argv[1], "dump")', lock)
        self.assertIn("B06VD is terminal: manual vendor calls remain disabled", lock)

    def test_identity_precedes_b06vc_in_every_stage(self) -> None:
        identity = "X58PROE-B06VD-HIGHQPI-CSI2A6-MINIT-ROMMON-20260905"
        for source in (BOOTBLOCK, ROMSTAGE):
            self.assertIn(identity, source)
            self.assertLess(source.index(identity), source.index("X58PROE-B06VC"))
        self.assertIn("X58PROE-B06VD-UNEXPECTED-RAMSTAGE-20260905", RAMMON)
        self.assertLess(
            RAMMON.index("CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT"),
            RAMMON.index("CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT"),
        )
        self.assertIn("experimental B06VD paired CSI2A6", MAINBOARD)
        self.assertLess(
            MAINBOARD.index("CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT"),
            MAINBOARD.index("CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT"),
        )

    def test_predecessor_defconfigs_remain_byte_exact(self) -> None:
        expected = {
            "x58-pro-e-b06va.config":
                "93c2e18fa306926526561acf72a7af87b35a27ce65313cc45cf4f01b78e995c9",
            "x58-pro-e-b06vb.config":
                "b62323678ff895e81e0059422ddc8c357268f5890680ae118ddd10c77019096c",
            "x58-pro-e-b06vc.config":
                "6079d3d9981b12eb0860ff61523a87799cf8f01aeb5772aa416ab8832c7d5ada",
        }
        for name, digest in expected.items():
            data = (ROOT / "configs" / name).read_bytes()
            self.assertEqual(hashlib.sha256(data).hexdigest(), digest)
            self.assertNotIn(
                b"CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT", data
            )


if __name__ == "__main__":
    unittest.main()
