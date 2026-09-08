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
DEFCONFIG = (ROOT / "configs/x58-pro-e-b06vk.config").read_text()
BUILD_SCRIPT = (ROOT / "scripts/build_x58_b06vk.sh").read_text()

VK_SYMBOL = "CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS"
VJ_SYMBOL = "CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS"
VI_SYMBOL = "CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS"
VK_ID = "X58PROE-B06VK-Q-CANONICAL-CLASS-SEABIOS-20260905"
VJ_ID = "X58PROE-B06VJ-COUPLED-PROFILE-Q-SEABIOS-20260905"

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

CANONICAL_RANGES = (
    (0x132C, 0x136D),
    (0x13BE, 0x13FD),
    (0x1859, 0x185C),
    (0x18E7, 0x18E9),
    (0x1AA4, 0x1AA7),
    (0x23A6, 0x23A6),
    (0x2411, 0x2434),
    (0x2459, 0x245C),
    (0x2461, 0x2461),
    (0x24A1, 0x24C4),
    (0x251F, 0x252E),
)

LIVE_Q_DIFF_OFFSETS = (
    0x1336,
    0x1338,
    0x1346,
    0x134C,
    0x1350,
    0x1358,
    0x1360,
    0x136A,
    0x136C,
    0x13C8,
    0x13CE,
    0x13F0,
    0x13FC,
    0x242D,
    0x24BD,
    0x252D,
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
    with tempfile.TemporaryDirectory(prefix="b06vk-contract-", dir="/tmp") as temp:
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


class B06VKSourceContractTests(unittest.TestCase):
    def test_executable_q_canonical_class_truth_table_and_cross_product(self) -> None:
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

            static const uint32_t raw_candidates[] = {
                    0x94299f43, 0xb6346533, 0xeb15c076, 0x5fb636d9,
                    0x50f67315, 0x69b4c386, 0xdd61cb51, 0x00000000,
                    0xffffffff,
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
                    _Static_assert(X58_B06V6_HANDOFF_VERSION == 8,
                                   "B06VK wire version");
                    _Static_assert(sizeof(struct x58_b06v6_handoff) == 160,
                                   "B06VK wire size");
                    _Static_assert(X58_B06VJ_PROFILE_Q == 6,
                                   "Q wire ID must not change");

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

                    /* Raw is non-semantic only for exact canonical Q. */
                    for (unsigned int raw = 0;
                         raw < sizeof(raw_candidates) / sizeof(raw_candidates[0]);
                         raw++) {
                            struct row q = rows[5];
                            q.work_raw = raw_candidates[raw];
                            if (select_row(&q) != X58_B06VJ_PROFILE_Q ||
                                !x58_b06vi_profile_tuple_exact(
                                    X58_B06VJ_PROFILE_Q, q.pre_a0, q.post_a0,
                                    q.post_9c, q.csi_raw, q.csi_canonical,
                                    q.work_raw, q.work_canonical))
                                    return 40 + (int)raw;
                    }

                    /* Every field remains mandatory except Q workspace raw. */
                    for (unsigned int row = 0; row < 6; row++) {
                            for (unsigned int field = 0; field < 8; field++) {
                                    struct row changed = rows[row];
                                    uint32_t *first = &changed.pre_a0;

                                    first[field] ^= 1u;
                                    if (row == 5 && field == 6) {
                                            if (select_row(&changed) !=
                                                X58_B06VJ_PROFILE_Q)
                                                    return 60;
                                    } else if (select_row(&changed) !=
                                               X58_B06VI_PROFILE_INVALID) {
                                            return 61 + (int)(row * 8 + field);
                                    }
                            }
                    }

                    /* Cross CPU, CSI dynamic/raw/canonical and workspace pairs. */
                    for (unsigned int cpu = 0; cpu < 2; cpu++) {
                            for (unsigned int dynamic = 0; dynamic < 2; dynamic++) {
                                    for (unsigned int csi_raw = 0; csi_raw < 2;
                                         csi_raw++) {
                                            for (unsigned int csi_canon = 0;
                                                 csi_canon < 2; csi_canon++) {
                                                    for (unsigned int raw = 0;
                                                         raw < sizeof(raw_candidates) /
                                                               sizeof(raw_candidates[0]);
                                                         raw++) {
                                                            for (unsigned int canon = 0;
                                                                 canon < 6; canon++) {
                                                                    struct row crossed = rows[raw < 6 ? raw : 5];
                                                                    uint32_t expected = 0;

                                                                    crossed.pre_a0 = rows[cpu ? 4 : 0].pre_a0;
                                                                    crossed.post_a0 = rows[cpu ? 4 : 0].post_a0;
                                                                    crossed.post_9c = rows[cpu ? 4 : 0].post_9c;
                                                                    crossed.csi_dynamic = rows[dynamic ? 4 : 0].csi_dynamic;
                                                                    crossed.csi_raw = rows[csi_raw ? 4 : 0].csi_raw;
                                                                    crossed.csi_canonical = csi_canon ?
                                                                            0x908dabb7 : 0x908dabb6;
                                                                    crossed.work_raw = raw_candidates[raw];
                                                                    crossed.work_canonical = rows[canon].work_canonical;

                                                                    if (!csi_canon && !cpu && !dynamic && !csi_raw) {
                                                                            if (raw < 4 && crossed.work_canonical ==
                                                                                rows[raw].work_canonical)
                                                                                    expected = rows[raw].id;
                                                                    }
                                                                    if (!csi_canon && cpu && dynamic && csi_raw) {
                                                                            if (raw == 4 && crossed.work_canonical ==
                                                                                rows[4].work_canonical)
                                                                                    expected = rows[4].id;
                                                                            if (crossed.work_canonical ==
                                                                                rows[5].work_canonical)
                                                                                    expected = rows[5].id;
                                                                    }
                                                                    if (select_row(&crossed) != expected)
                                                                            return 120;
                                                            }
                                                    }
                                            }
                                    }
                            }
                    }

                    if (x58_b06vi_profile_requires_deferred_rearm(1) ||
                        x58_b06vi_profile_requires_deferred_rearm(4) ||
                        !x58_b06vi_profile_requires_deferred_rearm(5) ||
                        !x58_b06vi_profile_requires_deferred_rearm(6))
                            return 121;

                    puts("B06VK Q-canonical truth-table harness PASS");
                    return 0;
            }
        '''
        result = compile_and_run(textwrap.dedent(harness))
        self.assertEqual(
            result.returncode,
            0,
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}",
        )
        self.assertIn("Q-canonical truth-table harness PASS", result.stdout)

    def test_q_marker_helper_accepts_raw_telemetry_and_rejects_mutations(self) -> None:
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

            #define CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS 1
            #define X58_B06VJ_EXPECTED_WORKSPACE_RAW_Q_FNV 0x69b4c386u
            #define X58_B06VJ_EXPECTED_WORKSPACE_CANONICAL_Q_FNV 0x0b161f01u

            {helper}

            int main(void)
            {{
                    uint8_t workspace[0x2bcc] = {{ 0 }};
                    static const uint16_t offsets[] = {{ {offsets} }};
                    static const uint8_t values[] = {{ {values} }};
                    static const uint32_t raws[] = {{
                            0x69b4c386u, 0xdd61cb51u, 0u, 0xffffffffu
                    }};

                    {assignments}
                    for (size_t i = 0; i < sizeof(raws) / sizeof(raws[0]); i++) {{
                            if (!b06vj_q_workspace_pattern_exact(workspace,
                                        raws[i], 0x0b161f01u))
                                    return 1;
                    }}
                    if (b06vj_q_workspace_pattern_exact(NULL,
                                0xdd61cb51u, 0x0b161f01u) ||
                        b06vj_q_workspace_pattern_exact(workspace,
                                0xdd61cb51u, 0x0b161f00u))
                            return 2;
                    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]);
                         i++) {{
                            workspace[offsets[i]] = values[i] ^ 1u;
                            if (b06vj_q_workspace_pattern_exact(workspace,
                                        0xdd61cb51u, 0x0b161f01u))
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

    def test_vj_and_vi_wire_and_raw_regressions(self) -> None:
        common = r'''
            #include <stdint.h>
            #define CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI 1
            #define CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF 1
            #define CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY 1
            #define CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS 1
        '''
        vj = common + r'''
            #define CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS 1
            #define CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS 0
            #include "b06v6_handoff.h"
            int main(void)
            {
                    _Static_assert(X58_B06V6_HANDOFF_VERSION == 7,
                                   "B06VJ wire version changed");
                    if (x58_b06vi_profile_for_tuple(
                                0x17c00, 0x17c00, 0x00a00502, 0x0c,
                                0x8b38506a, 0x908dabb6,
                                0x69b4c386, 0x0b161f01) != 6)
                            return 1;
                    if (x58_b06vi_profile_for_tuple(
                                0x17c00, 0x17c00, 0x00a00502, 0x0c,
                                0x8b38506a, 0x908dabb6,
                                0xdd61cb51, 0x0b161f01) != 0)
                            return 2;
                    return 0;
            }
        '''
        vi = common + r'''
            #define CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS 0
            #define CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS 0
            #include "b06v6_handoff.h"
            int main(void)
            {
                    _Static_assert(X58_B06V6_HANDOFF_VERSION == 6,
                                   "B06VI wire version changed");
                    if (x58_b06vi_profile_for_tuple(
                                0x17c00, 0x17c00, 0x00a00502, 0x0c,
                                0x8b38506a, 0x908dabb6,
                                0x50f67315, 0x92c70df4) != 5)
                            return 1;
                    if (x58_b06vi_profile_for_tuple(
                                0x17c00, 0x17c00, 0x00a00502, 0x0c,
                                0x8b38506a, 0x908dabb6,
                                0x69b4c386, 0x0b161f01) != 0)
                            return 2;
                    return 0;
            }
        '''
        for source in (vj, vi):
            result = compile_and_run(textwrap.dedent(source))
            self.assertEqual(
                result.returncode,
                0,
                f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}",
            )

    def test_canonical_ranges_are_frozen_and_cover_only_observed_diffs(self) -> None:
        initializer = between(
            ROMSTAGE,
            "static const struct b06v7_zero_range b06vf_workspace_dynamic_ranges[]",
            "#endif\n\n_Static_assert",
        )
        parsed = tuple(
            (int(first, 16), int(last, 16))
            for first, last in re.findall(
                r"\{\s*0x([0-9a-f]+),\s*0x([0-9a-f]+)\s*\}", initializer
            )
        )
        self.assertEqual(parsed, CANONICAL_RANGES)
        self.assertEqual(sum(last - first + 1 for first, last in parsed), 235)
        for offset in LIVE_Q_DIFF_OFFSETS:
            self.assertTrue(
                any(first <= offset <= last for first, last in parsed),
                f"0x{offset:04x} escaped the frozen canonical ranges",
            )
        marker_offsets = {offset for offset, _ in Q_MARKERS}
        self.assertTrue(set(LIVE_Q_DIFF_OFFSETS).isdisjoint(marker_offsets))
        self.assertEqual(
            {
                offset
                for offset in marker_offsets
                if any(first <= offset <= last for first, last in parsed)
            },
            {0x2459, 0x245A, 0x245B, 0x245C, 0x2461},
        )

    def test_kconfig_defconfig_identity_and_build_helper_are_distinct(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        self.assertIn("depends on X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS", option)
        self.assertIn("default n", option)
        self.assertNotIn("default y", option)
        for evidence in (
            "0x69b4c386",
            "0xdd61cb51",
            "0x0b161f01",
            "16 differing bytes",
            "0x00017c00",
            "0x00a00502",
            "0x0c/0x8b38506a/0x908dabb6",
            "all 26",
            "version-8",
        ):
            self.assertIn(evidence, option)

        for symbol in (VK_SYMBOL, VJ_SYMBOL, VI_SYMBOL):
            self.assertIn(f"{symbol}=y", DEFCONFIG)
        self.assertIn("CONFIG_PAYLOAD_SEABIOS=y", DEFCONFIG)
        self.assertIn('CONFIG_LOCALVERSION="x58-pro-e-b06vk"', DEFCONFIG)
        self.assertIn(
            'CONFIG_MAINBOARD_PART_NUMBER="X58 Pro-E B06VK profile Q canonical class SeaBIOS probe"',
            DEFCONFIG,
        )

        for source in (BOOTBLOCK, ROMSTAGE, RAMMON):
            self.assertIn(VK_ID, source)
            self.assertIn(VJ_ID, source)
            self.assertLess(source.index(VK_ID), source.index(VJ_ID))
        self.assertLess(MAINBOARD.index(VK_SYMBOL), MAINBOARD.index(VJ_SYMBOL))

        defaults = KCONFIG[KCONFIG.index("config PAYLOAD_CONFIGFILE") :]
        self.assertLess(
            defaults.index(VK_SYMBOL.removeprefix("CONFIG_")),
            defaults.index(VJ_SYMBOL.removeprefix("CONFIG_")),
        )
        parts = KCONFIG[KCONFIG.index("config MAINBOARD_PART_NUMBER") :]
        self.assertLess(parts.index("B06VK"), parts.index("B06VJ"))

        for token in (
            "configs/x58-pro-e-b06vk.config",
            "msi-x58-pro-e-b06vk-coreboot-base-4MiB.rom",
            "blobs-local/msi-x58-pro-e/b06vk",
            "tests.test_x58_b06vi_source_contract",
            "tests.test_x58_b06vj_source_contract",
            "tests.test_x58_b06vk_source_contract",
            "install_exact",
            "verify-composite",
            "x58_b06v9_wrapper_patch.py\" verify",
            "--flash-size 0x1000000",
        ):
            self.assertIn(token, BUILD_SCRIPT)

    def test_only_q_raw_is_conditional_and_marker_gate_remains_mandatory(self) -> None:
        table = function_text(HANDOFF_HEADER, "x58_b06vi_profile_for_tuple")
        self.assertEqual(table.count("return X58_B06VI_PROFILE_PRIMARY_"), 4)
        self.assertEqual(table.count("return X58_B06VI_PROFILE_O;"), 1)
        self.assertEqual(table.count("return X58_B06VJ_PROFILE_Q;"), 1)
        self.assertEqual(
            table.count("#if !CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS"),
            1,
        )
        conditional = between(
            table,
            "#if !CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS",
            "#endif",
        )
        self.assertIn("X58_B06VJ_EXPECTED_WORKSPACE_RAW_Q_FNV", conditional)
        for exact_raw in (
            "X58_B06VF_EXPECTED_WORKSPACE_RAW_A_FNV",
            "X58_B06VF_EXPECTED_WORKSPACE_RAW_B_FNV",
            "X58_B06VH_EXPECTED_WORKSPACE_RAW_P_FNV",
            "X58_B06VH_EXPECTED_WORKSPACE_RAW_N_FNV",
            "X58_B06VI_EXPECTED_WORKSPACE_RAW_O_FNV",
        ):
            self.assertIn(exact_raw, table)
            self.assertNotIn(exact_raw, conditional)
        self.assertNotRegex(table, r"workspace_.*[<>]=?")
        self.assertNotRegex(table, r"(?<!&)&(?!&)")

        promotion = function_text(ROMSTAGE, "b06ve_promote_post_minit")
        self.assertIn("workspace_pattern_exact", promotion)
        self.assertIn("b06vi_workspace_pattern_exact", promotion)
        self.assertIn("b06vi_o_workspace_pattern_exact", promotion)
        self.assertIn("profile_id == X58_B06VI_PROFILE_INVALID", promotion)
        self.assertIn("x58_b06vi_profile_tuple_exact", promotion)
        for exact_gate in (
            "X58_B06V6_EXPECTED_POLICY_FNV",
            "b06vi_post_minit_common_endpoint_exact",
            "post_minit_i801_exact",
            "B06V6_CMOS_GUARD_IN_PROGRESS",
            "workspace[1] != 0",
            "workspace[2] != 0",
            "B06V5_WORK_B3_FLAGS_OFFSET",
            "B06V5_WORK_COMPLETE_OFFSET",
            "X58_B06V6_EXPECTED_MC_MAPPER",
            "X58_B06V6_EXPECTED_MC_COMMON_F8",
            "X58_B06V6_EXPECTED_CH2_DOD",
            "X58_B06V6_EXPECTED_CH2_RANKS",
            "X58_B06V6_EXPECTED_CH2_STATUS",
        ):
            self.assertIn(exact_gate, promotion)
        self.assertIn("Q_RAW_MODE=TELEMETRY_ONLY_FOR_PROFILE_06", promotion)
        self.assertIn("WORK_RAW_TELEMETRY", promotion)

    def test_v8_handoff_consumer_and_o_family_order_remain_fail_closed(self) -> None:
        validator = function_text(HANDOFF, "x58_b06v6_raminit_result_is_exact")
        self.assertIn("x58_b06vi_profile_tuple_exact", validator)
        self.assertIn("result->workspace_fnv", validator)
        handoff_validator = function_text(HANDOFF, "x58_b06v6_handoff_is_valid")
        self.assertIn("handoff->version != X58_B06V6_HANDOFF_VERSION", handoff_validator)
        self.assertIn("handoff->digest == x58_b06v6_handoff_digest", handoff_validator)

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
            tuple(
                sorted(
                    (handoff_readback, deferred_at, rearm_at, finalize_at, postcar_at)
                )
            ),
        )
        for evidence in (
            "B06VK_O_FAMILY_POSTMEM_REARM",
            "B06VK_GUARD_FINALIZE",
            "full v8 postmem readback",
            "after v8 CBMEM readback",
        ):
            self.assertIn(evidence, postmem)
        self.assertIn("B06VK recovery ROMMON", ROMSTAGE)
        self.assertIn("manual vendor calls remain disabled", ROMSTAGE)


if __name__ == "__main__":
    unittest.main()
