from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import stat
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).parents[1] / "scripts" / "run_uefi_report.py"
SPEC = importlib.util.spec_from_file_location("run_uefi_report", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class UefiReportRunnerTests(unittest.TestCase):
    def test_binds_report_and_diagnostics_to_input(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            image = root / "fixture.fd"
            image.write_bytes(b"firmware")
            extractor = root / "fake-extractor"
            extractor.write_text(
                "#!/bin/sh\n"
                "if [ \"$1\" = --version ]; then echo fake-1; exit 0; fi\n"
                "printf report > \"$1.report.txt\"\n"
                "echo parser-warning >&2\n",
                encoding="utf-8",
            )
            extractor.chmod(extractor.stat().st_mode | stat.S_IXUSR)

            metadata, path = MODULE.generate_report(
                extractor, image, "deadbeef"
            )
            stored = json.loads(path.read_text(encoding="utf-8"))

        self.assertEqual(metadata["returncode"], 0)
        self.assertEqual(stored["input"]["size"], 8)
        self.assertEqual(stored["report"]["size"], 6)
        self.assertIn("parser-warning", stored["stderr"])
        self.assertEqual(stored["tool_commit"], "deadbeef")


if __name__ == "__main__":
    unittest.main()
