from __future__ import annotations

import base64
import importlib.util
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock


SCRIPT = Path(__file__).parents[1] / "scripts" / "capture_x58_uart.py"
CAPTURES = Path(__file__).parents[1] / "research" / "msi" / "captures"
SPEC = importlib.util.spec_from_file_location("capture_x58_uart", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class CaptureX58UartTests(unittest.TestCase):
    def test_existing_cli_defaults_are_preserved(self) -> None:
        args = MODULE.parse_args(["--output", "boot.raw"])

        self.assertEqual(args.device, "/dev/ttyUSB1")
        self.assertEqual(args.baud, 115200)
        self.assertEqual(args.quiet, 0.5)
        self.assertEqual(args.timeout, 300.0)
        self.assertEqual(args.max_bytes, 16 * 1024 * 1024)
        self.assertEqual(args.rate_policy, "warn")
        self.assertEqual(args.replay_policy, "fail")
        self.assertEqual(
            args.replay_burst_allowance,
            MODULE.DEFAULT_REPLAY_BURST_ALLOWANCE,
        )
        self.assertEqual(
            args.replay_identical_lines,
            MODULE.DEFAULT_REPLAY_IDENTICAL_LINES,
        )
        self.assertIsNone(args.lock_file)
        self.assertFalse(args.collapse_identical_lines)
        self.assertIn("x58-usb> ", MODULE.DEFAULT_MARKERS)

    def test_replay_guard_accepts_real_b06wf_multireset_suffix(self) -> None:
        raw = CAPTURES / "2026-09-07-b06wf-repeat.raw"
        if not raw.is_file():
            self.skipTest("private historical replay fixture is not distributed")
        data = raw.read_bytes()
        self.assertEqual(
            MODULE.hashlib.sha256(data).hexdigest(),
            "fd4991e20532e65fc23b5072049c6348bc545b6d69161d48df7188fe7fa295be",
        )
        guard = MODULE.ReplayGuard(
            baud=115200,
            burst_allowance=MODULE.DEFAULT_REPLAY_BURST_ALLOWANCE,
            identical_line_limit=MODULE.DEFAULT_REPLAY_IDENTICAL_LINES,
            minimum_line_bytes=MODULE.DEFAULT_REPLAY_MIN_LINE_BYTES,
            started_monotonic_ns=0,
        )

        evidence = guard.observe(data, len(data), 0)

        self.assertIsNone(evidence)
        self.assertEqual(guard.max_identical_lines, 2)

    def test_replay_guard_rejects_observed_b06wf_replayed_suffix(self) -> None:
        raw = CAPTURES / "2026-09-07-b06wf-suffix.raw"
        if not raw.is_file():
            self.skipTest("private historical replay fixture is not distributed")
        data = raw.read_bytes()
        self.assertEqual(
            MODULE.hashlib.sha256(data).hexdigest(),
            "48cdaf6f58da097cc4ee68e135a6fcde5360f5efed4d0677c55656ea6c1a4a30",
        )
        guard = MODULE.ReplayGuard(
            baud=115200,
            burst_allowance=MODULE.DEFAULT_REPLAY_BURST_ALLOWANCE,
            identical_line_limit=MODULE.DEFAULT_REPLAY_IDENTICAL_LINES,
            minimum_line_bytes=MODULE.DEFAULT_REPLAY_MIN_LINE_BYTES,
            started_monotonic_ns=0,
        )

        evidence = guard.observe(data, len(data), 0)

        self.assertIsNotNone(evidence)
        assert evidence is not None
        self.assertEqual(evidence.kind, "identical-line-run")
        self.assertEqual(evidence.identical_lines, 64)
        self.assertEqual(evidence.identical_bytes, 64 * 17)
        self.assertEqual(
            base64.b64decode(evidence.line_preview_base64),
            b"ATUS=ok CODE=00\r\n",
        )
        self.assertEqual(
            evidence.line_sha256,
            "a69587f90b35d9dcb942c3053ecbfb12372794f6423ecebe09cf5a036a8fce79",
        )
        self.assertEqual(evidence.burst_observed_bytes, len(data))
        self.assertEqual(
            evidence.burst_allowed_bytes,
            MODULE.DEFAULT_REPLAY_BURST_ALLOWANCE,
        )

    def test_replay_guard_token_bucket_has_bounded_quiet_credit(self) -> None:
        guard = MODULE.ReplayGuard(
            baud=1000,
            burst_allowance=100,
            identical_line_limit=64,
            minimum_line_bytes=8,
            started_monotonic_ns=0,
        )
        self.assertIsNone(guard.observe(b"a" * 100, 100, 0))

        evidence = guard.observe(b"b" * 101, 201, 10_000_000_000)

        self.assertIsNotNone(evidence)
        assert evidence is not None
        self.assertEqual(evidence.kind, "burst-rate")
        self.assertEqual(evidence.burst_observed_bytes, 101)
        self.assertEqual(evidence.burst_allowed_bytes, 100)

    @unittest.skipUnless(os.name == "posix", "PTY test requires POSIX")
    def test_default_replay_policy_stops_capture_and_marks_metadata(self) -> None:
        import pty

        master, slave = pty.openpty()
        writer: threading.Thread | None = None
        try:
            with tempfile.TemporaryDirectory() as temporary:
                raw = Path(temporary) / "replay.raw"
                payload = b"ATUS=ok CODE=00\r\n" * 64

                def send_payload() -> None:
                    time.sleep(0.1)
                    os.write(master, payload)

                writer = threading.Thread(target=send_payload)
                writer.start()
                argv = [
                    str(SCRIPT),
                    "--device",
                    os.ttyname(slave),
                    "--output",
                    str(raw),
                    "--start-immediately",
                    "--timeout",
                    "2",
                    "--marker",
                    "NEVER",
                ]
                with mock.patch.object(sys, "argv", argv), mock.patch(
                    "sys.stdout", new_callable=io.StringIO
                ), mock.patch("sys.stderr", new_callable=io.StringIO):
                    return_code = MODULE.main()
                writer.join(timeout=1)
                metadata = json.loads(
                    Path(f"{raw}.metadata.json").read_text(encoding="utf-8")
                )
                captured = raw.read_bytes()

            self.assertEqual(return_code, 3)
            self.assertEqual(captured, payload)
            self.assertEqual(metadata["status"], "replay-failed")
            self.assertEqual(
                metadata["replay_guard"]["evidence"]["kind"],
                "identical-line-run",
            )
            self.assertEqual(
                metadata["replay_guard"]["evidence"]["identical_lines"],
                64,
            )
        finally:
            if writer is not None:
                writer.join(timeout=1)
            os.close(master)
            os.close(slave)

    @unittest.skipUnless(os.name == "posix", "PTY test requires POSIX")
    def test_quiet_gated_capture_flushes_input_and_output(self) -> None:
        import pty

        master, slave = pty.openpty()
        try:
            with tempfile.TemporaryDirectory() as temporary:
                raw = Path(temporary) / "quiet.raw"
                result = MODULE.CaptureResult(
                    total=0,
                    sha256=MODULE.hashlib.sha256(b"").hexdigest(),
                    matched=b"END",
                    started_monotonic_ns=1,
                    ended_monotonic_ns=2,
                    error=None,
                )
                argv = [
                    str(SCRIPT),
                    "--device",
                    os.ttyname(slave),
                    "--output",
                    str(raw),
                    "--quiet",
                    "0",
                    "--marker",
                    "END",
                ]
                with mock.patch.object(sys, "argv", argv), mock.patch.object(
                    MODULE.termios, "tcflush"
                ) as tcflush, mock.patch.object(
                    MODULE, "capture", return_value=result
                ), mock.patch("sys.stdout", new_callable=io.StringIO), mock.patch(
                    "sys.stderr", new_callable=io.StringIO
                ):
                    self.assertEqual(MODULE.main(), 0)

            self.assertEqual(
                tcflush.call_args_list,
                [
                    mock.call(mock.ANY, MODULE.termios.TCIOFLUSH),
                    mock.call(mock.ANY, MODULE.termios.TCIOFLUSH),
                ],
            )
        finally:
            os.close(master)
            os.close(slave)

    @unittest.skipUnless(os.name == "posix", "PTY test requires POSIX")
    def test_start_immediately_does_not_flush_either_queue(self) -> None:
        import pty

        master, slave = pty.openpty()
        try:
            with tempfile.TemporaryDirectory() as temporary:
                raw = Path(temporary) / "immediate.raw"
                result = MODULE.CaptureResult(
                    total=0,
                    sha256=MODULE.hashlib.sha256(b"").hexdigest(),
                    matched=b"END",
                    started_monotonic_ns=1,
                    ended_monotonic_ns=2,
                    error=None,
                )
                argv = [
                    str(SCRIPT),
                    "--device",
                    os.ttyname(slave),
                    "--output",
                    str(raw),
                    "--start-immediately",
                    "--marker",
                    "END",
                ]
                with mock.patch.object(sys, "argv", argv), mock.patch.object(
                    MODULE.termios, "tcflush"
                ) as tcflush, mock.patch.object(
                    MODULE, "capture", return_value=result
                ), mock.patch("sys.stdout", new_callable=io.StringIO), mock.patch(
                    "sys.stderr", new_callable=io.StringIO
                ):
                    self.assertEqual(MODULE.main(), 0)

            tcflush.assert_not_called()
        finally:
            os.close(master)
            os.close(slave)

    def test_tioc_excl_and_release_use_kernel_tty_exclusivity(self) -> None:
        with mock.patch.object(MODULE.fcntl, "ioctl") as ioctl:
            MODULE.claim_tty_exclusive(17)
            MODULE.release_tty_exclusive(17)

        self.assertEqual(
            ioctl.call_args_list,
            [
                mock.call(17, MODULE.termios.TIOCEXCL),
                mock.call(17, MODULE.termios.TIOCNXCL),
            ],
        )

    def test_project_lock_is_nonblocking_and_reusable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "capture.lock"
            first = MODULE.acquire_project_lock(path)
            try:
                with self.assertRaisesRegex(RuntimeError, "already held"):
                    MODULE.acquire_project_lock(path)
            finally:
                MODULE.release_project_lock(first)

            second = MODULE.acquire_project_lock(path)
            MODULE.release_project_lock(second)

    def test_rate_assessment_rejects_replay_speed(self) -> None:
        normal = MODULE.assess_rate(11520, 0, 1_000_000_000, 115200, 4096)
        replay = MODULE.assess_rate(
            4_194_298, 0, 37_448_000_000, 115200, 4096
        )

        self.assertTrue(normal.plausible)
        self.assertFalse(replay.plausible)
        self.assertGreater(replay.observed_bytes_per_second, 100_000)
        self.assertEqual(replay.nominal_max_bytes_per_second, 11_520)

    @unittest.skipUnless(os.name == "posix", "PTY test requires POSIX")
    def test_fail_policy_marks_implausible_capture(self) -> None:
        import pty

        master, slave = pty.openpty()
        try:
            result = MODULE.CaptureResult(
                total=4_194_298,
                sha256="0" * 64,
                matched=b"END",
                started_monotonic_ns=1_000_000_000,
                ended_monotonic_ns=38_448_000_000,
                error=None,
            )
            with tempfile.TemporaryDirectory() as temporary:
                raw = Path(temporary) / "replay.raw"
                argv = [
                    str(SCRIPT),
                    "--device",
                    os.ttyname(slave),
                    "--output",
                    str(raw),
                    "--start-immediately",
                    "--rate-policy",
                    "fail",
                    "--marker",
                    "END",
                ]
                with mock.patch.object(sys, "argv", argv), mock.patch.object(
                    MODULE, "capture", return_value=result
                ), mock.patch("sys.stdout", new_callable=io.StringIO), mock.patch(
                    "sys.stderr", new_callable=io.StringIO
                ) as stderr:
                    return_code = MODULE.main()
                metadata = json.loads(
                    Path(f"{raw}.metadata.json").read_text(encoding="utf-8")
                )

            self.assertEqual(return_code, 2)
            self.assertEqual(metadata["status"], "rate-failed")
            self.assertFalse(metadata["rate_check"]["plausible"])
            self.assertIn("physically implausible", stderr.getvalue())
        finally:
            os.close(master)
            os.close(slave)

    def test_rle_is_separate_reversible_derived_artifact(self) -> None:
        raw_data = b"same\r\nsame\r\nnext\nlast-without-newline"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            raw = root / "boot.raw"
            derived = root / "boot.raw.rle.jsonl"
            raw.write_bytes(raw_data)
            raw_sha256 = MODULE.hashlib.sha256(raw_data).hexdigest()

            info = MODULE.collapse_identical_lines(raw, derived, raw_sha256)
            records = [json.loads(line) for line in derived.read_text().splitlines()]
            raw_after = raw.read_bytes()

        self.assertEqual(raw_after, raw_data)
        self.assertEqual(records[0]["artifact"], "derived-not-raw-evidence")
        self.assertEqual(records[0]["raw_sha256"], raw_sha256)
        self.assertEqual([record["count"] for record in records[1:]], [2, 1, 1])
        decoded = [
            base64.b64decode(record["data_base64"]) for record in records[1:]
        ]
        self.assertEqual(decoded, [b"same\r\n", b"next\n", b"last-without-newline"])
        self.assertEqual(info["source_lines"], 4)
        self.assertEqual(info["runs"], 3)

    @unittest.skipUnless(os.name == "posix", "PTY test requires POSIX")
    def test_main_writes_raw_and_monotonic_metadata(self) -> None:
        import pty

        master, slave = pty.openpty()
        writer: threading.Thread | None = None
        try:
            device = os.ttyname(slave)
            with tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                raw = root / "boot.raw"
                lock = root / "capture.lock"
                payload = b"[BOOTBLOCK] test\r\nEND\r\n"

                def send_payload() -> None:
                    time.sleep(0.1)
                    os.write(master, payload)

                writer = threading.Thread(target=send_payload)
                writer.start()
                argv = [
                    str(SCRIPT),
                    "--device",
                    device,
                    "--output",
                    str(raw),
                    "--lock-file",
                    str(lock),
                    "--start-immediately",
                    "--timeout",
                    "2",
                    "--marker-linger",
                    "0",
                    "--marker",
                    "END",
                    "--collapse-identical-lines",
                ]
                with mock.patch.object(sys, "argv", argv), mock.patch(
                    "sys.stdout", new_callable=io.StringIO
                ), mock.patch("sys.stderr", new_callable=io.StringIO):
                    return_code = MODULE.main()
                writer.join(timeout=1)

                metadata = json.loads(
                    Path(f"{raw}.metadata.json").read_text(encoding="utf-8")
                )

                self.assertEqual(return_code, 0)
                self.assertEqual(raw.read_bytes(), payload)
                self.assertEqual(metadata["status"], "ok")
                self.assertEqual(
                    metadata["capture_policy"]["max_bytes"],
                    16 * 1024 * 1024,
                )
                self.assertEqual(metadata["capture_policy"]["markers"], ["END"])
                self.assertEqual(
                    metadata["capture_policy"]["marker_linger_seconds"], 0.0
                )
                self.assertTrue(metadata["exclusivity"]["tioc_excl"])
                self.assertTrue(metadata["exclusivity"]["lock_acquired"])
                self.assertLessEqual(
                    metadata["timing"]["capture_started_monotonic_ns"],
                    metadata["timing"]["capture_ended_monotonic_ns"],
                )
                self.assertTrue(metadata["rate_check"]["plausible"])
                self.assertEqual(
                    metadata["collapsed_identical_lines"]["artifact"],
                    "derived-not-raw-evidence",
                )
        finally:
            if writer is not None:
                writer.join(timeout=1)
            os.close(master)
            os.close(slave)


if __name__ == "__main__":
    unittest.main()
