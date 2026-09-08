"""Public-fork path/bootstrap/identity tests: mock Git, never build firmware."""

import importlib.util
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

from x58_test_paths import COREBOOT_ROOT, TOOLS_ROOT, coreboot_root


def load_script(name):
    spec = importlib.util.spec_from_file_location(name, TOOLS_ROOT / "scripts" / f"{name}.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


IPXE = load_script("x58_validate_ipxe")
MANIFEST = load_script("x58_build_manifest")

# Deliberately synthetic Git process: all changes stay inside host tempdirs.
# It records commands but never invokes Git, a compiler or a network client.
FAKE_GIT = r'''#!/usr/bin/env python3
import json, os, pathlib, sys
args = sys.argv[1:]
directory = pathlib.Path.cwd()
while args and args[0] in ("-C", "-c"):
    option, value = args[:2]
    args = args[2:]
    if option == "-C": directory = pathlib.Path(value)
with open(os.environ["X58_TEST_GIT_LOG"], "a") as log:
    log.write(json.dumps(args) + "\n")
command = args[0]
state = directory / ".fake-revision"
if command == "clone":
    origin, destination = args[-2:]
    target = pathlib.Path(destination)
    target.mkdir(parents=True)
    (target / ".fake-revision").write_text("unselected")
elif command == "checkout":
    state.write_text(args[-1])
elif command == "rev-parse":
    print(str(directory) if args[-1] == "--show-toplevel" else state.read_text())
elif command == "diff":
    sys.exit(1 if os.environ.get("X58_TEST_DIRTY") else 0)
elif command == "commit":
    state.write_text("5497f43189374647b3b0f282aa497e71c891f3c5")
elif command not in ("status", "cat-file", "apply", "add", "fetch"):
    raise SystemExit("unexpected mock Git command: " + command)
'''


class PublicBuildTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="x58-public-build-hosttest-")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.env = dict(os.environ)
        for name in ("BASH_ENV", "ENV", "X58_VENDOR_ROM", "X58_OUTPUT_DIR",
                     "X58_TEST_DIRTY", "PYTHONOPTIMIZE"):
            self.env.pop(name, None)
        self.env["PYTHONDONTWRITEBYTECODE"] = "1"

    def make_tree(self):
        root = self.work / "fork"
        tools = root / "util/x58"
        scripts = tools / "scripts"
        scripts.mkdir(parents=True)
        board = root / "src/mainboard/msi/x58_pro_e"
        board.mkdir(parents=True)
        (board / "Kconfig").write_text("# host fixture, not firmware configuration\n")
        (root / "Makefile").write_text("# never executed\n")
        for name in ("prepare_x58_payloads.sh", "x58_layout.sh", "x58_payload_pins.sh"):
            shutil.copyfile(TOOLS_ROOT / "scripts" / name, scripts / name)
        (tools / "patches").mkdir()
        patch_name = "seabios-b52ca86-usb-deferred-trace.patch"
        shutil.copyfile(TOOLS_ROOT / "patches" / patch_name, tools / "patches" / patch_name)
        fakebin = self.work / "bin"
        fakebin.mkdir()
        (fakebin / "git").write_text(FAKE_GIT)
        (fakebin / "git").chmod(0o700)
        self.env["PATH"] = str(fakebin) + os.pathsep + self.env["PATH"]
        self.env["X58_TEST_GIT_LOG"] = str(self.work / "git.jsonl")
        return root, tools

    def events(self):
        path = Path(self.env["X58_TEST_GIT_LOG"])
        return [json.loads(line) for line in path.read_text().splitlines()] if path.exists() else []

    def prepare(self, tools, *args):
        return subprocess.run(["bash", str(tools / "scripts/prepare_x58_payloads.sh"), *args],
                              env=self.env, cwd=self.work, capture_output=True, text=True)

    def test_real_fork_root_is_not_a_nested_coreboot_directory(self):
        self.assertEqual(COREBOOT_ROOT, TOOLS_ROOT.parent.parent)
        self.assertTrue((COREBOOT_ROOT / "src/mainboard/msi/x58_pro_e/Kconfig").is_file())

    def test_both_layouts_and_unknown_layout(self):
        root, tools = self.make_tree()
        self.assertEqual(coreboot_root(tools), root)
        workspace = self.work / "workspace"
        (workspace / "coreboot/src/mainboard/msi/x58_pro_e").mkdir(parents=True)
        (workspace / "coreboot/src/mainboard/msi/x58_pro_e/Kconfig").touch()
        (workspace / "coreboot/Makefile").touch()
        self.assertEqual(coreboot_root(workspace), workspace / "coreboot")
        with self.assertRaises(RuntimeError):
            coreboot_root(self.work / "unknown")

    def test_check_missing_checkout_does_not_call_git_or_fetch(self):
        _, tools = self.make_tree()
        result = self.prepare(tools, "--check")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing payload checkout", result.stderr)
        self.assertEqual(self.events(), [])

    def test_mock_bootstrap_and_subsequent_read_only_check(self):
        root, tools = self.make_tree()
        result = self.prepare(tools)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("No firmware image was built", result.stdout)
        commands = self.events()
        self.assertEqual(sum(event[0] == "clone" for event in commands), 3)
        self.assertEqual(sum(event[0] == "commit" for event in commands), 1)
        checkpoint = len(commands)
        result = self.prepare(tools, "--check")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse({event[0] for event in self.events()[checkpoint:]}
                         & {"clone", "checkout", "fetch", "apply", "add", "commit"})
        self.assertFalse((root / "build").exists())

    def test_dirty_payload_refused_before_mutating_git_command(self):
        _, tools = self.make_tree()
        self.assertEqual(self.prepare(tools).returncode, 0)
        checkpoint = len(self.events())
        self.env["X58_TEST_DIRTY"] = "1"
        self.assertNotEqual(self.prepare(tools).returncode, 0)
        self.assertFalse({event[0] for event in self.events()[checkpoint:]}
                         & {"clone", "checkout", "fetch", "apply", "add", "commit"})

    def test_modified_patch_refused_without_git(self):
        _, tools = self.make_tree()
        (tools / "patches/seabios-b52ca86-usb-deferred-trace.patch").write_text("bad\n")
        self.assertNotEqual(self.prepare(tools).returncode, 0)
        self.assertEqual(self.events(), [])

    def test_dev_and_legacy_entry_fail_closed_without_firmware_build(self):
        for script, needle in (("build_x58_development.sh", "set X58_VENDOR_ROM"),
                               ("build_x58_b06wk.sh", "historical release identity")):
            result = subprocess.run(["bash", str(TOOLS_ROOT / "scripts" / script)],
                                    env=self.env, cwd=self.work, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(needle, result.stderr)

    def test_dev_config_diff_is_only_explicit_identity(self):
        before = (TOOLS_ROOT / "configs/x58-pro-e-b06wk.config").read_text()
        after = (TOOLS_ROOT / "configs/x58-pro-e-development.config").read_text()
        settings = lambda text: dict(line.split("=", 1) for line in text.splitlines()
                                     if line.startswith("CONFIG_") and "=" in line)
        old, new = settings(before), settings(after)
        self.assertEqual(new.pop("CONFIG_MAINBOARD_PART_NUMBER"), '"X58 Pro-E development SPD10"')
        self.assertEqual(new.pop("CONFIG_LOCALVERSION"), '"x58-pro-e-development-spd10"')
        old.pop("CONFIG_MAINBOARD_PART_NUMBER")
        old.pop("CONFIG_LOCALVERSION")
        self.assertEqual(old, new)

    def test_development_build_has_nonhistorical_payload_gate_and_effective_epoch(self):
        text = (TOOLS_ROOT / "scripts/build_x58_b06_sata_successor.sh").read_text()
        self.assertIn('make -C "${coreboot_dir}" SOURCE_DATE_EPOCH="${epoch}" -j', text)
        self.assertIn('make -C "${coreboot_dir}" SOURCE_DATE_EPOCH="${epoch}" olddefconfig', text)
        self.assertIn('python3 "${repo_root}/scripts/x58_validate_ipxe.py" "${ipxe_rom}"', text)
        self.assertIn('variant=development-spd10', text)
        self.assertIn('build_id="X58PROE-DEVELOPMENT-SPD10"', text)

    def test_manifest_write_is_idempotent_and_refuses_replacement(self):
        path = self.work / "manifest.json"
        MANIFEST.write_exact(path, {"synthetic": True})
        MANIFEST.write_exact(path, {"synthetic": True})
        with self.assertRaises(ValueError):
            MANIFEST.write_exact(path, {"synthetic": False})


class IpxeOptionRomTests(unittest.TestCase):
    def image(self):
        data = bytearray(512)
        data[:3] = b"\x55\xaa\x01"
        struct.pack_into("<H", data, 0x18, 0x20)
        data[0x20:0x24] = b"PCIR"
        struct.pack_into("<HH", data, 0x24, 0x10ec, 0x8168)
        struct.pack_into("<H", data, 0x30, 1)
        data[0x35] = 0x80
        data[-1] = -sum(data) & 255
        return data

    def test_synthetic_valid_rom(self):
        self.assertEqual(len(IPXE.validate(self.image())), 64)

    def test_changed_checksum_bad_id_and_truncated_data(self):
        original = self.image()
        for offset in (0, 2, 0x18, 0x20, 0x24, 0x26, 0x30, 0x34, 0x35):
            with self.subTest(offset=offset):
                changed = bytearray(original)
                changed[offset] ^= 1
                changed[-1] = 0
                changed[-1] = -sum(changed) & 255
                with self.assertRaises(ValueError):
                    IPXE.validate(changed)
        original[-1] ^= 1
        with self.assertRaises(ValueError):
            IPXE.validate(original)
        with self.assertRaises(ValueError):
            IPXE.validate(bytes(64))
