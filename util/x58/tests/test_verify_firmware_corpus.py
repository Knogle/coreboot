from __future__ import annotations

import hashlib
from pathlib import Path
import sys
import tempfile
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

from verify_firmware_corpus import verify_artifact


class VerifyFirmwareCorpusTests(unittest.TestCase):
    def test_verified_mismatch_and_missing(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            body = b"firmware fixture"
            (root / "fixture.bin").write_bytes(body)
            artifact = {
                "local_path": "fixture.bin",
                "size": len(body),
                "sha256": hashlib.sha256(body).hexdigest(),
            }
            self.assertEqual(verify_artifact(root, artifact)["status"], "verified")
            artifact["size"] += 1
            self.assertEqual(verify_artifact(root, artifact)["status"], "size-mismatch")
            artifact["local_path"] = "missing.bin"
            self.assertEqual(verify_artifact(root, artifact)["status"], "missing")


if __name__ == "__main__":
    unittest.main()
