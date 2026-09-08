#!/usr/bin/env python3
"""Verify, extract, and compose the pinned MSI X58 vendor-init modules.

This tool operates only on a user-supplied ``A7522IMS.8F0``.  It never
downloads firmware and the repository must not contain either that image or
the extracted proprietary modules.  The three operations are deliberately
separate:

``extract``
    Verify the complete vendor ROM, extract CSI_INITDLL and MINITDLL into an
    explicitly selected local directory, and write a deterministic manifest.

``compose``
    Verify the vendor ROM and a 4 MiB coreboot image, require every selected
    module/support range to be erased (or already byte-identical), then write
    a new image.  Bytes outside selected ranges must remain unchanged.

``verify-composite``
    Verify that a composed 4 MiB image contains every selected exact range.

The PE images placed in the composed ROM are deterministic *loaded-memory*
images: headers are copied through SizeOfHeaders, sections are copied from
PointerToRawData to VirtualAddress, and the remaining SizeOfImage bytes are
zero-filled.  For the exact pinned full-ROM both modules prove byte-identical
between raw and loaded layouts.  An older locally generated CSI file with hash
``b1d144...`` is not the raw full-ROM slice and is intentionally rejected.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import struct
import sys
from typing import Any


ROM_SIZE = 0x400000
ROM_SHA256 = "ee7912da81a3cf47b6bedef7dedfa56cbc31723f5cdd7f0d75a7f64e336c04a8"
MANIFEST_NAME = "msi-x58-vendor-init.json"


class VerificationError(RuntimeError):
	"""The input is not the exact artifact or layout expected by this tool."""


@dataclass(frozen=True)
class ModuleSpec:
	name: str
	filename: str
	offset: int
	size: int
	raw_sha256: str
	loaded_sha256: str
	fnv1a32: int
	loaded_filename: str
	image_base: int
	entry_rva: int
	entry_address: int
	entry_file_offset: int
	entry_prefix_size: int
	entry_prefix_sha256: str
	pe_offset: int
	coff_characteristics: int
	size_of_headers: int
	relocation_rva: int
	relocation_size: int
	direct_xip_layout: bool


@dataclass(frozen=True)
class SupportSpec:
	name: str
	filename: str
	offset: int
	size: int
	sha256: str
	fnv1a32: int
	virtual_address: int


MODULES = (
	ModuleSpec(
		name="MINITDLL",
		filename="MINITDLL-region.bin",
		offset=0x3C1DC0,
		size=0x18700,
		raw_sha256="54fb7d14c1d88c0a42ea0e7b51511735983686b79fdf9db64e5ace2fc651264a",
		loaded_sha256="54fb7d14c1d88c0a42ea0e7b51511735983686b79fdf9db64e5ace2fc651264a",
		fnv1a32=0x6465D8F5,
		loaded_filename="MINITDLL-loaded.bin",
		image_base=0xFFFC1DC0,
		entry_rva=0x240,
		entry_address=0xFFFC2000,
		entry_file_offset=0x240,
		entry_prefix_size=12,
		entry_prefix_sha256="86c1b207a5c9634e18771e211de77cb0793f899f475136dc62e3363c3bcd2a71",
		pe_offset=0xC0,
		coff_characteristics=0x210E,
		size_of_headers=0x240,
		relocation_rva=0x185A0,
		relocation_size=0x98,
		direct_xip_layout=True,
	),
	ModuleSpec(
		name="CSI_INITDLL",
		filename="CSI_INITDLL-region.bin",
		offset=0x3E6DE0,
		size=0x75E0,
		raw_sha256="e7c42f1a3474fc007c6d0731dc94f753f1fb0367d1ddbf8a3437edeb1daeb150",
		loaded_sha256="e7c42f1a3474fc007c6d0731dc94f753f1fb0367d1ddbf8a3437edeb1daeb150",
		fnv1a32=0x77990DB9,
		loaded_filename="CSI_INITDLL-loaded.bin",
		image_base=0xFFFE6DE0,
		entry_rva=0x220,
		entry_address=0xFFFE7000,
		entry_file_offset=0x220,
		entry_prefix_size=12,
		entry_prefix_sha256="830b6580ca7775ab85c9050f64aaf3bf6b47a8f5218bfad6d8d6bbf35b3b376f",
		pe_offset=0xB0,
		coff_characteristics=0x210E,
		size_of_headers=0x220,
		relocation_rva=0x7560,
		relocation_size=0x2C,
		direct_xip_layout=True,
	),
)


SUPPORT_SLICES = (
	SupportSpec(
		name="CSI_WRAPPER",
		filename="CSI-wrapper-fffc04e2.bin",
		offset=0x3C04E2,
		size=0x837,
		sha256="546f0c4d455ce250b3fc6047d3864b25d6fd16e7cf1c3bfaa6b61d69510af6c3",
		fnv1a32=0x65B20E50,
		virtual_address=0xFFFC04E2,
	),
	SupportSpec(
		name="CSI_CMOS_READ_FIELD_HELPER",
		filename="CSI-helper-fffc1554.bin",
		offset=0x3C1554,
		size=0x25,
		sha256="6641f8cc9bfb9b08df0f39b688cbeeab93a0c92bff0d47ee565e525dd3179d75",
		fnv1a32=0x99AEB979,
		virtual_address=0xFFFC1554,
	),
)


def sha256(payload: bytes) -> str:
	return hashlib.sha256(payload).hexdigest()


def fnv1a32(payload: bytes) -> int:
	digest = 2166136261
	for value in payload:
		digest = ((digest ^ value) * 16777619) & 0xFFFFFFFF
	return digest


def hex32(value: int) -> str:
	return f"0x{value:08x}"


def require_range(payload: bytes, offset: int, size: int, description: str) -> None:
	if offset < 0 or size < 0 or offset + size > len(payload):
		raise VerificationError(
			f"{description} range {hex32(offset)}+{hex32(size)} exceeds "
			f"the {hex32(len(payload))}-byte input"
		)


def unpack_u16(payload: bytes, offset: int, description: str) -> int:
	require_range(payload, offset, 2, description)
	return struct.unpack_from("<H", payload, offset)[0]


def unpack_u32(payload: bytes, offset: int, description: str) -> int:
	require_range(payload, offset, 4, description)
	return struct.unpack_from("<I", payload, offset)[0]


def parse_sections(
	payload: bytes, section_table: int, count: int
) -> list[dict[str, Any]]:
	sections = []
	for index in range(count):
		offset = section_table + index * 40
		require_range(payload, offset, 40, f"PE section header {index}")
		raw_name = payload[offset : offset + 8]
		name = raw_name.split(b"\0", 1)[0].decode("ascii", errors="strict")
		virtual_size = unpack_u32(payload, offset + 8, "section virtual size")
		virtual_address = unpack_u32(payload, offset + 12, "section RVA")
		raw_size = unpack_u32(payload, offset + 16, "section raw size")
		raw_pointer = unpack_u32(payload, offset + 20, "section raw pointer")
		characteristics = unpack_u32(payload, offset + 36, "section characteristics")
		if raw_size:
			require_range(payload, raw_pointer, raw_size, f"section {name!r}")
		sections.append(
			{
				"name": name,
				"virtual_size": virtual_size,
				"virtual_address": virtual_address,
				"raw_size": raw_size,
				"raw_pointer": raw_pointer,
				"characteristics": characteristics,
			}
		)
	return sections


def rva_to_file_offset(
	rva: int, size_of_headers: int, sections: list[dict[str, Any]]
) -> int:
	if rva < size_of_headers:
		return rva
	for section in sections:
		start = section["virtual_address"]
		span = max(section["virtual_size"], section["raw_size"])
		if start <= rva < start + span:
			delta = rva - start
			if delta >= section["raw_size"]:
				raise VerificationError(
					f"RVA {hex32(rva)} lies in the zero-filled tail of "
					f"section {section['name']!r}"
				)
			return section["raw_pointer"] + delta
	raise VerificationError(f"RVA {hex32(rva)} is not mapped by any PE section")


def verify_pe(payload: bytes, spec: ModuleSpec) -> tuple[dict[str, Any], bytes]:
	"""Validate a pinned raw PE and construct its loaded-memory byte layout."""
	if payload[:2] != b"MZ":
		raise VerificationError(f"{spec.name}: missing MZ signature")
	pe_offset = unpack_u32(payload, 0x3C, f"{spec.name} e_lfanew")
	require_range(payload, pe_offset, 24, f"{spec.name} PE/COFF header")
	if payload[pe_offset : pe_offset + 4] != b"PE\0\0":
		raise VerificationError(f"{spec.name}: missing PE signature")

	coff = pe_offset + 4
	machine = unpack_u16(payload, coff, "COFF machine")
	section_count = unpack_u16(payload, coff + 2, "COFF section count")
	optional_size = unpack_u16(payload, coff + 16, "COFF optional-header size")
	characteristics = unpack_u16(payload, coff + 18, "COFF characteristics")
	optional = coff + 20
	require_range(payload, optional, optional_size, "PE optional header")

	magic = unpack_u16(payload, optional, "PE optional-header magic")
	entry_rva = unpack_u32(payload, optional + 16, "PE entry RVA")
	image_base = unpack_u32(payload, optional + 28, "PE image base")
	section_alignment = unpack_u32(payload, optional + 32, "PE section alignment")
	file_alignment = unpack_u32(payload, optional + 36, "PE file alignment")
	size_of_image = unpack_u32(payload, optional + 56, "PE image size")
	size_of_headers = unpack_u32(payload, optional + 60, "PE header size")
	subsystem = unpack_u16(payload, optional + 68, "PE subsystem")
	directory_count = unpack_u32(payload, optional + 92, "PE directory count")
	if directory_count < 6 or optional_size < 0xE0:
		raise VerificationError(f"{spec.name}: truncated PE data-directory table")
	directories = []
	for index in range(6):
		directory = optional + 96 + index * 8
		directories.append(
			(
				unpack_u32(payload, directory, f"PE directory {index} RVA"),
				unpack_u32(payload, directory + 4, f"PE directory {index} size"),
			)
		)

	section_table = optional + optional_size
	sections = parse_sections(payload, section_table, section_count)
	entry_file_offset = rva_to_file_offset(entry_rva, size_of_headers, sections)
	require_range(payload, entry_file_offset, spec.entry_prefix_size, "PE entry prefix")

	checks = {
		"PE header offset": (pe_offset, spec.pe_offset),
		"machine": (machine, 0x14C),
		"section count": (section_count, 3),
		"optional-header size": (optional_size, 0xE0),
		"COFF characteristics": (characteristics, spec.coff_characteristics),
		"optional-header magic": (magic, 0x10B),
		"entry RVA": (entry_rva, spec.entry_rva),
		"image base": (image_base, spec.image_base),
		"section alignment": (section_alignment, 0x20),
		"file alignment": (file_alignment, 0x20),
		"image size": (size_of_image, spec.size),
		"header size": (size_of_headers, spec.size_of_headers),
		"subsystem": (subsystem, 3),
		"entry file offset": (entry_file_offset, spec.entry_file_offset),
	}
	for description, (actual, expected) in checks.items():
		if actual != expected:
			raise VerificationError(
				f"{spec.name}: {description} is {hex32(actual)}, "
				f"expected {hex32(expected)}"
			)
	if not characteristics & 0x2000:
		raise VerificationError(f"{spec.name}: PE is not marked as a DLL")
	if directories[0] != (0, 0) or directories[1] != (0, 0):
		raise VerificationError(f"{spec.name}: unexpected export/import directory")
	if directories[5] != (spec.relocation_rva, spec.relocation_size):
		raise VerificationError(
			f"{spec.name}: relocation directory is "
			f"{hex32(directories[5][0])}+{hex32(directories[5][1])}, expected "
			f"{hex32(spec.relocation_rva)}+{hex32(spec.relocation_size)}"
		)
	if [section["name"] for section in sections] != [".text", ".data", ".reloc"]:
		raise VerificationError(f"{spec.name}: unexpected PE section order")
	# Publish only the measured length and digest, never vendor opcodes.
	entry_prefix = payload[entry_file_offset : entry_file_offset + spec.entry_prefix_size]
	if sha256(entry_prefix) != spec.entry_prefix_sha256:
		raise VerificationError(f"{spec.name}: entry-point prefix SHA-256 mismatch")

	loaded = bytearray(size_of_image)
	require_range(payload, 0, size_of_headers, "PE headers")
	loaded[:size_of_headers] = payload[:size_of_headers]
	loaded_ranges = [(0, size_of_headers, "headers")]
	for section in sections:
		destination = section["virtual_address"]
		copy_size = section["raw_size"]
		if destination + copy_size > size_of_image:
			raise VerificationError(
				f"{spec.name}: loaded section {section['name']!r} exceeds "
				f"SizeOfImage ({hex32(destination)}+{hex32(copy_size)})"
			)
		for start, end, description in loaded_ranges:
			if destination < end and start < destination + copy_size:
				raise VerificationError(
					f"{spec.name}: loaded section {section['name']!r} overlaps "
					f"{description}"
				)
		loaded[destination : destination + copy_size] = payload[
			section["raw_pointer"] : section["raw_pointer"] + copy_size
		]
		loaded_ranges.append(
			(destination, destination + copy_size, f"section {section['name']!r}")
		)
	loaded_payload = bytes(loaded)
	loaded_hash = sha256(loaded_payload)
	if loaded_hash != spec.loaded_sha256:
		raise VerificationError(
			f"{spec.name}: loaded-memory SHA-256 is {loaded_hash}, "
			f"expected {spec.loaded_sha256}"
		)
	loaded_fnv1a32 = fnv1a32(loaded_payload)
	if loaded_fnv1a32 != spec.fnv1a32:
		raise VerificationError(
			f"{spec.name}: loaded-memory FNV-1a-32 is "
			f"{hex32(loaded_fnv1a32)}, expected {hex32(spec.fnv1a32)}"
		)

	direct_xip_layout = loaded_payload == payload
	if direct_xip_layout != spec.direct_xip_layout:
		raise VerificationError(
			f"{spec.name}: direct-XIP layout property changed unexpectedly"
		)
	if image_base + entry_rva != spec.entry_address:
		raise VerificationError(f"{spec.name}: preferred entry address mismatch")

	metadata = {
		"pe_header_offset": hex32(pe_offset),
		"machine": "i386 (0x014c)",
		"pe_magic": "PE32 (0x010b)",
		"image_base": hex32(image_base),
		"size_of_image": hex32(size_of_image),
		"size_of_headers": hex32(size_of_headers),
		"entry_rva": hex32(entry_rva),
		"entry_address": hex32(image_base + entry_rva),
		"entry_file_offset": hex32(entry_file_offset),
		"section_alignment": hex32(section_alignment),
		"file_alignment": hex32(file_alignment),
		"imports": "none",
		"exports": "none",
		"relocations": {
			"rva": hex32(directories[5][0]),
			"size": hex32(directories[5][1]),
		},
		"direct_xip_layout": direct_xip_layout,
		"loaded_image_sha256": loaded_hash,
		"loaded_image_fnv1a32": hex32(loaded_fnv1a32),
		"sections": [
			{
				"name": section["name"],
				"virtual_address": hex32(section["virtual_address"]),
				"virtual_size": hex32(section["virtual_size"]),
				"raw_pointer": hex32(section["raw_pointer"]),
				"raw_size": hex32(section["raw_size"]),
			}
			for section in sections
		],
	}
	return metadata, loaded_payload


def validate_module_layout() -> None:
	previous_end = 0
	for spec in sorted(MODULES, key=lambda item: item.offset):
		if spec.offset < previous_end:
			raise VerificationError(f"module layout overlap at {spec.name}")
		if spec.offset + spec.size > ROM_SIZE:
			raise VerificationError(f"module {spec.name} exceeds the 4 MiB ROM")
		if 0xFFC00000 + spec.offset != spec.image_base:
			raise VerificationError(f"module {spec.name} preferred-base mismatch")
		previous_end = spec.offset + spec.size
	all_regions = [
		(spec.offset, spec.offset + spec.size, spec.name) for spec in MODULES
	] + [
		(spec.offset, spec.offset + spec.size, spec.name) for spec in SUPPORT_SLICES
	]
	sorted_regions = sorted(all_regions)
	for index, (start, end, name) in enumerate(sorted_regions):
		if start < 0 or end > ROM_SIZE:
			raise VerificationError(f"region {name} exceeds the 4 MiB ROM")
		if index and start < sorted_regions[index - 1][1]:
			raise VerificationError(f"region layout overlap at {name}")


def decode_rel32_call(payload: bytes, address: int) -> int:
	offset = address - 0xFFC00000
	require_range(payload, offset, 5, f"call at {hex32(address)}")
	if payload[offset] != 0xE8:
		raise VerificationError(f"expected relative call at {hex32(address)}")
	displacement = struct.unpack_from("<i", payload, offset + 1)[0]
	return address + 5 + displacement


def load_support_slices(payload: bytes) -> tuple[list[dict[str, Any]], dict[str, Any]]:
	slices = []
	for spec in SUPPORT_SLICES:
		region = payload[spec.offset : spec.offset + spec.size]
		actual_hash = sha256(region)
		if actual_hash != spec.sha256:
			raise VerificationError(
				f"{spec.name} SHA-256 is {actual_hash}, expected {spec.sha256}"
			)
		actual_fnv1a32 = fnv1a32(region)
		if actual_fnv1a32 != spec.fnv1a32:
			raise VerificationError(
				f"{spec.name} FNV-1a-32 is {hex32(actual_fnv1a32)}, "
				f"expected {hex32(spec.fnv1a32)}"
			)
		if 0xFFC00000 + spec.offset != spec.virtual_address:
			raise VerificationError(f"{spec.name}: virtual-address mismatch")
		slices.append({"spec": spec, "payload": region})

	wrapper_call = decode_rel32_call(payload, 0xFFFC0BF7)
	helper_call = decode_rel32_call(payload, 0xFFFC1554)
	indirect_sequence_offset = 0x3C0CF8
	require_range(payload, indirect_sequence_offset, 7, "CSI indirect entry call")
	indirect_sequence = payload[indirect_sequence_offset : indirect_sequence_offset + 7]
	if indirect_sequence[:1] != b"\xb8" or indirect_sequence[5:] != b"\xff\xd0":
		raise VerificationError("CSI wrapper constant-entry call sequence changed")
	indirect_target = struct.unpack_from("<I", indirect_sequence, 1)[0]
	if wrapper_call != 0xFFFC1554:
		raise VerificationError("CSI wrapper no longer calls the pinned CMOS helper")
	if helper_call != 0xFFFC1566:
		raise VerificationError("CSI CMOS helper transitive call target changed")
	if indirect_target != 0xFFFE7000:
		raise VerificationError("CSI wrapper no longer calls the pinned CSI entry")
	return slices, {
		"evidence": (
			"Linear disassembly of the hash-pinned complete wrapper function found "
			"one direct call outside its range and one constant indirect module call. "
			"The first support slice is exactly [0xfffc04e2,0xfffc0d19). The "
			"second is exactly [0xfffc1554,0xfffc1579), which contains helper "
			"0xfffc1554 and its sole transitive callee 0xfffc1566. The unrelated "
			"gap is deliberately excluded. All decoded relative branch targets stay "
			"inside their respective slices; no additional flash code/data reference "
			"was observed. Absolute non-flash operands are hardware/CMOS/MMIO inputs."
		),
		"audited_calls": [
			{
				"call_site": "0xfffc0bf7",
				"kind": "rel32",
				"target": hex32(wrapper_call),
				"target_in": "CSI_CMOS_READ_FIELD_HELPER",
			},
			{
				"call_site": "0xfffc0cfd",
				"kind": "constant indirect via eax",
				"target": hex32(indirect_target),
				"target_in": "CSI_INITDLL",
			},
			{
				"call_site": "0xfffc1554",
				"kind": "rel32",
				"target": hex32(helper_call),
				"target_in": "CSI_CMOS_READ_FIELD_HELPER",
			},
		],
	}


def load_vendor_rom(
	path: Path,
) -> tuple[bytes, list[dict[str, Any]], list[dict[str, Any]], dict[str, Any]]:
	if not path.is_file():
		raise VerificationError(f"not a regular vendor ROM: {path}")
	payload = path.read_bytes()
	actual_hash = sha256(payload)
	if len(payload) != ROM_SIZE:
		raise VerificationError(
			f"vendor ROM size is {hex32(len(payload))}, expected {hex32(ROM_SIZE)}"
		)
	if actual_hash != ROM_SHA256:
		raise VerificationError(
			f"vendor ROM SHA-256 is {actual_hash}, expected {ROM_SHA256}"
		)

	validate_module_layout()
	modules = []
	for spec in MODULES:
		module = payload[spec.offset : spec.offset + spec.size]
		module_hash = sha256(module)
		if module_hash != spec.raw_sha256:
			raise VerificationError(
				f"{spec.name} raw SHA-256 is {module_hash}, expected {spec.raw_sha256}"
			)
		module_fnv1a32 = fnv1a32(module)
		if module_fnv1a32 != spec.fnv1a32:
			raise VerificationError(
				f"{spec.name} raw FNV-1a-32 is {hex32(module_fnv1a32)}, "
				f"expected {hex32(spec.fnv1a32)}"
			)
		pe_metadata, loaded = verify_pe(module, spec)
		modules.append(
			{
				"spec": spec,
				"raw_payload": module,
				"loaded_payload": loaded,
				"pe": pe_metadata,
			}
		)
	support_slices, support_audit = load_support_slices(payload)
	return payload, modules, support_slices, support_audit


def module_manifest(module: dict[str, Any]) -> dict[str, Any]:
	spec = module["spec"]
	return {
		"name": spec.name,
		"rom_offset": hex32(spec.offset),
		"rom_end_exclusive": hex32(spec.offset + spec.size),
		"raw": {
			"filename": spec.filename,
			"size": len(module["raw_payload"]),
			"sha256": spec.raw_sha256,
			"fnv1a32": hex32(spec.fnv1a32),
		},
		"loaded": {
			"filename": spec.loaded_filename,
			"size": len(module["loaded_payload"]),
			"sha256": spec.loaded_sha256,
			"fnv1a32": hex32(spec.fnv1a32),
			"byte_identical_to_raw": module["loaded_payload"]
			== module["raw_payload"],
		},
		"pe": module["pe"],
	}


def support_manifest(
	support_slices: list[dict[str, Any]], support_audit: dict[str, Any]
) -> dict[str, Any]:
	return {
		"optional": True,
		"purpose": (
			"Exact original-address CSI policy wrapper and its only local "
			"out-of-range transitive helper; not required when coreboot "
			"reimplements policy construction."
		),
		"slices": [
			{
				"name": item["spec"].name,
				"filename": item["spec"].filename,
				"rom_offset": hex32(item["spec"].offset),
				"rom_end_exclusive": hex32(
					item["spec"].offset + item["spec"].size
				),
				"virtual_address": hex32(item["spec"].virtual_address),
				"size": item["spec"].size,
				"sha256": item["spec"].sha256,
				"fnv1a32": hex32(item["spec"].fnv1a32),
			}
			for item in support_slices
		],
		"static_call_audit": support_audit,
	}


def deterministic_manifest(
	modules: list[dict[str, Any]],
	support_slices: list[dict[str, Any]],
	support_audit: dict[str, Any],
) -> dict[str, Any]:
	return {
		"format_version": 1,
		"source": {
			"filename": "A7522IMS.8F0",
			"size": ROM_SIZE,
			"sha256": ROM_SHA256,
		},
		"layout": (
			"Composed ROM uses deterministic loaded PE memory images at the "
			"original ImageBase offsets. For this pinned full-ROM raw and "
			"loaded module bytes are identical because headers and every "
			"section raw pointer are already RVA-congruent."
		),
		"rejected_legacy_csi_sha256": (
			"b1d14415fcdd7f0a7cf15d0103a06cf9f3f731238f7a0d221a7e48e956215a8b"
		),
		"rejected_legacy_csi_reason": (
			"Locally normalized/modified PE, not the exact raw slice of the "
			"hash-pinned A7522IMS.8F0 full-ROM."
		),
		"modules": [module_manifest(module) for module in modules],
		"csi_wrapper_support": support_manifest(
			support_slices, support_audit
		),
	}


def write_exact(path: Path, payload: bytes, force: bool) -> str:
	if path.exists():
		if not path.is_file():
			raise VerificationError(f"output exists and is not a regular file: {path}")
		if path.read_bytes() == payload:
			return "already-verified"
		if not force:
			raise VerificationError(
				f"output exists with different bytes: {path} (use --force to replace)"
			)
	path.parent.mkdir(parents=True, exist_ok=True)
	path.write_bytes(payload)
	if path.read_bytes() != payload:
		raise VerificationError(f"write verification failed: {path}")
	return "written"


def command_extract(args: argparse.Namespace) -> int:
	_, modules, support_slices, support_audit = load_vendor_rom(args.vendor_rom)
	statuses = []
	for module in modules:
		spec = module["spec"]
		raw_path = args.output_dir / spec.filename
		loaded_path = args.output_dir / spec.loaded_filename
		raw_status = write_exact(raw_path, module["raw_payload"], args.force)
		loaded_status = write_exact(
			loaded_path, module["loaded_payload"], args.force
		)
		statuses.append(
			{
				"name": spec.name,
				"raw_output": str(raw_path),
				"raw_status": raw_status,
				"raw_size": len(module["raw_payload"]),
				"raw_sha256": spec.raw_sha256,
				"fnv1a32": hex32(spec.fnv1a32),
				"loaded_output": str(loaded_path),
				"loaded_status": loaded_status,
				"loaded_size": len(module["loaded_payload"]),
				"loaded_sha256": spec.loaded_sha256,
				"loaded_equals_raw": module["loaded_payload"]
				== module["raw_payload"],
				"pe": module["pe"],
			}
		)
	support_statuses = []
	if args.include_csi_wrapper_support:
		for item in support_slices:
			spec = item["spec"]
			path = args.output_dir / spec.filename
			support_statuses.append(
				{
					"name": spec.name,
					"output": str(path),
					"status": write_exact(path, item["payload"], args.force),
					"size": spec.size,
					"sha256": spec.sha256,
					"fnv1a32": hex32(spec.fnv1a32),
				}
			)

	manifest_payload = (
		json.dumps(
			deterministic_manifest(modules, support_slices, support_audit),
			indent=2,
			sort_keys=True,
		)
		+ "\n"
	).encode("utf-8")
	manifest_path = args.output_dir / MANIFEST_NAME
	manifest_status = write_exact(manifest_path, manifest_payload, args.force)
	print(
		json.dumps(
			{
				"operation": "extract",
				"vendor_rom": str(args.vendor_rom),
				"vendor_rom_sha256": ROM_SHA256,
				"manifest": str(manifest_path),
				"manifest_status": manifest_status,
				"modules": statuses,
				"csi_wrapper_support": support_statuses,
			},
			indent=2,
			sort_keys=True,
		)
	)
	return 0


def load_coreboot_rom(path: Path) -> bytes:
	if not path.is_file():
		raise VerificationError(f"not a regular coreboot ROM: {path}")
	payload = path.read_bytes()
	if len(payload) != ROM_SIZE:
		raise VerificationError(
			f"coreboot ROM size is {hex32(len(payload))}, expected {hex32(ROM_SIZE)}"
		)
	return payload


def composition_regions(
	modules: list[dict[str, Any]],
	support_slices: list[dict[str, Any]],
	include_support: bool,
) -> list[dict[str, Any]]:
	regions = [
		{
			"name": module["spec"].name,
			"offset": module["spec"].offset,
			"size": len(module["loaded_payload"]),
			"payload": module["loaded_payload"],
			"sha256": module["spec"].loaded_sha256,
			"fnv1a32": module["spec"].fnv1a32,
			"layout": "loaded-pe-memory-image",
		}
		for module in modules
	]
	if include_support:
		regions.extend(
			{
				"name": item["spec"].name,
				"offset": item["spec"].offset,
				"size": item["spec"].size,
				"payload": item["payload"],
				"sha256": item["spec"].sha256,
				"fnv1a32": item["spec"].fnv1a32,
				"layout": "original-address-code-slice",
			}
			for item in support_slices
		)
	return sorted(regions, key=lambda item: item["offset"])


def verify_composite_bytes(
	image: bytes, regions: list[dict[str, Any]]
) -> list[dict[str, Any]]:
	if len(image) != ROM_SIZE:
		raise VerificationError(
			f"composite image size is {hex32(len(image))}, expected {hex32(ROM_SIZE)}"
		)
	results = []
	for region in regions:
		actual = image[region["offset"] : region["offset"] + region["size"]]
		actual_hash = sha256(actual)
		actual_fnv1a32 = fnv1a32(actual)
		if actual != region["payload"]:
			raise VerificationError(
				f"composite {region['name']} bytes differ at "
				f"{hex32(region['offset'])}.."
				f"{hex32(region['offset'] + region['size'])}; range SHA-256 "
				f"is {actual_hash}, expected {region['sha256']}"
			)
		results.append(
			{
				"name": region["name"],
				"offset": hex32(region["offset"]),
				"end_exclusive": hex32(region["offset"] + region["size"]),
				"sha256": actual_hash,
				"fnv1a32": hex32(actual_fnv1a32),
				"layout": region["layout"],
				"status": "verified",
			}
		)
	return results


def paths_are_same(left: Path, right: Path) -> bool:
	return left.resolve(strict=False) == right.resolve(strict=False)


def command_compose(args: argparse.Namespace) -> int:
	if paths_are_same(args.output_rom, args.coreboot_rom):
		raise VerificationError("--output-rom must differ from --coreboot-rom")
	if paths_are_same(args.output_rom, args.vendor_rom):
		raise VerificationError("--output-rom must differ from --vendor-rom")
	_, modules, support_slices, _ = load_vendor_rom(args.vendor_rom)
	regions = composition_regions(
		modules, support_slices, args.include_csi_wrapper_support
	)
	base = load_coreboot_rom(args.coreboot_rom)
	composite = bytearray(base)
	range_statuses = []
	for region in regions:
		start = region["offset"]
		end = start + region["size"]
		current = base[start:end]
		if current == region["payload"]:
			status = "already-present"
		elif current == b"\xff" * region["size"]:
			status = "inserted-into-erased-range"
		else:
			first_conflict = next(
				index for index, value in enumerate(current) if value != 0xFF
			)
			non_erased = sum(value != 0xFF for value in current)
			raise VerificationError(
				f"{region['name']} target range is neither erased nor already exact: "
				f"{non_erased} non-0xff bytes, first at "
				f"{hex32(start + first_conflict)}, range SHA-256 "
				f"{sha256(current)}"
			)
		composite[start:end] = region["payload"]
		range_statuses.append(
			{
				"name": region["name"],
				"offset": hex32(start),
				"end_exclusive": hex32(end),
				"layout": region["layout"],
				"status": status,
			}
		)

	cursor = 0
	for region in regions:
		if composite[cursor : region["offset"]] != base[cursor : region["offset"]]:
			raise VerificationError(
				f"internal error: bytes outside module ranges changed before "
				f"{hex32(region['offset'])}"
			)
		cursor = region["offset"] + region["size"]
	if composite[cursor:] != base[cursor:]:
		raise VerificationError("internal error: trailing bytes outside ranges changed")
	verify_composite_bytes(bytes(composite), regions)
	output_status = write_exact(args.output_rom, bytes(composite), args.force)
	written = args.output_rom.read_bytes()
	verified = verify_composite_bytes(written, regions)
	if sha256(written) != sha256(bytes(composite)):
		raise VerificationError("composite output hash changed during write")

	print(
		json.dumps(
			{
				"operation": "compose",
				"vendor_rom": str(args.vendor_rom),
				"vendor_rom_sha256": ROM_SHA256,
				"coreboot_rom": str(args.coreboot_rom),
				"coreboot_rom_sha256": sha256(base),
				"output_rom": str(args.output_rom),
				"output_rom_sha256": sha256(written),
				"output_status": output_status,
				"ranges": range_statuses,
				"readback": verified,
			},
			indent=2,
			sort_keys=True,
		)
	)
	return 0


def command_verify_composite(args: argparse.Namespace) -> int:
	_, modules, support_slices, _ = load_vendor_rom(args.vendor_rom)
	regions = composition_regions(
		modules, support_slices, args.include_csi_wrapper_support
	)
	image = load_coreboot_rom(args.image)
	verified = verify_composite_bytes(image, regions)
	print(
		json.dumps(
			{
				"operation": "verify-composite",
				"vendor_rom": str(args.vendor_rom),
				"vendor_rom_sha256": ROM_SHA256,
				"image": str(args.image),
				"image_sha256": sha256(image),
				"modules": verified,
			},
			indent=2,
			sort_keys=True,
		)
	)
	return 0


def make_parser() -> argparse.ArgumentParser:
	parser = argparse.ArgumentParser(description=__doc__)
	subparsers = parser.add_subparsers(dest="command", required=True)

	extract = subparsers.add_parser(
		"extract", help="extract exact modules from the pinned MSI vendor ROM"
	)
	extract.add_argument("--vendor-rom", type=Path, required=True)
	extract.add_argument("--output-dir", type=Path, required=True)
	extract.add_argument(
		"--include-csi-wrapper-support",
		action="store_true",
		help="also extract the audited original-address CSI wrapper/helper slices",
	)
	extract.add_argument(
		"--force", action="store_true", help="replace differing explicit outputs"
	)
	extract.set_defaults(handler=command_extract)

	compose = subparsers.add_parser(
		"compose",
		help="insert exact loaded modules into erased ranges of a 4 MiB ROM",
	)
	compose.add_argument("--vendor-rom", type=Path, required=True)
	compose.add_argument("--coreboot-rom", type=Path, required=True)
	compose.add_argument("--output-rom", type=Path, required=True)
	compose.add_argument(
		"--include-csi-wrapper-support",
		action="store_true",
		help="also insert the audited CSI wrapper/helper slices",
	)
	compose.add_argument(
		"--force", action="store_true", help="replace a differing explicit output"
	)
	compose.set_defaults(handler=command_compose)

	verify = subparsers.add_parser(
		"verify-composite",
		help="verify loaded vendor-module bytes in a 4 MiB image",
	)
	verify.add_argument("--vendor-rom", type=Path, required=True)
	verify.add_argument("--image", type=Path, required=True)
	verify.add_argument(
		"--include-csi-wrapper-support",
		action="store_true",
		help="also require the audited CSI wrapper/helper slices",
	)
	verify.set_defaults(handler=command_verify_composite)
	return parser


def main() -> int:
	parser = make_parser()
	args = parser.parse_args()
	try:
		return args.handler(args)
	except (OSError, UnicodeError, VerificationError) as error:
		print(f"error: {error}", file=sys.stderr)
		return 1


if __name__ == "__main__":
	raise SystemExit(main())
