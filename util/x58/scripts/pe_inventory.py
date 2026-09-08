#!/usr/bin/env python3
"""Inventory embedded PE images without extracting proprietary bytes.

The output deliberately separates hashes of the raw embedded image, individual
sections, and a relocation-normalized representation.  A canonical hash is an
analysis aid only: matching hashes do not establish a compatible firmware ABI.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import struct
from typing import Any, Iterable


class PeFormatError(ValueError):
    """Raised when a candidate does not contain a bounded PE image."""


def _u16(data: bytes | bytearray, offset: int) -> int:
    if offset < 0 or offset + 2 > len(data):
        raise PeFormatError(f"u16 outside image at 0x{offset:x}")
    return struct.unpack_from("<H", data, offset)[0]


def _u32(data: bytes | bytearray, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        raise PeFormatError(f"u32 outside image at 0x{offset:x}")
    return struct.unpack_from("<I", data, offset)[0]


def _u64(data: bytes | bytearray, offset: int) -> int:
    if offset < 0 or offset + 8 > len(data):
        raise PeFormatError(f"u64 outside image at 0x{offset:x}")
    return struct.unpack_from("<Q", data, offset)[0]


def _sha256(data: bytes | bytearray) -> str:
    return hashlib.sha256(data).hexdigest()


@dataclass(frozen=True)
class Section:
    name: str
    virtual_size: int
    rva: int
    raw_size: int
    raw_offset: int
    characteristics: int


@dataclass(frozen=True)
class PeImage:
    source_offset: int
    raw: bytes
    machine: int
    timestamp: int
    image_base: int
    entry_rva: int
    size_of_image: int
    pe_magic: int
    optional_offset: int
    checksum_offset: int
    image_base_offset: int
    relocation_rva: int
    relocation_size: int
    sections: tuple[Section, ...]

    def rva_to_raw(self, rva: int, size: int = 1) -> int:
        if size < 0:
            raise PeFormatError("negative RVA length")
        for section in self.sections:
            span = max(section.virtual_size, section.raw_size)
            if section.rva <= rva and rva + size <= section.rva + span:
                relative = rva - section.rva
                if relative + size > section.raw_size:
                    raise PeFormatError(
                        f"RVA 0x{rva:x} lies in zero-filled section tail"
                    )
                return section.raw_offset + relative
        size_of_headers = _u32(self.raw, self.optional_offset + 60)
        if 0 <= rva and rva + size <= size_of_headers:
            return rva
        raise PeFormatError(f"unmapped RVA 0x{rva:x}")


@dataclass(frozen=True)
class TeImage:
    source_offset: int
    raw: bytes
    machine: int
    subsystem: int
    stripped_size: int
    image_base: int
    entry_rva: int
    base_of_code: int
    relocation_rva: int
    relocation_size: int
    sections: tuple[Section, ...]

    def rva_to_raw(self, rva: int, size: int = 1) -> int:
        if size < 0:
            raise PeFormatError("negative RVA length")
        for section in self.sections:
            span = max(section.virtual_size, section.raw_size)
            if section.rva <= rva and rva + size <= section.rva + span:
                relative = rva - section.rva
                if relative + size > section.raw_size:
                    raise PeFormatError(
                        f"RVA 0x{rva:x} lies in zero-filled TE section tail"
                    )
                return section.raw_offset + relative
        raise PeFormatError(f"unmapped TE RVA 0x{rva:x}")


def parse_pe(container: bytes, offset: int = 0) -> PeImage:
    if offset < 0 or offset + 0x40 > len(container):
        raise PeFormatError("truncated DOS header")
    if container[offset : offset + 2] != b"MZ":
        raise PeFormatError("missing MZ signature")

    pe_relative = _u32(container, offset + 0x3C)
    if pe_relative < 0x40 or pe_relative > 0x100000:
        raise PeFormatError("implausible e_lfanew")
    nt = offset + pe_relative
    if nt + 24 > len(container) or container[nt : nt + 4] != b"PE\0\0":
        raise PeFormatError("missing PE signature")

    machine = _u16(container, nt + 4)
    section_count = _u16(container, nt + 6)
    timestamp = _u32(container, nt + 8)
    optional_size = _u16(container, nt + 20)
    optional = nt + 24
    if section_count == 0 or section_count > 96:
        raise PeFormatError("implausible section count")
    if optional_size < 0x60 or optional + optional_size > len(container):
        raise PeFormatError("truncated optional header")

    magic = _u16(container, optional)
    if magic == 0x10B:
        image_base = _u32(container, optional + 28)
        image_base_relative = 28
        directory_offset = 96
    elif magic == 0x20B:
        image_base = _u64(container, optional + 24)
        image_base_relative = 24
        directory_offset = 112
    else:
        raise PeFormatError(f"unsupported optional-header magic 0x{magic:x}")

    entry_rva = _u32(container, optional + 16)
    size_of_image = _u32(container, optional + 56)
    size_of_headers = _u32(container, optional + 60)
    if size_of_headers < pe_relative + 24 + optional_size:
        raise PeFormatError("SizeOfHeaders does not cover PE headers")
    if optional_size < directory_offset + 8 * 6:
        raise PeFormatError("optional header lacks relocation directory")
    relocation_rva = _u32(container, optional + directory_offset + 8 * 5)
    relocation_size = _u32(container, optional + directory_offset + 8 * 5 + 4)

    section_table = optional + optional_size
    if section_table + section_count * 40 > len(container):
        raise PeFormatError("truncated section table")
    sections: list[Section] = []
    raw_end = size_of_headers
    for index in range(section_count):
        header = section_table + index * 40
        name_bytes = container[header : header + 8].split(b"\0", 1)[0]
        name = name_bytes.decode("ascii", errors="backslashreplace")
        virtual_size = _u32(container, header + 8)
        rva = _u32(container, header + 12)
        raw_size = _u32(container, header + 16)
        raw_offset = _u32(container, header + 20)
        characteristics = _u32(container, header + 36)
        if raw_size and (raw_offset < size_of_headers or raw_offset + raw_size < raw_offset):
            raise PeFormatError(f"invalid raw range for section {name!r}")
        raw_end = max(raw_end, raw_offset + raw_size)
        sections.append(
            Section(name, virtual_size, rva, raw_size, raw_offset, characteristics)
        )

    if raw_end <= 0 or offset + raw_end > len(container):
        raise PeFormatError("PE raw image extends outside input")
    raw = container[offset : offset + raw_end]
    return PeImage(
        source_offset=offset,
        raw=raw,
        machine=machine,
        timestamp=timestamp,
        image_base=image_base,
        entry_rva=entry_rva,
        size_of_image=size_of_image,
        pe_magic=magic,
        optional_offset=optional - offset,
        checksum_offset=optional - offset + 64,
        image_base_offset=optional - offset + image_base_relative,
        relocation_rva=relocation_rva,
        relocation_size=relocation_size,
        sections=tuple(sections),
    )


def parse_te(container: bytes, offset: int = 0) -> TeImage:
    te_header_size = 40
    if offset < 0 or offset + te_header_size > len(container):
        raise PeFormatError("truncated TE header")
    if container[offset : offset + 2] != b"VZ":
        raise PeFormatError("missing TE signature")
    machine = _u16(container, offset + 2)
    section_count = container[offset + 4]
    subsystem = container[offset + 5]
    stripped_size = _u16(container, offset + 6)
    entry_rva = _u32(container, offset + 8)
    base_of_code = _u32(container, offset + 12)
    image_base = _u64(container, offset + 16)
    relocation_rva = _u32(container, offset + 24)
    relocation_size = _u32(container, offset + 28)
    if section_count == 0 or section_count > 96:
        raise PeFormatError("implausible TE section count")
    if stripped_size < te_header_size:
        raise PeFormatError("TE StrippedSize is smaller than its header")

    section_table = offset + te_header_size
    if section_table + section_count * 40 > len(container):
        raise PeFormatError("truncated TE section table")
    adjustment = stripped_size - te_header_size
    sections: list[Section] = []
    raw_end = te_header_size + section_count * 40
    for index in range(section_count):
        header = section_table + index * 40
        name_bytes = container[header : header + 8].split(b"\0", 1)[0]
        name = name_bytes.decode("ascii", errors="backslashreplace")
        virtual_size = _u32(container, header + 8)
        rva = _u32(container, header + 12)
        raw_size = _u32(container, header + 16)
        original_raw_offset = _u32(container, header + 20)
        characteristics = _u32(container, header + 36)
        if raw_size and original_raw_offset < adjustment:
            raise PeFormatError(f"invalid TE raw range for section {name!r}")
        raw_offset = original_raw_offset - adjustment
        if raw_offset < te_header_size + section_count * 40:
            raise PeFormatError(f"TE section {name!r} overlaps headers")
        raw_end = max(raw_end, raw_offset + raw_size)
        sections.append(
            Section(name, virtual_size, rva, raw_size, raw_offset, characteristics)
        )
    if offset + raw_end > len(container):
        raise PeFormatError("TE raw image extends outside input")
    return TeImage(
        source_offset=offset,
        raw=container[offset : offset + raw_end],
        machine=machine,
        subsystem=subsystem,
        stripped_size=stripped_size,
        image_base=image_base,
        entry_rva=entry_rva,
        base_of_code=base_of_code,
        relocation_rva=relocation_rva,
        relocation_size=relocation_size,
        sections=tuple(sections),
    )


def scan_pe_images(data: bytes) -> list[PeImage]:
    found: list[PeImage] = []
    start = 0
    while True:
        offset = data.find(b"MZ", start)
        if offset < 0:
            break
        start = offset + 1
        try:
            found.append(parse_pe(data, offset))
        except PeFormatError:
            continue
    return found


def scan_te_images(data: bytes) -> list[TeImage]:
    found: list[TeImage] = []
    start = 0
    while True:
        offset = data.find(b"VZ", start)
        if offset < 0:
            break
        start = offset + 1
        try:
            found.append(parse_te(data, offset))
        except PeFormatError:
            continue
    return found


def relocation_entries(image: PeImage) -> list[tuple[int, int]]:
    if not image.relocation_rva or not image.relocation_size:
        return []
    table = image.rva_to_raw(image.relocation_rva, image.relocation_size)
    end = table + image.relocation_size
    entries: list[tuple[int, int]] = []
    cursor = table
    while cursor < end:
        if cursor + 8 > end:
            raise PeFormatError("truncated relocation block")
        page_rva = _u32(image.raw, cursor)
        block_size = _u32(image.raw, cursor + 4)
        if block_size < 8 or block_size % 2 or cursor + block_size > end:
            raise PeFormatError("invalid relocation block size")
        for item_offset in range(cursor + 8, cursor + block_size, 2):
            item = _u16(image.raw, item_offset)
            relocation_type = item >> 12
            if relocation_type:
                entries.append((relocation_type, page_rva + (item & 0x0FFF)))
        cursor += block_size
    return entries


def te_relocation_entries(image: TeImage) -> list[tuple[int, int]]:
    if not image.relocation_rva or not image.relocation_size:
        return []
    table = image.rva_to_raw(image.relocation_rva, image.relocation_size)
    end = table + image.relocation_size
    entries: list[tuple[int, int]] = []
    cursor = table
    while cursor < end:
        if cursor + 8 > end:
            raise PeFormatError("truncated TE relocation block")
        page_rva = _u32(image.raw, cursor)
        block_size = _u32(image.raw, cursor + 4)
        if block_size < 8 or block_size % 2 or cursor + block_size > end:
            raise PeFormatError("invalid TE relocation block size")
        for item_offset in range(cursor + 8, cursor + block_size, 2):
            item = _u16(image.raw, item_offset)
            relocation_type = item >> 12
            if relocation_type:
                entries.append((relocation_type, page_rva + (item & 0x0FFF)))
        cursor += block_size
    return entries


def canonical_bytes(image: PeImage) -> tuple[bytes, int, tuple[int, ...]]:
    """Return a metadata- and preferred-base-normalized PE representation."""

    normalized = bytearray(image.raw)
    nt = image.optional_offset - 24
    struct.pack_into("<I", normalized, nt + 8, 0)  # COFF TimeDateStamp
    struct.pack_into("<I", normalized, image.checksum_offset, 0)
    if image.pe_magic == 0x10B:
        struct.pack_into("<I", normalized, image.image_base_offset, 0)
    else:
        struct.pack_into("<Q", normalized, image.image_base_offset, 0)

    unsupported: set[int] = set()
    applied = 0
    for relocation_type, target_rva in relocation_entries(image):
        if relocation_type == 3 and image.pe_magic == 0x10B:  # HIGHLOW
            raw_offset = image.rva_to_raw(target_rva, 4)
            value = _u32(normalized, raw_offset)
            struct.pack_into("<I", normalized, raw_offset, (value - image.image_base) & 0xFFFFFFFF)
            applied += 1
        elif relocation_type == 10 and image.pe_magic == 0x20B:  # DIR64
            raw_offset = image.rva_to_raw(target_rva, 8)
            value = _u64(normalized, raw_offset)
            struct.pack_into("<Q", normalized, raw_offset, (value - image.image_base) & 0xFFFFFFFFFFFFFFFF)
            applied += 1
        else:
            unsupported.add(relocation_type)
    return bytes(normalized), applied, tuple(sorted(unsupported))


def canonical_te_bytes(image: TeImage) -> tuple[bytes, int, tuple[int, ...]]:
    normalized = bytearray(image.raw)
    struct.pack_into("<Q", normalized, 16, 0)
    unsupported: set[int] = set()
    applied = 0
    for relocation_type, target_rva in te_relocation_entries(image):
        if relocation_type == 3 and image.machine == 0x14C:  # HIGHLOW / IA32
            raw_offset = image.rva_to_raw(target_rva, 4)
            value = _u32(normalized, raw_offset)
            struct.pack_into(
                "<I", normalized, raw_offset, (value - image.image_base) & 0xFFFFFFFF
            )
            applied += 1
        elif relocation_type == 10 and image.machine == 0x8664:  # DIR64 / X64
            raw_offset = image.rva_to_raw(target_rva, 8)
            value = _u64(normalized, raw_offset)
            struct.pack_into(
                "<Q",
                normalized,
                raw_offset,
                (value - image.image_base) & 0xFFFFFFFFFFFFFFFF,
            )
            applied += 1
        else:
            unsupported.add(relocation_type)
    return bytes(normalized), applied, tuple(sorted(unsupported))


def _pdb_path(raw: bytes) -> str | None:
    position = raw.find(b"RSDS")
    if position < 0 or position + 24 >= len(raw):
        return None
    end = raw.find(b"\0", position + 24)
    if end < 0:
        return None
    return raw[position + 24 : end].decode("utf-8", errors="backslashreplace")


def _feature_counts(data: bytes) -> dict[str, int]:
    patterns = {
        "cpuid_0fa2": b"\x0f\xa2",
        "rdmsr_0f32": b"\x0f\x32",
        "wrmsr_0f30": b"\x0f\x30",
        "movntdq_660fe7": b"\x66\x0f\xe7",
        "pci_cf8_literal": b"\xf8\x0c\x00\x00",
        "pci_cfc_literal": b"\xfc\x0c\x00\x00",
        "reset_cf9_literal": b"\xf9\x0c",
    }
    return {name: data.count(pattern) for name, pattern in patterns.items()}


def image_record(image: PeImage) -> dict[str, Any]:
    canonical, applied, unsupported = canonical_bytes(image)
    sections: list[dict[str, Any]] = []
    executable = bytearray()
    for section in image.sections:
        body = image.raw[section.raw_offset : section.raw_offset + section.raw_size]
        if section.characteristics & 0x20000000:
            executable.extend(body)
        sections.append(
            {
                "name": section.name,
                "rva": f"0x{section.rva:x}",
                "virtual_size": section.virtual_size,
                "raw_offset": f"0x{section.raw_offset:x}",
                "raw_size": section.raw_size,
                "characteristics": f"0x{section.characteristics:x}",
                "sha256": _sha256(body),
            }
        )
    return {
        "offset": f"0x{image.source_offset:x}",
        "raw_size": len(image.raw),
        "raw_sha256": _sha256(image.raw),
        "canonical_sha256": _sha256(canonical),
        "relocations_normalized": applied,
        "unsupported_relocation_types": list(unsupported),
        "machine": f"0x{image.machine:x}",
        "timestamp": image.timestamp,
        "pe_magic": f"0x{image.pe_magic:x}",
        "image_base": f"0x{image.image_base:x}",
        "entry_rva": f"0x{image.entry_rva:x}",
        "size_of_image": image.size_of_image,
        "pdb_path": _pdb_path(image.raw),
        "executable_sha256": _sha256(executable),
        "features": _feature_counts(executable),
        "sections": sections,
    }


def te_image_record(image: TeImage) -> dict[str, Any]:
    canonical, applied, unsupported = canonical_te_bytes(image)
    sections: list[dict[str, Any]] = []
    executable = bytearray()
    for section in image.sections:
        body = image.raw[section.raw_offset : section.raw_offset + section.raw_size]
        if section.characteristics & 0x20000000:
            executable.extend(body)
        sections.append(
            {
                "name": section.name,
                "rva": f"0x{section.rva:x}",
                "virtual_size": section.virtual_size,
                "raw_offset": f"0x{section.raw_offset:x}",
                "raw_size": section.raw_size,
                "characteristics": f"0x{section.characteristics:x}",
                "sha256": _sha256(body),
            }
        )
    return {
        "offset": f"0x{image.source_offset:x}",
        "raw_size": len(image.raw),
        "raw_sha256": _sha256(image.raw),
        "canonical_sha256": _sha256(canonical),
        "relocations_normalized": applied,
        "unsupported_relocation_types": list(unsupported),
        "machine": f"0x{image.machine:x}",
        "subsystem": f"0x{image.subsystem:x}",
        "stripped_size": image.stripped_size,
        "image_base": f"0x{image.image_base:x}",
        "entry_rva": f"0x{image.entry_rva:x}",
        "base_of_code": f"0x{image.base_of_code:x}",
        "pdb_path": _pdb_path(image.raw),
        "executable_sha256": _sha256(executable),
        "features": _feature_counts(executable),
        "sections": sections,
    }


def inventory(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    return {
        "path": str(path),
        "size": len(data),
        "sha256": _sha256(data),
        "pe_images": [image_record(image) for image in scan_pe_images(data)],
        "te_images": [te_image_record(image) for image in scan_te_images(data)],
    }


def _iter_paths(values: Iterable[Path]) -> list[Path]:
    paths = list(values)
    missing = [str(path) for path in paths if not path.is_file()]
    if missing:
        raise SystemExit("not a regular file: " + ", ".join(missing))
    return paths


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("images", nargs="+", type=Path)
    parser.add_argument(
        "--compact", action="store_true", help="emit compact rather than indented JSON"
    )
    args = parser.parse_args()
    result = [inventory(path) for path in _iter_paths(args.images)]
    print(json.dumps(result, indent=None if args.compact else 2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
