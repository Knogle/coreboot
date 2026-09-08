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
DEFCONFIG = (ROOT / "configs/x58-pro-e-b06vi.config").read_text()

VI_SYMBOL = "CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS"
VH_SYMBOL = "CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS"
VI_ID = "X58PROE-B06VI-COUPLED-PROFILE-O-SEABIOS-20260905"
VH_ID = "X58PROE-B06VH-PRIMARY-WORKSPACE-REARM-SEABIOS-20260905"


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
    with tempfile.TemporaryDirectory(prefix="b06vi-contract-", dir="/tmp") as temp:
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


class B06VISourceContractTests(unittest.TestCase):
    def test_executable_five_row_truth_table_rejects_crosses_and_mutations(self) -> None:
        harness = r'''
            #include <stdint.h>
            #include <stdio.h>

            #define CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI 1
            #define CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF 1
            #define CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY 1
            #define CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS 1
            #include "b06v6_handoff.h"

            struct row {
                    uint32_t id;
                    uint32_t pre_a0;
                    uint32_t post_a0;
                    uint32_t post_9c;
                    uint32_t csi_dynamic;
                    uint32_t csi_raw;
                    uint32_t csi_canonical;
                    uint32_t work_raw;
                    uint32_t work_canonical;
            };

            static const struct row rows[] = {
                    { 1, 0x00017000, 0x00017000, 0x00b00502, 0x08,
                      0x03e3d24e, 0x908dabb6, 0x94299f43, 0xa6f9c2e6 },
                    { 2, 0x00017000, 0x00017000, 0x00b00502, 0x08,
                      0x03e3d24e, 0x908dabb6, 0xb6346533, 0xa6f9c2e6 },
                    { 3, 0x00017000, 0x00017000, 0x00b00502, 0x08,
                      0x03e3d24e, 0x908dabb6, 0xeb15c076, 0xc313e060 },
                    { 4, 0x00017000, 0x00017000, 0x00b00502, 0x08,
                      0x03e3d24e, 0x908dabb6, 0x5fb636d9, 0xb3fafec0 },
                    { 5, 0x00017c00, 0x00017c00, 0x00a00502, 0x0c,
                      0x8b38506a, 0x908dabb6, 0x50f67315, 0x92c70df4 },
            };

            static uint32_t select_row(const struct row *row)
            {
                    return x58_b06vi_profile_for_tuple(
                            row->pre_a0, row->post_a0, row->post_9c,
                            (uint8_t)row->csi_dynamic, row->csi_raw,
                            row->csi_canonical, row->work_raw,
                            row->work_canonical);
            }

            int main(void)
            {
                    _Static_assert(X58_B06V6_HANDOFF_VERSION == 6,
                                   "B06VI wire version");
                    _Static_assert(sizeof(struct x58_b06v6_handoff) == 160,
                                   "B06VI wire size");

                    for (unsigned int i = 0; i < 5; i++) {
                            if (select_row(&rows[i]) != rows[i].id)
                                    return 10 + (int)i;
                            if (!x58_b06vi_profile_tuple_exact(
                                        rows[i].id, rows[i].pre_a0,
                                        rows[i].post_a0, rows[i].post_9c,
                                        rows[i].csi_raw, rows[i].csi_canonical,
                                        rows[i].work_raw,
                                        rows[i].work_canonical))
                                    return 20 + (int)i;
                            for (uint32_t wrong_id = 1; wrong_id <= 5; wrong_id++) {
                                    if (wrong_id != rows[i].id &&
                                        x58_b06vi_profile_tuple_exact(
                                            wrong_id, rows[i].pre_a0,
                                            rows[i].post_a0, rows[i].post_9c,
                                            rows[i].csi_raw,
                                            rows[i].csi_canonical,
                                            rows[i].work_raw,
                                            rows[i].work_canonical))
                                            return 30 + (int)i;
                            }
                    }

                    /* Cartesian CPU/CSI/workspace components: five rows only. */
                    for (unsigned int cpu = 0; cpu < 2; cpu++) {
                            for (unsigned int csi = 0; csi < 2; csi++) {
                                    for (unsigned int work = 0; work < 5; work++) {
                                            struct row crossed = rows[work];
                                            uint32_t expected = 0;

                                            crossed.pre_a0 = rows[cpu ? 4 : 0].pre_a0;
                                            crossed.post_a0 = rows[cpu ? 4 : 0].post_a0;
                                            crossed.post_9c = rows[cpu ? 4 : 0].post_9c;
                                            crossed.csi_dynamic =
                                                rows[csi ? 4 : 0].csi_dynamic;
                                            crossed.csi_raw = rows[csi ? 4 : 0].csi_raw;
                                            crossed.csi_canonical =
                                                rows[csi ? 4 : 0].csi_canonical;
                                            if (!cpu && !csi && work < 4)
                                                    expected = rows[work].id;
                                            if (cpu && csi && work == 4)
                                                    expected = rows[4].id;
                                            if (select_row(&crossed) != expected)
                                                    return 40 + (int)(cpu * 10 + csi * 5 + work);
                                    }
                            }
                    }

                    /* Every single field is part of the indivisible row. */
                    for (unsigned int row = 0; row < 5; row++) {
                            for (unsigned int field = 0; field < 8; field++) {
                                    struct row changed = rows[row];
                                    uint32_t *first = &changed.pre_a0;

                                    first[field] ^= 1u;
                                    if (select_row(&changed) != X58_B06VI_PROFILE_INVALID)
                                            return 80 + (int)(row * 8 + field);
                            }
                    }

                    puts("B06VI five-row truth-table harness PASS");
                    return 0;
            }
        '''
        result = compile_and_run(textwrap.dedent(harness))
        self.assertEqual(
            result.returncode,
            0,
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}",
        )
        self.assertIn("five-row truth-table harness PASS", result.stdout)

    def test_vi_disabled_compiles_to_the_unchanged_vh_wire_contract(self) -> None:
        harness = r'''
            #include <stdint.h>

            #define CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI 1
            #define CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF 1
            #define CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY 1
            #define CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS 0
            #include "b06v6_handoff.h"

            int main(void)
            {
                    _Static_assert(X58_B06V6_HANDOFF_VERSION == 5,
                                   "B06VH wire version changed");
                    _Static_assert(sizeof(struct x58_b06v6_handoff) == 144,
                                   "B06VH wire size changed");
                    if (!x58_b06vh_workspace_digest_pair_exact(
                                0x94299f43, 0xa6f9c2e6) ||
                        !x58_b06vh_workspace_digest_pair_exact(
                                0xb6346533, 0xa6f9c2e6) ||
                        !x58_b06vh_workspace_digest_pair_exact(
                                0xeb15c076, 0xc313e060) ||
                        !x58_b06vh_workspace_digest_pair_exact(
                                0x5fb636d9, 0xb3fafec0))
                            return 1;
                    if (x58_b06vh_workspace_digest_pair_exact(
                                0x50f67315, 0x92c70df4))
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

    def test_option_identity_and_defconfig_are_explicit_and_default_off(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        self.assertIn("depends on X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS", option)
        self.assertIn("default n", option)
        self.assertNotIn("default y", option)
        for evidence in (
            "0x00017c00",
            "0x00a00502",
            "0x0c/0x8b38506a/0x908dabb6",
            "0x50f67315/0x92c70df4",
            "08:00:5d:01:a3",
            "version-6",
        ):
            self.assertIn(evidence, option)

        self.assertIn(f"{VI_SYMBOL}=y", DEFCONFIG)
        self.assertIn(f"{VH_SYMBOL}=y", DEFCONFIG)
        self.assertIn("CONFIG_X58_PRO_E_BRINGUP_STAGE=11", DEFCONFIG)
        self.assertIn("CONFIG_PAYLOAD_SEABIOS=y", DEFCONFIG)

        for source in (BOOTBLOCK, ROMSTAGE, RAMMON):
            self.assertIn(VI_ID, source)
            self.assertIn(VH_ID, source)
            self.assertLess(source.index(VI_ID), source.index(VH_ID))
        self.assertLess(MAINBOARD.index(VI_SYMBOL), MAINBOARD.index(VH_SYMBOL))

        payload_defaults = KCONFIG[KCONFIG.index("config PAYLOAD_CONFIGFILE") :]
        self.assertLess(
            payload_defaults.index(VI_SYMBOL.removeprefix("CONFIG_")),
            payload_defaults.index(VH_SYMBOL.removeprefix("CONFIG_")),
        )
        part_numbers = KCONFIG[KCONFIG.index("config MAINBOARD_PART_NUMBER") :]
        self.assertLess(part_numbers.index("B06VI"), part_numbers.index("B06VH"))

    def test_shared_header_is_the_only_five_row_admission_table(self) -> None:
        table = function_text(HANDOFF_HEADER, "x58_b06vi_profile_for_tuple")
        consumer = function_text(HANDOFF, "x58_b06v6_raminit_result_is_exact")
        promotion = function_text(ROMSTAGE, "b06ve_promote_post_minit")

        self.assertEqual(table.count("return X58_B06VI_PROFILE_PRIMARY_"), 4)
        self.assertEqual(table.count("return X58_B06VI_PROFILE_O"), 1)
        self.assertIn("return X58_B06VI_PROFILE_INVALID", table)
        for field in (
            "saved_pre_a0",
            "post_minit_cpu_a0",
            "post_minit_cpu_9c",
            "csi_dynamic_2a6",
            "csi_raw_digest",
            "csi_canonical_digest",
            "workspace_raw_digest",
            "workspace_canonical_digest",
        ):
            self.assertIn(field, table)

        self.assertIn("x58_b06vi_profile_tuple_exact", consumer)
        self.assertIn("result->profile_id", consumer)
        self.assertIn("x58_b06vi_profile_for_tuple", promotion)
        self.assertIn("x58_b06vi_profile_tuple_exact", promotion)

    def test_platform_state_exception_is_o_workspace_only_then_fully_gated(self) -> None:
        promotion = function_text(ROMSTAGE, "b06ve_promote_post_minit")
        admission = between(
            promotion, "probe_status_admitted =", "b06v8_capture_qpi_tuple"
        )
        vi_exception = between(
            admission,
            "#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS",
            "#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS",
        )

        self.assertIn("X58_VENDOR_ERR_PLATFORM_STATE", admission)
        self.assertIn("workspace_pattern_exact", vi_exception)
        # The union pattern helper also accepts A/B/P/N, so the exception must
        # additionally pin O's raw and canonical pair (or call an O-only helper).
        self.assertRegex(
            vi_exception,
            r"X58_B06VI_EXPECTED_WORKSPACE_RAW_O_FNV|b06vi_o_workspace_pattern_exact",
        )
        self.assertRegex(
            vi_exception,
            r"X58_B06VI_EXPECTED_WORKSPACE_CANONICAL_O_FNV|b06vi_o_workspace_pattern_exact",
        )

        select_at = promotion.index("profile_id = x58_b06vi_profile_for_tuple")
        exact_gate_at = promotion.index("if (minit_call_status != X58_VENDOR_OK", select_at)
        exact_gate_end = promotion.index("#else", exact_gate_at)
        exact_gate = promotion[exact_gate_at:exact_gate_end]
        for gate in (
            "!probe_status_admitted",
            "!workspace_pattern_exact",
            "profile_id == X58_B06VI_PROFILE_INVALID",
            "!x58_b06vi_profile_tuple_exact",
            "pre_minit_tuple->cpu_a0 != saved_pre_a0",
            "pre_minit_tuple->cpu_a0 != post_minit_tuple.cpu_a0",
            "pre_minit_tuple->cpu_9c != post_minit_tuple.cpu_9c",
            "!b06vi_post_minit_common_endpoint_exact",
            "!post_minit_i801_exact",
        ):
            self.assertIn(gate, exact_gate)
        self.assertLess(admission.index("X58_VENDOR_ERR_PLATFORM_STATE"), select_at)
        self.assertLess(select_at, exact_gate_at)

    def test_non_o_observation_path_remains_terminal(self) -> None:
        observe = function_text(ROMSTAGE, "b06vb_high_qpi_minit_observe")
        final_snapshot = observe.index("final_csi_state = x58_vendor_csi_state_snapshot")
        observation_at = observe.index(
            "if (csi_profile == B06VG_CSI_OBSERVATION", final_snapshot
        )
        terminal_at = observe.index("B06VG_OBSERVATION_MINIT_TERMINAL", observation_at)
        profile_required_at = observe.index("B06VG_PRIMARY_PROFILE_REQUIRED", terminal_at)
        promote_at = observe.index("return b06ve_promote_post_minit", profile_required_at)
        condition = observe[observation_at : observe.index(") {", observation_at) + 3]
        terminal_branch = observe[observation_at:profile_required_at]

        self.assertIn("&& !b06vi_o_candidate", condition)
        self.assertIn("return b06v6_fallback", terminal_branch)
        self.assertNotIn("x58_b06v6_handoff_record_success", terminal_branch)
        self.assertEqual(
            (observation_at, terminal_at, profile_required_at, promote_at),
            tuple(sorted((observation_at, terminal_at, profile_required_at, promote_at))),
        )

    def test_o_rearm_is_deferred_until_all_postmem_tests_and_readback(self) -> None:
        promotion = function_text(ROMSTAGE, "b06ve_promote_post_minit")
        rearm_block_at = promotion.index("if (x58_b06vi_profile_is_primary(profile_id))")
        record_at = promotion.index("x58_b06v6_handoff_record_success", rearm_block_at)
        self.assertLess(rearm_block_at, record_at)
        self.assertNotIn("x58_b06vi_rearm_o_guard_after_postmem", promotion)
        self.assertIn("O I801 rearm deferred until full v6 postmem", promotion)

        postmem = function_text(HANDOFF, "platform_romstage_post_mem")
        lowmem_at = postmem.index("b06v6_destructive_window_test(X58_B06VF_LOWMEM_BASE")
        alias_at = postmem.index("b06v6_transactional_smoke", lowmem_at)
        cbmem_test_at = postmem.index(
            "b06v6_destructive_window_test(X58_B06V6_CBMEM_BASE", alias_at
        )
        object_test_at = postmem.index(
            "b06v6_destructive_window_test(X58_B06V6_OBJECT_BASE", cbmem_test_at
        )
        cbmem_init_at = postmem.index("cbmem_initialize_empty_id_size", object_test_at)
        handoff_digest_at = postmem.index("handoff->digest =", cbmem_init_at)
        handoff_readback_at = postmem.index(
            "x58_b06v6_handoff_is_valid(handoff)", handoff_digest_at
        )
        profile_o_at = postmem.index(
            "x58_b06vi_profile_requires_deferred_rearm", handoff_readback_at
        )
        rearm_at = postmem.index("x58_b06vi_rearm_o_guard_after_postmem", profile_o_at)
        finalize_at = postmem.index("x58_b06ve_finalize_persistent_guard", rearm_at)
        postcar_at = postmem.index("entering postcar", finalize_at)
        ordered = (
            lowmem_at,
            alias_at,
            cbmem_test_at,
            object_test_at,
            cbmem_init_at,
            handoff_digest_at,
            handoff_readback_at,
            profile_o_at,
            rearm_at,
            finalize_at,
            postcar_at,
        )
        self.assertEqual(ordered, tuple(sorted(ordered)))

    def test_handoff_v6_carries_profile_endpoints_and_covers_them_by_digest(self) -> None:
        version = between(
            HANDOFF_HEADER,
            "#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF",
            "/* Conservative, hardware-smoked regions",
        )
        result_struct = between(
            HANDOFF_HEADER,
            "struct x58_b06v6_raminit_result {",
            "/* Persisted in CBMEM",
        )
        wire_struct = between(
            HANDOFF_HEADER,
            "struct x58_b06v6_handoff {",
            "/* Romstage-only producer",
        )
        digest = function_text(HANDOFF, "x58_b06v6_handoff_digest")
        validator = function_text(HANDOFF, "x58_b06v6_handoff_is_valid")

        self.assertRegex(
            version,
            rf"#elif {VI_SYMBOL}\s+#define X58_B06V6_HANDOFF_VERSION\s+6u",
        )
        self.assertIn("_Static_assert(sizeof(struct x58_b06v6_handoff) == 160", HANDOFF)
        for field in (
            "profile_id",
            "saved_pre_a0",
            "post_minit_cpu_a0",
            "post_minit_cpu_9c",
        ):
            self.assertIn(f"uint32_t {field};", result_struct)
            self.assertIn(f".{field} =", ROMSTAGE)
        self.assertIn("struct x58_b06v6_raminit_result raminit", wire_struct)
        self.assertIn("offsetof(struct x58_b06v6_handoff, digest)", digest)
        self.assertIn("handoff->version != X58_B06V6_HANDOFF_VERSION", validator)
        self.assertIn("handoff->structure_size != sizeof(*handoff)", validator)
        self.assertIn("x58_b06v6_raminit_result_is_exact", validator)
        self.assertIn("handoff->digest == x58_b06v6_handoff_digest(handoff)", validator)


if __name__ == "__main__":
    unittest.main()
