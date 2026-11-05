#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Audit the retained desktop CPU firmware; never access or modify hardware.

The layouts follow vendorcode/amd/psp/{psp_efs,combo_directory,
psp_directory,bios_directory}.h. This deliberately describes the qualified
B565 container, rather than guessing addresses in arbitrary vendor images.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

FLASH_SIZE = 0x2000000
BANK_SIZE = 0x1000000
TEMPLATE_SHA256 = 'aa9cbc87e545fef85c2ed754c2ec3d5ef1755c07ff9a5cfc3ab858dc2e7cf86d'
DESKTOP_PSP_ID = 0xbc0b0500
MICROCODE_REVISIONS = (0x8700, 0x8710, 0xa200, 0xa210, 0xa212)
VERMEER_BOOT_UCODE_SOURCE = 0x16e1b00
VERMEER_BOOT_UCODE_BYTES = 5568
VERMEER_BOOT_UCODE_OLD = 'e86d516a8e72f3fe6e16bc62594017e96d3d0e6ba7ace368f755f013bb22f641'
VERMEER_BOOT_UCODE_NEW = '72089265a025f253079dc1831d6c7b263f1bd20482b78ecb799b88a87569f6d0'
VERMEER_ABL_BYTES = 511680
VERMEER_ABL_SHA256 = 'f67f88a0bcf1949dae9c1f74cb9a889e4c645f83d9eb4ebe7bbd36b741862f5a'
VERMEER_ABL_SOURCE = 0xec1000
# Exact original directory instances in the qualified template. The new unit
# does not fit either old allocation. These hashes bind routing, not signatures.
VERMEER_ABL_DIRECTORIES = (
    (0xe9000, 400,
     '14efc14c27f7621776c4eecb209efd2bd26c8d347d357cba6a5b30951e2433aa',
     'a579bb67060f95ba6bc38e9495f3016a80dd465a97bbe17a53f5e632f11e6439'),
    (0x4d1000, 496,
     'edfdd976a35785164125581a3944194756c5688b61f178ab13e5143116013924',
     '0ab9563f946c896940acfd90975dc5a9de9cef559e7d77e05262a059b28a5533'),
)
VERMEER_ABL_DIRECTORY_REF_SHA256 = 'f8bd80434a7e4b2af806b6b2370eea874b4e5db094879f656ae60cbcd6964411'
FAMILIES = {
    'matisse': dict(cpuid_models=[0x00870f00, 0x00870f10], subprogram=0,
                    max_threads=32, microcode=[0x8700, 0x8710]),
    'vermeer': dict(cpuid_models=[0x00a20f00, 0x00a20f10], subprogram=1,
                    max_threads=32, microcode=[0xa200, 0xa210, 0xa212]),
}


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def span(image, offset, size):
    require(0 <= offset <= len(image) and 0 <= size <= len(image) - offset,
            'Firmware reference exceeds image')
    return image[offset:offset + size]


def fletcher32(data):
    require(len(data) % 2 == 0, 'Odd directory checksum extent')
    words = struct.unpack('<%dH' % (len(data) // 2), data)
    low = high = 0xffff
    for start in range(0, len(words), 360):
        for word in words[start:start + 360]:
            low += word
            high += low
        low = (low & 0xffff) + (low >> 16)
        high = (high & 0xffff) + (high >> 16)
    low = (low & 0xffff) + (low >> 16)
    high = (high & 0xffff) + (high >> 16)
    return (high << 16) | low


def directory(image, offset, magic):
    header_size = 32 if magic in (b'2PSP', b'2BHD') else 16
    width = 24 if magic in (b'$BHD', b'$BL2') else 16
    head = span(image, offset, header_size)
    require(head[:4] == magic, 'Unexpected firmware directory signature')
    checksum, count = struct.unpack_from('<II', head, 4)
    require(0 < count <= 64, 'Firmware directory count')
    data = span(image, offset, header_size + count * width)
    require(fletcher32(data[8:]) == checksum, 'Firmware directory checksum')
    result = dict(offset=offset, magic=magic.decode(), bytes=len(data),
                  sha256=digest(data), checksum_valid=True, entries=[])
    if header_size == 32:
        require(struct.unpack_from('<I', head, 12)[0] == 0, 'Unknown combo lookup')
        require(head[16:] == bytes(16), 'Unknown combo header extension')
    else:
        # These selected directories use the retained 16-MiB-bank addressing.
        require((struct.unpack_from('<I', head, 12)[0] >> 29) & 3 == 1,
                'Unqualified directory address mode')
    for index in range(count):
        start = header_size + index * width
        attributes, size, source = struct.unpack_from('<IIQ', data, start)
        if header_size == 32:
            require(attributes == 0, 'Unqualified combo ID selector')
            row = dict(id=size, source=source)
        else:
            require(source >> 62 == 0, 'Unqualified per-entry address mode')
            row = dict(type=attributes & 0xff, attributes=attributes,
                       subprogram=(attributes >> (24 if width == 24 else 8)) &
                       (7 if width == 24 else 0xff), bytes=size, source=source)
            if width == 24:
                row['destination'] = struct.unpack_from('<Q', data, start + 16)[0]
        result['entries'].append(row)
    return result


def bank_source(source):
    require(0 <= source < BANK_SIZE, 'Unqualified desktop bank reference')
    return BANK_SIZE + source


def only_entry(table, kind, subprogram=None):
    entries = [entry for entry in table['entries'] if entry['type'] == kind and
               (subprogram is None or entry['subprogram'] == subprogram)]
    require(len(entries) == 1, 'Missing or ambiguous firmware component')
    return entries[0]


def decode_abl(data):
    """Check all twenty aggregate stages and their stored payload digests.

    A digest match checks content, not a cryptographic signature or execution.
    The aggregate manifest has twenty bounded (stage, offset) pairs.
    """
    require(len(data) >= 0x3a0 and data[0x7c:0x7f] == b'\x30\xff\0',
            'Unqualified ABL aggregate header')
    entries = [struct.unpack_from('<II', data, 0x100 + i * 8) for i in range(20)]
    require([entry[0] for entry in entries] == list(range(20)), 'ABL stage IDs')
    offsets = [entry[1] for entry in entries]
    require(offsets[0] == 0x3a0 and offsets == sorted(set(offsets)),
            'ABL manifest offsets')
    stages = []
    for index, offset in entries:
        header = span(data, offset, 0x100)
        unpacked = struct.unpack_from('<I', header, 0x50)[0]
        compressed = struct.unpack_from('<I', header, 0x54)[0]
        stored = struct.unpack_from('<I', header, 0x6c)[0]
        require(header[0x7c:0x7f] == bytes((0x30, index, 0)), 'ABL stage header')
        require(0 < unpacked <= 0x400000 and 0 < compressed <= stored - 0x100,
                'ABL stage compression extent')
        end = offsets[index + 1] if index + 1 < len(offsets) else len(data)
        require(offset + stored == end, 'ABL stage storage boundary')
        packed = span(data, offset + 0x100, compressed)
        decoder = zlib.decompressobj()
        decoded = decoder.decompress(packed, unpacked + 1)
        require(decoder.eof and not decoder.unused_data and
                not decoder.unconsumed_tail and len(decoded) == unpacked,
                'ABL bounded decompression')
        require(hashlib.sha256(decoded).digest() == header[0xd0:0xf0],
                'ABL payload digest differs')
        stages.append(dict(stage=index, offset=offset, stored_bytes=stored,
                           uncompressed_bytes=unpacked, sha256=digest(decoded),
                           header_payload_digest_valid=True,
                           header_tag=header[0x10:0x14].hex()))
    return stages


def microcode_header(data):
    require(len(data) >= 32, 'Truncated AMD microcode')
    patch = struct.unpack_from('<I', data, 4)[0]
    revision = struct.unpack_from('<H', data, 24)[0]
    require(patch and revision in MICROCODE_REVISIONS, 'Unexpected desktop microcode')
    return dict(processor_revision=revision, patch_id=patch,
                cbfs_name='cpu_microcode_%04x.bin' % revision,
                bytes=len(data), sha256=digest(data))


def microcode_inputs(contract, directory=None):
    """Bind explicit CBFS inputs separately from the preserved PSP cohort."""
    patches = [patch for family in contract['families'].values()
               for patch in family['microcode']]
    require(sorted(patch['processor_revision'] for patch in patches) ==
            list(MICROCODE_REVISIONS), 'Incomplete desktop microcode inputs')
    selected = []
    for patch in patches:
        if directory is None:
            selected.append(dict(patch))
            continue
        path = Path(directory) / patch['cbfs_name']
        data = path.read_bytes()
        row = microcode_header(data)
        require(row['processor_revision'] == patch['processor_revision'],
                'Desktop microcode processor binding')
        require(row['patch_id'] >= patch['patch_id'], 'Desktop microcode downgrade')
        selected.append(dict(row, input_file=str(path),
                             template_patch_id=patch['patch_id']))
    return selected


def audit(image, reference=None):
    require(len(image) == FLASH_SIZE, 'Expected full 32-MiB image')
    require(image[:BANK_SIZE] == image[BANK_SIZE:], 'Firmware bank mirror differs')
    if reference is not None:
        require(len(reference) == FLASH_SIZE, 'Reference image size')
    efs = span(image, BANK_SIZE + 0x20000, 0x40)
    require(efs[:4] == b'\xaa\x55\xaa\x55', 'Embedded firmware structure')
    combo = directory(image, bank_source(struct.unpack_from('<I', efs, 0x14)[0]),
                      b'2PSP')
    psp_route = [row for row in combo['entries'] if row['id'] == DESKTOP_PSP_ID]
    require(len(psp_route) == 1, 'Desktop PSP combo route')
    root = directory(image, bank_source(psp_route[0]['source']), b'$PSP')
    leaf = directory(image, bank_source(only_entry(root, 0x40)['source']), b'$PL2')
    # Matisse/Vermeer use the default BIOS2 entry; the BIOS3 combo routes APUs.
    bios = directory(image, bank_source(struct.unpack_from('<I', efs, 0x20)[0]),
                     b'$BHD')
    bios_leaf = directory(image, bank_source(only_entry(bios, 0x70)['source']), b'$BL2')
    tables = [root, leaf, bios, bios_leaf]
    preserved = []
    microcode = {}
    for table in tables:
        start, size = table['offset'], table['bytes']
        if reference is not None:
            require(span(reference, start, size) == span(image, start, size),
                    'Desktop directory changed from reference')
        for entry in table['entries']:
            kind, size = entry['type'], entry['bytes']
            if size in (0, 0xffffffff) or kind == 0x0b:
                continue  # Inline value or DRAM destination, not a flash buffer.
            start = bank_source(entry['source'])
            if kind == 0x62:
                require(entry['destination'] == 0x09bfffe0 and size == 0x400020,
                        'Desktop pre-x86 reset destination changed')
                # The directory size describes decompressed DRAM, not flash.
                continue  # Native packer replaces this shared x86 image.
            data = span(image, start, size)
            if reference is not None:
                require(data == span(reference, start, size),
                        'Desktop component changed from reference')
            preserved.append(dict(table=table['offset'], type=kind,
                                  subprogram=entry['subprogram'], offset=start,
                                  bytes=size, sha256=digest(data)))
            if kind == 0x66:
                row = dict(microcode_header(data), source=start)
                require(row['processor_revision'] not in microcode,
                        'Duplicate desktop microcode revision')
                microcode[row['processor_revision']] = row
    require(set(microcode) == set(MICROCODE_REVISIONS), 'Incomplete desktop microcode set')
    families = {}
    for name, policy in FAMILIES.items():
        subprogram = policy['subprogram']
        abl = only_entry(leaf, 0x30, subprogram)
        data = span(image, bank_source(abl['source']), abl['bytes'])
        stages = decode_abl(data)
        smu = only_entry(leaf, 0x08, subprogram)
        apcb = only_entry(bios_leaf, 0x60, subprogram)
        apcb_data = span(image, bank_source(apcb['source']), apcb['bytes'])
        require(apcb_data[:4] == b'APCB', 'Desktop APCB signature')
        length = struct.unpack_from('<I', apcb_data, 8)[0]
        require(32 <= length <= len(apcb_data) and sum(apcb_data[:length]) % 256 == 0,
                'Desktop APCB length/checksum')
        families[name] = dict(psp_id=DESKTOP_PSP_ID, subprogram=subprogram,
                              cpuid_models=policy['cpuid_models'], max_threads=32,
                              abl_source=bank_source(abl['source']),
                              abl_sha256=digest(data), abl_stages=stages,
                              smu_source=bank_source(smu['source']),
                              apcb_source=bank_source(apcb['source']),
                              apcb_declared_bytes=length, apcb_checksum_valid=True,
                              microcode=[microcode[r] for r in policy['microcode']],
                              stage_backend_linked=False, target_boot_qualified=False)
    return dict(schema=1, image_sha256=digest(image), image_bytes=len(image),
                combo=combo, directories=tables, components=preserved,
                families=families, reference_compared=reference is not None,
                reference_sha256=digest(reference) if reference is not None else None,
                shared_x86_reset_replacement_expected=True,
                signatures_or_hardware_execution_verified=False,
                legacy_1000_2000_route_qualified=False)


def vermeer_boot_microcode_slot(contract):
    """Identify the one qualified A212 input; never permit arbitrary component updates."""
    rows = [row for row in contract['families']['vermeer']['microcode']
            if row['processor_revision'] == 0xa212]
    require(len(rows) == 1, 'Missing original Vermeer boot microcode')
    row = rows[0]
    require(row['source'] == VERMEER_BOOT_UCODE_SOURCE and
            row['bytes'] == VERMEER_BOOT_UCODE_BYTES and
            row['patch_id'] == 0x0a201210 and row['sha256'] == VERMEER_BOOT_UCODE_OLD,
            'Unqualified original Vermeer boot microcode slot')
    return row


def vermeer_boot_microcode_updates(contract, selected):
    """Bind the literal F20 A21213 patch to the existing mirrored BIOS type66 slots."""
    slot = vermeer_boot_microcode_slot(contract)
    patches = [row for row in selected if row['processor_revision'] == 0xa212]
    require(len(patches) == 1, 'Missing selected Vermeer boot microcode')
    patch = patches[0]
    require(patch['patch_id'] == 0x0a201213 and
            patch['bytes'] == VERMEER_BOOT_UCODE_BYTES and
            patch['sha256'] == VERMEER_BOOT_UCODE_NEW and patch.get('input_file'),
            'Vermeer boot input requires the original qualified F20 A21213 patch')
    return [dict(offset=source, bytes=slot['bytes'], previous_sha256=slot['sha256'],
                 sha256=patch['sha256'], input_file=patch['input_file'])
            for source in (slot['source'] - BANK_SIZE, slot['source'])]


def vermeer_abl_updates(template, contract, path):
    """Prepare the literal original F20 unit and four exact routing changes.

    Requires the large payload layout: the compact payload occupies this unit's
    allocation. No header, signed stage, key, APCB or fuse policy is rewritten.
    """
    require(digest(template) == TEMPLATE_SHA256 and
            contract.get('image_sha256') == TEMPLATE_SHA256,
            'Vermeer ABL requires the qualified original template')
    data = Path(path).read_bytes()
    require(len(data) == VERMEER_ABL_BYTES and digest(data) == VERMEER_ABL_SHA256,
            'Vermeer ABL must be the complete original F20 signed unit')
    require(len(decode_abl(data)) == 20, 'Complete Vermeer ABL stage set')
    updates = []
    for bank in (0, BANK_SIZE):
        source = bank + VERMEER_ABL_SOURCE
        require(span(template, source, len(data)) == b'\xff' * len(data),
                'Vermeer ABL allocation is not erased template padding')
        updates.append(dict(offset=source, bytes=len(data), sha256=digest(data),
                            input_file=str(path), kind='original_Vermeer_ABL'))
        for relative, size, old_hash, new_hash in VERMEER_ABL_DIRECTORIES:
            offset = bank + relative
            original = span(template, offset, size)
            require(digest(original) == old_hash, 'Original Vermeer routing differs')
            table = bytearray(original)
            count = struct.unpack_from('<I', table, 8)[0]
            found = 0
            for index in range(count):
                position = 16 + 16 * index
                attributes, length, old_source = struct.unpack_from('<IIQ', table, position)
                if attributes != 0x130:
                    continue
                require(length == 510112 and old_source in (0x12e100, 0x56e700),
                        'Original Vermeer ABL route differs')
                struct.pack_into('<IQ', table, position + 4, len(data), VERMEER_ABL_SOURCE)
                found += 1
            require(found == 1, 'Ambiguous original Vermeer ABL route')
            struct.pack_into('<I', table, 4, fletcher32(table[8:]))
            require(digest(table) == new_hash, 'Vermeer routing update differs')
            updates.append(dict(offset=offset, bytes=size, sha256=new_hash,
                                data_hex=table.hex(), kind='Vermeer_ABL_directory'))
    return updates


def check_preserved(image, contract, vermeer_boot_microcode=False, vermeer_abl=False):
    require(contract.get('schema') == 1 and
            contract.get('image_sha256') == TEMPLATE_SHA256,
            'CPU firmware contract must describe the qualified template')
    slot = vermeer_boot_microcode_slot(contract) if vermeer_boot_microcode else None
    updated = 0
    for row in contract['directories'] + contract['components']:
        expected = row['sha256']
        if vermeer_abl:
            for offset, size, old_hash, new_hash in VERMEER_ABL_DIRECTORIES:
                if row['offset'] == BANK_SIZE + offset and row['bytes'] == size:
                    require(expected == old_hash, 'Original ABL directory contract differs')
                    expected = new_hash
            # Type40 references the 496-byte leaf plus 528 bytes of unchanged
            # padding. Bind the whole 1024 bytes, rather than masking its leaf.
            if row.get('type') == 0x40 and row['offset'] == 0x14d1000:
                require(row['bytes'] == 1024 and expected ==
                        'c8a1fe85e41c586f586f054c1420fb848c4c9d27fcc9be886e5c23c15f47475b',
                        'Original ABL leaf-reference contract differs')
                expected = VERMEER_ABL_DIRECTORY_REF_SHA256
        if slot and row['offset'] == slot['source']:
            require(row.get('type') == 0x66 and row.get('subprogram') == 0 and
                    row['bytes'] == slot['bytes'] and row['sha256'] == slot['sha256'],
                    'Vermeer boot update must match the original type66 component')
            expected = VERMEER_BOOT_UCODE_NEW
            updated += 1
        require(digest(span(image, row['offset'], row['bytes'])) == expected,
                'CPU-specific retained firmware differs after packing')
    if slot:
        require(updated == 1, 'Ambiguous Vermeer boot microcode update')
        require(digest(span(image, slot['source'] - BANK_SIZE, slot['bytes'])) ==
                VERMEER_BOOT_UCODE_NEW, 'Lower Vermeer boot microcode mirror differs')
    if vermeer_abl:
        for bank in (0, BANK_SIZE):
            require(digest(span(image, bank + VERMEER_ABL_SOURCE, VERMEER_ABL_BYTES)) ==
                    VERMEER_ABL_SHA256, 'Original Vermeer ABL allocation differs')
            for offset, size, _, expected in VERMEER_ABL_DIRECTORIES:
                require(digest(span(image, bank + offset, size)) == expected,
                        'Vermeer ABL routing mirror differs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--reference', type=Path)
    parser.add_argument('--extract-microcode', type=Path,
                        help='Explicit HOST extraction directory; never a flash target')
    args = parser.parse_args()
    image = args.image.read_bytes()
    result = audit(image, args.reference.read_bytes() if args.reference else None)
    if args.extract_microcode:
        require(digest(image) == TEMPLATE_SHA256, 'Extract only the qualified template')
        args.extract_microcode.mkdir(parents=True, exist_ok=True)
        for family in result['families'].values():
            for row in family['microcode']:
                target = args.extract_microcode / row['cbfs_name']
                data = span(image, row['source'], row['bytes'])
                if target.exists():
                    require(target.read_bytes() == data, 'Existing microcode differs')
                else:
                    target.write_bytes(data)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
