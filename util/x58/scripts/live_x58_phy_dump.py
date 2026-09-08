#!/usr/bin/env python3
"""Dump MINIT-described X58 PHY fields through the B06J SerialICE endpoint.

Descriptor input is read from stdin as ``index start width`` triples.  The
operation is logically read-only: it temporarily selects one scan channel and
issues only PHY read commands.  The original selector is restored on exit.
"""

from __future__ import annotations

import argparse
import json
import sys

from live_x58_rd_sweep import PHY_BUSY, PHY_COMMAND, PHY_COMMON, SerialICE
from live_x58_rd_sweep import X58Experiment


CHANNEL_MASK = 3 << 25


def descriptors():
    for line_number, line in enumerate(sys.stdin, 1):
        stripped = line.partition("#")[0].strip()
        if not stripped:
            continue
        fields = stripped.split()
        if len(fields) != 3:
            raise RuntimeError(f"descriptor line {line_number} is malformed")
        index, start, width = (int(field, 0) for field in fields)
        if width < 2 or width > 30 or start + width - 1 > 0x1603:
            raise RuntimeError(f"descriptor line {line_number} is invalid")
        yield index, start, width


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", default="/dev/ttyUSB1")
    parser.add_argument("--channel", type=int, choices=range(3), default=2)
    parser.add_argument("--byte-delay", type=float, default=0.0002)
    parser.add_argument("--timeout", type=float, default=1.0)
    args = parser.parse_args()

    serial = SerialICE(args.device, args.byte_delay, args.timeout)
    experiment = None
    original_select = None
    try:
        version = serial.command("vi")
        if "SerialICE v1.5 B06J-X58" not in version:
            raise RuntimeError(f"unexpected endpoint: {version!r}")
        experiment = X58Experiment(serial)
        experiment.poll_clear(PHY_COMMON, PHY_COMMAND, PHY_BUSY)
        original_select = experiment.pci_read32(PHY_COMMON, 0x5C)
        selected = (original_select & ~CHANNEL_MASK) | args.channel << 25
        experiment.pci_write32(PHY_COMMON, 0x5C, selected)
        readback = experiment.pci_read32(PHY_COMMON, 0x5C)
        if readback != selected:
            raise RuntimeError(
                f"channel selector readback 0x{readback:08x} != 0x{selected:08x}"
            )
        print(
            json.dumps(
                {
                    "event": "start",
                    "channel": args.channel,
                    "selector_before": original_select,
                    "selector_selected": selected,
                    "qpi_ph_pis": experiment.pci_read32((0xFF, 2, 1), 0x80),
                },
                separators=(",", ":"),
            ),
            flush=True,
        )
        count = 0
        for index, start, width in descriptors():
            value = experiment.phy_read(start, width)
            print(
                json.dumps(
                    {
                        "index": index,
                        "start": start,
                        "width": width,
                        "value": value,
                    },
                    separators=(",", ":"),
                ),
                flush=True,
            )
            count += 1
        print(
            json.dumps(
                {
                    "event": "complete",
                    "count": count,
                    "qpi_ph_pis": experiment.pci_read32((0xFF, 2, 1), 0x80),
                },
                separators=(",", ":"),
            ),
            flush=True,
        )
    finally:
        if experiment is not None and original_select is not None:
            experiment.poll_clear(PHY_COMMON, PHY_COMMAND, PHY_BUSY)
            experiment.pci_write32(PHY_COMMON, 0x5C, original_select)
        serial.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
