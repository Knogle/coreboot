#!/usr/bin/env python3
"""Pure status-contract tests for run_x58_rommon_script.py."""

from __future__ import annotations

import unittest

import run_x58_rommon_script as executor


def status(*, txn: str = "00", trace: str = "00",
           txn_fnv: str = "00000000", mutations: str = "00",
           rollback: str = "00", attempted: str = "00",
           rolled_back: str = "00") -> bytes:
    return (
        b"[SCRIPT] FORMAT=01 OPS=20 PROGRAM_FNV=492f81ca SEALED=01 "
        + f"TXN_VALID={txn} TRACE={trace} TXN_FNV={txn_fnv} ".encode()
        + f"LAST=ok MUT={mutations} NONREV=00 ".encode()
        + f"RB_AVAILABLE={rollback} RB_RESULT=ok ".encode()
        + f"RB_ATTEMPTED={attempted} RB_TRIES=00000001 ".encode()
        + f"ROLLED_BACK={rolled_back}\r\n".encode()
    )


class StatusTests(unittest.TestCase):
    def test_loaded_clean(self) -> None:
        parsed = executor.parse_last_status(status())
        executor.require_loaded_clean(parsed, 0x20, 0x492F81CA)

    def test_success_with_reversible_mutations(self) -> None:
        parsed = executor.parse_last_status(status(
            txn="01", trace="20", txn_fnv="fee81911",
            mutations="03", rollback="01"))
        executor.require_success(parsed, 0x20, 0x492F81CA, 3)

    def test_read_only_success(self) -> None:
        parsed = executor.parse_last_status(status(
            txn="01", trace="20", txn_fnv="616450c9"))
        executor.require_success(parsed, 0x20, 0x492F81CA, 0)

    def test_missing_rollback_is_rejected(self) -> None:
        parsed = executor.parse_last_status(status(
            txn="01", trace="20", txn_fnv="fee81911",
            mutations="03"))
        with self.assertRaisesRegex(executor.ExecutorError, "rollback data"):
            executor.require_success(parsed, 0x20, 0x492F81CA, 3)

    def test_rolled_back(self) -> None:
        parsed = executor.parse_last_status(status(
            txn="01", trace="20", txn_fnv="9b9cf8ad",
            mutations="03", attempted="01", rolled_back="01"))
        executor.require_rolled_back(parsed, 0x20, 0x492F81CA, 3)

    def test_discarded(self) -> None:
        parsed = executor.parse_last_status(status())
        executor.require_discarded(parsed, 0x20, 0x492F81CA)

    def test_incomplete_status_is_rejected(self) -> None:
        with self.assertRaisesRegex(executor.ExecutorError, "complete"):
            executor.parse_last_status(b"[SCRIPT] FORMAT=01 OPS=20")


if __name__ == "__main__":
    unittest.main()
