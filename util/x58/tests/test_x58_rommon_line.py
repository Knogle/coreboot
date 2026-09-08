#!/usr/bin/env python3

import importlib.util
import io
from pathlib import Path
import sys
import unittest
from unittest import mock


SCRIPT = Path(__file__).parents[1] / "scripts" / "x58_rommon_line.py"
SPEC = importlib.util.spec_from_file_location("x58_rommon_line", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class Transcript(io.BytesIO):
    def close(self):
        self.was_closed = True

    def fileno(self):
        return 99


class X58RommonLineTest(unittest.TestCase):
    def test_b06wf_usb_prompt_is_recognized(self):
        self.assertIsNotNone(MODULE.PROMPT.search(b"usb status\r\nx58-usb> "))
        self.assertIsNone(MODULE.PROMPT_MODES["usb"].search(b"rommon> "))
        self.assertIsNotNone(MODULE.PROMPT_MODES["usb"].search(b"x58-usb> "))

    def test_command_channel_discards_both_queues_by_default(self):
        with mock.patch.object(MODULE.termios, "tcflush") as tcflush:
            MODULE.prepare_command_channel(17)

        tcflush.assert_called_once_with(17, MODULE.termios.TCIOFLUSH)

    def test_command_channel_preserves_input_but_discards_output(self):
        with mock.patch.object(MODULE.termios, "tcflush") as tcflush:
            MODULE.prepare_command_channel(17, preserve_input=True)

        tcflush.assert_called_once_with(17, MODULE.termios.TCOFLUSH)

    def test_exclusive_log_failure_prevents_serial_open(self):
        argv = ["x58_rommon_line.py", "--log", "/tmp/existing", "id"]
        with mock.patch.object(sys, "argv", argv), \
             mock.patch.object(Path, "open", side_effect=FileExistsError()), \
             mock.patch.object(MODULE.os, "open") as serial_open:
            self.assertEqual(MODULE.main(), 1)
            serial_open.assert_not_called()

    def test_serial_open_failure_closes_transcript(self):
        argv = ["x58_rommon_line.py", "--log", "/tmp/new", "id"]
        transcript = mock.MagicMock()
        with mock.patch.object(sys, "argv", argv), \
             mock.patch.object(Path, "open", return_value=transcript), \
             mock.patch.object(MODULE.os, "open", side_effect=OSError("no uart")):
            self.assertEqual(MODULE.main(), 1)
        transcript.close.assert_called_once_with()

    def test_queue_flush_failure_sends_no_command(self):
        argv = ["x58_rommon_line.py", "--log", "/tmp/new", "id"]
        transcript = mock.MagicMock()
        with mock.patch.object(sys, "argv", argv), \
             mock.patch.object(Path, "open", return_value=transcript), \
             mock.patch.object(MODULE.os, "open", return_value=7), \
             mock.patch.object(MODULE.os, "close"), \
             mock.patch.object(MODULE, "configure"), \
             mock.patch.object(
                 MODULE,
                 "prepare_command_channel",
                 side_effect=OSError("flush failed"),
             ), \
             mock.patch.object(MODULE, "send_command") as send_command:
            self.assertEqual(MODULE.main(), 1)

        send_command.assert_not_called()

    def test_complete_responses_are_flushed_and_synced(self):
        argv = [
            "x58_rommon_line.py", "--keep-input", "--log", "/tmp/new",
            "id", "script status",
        ]
        transcript = Transcript()
        responses = [b"id\r\nrommon> ", b"status\r\nrommon> "]
        with mock.patch.object(sys, "argv", argv), \
             mock.patch.object(Path, "open", return_value=transcript), \
             mock.patch.object(MODULE.os, "open", return_value=7), \
             mock.patch.object(MODULE.os, "close") as serial_close, \
             mock.patch.object(MODULE, "configure"), \
             mock.patch.object(MODULE, "prepare_command_channel") as prepare, \
             mock.patch.object(MODULE, "send_command", side_effect=responses), \
             mock.patch.object(MODULE.os, "fsync") as fsync, \
             mock.patch.object(sys, "stdout") as stdout:
            stdout.buffer = mock.MagicMock()
            self.assertEqual(MODULE.main(), 0)
        self.assertEqual(transcript.getvalue(), b"".join(responses))
        self.assertEqual(fsync.call_count, 2)
        prepare.assert_called_once_with(7, preserve_input=True)
        serial_close.assert_called_once_with(7)


if __name__ == "__main__":
    unittest.main()
