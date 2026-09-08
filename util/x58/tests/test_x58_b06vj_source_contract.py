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
DEFCONFIG = (ROOT / "configs/x58-pro-e-b06vj.config").read_text()

VJ_SYMBOL = "CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS"
VI_SYMBOL = "CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS"
VJ_ID = "X58PROE-B06VJ-COUPLED-PROFILE-Q-SEABIOS-20260905"
VI_ID = "X58PROE-B06VI-COUPLED-PROFILE-O-SEABIOS-20260905"

Q_MARKERS = (
    (0x18D1, 0x43),
    (0x18D2, 0x18),
    (0x18D3, 0x00),
    (0x23A2, 0x06),
    (0x2402, 0x00),
    (0x2459, 0x00),
    (0x245A, 0x00),
    (0x245B, 0x00),
    (0x245C, 0x00),
    (0x245D, 0xFF),
    (0x245E, 0xFF),
    (0x245F, 0xFF),
    (0x2460, 0xFF),
    (0x2461, 0xFF),
    (0x26B6, 0x0C),
    (0x26B7, 0x78),
    (0x26B8, 0x37),
    (0x26B9, 0x0C),
    (0x26BA, 0x78),
    (0x26BB, 0x37),
    (0x26C0, 0x0A),
    (0x26C1, 0x7A),
    (0x26C2, 0x39),
    (0x26C3, 0x0A),
    (0x26C4, 0x7A),
    (0x26C5, 0x39),
)


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
    with tempfile.TemporaryDirectory(prefix="b06vj-contract-", dir="/tmp") as temp:
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


class B06VJSourceContractTests(unittest.TestCase):
    def test_executable_six_row_truth_table_rejects_crosses_and_mutations(self) -> None:
        harness = r'''
            #include <stdint.h>
            #include <stdio.h>

            #define CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI 1
            #define CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF 1
            #define CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY 1
            #define CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS 0
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
                    { 6, 0x00017c00, 0x00017c00, 0x00a00502, 0x0c,
                      0x8b38506a, 0x908dabb6, 0x69b4c386, 0x0b161f01 },
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
                    _Static_assert(X58_B06V6_HANDOFF_VERSION == 7,
                                   "B06VJ wire version");
                    _Static_assert(sizeof(struct x58_b06v6_handoff) == 160,
                                   "B06VJ wire size");
                    _Static_assert(X58_B06VJ_PROFILE_Q == 6,
                                   "B06VJ Q wire ID");

                    for (unsigned int i = 0; i < 6; i++) {
                            if (select_row(&rows[i]) != rows[i].id)
                                    return 10 + (int)i;
                            if (!x58_b06vi_profile_tuple_exact(
                                        rows[i].id, rows[i].pre_a0,
                                        rows[i].post_a0, rows[i].post_9c,
                                        rows[i].csi_raw, rows[i].csi_canonical,
                                        rows[i].work_raw,
                                        rows[i].work_canonical))
                                    return 20 + (int)i;
                            for (uint32_t wrong_id = 1; wrong_id <= 6; wrong_id++) {
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

                    /* Cross CPU, CSI dynamic/raw, and workspace raw/canonical. */
                    for (unsigned int cpu = 0; cpu < 2; cpu++) {
                            for (unsigned int csi_dynamic = 0; csi_dynamic < 2;
                                 csi_dynamic++) {
                                    for (unsigned int csi_raw = 0; csi_raw < 2;
                                         csi_raw++) {
                                            for (unsigned int work_raw = 0;
                                                 work_raw < 6; work_raw++) {
                                                    for (unsigned int work_canon = 0;
                                                         work_canon < 6;
                                                         work_canon++) {
                                                            struct row crossed =
                                                                    rows[work_raw];
                                                            uint32_t expected = 0;

                                                            crossed.pre_a0 =
                                                                    rows[cpu ? 4 : 0].pre_a0;
                                                            crossed.post_a0 =
                                                                    rows[cpu ? 4 : 0].post_a0;
                                                            crossed.post_9c =
                                                                    rows[cpu ? 4 : 0].post_9c;
                                                            crossed.csi_dynamic =
                                                                    rows[csi_dynamic ? 4 : 0].csi_dynamic;
                                                            crossed.csi_raw =
                                                                    rows[csi_raw ? 4 : 0].csi_raw;
                                                            crossed.csi_canonical = 0x908dabb6;
                                                            crossed.work_canonical =
                                                                    rows[work_canon].work_canonical;

                                                            if (!cpu && !csi_dynamic &&
                                                                !csi_raw && work_raw < 4 &&
                                                                crossed.work_canonical ==
                                                                    rows[work_raw].work_canonical)
                                                                    expected = rows[work_raw].id;
                                                            if (cpu && csi_dynamic && csi_raw &&
                                                                work_raw >= 4 &&
                                                                crossed.work_canonical ==
                                                                    rows[work_raw].work_canonical)
                                                                    expected = rows[work_raw].id;
                                                            if (select_row(&crossed) != expected)
                                                                    return 50;
                                                    }
                                            }
                                    }
                            }
                    }

                    /* Every tuple field is independently mandatory. */
                    for (unsigned int row = 0; row < 6; row++) {
                            for (unsigned int field = 0; field < 8; field++) {
                                    struct row changed = rows[row];
                                    uint32_t *first = &changed.pre_a0;

                                    first[field] ^= 1u;
                                    if (select_row(&changed) !=
                                        X58_B06VI_PROFILE_INVALID)
                                            return 60 + (int)(row * 8 + field);
                            }
                    }

                    if (x58_b06vi_profile_requires_deferred_rearm(1) ||
                        x58_b06vi_profile_requires_deferred_rearm(4) ||
                        !x58_b06vi_profile_requires_deferred_rearm(5) ||
                        !x58_b06vi_profile_requires_deferred_rearm(6))
                            return 120;

                    puts("B06VJ six-row truth-table harness PASS");
                    return 0;
            }
        '''
        result = compile_and_run(textwrap.dedent(harness))
        self.assertEqual(
            result.returncode,
            0,
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}",
        )
        self.assertIn("six-row truth-table harness PASS", result.stdout)

    def test_q_marker_helper_rejects_each_single_byte_mutation(self) -> None:
        helper = function_text(ROMSTAGE, "b06vj_q_workspace_pattern_exact")
        assignments = "\n".join(
            f"workspace[0x{offset:04x}] = 0x{value:02x};"
            for offset, value in Q_MARKERS
        )
        offsets = ", ".join(f"0x{offset:04x}" for offset, _ in Q_MARKERS)
        values = ", ".join(f"0x{value:02x}" for _, value in Q_MARKERS)
        harness = f'''
            #include <stdbool.h>
            #include <stddef.h>
            #include <stdint.h>

            #define X58_B06VJ_EXPECTED_WORKSPACE_RAW_Q_FNV 0x69b4c386u
            #define X58_B06VJ_EXPECTED_WORKSPACE_CANONICAL_Q_FNV 0x0b161f01u
            #define CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS 0

            {helper}

            int main(void)
            {{
                    uint8_t workspace[0x2bcc] = {{ 0 }};
                    static const uint16_t offsets[] = {{ {offsets} }};
                    static const uint8_t values[] = {{ {values} }};

                    {assignments}
                    if (!b06vj_q_workspace_pattern_exact(workspace,
                                0x69b4c386u, 0x0b161f01u))
                            return 1;
                    if (b06vj_q_workspace_pattern_exact(NULL,
                                0x69b4c386u, 0x0b161f01u) ||
                        b06vj_q_workspace_pattern_exact(workspace,
                                0x69b4c387u, 0x0b161f01u) ||
                        b06vj_q_workspace_pattern_exact(workspace,
                                0x69b4c386u, 0x0b161f00u))
                            return 2;
                    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]);
                         i++) {{
                            workspace[offsets[i]] = values[i] ^ 1u;
                            if (b06vj_q_workspace_pattern_exact(workspace,
                                        0x69b4c386u, 0x0b161f01u))
                                    return 10 + (int)i;
                            workspace[offsets[i]] = values[i];
                    }}
                    return 0;
            }}
        '''
        result = compile_and_run(textwrap.dedent(harness))
        self.assertEqual(
            result.returncode,
            0,
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}",
        )

    def test_vj_disabled_is_exactly_the_five_row_vi_wire_contract(self) -> None:
        harness = r'''
            #include <stdint.h>

            #define CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI 1
            #define CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF 1
            #define CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY 1
            #define CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS 0
            #define CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS 0
            #include "b06v6_handoff.h"

            int main(void)
            {
                    _Static_assert(X58_B06V6_HANDOFF_VERSION == 6,
                                   "B06VI wire version changed");
                    _Static_assert(sizeof(struct x58_b06v6_handoff) == 160,
                                   "B06VI wire size changed");
                    if (x58_b06vi_profile_for_tuple(
                                0x00017c00, 0x00017c00, 0x00a00502, 0x0c,
                                0x8b38506a, 0x908dabb6,
                                0x50f67315, 0x92c70df4) !=
                        X58_B06VI_PROFILE_O)
                            return 1;
                    if (x58_b06vi_profile_for_tuple(
                                0x00017c00, 0x00017c00, 0x00a00502, 0x0c,
                                0x8b38506a, 0x908dabb6,
                                0x69b4c386, 0x0b161f01) !=
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

    def test_kconfig_defconfig_and_build_identity_are_distinct(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        self.assertIn("depends on X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS", option)
        self.assertIn("default n", option)
        self.assertNotIn("default y", option)
        for evidence in (
            "0x00017c00",
            "0x00a00502",
            "0x0c/0x8b38506a/0x908dabb6",
            "0x69b4c386/0x0b161f01",
            "18d1..18d3=43:18:00",
            "2459..2461=00:00:00:00:ff:ff:ff:ff:ff",
            "08:00:5d:01:a3",
        ):
            self.assertIn(evidence, option)

        self.assertIn(f"{VJ_SYMBOL}=y", DEFCONFIG)
        self.assertIn(f"{VI_SYMBOL}=y", DEFCONFIG)
        self.assertIn("CONFIG_PAYLOAD_SEABIOS=y", DEFCONFIG)
        self.assertIn('CONFIG_LOCALVERSION="x58-pro-e-b06vj"', DEFCONFIG)

        for source in (BOOTBLOCK, ROMSTAGE, RAMMON):
            self.assertIn(VJ_ID, source)
            self.assertIn(VI_ID, source)
            self.assertLess(source.index(VJ_ID), source.index(VI_ID))
        self.assertLess(MAINBOARD.index(VJ_SYMBOL), MAINBOARD.index(VI_SYMBOL))

        defaults = KCONFIG[KCONFIG.index("config PAYLOAD_CONFIGFILE") :]
        self.assertLess(
            defaults.index(VJ_SYMBOL.removeprefix("CONFIG_")),
            defaults.index(VI_SYMBOL.removeprefix("CONFIG_")),
        )
        parts = KCONFIG[KCONFIG.index("config MAINBOARD_PART_NUMBER") :]
        self.assertLess(parts.index("B06VJ"), parts.index("B06VI"))

    def test_q_is_a_single_guarded_sixth_row_and_o_remains_present(self) -> None:
        table = function_text(HANDOFF_HEADER, "x58_b06vi_profile_for_tuple")
        self.assertEqual(table.count("return X58_B06VI_PROFILE_PRIMARY_"), 4)
        self.assertEqual(table.count("return X58_B06VI_PROFILE_O;"), 1)
        self.assertEqual(table.count("return X58_B06VJ_PROFILE_Q;"), 1)
        self.assertIn("#if CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS", table)
        for value in (
            "X58_B06VJ_EXPECTED_WORKSPACE_RAW_Q_FNV",
            "X58_B06VJ_EXPECTED_WORKSPACE_CANONICAL_Q_FNV",
            "X58_B06VI_EXPECTED_CSI_DYNAMIC_O",
            "X58_B06VI_EXPECTED_CSI_RAW_O_FNV",
        ):
            self.assertIn(value, table)
        self.assertNotRegex(table, r"workspace_.*[<>]=?")
        self.assertNotRegex(table, r"(?<!&)&(?!&)")

    def test_platform_exception_and_rearm_use_exact_o_family_helpers(self) -> None:
        promotion = function_text(ROMSTAGE, "b06ve_promote_post_minit")
        admission = between(
            promotion, "probe_status_admitted =", "b06v8_capture_qpi_tuple"
        )
        self.assertIn("X58_VENDOR_ERR_PLATFORM_STATE", admission)
        self.assertIn("b06vi_o_workspace_pattern_exact", admission)
        self.assertIn("workspace_pattern_exact", admission)

        deferred = function_text(
            HANDOFF_HEADER, "x58_b06vi_profile_requires_deferred_rearm"
        )
        self.assertIn("X58_B06VI_PROFILE_O", deferred)
        self.assertIn("X58_B06VJ_PROFILE_Q", deferred)

        postmem = function_text(HANDOFF, "platform_romstage_post_mem")
        handoff_readback = postmem.index("x58_b06v6_handoff_is_valid(handoff)")
        deferred_at = postmem.index(
            "x58_b06vi_profile_requires_deferred_rearm", handoff_readback
        )
        rearm_at = postmem.index("x58_b06vi_rearm_o_guard_after_postmem", deferred_at)
        finalize_at = postmem.index("x58_b06ve_finalize_persistent_guard", rearm_at)
        postcar_at = postmem.index("entering postcar", finalize_at)
        self.assertEqual(
            (handoff_readback, deferred_at, rearm_at, finalize_at, postcar_at),
            tuple(sorted((handoff_readback, deferred_at, rearm_at, finalize_at, postcar_at))),
        )
        self.assertIn("x58_b06vi_profile_requires_deferred_rearm(profile_id)", promotion)


if __name__ == "__main__":
    unittest.main()
