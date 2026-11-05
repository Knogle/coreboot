#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Inspect the built low-workspace ELF/SELF/CBFS contracts, without hardware."""

import argparse
import hashlib
import json
import lzma
from pathlib import Path
import struct
import subprocess


BASE, END = 0x02000000, 0x02800000


def symbols(path):
    output = subprocess.check_output(["nm", "-n", "-S", str(path)], text=True)
    result = {}
    for line in output.splitlines():
        fields = line.split()
        if len(fields) in (3, 4):
            result[fields[-1]] = dict(address=int(fields[0], 16),
                                      bytes=int(fields[1], 16) if len(fields) == 4 else 0)
    return result


def elf_read(path, address, size):
    data = path.read_bytes()
    assert data[:6] == b"\x7fELF\x01\x01"
    offset = struct.unpack_from("<I", data, 28)[0]
    stride, count = struct.unpack_from("<HH", data, 42)
    for i in range(count):
        kind, start, virtual, _, stored, _, _, _ = struct.unpack_from("<8I", data, offset + i * stride)
        if kind == 1 and virtual <= address and address + size <= virtual + stored:
            return data[start + address - virtual:start + address - virtual + size]
    raise ValueError((str(path), hex(address), size, "not file backed"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--obj", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--old-bridge", type=Path, help="Optional original bridge for historical comparisons")
    parser.add_argument("--seabios", action="store_true")
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    board = args.obj / "mainboard/asrockrack/b565d4_v1l"
    bridge, rom, ram = board / "bridge.elf", args.obj / "cbfs/fallback/romstage.debug", args.obj / "cbfs/fallback/ramstage.debug"
    bridge_symbols, rom_symbols, ram_symbols = [symbols(p) for p in (bridge, rom, ram)]
    assert bridge_symbols["native84_bridge_enter"]["address"] == BASE + 0x2000
    assert bridge_symbols["native84_bridge_stage_08"]["address"] == BASE + 0x202e
    assert bridge_symbols["native84_owned_fault"]["address"] == BASE + 0x2049
    assert bridge_symbols["_native84_bridge_end"]["address"] <= BASE + 0x4000
    bridge_bytes = (board / "bridge.bin").read_bytes()
    if args.old_bridge:
        assert bridge_bytes == args.old_bridge.read_bytes()
    else:
        assert hashlib.sha256(bridge_bytes).hexdigest() == \
            "9ccc1922aaec9760cf42f4d257d6a67fd7869d0ce64b231f83218b61f7cda227"
    assert rom_symbols["_program"]["address"] == 0x09d00000
    assert rom_symbols["_eprogram"]["address"] <= 0x09da0000
    cold = rom_symbols["cold_data"]
    assert cold["bytes"] == 16 + 0x28000
    assert rom_symbols["_bss"]["address"] <= cold["address"]
    assert cold["address"] + cold["bytes"] <= rom_symbols["_ebss"]["address"]
    assert ram_symbols["_early_reserved_dram"]["address"] == BASE
    assert ram_symbols["_eearly_reserved_dram"]["address"] == BASE + 0x150000
    assert ram_symbols["_earlyram_stack"]["address"] == BASE + 0xc000
    assert ram_symbols["_program"]["address"] == 0x20000000
    expected_policy = [1, 0x00840001, 2, 0x10011, 0, 0x600, 1000000, BASE + 0x2049]
    for path, sym in [(rom, rom_symbols), (ram, ram_symbols)]:
        policy = list(struct.unpack("<8I", elf_read(path, sym["native84_profile_policy"]["address"], 32)))
        assert policy == expected_policy, (str(path), policy)
    partitions = [(BASE, BASE + 0x38), (BASE + 0x40, BASE + 0x60),
                  (BASE + 0x100, BASE + 0x100 + 4328),
                  (BASE + 0x2000, BASE + 0x4000),
                  (BASE + 0x8000, BASE + 0xc000), (BASE + 0xc000, BASE + 0x10000),
                  (BASE + 0x10000, BASE + 0x40000),
                  (BASE + 0xd0000, BASE + 0x150000), (BASE + 0x150000, END)]
    for i, (begin, end) in enumerate(partitions):
        assert BASE <= begin < end <= END
        assert all(end <= a or b <= begin for a, b in partitions[:i])
    assert BASE + 0x10000 + 64 + 0x28000 <= BASE + 0x40000
    payload = (board / "payload.self").read_bytes()
    decoded_payload = bytearray()
    segments, offset = [], 0
    while True:
        kind, compression, stored_offset, address, stored, memory = struct.unpack_from(">IIIQII", payload, offset)
        offset += 28
        if kind == 0x454e5452:
            break
        assert memory and address + memory <= 0x100000000
        assert address + memory <= BASE or END <= address
        assert address + memory <= 0x09000000 or 0x0b100000 <= address
        if kind != 0x42535320:
            data = payload[stored_offset:stored_offset + stored]
            decoded_payload.extend(lzma.decompress(data, format=lzma.FORMAT_ALONE) if compression else data)
        segments.append(dict(base=hex(address), bytes=memory))
    medium = (board / "medium.bin").read_bytes()
    relative_pointer = struct.unpack_from("<i", medium, len(medium) - 4)[0]
    header_offset = len(medium) + relative_pointer
    assert 0 <= header_offset <= len(medium) - 32 and medium[header_offset:header_offset + 4] == b"ORBC"
    cbfs_location = BASE + 0x150000
    if args.seabios:
        assert struct.pack("<I", cbfs_location - 4) in decoded_payload
        assert struct.pack("<I", 0xba750000 - 4) not in decoded_payload
    paths = [bridge, rom, ram, board / "payload.self", board / "medium.bin", args.obj / "coreboot.rom"]
    report = dict(status="PASS_HOST_LOW_WORKSPACE_CONTRACTS", physical_execution=False,
                  scope="Built ELF/SELF/CBFS layout only; no DIMM training, boot or ECC proof",
                  workspace=[hex(BASE), hex(END)], partitions=[[hex(a), hex(b)] for a, b in partitions],
                  cold_snapshot=cold, policy=expected_policy, bridge_position_independent_bytes_unchanged=True,
                  payload_segments=segments, cbfs_location=hex(cbfs_location),
                  cbfs_header=hex(cbfs_location + relative_pointer),
                  seabios_compiled_pointer_checked=args.seabios,
                  inputs={str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths})
    (args.out / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({key: report[key] for key in ["status", "workspace", "cbfs_location", "cold_snapshot"]}))


if __name__ == "__main__":
    main()
