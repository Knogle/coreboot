#!/usr/bin/env python3
"""Read one explicitly named PCI function through an existing MMCONFIG window.

The tool opens /dev/mem read-only.  It neither scans buses nor writes PCI
configuration state.  Use it only with a PCIEXBAR base already observed from
the running firmware and an explicitly documented BDF.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys


BDF_RE = re.compile(
    r"^(?:(?P<domain>[0-9a-fA-F]{4}):)?(?P<bus>[0-9a-fA-F]{2}):"
    r"(?P<device>[0-9a-fA-F]{2})\.(?P<function>[0-7])$"
)


def parse_bdf(text: str) -> tuple[int, int, int]:
    match = BDF_RE.fullmatch(text)
    if not match:
        raise ValueError(f"invalid BDF: {text}")
    if match.group("domain") not in (None, "0000"):
        raise ValueError("MMCONFIG base applies only to PCI segment 0000")
    bus = int(match.group("bus"), 16)
    device = int(match.group("device"), 16)
    function = int(match.group("function"), 16)
    if device > 31:
        raise ValueError(f"PCI device out of range: {device}")
    return bus, device, function


def mmconfig_address(
    base: int, bus: int, device: int, function: int, offset: int = 0
) -> int:
    if base < 0 or base & ((1 << 20) - 1):
        raise ValueError("MMCONFIG base must be non-negative and 1 MiB aligned")
    if not 0 <= bus <= 255:
        raise ValueError("PCI bus out of range")
    if not 0 <= device <= 31:
        raise ValueError("PCI device out of range")
    if not 0 <= function <= 7:
        raise ValueError("PCI function out of range")
    if not 0 <= offset <= 4095:
        raise ValueError("PCI configuration offset out of range")
    return base | (bus << 20) | (device << 15) | (function << 12) | offset


def read_config(
    memory_device: Path, base: int, bdf: tuple[int, int, int], length: int
) -> bytes:
    address = mmconfig_address(base, *bdf)
    with memory_device.open("rb", buffering=0) as stream:
        stream.seek(address)
        data = stream.read(length)
    if len(data) != length:
        raise OSError(f"short /dev/mem read: requested {length}, received {len(data)}")
    return data


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True, type=lambda value: int(value, 0))
    parser.add_argument("--bdf", required=True, type=parse_bdf)
    parser.add_argument("--length", type=int, choices=(4, 256, 4096), default=256)
    parser.add_argument("--memory-device", type=Path, default=Path("/dev/mem"))
    arguments = parser.parse_args()

    try:
        data = read_config(
            arguments.memory_device, arguments.base, arguments.bdf, arguments.length
        )
    except (OSError, ValueError) as error:
        parser.error(str(error))

    sys.stdout.write(data.hex() + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
