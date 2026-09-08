import importlib.util
import io
import json
from pathlib import Path
import re
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "capture_x58_intenso_linux_diag", ROOT / "scripts/capture_x58_intenso_linux_diag.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)
PREFIX = (b"X58PROE-B06WK-ACPI-REPAIR-20260908\n", b"Press ESC for boot menu.\n",
          b"Select boot device:\n2. USB Intenso Ultra Line\n",
          b"ISOLINUX 6.03\nDefault settings (Runs from RAM)\nPress <TAB> to edit options")
PROMPT = (b"\x1b[18;01H\x1b[0;36;1m> \x1b[0m.linux /pmagic/bzImage "
          b"initrd=/pmagic/initrd.img\x1b[18;03H")


class GuardTests(unittest.TestCase):
    def setUp(self):
        self.now = 0
        self.guard = MODULE.EditorGuard(clock=lambda: self.now)

    def enter_editor(self):
        actions = [self.guard.feed(chunk) for chunk in PREFIX]
        self.assertEqual([action[0] for action in actions if action], [b"\x1b", b"2", b"\t"])
        self.assertEqual(self.guard.selector.stage, "editor_pending")
        self.guard.feed(PROMPT)
        self.assertEqual(self.guard.selector.stage, "editor")

    def command(self, wire, proof=True):
        command = {"hex": wire.hex(), "reason": "host test"}
        if proof:
            command["readback"] = self.guard.checkpoint()
        return command

    def redraw(self):
        self.assertEqual(self.guard.validate(self.command(b"\x01", False)), b"\x01")
        self.guard.transmitted(b"\x01")
        self.guard.feed(PROMPT)

    def test_tab_is_not_acknowledgement(self):
        for chunk in PREFIX:
            self.guard.feed(chunk)
        with self.assertRaisesRegex(ValueError, "acknowledged"):
            self.guard.validate(self.command(b"\x01", False))

    def test_initial_mutation_requires_requested_redraw(self):
        self.enter_editor()
        with self.assertRaisesRegex(ValueError, "redraw"):
            self.guard.validate(self.command(b"\x15"))
        self.redraw()
        self.assertEqual(self.guard.validate(self.command(b"\x15")), b"\x15")

    def test_missing_or_wrong_checkpoint_rejected(self):
        self.enter_editor()
        self.redraw()
        for command in (self.command(b"text", False),
                        dict(self.command(b"text"), readback={"raw_offset": 1, "sha256": "0" * 64})):
            with self.assertRaisesRegex(ValueError, "offset/SHA256"):
                self.guard.validate(command)

    def test_new_rx_invalidates_queued_checkpoint(self):
        self.enter_editor()
        self.redraw()
        queued = self.command(b"text")
        self.guard.feed(b"echo")
        with self.assertRaisesRegex(ValueError, "offset/SHA256"):
            self.guard.validate(queued)

    def test_mutation_consumes_checkpoint(self):
        self.enter_editor()
        self.redraw()
        self.guard.transmitted(self.guard.validate(self.command(b"\x15")))
        with self.assertRaisesRegex(ValueError, "consumed"):
            self.guard.validate(self.command(b"text"))
        self.guard.feed(b"\r> ")
        self.assertEqual(self.guard.validate(self.command(b"text")), b"text")

    def test_enter_requires_new_redraw_and_is_terminal(self):
        self.enter_editor()
        self.redraw()
        self.guard.transmitted(self.guard.validate(self.command(b"text")))
        self.guard.feed(b"text")
        with self.assertRaisesRegex(ValueError, "redraw"):
            self.guard.validate(self.command(b"\r"))
        self.redraw()
        self.guard.transmitted(self.guard.validate(self.command(b"\r")))
        self.assertEqual(self.guard.selector.stage, "booting")
        with self.assertRaisesRegex(ValueError, "stage=booting"):
            self.guard.validate(self.command(b"\x01", False))

    def test_freshness_applies_even_to_readback_keys(self):
        self.enter_editor()
        self.now = MODULE.FRESH_SECONDS + .1
        with self.assertRaisesRegex(ValueError, "stale"):
            self.guard.validate(self.command(b"\x01", False))

    def test_disallow_embedded_enter_and_large_fragments(self):
        self.enter_editor()
        self.redraw()
        for wire in (b"text\r", b"\n", b"\x1b", b"\t", b"x" * 17, b""):
            with self.assertRaises(ValueError):
                self.guard.validate(self.command(wire))

    def test_each_terminal_marker_across_every_split(self):
        for marker in (b"Loading /pmagic/bzImage...", b"Loading /pmagic/initrd.img...",
                       b"Loading initrd...", b"Probing EDD (edd=off to disable)...",
                       b"Linux version 6.8.0-31-generic", b"Booting the kernel.", b"\x1bc"):
            for split in range(len(marker) + 1):
                with self.subTest(marker=marker, split=split):
                    guard = MODULE.EditorGuard(clock=lambda: 0)
                    for chunk in PREFIX:
                        guard.feed(chunk)
                    guard.feed(PROMPT)
                    guard.feed(marker[:split])
                    guard.feed(marker[split:])
                    self.assertEqual(guard.selector.stage, "booting")
                    with self.assertRaises(ValueError):
                        guard.validate({"hex": "01", "reason": "stale redraw"})

    def test_terminal_wins_when_prompt_and_loader_share_chunk(self):
        for chunk in PREFIX:
            self.guard.feed(chunk)
        self.guard.feed(PROMPT + b"Loading /pmagic/bzImage...")
        self.assertEqual(self.guard.selector.stage, "booting")

    def test_latch_survives_new_firmware_or_editor_markers(self):
        self.enter_editor()
        self.guard.feed(b"Loading /pmagic/bzImage...")
        for chunk in PREFIX + (PROMPT,):
            self.assertIsNone(self.guard.feed(chunk))
        self.assertEqual(self.guard.selector.stage, "booting")

    def test_ambiguous_intenso_does_not_select(self):
        for chunk in PREFIX[:2]:
            self.guard.feed(chunk)
        self.assertIsNone(self.guard.feed(b"Select boot device:\n2. Intenso A\n3. Intenso B\n"))
        self.assertEqual(self.guard.selector.stage, "disks")

    def test_buffered_loader_preempts_first_tx_byte(self):
        self.enter_editor()
        sent = []
        def drain():
            self.guard.feed(b"Loading /pmagic/bzImage...")
            return True
        self.assertEqual(MODULE.guarded_send(b"text", "editor", self.guard,
                                            drain, sent.append, lambda _: None), b"")
        self.assertFalse(sent)

    def test_loader_between_bytes_cancels_remaining_fragment(self):
        self.enter_editor()
        sent = []
        def drain():
            if sent:
                self.guard.feed(b"Loading /pmagic/bzImage...")
            return True
        actual = MODULE.guarded_send(b"text", "editor", self.guard,
                                     drain, sent.append, lambda _: None)
        self.assertEqual(actual, b"t")
        self.assertEqual(sent, [b"t"])

    def test_undrained_rx_prevents_tx(self):
        self.enter_editor()
        self.assertEqual(MODULE.guarded_send(b"text", "editor", self.guard,
                                            lambda: False, self.fail, lambda _: None), b"")

    def test_hw06_replay_denies_all_32_late_transmissions(self):
        capture = ROOT / "research/msi/captures/b06wk-intenso-linux-diag-20260908-06.raw"
        if not capture.exists():
            self.skipTest("private/local HW06 capture unavailable")
        data = capture.read_bytes()
        metadata = json.loads(capture.with_suffix(".raw.json").read_text())
        late = [event for event in metadata["events"]
                if event["raw_offset"] == 109283 and event["message"].startswith("TX hex=")]
        self.assertEqual(len(late), 32)
        for size in (17, 64, 101, 512, 8192, len(data)):
            with self.subTest(chunk=size):
                guard = MODULE.EditorGuard(clock=lambda: 0)
                for offset in range(0, len(data), size):
                    guard.feed(data[offset:offset + size])
                self.assertEqual(guard.selector.stage, "booting")
                for event in late:
                    wire = re.search(r"TX hex=([0-9a-f]+)", event["message"]).group(1)
                    with self.assertRaisesRegex(ValueError, "stage=booting"):
                        guard.validate({"hex": wire, "reason": "historical late control"})

    def test_hw06_actual_editor_acknowledgement_then_loader(self):
        capture = ROOT / "research/msi/captures/b06wk-intenso-linux-diag-20260908-06.raw"
        if not capture.exists():
            self.skipTest("private/local HW06 capture unavailable")
        data = capture.read_bytes()
        cursor = 0
        for end, stage in ((98186, "disks"), (98314, "isolinux"),
                           (102353, "editor_pending"), (102941, "editor"),
                           (103074, "editor"), (103180, "booting")):
            while cursor < end:
                following = min(end, cursor + 64)
                self.guard.feed(data[cursor:following])
                cursor = following
            self.assertEqual(self.guard.selector.stage, stage, end)

    def test_menu_only_sends_sole_escape_then_passive(self):
        guard = MODULE.EditorGuard(stop_at_seabios_menu=True)
        actions = [guard.feed(chunk) for chunk in PREFIX + (PROMPT,)]
        self.assertEqual([action[0] for action in actions if action], [b"\x1b"])
        self.assertEqual(guard.selector.stage, "passive")
        for chunk in PREFIX + (PROMPT,):
            self.assertIsNone(guard.feed(chunk))

    def test_menu_only_refuses_all_fifo_hex_even_with_editor_proof(self):
        guard = MODULE.EditorGuard(stop_at_seabios_menu=True)
        for chunk in PREFIX + (PROMPT, b"Loading /pmagic/bzImage..."):
            guard.feed(chunk)
            for wire in ("1b", "32", "09", "01", "15", "0d", "74657874"):
                with self.assertRaisesRegex(ValueError, "menu-only"):
                    guard.validate({"hex": wire, "reason": "forbidden",
                                    "readback": guard.checkpoint()})

    def test_menu_only_hw06_entire_replay_no_digit_or_tab(self):
        capture = ROOT / "research/msi/captures/b06wk-intenso-linux-diag-20260908-06.raw"
        if not capture.exists():
            self.skipTest("private/local HW06 capture unavailable")
        data = capture.read_bytes()
        for size in (17, 64, 101, 512, 8192):
            with self.subTest(chunk=size):
                guard = MODULE.EditorGuard(stop_at_seabios_menu=True)
                actions = []
                for offset in range(0, len(data), size):
                    action = guard.feed(data[offset:offset + size])
                    if action:
                        actions.append(action[0])
                self.assertEqual(actions, [b"\x1b"])
                self.assertEqual(guard.selector.stage, "booting")

    def test_intenso_only_sends_escape_and_named_digit_then_passive(self):
        guard = MODULE.EditorGuard(stop_after_intenso=True)
        actions = [guard.feed(chunk) for chunk in PREFIX + (PROMPT,)]
        self.assertEqual([action[0] for action in actions if action], [b"\x1b", b"2"])
        self.assertEqual(guard.selector.stage, "passive")
        for chunk in PREFIX + (PROMPT,):
            self.assertIsNone(guard.feed(chunk))

    def test_intenso_only_refuses_all_fifo_hex(self):
        guard = MODULE.EditorGuard(stop_after_intenso=True)
        for chunk in PREFIX + (PROMPT, b"Loading /pmagic/bzImage..."):
            guard.feed(chunk)
            for wire in ("1b", "32", "09", "01", "15", "0d", "74657874"):
                with self.assertRaisesRegex(ValueError, "Intenso-only"):
                    guard.validate({"hex": wire, "reason": "forbidden",
                                    "readback": guard.checkpoint()})

    def test_intenso_only_hw06_entire_replay_no_tab(self):
        capture = ROOT / "research/msi/captures/b06wk-intenso-linux-diag-20260908-06.raw"
        if not capture.exists():
            self.skipTest("private/local HW06 capture unavailable")
        data = capture.read_bytes()
        for size in (17, 64, 101, 512, 8192):
            with self.subTest(chunk=size):
                guard = MODULE.EditorGuard(stop_after_intenso=True)
                actions = []
                for offset in range(0, len(data), size):
                    action = guard.feed(data[offset:offset + size])
                    if action:
                        actions.append(action[0])
                # An 8192-byte batch already contains loader evidence alongside
                # the disk menu: fail closed instead of sending a stale digit.
                expected = [b"\x1b"] if size == 8192 else [b"\x1b", b"2"]
                self.assertEqual(actions, expected)
                self.assertEqual(guard.selector.stage, "booting")

    def test_previous_os_output_is_ignored_until_fresh_wk_identity(self):
        for options in ({}, {"stop_at_seabios_menu": True}, {"stop_after_intenso": True}):
            with self.subTest(options=options):
                guard = MODULE.EditorGuard(**options)
                self.assertIsNone(guard.feed(b"Linux version 6.8.0 old boot\nProbing EDD\x1bc"))
                self.assertEqual(guard.selector.stage, "firmware")
                self.assertIsNone(guard.feed(PREFIX[0]))
                action = guard.feed(PREFIX[1])
                self.assertEqual(action[0], b"\x1b")

    def test_handoff_modes_are_mutually_exclusive(self):
        with self.assertRaisesRegex(ValueError, "mutually exclusive"):
            MODULE.EditorGuard(stop_at_seabios_menu=True, stop_after_intenso=True)
        argv = ["capture", "--output", "unused.raw", "--control", "unused.fifo",
                "--stop-at-seabios-menu", "--stop-after-intenso"]
        with patch("sys.argv", argv), patch("sys.stderr", io.StringIO()):
            with self.assertRaises(SystemExit) as caught:
                MODULE.main()
        self.assertEqual(caught.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
