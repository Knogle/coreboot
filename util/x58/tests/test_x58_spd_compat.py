#!/usr/bin/env python3
"""Host-only tests of the production SPD admission and CBMEM wire contracts.

Compile the real header helpers and consumer function bodies, without building
firmware or accessing hardware. Captured SPD identity remains separate from
the serial-neutral compatibility profile; this is not general DIMM support.
"""

import re
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path
from x58_test_paths import COREBOOT_ROOT as COREBOOT


ROOT = Path(__file__).resolve().parents[1]
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
HANDOFF = (BOARD / "b06v6_handoff.c").read_text()
ROMSTAGE = (BOARD / "romstage.c").read_text()

# Public synthetic fixture: supported SPD layout, with invented serial bytes
# 01 02 03 04. Its replacement differs only at byte 125 (04 -> 05). This is
# not a raw hardware capture. Historical fingerprint-only legacy gate tests
# below remain separate from the synthetic fixture's raw FNV values.
ORIGINAL_SPD = bytes.fromhex("""
    93130b0203190009035201080a00fe00
    5a785a305a10f07200053c3c00f08205
    00000000000000000008000000000000
    0000000000000000000000000f110101
    00000000000000000000000000000000
    00000000000000000000000000000000
    00000000000000000000000000000000
    0000000000859b080000010203044fec
    424c5334473344313630394453315330
    302e0100802c00000000000000000000
    00000000000000000000000000000000
    0c4a011301080000112a0a5afe005a5a
    5a7810f0723f0000053c3000f03c0000
    10002600000000000000000000000000
    00000000000000000000000000000000
    00000000000000000000000000000000
""")
SPD_FIXTURE = (
    "static const uint8_t original_spd[] = {"
    + ",".join(f"0x{value:02x}" for value in ORIGINAL_SPD)
    + "};\n_Static_assert(sizeof(original_spd) == 256, \"complete fixture\");\n"
)
C_INCLUDES = """
    #include <assert.h>
    #include <stdbool.h>
    #include <stddef.h>
    #include <stdint.h>
    #include <stdio.h>
    #include <string.h>
"""

# Each row adds the feature that historically changed the CBMEM wire version.
LEGACY_FEATURES = (
    "B06V7_ROBUST_SLOWQPI",
    "B06VE_HIGH_QPI_AUTO_HANDOFF",
    "B06VF_SEABIOS_ENTRY",
    "B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS",
    "B06VI_COUPLED_PROFILE_O_SEABIOS",
    "B06VJ_COUPLED_PROFILE_Q_SEABIOS",
    "B06VK_Q_CANONICAL_CLASS_SEABIOS",
    "B06VL_BROAD_HARD_GATE_SEABIOS",
)


def function_text(source: str, name: str) -> str:
    """Extract an actual, brace-balanced C definition, as in nearby tests."""
    definition = re.search(
        rf"\b{re.escape(name)}\s*\([^;{{}}]*\)\s*\{{", source, re.DOTALL
    )
    if definition is None:
        raise ValueError(f"no definition for function {name}")
    begin = source.rfind("\n", 0, definition.start()) + 1
    depth = 0
    for offset in range(definition.end() - 1, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[begin : offset + 1] + "\n"
    raise ValueError(f"unterminated function {name}")


def config_header(serial_independent: bool, legacy_version: int = 9) -> str:
    lines = [
        f"#define CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT {int(serial_independent)}"
    ]
    lines.extend(
        f"#define CONFIG_X58_PRO_E_{feature} {int(index < legacy_version - 1)}"
        for index, feature in enumerate(LEGACY_FEATURES)
    )
    return C_INCLUDES + "\n".join(lines) + '\n#include "b06v6_handoff.h"\n'


def compile_and_run(source: str) -> subprocess.CompletedProcess[str]:
    with tempfile.TemporaryDirectory(prefix="x58-spd-compat-", dir="/tmp") as temp:
        executable = Path(temp) / "contract"
        compiled = subprocess.run(
            ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(BOARD),
             "-x", "c", "-", "-o", str(executable)],
            input=source, text=True, capture_output=True, check=False, timeout=30,
        )
        if compiled.returncode:
            raise AssertionError(
                f"Host C compile failed:\n{compiled.stdout}\n{compiled.stderr}"
            )
        return subprocess.run(
            [str(executable)], text=True, capture_output=True, check=False, timeout=30,
        )


def consumer_harness(serial_independent: bool) -> str:
    """Use the real hash, RAM-init result gate and complete CBMEM validator."""
    source = config_header(serial_independent) + textwrap.dedent(r'''
        #define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
        #define B06V6_EXPECTED_MTRR_CAP 0x00000d0au
        #define B06V6_EXPECTED_MTRR_DEF_TYPE 0x00000800u
        #define B06V6_REQUIRED_FLAGS \
            (X58_B06V6_HANDOFF_EXACT_MINIT | X58_B06V6_HANDOFF_MTRR_GATED | \
             X58_B06V6_HANDOFF_CBMEM_SMOKED | X58_B06V6_HANDOFF_OBJECT_SMOKED | \
             X58_B06V6_HANDOFF_CBMEM_READY | X58_B06VE_HANDOFF_HIGH_QPI_POST_MINIT | \
             X58_B06VF_HANDOFF_LOWMEM_SMOKED | X58_B06VL_HANDOFF_BROAD_POST_MINIT)
    ''')
    source += "\n".join(function_text(HANDOFF, name) for name in (
        "fnv1a32", "x58_b06v6_handoff_digest",
        "x58_b06v6_raminit_result_is_exact", "x58_b06v6_handoff_is_valid",
    ))
    return source + textwrap.dedent(r'''
        static void make_valid(struct x58_b06v6_handoff *h)
        {
            memset(h, 0, sizeof(*h));
            h->magic = X58_B06V6_HANDOFF_MAGIC;
            h->version = X58_B06V6_HANDOFF_VERSION;
            h->structure_size = sizeof(*h);
            h->flags = B06V6_REQUIRED_FLAGS;
            h->raminit.cpuid = X58_B06V6_EXPECTED_CPUID;
            h->raminit.microcode_revision = X58_B06V6_EXPECTED_UCODE;
            h->raminit.spd_fnv = 0xfb66b530u;
        #if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
            h->raminit.spd_profile_fnv = 0x5194e521u;
        #endif
            h->raminit.csi_state_fnv = X58_B06VE_EXPECTED_CSI_FNV;
            h->raminit.policy_fnv = X58_B06V6_EXPECTED_POLICY_FNV;
            h->raminit.workspace_fnv = 0x12345678u;
            h->raminit.csi_state_canonical_fnv = X58_B06VE_EXPECTED_CSI_CANONICAL_FNV;
            h->raminit.workspace_canonical_fnv = 0x9abcdef0u;
            h->raminit.mc_mapper = X58_B06V6_EXPECTED_MC_MAPPER;
            h->raminit.mc_common_f8 = X58_B06V6_EXPECTED_MC_COMMON_F8;
            h->raminit.ch2_dod = X58_B06V6_EXPECTED_CH2_DOD;
            h->raminit.ch2_ranks = X58_B06V6_EXPECTED_CH2_RANKS;
            h->raminit.ch2_status = X58_B06V6_EXPECTED_CH2_STATUS;
            h->raminit.qpi_status = X58_B06VE_EXPECTED_QPI_STATUS;
            h->raminit.post_minit_ioh_stage_9c = X58_B06VE_EXPECTED_POST_MINIT_IOH_STAGE_9C;
            h->raminit.profile_id = X58_B06VL_PROFILE_BROAD;
            h->raminit.saved_pre_a0 = 0x00017200u;
            h->raminit.post_minit_cpu_a0 = 0x00017200u;
            h->raminit.post_minit_cpu_9c = 0x00b00502u;
            h->mtrr_cap = B06V6_EXPECTED_MTRR_CAP;
            h->mtrr_def_type = B06V6_EXPECTED_MTRR_DEF_TYPE;
            h->cbmem_base = X58_B06V6_CBMEM_BASE;
            h->cbmem_top = X58_B06V6_CBMEM_TOP;
            h->object_base = X58_B06V6_OBJECT_BASE;
            h->object_size = X58_B06V6_OBJECT_SIZE;
            h->object_max_size = X58_B06V6_OBJECT_MAX_SIZE;
            h->digest = x58_b06v6_handoff_digest(h);
        }
    ''')


class SPDCompatibilityTests(unittest.TestCase):
    def assert_harness_passes(self, source: str) -> None:
        result = compile_and_run(source)
        self.assertEqual(
            result.returncode, 0,
            f"Host C exit={result.returncode}\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}",
        )

    def test_synthetic_raw_identities_share_only_the_compatibility_digest(self) -> None:
        source = C_INCLUDES + '#include "spd_profile.h"\n' + SPD_FIXTURE
        source += function_text(HANDOFF, "fnv1a32")
        source += r'''
            int main(void)
            {
                uint8_t replacement[256], before[256];
                _Static_assert(X58_SPD_PROFILE_FNV == 0x5194e521u, "known profile");
                assert(fnv1a32(original_spd, 256) == 0x815f5091u);
                assert(x58_spd_profile_digest(original_spd, 256) == 0x5194e521u);
                memcpy(replacement, original_spd, 256);
                replacement[125] = 0x05;
                memcpy(before, replacement, 256);
                assert(fnv1a32(replacement, 256) == 0xe3b887eeu);
                assert(x58_spd_profile_digest(replacement, 256) == 0x5194e521u);
                assert(memcmp(replacement, before, 256) == 0);
                assert(fnv1a32(replacement, 256) == 0xe3b887eeu);
                assert(memcmp(original_spd + 122, "\x01\x02\x03\x04", 4) == 0);
                assert(memcmp(replacement + 122, "\x01\x02\x03\x05", 4) == 0);
                return 0;
            }
        '''
        self.assert_harness_passes(source)

    def test_exactly_four_serial_bytes_are_ignored_without_modifying_input(self) -> None:
        self.assert_harness_passes(
            C_INCLUDES + '#include "spd_profile.h"\n' + SPD_FIXTURE + r'''
            int main(void)
            {
                uint8_t candidate[256], before[256];
                /* All 256 values at each of the four serial offsets. */
                for (size_t offset = 122; offset <= 125; offset++) {
                    for (unsigned value = 0; value <= 255; value++) {
                        memcpy(candidate, original_spd, 256);
                        candidate[offset] = value;
                        memcpy(before, candidate, 256);
                        assert(x58_spd_profile_digest(candidate, 256) == 0x5194e521u);
                        assert(memcmp(candidate, before, 256) == 0);
                    }
                }
                /* Includes boundary bytes 121/126, geometry, CRC, part and XMP. */
                for (size_t offset = 0; offset < 256; offset++) {
                    if (offset >= 122 && offset <= 125)
                        continue;
                    for (unsigned bit = 0; bit < 8; bit++) {
                        memcpy(candidate, original_spd, 256);
                        candidate[offset] ^= 1u << bit;
                        memcpy(before, candidate, 256);
                        assert(x58_spd_profile_digest(candidate, 256) != 0x5194e521u);
                        assert(memcmp(candidate, before, 256) == 0);
                    }
                }
                return 0;
            }
        ''')

    def test_null_and_every_non_full_length_fail_closed(self) -> None:
        self.assert_harness_passes(
            C_INCLUDES + '#include "spd_profile.h"\n' + SPD_FIXTURE + r'''
            int main(void)
            {
                for (size_t size = 0; size <= 512; size++) {
                    assert(x58_spd_profile_digest(NULL, size) == 0);
                    if (size != 256)
                        assert(x58_spd_profile_digest(original_spd, size) == 0);
                }
                assert(x58_spd_profile_digest(NULL, SIZE_MAX) == 0);
                assert(x58_spd_profile_digest(original_spd, SIZE_MAX) == 0);
                return 0;
            }
        ''')

    def test_romstage_validity_gate_and_dirty_invalidation_in_both_modes(self) -> None:
        state_stub = r'''
            struct b06j_state {
                bool vendor_spd_valid;
                uint32_t vendor_spd_digest;
            #if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
                uint32_t vendor_spd_profile_digest;
            #endif
                bool vendor_external_write, vendor_pciexbar_ready;
                bool vendor_policy_ready, vendor_policy_modified;
            };
        '''
        for enabled in (False, True):
            with self.subTest(serial_independent=enabled):
                source = config_header(enabled) + state_stub
                source += function_text(ROMSTAGE, "b06v6_spd_is_exact")
                source += function_text(ROMSTAGE, "b06v0_mark_vendor_state_dirty")
                source += r'''
                    int main(void)
                    {
                        struct b06j_state state = { 0 };
                        state.vendor_spd_digest = 0xfb66b530u;
                    #if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
                        state.vendor_spd_profile_digest = 0x5194e521u;
                    #endif
                        assert(!b06v6_spd_is_exact(&state));
                        state.vendor_spd_valid = true;
                        assert(b06v6_spd_is_exact(&state));
                        state.vendor_spd_digest = 0x3f5e3f88u;
                        assert(b06v6_spd_is_exact(&state) ==
                               !!CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT);
                    #if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
                        state.vendor_spd_digest = 0xfb66b530u;
                        state.vendor_spd_profile_digest ^= 1u;
                        assert(!b06v6_spd_is_exact(&state));
                        state.vendor_spd_profile_digest = 0;
                        assert(!b06v6_spd_is_exact(&state));
                        state.vendor_spd_profile_digest = 0x5194e521u;
                    #endif
                        state.vendor_spd_digest = 0xfb66b530u;
                        assert(b06v6_spd_is_exact(&state));
                        b06v0_mark_vendor_state_dirty(&state);
                        assert(state.vendor_external_write);
                        assert(!state.vendor_spd_valid && state.vendor_spd_digest == 0);
                    #if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
                        assert(state.vendor_spd_profile_digest == 0);
                    #endif
                        assert(!b06v6_spd_is_exact(&state));
                        return 0;
                    }
                '''
                self.assert_harness_passes(source)

    def test_legacy_v1_through_v9_layouts_and_raw_only_gates_are_preserved(self) -> None:
        wire_sizes = (132, 140, 144, 144, 144, 160, 160, 160, 160)
        for version, size in enumerate(wire_sizes, start=1):
            with self.subTest(version=version):
                source = config_header(False, version)
                source += (
                    f'_Static_assert(X58_B06V6_HANDOFF_VERSION == {version}, "legacy version");\n'
                    f'_Static_assert(sizeof(struct x58_b06v6_handoff) == {size}, "legacy size");\n'
                )
                source += r'''
                    int main(void)
                    {
                        assert(x58_b06v6_spd_digest_is_exact(0xfb66b530u, 0x5194e521u));
                        assert(x58_b06v6_spd_digest_is_exact(0xfb66b530u, 0));
                        assert(x58_b06v6_spd_digest_is_exact(0xfb66b530u, 0xffffffffu));
                        assert(!x58_b06v6_spd_digest_is_exact(0x3f5e3f88u, 0x5194e521u));
                        assert(!x58_b06v6_spd_digest_is_exact(0, 0x5194e521u));
                        return 0;
                    }
                '''
                self.assert_harness_passes(source)

    def test_v10_profile_gate_does_not_fall_back_to_raw_identity(self) -> None:
        self.assert_harness_passes(config_header(True) + r'''
            _Static_assert(X58_B06V6_HANDOFF_VERSION == 10, "profile wire version");
            _Static_assert(sizeof(struct x58_b06v6_handoff) == 164, "profile wire size");
            int main(void)
            {
                assert(x58_b06v6_spd_digest_is_exact(0xfb66b530u, 0x5194e521u));
                assert(x58_b06v6_spd_digest_is_exact(0x3f5e3f88u, 0x5194e521u));
                assert(!x58_b06v6_spd_digest_is_exact(0xfb66b530u, 0));
                assert(!x58_b06v6_spd_digest_is_exact(0xfb66b530u, 0x5194e520u));
                assert(!x58_b06v6_spd_digest_is_exact(0x3f5e3f88u, 0));
                return 0;
            }
        ''')

    def test_real_consumer_requires_profile_and_preserves_other_hard_gates(self) -> None:
        for enabled in (False, True):
            with self.subTest(serial_independent=enabled):
                self.assert_harness_passes(consumer_harness(enabled) + r'''
                    int main(void)
                    {
                        struct x58_b06v6_handoff h;
                        make_valid(&h);
                        assert(x58_b06v6_handoff_is_valid(&h));
                        assert(!x58_b06v6_handoff_is_valid(NULL));
                        assert(!x58_b06v6_raminit_result_is_exact(NULL));
                        h.raminit.spd_fnv = 0x3f5e3f88u;
                        h.digest = x58_b06v6_handoff_digest(&h);
                        assert(x58_b06v6_handoff_is_valid(&h) ==
                               !!CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT);
                    #if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
                        /* Raw is retained, not overwritten with the profile. */
                        assert(h.raminit.spd_fnv == 0x3f5e3f88u);
                        /* And it is still checksum-covered telemetry. */
                        h.raminit.spd_fnv = 0xfb66b530u;
                        assert(!x58_b06v6_handoff_is_valid(&h));
                        h.digest = x58_b06v6_handoff_digest(&h);
                        assert(x58_b06v6_handoff_is_valid(&h));
                        h.raminit.spd_profile_fnv ^= 1u;
                        h.digest = x58_b06v6_handoff_digest(&h);
                        assert(!x58_b06v6_handoff_is_valid(&h));
                        make_valid(&h);
                        h.raminit.spd_profile_fnv = 0;
                        h.digest = x58_b06v6_handoff_digest(&h);
                        assert(!x58_b06v6_handoff_is_valid(&h));
                    #endif
                        /* Rehash semantic changes: do not merely trip integrity. */
                    #define REJECT_CHANGE(field, value) do { \
                        make_valid(&h); \
                        h.field = (value); \
                        h.digest = x58_b06v6_handoff_digest(&h); \
                        assert(!x58_b06v6_handoff_is_valid(&h)); \
                    } while (0)
                        REJECT_CHANGE(raminit.cpuid, 0);
                        REJECT_CHANGE(raminit.microcode_revision, 0);
                        REJECT_CHANGE(raminit.policy_fnv, 0);
                        REJECT_CHANGE(raminit.minit_eax, 1);
                        REJECT_CHANGE(raminit.mc_mapper, 0);
                        REJECT_CHANGE(raminit.mc_common_f8, 0);
                        REJECT_CHANGE(raminit.ch2_dod, 0x2b0u);
                        REJECT_CHANGE(raminit.ch2_ranks, 1);
                        REJECT_CHANGE(raminit.ch2_status, 0);
                        REJECT_CHANGE(raminit.qpi_status, 0);
                        REJECT_CHANGE(raminit.profile_id, X58_B06VJ_PROFILE_Q);
                        REJECT_CHANGE(flags, B06V6_REQUIRED_FLAGS &
                                      ~X58_B06VL_HANDOFF_BROAD_POST_MINIT);
                        REJECT_CHANGE(reserved[0], 1);
                        return 0;
                    }
                ''')

    def test_v10_consumer_rejects_legacy_version_and_short_record(self) -> None:
        self.assert_harness_passes(consumer_harness(True) + r'''
            int main(void)
            {
                struct x58_b06v6_handoff h;
                make_valid(&h);
                h.version = 9;
                h.digest = x58_b06v6_handoff_digest(&h);
                assert(!x58_b06v6_handoff_is_valid(&h));
                make_valid(&h);
                h.structure_size = 160;
                h.digest = x58_b06v6_handoff_digest(&h);
                assert(!x58_b06v6_handoff_is_valid(&h));
                make_valid(&h);
                h.version = 9;
                h.structure_size = 160;
                h.digest = x58_b06v6_handoff_digest(&h);
                assert(!x58_b06v6_handoff_is_valid(&h));
                return 0;
            }
        ''')

    def test_opt_in_and_reread_keep_identity_and_existing_validation(self) -> None:
        kconfig = (BOARD / "Kconfig").read_text()
        block = kconfig.split("config X58_PRO_E_SPD_SERIAL_INDEPENDENT\n", 1)[1]
        block = block.split("\nconfig ", 1)[0]
        self.assertIn("depends on X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS", block)
        self.assertIn("default n", block)
        working = (ROOT / "configs/x58-pro-e-b06wk.config").read_text()
        self.assertIn("CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT=y", working)
        legacy = (ROOT / "configs/x58-pro-e-b06vl.config").read_text()
        self.assertNotIn("CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT=y", legacy)
        reread = function_text(ROMSTAGE, "b06j_run_spd")
        for reset in (
            "state->vendor_spd_valid = false;",
            "state->vendor_spd_digest = 0;",
            "state->vendor_spd_profile_digest = 0;",
        ):
            self.assertLess(reread.index(reset), reread.index("if (state->smbus_timed_out)"))
        self.assertIn("b06v0_spd_matches_target(&result, terminal)", reread)
        self.assertIn("state->vendor_spd_ackmap == B06V0_TARGET_SPD_BITMAP", reread)
        self.assertIn("state->vendor_spd_ddr3map == B06V0_TARGET_SPD_BITMAP", reread)
        self.assertIn("state->vendor_spd_digest = b06v0_buffer_digest(result.spd,", reread)
        self.assertIn("state->vendor_spd_profile_digest = x58_spd_profile_digest(result.spd,", reread)
        self.assertIn('!b04_uart_puts(" FULL256_FNV1A=")', reread)
        self.assertIn("!b04_uart_put_hex(state->vendor_spd_digest, 8)", reread)
        self.assertEqual(ROMSTAGE.count("!b06v6_spd_is_exact(state)"), 4)
        self.assertNotIn("state->vendor_spd_digest != X58_B06V6_EXPECTED_SPD_FNV", ROMSTAGE)


if __name__ == "__main__":
    unittest.main()
