#!/usr/bin/env python3
"""Read the MSI X58 early-policy CMOS inputs through /dev/port.

Only the two CMOS selector ports are written.  Their original values are
restored in a finally block; neither CMOS data port is ever written.  The
ICH10 upper-bank enable is checked read-only before any extended access.
"""

from __future__ import annotations

import os
import struct


ICH10_LPC_CONFIG = "/sys/bus/pci/devices/0000:00:1f.0/config"
ICH10_RCBA_REGISTER = 0xF0
ICH10_RC_OFFSET = 0x3400
ICH10_RC_U128E = 1 << 2


STANDARD_ADDRESSES = (
	0x0E,
	0x57,
	0x58,
	0x5E,
	0x5F,
	0x69,
	0x70,
	0x71,
	0x72,
	0x73,
	0x74,
	0x75,
)
EXTENDED_ADDRESSES = (
	0x07,
	0x0A,
	0x0B,
	0x0C,
	0x0D,
	0x11,
	0x12,
	0x1E,
	0x28,
	0x44,
	0x80,
	0x81,
	0x82,
	0x88,
	0x89,
	0x8E,
	0xA8,
	0xC6,
	0xCA,
	0xDB,
	0xDD,
	0xF1,
	0xF5,
	0xF6,
	0xF7,
	0xF8,
	0xF9,
)


def read_port(fd: int, port: int) -> int:
	value = os.pread(fd, 1, port)
	if len(value) != 1:
		raise OSError(f"short /dev/port read at 0x{port:x}")
	return value[0]


def write_port(fd: int, port: int, value: int) -> None:
	if os.pwrite(fd, bytes((value & 0xFF,)), port) != 1:
		raise OSError(f"short /dev/port write at 0x{port:x}")


def read_ich10_rtc_configuration() -> tuple[int, int]:
	with open(ICH10_LPC_CONFIG, "rb", buffering=0) as config:
		config.seek(ICH10_RCBA_REGISTER)
		raw_rcba = config.read(4)
	if len(raw_rcba) != 4:
		raise OSError("short ICH10 RCBA PCI-config read")
	rcba = struct.unpack("<I", raw_rcba)[0]
	if not rcba & 1:
		raise RuntimeError(f"ICH10 RCBA decode is disabled: 0x{rcba:08x}")
	rc_address = (rcba & 0xFFFFC000) + ICH10_RC_OFFSET
	mem_fd = os.open("/dev/mem", os.O_RDONLY | os.O_CLOEXEC)
	try:
		raw_rc = os.pread(mem_fd, 4, rc_address)
	finally:
		os.close(mem_fd)
	if len(raw_rc) != 4:
		raise OSError(f"short /dev/mem read at 0x{rc_address:x}")
	return rcba, struct.unpack("<I", raw_rc)[0]


def main() -> None:
	rcba, rtc_configuration = read_ich10_rtc_configuration()
	if not rtc_configuration & ICH10_RC_U128E:
		raise SystemExit(
			f"REFUSED: RCBA=0x{rcba:08x} RC=0x{rtc_configuration:08x}; "
			"upper-128 RTC decode is disabled and 0x72/0x73 would alias 0x70/0x71"
		)
	fd = os.open("/dev/port", os.O_RDWR | os.O_CLOEXEC)
	try:
		saved_index_70 = read_port(fd, 0x70)
		saved_index_72 = read_port(fd, 0x72)
		try:
			alt_gp_smi_en_low = read_port(fd, 0x538)
			standard: dict[int, int] = {}
			for address in STANDARD_ADDRESSES:
				write_port(fd, 0x70, address | 0x80)
				read_port(fd, 0x61)
				standard[address] = read_port(fd, 0x71)

			extended: dict[int, int] = {}
			for address in EXTENDED_ADDRESSES:
				write_port(fd, 0x72, address & 0x7F)
				extended[address] = read_port(fd, 0x73)
		finally:
			write_port(fd, 0x72, saved_index_72)
			write_port(fd, 0x70, saved_index_70)
	finally:
		os.close(fd)

	print(
		f"RCBA={rcba:08x} RTC_RC={rtc_configuration:08x} U128E=1 "
		f"INDEX70={saved_index_70:02x} INDEX72={saved_index_72:02x} "
		f"ALT_GP_SMI_EN_LOW={alt_gp_smi_en_low:02x} "
		f"FIELD_6_4={(alt_gp_smi_en_low >> 4) & 7}"
	)
	for address in STANDARD_ADDRESSES:
		label = "DIAG_0E" if address == 0x0E else f"STD_{address:02X}"
		print(f"CMOS_{label}={standard[address]:02x}")
	for address in EXTENDED_ADDRESSES:
		print(f"CMOS_EXT_{address:02X}={extended[address]:02x}")


if __name__ == "__main__":
	main()
