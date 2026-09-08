#!/usr/bin/env python3
"""Inventory ACPI tables in an extracted AMIBIOS8 module without modifying it.

The scanner reports standard ACPI fields and deliberately leaves table
placement and pointer interpretation to the caller.  Static firmware templates
often contain zero pointers and invalid checksums until their loader relocates
and patches them at run time.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
from typing import Any


STANDARD_SIGNATURES = {
    b"APIC",
    b"DSDT",
    b"FACP",
    b"HPET",
    b"MCFG",
    b"OEMB",
    b"RSDT",
    b"SSDT",
    b"XSDT",
}


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def u64(data: bytes, offset: int) -> int:
    return struct.unpack_from("<Q", data, offset)[0]


def hex0(value: int) -> str:
    return f"0x{value:x}"


def text_field(raw: bytes) -> str:
    return raw.decode("ascii", errors="replace").rstrip(" \0")


def gas(data: bytes, offset: int) -> dict[str, Any]:
    return {
        "address_space_id": data[offset],
        "bit_width": data[offset + 1],
        "bit_offset": data[offset + 2],
        "access_size": data[offset + 3],
        "address": hex0(u64(data, offset + 4)),
    }


def decode_fadt(table: bytes) -> dict[str, Any]:
    decoded: dict[str, Any] = {}
    if len(table) < 116:
        return decoded

    decoded.update(
        {
            "firmware_ctrl": hex0(u32(table, 36)),
            "dsdt": hex0(u32(table, 40)),
            "preferred_pm_profile": table[45],
            "sci_interrupt": u16(table, 46),
            "smi_command_port": hex0(u32(table, 48)),
            "acpi_enable": hex0(table[52]),
            "acpi_disable": hex0(table[53]),
            "s4bios_request": hex0(table[54]),
            "pstate_control": hex0(table[55]),
            "pm1a_event_block": hex0(u32(table, 56)),
            "pm1b_event_block": hex0(u32(table, 60)),
            "pm1a_control_block": hex0(u32(table, 64)),
            "pm1b_control_block": hex0(u32(table, 68)),
            "pm2_control_block": hex0(u32(table, 72)),
            "pm_timer_block": hex0(u32(table, 76)),
            "gpe0_block": hex0(u32(table, 80)),
            "gpe1_block": hex0(u32(table, 84)),
            "pm1_event_length": table[88],
            "pm1_control_length": table[89],
            "pm2_control_length": table[90],
            "pm_timer_length": table[91],
            "gpe0_length": table[92],
            "gpe1_length": table[93],
            "iapc_boot_arch": hex0(u16(table, 109)),
            "flags": hex0(u32(table, 112)),
        }
    )
    if len(table) >= 129:
        decoded["reset_register"] = gas(table, 116)
        decoded["reset_value"] = hex0(table[128])
    return decoded


def decode_madt(table: bytes) -> dict[str, Any]:
    if len(table) < 44:
        return {}
    entries: list[dict[str, Any]] = []
    offset = 44
    while offset + 2 <= len(table):
        entry_type = table[offset]
        length = table[offset + 1]
        if length < 2 or offset + length > len(table):
            entries.append(
                {
                    "offset": hex0(offset),
                    "type": entry_type,
                    "length": length,
                    "truncated": True,
                }
            )
            break
        entry: dict[str, Any] = {
            "offset": hex0(offset),
            "type": entry_type,
            "length": length,
        }
        if entry_type == 0 and length == 8:
            entry.update(
                {
                    "kind": "local_apic",
                    "processor_id": table[offset + 2],
                    "apic_id": table[offset + 3],
                    "flags": hex0(u32(table, offset + 4)),
                }
            )
        elif entry_type == 1 and length == 12:
            entry.update(
                {
                    "kind": "io_apic",
                    "io_apic_id": table[offset + 2],
                    "address": hex0(u32(table, offset + 4)),
                    "gsi_base": u32(table, offset + 8),
                }
            )
        elif entry_type == 2 and length == 10:
            entry.update(
                {
                    "kind": "interrupt_source_override",
                    "bus": table[offset + 2],
                    "source_irq": table[offset + 3],
                    "gsi": u32(table, offset + 4),
                    "flags": hex0(u16(table, offset + 8)),
                }
            )
        entries.append(entry)
        offset += length
    return {
        "local_apic_address": hex0(u32(table, 36)),
        "flags": hex0(u32(table, 40)),
        "entries": entries,
    }


def decode_mcfg(table: bytes) -> dict[str, Any]:
    if len(table) < 44:
        return {}
    allocations = []
    for offset in range(44, len(table) - 15, 16):
        allocations.append(
            {
                "base_address": hex0(u64(table, offset)),
                "segment": u16(table, offset + 8),
                "start_bus": table[offset + 10],
                "end_bus": table[offset + 11],
            }
        )
    return {"allocations": allocations}


def decode_hpet(table: bytes) -> dict[str, Any]:
    if len(table) < 56:
        return {}
    return {
        "event_timer_block_id": hex0(u32(table, 36)),
        "base_address": gas(table, 40),
        "hpet_number": table[52],
        "minimum_clock_tick": u16(table, 53),
        "page_protection": table[55],
    }


def decode_root(table: bytes, entry_size: int) -> dict[str, Any]:
    if len(table) < 36 or (len(table) - 36) % entry_size:
        return {}
    unpack = u32 if entry_size == 4 else u64
    return {
        "entries": [
            hex0(unpack(table, offset))
            for offset in range(36, len(table), entry_size)
        ]
    }


def decode_table(signature: bytes, table: bytes) -> dict[str, Any]:
    if signature == b"FACP":
        return decode_fadt(table)
    if signature == b"APIC":
        return decode_madt(table)
    if signature == b"MCFG":
        return decode_mcfg(table)
    if signature == b"HPET":
        return decode_hpet(table)
    if signature == b"RSDT":
        return decode_root(table, 4)
    if signature == b"XSDT":
        return decode_root(table, 8)
    return {}


def scan_standard_tables(data: bytes) -> list[dict[str, Any]]:
    found: list[dict[str, Any]] = []
    for offset in range(0, len(data) - 36 + 1):
        signature = data[offset : offset + 4]
        if signature not in STANDARD_SIGNATURES:
            continue
        length = u32(data, offset + 4)
        if length < 36 or offset + length > len(data):
            continue
        table = data[offset : offset + length]
        record: dict[str, Any] = {
            "offset": hex0(offset),
            "signature": text_field(signature),
            "length": length,
            "revision": table[8],
            "checksum_byte": hex0(table[9]),
            "checksum_sum": hex0(sum(table) & 0xFF),
            "checksum_valid": (sum(table) & 0xFF) == 0,
            "oem_id": text_field(table[10:16]),
            "oem_table_id": text_field(table[16:24]),
            "oem_revision": hex0(u32(table, 24)),
            "creator_id": text_field(table[28:32]),
            "creator_revision": hex0(u32(table, 32)),
        }
        decoded = decode_table(signature, table)
        if decoded:
            record["decoded"] = decoded
        found.append(record)
    return found


def scan_facs(data: bytes) -> list[dict[str, Any]]:
    found: list[dict[str, Any]] = []
    start = 0
    while True:
        offset = data.find(b"FACS", start)
        if offset < 0:
            break
        start = offset + 1
        if offset + 64 > len(data):
            continue
        length = u32(data, offset + 4)
        if length < 64 or offset + length > len(data):
            continue
        facs = data[offset : offset + length]
        found.append(
            {
                "offset": hex0(offset),
                "length": length,
                "hardware_signature": hex0(u32(facs, 8)),
                "firmware_waking_vector": hex0(u32(facs, 12)),
                "global_lock": hex0(u32(facs, 16)),
                "flags": hex0(u32(facs, 20)),
                "x_firmware_waking_vector": hex0(u64(facs, 24)),
                "version": facs[32],
            }
        )
    return found


def inventory(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    return {
        "path": str(path),
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "tables": scan_standard_tables(data),
        "facs": scan_facs(data),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("images", nargs="+", type=Path)
    args = parser.parse_args()
    missing = [str(path) for path in args.images if not path.is_file()]
    if missing:
        parser.error("not a regular file: " + ", ".join(missing))
    print(json.dumps([inventory(path) for path in args.images], indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
