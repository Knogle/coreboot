#!/usr/bin/env python3
"""Read DDR3 SPD EEPROMs through Linux i2c-dev without EEPROM data writes.

Each transaction writes one byte containing only the EEPROM address pointer,
then reads data.  No transaction contains an EEPROM payload byte.
"""

from __future__ import annotations

import argparse
import errno
import ctypes
import fcntl
import hashlib
import json
import os
from pathlib import Path
import sys
from typing import Any


I2C_SLAVE = 0x0703
I2C_SMBUS = 0x0720
I2C_SMBUS_READ = 1
I2C_SMBUS_BYTE_DATA = 2
DEFAULT_ADDRESSES = tuple(range(0x50, 0x58))


class I2cSmbusData(ctypes.Union):
    _fields_ = (
        ("byte", ctypes.c_uint8),
        ("word", ctypes.c_uint16),
        ("block", ctypes.c_uint8 * 34),
    )


class I2cSmbusIoctlData(ctypes.Structure):
    _fields_ = (
        ("read_write", ctypes.c_uint8),
        ("command", ctypes.c_uint8),
        ("size", ctypes.c_uint32),
        ("data", ctypes.POINTER(I2cSmbusData)),
    )


LIBC = ctypes.CDLL(None, use_errno=True)


def smbus_read_byte_data(descriptor: int, command: int) -> int:
    data = I2cSmbusData()
    request = I2cSmbusIoctlData(
        read_write=I2C_SMBUS_READ,
        command=command,
        size=I2C_SMBUS_BYTE_DATA,
        data=ctypes.pointer(data),
    )
    result = LIBC.ioctl(descriptor, I2C_SMBUS, ctypes.byref(request))
    if result < 0:
        error_number = ctypes.get_errno()
        raise OSError(error_number, os.strerror(error_number))
    return data.byte


def read_eeprom(descriptor: int, address: int, length: int = 256) -> bytes:
    if not 0 <= address <= 0x7F:
        raise ValueError("I2C address out of range")
    if not 1 <= length <= 256:
        raise ValueError("SPD length must be between 1 and 256 bytes")
    fcntl.ioctl(descriptor, I2C_SLAVE, address)
    return bytes(smbus_read_byte_data(descriptor, offset) for offset in range(length))


def basic_ddr3_fields(payload: bytes) -> dict[str, Any]:
    if len(payload) < 146:
        return {"decode_error": "SPD shorter than 146 bytes"}
    memory_types = {0x0B: "DDR3 SDRAM"}
    module_types = {
        0x01: "RDIMM",
        0x02: "UDIMM",
        0x03: "SO-DIMM",
        0x04: "Micro-DIMM",
        0x05: "Mini-RDIMM",
        0x06: "Mini-UDIMM",
    }
    organization = payload[7]
    bus_width = payload[8]
    density_mbit = {
        0: 256,
        1: 512,
        2: 1024,
        3: 2048,
        4: 4096,
        5: 8192,
        6: 16384,
    }.get(payload[4] & 0x0F)
    bank_count = {0: 8, 1: 16}.get((payload[4] >> 4) & 0x07)
    rank_count = ((organization >> 3) & 0x07) + 1
    device_width = 4 << (organization & 0x07)
    primary_width = 8 << (bus_width & 0x07)
    module_capacity_mib = None
    if density_mbit is not None:
        module_capacity_mib = (
            (density_mbit // 8) * (primary_width // device_width) * rank_count
        )
    medium_timebase_ns = None
    if payload[11]:
        medium_timebase_ns = payload[10] / payload[11]

    def mtb(byte: int) -> float | None:
        if medium_timebase_ns is None:
            return None
        return round(byte * medium_timebase_ns, 4)

    cas_bitmap = payload[14] | (payload[15] << 8)
    cas_latencies = [4 + bit for bit in range(16) if cas_bitmap & (1 << bit)]
    crc_coverage = 117 if payload[0] & 0x80 else 126
    crc_calculated = spd_crc16(payload[:crc_coverage])
    crc_stored = payload[126] | (payload[127] << 8)
    part_number = payload[128:146].decode("ascii", errors="replace").strip(" \0\xff")
    return {
        "memory_type_code": f"0x{payload[2]:02x}",
        "memory_type": memory_types.get(payload[2], "unknown"),
        "module_type_code": f"0x{payload[3] & 0x0f:02x}",
        "module_type": module_types.get(payload[3] & 0x0F, "unknown"),
        "sdram_density_mbit": density_mbit,
        "bank_count": bank_count,
        "row_address_bits": 12 + ((payload[5] >> 3) & 0x07),
        "column_address_bits": 9 + (payload[5] & 0x07),
        "rank_count": rank_count,
        "device_width_bits": device_width,
        "primary_bus_width_bits": primary_width,
        "bus_width_extension_bits": 0 if ((bus_width >> 3) & 0x03) == 0 else 8,
        "module_capacity_mib": module_capacity_mib,
        "medium_timebase_ns": medium_timebase_ns,
        "tck_min_ns": mtb(payload[12]),
        "maximum_transfer_rate_mt_s": (
            round(2000 / mtb(payload[12])) if mtb(payload[12]) else None
        ),
        "supported_cas_latencies": cas_latencies,
        "taa_min_ns": mtb(payload[16]),
        "twr_min_ns": mtb(payload[17]),
        "trcd_min_ns": mtb(payload[18]),
        "trrd_min_ns": mtb(payload[19]),
        "trp_min_ns": mtb(payload[20]),
        "tras_min_ns": mtb(((payload[21] & 0x0F) << 8) | payload[22]),
        "trc_min_ns": mtb(((payload[21] & 0xF0) << 4) | payload[23]),
        "trfc_min_ns": mtb(payload[24] | (payload[25] << 8)),
        "twtr_min_ns": mtb(payload[26]),
        "trtp_min_ns": mtb(payload[27]),
        "tfaw_min_ns": mtb(((payload[28] & 0x0F) << 8) | payload[29]),
        "crc_coverage_bytes": crc_coverage,
        "crc_stored": f"0x{crc_stored:04x}",
        "crc_calculated": f"0x{crc_calculated:04x}",
        "crc_valid": crc_stored == crc_calculated,
        "part_number": part_number,
    }


def spd_crc16(payload: bytes) -> int:
    crc = 0
    for value in payload:
        crc ^= value << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def capture(bus: Path, addresses: tuple[int, ...]) -> dict[str, Any]:
    descriptor = os.open(bus, os.O_RDWR)
    devices = []
    try:
        for address in addresses:
            record: dict[str, Any] = {"address": f"0x{address:02x}"}
            try:
                payload = read_eeprom(descriptor, address)
            except OSError as error:
                record["present"] = False
                record["error"] = f"{type(error).__name__}: {error}"
                if error.errno not in (None, errno.ENXIO, errno.EREMOTEIO, errno.EIO):
                    record["unexpected_error"] = True
            else:
                record.update(
                    {
                        "present": True,
                        "size": len(payload),
                        "sha256": hashlib.sha256(payload).hexdigest(),
                        "hex": payload.hex(),
                        "decoded": basic_ddr3_fields(payload),
                    }
                )
            devices.append(record)
    finally:
        os.close(descriptor)
    return {
        "schema_version": 1,
        "bus": str(bus),
        "access_model": (
            "i2c-dev SMBus read-byte-data transactions only; command byte is "
            "the SPD offset; no EEPROM data writes"
        ),
        "devices": devices,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bus", type=Path, default=Path("/dev/i2c-0"))
    parser.add_argument(
        "--addresses",
        nargs="*",
        type=lambda value: int(value, 0),
        default=DEFAULT_ADDRESSES,
    )
    arguments = parser.parse_args()
    try:
        report = capture(arguments.bus, tuple(arguments.addresses))
    except (OSError, ValueError) as error:
        parser.error(str(error))
    json.dump(report, sys.stdout, indent=2, sort_keys=True)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
