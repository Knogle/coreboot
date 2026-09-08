#!/usr/bin/env python3
"""Temporarily expose documented X58 IOH QPI PCI functions and read them.

The only configuration write is a masked update of DEVHIDE1 bits 26..29 on
IOH device 20/function 0.  The original masked value is restored in a finally
block and verified.  No QPI configuration register is written.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
import subprocess
import sys

import read_mmconfig


IOH_HUB_BDF = "00:14.0"
DEVHIDE1_REGISTER = "f0.l"
QPI_HIDE_MASK = 0x3C00_0000
QPI_BDFS = ("00:10.0", "00:10.1", "00:11.0", "00:11.1")


def setpci_read() -> int:
    result = subprocess.run(
        ("setpci", "-s", IOH_HUB_BDF, DEVHIDE1_REGISTER),
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    )
    return int(result.stdout.strip(), 16)


def setpci_masked_write(value: int, mask: int = QPI_HIDE_MASK) -> None:
    subprocess.run(
        (
            "setpci",
            "-s",
            IOH_HUB_BDF,
            f"{DEVHIDE1_REGISTER}={value & mask:08x}:{mask:08x}",
        ),
        check=True,
    )


def describe_config(data: bytes) -> dict[str, object]:
    vendor_id, device_id = struct.unpack_from("<HH", data)
    return {
        "vendor_id": f"0x{vendor_id:04x}",
        "device_id": f"0x{device_id:04x}",
        "accessible": not all(byte == 0xFF for byte in data),
        "config_hex": data.hex(),
    }


def probe(base: int, memory_device: Path) -> tuple[dict[str, object], bool]:
    original = setpci_read()
    result: dict[str, object] = {
        "access_model": (
            "masked DEVHIDE1 write; read-only MMCONFIG QPI capture; "
            "original hide bits restored in finally"
        ),
        "ioh_hub_bdf": IOH_HUB_BDF,
        "devhide1_original": f"0x{original:08x}",
        "qpi_hide_mask": f"0x{QPI_HIDE_MASK:08x}",
        "qpi_functions": [],
    }
    restored = False
    try:
        setpci_masked_write(0)
        exposed = setpci_read()
        result["devhide1_during_probe"] = f"0x{exposed:08x}"
        result["hide_bits_cleared"] = (exposed & QPI_HIDE_MASK) == 0
        for text_bdf in QPI_BDFS:
            bdf = read_mmconfig.parse_bdf(text_bdf)
            data = read_mmconfig.read_config(memory_device, base, bdf, 256)
            description = describe_config(data)
            description["bdf"] = text_bdf
            result["qpi_functions"].append(description)
    except Exception as error:  # restoration must still run for every failure
        result["probe_error"] = f"{type(error).__name__}: {error}"
    finally:
        try:
            setpci_masked_write(original)
            restored_value = setpci_read()
            result["devhide1_after_restore"] = f"0x{restored_value:08x}"
            restored = (restored_value & QPI_HIDE_MASK) == (
                original & QPI_HIDE_MASK
            )
            result["restore_verified"] = restored
        except Exception as error:
            result["restore_error"] = f"{type(error).__name__}: {error}"
            result["restore_verified"] = False
    return result, restored


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True, type=lambda value: int(value, 0))
    parser.add_argument("--memory-device", type=Path, default=Path("/dev/mem"))
    arguments = parser.parse_args()

    result, restored = probe(arguments.base, arguments.memory_device)
    json.dump(result, sys.stdout, indent=2, sort_keys=True)
    sys.stdout.write("\n")
    return 0 if restored else 2


if __name__ == "__main__":
    raise SystemExit(main())
