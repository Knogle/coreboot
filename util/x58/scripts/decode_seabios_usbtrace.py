#!/usr/bin/env python3
"""Decode bounded SeaBIOS ``[USBTRACE]`` blocks from a serial log.

The diagnostic SeaBIOS patch records USB events in RAM while timing-sensitive
controller and enumeration code runs.  It prints the bounded append-only
buffer once, after ``usb_setup()``.  This tool ignores all unrelated serial
text, validates each trace block, and gives event names to its compact fields.
"""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import json
from pathlib import Path
import re
import sys
from typing import Iterable


EVENT_NAMES = {
    0x01: "setup_begin",
    0x02: "setup_end",
    0x10: "ehci_caps",
    0x11: "ehci_reset_pre",
    0x12: "ehci_reset_post",
    0x13: "ehci_run",
    0x14: "ehci_port_snapshot",
    0x15: "ehci_connect_reset_start",
    0x16: "ehci_reset_result",
    0x17: "ehci_companion_owner",
    0x20: "uhci_reset_pre",
    0x21: "uhci_reset_post",
    0x22: "uhci_run",
    0x23: "uhci_port_snapshot",
    0x24: "uhci_connect_reset_start",
    0x25: "uhci_reset_result",
    0x30: "device_detect",
    0x31: "device_reset",
    0x32: "set_address",
    0x33: "device_descriptor_8",
    0x34: "configuration_header",
    0x35: "configuration_full",
    0x36: "set_configuration",
    0x37: "driver_probe",
    0x38: "hid_boot_protocol",
    0x39: "hid_interrupt_pipe",
    0x3F: "disconnect",
}

CONTROLLER_TYPES = {
    0: "none",
    1: "uhci",
    2: "ohci",
    3: "ehci",
    4: "xhci",
}

BEGIN_RE = re.compile(r"^\[USBTRACE\] BEGIN count=(\d+) dropped=(\d+)\s*$")
ENTRY_RE = re.compile(
    r"^\[USBTRACE\]\s+(\d+)\s+EVT=([0-9a-fA-F]{2})\s+"
    r"TYPE=(\d+)\s+BDF=([0-9a-fA-F]{2}):([0-9a-fA-F]{2})\.([0-7])\s+"
    r"PORT=(\d+)\s+RET=(-?\d+)\s+A=([0-9a-fA-F]{8})\s+"
    r"B=([0-9a-fA-F]{8})\s*$"
)
END_RE = re.compile(r"^\[USBTRACE\] END\s*$")


@dataclass(frozen=True)
class UsbTraceEvent:
    index: int
    event: int
    event_name: str
    controller_type: int
    controller_name: str
    bdf: str | None
    port: int | None
    result: int
    value_a: int
    value_b: int


@dataclass(frozen=True)
class UsbTraceBlock:
    reported_count: int
    dropped: int
    events: tuple[UsbTraceEvent, ...]
    complete: bool
    errors: tuple[str, ...]

    @property
    def valid(self) -> bool:
        return self.complete and not self.errors and self.dropped == 0


def _event_from_match(match: re.Match[str]) -> UsbTraceEvent:
    index = int(match.group(1), 10)
    event = int(match.group(2), 16)
    controller_type = int(match.group(3), 10)
    bus = int(match.group(4), 16)
    device = int(match.group(5), 16)
    function = int(match.group(6), 16)
    raw_port = int(match.group(7), 10)
    bdf = None if (bus, device, function) == (0xFF, 0x1F, 7) else (
        f"{bus:02x}:{device:02x}.{function:x}"
    )
    return UsbTraceEvent(
        index=index,
        event=event,
        event_name=EVENT_NAMES.get(event, f"unknown_0x{event:02x}"),
        controller_type=controller_type,
        controller_name=CONTROLLER_TYPES.get(
            controller_type, f"unknown_{controller_type}"
        ),
        bdf=bdf,
        port=None if raw_port == 0xFF else raw_port,
        result=int(match.group(8), 10),
        value_a=int(match.group(9), 16),
        value_b=int(match.group(10), 16),
    )


def _finish_block(
    reported_count: int,
    dropped: int,
    events: list[UsbTraceEvent],
    complete: bool,
    inherited_errors: Iterable[str] = (),
) -> UsbTraceBlock:
    errors = list(inherited_errors)
    indices = [event.index for event in events]
    if indices != list(range(len(events))):
        errors.append(f"non-contiguous indices: {indices!r}")
    if reported_count != len(events):
        errors.append(
            f"reported count {reported_count} differs from parsed count {len(events)}"
        )
    if dropped:
        errors.append(f"firmware dropped {dropped} event(s)")
    return UsbTraceBlock(
        reported_count=reported_count,
        dropped=dropped,
        events=tuple(events),
        complete=complete,
        errors=tuple(errors),
    )


def parse_usbtrace(lines: Iterable[str]) -> list[UsbTraceBlock]:
    """Parse all trace blocks in *lines*, retaining incomplete blocks."""

    blocks: list[UsbTraceBlock] = []
    active: tuple[int, int, list[UsbTraceEvent], list[str]] | None = None

    for line_number, raw_line in enumerate(lines, 1):
        line = raw_line.rstrip("\r\n")
        begin = BEGIN_RE.match(line)
        if begin:
            if active is not None:
                count, dropped, events, errors = active
                errors.append(f"new BEGIN at line {line_number} before END")
                blocks.append(_finish_block(count, dropped, events, False, errors))
            active = (int(begin.group(1)), int(begin.group(2)), [], [])
            continue

        entry = ENTRY_RE.match(line)
        if entry:
            if active is not None:
                active[2].append(_event_from_match(entry))
            continue

        if END_RE.match(line):
            if active is not None:
                count, dropped, events, errors = active
                blocks.append(_finish_block(count, dropped, events, True, errors))
                active = None
            continue

        if line.startswith("[USBTRACE]") and active is not None:
            active[3].append(f"malformed trace line {line_number}: {line}")

    if active is not None:
        count, dropped, events, errors = active
        errors.append("end of input before END")
        blocks.append(_finish_block(count, dropped, events, False, errors))
    return blocks


def block_to_dict(block: UsbTraceBlock) -> dict[str, object]:
    result = asdict(block)
    for event in result["events"]:
        event["event_hex"] = f"0x{event['event']:02x}"
        event["value_a_hex"] = f"0x{event['value_a']:08x}"
        event["value_b_hex"] = f"0x{event['value_b']:08x}"
    result["valid"] = block.valid
    return result


def format_block(block: UsbTraceBlock, number: int) -> str:
    state = "VALID" if block.valid else "INVALID"
    lines = [
        f"trace {number}: {state}; events={len(block.events)}/"
        f"{block.reported_count}; dropped={block.dropped}; complete={int(block.complete)}"
    ]
    for event in block.events:
        location = event.bdf or "global"
        if event.port is not None:
            location += f"/port{event.port}"
        lines.append(
            f"  {event.index:02d}  {event.event_name:<26} "
            f"{event.controller_name:<5} {location:<15} ret={event.result:<3} "
            f"a={event.value_a:08x} b={event.value_b:08x}"
        )
    lines.extend(f"  ERROR: {error}" for error in block.errors)
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path, help="SeaBIOS/coreboot serial log")
    parser.add_argument("--json", action="store_true", help="emit JSON")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="return status 2 if no trace exists or any block is invalid",
    )
    args = parser.parse_args(argv)

    try:
        with args.log.open("r", encoding="utf-8", errors="replace") as stream:
            blocks = parse_usbtrace(stream)
    except OSError as error:
        parser.error(str(error))

    if args.json:
        print(json.dumps([block_to_dict(block) for block in blocks], indent=2))
    elif not blocks:
        print("no [USBTRACE] block found", file=sys.stderr)
    else:
        print("\n\n".join(format_block(block, i) for i, block in enumerate(blocks, 1)))

    if args.strict and (not blocks or any(not block.valid for block in blocks)):
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
