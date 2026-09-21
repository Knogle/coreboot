#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Extract the private firmware inputs required by the MSI X58 Pro-E port.

The user must supply either the official MSI 7522v8F.zip archive or its
A7522IMS.8F0 member. The script verifies the complete input before extracting
anything. It neither downloads nor contains proprietary firmware.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import os
from pathlib import Path
import stat
import zipfile


ROOT = Path(__file__).resolve().parents[3]
ROM_SIZE = 0x400000
ROM_SHA256 = "ee7912da81a3cf47b6bedef7dedfa56cbc31723f5cdd7f0d75a7f64e336c04a8"
ARCHIVE_SHA256 = "83b7cc523ed79ab1b86b810261d33f7bb3a27575b7479b9fbe26ad2a037adf7b"
ARCHIVE_MEMBER = "7522v8F/A7522IMS.8F0"


@dataclass(frozen=True)
class Region:
	filename: str
	offset: int
	size: int
	source_sha256: str
	output_sha256: str


REGIONS = (
	Region(
		"csi-wrapper.bin", 0x3C04E2, 0x837,
		"546f0c4d455ce250b3fc6047d3864b25d6fd16e7cf1c3bfaa6b61d69510af6c3",
		"d38c093271f18cfa5f399421654fcab2e61fa07efca85323904dcb74b89e0fa9",
	),
	Region(
		"csi-helper.bin", 0x3C1554, 0x25,
		"6641f8cc9bfb9b08df0f39b688cbeeab93a0c92bff0d47ee565e525dd3179d75",
		"6641f8cc9bfb9b08df0f39b688cbeeab93a0c92bff0d47ee565e525dd3179d75",
	),
	Region(
		"minit.bin", 0x3C1DC0, 0x18700,
		"54fb7d14c1d88c0a42ea0e7b51511735983686b79fdf9db64e5ace2fc651264a",
		"54fb7d14c1d88c0a42ea0e7b51511735983686b79fdf9db64e5ace2fc651264a",
	),
	Region(
		"csi.bin", 0x3E6DE0, 0x75E0,
		"e7c42f1a3474fc007c6d0731dc94f753f1fb0367d1ddbf8a3437edeb1daeb150",
		"e7c42f1a3474fc007c6d0731dc94f753f1fb0367d1ddbf8a3437edeb1daeb150",
	),
)

# Offsets are relative to csi-wrapper.bin. Each tuple contains the expected
# source bytes followed by their replacement.
WRAPPER_PATCHES = (
	(0x034, bytes.fromhex("00"), bytes.fromhex("01")),
	(0x096, bytes.fromhex("01"), bytes.fromhex("00")),
	(0x189, bytes.fromhex("01"), bytes.fromhex("00")),
	(0x35D, bytes.fromhex("01"), bytes.fromhex("00")),
	(0x52A, bytes.fromhex("01"), bytes.fromhex("00")),
	(0x6E0, bytes.fromhex("7409"), bytes.fromhex("9090")),
)


def sha256(data: bytes) -> str:
	return hashlib.sha256(data).hexdigest()


def read_vendor_rom(path: Path) -> bytes:
	data = path.read_bytes()
	if zipfile.is_zipfile(path):
		if sha256(data) != ARCHIVE_SHA256:
			raise ValueError("MSI archive SHA-256 mismatch")
		with zipfile.ZipFile(path) as archive:
			matches = [entry for entry in archive.infolist()
				   if entry.filename == ARCHIVE_MEMBER]
			if len(matches) != 1 or matches[0].is_dir():
				raise ValueError(f"archive must contain {ARCHIVE_MEMBER}")
			if matches[0].flag_bits & 1:
				raise ValueError("firmware member must not be encrypted")
			data = archive.read(matches[0])

	if len(data) != ROM_SIZE:
		raise ValueError(
			f"firmware size is 0x{len(data):x}, expected 0x{ROM_SIZE:x}"
		)
	if sha256(data) != ROM_SHA256:
		raise ValueError("MSI firmware SHA-256 mismatch")
	return data


def patch_wrapper(data: bytes) -> bytes:
	output = bytearray(data)
	for offset, source, replacement in WRAPPER_PATCHES:
		if len(source) != len(replacement):
			raise ValueError("internal error: invalid wrapper patch")
		actual = bytes(output[offset:offset + len(source)])
		if actual != source:
			raise ValueError(
				f"wrapper bytes at 0x{offset:x} do not match the expected input"
			)
		output[offset:offset + len(source)] = replacement
	return bytes(output)


def extract_regions(rom: bytes) -> dict[str, bytes]:
	outputs = {}
	for region in REGIONS:
		data = rom[region.offset:region.offset + region.size]
		if len(data) != region.size or sha256(data) != region.source_sha256:
			raise ValueError(f"{region.filename}: source region mismatch")
		if region.filename == "csi-wrapper.bin":
			data = patch_wrapper(data)
		if sha256(data) != region.output_sha256:
			raise ValueError(f"{region.filename}: output verification failed")
		outputs[region.filename] = data
	return outputs


def check_output_directory(path: Path) -> Path:
	if path.is_symlink():
		raise ValueError("output directory must not be a symlink")
	resolved = path.resolve()
	try:
		relative = resolved.relative_to(ROOT)
	except ValueError:
		return resolved
	if not relative.parts or relative.parts[0] != "site-local":
		raise ValueError("output inside the source tree must be below site-local")
	return resolved


def write_exact(path: Path, data: bytes, force: bool) -> str:
	flags = os.O_RDWR | os.O_NOFOLLOW | os.O_NONBLOCK
	try:
		descriptor = os.open(path, flags)
	except FileNotFoundError:
		descriptor = os.open(path, flags | os.O_CREAT | os.O_EXCL, 0o600)
		with os.fdopen(descriptor, "w+b") as output:
			output.write(data)
		return "written"

	with os.fdopen(descriptor, "r+b") as output:
		if not stat.S_ISREG(os.fstat(output.fileno()).st_mode):
			raise ValueError(f"not a regular file: {path}")
		existing = output.read(len(data) + 1)
		if existing == data:
			return "verified"
		if not force:
			raise ValueError(f"existing file differs: {path}")
		output.seek(0)
		output.truncate()
		output.write(data)
	return "written"


def verify_directory(path: Path) -> None:
	for region in REGIONS:
		data = (path / region.filename).read_bytes()
		if len(data) != region.size or sha256(data) != region.output_sha256:
			raise ValueError(f"{region.filename}: size or SHA-256 mismatch")


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	mode = parser.add_mutually_exclusive_group(required=True)
	mode.add_argument("--vendor-source", type=Path)
	mode.add_argument("--verify-dir", type=Path)
	parser.add_argument("--output-dir", type=Path)
	parser.add_argument("--force", action="store_true")
	args = parser.parse_args()

	try:
		if args.verify_dir:
			if args.output_dir:
				raise ValueError("--output-dir cannot be used with --verify-dir")
			verify_directory(args.verify_dir)
			print(f"MSI X58 Pro-E firmware verification: PASS ({len(REGIONS)} files)")
			return 0

		if not args.output_dir:
			raise ValueError("--output-dir is required with --vendor-source")
		output_dir = check_output_directory(args.output_dir)
		output_dir.mkdir(parents=True, mode=0o700, exist_ok=True)
		outputs = extract_regions(read_vendor_rom(args.vendor_source))
		for name, data in outputs.items():
			status = write_exact(output_dir / name, data, args.force)
			print(f"{status}: {output_dir / name}")
		verify_directory(output_dir)
	except (OSError, ValueError, zipfile.BadZipFile) as error:
		parser.error(str(error))

	print("MSI X58 Pro-E firmware extraction: PASS")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
