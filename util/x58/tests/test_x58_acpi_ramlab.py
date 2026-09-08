"""Host-only contracts for the network GRUB lab; never exercise hardware."""

from pathlib import Path
import hashlib
import json
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
WRAPPER = ROOT / "scripts/build_x58_acpi_ramlab.sh"
CONFIG = ROOT / "research/msi/acpi-ramlab/grub.cfg"
DSDT = ROOT / "builds/experimental/b06wj-release-20260907/dsdt-from-rom.aml"
CANDIDATES = ROOT / "builds/experimental/acpi-ramlab/b06wj-20260908"
VARIANTS = ("vga-only", "s0-s5-only", "s0-only")
HAVE_CANDIDATES = all((CANDIDATES / name / "dsdt.aml").is_file() for name in VARIANTS)
LEGACY_ROOT = ROOT / "builds/experimental/acpi-ramlab/b06wj-legacy-windows-20260908"
LEGACY = LEGACY_ROOT / "legacy-windows/dsdt.aml"
IOH_ROOT = ROOT / "builds/experimental/acpi-ramlab/b06wj-ioh-prt-20260908"
IOH = IOH_ROOT / "ioh-prt-only/dsdt.aml"


class ACPIRamLabContract(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="x58-acpi-ramlab-hosttest-")
        self.addCleanup(self.tmp.cleanup)
        self.work = Path(self.tmp.name)
        self.mods = self.work / "i386-pc"
        self.mods.mkdir()
        self.modules = re.search(r"^modules='([^']+)'", WRAPPER.read_text(), re.M)[1].split()
        for name in ["kernel.img", "moddep.lst"] + [f"{m}.mod" for m in self.modules]:
            (self.mods / name).write_bytes(b"HOST-TEST-FIXTURE-NOT-A-GRUB-MODULE\n")
        self.fake = self.work / "fake-mkstandalone"
        self.fake.write_text(
            "#!/bin/sh\n"
            'if [ "$1" = --version ]; then echo HOST-TEST-ONLY; exit 0; fi\n'
            'while [ "$#" -gt 0 ]; do\n'
            '  if [ "$1" = -o ]; then shift; result=$1; fi\n'
            '  shift\n'
            'done\n'
            'printf "HOST-TEST-ONLY-NOT-BOOTABLE\\n" > "$result"\n'
        )
        self.fake.chmod(0o700)
        self.out = self.work / "output"

    def run_wrapper(self, *extra):
        return subprocess.run(
            ["bash", str(WRAPPER), "--mkstandalone", str(self.fake),
             "--modules-dir", str(self.mods), "--output-dir", str(self.out),
             *extra], capture_output=True, text=True, check=False
        )

    def test_default_is_observation_then_cli(self):
        commands = [line.split()[0] for line in CONFIG.read_text().splitlines()
                    if line.strip() and not line.lstrip().startswith("#")]
        self.assertEqual(set(commands), {
            "serial", "terminal_input", "terminal_output", "set", "echo",
            "lsacpi", "lsmmap", "ls",
        })
        self.assertIn("X58-ACPI-RAMLAB-01", CONFIG.read_text())
        self.assertNotIn("menuentry", commands)

    def test_module_contract_and_no_new_disk_driver(self):
        self.assertTrue({"acpi", "lsacpi", "lsmmap", "memrw", "iorw", "hexdump",
                         "lspci", "setpci", "biosdisk", "serial", "chain"}
                        .issubset(self.modules))
        self.assertFalse({"ahci", "ata", "usb", "ehci", "uhci"}.intersection(self.modules))

    def test_shell_syntax(self):
        result = subprocess.run(["bash", "-n", str(WRAPPER)], check=False)
        self.assertEqual(result.returncode, 0)

    @unittest.skipUnless(DSDT.is_file(), "local released native DSDT artifact absent")
    def test_mock_build_records_restricted_invocation(self):
        result = self.run_wrapper()
        self.assertEqual(result.returncode, 0, result.stderr)
        inputs = (self.out / "build-inputs.txt").read_text()
        self.assertIn("i386-pc-pxe", inputs)
        self.assertIn("--install-modules=", inputs)
        self.assertIn("boot/grub/dsdt-wj.aml=", inputs)
        self.assertNotIn("--core-compress", inputs)
        for variant in VARIANTS:
            self.assertNotIn(f"dsdt-{variant}.aml=", inputs)
        self.assertNotIn("dsdt-legacy-windows.aml=", inputs)
        self.assertNotIn("dsdt-ioh-prt.aml=", inputs)
        self.assertEqual((self.out / "x58-acpi-ramlab-01.pxe").read_bytes(),
                         b"HOST-TEST-ONLY-NOT-BOOTABLE\n")
        self.assertTrue((self.out / "image.sha256").is_file())

    @unittest.skipUnless(DSDT.is_file() and HAVE_CANDIDATES, "local AML artifacts absent")
    def test_optional_candidates_exactly_grafted_and_logged(self):
        result = self.run_wrapper("--candidates-dir", str(CANDIDATES))
        self.assertEqual(result.returncode, 0, result.stderr)
        inputs = (self.out / "build-inputs.txt").read_text()
        for variant in VARIANTS:
            candidate = CANDIDATES / variant / "dsdt.aml"
            self.assertIn(f"boot/grub/dsdt-{variant}.aml={candidate}", inputs)
            self.assertIn(hashlib.sha256(candidate.read_bytes()).hexdigest(), inputs)
        self.assertIn("boot/grub/dsdt-wj.aml=", inputs)
        self.assertNotIn("boot/grub/dsdt-control.aml=", inputs)
        self.assertNotIn("boot/grub/dsdt-legacy-windows.aml=", inputs)
        self.assertNotIn("boot/grub/dsdt-ioh-prt.aml=", inputs)

    @unittest.skipUnless(DSDT.is_file() and IOH.is_file(), "local AML artifacts absent")
    def test_ioh_variant_is_independent_opt_in(self):
        result = self.run_wrapper("--ioh-prt", str(IOH))
        self.assertEqual(result.returncode, 0, result.stderr)
        inputs = (self.out / "build-inputs.txt").read_text()
        self.assertIn(f"boot/grub/dsdt-ioh-prt.aml={IOH}", inputs)
        self.assertIn("boot/grub/dsdt-wj.aml=", inputs)
        for variant in (*VARIANTS, "legacy-windows"):
            self.assertNotIn(f"dsdt-{variant}.aml=", inputs)

    @unittest.skipUnless(DSDT.is_file() and HAVE_CANDIDATES and LEGACY.is_file()
                         and IOH.is_file(), "local AML artifacts absent")
    def test_all_optional_candidates_are_separate_grafts(self):
        result = self.run_wrapper("--candidates-dir", str(CANDIDATES),
                                  "--legacy-windows", str(LEGACY),
                                  "--ioh-prt", str(IOH))
        self.assertEqual(result.returncode, 0, result.stderr)
        inputs = (self.out / "build-inputs.txt").read_text()
        for variant in (*VARIANTS, "legacy-windows", "ioh-prt"):
            self.assertIn(f"boot/grub/dsdt-{variant}.aml=", inputs)

    @unittest.skipUnless(IOH.is_file(), "local IOH PRT AML artifact absent")
    def test_ioh_pin_matches_manifest_and_actual_bytes(self):
        manifest = json.loads((IOH_ROOT / "manifest.json").read_text())
        entry = next(v for v in manifest["variants"] if v["variant"] == "ioh-prt-only")
        digest = hashlib.sha256(IOH.read_bytes()).hexdigest()
        self.assertEqual(digest, entry["sha256"])
        self.assertIn(f"expected_ioh_prt={digest}", WRAPPER.read_text())

    @unittest.skipUnless(DSDT.is_file(), "local released native DSDT artifact absent")
    def test_modified_ioh_variant_rejected_before_output(self):
        bad = self.work / "wrong-ioh.aml"
        bad.write_bytes(b"wrong IOH PRT candidate")
        result = self.run_wrapper("--ioh-prt", str(bad))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("candidate hash mismatch: ioh-prt", result.stderr)
        self.assertFalse(self.out.exists())

    @unittest.skipUnless(DSDT.is_file() and LEGACY.is_file(), "local AML artifacts absent")
    def test_legacy_variant_is_independent_opt_in(self):
        result = self.run_wrapper("--legacy-windows", str(LEGACY))
        self.assertEqual(result.returncode, 0, result.stderr)
        inputs = (self.out / "build-inputs.txt").read_text()
        self.assertIn(f"boot/grub/dsdt-legacy-windows.aml={LEGACY}", inputs)
        self.assertIn("boot/grub/dsdt-wj.aml=", inputs)
        for variant in VARIANTS:
            self.assertNotIn(f"dsdt-{variant}.aml=", inputs)

    @unittest.skipUnless(DSDT.is_file() and HAVE_CANDIDATES and LEGACY.is_file(),
                         "local AML artifacts absent")
    def test_legacy_variant_combines_with_original_three(self):
        result = self.run_wrapper("--candidates-dir", str(CANDIDATES),
                                  "--legacy-windows", str(LEGACY))
        self.assertEqual(result.returncode, 0, result.stderr)
        inputs = (self.out / "build-inputs.txt").read_text()
        for variant in (*VARIANTS, "legacy-windows"):
            self.assertIn(f"boot/grub/dsdt-{variant}.aml=", inputs)

    @unittest.skipUnless(LEGACY.is_file(), "local legacy-windows AML artifact absent")
    def test_legacy_pin_matches_manifest_and_actual_bytes(self):
        manifest = json.loads((LEGACY_ROOT / "manifest.json").read_text())
        entry = next(v for v in manifest["variants"] if v["variant"] == "legacy-windows")
        digest = hashlib.sha256(LEGACY.read_bytes()).hexdigest()
        self.assertEqual(digest, entry["sha256"])
        self.assertIn(f"expected_legacy_windows={digest}", WRAPPER.read_text())

    @unittest.skipUnless(DSDT.is_file(), "local released native DSDT artifact absent")
    def test_modified_legacy_variant_rejected_before_output(self):
        bad = self.work / "wrong-legacy.aml"
        bad.write_bytes(b"wrong legacy windows candidate")
        result = self.run_wrapper("--legacy-windows", str(bad))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("candidate hash mismatch: legacy-windows", result.stderr)
        self.assertFalse(self.out.exists())

    @unittest.skipUnless(HAVE_CANDIDATES, "local AML artifacts absent")
    def test_candidate_pins_match_manifest_and_actual_bytes(self):
        manifest = json.loads((CANDIDATES / "manifest.json").read_text())
        entries = {entry["variant"]: entry for entry in manifest["variants"]}
        wrapper = WRAPPER.read_text()
        for variant in VARIANTS:
            digest = hashlib.sha256((CANDIDATES / variant / "dsdt.aml").read_bytes()).hexdigest()
            self.assertEqual(digest, entries[variant]["sha256"])
            self.assertIn(f"{variant}) expected_candidate={digest}", wrapper)

    @unittest.skipUnless(DSDT.is_file() and HAVE_CANDIDATES, "local AML artifacts absent")
    def test_modified_candidate_rejected_before_output(self):
        candidate_dir = self.work / "candidates"
        for variant in VARIANTS:
            target = candidate_dir / variant / "dsdt.aml"
            target.parent.mkdir(parents=True)
            target.write_bytes((CANDIDATES / variant / "dsdt.aml").read_bytes())
        (candidate_dir / "s0-only/dsdt.aml").write_bytes(b"wrong candidate")
        result = self.run_wrapper("--candidates-dir", str(candidate_dir))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("candidate hash mismatch: s0-only", result.stderr)
        self.assertFalse(self.out.exists())

    @unittest.skipUnless(DSDT.is_file(), "local released native DSDT artifact absent")
    def test_missing_candidate_rejected_before_output(self):
        empty = self.work / "empty-candidates"
        empty.mkdir()
        result = self.run_wrapper("--candidates-dir", str(empty))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing candidate: vga-only", result.stderr)
        self.assertFalse(self.out.exists())

    def test_wrong_dsdt_rejected_before_output(self):
        wrong = self.work / "wrong.aml"
        wrong.write_bytes(b"not the released DSDT")
        result = self.run_wrapper("--dsdt", str(wrong))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not the unchanged released", result.stderr)
        self.assertFalse(self.out.exists())

    @unittest.skipUnless(DSDT.is_file(), "local released native DSDT artifact absent")
    def test_missing_module_rejected_before_output(self):
        (self.mods / "setpci.mod").unlink()
        result = self.run_wrapper()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("setpci.mod", result.stderr)
        self.assertFalse(self.out.exists())

    @unittest.skipUnless(DSDT.is_file(), "local released native DSDT artifact absent")
    def test_existing_directory_never_overwritten(self):
        self.out.mkdir()
        sentinel = self.out / "keep"
        sentinel.write_bytes(b"existing user result")
        result = self.run_wrapper()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("refusing overwrite", result.stderr)
        self.assertEqual(sentinel.read_bytes(), b"existing user result")


if __name__ == "__main__":
    unittest.main()
