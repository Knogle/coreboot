#!/usr/bin/env python3
"""Pure parser/status tests for load_x58_rommon_script.py."""

from __future__ import annotations

import unittest

import load_x58_rommon_script as loader


class StatusTests(unittest.TestCase):
    def test_clean_status(self) -> None:
        loader.require_clean_transaction(
            b"[SCRIPT] FORMAT=01 OPS=00 PROGRAM_FNV=00000000 "
            b"SEALED=00 TXN_VALID=00 TRACE=00\r\n"
        )

    def test_retained_transaction_rejected(self) -> None:
        with self.assertRaisesRegex(loader.LoaderError, "retained transaction"):
            loader.require_clean_transaction(
                b"[SCRIPT] FORMAT=01 OPS=20 PROGRAM_FNV=12345678 "
                b"SEALED=01 TXN_VALID=01 TRACE=20\r\n"
            )

    def test_loaded_status(self) -> None:
        loader.require_loaded_status(
            b"[SCRIPT] FORMAT=01 OPS=20 PROGRAM_FNV=9867da61 "
            b"SEALED=01 TXN_VALID=00 TRACE=00\r\n",
            0x20,
            0x9867DA61,
        )

    def test_wrong_digest_rejected(self) -> None:
        with self.assertRaisesRegex(loader.LoaderError, "digest differs"):
            loader.require_loaded_status(
                b"[SCRIPT] FORMAT=01 OPS=20 PROGRAM_FNV=9867da60 "
                b"SEALED=01 TXN_VALID=00 TRACE=00\r\n",
                0x20,
                0x9867DA61,
            )


if __name__ == "__main__":
    unittest.main()
