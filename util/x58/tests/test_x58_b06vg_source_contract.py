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
BOOTBLOCK = (BOARD / "bootblock.c").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
DEFCONFIG = (ROOT / "configs/x58-pro-e-b06vg.config").read_text()

B06VG_SYMBOL = "CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS"
B06VF_SYMBOL = "CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY"
B06VG_ID = "X58PROE-B06VG-COUPLED-MINIT-SEABIOS-20260905"
B06VF_ID = "X58PROE-B06VF-SEABIOS-ENTRY-PROBE-20260905"

PRIMARY = "primary"
OBSERVATION = "observation"
REJECTED = "rejected"
ALLOWED_PRE_A0 = (
    0x00017000,
    0x00017400,
    0x00017600,
    0x00017800,
    0x00017A00,
    0x00017C00,
)
CSI_PAIRS = ((0x08, 0x03E3D24E), (0x0C, 0x8B38506A))


def between(source: str, first: str, last: str) -> str:
    begin = source.index(first)
    end = source.index(last, begin)
    return source[begin:end]


def function_text(source: str, name: str) -> str:
    """Return one C function, including its signature and balanced body."""
    definition = re.search(
        rf"\b{re.escape(name)}\s*\([^;{{}}]*\)\s*\{{", source, re.DOTALL
    )
    if definition is None:
        raise ValueError(f"no definition for function {name}")
    name_at = definition.start()
    signature_at = source.rfind("\n", 0, name_at) + 1
    body_at = definition.end() - 1
    depth = 0

    for offset in range(body_at, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[signature_at:offset + 1]
    raise ValueError(f"unterminated function {name}")


def preprocessor_block(source: str, marker: str, start: int = 0) -> str:
    """Return a balanced #if block starting with the requested marker."""
    begin = source.index(marker, start)
    depth = 0
    position = begin

    while position < len(source):
        line_end = source.find("\n", position)
        if line_end < 0:
            line_end = len(source)
        line = source[position:line_end].lstrip()
        if re.match(r"#\s*(if|ifdef|ifndef)\b", line):
            depth += 1
        elif re.match(r"#\s*endif\b", line):
            depth -= 1
            if depth == 0:
                return source[begin:line_end]
        position = line_end + 1
    raise ValueError(f"unterminated preprocessor block {marker}")


def b06vg_profile_contract(
    saved_pre_a0: int,
    post_a0: int,
    post_9c: int,
    dynamic_byte: int,
    raw_digest: int,
) -> str:
    """Executable specification for the two deliberately non-overlapping paths."""
    pair_exact = (dynamic_byte, raw_digest) in CSI_PAIRS
    if (
        dynamic_byte == 0x08
        and raw_digest == 0x03E3D24E
        and saved_pre_a0 == 0x00017000
        and post_a0 == saved_pre_a0
        and post_9c == 0x00B00502
    ):
        return PRIMARY
    if (
        pair_exact
        and saved_pre_a0 in ALLOWED_PRE_A0
        and saved_pre_a0 != 0x00017000
        and post_a0 == saved_pre_a0
        and post_9c == 0x00A00502
    ):
        return OBSERVATION
    return REJECTED


class B06VGSourceContractTests(unittest.TestCase):
    def test_option_is_default_off_and_depends_only_on_b06vf_at_this_layer(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        self.assertIn("depends on X58_PRO_E_B06VF_SEABIOS_ENTRY", option)
        self.assertIn("default n", option)
        self.assertNotIn("default y", option)
        for fact in (
            "0x00017400",
            "0x00017800",
            "0x03e3d24e",
            "0x8b38506a",
            "one guarded MINIT",
            "SeaBIOS",
        ):
            self.assertIn(fact, option)

    def test_defconfig_selects_full_chain_and_existing_reduced_seabios(self) -> None:
        for symbol in (
            "CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF=y",
            "CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS=y",
            "CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE=y",
            "CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT=y",
            "CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF=y",
            "CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY=y",
            "CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS=y",
            "CONFIG_PAYLOAD_SEABIOS=y",
            "CONFIG_SEABIOS_STABLE=y",
        ):
            self.assertIn(symbol, DEFCONFIG)
        self.assertIn(
            'CONFIG_MAINBOARD_PART_NUMBER="X58 Pro-E B06VG coupled MINIT '
            'SeaBIOS probe"',
            DEFCONFIG,
        )
        self.assertIn('CONFIG_LOCALVERSION="x58-pro-e-b06vg"', DEFCONFIG)
        self.assertIn(
            'CONFIG_PAYLOAD_CONFIGFILE="$(top)/src/mainboard/'
            '$(MAINBOARDDIR)/config_seabios_b06vf"',
            DEFCONFIG,
        )
        self.assertNotIn("CONFIG_PAYLOAD_NONE=y", DEFCONFIG)

    def test_vg_identity_precedes_inherited_vf_identity_everywhere(self) -> None:
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON):
            self.assertIn(B06VG_ID, source)
            self.assertIn(B06VF_ID, source)
            self.assertLess(source.index(B06VG_ID), source.index(B06VF_ID))

        id_command = between(
            ROMSTAGE,
            'if (b06j_streq(argv[0], "id") && argc == 1)',
            'if (b06j_streq(argv[0], "unlock") && argc == 2)',
        )
        self.assertLess(
            id_command.index(B06VG_SYMBOL), id_command.index(B06VF_SYMBOL)
        )
        board_name = between(MAINBOARD, "\t.name =", ",\n#if")
        self.assertLess(
            board_name.index(B06VG_SYMBOL), board_name.index(B06VF_SYMBOL)
        )
        self.assertIn("experimental B06VG coupled MINIT/SeaBIOS gate", board_name)

        defaults = KCONFIG[KCONFIG.index("config MAINBOARD_PART_NUMBER"):]
        self.assertLess(defaults.index("B06VG"), defaults.index("B06VF"))

    def test_executable_profile_spec_accepts_only_the_primary_tuple(self) -> None:
        self.assertEqual(
            b06vg_profile_contract(
                0x00017000, 0x00017000, 0x00B00502, 0x08, 0x03E3D24E
            ),
            PRIMARY,
        )
        for dynamic_byte, raw_digest in CSI_PAIRS:
            for post_9c in (0x00A00502, 0x00B00502):
                if (dynamic_byte, raw_digest, post_9c) == (
                    0x08,
                    0x03E3D24E,
                    0x00B00502,
                ):
                    continue
                self.assertEqual(
                    b06vg_profile_contract(
                        0x00017000,
                        0x00017000,
                        post_9c,
                        dynamic_byte,
                        raw_digest,
                    ),
                    REJECTED,
                )

    def test_executable_profile_spec_accepts_each_nonprimary_observation(self) -> None:
        for saved_pre_a0 in ALLOWED_PRE_A0[1:]:
            for dynamic_byte, raw_digest in CSI_PAIRS:
                self.assertEqual(
                    b06vg_profile_contract(
                        saved_pre_a0,
                        saved_pre_a0,
                        0x00A00502,
                        dynamic_byte,
                        raw_digest,
                    ),
                    OBSERVATION,
                )

    def test_executable_profile_spec_rejects_crossed_or_broadened_inputs(self) -> None:
        invalid = (
            # Cross the two observed byte/raw pairs.
            (0x00017800, 0x00017800, 0x00A00502, 0x08, 0x8B38506A),
            (0x00017800, 0x00017800, 0x00A00502, 0x0C, 0x03E3D24E),
            # Do not infer a range or masked field from the finite observations.
            (0x00017200, 0x00017200, 0x00A00502, 0x08, 0x03E3D24E),
            (0x00017E00, 0x00017E00, 0x00A00502, 0x0C, 0x8B38506A),
            # The observation must preserve pre-A0 and use the A endpoint.
            (0x00017800, 0x00017A00, 0x00A00502, 0x0C, 0x8B38506A),
            (0x00017800, 0x00017800, 0x00B00502, 0x0C, 0x8B38506A),
            # The primary endpoint cannot borrow an observation pre-state.
            (0x00017800, 0x00017000, 0x00B00502, 0x08, 0x03E3D24E),
        )
        for values in invalid:
            self.assertEqual(b06vg_profile_contract(*values), REJECTED)

    def test_six_value_allowlist_is_literal_and_has_no_range_or_mask(self) -> None:
        helper = function_text(
            VENDOR_HEADER, "x58_vendor_b06vg_high_qpi_cpu_a0_exact"
        )
        for value in ALLOWED_PRE_A0:
            self.assertIn(f"0x{value:08x}u", helper)
        for rejected in (0x00017200, 0x00017E00):
            self.assertNotIn(f"0x{rejected:08x}u", helper)
        self.assertNotIn("GENMASK", helper)
        self.assertNotIn("_MASK", helper)
        self.assertNotRegex(helper, r"\bvalue\s*(?:<=|>=|<|>)")
        self.assertNotRegex(helper, r"\bvalue\s*&")

    def test_pre_a0_is_sampled_twice_and_stably_saved_before_csi(self) -> None:
        capture = function_text(ROMSTAGE, "b06vg_capture_stable_pre_a0")
        self.assertIn("pre_a0_first", capture)
        self.assertIn("pre_a0_second", capture)
        self.assertGreaterEqual(capture.count("0xa0, 4"), 2)
        self.assertIn("pre_a0_first != pre_a0_second", capture)
        self.assertIn("x58_vendor_b06vg_high_qpi_cpu_a0_exact", capture)
        self.assertIn("*saved_pre_a0", capture)

        probe = function_text(ROMSTAGE, "b06v8_high_qpi_probe")
        capture_at = probe.index("b06vg_capture_stable_pre_a0")
        csi_at = probe.index("x58_vendor_b06v8_select_high_qpi_csi", capture_at)
        observe_at = probe.index("b06vb_high_qpi_minit_observe", csi_at)
        self.assertLess(capture_at, csi_at)
        self.assertLess(csi_at, observe_at)
        self.assertIn("saved_pre_a0", probe[capture_at:observe_at + 256])

    def test_c_classifier_contains_two_disjoint_exact_profiles(self) -> None:
        enum_block = between(
            ROMSTAGE, "enum b06vg_csi_profile {", "};"
        )
        for member in (
            "B06VG_CSI_REJECTED",
            "B06VG_CSI_PRIMARY",
            "B06VG_CSI_OBSERVATION",
        ):
            self.assertIn(member, enum_block)

        selector = function_text(ROMSTAGE, "b06vg_select_csi_profile")
        for gate in (
            "saved_pre_a0",
            "tuple->cpu_a0",
            "tuple->cpu_9c",
            "0x00017000",
            "0x00b00502",
            "0x00a00502",
            "B06VD_CSI_RAW_08_FNV",
            "B06VD_CSI_RAW_0C_FNV",
            "B06VG_CSI_PRIMARY",
            "B06VG_CSI_OBSERVATION",
            "B06VG_CSI_REJECTED",
        ):
            self.assertIn(gate, selector)
        self.assertIn("tuple->cpu_a0 != saved_pre_a0", selector)
        self.assertIn("saved_pre_a0 == 0x00017000", selector)
        self.assertIn("saved_pre_a0 != 0x00017000", selector)
        self.assertIn("x58_vendor_b06vg_high_qpi_cpu_a0_exact", selector)
        self.assertIn("pair_08 = state[B06VD_CSI_DYNAMIC_OFFSET] == 0x08", selector)
        self.assertIn("raw_digest == B06VD_CSI_RAW_08_FNV", selector)
        self.assertIn("pair_0c = state[B06VD_CSI_DYNAMIC_OFFSET] == 0x0c", selector)
        self.assertIn("raw_digest == B06VD_CSI_RAW_0C_FNV", selector)
        self.assertIn("if (!pair_08 && !pair_0c)", selector)

        primary = between(
            selector,
            "if (saved_pre_a0 == 0x00017000",
            "if (saved_pre_a0 != 0x00017000",
        )
        observation = selector[selector.index("if (saved_pre_a0 != 0x00017000"):]
        self.assertIn("pair_08", primary)
        self.assertNotIn("pair_0c", primary)
        self.assertIn("(pair_08 || pair_0c)", observation)

    def test_observation_calls_guarded_minit_but_cannot_promote(self) -> None:
        observe = function_text(ROMSTAGE, "b06vb_high_qpi_minit_observe")
        self.assertEqual(observe.count("x58_vendor_call_minit("), 1)
        self.assertIn("csi_info->b06vg_pre_csi_cpu_a0_valid", observe)
        self.assertIn("csi_info->b06vg_pre_csi_cpu_a0 != saved_pre_a0", observe)
        selector_at = observe.index("b06vg_select_csi_profile")
        accept_at = observe.index("x58_vendor_accept_csi_result", selector_at)
        authorize_at = observe.index(
            "x58_vendor_b06vb_authorize_high_qpi_minit", accept_at
        )
        arm_at = observe.index("x58_vendor_arm_minit", authorize_at)
        call_at = observe.index("x58_vendor_call_minit", arm_at)
        terminal_at = observe.index("B06VG_OBSERVATION_MINIT_TERMINAL", call_at)
        promote_at = observe.index("b06ve_promote_post_minit", terminal_at)
        ordered = (
            selector_at,
            accept_at,
            authorize_at,
            arm_at,
            call_at,
            terminal_at,
            promote_at,
        )
        self.assertEqual(ordered, tuple(sorted(ordered)))

        observation_branch_at = observe.rfind(
            "B06VG_CSI_OBSERVATION", call_at, terminal_at
        )
        self.assertGreaterEqual(observation_branch_at, 0)
        observation_branch = observe[observation_branch_at:terminal_at + 96]
        self.assertIn("return b06v6_fallback", observation_branch)
        self.assertNotIn("b06ve_promote_post_minit", observation_branch)
        self.assertNotIn("B06V6_AUTO_READY", observation_branch)
        self.assertNotIn("x58_b06v6_handoff_record_success", observation_branch)

        primary_tail = observe[terminal_at:promote_at + 96]
        self.assertIn("B06VG_CSI_PRIMARY", primary_tail)
        self.assertNotIn("B06VG_CSI_OBSERVATION", primary_tail)

    def test_vendor_post_csi_gate_keeps_exact_pairs_and_endpoints(self) -> None:
        gate = function_text(VENDOR_INIT, "b06vb_high_qpi_post_csi_platform_exact")
        pair_gate = function_text(VENDOR_INIT, "b06vd_csi_digest_pair_exact")
        for value in (
            "X58_VENDOR_B06VD_CSI_RAW_08_FNV1A",
            "X58_VENDOR_B06VD_CSI_RAW_0C_FNV1A",
        ):
            self.assertIn(value, pair_gate)
        for value in (
            "0x00b00502u",
            "0x00a00502u",
            "0x00017000u",
        ):
            self.assertIn(value, gate)
        self.assertIn("x58_vendor_b06vg_high_qpi_cpu_a0_exact", gate)
        self.assertNotIn("GENMASK", gate)

    def test_predecessor_primary_promotion_contract_remains_present(self) -> None:
        promotion = function_text(ROMSTAGE, "b06ve_promote_post_minit")
        for gate in (
            "b06ve_post_minit_tuple_exact",
            "x58_b06v6_handoff_record_success",
            "B06V6_AUTO_READY",
            "B06VF_POST_MINIT_CANONICAL_EXACT_GATE",
        ):
            self.assertIn(gate, promotion)
        self.assertNotIn("B06VG_CSI_OBSERVATION", promotion)


if __name__ == "__main__":
    unittest.main()
