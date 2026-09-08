#!/usr/bin/env python3
"""Capture B06WK -> SeaBIOS -> Intenso, with fail-closed editor controls.

No reset, flash, register or disk writes. Only the owned UART is transmitted to.
TAB means editor_pending, not editor acknowledgement. Loading/kernel evidence
latches booting permanently, even across a subsequent target reset.

With --stop-at-seabios-menu, send only the invited SeaBIOS ESC, then remain
passive while capturing. The operator owns device selection and all subsequent
keyboard input. Every FIFO hex command is refused in this mode.
With --stop-after-intenso, send ESC and the unique Intenso menu digit only,
then capture passively without TAB, editor input or other FIFO hex commands.
These two options are mutually exclusive; neither changes the boot medium.

FIFO JSON: {"hex":"01","reason":"request command redraw"}; review the raw
capture, then send at most 16 printable bytes or one editing key, accompanied
by "readback":{"raw_offset":N,"sha256":"SHA256 of that entire raw prefix"}.
Every mutation consumes its checkpoint; wait for echo and review again. The
checkpoint must be current and <=60 seconds old. Before initial mutation and
before a separate Enter, request Ctrl-A (01) or Ctrl-L (0c) and review its
response. Ctrl-U (15), Ctrl-K (0b), Ctrl-E (05) and standalone Enter (0d) are
supported. Enter latches booting immediately; no later hex control is accepted.
{"stop":true} closes only this capture. No automatic command rewrite exists.

The checkpoint is an operator assertion, not a command-line parser. It cannot
prove target state if a physical key or silent target transition races TX.
Already-buffered RX is always drained first, including between transmitted
bytes, and terminal evidence cancels the remainder of an in-flight fragment.
Historical capture helpers and logs remain unchanged. Host tests are not a
hardware validation of this replacement.
"""
import argparse
import fcntl
import hashlib
import json
import os
import re
import select
import termios
import time
from datetime import datetime, timezone


ANSI_CSI = re.compile(rb"\x1b\[[0-?]*[ -/]*[@-~]")
EDITOR_PROMPT = re.compile(rb"> (?:\.linux )?/pmagic/bzImage(?:\s|$)")
LOADER = re.compile(rb"Loading\s+(?:/pmagic/(?:bzImage|initrd)|initrd)|"
                    rb"Probing EDD|Booting the kernel|Linux version \d")
FRESH_SECONDS = 60


class BootSelector:
    def __init__(self, stop_at_seabios_menu=False, stop_after_intenso=False):
        if stop_at_seabios_menu and stop_after_intenso:
            raise ValueError("capture handoff modes are mutually exclusive")
        self.stage = "firmware"
        self.window = bytearray()
        self.stop_at_seabios_menu = stop_at_seabios_menu
        self.stop_after_intenso = stop_after_intenso

    def feed(self, chunk):
        self.window.extend(chunk)
        raw = bytes(self.window)
        text = ANSI_CSI.sub(b"", raw)
        del self.window[:-65536]
        if self.stage == "booting":
            return None
        if self.stage == "firmware":
            marker = text.find(b"X58PROE-B06WK-ACPI-REPAIR-20260908")
            if marker < 0:
                return None  # Previous OS output must not poison a pre-reset capture.
            self.stage = "seabios"
            text = text[marker:]
            self.window[:] = text
            del self.window[:-65536]
            raw = text
        if LOADER.search(text) or (self.stage in ("editor_pending", "editor")
                                   and b"\x1bc" in raw):
            self.stage = "booting"
            return None
        if self.stage == "passive":
            return None
        if self.stop_at_seabios_menu and self.stage == "menu_wait":
            if b"Select boot device:" in text:
                self.stage = "passive"
            return None
        action = None
        if self.stage == "seabios" and b"Press ESC for boot menu." in text:
            self.stage = "menu_wait" if self.stop_at_seabios_menu else "disks"
            action = (b"\x1b", "observed SeaBIOS ESC invitation", self.stage)
            self.window[:] = text.split(b"Press ESC for boot menu.", 1)[1]
        elif self.stage == "disks" and b"Select boot device:" in text:
            matches = list(re.finditer(
                rb"(?:^|[\r\n])([1-9])\. [^\r\n]*Intenso[^\r\n]*(?=[\r\n])", text, re.I))
            if len({m.group(1) for m in matches}) == 1:
                self.stage = "passive" if self.stop_after_intenso else "isolinux"
                action = (matches[0].group(1), "Intenso menu digit matched by name", self.stage)
                self.window[:] = text[matches[0].end():]
        elif (self.stage == "isolinux" and b"ISOLINUX 6.03" in text
              and b"Default settings (Runs from RAM)" in text
              and b"Press <TAB> to edit options" in text):
            self.stage = "editor_pending"
            action = (b"\t", "TAB invitation observed; editor acknowledgement pending", self.stage)
            self.window.clear()
        elif self.stage == "editor_pending" and EDITOR_PROMPT.search(text):
            self.stage = "editor"
        return action


class EditorGuard:
    """Pure RX/TX policy, replayable without opening any hardware."""
    def __init__(self, clock=time.monotonic, stop_at_seabios_menu=False, stop_after_intenso=False):
        self.selector = BootSelector(stop_at_seabios_menu=stop_at_seabios_menu,
                                     stop_after_intenso=stop_after_intenso)
        self.clock = clock
        self.offset = 0
        self.digest = hashlib.sha256()
        self.last_rx = float("-inf")
        self.consumed_offset = -1
        self.redraw = None
        self.redraw_ack = False
        self.edit_started = False

    def feed(self, chunk):
        if not chunk:
            return None
        self.offset += len(chunk)
        self.digest.update(chunk)
        self.last_rx = self.clock()
        action = self.selector.feed(chunk)
        if self.redraw is not None:
            self.redraw.extend(chunk)
            del self.redraw[:-65536]
            self.redraw_ack = bool(EDITOR_PROMPT.search(ANSI_CSI.sub(b"", bytes(self.redraw))))
        return action

    def checkpoint(self):
        return {"raw_offset": self.offset, "sha256": self.digest.hexdigest()}

    def validate(self, command):
        if self.selector.stop_at_seabios_menu:
            raise ValueError("menu-only capture forbids all FIFO hex TX; operator owns keyboard")
        if self.selector.stop_after_intenso:
            raise ValueError("Intenso-only handoff forbids all FIFO hex TX")
        if self.selector.stage != "editor":
            raise ValueError("hex TX requires acknowledged editor; stage=" + self.selector.stage)
        if self.clock() - self.last_rx > FRESH_SECONDS:
            raise ValueError("editor RX is stale")
        wire = bytes.fromhex(command["hex"])
        if not wire or len(wire) > 16:
            raise ValueError("TX must be 1..16 bytes")
        if not isinstance(command.get("reason"), str) or not command["reason"]:
            raise ValueError("explicit reason required")
        if wire in (b"\x01", b"\x0c"):
            return wire
        if not (all(0x20 <= byte <= 0x7e for byte in wire)
                or wire in (b"\x05", b"\x0b", b"\x15", b"\r")):
            raise ValueError("only printable fragments or standalone supported editor keys")
        if command.get("readback") != self.checkpoint():
            raise ValueError("missing or stale reviewed raw offset/SHA256")
        if self.offset <= self.consumed_offset:
            raise ValueError("checkpoint consumed; wait for and review new RX")
        if (wire == b"\r" or not self.edit_started) and not self.redraw_ack:
            raise ValueError("fresh requested Ctrl-A/Ctrl-L redraw must be reviewed first")
        return wire

    def transmitted(self, wire):
        self.consumed_offset = self.offset
        if wire in (b"\x01", b"\x0c"):
            self.redraw = bytearray()
            self.redraw_ack = False
        elif wire == b"\r":
            self.selector.stage = "booting"
        else:
            self.edit_started = True
            self.redraw = None
            self.redraw_ack = False


def guarded_send(wire, expected_stage, guard, drain_rx, write_byte, pause=time.sleep):
    """Drain RX before *each* byte. Return actual bytes, never the planned TX."""
    sent = bytearray()
    for byte in wire:
        if not drain_rx() or guard.selector.stage != expected_stage:
            break
        write_byte(bytes((byte,)))
        sent.append(byte)
        pause(.003)
    return bytes(sent)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True)
    parser.add_argument("--control", required=True)
    parser.add_argument("--seconds", type=int, default=1200)
    handoff = parser.add_mutually_exclusive_group()
    handoff.add_argument("--stop-at-seabios-menu", action="store_true",
                         help="send only the SeaBIOS ESC, then capture passively; refuse all FIFO hex TX")
    handoff.add_argument("--stop-after-intenso", action="store_true",
                         help="send ESC and the unique Intenso digit, then capture passively; no TAB or FIFO hex TX")
    args = parser.parse_args()
    if not 1 <= args.seconds <= 3600:
        parser.error("capture deadline must be 1..3600 seconds")
    if os.path.exists(args.output + ".json"):
        parser.error("metadata already exists")
    started = time.monotonic()
    started_utc = datetime.now(timezone.utc).isoformat()
    guard = EditorGuard(stop_at_seabios_menu=args.stop_at_seabios_menu,
                        stop_after_intenso=args.stop_after_intenso)
    events, automatic = [], []
    pending, controls = bytearray(), bytearray()
    last_report = started
    checkpoint_reported = -1
    fd = fifo = None
    created_fifo = False
    error = None
    stop = False

    def event(message):
        item = {"utc": datetime.now(timezone.utc).isoformat(),
                "seconds": round(time.monotonic() - started, 3),
                "raw_offset": guard.offset, "stage": guard.selector.stage, "message": message}
        events.append(item)
        print("[CAPTURE] " + json.dumps(item), flush=True)

    with open("/tmp/x58-usb-live.lock", "a") as lock, open(args.output, "xb", buffering=0) as raw:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)

        def drain_rx():
            # Bounded drain: continuous output suppresses TX, never starves capture.
            for _ in range(32):
                try:
                    chunk = os.read(fd, 8192)
                except BlockingIOError:
                    return True
                if not chunk:
                    return True
                raw.write(chunk)
                pending.extend(chunk)
                before = guard.selector.stage
                action = guard.feed(chunk)
                if before != guard.selector.stage:
                    event("RX state transition " + before + " -> " + guard.selector.stage)
                if action:
                    automatic.append(action)
                while b"\n" in pending:
                    line, _, rest = pending.partition(b"\n")
                    pending[:] = rest
                    if line and len(line) < 4096:
                        print(bytes(line).decode("ascii", errors="backslashreplace"), flush=True)
                if len(pending) > 32768:
                    del pending[:-8192]
            return not select.select([fd], [], [], 0)[0]

        def transmit(wire, reason, expected_stage):
            offset = guard.offset
            sent = guarded_send(wire, expected_stage, guard, drain_rx,
                                lambda byte: os.write(fd, byte))
            if sent:
                event("TX hex=" + sent.hex() + ": " + reason + "; start_raw_offset=" + str(offset))
            if sent != wire:
                event("TX remainder cancelled after RX/state change; planned_hex=" + wire.hex())
            return sent

        try:
            os.mkfifo(args.control, 0o600)
            created_fifo = True
            fifo = os.open(args.control, os.O_RDWR | os.O_NONBLOCK)
            fd = os.open("/dev/ttyUSB1", os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            fcntl.ioctl(fd, termios.TIOCEXCL)
            attrs = termios.tcgetattr(fd)
            attrs[:6] = [0, 0, termios.CLOCAL | termios.CREAD | termios.CS8, 0,
                         termios.B115200, termios.B115200]
            attrs[6][termios.VMIN] = 0
            attrs[6][termios.VTIME] = 0
            termios.tcsetattr(fd, termios.TCSANOW, attrs)
            termios.tcflush(fd, termios.TCOFLUSH)
            if args.stop_at_seabios_menu:
                event("ARMED: WK -> SeaBIOS ESC only -> passive capture; operator owns keyboard")
            elif args.stop_after_intenso:
                event("ARMED: WK -> ESC -> Intenso by name -> passive capture; no TAB or further keys")
            else:
                event("ARMED: WK -> ESC -> Intenso -> TAB acknowledgement; terminal booting latch")
            while not stop and time.monotonic() - started < args.seconds:
                ready, _, _ = select.select([fd, fifo], [], [], .1)
                drained = drain_rx()  # RX priority even when select initially saw only FIFO.
                if guard.offset >= 32 * 1024 * 1024:
                    event("32 MiB raw capture limit reached")
                    break
                while automatic:
                    wire, reason, stage = automatic.pop(0)
                    if drained and guard.selector.stage == stage:
                        transmit(wire, reason, stage)
                    else:
                        event("stale automatic action cancelled: " + reason)
                if fifo in ready:
                    controls.extend(os.read(fifo, 4096))
                if len(controls) > 16384:
                    raise ValueError("control message exceeded limit")
                while b"\n" in controls and not stop:
                    line, _, rest = controls.partition(b"\n")
                    controls[:] = rest
                    try:
                        command = json.loads(line)
                        if not isinstance(command, dict):
                            raise ValueError("control must be a JSON object")
                        if command.get("stop") is True:
                            event("operator capture stop; target unchanged")
                            stop = True
                            break
                        if not drain_rx():
                            raise ValueError("UART RX not drained; defer control until quiet")
                        wire = guard.validate(command)
                        # A second drain during send rechecks terminal state. A checkpoint
                        # change before byte zero also invalidates an explicitly reviewed TX.
                        def checked_drain():
                            if not drain_rx():
                                return False
                            if command.get("readback") is not None and not sent_bytes:
                                return command["readback"] == guard.checkpoint()
                            return True
                        sent_bytes = bytearray()
                        def write_byte(byte):
                            os.write(fd, byte)
                            sent_bytes.extend(byte)
                            # Commit immediately, so Enter forbids any subsequent FIFO TX.
                            if len(wire) == 1:
                                guard.transmitted(byte)
                        sent = guarded_send(wire, "editor", guard, checked_drain, write_byte)
                        if sent:
                            if len(wire) > 1:
                                guard.transmitted(sent)
                            event("TX hex=" + sent.hex() + ": " + command["reason"])
                        if sent != wire:
                            event("TX remainder cancelled; planned_hex=" + wire.hex())
                    except (ValueError, KeyError, TypeError) as exc:
                        event("REJECTED control: " + str(exc))
                if (guard.selector.stage == "editor" and guard.offset != checkpoint_reported
                        and time.monotonic() - guard.last_rx > .2):
                    event("EDITOR_READBACK_CANDIDATE " + json.dumps(guard.checkpoint()))
                    if pending:
                        print("[RX_FRAGMENT] " + repr(bytes(pending[-8192:])), flush=True)
                    checkpoint_reported = guard.offset
                if time.monotonic() - last_report > 30:
                    event("heartbeat bytes=" + str(guard.offset))
                    last_report = time.monotonic()
            event("capture complete; no subsequent key or reset")
        except BaseException as exc:
            error = repr(exc)
            event("capture exception: " + error)
            raise
        finally:
            if fd is not None:
                fcntl.ioctl(fd, termios.TIOCNXCL)
                os.close(fd)
            if fifo is not None:
                os.close(fifo)
            if created_fifo:
                os.unlink(args.control)
            with open(args.output + ".json", "x") as metadata:
                json.dump({"started_utc": started_utc, "bytes": guard.offset,
                           "sha256": guard.digest.hexdigest(), "stage": guard.selector.stage,
                           "stop_at_seabios_menu": args.stop_at_seabios_menu,
                           "stop_after_intenso": args.stop_after_intenso,
                           "error": error, "events": events}, metadata, indent=2)


if __name__ == "__main__":
    main()
