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
HANDOFF_HEADER = (BOARD / "b06v6_handoff.h").read_text()
HANDOFF = (BOARD / "b06v6_handoff.c").read_text()
BOOTBLOCK = (BOARD / "bootblock.c").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
DEFCONFIG = (ROOT / "configs/x58-pro-e-b06ve.config").read_text()

B06VE_SYMBOL = "CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF"
B06VE_ID = "X58PROE-B06VE-HIGHQPI-AUTO-RAMSTAGE-ROMMON-20260905"


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


def b06ve_promotion_function() -> str:
    """Find the B06VE-only helper which records the exact RAM-init result."""
    candidates = re.findall(r"\b(b06ve_[a-zA-Z0-9_]+)\s*\(", ROMSTAGE)
    for name in dict.fromkeys(candidates):
        try:
            implementation = function_text(ROMSTAGE, name)
        except (ValueError, IndexError):
            continue
        if "x58_b06v6_handoff_record_success" in implementation:
            return implementation
    raise AssertionError("no B06VE result-promotion helper records the handoff")


class B06VESourceContractTests(unittest.TestCase):
    def test_option_is_default_off_and_narrowly_inherits_b06vd(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        self.assertIn(
            "depends on X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT", option
        )
        self.assertIn("default n", option)
        self.assertIn("only CSI byte 0x2a6=0x08", option)
        self.assertIn("0x0c/", option)
        self.assertIn("diagnostic fallback", option)
        self.assertIn("version-3 CBMEM handoff", option)
        self.assertIn("does not enable normal PCI enumeration", option)
        self.assertIn("or a payload", option)

    def test_defconfig_selects_full_chain_and_has_no_payload(self) -> None:
        for symbol in (
            "CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF=y",
            "CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS=y",
            "CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE=y",
            "CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT=y",
            "CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF=y",
        ):
            self.assertIn(symbol, DEFCONFIG)
        self.assertIn("CONFIG_X58_PRO_E_BRINGUP_STAGE=11", DEFCONFIG)
        self.assertIn(
            'CONFIG_MAINBOARD_PART_NUMBER="X58 Pro-E B06VE High-QPI '
            'auto RAM-stage ROMMON"',
            DEFCONFIG,
        )
        self.assertIn('CONFIG_LOCALVERSION="x58-pro-e-b06ve"', DEFCONFIG)
        self.assertIn("CONFIG_PAYLOAD_NONE=y", DEFCONFIG)
        self.assertNotRegex(DEFCONFIG, r"CONFIG_PAYLOAD_(SEA|EDK2|ELF)=y")

    def test_exact_identity_precedes_every_inherited_identity(self) -> None:
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON):
            self.assertIn(B06VE_ID, source)
        self.assertLess(BOOTBLOCK.index(B06VE_ID), BOOTBLOCK.index("X58PROE-B06VD"))
        self.assertLess(ROMSTAGE.index(B06VE_ID), ROMSTAGE.index("X58PROE-B06VD"))
        self.assertLess(RAMMON.index(B06VE_ID), RAMMON.index("X58PROE-B06V7"))

        id_command = between(
            ROMSTAGE,
            'if (b06j_streq(argv[0], "id") && argc == 1)',
            'if (b06j_streq(argv[0], "unlock") && argc == 2)',
        )
        self.assertIn(B06VE_SYMBOL, id_command)
        self.assertIn(B06VE_ID, id_command)
        self.assertLess(id_command.index(B06VE_SYMBOL), id_command.index("B06VD"))
        self.assertIn("experimental B06VE High-QPI DRAM ROMMON", MAINBOARD)

    def test_handoff_v3_has_new_field_and_separate_required_flag(self) -> None:
        version = preprocessor_block(
            HANDOFF_HEADER, f"#if {B06VE_SYMBOL}"
        )
        self.assertIn("#define X58_B06V6_HANDOFF_VERSION\t3u", version)

        result = between(
            HANDOFF_HEADER,
            "struct x58_b06v6_raminit_result {",
            "struct x58_b06v6_handoff {",
        )
        self.assertIn(B06VE_SYMBOL, result)
        self.assertIn("uint32_t post_minit_ioh_stage_9c;", result)

        flags = between(
            HANDOFF_HEADER,
            "enum x58_b06v6_handoff_flags {",
            "};",
        )
        self.assertIn(
            "X58_B06VE_HANDOFF_HIGH_QPI_POST_MINIT = 1u << 5", flags
        )
        common = between(
            HANDOFF,
            "#define B06V6_COMMON_REQUIRED_FLAGS",
            f"#if {B06VE_SYMBOL}",
        )
        self.assertNotIn("X58_B06VE_HANDOFF_HIGH_QPI_POST_MINIT", common)
        required = preprocessor_block(
            HANDOFF, f"#if {B06VE_SYMBOL}", HANDOFF.index(common)
        )
        self.assertIn("B06V6_COMMON_REQUIRED_FLAGS", required)
        self.assertIn("X58_B06VE_HANDOFF_HIGH_QPI_POST_MINIT", required)
        validator = function_text(HANDOFF, "x58_b06v6_handoff_is_valid")
        self.assertIn("handoff->flags != B06V6_REQUIRED_FLAGS", validator)

    def test_exact_high_qpi_result_values_are_handoff_gates(self) -> None:
        for definition in (
            "X58_B06VE_EXPECTED_CSI_FNV\t0x03e3d24eu",
            "X58_B06VE_EXPECTED_CSI_CANONICAL_FNV\t0x908dabb6u",
            "X58_B06VE_EXPECTED_WORKSPACE_FNV\t0x94299f43u",
            "X58_B06VE_EXPECTED_QPI_STATUS\t0x070f0f03u",
            "X58_B06VE_EXPECTED_POST_MINIT_IOH_STAGE_9C\t0xbf000000u",
        ):
            self.assertIn(definition, HANDOFF_HEADER)

        exact = function_text(HANDOFF, "x58_b06v6_raminit_result_is_exact")
        for gate in (
            "result->csi_state_fnv == X58_B06VE_EXPECTED_CSI_FNV",
            "X58_B06VE_EXPECTED_CSI_CANONICAL_FNV",
            "result->workspace_fnv == X58_B06VE_EXPECTED_WORKSPACE_FNV",
            "result->qpi_status == X58_B06VE_EXPECTED_QPI_STATUS",
            "result->post_minit_ioh_stage_9c ==",
            "X58_B06VE_EXPECTED_POST_MINIT_IOH_STAGE_9C",
        ):
            self.assertIn(gate, exact)

    def test_post_minit_vendor_gate_is_distinct_and_requires_bf_stage(self) -> None:
        predicate = function_text(
            VENDOR_INIT, "b06ve_high_qpi_post_minit_platform_exact"
        )
        for exact_value in (
            "0x160c0112u",
            "0x0040a0a8u",
            "0x070f0f03u",
            "0x00b00502u",
            "0x00017000u",
            "0x0616fc00u",
            "0xbf000000u",
            "X58_VENDOR_MEMORY_CLOCK_STATE",
            "X58_VENDOR_MEMORY_RATIO_STATE",
        ):
            self.assertIn(exact_value, predicate)
        self.assertNotIn("0xea000000u", predicate)

        public_probe = function_text(
            VENDOR_INIT, "x58_vendor_b06ve_post_minit_probe"
        )
        self.assertIn("common_preflight()", public_probe)
        self.assertIn("b06ve_high_qpi_post_minit_platform_exact()", public_probe)
        self.assertIn("x58_vendor_b06ve_post_minit_probe", VENDOR_HEADER)

        platform_gate = function_text(VENDOR_INIT, "csi_platform_gate")
        minit = platform_gate.index("runtime.minit_returned")
        post_minit = platform_gate.index(
            "b06ve_high_qpi_post_minit_platform_exact", minit
        )
        post_csi = platform_gate.index("runtime.csi_returned", post_minit)
        self.assertLess(minit, post_minit)
        self.assertLess(post_minit, post_csi)

    def test_0c_pair_is_rejected_before_accept_or_minit(self) -> None:
        selector = function_text(
            ROMSTAGE, "b06vd_select_expected_csi_raw_digest"
        )
        self.assertIn("state[B06VD_CSI_DYNAMIC_OFFSET] == 0x0c", selector)
        self.assertIn("B06VD_CSI_RAW_0C_FNV", selector)

        observe = function_text(ROMSTAGE, "b06vb_high_qpi_minit_observe")
        accept = observe.index("x58_vendor_accept_csi_result(")
        ve_gate_at = observe.rfind(f"#if {B06VE_SYMBOL}", 0, accept)
        self.assertGreaterEqual(ve_gate_at, 0)
        ve_gate = preprocessor_block(observe, f"#if {B06VE_SYMBOL}", ve_gate_at)
        self.assertLess(ve_gate_at + len(ve_gate), accept)
        self.assertIn("csi_state[B06VD_CSI_DYNAMIC_OFFSET] != 0x08", ve_gate)
        self.assertIn("expected_csi_raw_fnv != B06VD_CSI_RAW_08_FNV", ve_gate)
        self.assertIn("b06v6_fallback", ve_gate)

        authorize = observe.index("x58_vendor_b06vb_authorize_high_qpi_minit(")
        arm = observe.index("x58_vendor_arm_minit(", authorize)
        call = observe.index("x58_vendor_call_minit(", arm)
        self.assertLess(ve_gate_at, accept)
        self.assertLess(accept, authorize)
        self.assertLess(authorize, arm)
        self.assertLess(arm, call)

    def test_auto_promotion_exists_only_in_the_b06ve_conditional_path(self) -> None:
        observe = function_text(ROMSTAGE, "b06vb_high_qpi_minit_observe")
        ve_tail_at = observe.rfind(f"#if {B06VE_SYMBOL}")
        self.assertGreaterEqual(ve_tail_at, 0)
        ve_tail = preprocessor_block(observe, f"#if {B06VE_SYMBOL}", ve_tail_at)
        b06ve_calls = re.findall(r"\b(b06ve_[a-zA-Z0-9_]+)\s*\(", ve_tail)
        self.assertTrue(b06ve_calls, "B06VE tail has no isolated promotion helper")

        promotion = b06ve_promotion_function()
        self.assertIn("x58_b06v6_handoff_record_success", promotion)
        self.assertIn("B06V6_AUTO_READY", promotion)
        ve_contract = (
            observe
            + promotion
            + function_text(ROMSTAGE, "b06ve_post_minit_tuple_exact")
        )
        self.assertIn("x58_vendor_b06ve_post_minit_probe", ve_contract)
        for expected in (
            "X58_B06VE_EXPECTED_CSI_FNV",
            "X58_B06VE_EXPECTED_CSI_CANONICAL_FNV",
            "X58_B06VE_EXPECTED_WORKSPACE_FNV",
            "X58_B06VE_EXPECTED_QPI_STATUS",
            "X58_B06VE_EXPECTED_POST_MINIT_IOH_STAGE_9C",
        ):
            self.assertIn(expected, ve_contract)

        terminal = observe.index(
            'return b06v6_fallback("B06VB_MINIT_OBSERVATION_TERMINAL")'
        )
        self.assertGreater(terminal, ve_tail_at + len(ve_tail))
        self.assertNotIn("x58_b06v6_handoff_record_success", observe)
        self.assertNotIn("B06V6_AUTO_READY", observe)

    def test_persistent_guard_is_finalized_only_after_dram_and_cbmem(self) -> None:
        postmem = function_text(HANDOFF, "platform_romstage_post_mem")
        ordered = (
            "b06v6_mtrr_state_is_exact",
            "b06v6_transactional_smoke",
            "b06v6_destructive_window_test(X58_B06V6_CBMEM_BASE",
            "b06v6_destructive_window_test(X58_B06V6_OBJECT_BASE",
            "cbmem_initialize_empty_id_size",
            "x58_b06v6_handoff_is_valid(handoff)",
            "x58_b06ve_finalize_persistent_guard()",
        )
        positions = [postmem.index(item) for item in ordered]
        self.assertEqual(positions, sorted(positions))
        self.assertEqual(
            HANDOFF.count("x58_b06ve_finalize_persistent_guard()"), 1
        )
        finalize = postmem.index("x58_b06ve_finalize_persistent_guard()")
        self.assertNotIn("x58_b06v6_handoff_is_valid", postmem[finalize:])

        finalizer = function_text(
            ROMSTAGE, "x58_b06ve_finalize_persistent_guard"
        )
        self.assertIn(
            "b06v6_set_i801_signature(&b06v6_clear_signature)", finalizer
        )
        self.assertIn(
            "b06v6_write_cmos_diagnostic(B06V8_CMOS_COLD_AUTHORIZATION)",
            finalizer,
        )
        self.assertIn("b06v6_i801_signature_matches", finalizer)
        self.assertIn("b06v6_read_cmos_diagnostic", finalizer)

        promotion = b06ve_promotion_function()
        self.assertNotIn("x58_b06ve_finalize_persistent_guard", promotion)
        self.assertNotIn("b06v6_clear_signature", promotion)
        self.assertNotIn("B06V8_CMOS_COLD_AUTHORIZATION", promotion)


if __name__ == "__main__":
    unittest.main()
