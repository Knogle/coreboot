#!/usr/bin/env python3
"""Recover direct PCI configuration accesses from MSI X58 MINITDLL.

The MSI A7522IMS.8F0 MINITDLL contains small cdecl MMCONFIG helpers.  Their
arguments are pushed in the following source order immediately before a call:

    read:  offset, function, device, bus, context
    write: value, offset, function, device, bus, context

This script disassembles a user-supplied PE with GNU objdump, recognizes calls
to those helpers, and decodes only literal device/function/offset arguments.
Register names come from Intel Xeon 5500 Series Datasheet Volume 2 (321322)
and its Xeon 5600 Series supplement (323370).  Dynamic operands remain
explicitly unresolved; the tool does not guess them.
"""

from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
from typing import Any


PROFILE = "msi-x58-pro-e-minitdll-text-c93135af"
EXPECTED_TEXT_SHA256 = "c93135af56b85c28bb6f6da6589a2f5a7a35c6a79407b21745d369bc1aa292f4"
LEGACY_NORMALIZED_SHA256 = (
    "217d325ef82c4af1da4a95c158c3109b1de0f7975ce0b180e562f00c51023189"
)


@dataclass(frozen=True)
class Wrapper:
    operation: str
    width_bytes: int

    @property
    def argument_names(self) -> tuple[str, ...]:
        common = ("offset", "function", "device", "bus", "context")
        if self.operation == "write":
            return ("value", *common)
        return common


WRAPPERS = {
    0xFFFC47B6: Wrapper("read", 1),
    0xFFFC47E2: Wrapper("read", 2),
    0xFFFC480F: Wrapper("read", 4),
    0xFFFC486E: Wrapper("write", 1),
    0xFFFC489F: Wrapper("write", 2),
    0xFFFC48D2: Wrapper("write", 4),
}


BDF_ROLES = {
    (0, 0): "generic_noncore",
    (0, 1): "system_address_decoder",
    (2, 0): "qpi_link0",
    (2, 1): "qpi_phy0",
    (2, 4): "qpi_link1",
    (2, 5): "qpi_phy1",
    (3, 0): "memory_controller",
    (3, 1): "target_address_decode",
    (3, 2): "memory_controller_ras",
    (3, 4): "memory_controller_test",
}
for _channel in range(3):
    for _function, _suffix in enumerate(("control", "address", "rank", "timing")):
        BDF_ROLES[(4 + _channel, _function)] = f"channel{_channel}_{_suffix}"


FIXED_REGISTER_NAMES = {
    ("generic_noncore", 0x60): "MAX_RTIDS",
    ("generic_noncore", 0x80): "DESIRED_CORES",
    ("generic_noncore", 0x88): "MEMLOCK_STATUS",
    ("generic_noncore", 0x90): "MC_CFG_CONTROL",
    ("generic_noncore", 0xC0): "CURRENT_UCLK_RATIO",
    ("system_address_decoder", 0x50): "SAD_PCIEXBAR_LO",
    ("qpi_link0", 0x48): "QPI_QPILCL_L0",
    ("qpi_link0", 0x50): "QPI_QPILS_L0",
    ("qpi_phy0", 0x80): "QPI_0_PH_PIS",
    ("memory_controller", 0x48): "MC_CONTROL",
    ("memory_controller", 0x4C): "MC_STATUS",
    ("memory_controller", 0x5C): "MC_RESET_CONTROL",
    ("memory_controller", 0x60): "MC_CHANNEL_MAPPER",
    ("memory_controller", 0x64): "MC_MAX_DOD",
    ("memory_controller", 0x70): "MC_RD_CRDT_INIT",
    ("memory_controller", 0x74): "MC_CRDT_WR_THLD",
    ("memory_controller", 0x78): "MC_SCRUBADDR_LO",
    ("memory_controller", 0x7C): "MC_SCRUBADDR_HI",
    ("memory_controller_ras", 0x48): "MC_SSRCONTROL",
    ("memory_controller_ras", 0x4C): "MC_SCRUB_CONTROL",
    ("memory_controller_ras", 0x50): "MC_RAS_ENABLES",
    ("memory_controller_ras", 0x54): "MC_RAS_STATUS",
    ("memory_controller_ras", 0x60): "MC_SSRSTATUS",
    ("memory_controller_test", 0x50): "MC_DIMM_CLK_RATIO_STATUS",
    ("memory_controller_test", 0x54): "MC_DIMM_CLK_RATIO",
    ("memory_controller_test", 0x5C): "MC_TEST_OBSERVED_5C",
    ("memory_controller_test", 0x6C): "MC_TEST_PH_CTR",
    ("memory_controller_test", 0x80): "MC_TEST_PH_PIS",
    ("memory_controller_test", 0xA8): "MC_TEST_PAT_GCTR",
    ("memory_controller_test", 0xAC): "MC_TEST_OBSERVED_AC",
    ("memory_controller_test", 0xB0): "MC_TEST_PAT_BA",
    ("memory_controller_test", 0xBC): "MC_TEST_PAT_IS",
    ("memory_controller_test", 0xC0): "MC_TEST_PAT_DCD",
    ("memory_controller_test", 0xC4): "MC_TEST_OBSERVED_C4",
    ("memory_controller_test", 0xC8): "MC_TEST_OBSERVED_C8",
    ("memory_controller_test", 0xCC): "MC_TEST_OBSERVED_CC",
    ("memory_controller_test", 0xF8): "MC_TEST_OBSERVED_F8",
    ("memory_controller_test", 0xFC): "MC_TEST_OBSERVED_FC",
}


INSTRUCTION_RE = re.compile(
    r"^\s*(?P<address>[0-9a-fA-F]+):\s+"
    r"(?:(?:[0-9a-fA-F]{2})\s+)+"
    r"(?P<mnemonic>\S+)(?:\s+(?P<operands>.*?))?\s*$"
)
CALL_TARGET_RE = re.compile(r"(?:^|\s)0x(?P<target>[0-9a-fA-F]+)(?:\s|$)")
IMMEDIATE_RE = re.compile(r"^-?0x[0-9a-fA-F]+$")


def _literal(operand: str) -> int | None:
    operand = operand.strip()
    if not IMMEDIATE_RE.fullmatch(operand):
        return None
    return int(operand, 16)


def _register_name(role: str | None, offset: int | None) -> str | None:
    if role is None or offset is None:
        return None
    fixed = FIXED_REGISTER_NAMES.get((role, offset))
    if fixed:
        return fixed
    if role == "system_address_decoder":
        if 0x80 <= offset <= 0x9C and offset % 4 == 0:
            return f"SAD_DRAM_RULE_{(offset - 0x80) // 4}"
        if 0xC0 <= offset <= 0xDC and offset % 4 == 0:
            return f"SAD_INTERLEAVE_LIST_{(offset - 0xC0) // 4}"
    if role == "target_address_decode":
        if 0x80 <= offset <= 0x9C and offset % 4 == 0:
            return f"TAD_DRAM_RULE_{(offset - 0x80) // 4}"
        if 0xC0 <= offset <= 0xDC and offset % 4 == 0:
            return f"TAD_INTERLEAVE_LIST_{(offset - 0xC0) // 4}"
    if role.endswith("_control"):
        names = {
            0x50: "MC_CHANNEL_DIMM_RESET_CMD",
            0x54: "MC_CHANNEL_DIMM_INIT_CMD",
            0x58: "MC_CHANNEL_DIMM_INIT_PARAMS",
            0x5C: "MC_CHANNEL_DIMM_INIT_STATUS",
            0x60: "MC_CHANNEL_DDR3CMD",
            0x68: "MC_CHANNEL_REFRESH_THROTTLE_SUPPORT",
            0x70: "MC_CHANNEL_MRS_VALUE_0_1",
            0x74: "MC_CHANNEL_MRS_VALUE_2",
            0x7C: "MC_CHANNEL_RANK_PRESENT",
            0x80: "MC_CHANNEL_RANK_TIMING_A",
            0x84: "MC_CHANNEL_RANK_TIMING_B",
            0x88: "MC_CHANNEL_BANK_TIMING",
            0x8C: "MC_CHANNEL_REFRESH_TIMING",
            0x90: "MC_CHANNEL_CKE_TIMING",
            0x94: "MC_CHANNEL_ZQ_TIMING",
            0x98: "MC_CHANNEL_RCOMP_PARAMS",
            0x9C: "MC_CHANNEL_ODT_PARAMS1",
            0xA0: "MC_CHANNEL_ODT_PARAMS2",
            0xA4: "MC_CHANNEL_ODT_MATRIX_RANK_0_3_RD",
            0xA8: "MC_CHANNEL_ODT_MATRIX_RANK_4_7_RD",
            0xAC: "MC_CHANNEL_ODT_MATRIX_RANK_0_3_WR",
            0xB0: "MC_CHANNEL_ODT_MATRIX_RANK_4_7_WR",
            0xB4: "MC_CHANNEL_WAQ_PARAMS",
            0xB8: "MC_CHANNEL_SCHEDULER_PARAMS",
            0xBC: "MC_CHANNEL_MAINTENANCE_OPS",
            0xC0: "MC_CHANNEL_TX_BG_SETTINGS",
            0xC8: "MC_CHANNEL_RX_BGF_SETTINGS",
            0xCC: "MC_CHANNEL_EW_BGF_SETTINGS",
            0xD0: "MC_CHANNEL_EW_BGF_OFFSET_SETTINGS",
            0xD4: "MC_CHANNEL_ROUND_TRIP_LATENCY",
            0xD8: "MC_CHANNEL_PAGETABLE_PARAMS1",
            0xE0: "MC_TX_BG_CMD_DATA_RATIO_SETTING",
            0xE4: "MC_TX_BG_CMD_OFFSET_SETTINGS",
            0xE8: "MC_TX_BG_DATA_OFFSET_SETTINGS",
            0xF0: "MC_CHANNEL_ADDR_MATCH",
            0xF8: "MC_CHANNEL_ECC_ERROR_MASK",
            0xFC: "MC_CHANNEL_ECC_ERROR_INJECT",
        }
        return names.get(offset)
    if role.endswith("_address"):
        if offset in (0x48, 0x4C, 0x50):
            return f"MC_DOD_CH_DIMM{(offset - 0x48) // 4}"
        if 0x80 <= offset <= 0x9C and offset % 4 == 0:
            return f"MC_SAG_CH_{(offset - 0x80) // 4}"
    if role.endswith("_rank"):
        if 0x40 <= offset <= 0x5C and offset % 4 == 0:
            return f"MC_RIR_LIMIT_CH_{(offset - 0x40) // 4}"
        if 0x80 <= offset <= 0xFC and offset % 4 == 0:
            return f"MC_RIR_WAY_CH_{(offset - 0x80) // 4}"
    return None


def pe_section_payload(payload: bytes, requested_name: bytes) -> bytes:
    """Return the meaningful bytes of a PE section without normalizing the PE."""
    if len(payload) < 0x40:
        raise ValueError("input is too short for a DOS header")
    pe_offset = struct.unpack_from("<I", payload, 0x3C)[0]
    if pe_offset + 24 > len(payload) or payload[pe_offset : pe_offset + 4] != b"PE\0\0":
        raise ValueError("input does not contain a valid PE header")
    section_count = struct.unpack_from("<H", payload, pe_offset + 6)[0]
    optional_size = struct.unpack_from("<H", payload, pe_offset + 20)[0]
    table_offset = pe_offset + 24 + optional_size
    for index in range(section_count):
        entry = table_offset + index * 40
        if entry + 40 > len(payload):
            raise ValueError("truncated PE section table")
        name = payload[entry : entry + 8].split(b"\0", 1)[0]
        if name != requested_name:
            continue
        virtual_size, raw_size, raw_offset = struct.unpack_from(
            "<I4xII", payload, entry + 8
        )
        meaningful_size = min(virtual_size, raw_size) if virtual_size else raw_size
        if raw_offset + meaningful_size > len(payload):
            raise ValueError(f"truncated {requested_name.decode(errors='replace')} section")
        return payload[raw_offset : raw_offset + meaningful_size]
    raise ValueError(f"PE section not found: {requested_name.decode(errors='replace')}")


def parse_disassembly(text: str) -> list[dict[str, Any]]:
    accesses: list[dict[str, Any]] = []
    pushes: list[tuple[int, str]] = []
    for line in text.splitlines():
        match = INSTRUCTION_RE.match(line)
        if not match:
            continue
        address = int(match.group("address"), 16)
        mnemonic = match.group("mnemonic")
        operands = (match.group("operands") or "").strip()
        if mnemonic == "push":
            pushes.append((address, operands))
            pushes = pushes[-12:]
            continue
        if mnemonic == "call":
            target_match = CALL_TARGET_RE.search(operands)
            target = int(target_match.group("target"), 16) if target_match else None
            wrapper = WRAPPERS.get(target)
            if wrapper is not None:
                count = len(wrapper.argument_names)
                selected = pushes[-count:]
                arguments: dict[str, dict[str, Any]] = {}
                if len(selected) == count:
                    for name, (push_address, operand) in zip(
                        wrapper.argument_names, selected, strict=True
                    ):
                        value = _literal(operand)
                        argument = {
                            "push_address": f"0x{push_address:08x}",
                            "operand": operand,
                            "literal": value is not None,
                        }
                        if value is not None:
                            argument["value"] = f"0x{value:x}"
                        arguments[name] = argument
                device = _literal(arguments.get("device", {}).get("operand", ""))
                function = _literal(arguments.get("function", {}).get("operand", ""))
                offset = _literal(arguments.get("offset", {}).get("operand", ""))
                role = BDF_ROLES.get((device, function))
                accesses.append(
                    {
                        "call_address": f"0x{address:08x}",
                        "helper_address": f"0x{target:08x}",
                        "operation": wrapper.operation,
                        "width_bytes": wrapper.width_bytes,
                        "arguments_complete": len(selected) == count,
                        "arguments": arguments,
                        "device": device,
                        "function": function,
                        "offset": f"0x{offset:x}" if offset is not None else None,
                        "role": role,
                        "register": _register_name(role, offset),
                        "decoded_uncore_target": role is not None,
                    }
                )
            pushes = []
            continue
        if mnemonic.startswith("j") or mnemonic in {"ret", "retf", "iret"}:
            pushes = []
    return accesses


def analyze(path: Path, objdump: str = "objdump") -> dict[str, Any]:
    payload = path.read_bytes()
    text_payload = pe_section_payload(payload, b".text")
    input_sha256 = hashlib.sha256(payload).hexdigest()
    text_sha256 = hashlib.sha256(text_payload).hexdigest()
    result = subprocess.run(
        [objdump, "-d", "-M", "intel", str(path)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or f"{objdump} exited {result.returncode}")
    accesses = parse_disassembly(result.stdout)
    by_operation = Counter(
        f"{item['operation']}{item['width_bytes'] * 8}" for item in accesses
    )
    by_role = Counter(
        item["role"] for item in accesses if item["decoded_uncore_target"]
    )
    return {
        "schema_version": 1,
        "profile": PROFILE,
        "input": str(path),
        "input_size": len(payload),
        "input_sha256": input_sha256,
        "text_size": len(text_payload),
        "text_sha256": text_sha256,
        "profile_hash_basis": ".text section SHA-256",
        "profile_hash_matches": text_sha256 == EXPECTED_TEXT_SHA256,
        "legacy_normalized_input_hash_matches": (
            input_sha256 == LEGACY_NORMALIZED_SHA256
        ),
        "method": "GNU objdump plus literal cdecl-argument recovery",
        "evidence_limit": (
            "Only direct literal device/function/offset arguments are decoded; "
            "dynamic operands are retained without inferred values."
        ),
        "summary": {
            "access_count": len(accesses),
            "decoded_uncore_count": sum(
                item["decoded_uncore_target"] for item in accesses
            ),
            "named_register_count": sum(item["register"] is not None for item in accesses),
            "by_operation": dict(sorted(by_operation.items())),
            "by_role": dict(sorted(by_role.items())),
        },
        "accesses": accesses,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path, help="locally extracted MINITDLL PE image")
    parser.add_argument(
        "--only-decoded-uncore",
        action="store_true",
        help="omit call sites whose device/function pair is not a literal known Uncore BDF",
    )
    parser.add_argument("--objdump", default="objdump", help="GNU objdump executable")
    args = parser.parse_args()
    try:
        report = analyze(args.image, args.objdump)
    except (OSError, RuntimeError) as error:
        parser.error(str(error))
    if args.only_decoded_uncore:
        report["accesses"] = [
            item for item in report["accesses"] if item["decoded_uncore_target"]
        ]
        report["output_filter"] = "decoded Uncore targets only"
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
