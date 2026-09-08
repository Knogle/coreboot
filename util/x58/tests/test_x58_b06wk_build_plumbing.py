"""Host-only B06WK identity/configuration contracts; no firmware builds or I/O."""

from pathlib import Path
import re
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
SYMBOL = "CONFIG_X58_PRO_E_B06WK_ACPI_REPAIR"
BUILD_ID = "X58PROE-B06WK-ACPI-REPAIR-20260908"
PREVIOUS_ID = "X58PROE-B06WJ-ACPI-PLATFORM-20260907"


def source(name):
    return (BOARD / name).read_text()


def config(name):
    settings = {}
    for line in (ROOT / "configs" / name).read_text().splitlines():
        match = re.fullmatch(r"(CONFIG_\w+)=(.*)", line)
        disabled = re.fullmatch(r"# (CONFIG_\w+) is not set", line)
        if match:
            settings[match[1]] = match[2]
        elif disabled:
            settings[disabled[1]] = "n"
    return settings


class B06WKBuildPlumbingTests(unittest.TestCase):
    def test_working_config_adds_selector_identity_and_pending_spd_fix(self):
        before = config("x58-pro-e-b06wj.config")
        after = config("x58-pro-e-b06wk.config")
        self.assertEqual(after.pop(SYMBOL), "y")
        self.assertNotIn(SYMBOL, before)
        self.assertEqual(after.pop("CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT"), "y")
        self.assertNotIn("CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT", before)
        self.assertEqual(after.pop("CONFIG_MAINBOARD_PART_NUMBER"),
                         '"X58 Pro-E B06WK ACPI repair experiment"')
        self.assertEqual(after.pop("CONFIG_LOCALVERSION"), '"x58-pro-e-b06wk"')
        before.pop("CONFIG_MAINBOARD_PART_NUMBER")
        before.pop("CONFIG_LOCALVERSION")
        self.assertEqual(after, before)

    def test_selector_default_off_and_enabled_only_by_wk_config(self):
        block = source("Kconfig").split("config " + SYMBOL[7:] + "\n", 1)[1]
        block = block.split("\nconfig ", 1)[0]
        self.assertIn("depends on X58_PRO_E_B06WJ_ACPI_PLATFORM", block)
        self.assertIn("depends on PAYLOAD_SEABIOS", block)
        self.assertIn("\tdefault n\n", block)
        self.assertNotIn("\tselect ", block)
        enabled = [p.name for p in (ROOT / "configs").glob("*.config")
                   if re.search(r"^" + SYMBOL + r"=y$", p.read_text(), re.M)]
        self.assertEqual(sorted(enabled), ["x58-pro-e-b06wk.config",
                                           "x58-pro-e-development.config"])

    def test_board_identity_precedes_inherited_identity(self):
        for name in ("bootblock.c", "romstage.c", "ramstage_rommon.c",
                     "b06wg_usb_auto.h", "b06vn_pci.c", "mainboard.c"):
            with self.subTest(name=name):
                text = source(name)
                if name == "b06vn_pci.c":
                    text = text.split("#define B06VN_BUILD_ID B06WK_BUILD_ID", 1)[1]
                    self.assertIn("#elif CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM", text)
                else:
                    self.assertLess(text.index(SYMBOL),
                                    text.index("CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM"))
        self.assertEqual(source("romstage.c").count(BUILD_ID), 2)
        self.assertIn(BUILD_ID + ' CPUID="', source("romstage.c"))
        self.assertIn("[PAYLOAD] B06WK ACPI repair experiment", source("ramstage_rommon.c"))

    def test_usb_identity_preprocessing_preserves_wj_when_wk_off(self):
        for enabled, expected, stage in ((1, BUILD_ID, "B06WK-AUTO-GPIO57-USB1"),
                                          (0, PREVIOUS_ID, "B06WJ-AUTO-GPIO57-USB1")):
            result = subprocess.run(
                ["cpp", "-P", "-x", "c", "-I", str(BOARD), f"-D{SYMBOL}={enabled}",
                 "-DCONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM=1", "-"],
                input=source("b06wg_usb_auto.h") + "\nB06WG_BUILD_ID\nB06WG_STAGE_ID\n",
                capture_output=True, text=True, check=True)
            self.assertEqual(result.stdout.split(), [f'"{expected}"', f'"{stage}"'])

    def test_common_table_header_exposes_wk_without_losing_wj_literal(self):
        header = source("b06wj_acpi.h")
        self.assertIn(f'#define B06WK_BUILD_ID "{BUILD_ID}"', header)
        self.assertIn(f"#elif {SYMBOL}\n#define B06WJ_BUILD_ID B06WK_BUILD_ID", header)
        self.assertIn(f'#else\n#define B06WJ_BUILD_ID "{PREVIOUS_ID}"', header)

    def test_builder_wrapper_and_shell_syntax(self):
        wrapper = ROOT / "scripts/build_x58_b06wk.sh"
        shared = ROOT / "scripts/build_x58_b06_sata_successor.sh"
        self.assertIn('scripts/build_x58_b06_sata_successor.sh" b06wk', wrapper.read_text())
        self.assertTrue(wrapper.stat().st_mode & 0o111)
        subprocess.run(["bash", "-n", str(wrapper)], check=True, capture_output=True)
        subprocess.run(["bash", "-n", str(shared)], check=True, capture_output=True)

    def test_builder_enforces_wk_identity_and_inherited_payload_policy(self):
        script = (ROOT / "scripts/build_x58_b06_sata_successor.sh").read_text()
        self.assertIn(f'\tb06wk)\n\t\tbuild_id="{BUILD_ID}"\n\t\ttrace=1', script)
        for line in (f"'{SYMBOL}=y'", f"'{SYMBOL}=n'",
                     "'CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM=y'",
                     "'CONFIG_HPET_MIN_TICKS=0x80'",
                     "'CONFIG_MAINBOARD_PART_NUMBER=\"X58 Pro-E B06WK ACPI repair experiment\"'",
                     "'CONFIG_LOCALVERSION=\"x58-pro-e-b06wk\"'"):
            self.assertIn(line, script)
        self.assertIn("b06wf|b06wg|b06wh|b06wi|b06wj|b06wk)", script)
        self.assertRegex(script, r'"\$\{variant\}" == b06wj \|\| "\$\{variant\}" == b06wk \]\]; then\n\s*require_line "\$\{seabios_config\}" \'CONFIG_DEBUG_LEVEL=6\'')
        self.assertIn('cmp -s -- "${first_rom}" "${second_rom}"', script)
        self.assertIn("refusing to replace differing artifact", script)
        self.assertIn("--flash-size 0x1000000", script)
        self.assertIn("existing releases are immutable", script)


if __name__ == "__main__":
    unittest.main()
