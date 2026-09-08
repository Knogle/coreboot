#!/usr/bin/env python3

import re
import subprocess
import tempfile
import textwrap
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
VENDOR_INIT = (BOARD / "vendor_init.c").read_text()
VENDOR_HEADER = (BOARD / "vendor_init.h").read_text()
DEFCONFIG = (ROOT / "configs/x58-pro-e-b06vl.config").read_text()
BUILD_SCRIPT = (ROOT / "scripts/build_x58_b06vl.sh").read_text()

VL_SYMBOL = "CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS"
VK_SYMBOL = "CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS"
VL_ID = "X58PROE-B06VL-BROAD-HARD-GATE-SEABIOS-20260905"
VK_ID = "X58PROE-B06VK-Q-CANONICAL-CLASS-SEABIOS-20260905"


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


def compile_and_run(source: str) -> subprocess.CompletedProcess[str]:
    with tempfile.TemporaryDirectory(prefix="b06vl-contract-", dir="/tmp") as temp:
        executable = Path(temp) / "contract"
        compile_result = subprocess.run(
            [
                "cc",
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(BOARD),
                "-x",
                "c",
                "-",
                "-o",
                str(executable),
            ],
            input=source,
            text=True,
            capture_output=True,
            check=False,
        )
        if compile_result.returncode != 0:
            raise AssertionError(
                "C contract harness did not compile:\n"
                f"stdout:\n{compile_result.stdout}\n"
                f"stderr:\n{compile_result.stderr}"
            )
        return subprocess.run(
            [str(executable)], text=True, capture_output=True, check=False
        )


class B06VLSourceContractTests(unittest.TestCase):
    def test_executable_broad_truth_table_is_seven_by_two_by_two(self) -> None:
        harness = r'''
            #include <stdint.h>
            #include <stdio.h>

            #define CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI 1
            #define CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF 1
            #define CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY 1
            #define CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS 1
            #include "b06v6_handoff.h"

            static const uint32_t a0_values[] = {
                    0x00017000, 0x00017200, 0x00017400, 0x00017600,
                    0x00017800, 0x00017a00, 0x00017c00,
            };
            static const uint32_t cpu_9c_values[] = {
                    0x00a00502, 0x00b00502,
            };
            struct csi_pair {
                    uint8_t dynamic;
                    uint32_t raw;
            };
            static const struct csi_pair csi_pairs[] = {
                    { 0x08, 0x03e3d24e },
                    { 0x0c, 0x8b38506a },
            };
            static const uint32_t work_raw[] = {
                    0x00000000, 0xffffffff, 0x0395518f,
                    0x69b4c386, 0xdd61cb51,
            };
            static const uint32_t work_canonical[] = {
                    0x00000000, 0xffffffff, 0x95abbb4b,
                    0x0b161f01, 0xa6f9c2e6,
            };

            static uint32_t select(uint32_t a0, uint32_t post_a0,
                                   uint32_t cpu_9c,
                                   const struct csi_pair *csi,
                                   uint32_t raw, uint32_t canonical)
            {
                    return x58_b06vi_profile_for_tuple(
                            a0, post_a0, cpu_9c, csi->dynamic, csi->raw,
                            0x908dabb6, raw, canonical);
            }

            int main(void)
            {
                    _Static_assert(X58_B06V6_HANDOFF_VERSION == 9,
                                   "B06VL wire version");
                    _Static_assert(sizeof(struct x58_b06v6_handoff) == 160,
                                   "B06VL wire size");
                    _Static_assert(X58_B06VL_PROFILE_BROAD == 7,
                                   "B06VL profile ID");
                    _Static_assert(X58_B06VL_HANDOFF_BROAD_POST_MINIT == (1u << 7),
                                   "B06VL evidence flag");

                    for (unsigned int a0 = 0;
                         a0 < sizeof(a0_values) / sizeof(a0_values[0]); a0++) {
                            if (!x58_b06vl_handoff_cpu_a0_candidate(a0_values[a0]))
                                    return 10 + (int)a0;
                            for (unsigned int nine = 0;
                                 nine < sizeof(cpu_9c_values) /
                                        sizeof(cpu_9c_values[0]); nine++) {
                                    for (unsigned int csi = 0;
                                         csi < sizeof(csi_pairs) /
                                               sizeof(csi_pairs[0]); csi++) {
                                            for (unsigned int work = 0;
                                                 work < sizeof(work_raw) /
                                                        sizeof(work_raw[0]); work++) {
                                                    if (select(a0_values[a0],
                                                               a0_values[a0],
                                                               cpu_9c_values[nine],
                                                               &csi_pairs[csi],
                                                               work_raw[work],
                                                               work_canonical[work]) !=
                                                        X58_B06VL_PROFILE_BROAD)
                                                            return 30;
                                                    if (!x58_b06vi_profile_tuple_exact(
                                                                X58_B06VL_PROFILE_BROAD,
                                                                a0_values[a0],
                                                                a0_values[a0],
                                                                cpu_9c_values[nine],
                                                                csi_pairs[csi].raw,
                                                                0x908dabb6,
                                                                work_raw[work],
                                                                work_canonical[work]))
                                                            return 31;
                                            }
                                    }
                            }
                    }

                    {
                            static const uint32_t rejected_a0[] = {
                                    0, 0x00016e00, 0x00017100, 0x00017300,
                                    0x00017d00, 0x00017e00,
                            };
                            for (unsigned int i = 0;
                                 i < sizeof(rejected_a0) /
                                     sizeof(rejected_a0[0]); i++) {
                                    if (x58_b06vl_handoff_cpu_a0_candidate(
                                                rejected_a0[i]))
                                            return 40 + (int)i;
                                    if (select(rejected_a0[i], rejected_a0[i],
                                               0x00b00502, &csi_pairs[0],
                                               0, 0) !=
                                        X58_B06VI_PROFILE_INVALID)
                                            return 50 + (int)i;
                            }
                    }

                    if (select(0x17000, 0x17200, 0x00b00502,
                               &csi_pairs[0], 0, 0) != 0)
                            return 60;
                    if (select(0x17000, 0x17000, 0x00c00502,
                               &csi_pairs[0], 0, 0) != 0)
                            return 61;
                    {
                            struct csi_pair crossed = { 0x08, 0x8b38506a };
                            struct csi_pair bad = { 0x08, 0x03e3d24f };
                            if (select(0x17000, 0x17000, 0x00b00502,
                                       &crossed, 0, 0) != 0 ||
                                select(0x17000, 0x17000, 0x00b00502,
                                       &bad, 0, 0) != 0)
                                    return 62;
                    }
                    if (x58_b06vi_profile_for_tuple(
                                0x17000, 0x17000, 0x00b00502, 0x08,
                                0x03e3d24e, 0x908dabb7, 0, 0) != 0)
                            return 63;
                    if (!x58_b06vi_profile_requires_deferred_rearm(
                                X58_B06VL_PROFILE_BROAD) ||
                        x58_b06vi_profile_tuple_exact(
                                X58_B06VJ_PROFILE_Q, 0x17000, 0x17000,
                                0x00b00502, 0x03e3d24e, 0x908dabb6, 0, 0))
                            return 64;

                    puts("B06VL broad truth-table harness PASS");
                    return 0;
            }
        '''
        result = compile_and_run(textwrap.dedent(harness))
        self.assertEqual(
            result.returncode,
            0,
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}",
        )
        self.assertIn("broad truth-table harness PASS", result.stdout)

    def test_vl_disabled_preserves_b06vk_wire_and_q_contract(self) -> None:
        harness = r'''
            #include <stdint.h>

            #define CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI 1
            #define CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF 1
            #define CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY 1
            #define CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS 0
            #include "b06v6_handoff.h"

            int main(void)
            {
                    _Static_assert(X58_B06V6_HANDOFF_VERSION == 8,
                                   "B06VK wire version changed");
                    _Static_assert(sizeof(struct x58_b06v6_handoff) == 160,
                                   "B06VK wire size changed");
                    if (x58_b06vi_profile_for_tuple(
                                0x17c00, 0x17c00, 0x00a00502, 0x0c,
                                0x8b38506a, 0x908dabb6,
                                0xdd61cb51, 0x0b161f01) !=
                        X58_B06VJ_PROFILE_Q)
                            return 1;
                    if (x58_b06vi_profile_for_tuple(
                                0x17200, 0x17200, 0x00a00502, 0x0c,
                                0x8b38506a, 0x908dabb6, 0, 0) !=
                        X58_B06VI_PROFILE_INVALID)
                            return 2;
                    return 0;
            }
        '''
        result = compile_and_run(textwrap.dedent(harness))
        self.assertEqual(
            result.returncode,
            0,
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}",
        )

    def test_a0_policy_is_seven_explicit_literals_not_a_range(self) -> None:
        for source, helper_name in (
            (VENDOR_HEADER, "x58_vendor_b06vl_high_qpi_cpu_a0_candidate"),
            (HANDOFF_HEADER, "x58_b06vl_handoff_cpu_a0_candidate"),
        ):
            helper = function_text(source, helper_name)
            literals = {
                int(value, 16)
                for value in re.findall(r"case\s+(0x[0-9a-fA-F]+)u", helper)
            }
            self.assertEqual(
                literals,
                {0x17000, 0x17200, 0x17400, 0x17600,
                 0x17800, 0x17A00, 0x17C00},
            )
            self.assertNotIn("value >=", helper)
            self.assertNotIn("value <=", helper)
            self.assertNotIn("value &", helper)

    def test_option_identity_config_and_builder_are_separate(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        self.assertIn(
            "depends on X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS", option
        )
        self.assertIn("default n", option)
        self.assertIn("seven explicit observed CPU-A0 values", option)
        self.assertIn("workspace raw and canonical hashes are telemetry only", option)
        self.assertIn("explicitly risky", option)

        self.assertIn(f"{VL_SYMBOL}=y", DEFCONFIG)
        self.assertIn(f"{VK_SYMBOL}=y", DEFCONFIG)
        self.assertIn('CONFIG_LOCALVERSION="x58-pro-e-b06vl"', DEFCONFIG)
        self.assertIn("CONFIG_NO_GFX_INIT=y", DEFCONFIG)
        self.assertIn("# CONFIG_VGA_ROM_RUN is not set", DEFCONFIG)
        self.assertIn("test_x58_b06vl_source_contract", BUILD_SCRIPT)
        self.assertIn("--include-csi-wrapper-support", BUILD_SCRIPT)
        self.assertIn("--flash-size 0x1000000", BUILD_SCRIPT)

        for source in (BOOTBLOCK, ROMSTAGE, RAMMON):
            self.assertIn(VL_ID, source)
            self.assertIn(VK_ID, source)
            self.assertLess(source.index(VL_ID), source.index(VK_ID))
        self.assertLess(MAINBOARD.index(VL_SYMBOL), MAINBOARD.index(VK_SYMBOL))

    def test_pre_minit_admission_requires_hard_csi_and_platform_state(self) -> None:
        selector = function_text(ROMSTAGE, "b06vg_select_csi_profile")
        vendor_gate = function_text(
            VENDOR_INIT, "b06vb_high_qpi_post_csi_platform_exact"
        )
        for source in (selector, vendor_gate):
            self.assertIn("x58_vendor_b06vl_high_qpi_cpu_a0_candidate", source)
            self.assertIn("0x00a00502", source)
            self.assertIn("0x00b00502", source)
            self.assertIn("0x004060a0", source)
            self.assertIn("0x070f0f03", source)
            self.assertIn("0x0616fc00", source)
            self.assertIn("0x00000600", source)
        self.assertIn("tuple->cpu_a0 == saved_pre_a0", selector)
        self.assertIn(
            "post_cpu_a0 == runtime.b06vg_pre_csi_cpu_a0", vendor_gate
        )
        self.assertIn("B06VD_CSI_RAW_08_FNV", selector)
        self.assertIn("B06VD_CSI_RAW_0C_FNV", selector)
        self.assertIn("b06vb_csi_state_exact(state)", selector)
        self.assertIn("b06vd_csi_canonical_digest(state)", selector)
        self.assertIn("runtime.last_call.eax != 0", vendor_gate)
        self.assertIn("runtime.last_call.edx != runtime.last_call.edi", vendor_gate)

    def test_post_minit_gate_ignores_only_workspace_profile_hashes(self) -> None:
        promote = function_text(ROMSTAGE, "b06ve_promote_post_minit")
        for required in (
            "minit_call_status != X58_VENDOR_OK",
            "!info.canaries_valid",
            "!info.wrapper_signature_valid",
            "!info.csi_signature_valid",
            "!info.minit_signature_valid",
            "profile_id != X58_B06VL_PROFILE_BROAD",
            "!x58_b06vi_profile_tuple_exact",
            "minit_result->eax != 0",
            "workspace[1] != 0",
            "workspace[2] != 0",
            "workspace[B06V5_WORK_B3_FLAGS_OFFSET] != 0x02",
            "workspace[B06V5_WORK_COMPLETE_OFFSET] != 0x01",
            "!b06vb_minit_result_is_self_consistent",
            "!post_minit_i801_exact",
            "post_minit_cmos != B06V6_CMOS_GUARD_IN_PROGRESS",
            "!b06vi_post_minit_common_endpoint_exact",
            "!b06v6_spd_is_exact(state)",
            "B06VL BROAD_UNSAFE HARD_RETURN_GATE=PASS",
        ):
            self.assertIn(required, promote)
        self.assertIn(
            "#if !CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS\n"
            "\t    !workspace_pattern_exact ||",
            promote,
        )
        self.assertIn(
            "probe_status == X58_VENDOR_ERR_PLATFORM_STATE", promote
        )
        self.assertIn("WORKSPACE_HASH_MODE=RAW_AND_CANONICAL_TELEMETRY_ONLY", promote)

    def test_broad_reaches_promotion_and_rearm_is_postmem_only(self) -> None:
        minit_path = function_text(ROMSTAGE, "b06vb_high_qpi_minit_observe")
        postmem = function_text(HANDOFF, "platform_romstage_post_mem")
        self.assertIn("csi_profile != B06VL_CSI_BROAD_HARD_GATED", minit_path)
        self.assertIn("REARM_AFTER_FULL_POSTMEM=01 BROAD_UNSAFE=01", minit_path)
        self.assertIn("return b06ve_promote_post_minit", minit_path)

        lowmem = postmem.index("b06v6_destructive_window_test(X58_B06VF_LOWMEM_BASE")
        alias = postmem.index("b06v6_transactional_smoke(window_smoke", lowmem)
        cbmem_window = postmem.index(
            "b06v6_destructive_window_test(X58_B06V6_CBMEM_BASE", alias
        )
        object_window = postmem.index(
            "b06v6_destructive_window_test(X58_B06V6_OBJECT_BASE", cbmem_window
        )
        handoff_readback = postmem.index("x58_b06v6_handoff_is_valid(handoff)")
        rearm = postmem.index("x58_b06vi_rearm_o_guard_after_postmem()")
        finalize = postmem.index("x58_b06ve_finalize_persistent_guard()")
        self.assertLess(lowmem, alias)
        self.assertLess(alias, cbmem_window)
        self.assertLess(cbmem_window, object_window)
        self.assertLess(object_window, handoff_readback)
        self.assertLess(handoff_readback, rearm)
        self.assertLess(rearm, finalize)

    def test_v9_consumer_requires_broad_profile_and_evidence_flag(self) -> None:
        consumer = function_text(HANDOFF, "x58_b06v6_raminit_result_is_exact")
        validator = function_text(HANDOFF, "x58_b06v6_handoff_is_valid")
        self.assertIn(
            "result->profile_id == X58_B06VL_PROFILE_BROAD", consumer
        )
        self.assertIn("x58_b06vi_profile_tuple_exact", consumer)
        self.assertIn(
            "#define B06VL_REQUIRED_FLAG "
            "X58_B06VL_HANDOFF_BROAD_POST_MINIT",
            HANDOFF,
        )
        required_flags = between(
            HANDOFF,
            "#define B06V6_COMMON_REQUIRED_FLAGS",
            "#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS\n"
            "\t_Static_assert",
        )
        self.assertIn("X58_B06VF_HANDOFF_LOWMEM_SMOKED | B06VL_REQUIRED_FLAG", required_flags)
        self.assertIn("handoff->version != X58_B06V6_HANDOFF_VERSION", validator)
        self.assertIn("handoff->flags != B06V6_REQUIRED_FLAGS", validator)
        self.assertIn("handoff->digest == x58_b06v6_handoff_digest", validator)

        # Compile the actual production digest/result/validator function bodies,
        # not a parallel Python model.  Recompute the digest after semantic
        # mutations so each rejection is attributable to the named hard gate.
        harness = textwrap.dedent(r'''
            #include <stdbool.h>
            #include <stddef.h>
            #include <stdint.h>
            #include <stdio.h>
            #include <string.h>

            #define CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI 1
            #define CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF 1
            #define CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY 1
            #define CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS 1
            #include "b06v6_handoff.h"

            #define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
            #define B06V6_EXPECTED_MTRR_CAP 0x00000d0au
            #define B06V6_EXPECTED_MTRR_DEF_TYPE 0x00000800u
            #define B06V6_REQUIRED_FLAGS \
                    (X58_B06V6_HANDOFF_EXACT_MINIT | \
                     X58_B06V6_HANDOFF_MTRR_GATED | \
                     X58_B06V6_HANDOFF_CBMEM_SMOKED | \
                     X58_B06V6_HANDOFF_OBJECT_SMOKED | \
                     X58_B06V6_HANDOFF_CBMEM_READY | \
                     X58_B06VE_HANDOFF_HIGH_QPI_POST_MINIT | \
                     X58_B06VF_HANDOFF_LOWMEM_SMOKED | \
                     X58_B06VL_HANDOFF_BROAD_POST_MINIT)
        ''')
        harness += "\n" + function_text(HANDOFF, "fnv1a32") + "\n"
        harness += "\n" + function_text(
            HANDOFF, "x58_b06v6_handoff_digest"
        ) + "\n"
        harness += "\n" + consumer + "\n"
        harness += "\n" + validator + "\n"
        harness += textwrap.dedent(r'''
            static void make_valid(struct x58_b06v6_handoff *handoff)
            {
                    memset(handoff, 0, sizeof(*handoff));
                    handoff->magic = X58_B06V6_HANDOFF_MAGIC;
                    handoff->version = X58_B06V6_HANDOFF_VERSION;
                    handoff->structure_size = sizeof(*handoff);
                    handoff->flags = B06V6_REQUIRED_FLAGS;
                    handoff->raminit.cpuid = X58_B06V6_EXPECTED_CPUID;
                    handoff->raminit.microcode_revision =
                            X58_B06V6_EXPECTED_UCODE;
                    handoff->raminit.spd_fnv = X58_B06V6_EXPECTED_SPD_FNV;
                    handoff->raminit.csi_state_fnv =
                            X58_B06VE_EXPECTED_CSI_FNV;
                    handoff->raminit.policy_fnv =
                            X58_B06V6_EXPECTED_POLICY_FNV;
                    handoff->raminit.workspace_fnv = 0x12345678u;
                    handoff->raminit.csi_state_canonical_fnv =
                            X58_B06VE_EXPECTED_CSI_CANONICAL_FNV;
                    handoff->raminit.workspace_canonical_fnv = 0x9abcdef0u;
                    handoff->raminit.minit_eax = 0;
                    handoff->raminit.mc_mapper = X58_B06V6_EXPECTED_MC_MAPPER;
                    handoff->raminit.mc_common_f8 =
                            X58_B06V6_EXPECTED_MC_COMMON_F8;
                    handoff->raminit.ch2_dod = X58_B06V6_EXPECTED_CH2_DOD;
                    handoff->raminit.ch2_ranks = X58_B06V6_EXPECTED_CH2_RANKS;
                    handoff->raminit.ch2_status = X58_B06V6_EXPECTED_CH2_STATUS;
                    handoff->raminit.qpi_status = X58_B06VE_EXPECTED_QPI_STATUS;
                    handoff->raminit.post_minit_ioh_stage_9c =
                            X58_B06VE_EXPECTED_POST_MINIT_IOH_STAGE_9C;
                    handoff->raminit.profile_id = X58_B06VL_PROFILE_BROAD;
                    handoff->raminit.saved_pre_a0 = 0x00017200u;
                    handoff->raminit.post_minit_cpu_a0 = 0x00017200u;
                    handoff->raminit.post_minit_cpu_9c = 0x00b00502u;
                    handoff->mtrr_cap = B06V6_EXPECTED_MTRR_CAP;
                    handoff->mtrr_def_type = B06V6_EXPECTED_MTRR_DEF_TYPE;
                    handoff->cbmem_base = X58_B06V6_CBMEM_BASE;
                    handoff->cbmem_top = X58_B06V6_CBMEM_TOP;
                    handoff->object_base = X58_B06V6_OBJECT_BASE;
                    handoff->object_size = X58_B06V6_OBJECT_SIZE;
                    handoff->object_max_size = X58_B06V6_OBJECT_MAX_SIZE;
                    handoff->digest = x58_b06v6_handoff_digest(handoff);
            }

            int main(void)
            {
                    struct x58_b06v6_handoff handoff;

                    make_valid(&handoff);
                    if (!x58_b06v6_handoff_is_valid(&handoff))
                            return 1;

                    handoff.version = 8;
                    handoff.digest = x58_b06v6_handoff_digest(&handoff);
                    if (x58_b06v6_handoff_is_valid(&handoff))
                            return 2;

                    make_valid(&handoff);
                    handoff.raminit.profile_id = X58_B06VJ_PROFILE_Q;
                    handoff.digest = x58_b06v6_handoff_digest(&handoff);
                    if (x58_b06v6_handoff_is_valid(&handoff))
                            return 3;

                    make_valid(&handoff);
                    handoff.flags &= ~X58_B06VL_HANDOFF_BROAD_POST_MINIT;
                    handoff.digest = x58_b06v6_handoff_digest(&handoff);
                    if (x58_b06v6_handoff_is_valid(&handoff))
                            return 4;

                    make_valid(&handoff);
                    handoff.digest ^= 1u;
                    if (x58_b06v6_handoff_is_valid(&handoff))
                            return 5;

                    puts("B06VL v9 consumer mutation harness PASS");
                    return 0;
            }
        ''')
        result = compile_and_run(harness)
        self.assertEqual(
            result.returncode,
            0,
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}",
        )
        self.assertIn("consumer mutation harness PASS", result.stdout)


if __name__ == "__main__":
    unittest.main()
