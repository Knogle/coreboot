#!/usr/bin/env python3

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
HANDOFF = (BOARD / "b06v6_handoff.c").read_text()
HANDOFF_HEADER = (BOARD / "b06v6_handoff.h").read_text()

VH_SYMBOL = "CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS"
VG_SYMBOL = "CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS"
VH_ID = "X58PROE-B06VH-PRIMARY-WORKSPACE-REARM-SEABIOS-20260905"
VG_ID = "X58PROE-B06VG-COUPLED-MINIT-SEABIOS-20260905"

PRIMARY_PROFILES = {
    "A": (0x94299F43, 0xA6F9C2E6),
    "B": (0xB6346533, 0xA6F9C2E6),
    "P": (0xEB15C076, 0xC313E060),
    "N": (0x5FB636D9, 0xB3FAFEC0),
}
OBSERVATION_C = (0x64E3C821, 0xE20C406B)


def between(source: str, first: str, last: str) -> str:
    begin = source.index(first)
    end = source.index(last, begin)
    return source[begin:end]


def function_text(source: str, name: str) -> str:
    definition = re.search(
        rf"\b{re.escape(name)}\s*\([^;{{}}]*\)\s*\{{", source, re.DOTALL
    )
    if definition is None:
        raise ValueError(f"no definition for function {name}")
    begin = source.rfind("\n", 0, definition.start()) + 1
    body = definition.end() - 1
    depth = 0
    for offset in range(body, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[begin : offset + 1]
    raise ValueError(f"unterminated function {name}")


def digest_pair_contract(raw: int, canonical: int) -> bool:
    return (raw, canonical) in PRIMARY_PROFILES.values()


class B06VHSourceContractTests(unittest.TestCase):
    def test_option_is_default_off_and_strictly_layers_on_b06vg(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        self.assertIn("depends on X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS", option)
        self.assertIn("default n", option)
        self.assertNotIn("default y", option)
        for evidence in (
            "0xeb15c076",
            "0xc313e060",
            "0x5fb636d9",
            "0xb3fafec0",
            "08:00:5d:01:a3",
            "08:64:9b:5c:a3",
        ):
            self.assertIn(evidence, option.lower())

    def test_vh_identity_precedes_vg_in_every_stage_and_board_name(self) -> None:
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON):
            self.assertIn(VH_ID, source)
            self.assertIn(VG_ID, source)
            self.assertLess(source.index(VH_ID), source.index(VG_ID))

        id_command = between(
            ROMSTAGE,
            'if (b06j_streq(argv[0], "id") && argc == 1)',
            'if (b06j_streq(argv[0], "unlock") && argc == 2)',
        )
        self.assertLess(id_command.index(VH_SYMBOL), id_command.index(VG_SYMBOL))
        board_name = between(MAINBOARD, "\t.name =", ",\n#if")
        self.assertLess(board_name.index(VH_SYMBOL), board_name.index(VG_SYMBOL))

        defaults = KCONFIG[KCONFIG.index("config MAINBOARD_PART_NUMBER") :]
        self.assertLess(defaults.index("B06VH"), defaults.index("B06VG"))

    def test_executable_digest_truth_table_is_disjoint_and_rejects_c(self) -> None:
        for raw, canonical in PRIMARY_PROFILES.values():
            self.assertTrue(digest_pair_contract(raw, canonical))
        self.assertFalse(digest_pair_contract(*OBSERVATION_C))

        all_raw = [pair[0] for pair in PRIMARY_PROFILES.values()] + [OBSERVATION_C[0]]
        all_canonical = list({pair[1] for pair in PRIMARY_PROFILES.values()}) + [
            OBSERVATION_C[1]
        ]
        for raw in all_raw:
            for canonical in all_canonical:
                expected = (raw, canonical) in PRIMARY_PROFILES.values()
                self.assertEqual(digest_pair_contract(raw, canonical), expected)

    def test_shared_digest_helper_encodes_only_four_exact_pairs(self) -> None:
        helper = function_text(HANDOFF_HEADER, "x58_b06vh_workspace_digest_pair_exact")
        for raw, canonical in PRIMARY_PROFILES.values():
            self.assertIn(f"0x{raw:08x}u", HANDOFF_HEADER.lower())
            self.assertIn(f"0x{canonical:08x}u", HANDOFF_HEADER.lower())
        for forbidden in OBSERVATION_C:
            self.assertNotIn(f"0x{forbidden:08x}u", helper.lower())
        self.assertNotIn("357dc65a", helper.lower())

        # The source must express pairs, not independent raw/canonical sets.
        self.assertGreaterEqual(helper.count("&&"), 4)
        self.assertGreaterEqual(helper.count("||"), 3)
        self.assertIn("raw_digest", helper)
        self.assertIn("canonical_digest", helper)

    def test_workspace_pattern_helper_couples_markers_and_residuals(self) -> None:
        helper = function_text(ROMSTAGE, "b06vh_workspace_pattern_exact")
        self.assertIn("x58_b06vh_workspace_digest_pair_exact", helper)
        for raw, _canonical in PRIMARY_PROFILES.values():
            self.assertIn(f"0x{raw:08x}u", HANDOFF_HEADER.lower())
        for offset in (
            0x18D1,
            0x18D2,
            0x18D3,
            0x23A2,
            0x2402,
            0x2459,
            0x245C,
            0x245D,
            0x2460,
            0x2461,
            0x26B6,
            0x26BB,
            0x26C0,
            0x26C5,
        ):
            self.assertIn(f"0x{offset:x}", helper.lower())
        self.assertNotIn("357dc65a", helper.lower())
        self.assertNotIn("64e3c821", helper.lower())
        self.assertNotIn("e20c406b", helper.lower())

    def test_post_minit_guard_requires_transformed_tuple_then_rearms(self) -> None:
        observe = function_text(ROMSTAGE, "b06vb_high_qpi_minit_observe")
        promotion = function_text(ROMSTAGE, "b06ve_promote_post_minit")
        transformed = between(
            ROMSTAGE,
            "b06vh_minit_return_signature = {",
            "};",
        )
        for field, value in (
            ("control", "I801_BYTE_DATA"),
            ("command", "0x00"),
            ("xmit_address", "0x5d"),
            ("data0", "0x01"),
            ("data1", "0xa3"),
        ):
            self.assertRegex(transformed, rf"\.{field}\s*=\s*{value}")

        call_at = observe.index("x58_vendor_call_minit(")
        transformed_at = observe.index("b06vh_minit_return_signature", call_at)
        cmos_at = observe.index(
            "post_minit_cmos = b06v6_read_cmos_diagnostic()", transformed_at
        )
        raw_at = observe.index("POST_MINIT_I801_RAW=", transformed_at)
        observation_at = observe.index("csi_profile == B06VG_CSI_OBSERVATION")
        terminal_at = observe.index("B06VG_OBSERVATION_MINIT_TERMINAL", observation_at)
        primary_at = observe.index("csi_profile != B06VG_CSI_PRIMARY", terminal_at)
        promotion_at = observe.index("b06ve_promote_post_minit", primary_at)

        ordered = (
            call_at,
            transformed_at,
            cmos_at,
            raw_at,
            observation_at,
            terminal_at,
            primary_at,
            promotion_at,
        )
        self.assertEqual(ordered, tuple(sorted(ordered)))
        self.assertEqual(
            observe.count(
                "b06v6_set_i801_signature(&b06vb_minit_in_progress_signature)"
            ),
            1,
        )
        self.assertEqual(observe.count("x58_vendor_call_minit("), 1)

        # The returned tuple is not rewritten on an OBSERVATION path.  Only
        # the fully exact PRIMARY promotion may recreate the finalizer marker.
        exact_gate_at = promotion.index("if (minit_call_status")
        exact_gate_end = promotion.index("#if CONFIG_X58_PRO_E_B06VH", exact_gate_at)
        setter_at = promotion.index("b06v6_set_i801_signature(", exact_gate_end)
        marker_at = promotion.index(
            "&b06vb_minit_in_progress_signature", setter_at
        )
        fresh_capture_at = promotion.index(
            "b06v6_capture_i801_signature", marker_at
        )
        rearmed_match_at = promotion.index(
            "b06vb_minit_in_progress_signature", fresh_capture_at
        )
        rearmed_log_at = promotion.index("REARMED_EXACT=", rearmed_match_at)
        handoff_at = promotion.index("x58_b06v6_handoff_record_success", rearmed_log_at)
        self.assertEqual(
            (
                exact_gate_at,
                exact_gate_end,
                setter_at,
                marker_at,
                fresh_capture_at,
                rearmed_match_at,
                rearmed_log_at,
                handoff_at,
            ),
            tuple(
                sorted(
                    (
                        exact_gate_at,
                        exact_gate_end,
                        setter_at,
                        marker_at,
                        fresh_capture_at,
                        rearmed_match_at,
                        rearmed_log_at,
                        handoff_at,
                    )
                )
            ),
        )

    def test_promotion_uses_complete_profile_and_never_broad_canonical(self) -> None:
        promotion = function_text(ROMSTAGE, "b06ve_promote_post_minit")
        self.assertIn("b06vh_workspace_pattern_exact", promotion)
        self.assertIn("workspace_fnv", promotion)
        self.assertIn("workspace_canonical_fnv", promotion)
        self.assertNotIn("357dc65a", promotion.lower())
        self.assertNotIn("64e3c821", promotion.lower())
        self.assertNotIn("e20c406b", promotion.lower())

        probe_exception = between(
            promotion,
            "probe_status_admitted =",
            "b06v8_capture_qpi_tuple",
        )
        self.assertIn("X58_VENDOR_ERR_PLATFORM_STATE", probe_exception)
        self.assertIn("workspace_pattern_exact", probe_exception)
        self.assertIn("x58_b06vh_workspace_digest_pair_exact", probe_exception)

        exact_gate = promotion[promotion.index("if (minit_call_status") :]
        self.assertIn("workspace_pattern_exact", exact_gate)
        self.assertIn("x58_b06vh_workspace_digest_pair_exact", exact_gate)
        self.assertIn("post_minit_i801_exact", exact_gate)
        self.assertIn("x58_b06v6_handoff_record_success", exact_gate)

    def test_handoff_consumer_rechecks_the_same_disjoint_digest_pairs(self) -> None:
        consumer = function_text(HANDOFF, "x58_b06v6_raminit_result_is_exact")
        self.assertIn("x58_b06vh_workspace_digest_pair_exact", consumer)
        self.assertIn("result->workspace_fnv", consumer)
        self.assertIn("result->workspace_canonical_fnv", consumer)
        self.assertNotIn("357dc65a", consumer.lower())
        self.assertNotIn("64e3c821", consumer.lower())
        self.assertNotIn("e20c406b", consumer.lower())

    def test_observation_remains_terminal_before_primary_handoff(self) -> None:
        observe = function_text(ROMSTAGE, "b06vb_high_qpi_minit_observe")
        observation_at = observe.index("csi_profile == B06VG_CSI_OBSERVATION")
        terminal_at = observe.index("B06VG_OBSERVATION_MINIT_TERMINAL", observation_at)
        primary_at = observe.index("csi_profile != B06VG_CSI_PRIMARY", terminal_at)
        promotion_at = observe.index("b06ve_promote_post_minit", primary_at)
        branch = observe[observation_at : terminal_at + 128]

        self.assertIn("return b06v6_fallback", branch)
        self.assertNotIn("b06ve_promote_post_minit", branch)
        self.assertNotIn("x58_b06v6_handoff_record_success", branch)
        self.assertNotIn("B06V6_AUTO_READY", branch)
        self.assertLess(terminal_at, primary_at)
        self.assertLess(primary_at, promotion_at)

    def test_existing_destructive_tests_and_guard_finalize_precede_payload(self) -> None:
        postmem = function_text(HANDOFF, "platform_romstage_post_mem")
        lowmem_at = postmem.index(
            "b06v6_destructive_window_test(X58_B06VF_LOWMEM_BASE"
        )
        alias_at = postmem.index("b06v6_transactional_smoke", lowmem_at)
        cbmem_window_at = postmem.index(
            "b06v6_destructive_window_test(X58_B06V6_CBMEM_BASE", alias_at
        )
        object_window_at = postmem.index(
            "b06v6_destructive_window_test(X58_B06V6_OBJECT_BASE",
            cbmem_window_at,
        )
        handoff_write_at = postmem.index("handoff->magic", object_window_at)
        handoff_readback_at = postmem.index(
            "x58_b06v6_handoff_is_valid(handoff)", handoff_write_at
        )
        finalize_at = postmem.index(
            "x58_b06ve_finalize_persistent_guard()", handoff_readback_at
        )
        self.assertEqual(
            (
                lowmem_at,
                alias_at,
                cbmem_window_at,
                object_window_at,
                handoff_write_at,
                handoff_readback_at,
                finalize_at,
            ),
            tuple(
                sorted(
                    (
                        lowmem_at,
                        alias_at,
                        cbmem_window_at,
                        object_window_at,
                        handoff_write_at,
                        handoff_readback_at,
                        finalize_at,
                    )
                )
            ),
        )


if __name__ == "__main__":
    unittest.main()
