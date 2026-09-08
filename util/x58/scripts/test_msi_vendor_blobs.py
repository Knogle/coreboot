#!/usr/bin/env python3
"""Regression tests for scripts/msi_vendor_blobs.py.

The pinned-ROM integration test uses ``MSI_X58_VENDOR_ROM`` when set, then
tries the repository's ignored local corpus path.  It is skipped when the
user-supplied proprietary input is absent.
"""

from __future__ import annotations

import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest

import msi_vendor_blobs as blobs


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_VENDOR_ROM = (
	PROJECT_ROOT
	/ "blobs-local"
	/ "msi-x58-pro-e"
	/ "7522v8F"
	/ "A7522IMS.8F0"
)


def vendor_rom_path() -> Path:
	configured = os.environ.get("MSI_X58_VENDOR_ROM")
	return Path(configured) if configured else DEFAULT_VENDOR_ROM


class PureFunctionTests(unittest.TestCase):
	def test_rva_mapping_accounts_for_raw_pointer(self) -> None:
		sections = [
			{
				"name": ".text",
				"virtual_address": 0x220,
				"virtual_size": 0x80,
				"raw_pointer": 0x200,
				"raw_size": 0x80,
			}
		]
		self.assertEqual(blobs.rva_to_file_offset(0x240, 0x200, sections), 0x220)

	def test_exact_write_is_idempotent_and_fail_closed(self) -> None:
		with tempfile.TemporaryDirectory() as directory:
			path = Path(directory) / "output.bin"
			self.assertEqual(blobs.write_exact(path, b"first", False), "written")
			self.assertEqual(
				blobs.write_exact(path, b"first", False), "already-verified"
			)
			with self.assertRaises(blobs.VerificationError):
				blobs.write_exact(path, b"different", False)
			self.assertEqual(
				blobs.write_exact(path, b"different", True), "written"
			)

	def test_static_region_layout_has_no_overlap(self) -> None:
		blobs.validate_module_layout()

	def test_runtime_pe_header_offsets_are_pinned(self) -> None:
		self.assertEqual(
			{spec.name: spec.pe_offset for spec in blobs.MODULES},
			{"MINITDLL": 0xC0, "CSI_INITDLL": 0xB0},
		)

	def test_fnv1a32_reference_vector(self) -> None:
		self.assertEqual(blobs.fnv1a32(b""), 0x811C9DC5)
		self.assertEqual(blobs.fnv1a32(b"foobar"), 0xBF9CF968)


@unittest.skipUnless(vendor_rom_path().is_file(), "pinned local MSI ROM absent")
class PinnedVendorIntegrationTests(unittest.TestCase):
	@classmethod
	def setUpClass(cls) -> None:
		(
			cls.vendor,
			cls.modules,
			cls.support,
			cls.audit,
		) = blobs.load_vendor_rom(vendor_rom_path())

	def test_full_rom_and_raw_loaded_hashes(self) -> None:
		self.assertEqual(blobs.sha256(self.vendor), blobs.ROM_SHA256)
		self.assertEqual(len(self.modules), 2)
		for module in self.modules:
			self.assertEqual(module["raw_payload"], module["loaded_payload"])
			self.assertEqual(
				blobs.sha256(module["raw_payload"]),
				module["spec"].raw_sha256,
			)
			self.assertTrue(module["pe"]["direct_xip_layout"])
			self.assertEqual(
				blobs.fnv1a32(module["loaded_payload"]),
				module["spec"].fnv1a32,
			)

	def test_csi_uses_authoritative_raw_slice(self) -> None:
		csi = next(
			module for module in self.modules if module["spec"].name == "CSI_INITDLL"
		)
		self.assertEqual(len(csi["raw_payload"]), 0x75E0)
		self.assertEqual(csi["pe"]["pe_header_offset"], "0x000000b0")
		self.assertEqual(csi["pe"]["size_of_headers"], "0x00000220")
		self.assertEqual(csi["pe"]["entry_file_offset"], "0x00000220")
		self.assertNotEqual(
			blobs.sha256(csi["raw_payload"]),
			"b1d14415fcdd7f0a7cf15d0103a06cf9f3f731238f7a0d221a7e48e956215a8b",
		)

	def test_wrapper_transitive_calls_are_closed(self) -> None:
		calls = self.audit["audited_calls"]
		self.assertEqual(
			[(call["call_site"], call["target"]) for call in calls],
			[
				("0xfffc0bf7", "0xfffc1554"),
				("0xfffc0cfd", "0xfffe7000"),
				("0xfffc1554", "0xfffc1566"),
			],
		)

	def test_composite_verifier_detects_one_byte_change(self) -> None:
		regions = blobs.composition_regions(self.modules, self.support, True)
		image = bytearray(b"\xff" * blobs.ROM_SIZE)
		for region in regions:
			start = region["offset"]
			image[start : start + region["size"]] = region["payload"]
		blobs.verify_composite_bytes(bytes(image), regions)
		image[regions[0]["offset"]] ^= 0x01
		with self.assertRaises(blobs.VerificationError):
			blobs.verify_composite_bytes(bytes(image), regions)

	def test_compose_refuses_occupied_target_range(self) -> None:
		with tempfile.TemporaryDirectory() as directory:
			base = bytearray(b"\xff" * blobs.ROM_SIZE)
			base[self.modules[0]["spec"].offset] = 0x00
			base_path = Path(directory) / "base.rom"
			output_path = Path(directory) / "output.rom"
			base_path.write_bytes(base)
			args = SimpleNamespace(
				vendor_rom=vendor_rom_path(),
				coreboot_rom=base_path,
				output_rom=output_path,
				include_csi_wrapper_support=False,
				force=False,
			)
			with self.assertRaises(blobs.VerificationError):
				blobs.command_compose(args)
			self.assertFalse(output_path.exists())


if __name__ == "__main__":
	unittest.main()
