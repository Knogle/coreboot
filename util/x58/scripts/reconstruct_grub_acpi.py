#!/usr/bin/env python3
"""Reconstruct ACPI bytes explicitly observed in ANSI-decorated GRUB hexdumps.

No gaps are zero-filled and no RSDP is synthesized from decoded text. Repeated
observations must agree byte for byte. Existing inputs/outputs are untouched;
output is a new directory of diagnostic tables, never a firmware image.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

from analyze_msi_acpi import decode_table, gas
from x58_paths import coreboot_root


ROOT = Path(__file__).resolve().parents[1]
ANSI = re.compile(rb"\x1b\[[0-?]*[ -/]*[@-~]")
ROW = re.compile(r"^([0-9a-fA-F]{8,16})\s{2,}([^|]+)\|(.*)\|\s*$")
ROW_START = re.compile(r"^[0-9a-fA-F]{8,16}\s{2,}")
DEFAULT_TABLES = {
    "RSDP": 0x1676000, "RSDT": 0x1676030, "XSDT": 0x16760E0,
    "FACS": 0x1676240, "DSDT": 0x1676280, "FADT": 0x16766C0,
    "SSDT": 0x16767E0, "MCFG": 0x1676860, "APIC": 0x16768A0,
    "SPCR": 0x1676900, "HPET": 0x1676960,
}
RELEASE_DSDT_SHA = "63ba8a0d497cc6998bfffd35ea816d7bba5d85f811337ce0bfb21f7c854d7d03"


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def parse_rows(raw: bytes) -> list[tuple[int, int, bytes]]:
    cleaned = ANSI.sub(b"", raw).decode("ascii", errors="replace")
    rows = []
    for number, line in enumerate(cleaned.splitlines(), 1):
        match = ROW.fullmatch(line)
        if not match:
            if ROW_START.match(line):
                raise ValueError(f"malformed/truncated hexdump row at normalized line {number}")
            continue
        tokens = match[2].split()
        if not 1 <= len(tokens) <= 16 or any(not re.fullmatch(r"[0-9a-fA-F]{2}", t) for t in tokens):
            raise ValueError(f"invalid hexdump byte column at normalized line {number}")
        if len(match[3]) != len(tokens):
            raise ValueError(f"hexdump ASCII/byte column length mismatch at normalized line {number}")
        rows.append((number, int(match[1], 16), bytes.fromhex(" ".join(tokens))))
    return rows


def add_rows(memory: dict[int, int], rows: list[tuple[int, int, bytes]]) -> None:
    for line, address, data in rows:
        for offset, byte in enumerate(data):
            where = address + offset
            if where in memory and memory[where] != byte:
                raise ValueError(f"conflicting observed byte at {where:#x}, normalized line {line}")
            memory[where] = byte


def read_exact(memory: dict[int, int], address: int, size: int) -> bytes:
    if not 0 < size <= 1024 * 1024:
        raise ValueError("invalid/unbounded requested table length")
    missing = next((p for p in range(address, address + size) if p not in memory), None)
    if missing is not None:
        raise ValueError(f"missing explicitly observed byte at {missing:#x}")
    return bytes(memory[p] for p in range(address, address + size))


def read_table(memory: dict[int, int], name: str, address: int) -> tuple[bytes, dict]:
    signature = b"RSD PTR " if name == "RSDP" else ("FACP" if name == "FADT" else name).encode()
    if read_exact(memory, address, len(signature)) != signature:
        raise ValueError(f"unexpected {name} signature at {address:#x}")
    if name == "RSDP":
        header = read_exact(memory, address, 20)
        length = 20 if header[15] == 0 else struct.unpack_from("<I", read_exact(memory, address, 24), 20)[0]
        if header[15] != 0 and length < 36:
            raise ValueError("extended RSDP shorter than 36 bytes")
        data = read_exact(memory, address, length)
        if sum(data[:20]) & 255 or sum(data) & 255:
            raise ValueError("RSDP checksum invalid")
        fields = {"revision": header[15], "rsdt_address": hex(struct.unpack_from("<I", header, 16)[0]),
                  "legacy_checksum_valid": True, "extended_checksum_valid": True if length > 20 else None}
        if length >= 36:
            fields["xsdt_address"] = hex(struct.unpack_from("<Q", data, 24)[0])
    else:
        length = struct.unpack_from("<I", read_exact(memory, address, 8), 4)[0]
        if length < (64 if name == "FACS" else 36):
            raise ValueError("ACPI table shorter than required header")
        data = read_exact(memory, address, length)
        if name == "FACS":
            fields = {"checksum_valid": None, "checksum_note": "FACS has no ACPI checksum field",
                      "hardware_signature": hex(struct.unpack_from("<I", data, 8)[0]),
                      "firmware_waking_vector": hex(struct.unpack_from("<I", data, 12)[0]),
                      "global_lock": hex(struct.unpack_from("<I", data, 16)[0]),
                      "flags": hex(struct.unpack_from("<I", data, 20)[0]), "version": data[32]}
        else:
            if sum(data) & 255:
                raise ValueError("ACPI table checksum invalid")
            fields = {"revision": data[8], "checksum_valid": True,
                      "oem_id": data[10:16].decode("ascii", "replace"),
                      "oem_table_id": data[16:24].decode("ascii", "replace"),
                      "decoded": decode_table(signature, data)}
            if name == "FADT" and length >= 244:
                fields["decoded"].update({"x_firmware_ctrl": hex(struct.unpack_from("<Q", data, 132)[0]),
                                           "x_dsdt": hex(struct.unpack_from("<Q", data, 140)[0]),
                                           "x_pm1a_event_block": gas(data, 148),
                                           "x_pm1a_control_block": gas(data, 172),
                                           "x_pm_timer_block": gas(data, 208),
                                           "x_gpe0_block": gas(data, 220),
                                           "p_lvl2_latency": struct.unpack_from("<H", data, 96)[0],
                                           "p_lvl3_latency": struct.unpack_from("<H", data, 98)[0]})
    return data, {"name": name, "physical_address": hex(address), "length": len(data),
                  "sha256": sha(data), **fields}


def run_iasl(command: list[str], log: Path) -> dict:
    result = subprocess.run(command, capture_output=True, text=True, check=False, cwd=ROOT)
    with log.open("x") as stream:
        stream.write(result.stdout)
        stream.write(result.stderr)
    return {"command": command, "exit_code": result.returncode,
            "log": log.name, "log_sha256": sha(log.read_bytes()),
            "compile_zero_errors_warnings": bool(re.search(r"Compilation successful\. 0 Errors, 0 Warnings", result.stdout))}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", nargs="+", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--table", action="append", help="explicit NAME=0xADDRESS; replaces WJ defaults")
    parser.add_argument("--iasl", type=Path, default=coreboot_root() / "util/crossgcc/xgcc/bin/iasl")
    args = parser.parse_args()
    tables = DEFAULT_TABLES.copy()
    if args.table:
        tables = {}
        for value in args.table:
            name, address = value.split("=", 1)
            if name not in DEFAULT_TABLES:
                parser.error(f"unsupported table name: {name}")
            tables[name] = int(address, 0)
    inputs, memory, observations = [], {}, []
    for path in args.captures:
        raw = path.read_bytes()
        rows = parse_rows(raw)
        add_rows(memory, rows)
        inputs.append({"path": str(path), "bytes": len(raw), "sha256": sha(raw), "rows": len(rows)})
        observations.extend({"capture": str(path), "normalized_line": line,
                             "address": hex(address), "bytes": len(data)} for line, address, data in rows)
    output = args.output.resolve()
    if not output.is_relative_to((ROOT / "builds/experimental").resolve()):
        parser.error("diagnostic output must be beneath builds/experimental")
    output.mkdir(parents=True, exist_ok=False)
    artifacts, errors = [], []
    for name, address in tables.items():
        try:
            data, record = read_table(memory, name, address)
        except ValueError as error:
            errors.append({"table": name, "physical_address": hex(address), "error": str(error)})
            continue
        filename = name + (".aml" if name in ("DSDT", "SSDT") else ".bin")
        with (output / filename).open("xb") as stream:
            stream.write(data)
        record["file"] = filename
        if name == "DSDT":
            record["matches_released_b06wj_dsdt"] = sha(data) == RELEASE_DSDT_SHA
        artifacts.append(record)
    aml_tables = [a for a in artifacts if a["name"] in ("DSDT", "SSDT")]
    for artifact in aml_tables:
        name = artifact["name"]
        command = [str(args.iasl), "-p", str(output / name)]
        external = [str(output / a["file"]) for a in aml_tables if a != artifact]
        if external:
            command += ["-e", *external]
        command += ["-d", str(output / artifact["file"])]
        disasm = run_iasl(command, output / f"{name}-disassemble.log")
        artifact["iasl_disassembly"] = disasm
        if disasm["exit_code"] == 0:
            compile_result = run_iasl([str(args.iasl), "-p", str(output / f"{name}-recompiled"),
                                       str(output / f"{name}.dsl")], output / f"{name}-compile.log")
            artifact["iasl_compilation"] = compile_result
            if compile_result["exit_code"] == 0:
                rebuilt = (output / f"{name}-recompiled.aml").read_bytes()
                original = (output / artifact["file"]).read_bytes()
                artifact["recompiled_sha256"] = sha(rebuilt)
                artifact["recompiled_body_identical"] = rebuilt[36:] == original[36:]
                artifact["recompiled_byte_identical"] = rebuilt == original
    by_name = {a["name"]: a for a in artifacts}
    relationships = {}
    if "RSDT" in by_name and "XSDT" in by_name:
        relationships["rsdt_xsdt_entries_equal"] = by_name["RSDT"]["decoded"]["entries"] == by_name["XSDT"]["decoded"]["entries"]
    if "FADT" in by_name:
        decoded = by_name["FADT"]["decoded"]
        relationships["fadt_legacy_extended_dsdt_equal"] = decoded["dsdt"] == decoded.get("x_dsdt")
        relationships["fadt_legacy_extended_facs_equal"] = decoded["firmware_ctrl"] == decoded.get("x_firmware_ctrl")
        for key, field in (("DSDT", "dsdt"), ("FACS", "firmware_ctrl")):
            if key in by_name:
                relationships[f"fadt_{key.lower()}_points_to_reconstructed_table"] = decoded[field] == by_name[key]["physical_address"]
    manifest = {"inputs": inputs, "observed_rows": observations, "unique_observed_bytes": len(memory),
                "tables": artifacts, "errors": errors, "relationships": relationships,
                "all_requested_tables_complete": not errors,
                "script_sha256": sha(Path(__file__).read_bytes()),
                "iasl_sha256": sha(args.iasl.read_bytes()),
                "scope": "Read-only reconstruction from captured bytes; checksum validity is not Windows ACPI semantic acceptance."}
    with (output / "manifest.json").open("x") as stream:
        json.dump(manifest, stream, indent=2, sort_keys=True)
        stream.write("\n")
    print(json.dumps({"output": str(output), "tables": len(artifacts), "errors": errors,
                      "relationships": relationships,
                      "dsdt_matches_release": by_name.get("DSDT", {}).get("matches_released_b06wj_dsdt")}, indent=2))
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
