#!/usr/bin/env python3
"""Compare two JSON snapshots produced by dump_nehalem_uncore.py."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct
from typing import Any


def _device_key(device: dict[str, Any]) -> tuple[str, str, str, str]:
    return (
        device["bdf"],
        device.get("vendor_id") or "",
        device.get("device_id") or "",
        device["role"],
    )


def _device_identity(key: tuple[str, str, str, str]) -> dict[str, Any]:
    return {
        "bdf": key[0],
        "vendor_id": key[1] or None,
        "device_id": key[2] or None,
        "role": key[3],
    }


def _register_names(device: dict[str, Any]) -> dict[int, str]:
    names = {}
    for register in device.get("named_registers", []):
        names[int(register["offset"], 16)] = register["name"]
    return names


def _changed_dwords(
    before: dict[str, Any], after: dict[str, Any], only_known: bool
) -> list[dict[str, Any]]:
    left = bytes.fromhex(before.get("config_hex", ""))
    right = bytes.fromhex(after.get("config_hex", ""))
    names = _register_names(before) | _register_names(after)
    limit = min(len(left), len(right)) & ~3
    changes = []
    for offset in range(0, limit, 4):
        old = struct.unpack_from("<I", left, offset)[0]
        new = struct.unpack_from("<I", right, offset)[0]
        if old == new or (only_known and offset not in names):
            continue
        change = {
            "offset": f"0x{offset:03x}",
            "width_bytes": 4,
            "before": f"0x{old:08x}",
            "after": f"0x{new:08x}",
            "xor": f"0x{old ^ new:08x}",
        }
        if offset in names:
            change["name"] = names[offset]
        changes.append(change)
    return changes


def compare_snapshots(
    before: dict[str, Any], after: dict[str, Any], only_known: bool = False
) -> dict[str, Any]:
    left = {_device_key(device): device for device in before.get("devices", [])}
    right = {_device_key(device): device for device in after.get("devices", [])}
    common = sorted(left.keys() & right.keys())
    changed_devices = []
    for key in common:
        capture_changes = {}
        for field in ("config_error", "config_bytes_requested", "config_bytes_read"):
            old = left[key].get(field)
            new = right[key].get(field)
            if old != new:
                capture_changes[field] = {"before": old, "after": new}
        changes = _changed_dwords(left[key], right[key], only_known)
        if changes or capture_changes:
            entry = {**_device_identity(key), "dword_changes": changes}
            if capture_changes:
                entry["capture_changes"] = capture_changes
            changed_devices.append(entry)
    return {
        "schema_version": 1,
        "only_known_registers": only_known,
        "added_devices": [
            _device_identity(key) for key in sorted(right.keys() - left.keys())
        ],
        "removed_devices": [
            _device_identity(key) for key in sorted(left.keys() - right.keys())
        ],
        "changed_devices": changed_devices,
    }


def _load(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValueError(f"cannot read {path}: {error}") from error
    if value.get("schema_version") != 1 or not isinstance(value.get("devices"), list):
        raise ValueError(f"not an Uncore snapshot: {path}")
    return value


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    parser.add_argument(
        "--only-known",
        action="store_true",
        help="report only offsets named from the Linux i7core_edac register set",
    )
    args = parser.parse_args()
    try:
        result = compare_snapshots(
            _load(args.before), _load(args.after), only_known=args.only_known
        )
    except ValueError as error:
        parser.error(str(error))
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
