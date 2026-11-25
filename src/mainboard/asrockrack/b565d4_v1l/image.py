#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Build the B565D4 DRAM-first container from ordinary coreboot build outputs."""

import argparse
import hashlib
import json
import lzma
import os
import stat
import struct
import subprocess
import tempfile
import zlib
from pathlib import Path

import firmware_contract

FLASH_SIZE = firmware_contract.FLASH_SIZE
BANK_SIZE = firmware_contract.BANK_SIZE
RESET_START, RESET_END = 0x1e01fd0, 0x1ebc888
MEDIA_START, MEDIA_END = 0x1d60000, 0x1de1000
PAYLOAD_START, PAYLOAD_END = RESET_END, FLASH_SIZE
PAYLOAD_SOURCE = 0x1ec1000
# Three complete vendor FV_IMAGE files, unused by the native x86 flow.
# PSP and BIOS directory sources do not reference this span.
LARGE_PAYLOAD_START, LARGE_PAYLOAD_END = 0x1a5d508, 0x1d5f4d8
LARGE_PAYLOAD_SOURCE = 0x1a5e000
RESET_BASE, RESET_SIZE = 0x09bfffe0, 0x400020
TEMPLATE_SHA256 = firmware_contract.TEMPLATE_SHA256
RESET_SOURCES = (0x1e02000, 0x1e14000, 0x1e25300, 0x1e36600, 0x1e47900,
                 0x1e58c00, 0x1e69f00, 0x1e7b200, 0x1e8c500, 0x1e9d800)


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def run(argv):
    subprocess.run([str(x) for x in argv], check=True)


def update_file(path, data):
    path = Path(path)
    try:
        destination = path.lstat()
    except FileNotFoundError:
        mask = os.umask(0o777)
        os.umask(mask)
        mode = 0o666 & ~mask
    else:
        if stat.S_ISLNK(destination.st_mode):
            raise ValueError('Refusing symlink output')
        if not stat.S_ISREG(destination.st_mode):
            raise ValueError('Refusing non-regular output')
        if path.read_bytes() == data:
            return
        mode = stat.S_IMODE(destination.st_mode)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode='wb', prefix='.' + path.name + '.',
                                         dir=path.parent, delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
            stream.flush()
            os.fchmod(stream.fileno(), mode)
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            try:
                temporary.unlink()
            except FileNotFoundError:
                pass


def cbfs_entries(image):
    position = 0x1000
    while position + 24 <= len(image):
        require(image[position:position + 8] == b'LARCHIVE', 'CBFS record signature')
        size, kind, attributes, offset = struct.unpack_from('>4I', image, position + 8)
        require(offset >= 24 and position + offset + size <= len(image), 'CBFS extent')
        name = image[position + 24:position + offset].split(b'\0', 1)[0].decode()
        yield dict(name=name, kind=kind, header=position, data=position + offset,
                   size=size, attributes=position + attributes if attributes else 0)
        position = (position + offset + size + 63) & ~63


def payload_layout(large):
    if large:
        return LARGE_PAYLOAD_START, LARGE_PAYLOAD_END, LARGE_PAYLOAD_SOURCE
    return PAYLOAD_START, PAYLOAD_END, PAYLOAD_SOURCE


def payload_command(args):
    _, payload_end, payload_source = payload_layout(args.large_payload)
    source = Path(args.input).read_bytes()
    # Reuse a standard coreboot SELF without wrapping it as a second payload.
    if source[:4] in (b'CODE', b'DATA', b'BSS '):
        update_file(args.output, source)
    else:
        with tempfile.TemporaryDirectory(prefix='b565d4-payload-') as temporary:
            image = Path(temporary) / 'payload.cbfs'
            run([args.cbfstool, image, 'create', '-m', 'x86',
                 '-s', '0x400000' if args.large_payload else '0x200000'])
            run([args.cbfstool, image, 'add-payload', '-f', args.input,
                 '-n', 'payload', '-c', 'lzma'])
            run([args.cbfstool, image, 'extract', '-n', 'payload', '-U', '-f', args.output])
    data = Path(args.output).read_bytes()
    position = 0
    segments = []
    entry = None
    while position + 28 <= len(data):
        kind, compression, offset, address, size, memory = struct.unpack_from('>IIIQII', data, position)
        position += 28
        if kind == 0x454e5452:  # ENTRY
            entry = address
            break
        require(kind in (0x434f4445, 0x44415441, 0x42535320), 'Unsupported SELF segment')
        require(memory and address + memory <= 0x100000000, 'IA32 SELF memory extent')
        if kind == 0x42535320:
            decoded = b''
        else:
            require(offset + size <= len(data), 'SELF stored extent')
            stored = data[offset:offset + size]
            require(compression in (0, 1), 'SELF compression must be NONE or LZMA')
            decoded = lzma.decompress(stored, format=lzma.FORMAT_ALONE) if compression else stored
        require(len(decoded) <= memory, 'SELF initialized size exceeds memory size')
        decoded += b'\0' * (memory - len(decoded))
        segments.append((address, memory, zlib.crc32(decoded)))
    require(entry is not None and segments and len(segments) <= 32, 'Missing SELF entry')
    require(any(address <= entry < address + size for address, size, _ in segments), 'SELF entry extent')
    read_bytes = (len(data) + 63) & ~63
    require(payload_source + read_bytes <= payload_end - 24, 'Payload does not fit its FFS region')
    header = ['/* SPDX-License-Identifier: GPL-2.0-only */',
              '/* Generated from the selected standard coreboot SELF payload. */',
              '#ifndef B565D4_PAYLOAD_CONTRACT_H', '#define B565D4_PAYLOAD_CONTRACT_H',
              '#define SELF_BYTES %dU' % len(data), '#define SELF_ENTRY 0x%xU' % entry,
              '#define NATIVE253_SPI_OFFSET 0x%xU' % payload_source,
              '#define NATIVE253_SPI_BYTES %dU' % read_bytes,
              '#define NATIVE253_SELF_CRC32 0x%08xU' % zlib.crc32(data),
              'static const uint8_t b565d4_self_header[] __attribute__((unused)) = {',
              ','.join('0x%02x' % value for value in data[:position]), '};',
              'static const struct { uint32_t address, bytes, crc32; } b565d4_payload_segments[] __attribute__((unused)) = {']
    header += ['{ 0x%xU, %dU, 0x%08xU },' % segment for segment in segments]
    header += ['};', '#endif', '']
    update_file(args.header, '\n'.join(header).encode())


def bridge_command(args):
    data = Path(args.input).read_bytes()
    require(len(data) == 77, 'IA32 context bridge size changed')
    header = ['/* SPDX-License-Identifier: GPL-2.0-only */',
              '/* Generated from the persistent IA32 context/fault assembly. */',
              '#ifndef B565D4_BRIDGE_BYTES_H', '#define B565D4_BRIDGE_BYTES_H',
              '#include <stdint.h>',
              'static const uint8_t native99_bridge[] __attribute__((aligned(16))) = {',
              ','.join('0x%02x' % value for value in data), '};', '#endif', '']
    update_file(args.header, '\n'.join(header).encode())


def medium_command(args):
    image = bytearray(Path(args.input).read_bytes())
    require(len(image) == 0x80000, 'Later medium size')
    entries = list(cbfs_entries(image))
    master, = [row for row in entries if row['name'] == 'cbfs_master_header']
    pointer, = [row for row in entries if row['name'] == 'header_pointer']
    require(master['size'] == 32 and pointer['size'] == 4 and
            pointer['data'] == len(image) - 4, 'Legacy CBFS header placement')
    magic, version, size, bootblock, align, offset = struct.unpack_from('>6I', image, master['data'])
    require(magic == 0x4f524243 and size == len(image) and align == 64 and
            offset == 0x1000, 'Legacy CBFS header contract')
    # The intermediate's absolute flash pointer cannot describe its RAM copy.
    # SeaBIOS supports a signed offset relative to CONFIG_CBFS_LOCATION.
    struct.pack_into('<i', image, pointer['data'], master['data'] - len(image))
    update_file(args.output, image)


def memory_command(args):
    image = Path(args.input).read_bytes()
    require(len(image) == 0x80000, 'Later medium size')
    record, = [row for row in cbfs_entries(image) if row['name'] == args.prefix + '/ramstage']
    attributes = record['attributes']
    stage = compression = None
    while attributes and attributes < record['data']:
        tag, size = struct.unpack_from('>II', image, attributes)
        require(size >= 8 and attributes + size <= record['data'], 'CBFS attribute extent')
        if tag == 0x53746748:
            stage = struct.unpack_from('>QII', image, attributes + 8)
        if tag == 0x42435a4c:
            compression = struct.unpack_from('>II', image, attributes + 8)
        attributes += size
    require(stage and compression and compression[0] == 1, 'Expected compressed rmodule stage')
    stored = image[record['data']:record['data'] + record['size']]
    decoded = lzma.decompress(stored, format=lzma.FORMAT_ALONE)
    require(len(decoded) == compression[1] and len(decoded) >= 64, 'Rmodule decoded extent')
    magic, version, kind, *fields = struct.unpack_from('<HBB15I', decoded)
    require((magic, version, kind) == (0xf8fe, 1, 0), 'Rmodule ABI')
    payload_begin, payload_end, reloc_begin, reloc_end, link, program, entry, params, params_end, bss, bss_end = fields[:11]
    require(entry == link and payload_begin == 64 and payload_end <= len(decoded), 'Rmodule placement')
    require(link <= params <= params_end <= link + payload_end - payload_begin, 'Rmodule parameter span')
    params -= link
    params_end -= link
    require(params_end - params == 56 and params_end <= payload_end - payload_begin, 'Descriptor parameter ABI')
    require(stage == (0, 0, program + 64), 'CBFS stage memory ABI')
    values = dict(RAM_STORED_BYTES=record['size'], RAM_SERIALIZED_BYTES=len(decoded),
                  RAM_STAGE_MEMORY=stage[2], RAM_PROGRAM_BYTES=program,
                  RAM_REGION_BYTES=(program + 64 + 4096 + 4095) & ~4095,
                  RAM_PARAMETERS_OFFSET=params, RAM_INITIALIZED_BYTES=payload_end - payload_begin,
                  RAM_CBFS_DATA_OFFSET=record['data'])
    header = ['/* SPDX-License-Identifier: GPL-2.0-only */',
              '/* Generated from the complete later CBFS and its actual rmodule. */',
              '#ifndef B565D4_RAMSTAGE_CONTRACT_H', '#define B565D4_RAMSTAGE_CONTRACT_H']
    header += ['#define %s 0x%xU' % pair for pair in values.items()]
    header += ['#define B565D4_MEDIA_SHA256 { ' + ','.join('0x%02x' % value for value in hashlib.sha256(image).digest()) + ' }', '#endif', '']
    update_file(args.header, '\n'.join(header).encode())


def elf_load(path):
    data = Path(path).read_bytes()
    require(data[:4] == b'\x7fELF' and data[5] == 1, 'Little-endian ELF required')
    if data[4] == 1:
        entry, phoff = struct.unpack_from('<II', data, 24)
        phsize, count = struct.unpack_from('<HH', data, 42)
        rows = [struct.unpack_from('<8I', data, phoff + index * phsize) for index in range(count)]
        loads = [(row[3], data[row[1]:row[1] + row[4]], row[5]) for row in rows if row[0] == 1 and row[4]]
    else:
        require(data[4] == 2, 'ELF class')
        entry, phoff = struct.unpack_from('<QQ', data, 24)
        phsize, count = struct.unpack_from('<HH', data, 54)
        rows = [struct.unpack_from('<II6Q', data, phoff + index * phsize) for index in range(count)]
        loads = [(row[4], data[row[2]:row[2] + row[5]], row[6]) for row in rows if row[0] == 1 and row[5]]
    require(len(loads) == 1, 'Exactly one initialized PSP-stage LOAD required')
    return entry, loads[0]


def set24(data, offset, value):
    require(24 <= value < 0xffffff, 'FFS size')
    data[offset:offset + 3] = value.to_bytes(3, 'little')


def ffs_header(header, body):
    header = bytearray(header)
    set24(header, 20, len(header) + len(body))
    header[17] = -sum(body) & 255 if header[19] & 0x40 else 0xaa
    header[16] = 0
    temporary = bytearray(header)
    temporary[17] = temporary[23] = 0
    header[16] = -sum(temporary) & 255
    return bytes(header) + body


def pad_file(size):
    require(size >= 24 and size % 8 == 0, 'FFS padding extent')
    header = bytearray(b'\xff' * 24)
    header[17:20] = bytes((0xaa, 0xf0, 0))
    header[23] = 0xf8
    return ffs_header(header, b'\xff' * (size - 24))


def full_fmap(payload_source=PAYLOAD_SOURCE, payload_end=PAYLOAD_END):
    areas = [('BIOS', 0, FLASH_SIZE), ('FMAP', 0, 0x1000),
             ('COREBOOT', 0x1d62000, 0x7f000),
             ('PAYLOAD', payload_source, payload_end - payload_source)]
    result = struct.pack('<8sBBQI32sH', b'__FMAP__', 1, 1, 0, FLASH_SIZE, b'B565D4_FLASH', len(areas))
    return result + b''.join(struct.pack('<II32sH', offset, size, name.encode(), 0)
                             for name, offset, size in areas)


def pack_command(args):
    payload_start, payload_end, payload_source = payload_layout(args.large_payload)
    template = Path(args.template).read_bytes()
    require(len(template) == FLASH_SIZE and sha256(template) == TEMPLATE_SHA256, 'Qualified firmware template identity')
    require(template[:BANK_SIZE] == template[BANK_SIZE:], 'Mirrored firmware template')
    require(template[:0x1000] == b'\xff' * 0x1000, 'Physical FMAP padding must be unused')
    cpu_contract = None
    if args.cpu_contract:
        cpu_contract = json.loads(Path(args.cpu_contract).read_text())
        firmware_contract.check_preserved(template, cpu_contract)
    require(not args.desktop_microcode or cpu_contract,
            'Desktop microcode requires the retained CPU firmware contract')
    require(not args.desktop_microcode_dir or args.desktop_microcode,
            'Desktop microcode directory requires desktop preparation')
    require(args.native_backend == 'cezanne' or args.desktop_microcode,
            'Desktop runtime requires the desktop firmware inputs')
    require(bool(args.vermeer_boot_microcode) == bool(args.vermeer_abl),
            'Original Vermeer boot13 and matching ABL must be supplied together')
    selected_microcode = (firmware_contract.microcode_inputs(cpu_contract,
                          args.desktop_microcode_dir) if args.desktop_microcode else [])
    require(args.native_backend != 'vermeer' or args.vermeer_abl or
            not any(patch['processor_revision'] == 0xa212 and
                    patch['patch_id'] == 0x0a201213 for patch in selected_microcode),
            'Original Vermeer A21213 CBFS input requires the paired F20 ABL cohort')
    require(not args.vermeer_boot_microcode or
            (args.native_backend == 'vermeer' and args.desktop_microcode),
            'Vermeer boot microcode requires the matching desktop backend and CBFS inputs')
    boot_microcode = (firmware_contract.vermeer_boot_microcode_updates(
                     cpu_contract, selected_microcode) if args.vermeer_boot_microcode else [])
    require(not args.vermeer_abl or (args.native_backend == 'vermeer' and
            args.vermeer_boot_microcode and args.large_payload),
            'Original Vermeer ABL requires Vermeer, original boot13 and large payload layout')
    abl_updates = (firmware_contract.vermeer_abl_updates(template, cpu_contract,
                   args.vermeer_abl) if args.vermeer_abl else [])
    bb_entry, (bb_address, bb, bb_memory) = elf_load(args.bootblock)
    rom_entry, (rom_address, rom, rom_memory) = elf_load(args.romstage)
    require(0x09fe0000 <= bb_address < 0x0a000000 and bb_address + len(bb) == 0x0a000000, 'Bootblock reset placement')
    require(rom_address == 0x09d00000 and rom_memory <= 0xa0000, 'Romstage placement')
    require(0x09c00000 <= bb_entry < 0x0a000000 and rom_address <= rom_entry < rom_address + len(rom), 'Stage entries')
    medium = Path(args.medium).read_bytes()
    require(len(medium) == 0x80000, 'Later medium extent')
    cmos_options = {}
    early_microcode = {}
    with tempfile.TemporaryDirectory(prefix='b565d4-reset-') as temporary:
        early = Path(temporary) / 'early.cbfs'
        run([args.cbfstool, early, 'create', '-M', args.fmap, '-r', 'COREBOOT'])
        run([args.cbfstool, early, 'add-stage', '-f', args.romstage,
             '-n', args.prefix + '/romstage', '-c', 'none'])
        if args.native_backend == 'matisse':
            # The B0 bootblock timing comparison uses the same qualified patch
            # as the later CBFS. PSP already supplies this early medium in DRAM.
            name = 'cpu_microcode_8710.bin'
            patch, = [entry for entry in selected_microcode if entry['cbfs_name'] == name]
            record, = [entry for entry in cbfs_entries(medium) if entry['name'] == name]
            data = medium[record['data']:record['data'] + record['size']]
            require(record['kind'] == 0x53 and len(data) == patch['bytes'] and
                    sha256(data) == patch['sha256'], 'Qualified early Matisse microcode')
            path = Path(temporary) / name
            path.write_bytes(data)
            run([args.cbfstool, early, 'add', '-n', name, '-f', path,
                 '-t', 'microcode', '-a', '16'])
            early_microcode[name] = dict(bytes=len(data), sha256=sha256(data))
        if args.cmos_options:
            # Generic bootblock CMOS sanitization and early option reads use
            # the PSP-loaded medium. Keep its layout/defaults identical to
            # those in the later medium used by ramstage and the payload.
            for name, kind in [('cmos_layout.bin', 'cmos_layout'),
                               ('cmos.default', 'cmos_default')]:
                path = Path(temporary) / name
                run([args.cbfstool, args.medium, 'extract', '-n', name, '-f', path])
                data = path.read_bytes()
                require(0 < len(data) <= 0x1000, 'Bounded CMOS option data')
                run([args.cbfstool, early, 'add', '-n', name, '-f', path, '-t', kind])
                cmos_options[name] = dict(bytes=len(data), sha256=sha256(data))
        early_bytes = early.read_bytes()
    require(len(early_bytes) == 0x80000, 'Early logical medium size')
    reset = bytearray(RESET_SIZE)
    reset[0x10020:0x90020] = early_bytes
    reset[bb_address - RESET_BASE:bb_address - RESET_BASE + len(bb)] = bb
    packed = zlib.compress(reset, 1)
    require(zlib.decompress(packed) == reset and len(packed) + 0x100 <= 0x11300, 'PSP reset compression extent')
    output = bytearray(template)
    main_prefix = bytearray(template[RESET_START:0x1e02100])
    struct.pack_into('<I', main_prefix, 0x1e02014 - RESET_START, len(packed))
    set24(main_prefix, 24, len(main_prefix) - 24 + len(packed))
    main_file = ffs_header(main_prefix[:24], bytes(main_prefix[24:]) + packed)
    aligned = (len(main_file) + 7) & ~7
    output[RESET_START:RESET_END] = main_file + b'\xff' * (aligned - len(main_file)) + pad_file(RESET_END - RESET_START - aligned)
    amd_header = bytearray(template[0x1e02000:0x1e02100])
    struct.pack_into('<I', amd_header, 0x14, len(packed))
    for source in RESET_SOURCES[1:]:
        output[source:source + 0x11300] = amd_header + packed + b'\xff' * (0x11200 - len(packed))
    header = template[0x1d60fe8:0x1d61000]
    output[0x1d60fe8:MEDIA_END] = ffs_header(header, medium)
    payload = Path(args.payload).read_bytes()
    payload += b'\xff' * ((-len(payload)) & 63)
    require(payload_source + len(payload) + 24 <= payload_end, 'Payload region extent')
    payload_header = bytearray(header)
    payload_header[:16] = hashlib.sha256(b'B565D4 coreboot payload' + payload).digest()[:16]
    payload_file = ffs_header(payload_header, payload)
    output[payload_start:payload_end] = (pad_file(payload_source - 24 - payload_start) +
                                        payload_file + pad_file(payload_end - payload_source - len(payload)))
    physical_map = full_fmap(payload_source, payload_end)
    output[:len(physical_map)] = physical_map
    for patch in boot_microcode:
        data = Path(patch['input_file']).read_bytes()
        require(len(data) == patch['bytes'] and sha256(data) == patch['sha256'],
                'Selected Vermeer boot microcode changed while packing')
        output[patch['offset']:patch['offset'] + patch['bytes']] = data
    for patch in abl_updates:
        data = (bytes.fromhex(patch['data_hex']) if 'data_hex' in patch else
                Path(patch['input_file']).read_bytes())
        require(len(data) == patch['bytes'] and sha256(data) == patch['sha256'],
                'Original Vermeer ABL input changed while packing')
        output[patch['offset']:patch['offset'] + patch['bytes']] = data
    abl_storage_headers = []
    if abl_updates:
        # Keep the complete vendor firmware volumes well formed. The original
        # compact slot is an erased FV tail, not a pre-existing FFS pad file.
        # Place the signed unit inside a RAW file without changing its bytes or
        # its PSP directory source; cover the remaining tail with pad files.
        data = Path(args.vermeer_abl).read_bytes()
        require(len(data) == firmware_contract.VERMEER_ABL_BYTES and
                sha256(data) == firmware_contract.VERMEER_ABL_SHA256,
                'Original Vermeer ABL changed before FFS storage')
        source = BANK_SIZE + firmware_contract.VERMEER_ABL_SOURCE
        abl_header = bytearray(header)
        abl_header[:16] = hashlib.sha256(b'B565D4 original Vermeer F20 ABL' + data).digest()[:16]
        output[PAYLOAD_START:PAYLOAD_END] = (
            pad_file(source - 24 - PAYLOAD_START) + ffs_header(abl_header, data) +
            pad_file(PAYLOAD_END - source - len(data)))
        output[PAYLOAD_START - BANK_SIZE:PAYLOAD_END - BANK_SIZE] = output[PAYLOAD_START:PAYLOAD_END]
        for bank in (0, BANK_SIZE):
            for offset in (PAYLOAD_START - BANK_SIZE, source - BANK_SIZE - 24,
                           source - BANK_SIZE + len(data)):
                offset += bank
                require(template[offset:offset + 24] == b'\xff' * 24,
                        'Vermeer ABL FFS header is not erased template padding')
                abl_storage_headers.append((offset, offset + 24))
    for begin, end in [(MEDIA_START, MEDIA_END), (RESET_START, RESET_END),
                       (payload_start, payload_end), (BANK_SIZE, BANK_SIZE + 0x1000)]:
        if begin == BANK_SIZE:
            output[begin:end] = output[:0x1000]
        else:
            output[begin - BANK_SIZE:end - BANK_SIZE] = output[begin:end]
    allowed = [(0, 0x1000), (BANK_SIZE, BANK_SIZE + 0x1000),
               (MEDIA_START, MEDIA_END), (RESET_START, RESET_END), (payload_start, payload_end),
               (MEDIA_START - BANK_SIZE, MEDIA_END - BANK_SIZE),
               (RESET_START - BANK_SIZE, RESET_END - BANK_SIZE),
               (payload_start - BANK_SIZE, payload_end - BANK_SIZE)]
    allowed += [(patch['offset'], patch['offset'] + patch['bytes'])
                for patch in boot_microcode]
    allowed += [(patch['offset'], patch['offset'] + patch['bytes'])
                for patch in abl_updates]
    allowed += abl_storage_headers
    cursor = 0
    for begin, end in sorted(allowed):
        require(output[cursor:begin] == template[cursor:begin], 'Change outside x86 and FMAP regions')
        cursor = end
    require(output[cursor:] == template[cursor:], 'Change beyond allowed regions')
    require(output[:BANK_SIZE] == output[BANK_SIZE:], 'Final bank mirrors')
    if cpu_contract:
        firmware_contract.check_preserved(output, cpu_contract, args.vermeer_boot_microcode,
                                          bool(abl_updates))
    if args.desktop_microcode:
        entries = list(cbfs_entries(medium))
        for patch in selected_microcode:
            found = [row for row in entries if row['name'] == patch['cbfs_name']]
            require(len(found) == 1, 'Missing desktop microcode CBFS file')
            row = found[0]
            data = medium[row['data']:row['data'] + row['size']]
            require(row['kind'] == 0x53 and len(data) == patch['bytes'] and
                    sha256(data) == patch['sha256'], 'Desktop microcode CBFS bytes')
    for source in RESET_SOURCES:
        count = struct.unpack_from('<I', output, source + 0x14)[0]
        require(zlib.decompress(output[source + 0x100:source + 0x100 + count]) == reset, 'Reset stream readback')
    update_file(args.output, output)
    report = dict(template_sha256=sha256(template), image_sha256=sha256(output),
                  medium_sha256=sha256(medium), payload_sha256=sha256(Path(args.payload).read_bytes()),
                  reset_decompressed_sha256=sha256(reset), reset_streams=len(RESET_SOURCES),
                  early_microcode=early_microcode,
                  bootblock_entry=hex(bb_entry), romstage_entry=hex(rom_entry),
                  payload_slot=[hex(payload_start), hex(payload_end)],
                  payload_spi_source=hex(payload_source), large_payload=args.large_payload,
                  allowed_changes=allowed, all_other_template_bytes_preserved=True,
                  stage_inputs_built_from_source=True, hardware_execution_proven=False)
    if cmos_options:
        report['cmos_options'] = dict(early_and_later_identical=True, files=cmos_options)
    if cpu_contract:
        report['desktop_cpu_preparation'] = dict(
            firmware_contract_sha256=sha256(Path(args.cpu_contract).read_bytes()),
            retained_families=list(cpu_contract['families']),
            cpu_specific_firmware_preserved=not bool(boot_microcode or abl_updates),
            signed_PSP_ABL_APCB_components_preserved=not bool(abl_updates),
            all_other_PSP_code_key_policy_preserved=True,
            vermeer_abl_updates=abl_updates,
            vermeer_abl_FFS_header_extents=abl_storage_headers,
            vermeer_boot_microcode_updates=boot_microcode,
            desktop_microcode_in_cbfs=args.desktop_microcode,
            desktop_microcode_inputs=selected_microcode,
            linked_soc_backend=args.native_backend, other_cpu_boot_qualified=False)
    update_file(args.report, (json.dumps(report, indent=2) + '\n').encode())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    payload = commands.add_parser('payload')
    for name in ['cbfstool', 'input', 'output', 'header']:
        payload.add_argument('--' + name, required=True)
    payload.add_argument('--large-payload', action='store_true')
    payload.set_defaults(function=payload_command)
    bridge = commands.add_parser('bridge')
    bridge.add_argument('--input', required=True)
    bridge.add_argument('--header', required=True)
    bridge.set_defaults(function=bridge_command)
    medium = commands.add_parser('medium')
    medium.add_argument('--input', required=True)
    medium.add_argument('--output', required=True)
    medium.set_defaults(function=medium_command)
    memory = commands.add_parser('memory')
    for name in ['input', 'header', 'prefix']:
        memory.add_argument('--' + name, required=True)
    memory.set_defaults(function=memory_command)
    pack = commands.add_parser('pack')
    for name in ['template', 'bootblock', 'romstage', 'medium', 'payload', 'fmap',
                 'cbfstool', 'prefix', 'output', 'report']:
        pack.add_argument('--' + name, required=True)
    pack.add_argument('--large-payload', action='store_true')
    pack.add_argument('--cpu-contract')
    pack.add_argument('--desktop-microcode', action='store_true')
    pack.add_argument('--desktop-microcode-dir')
    pack.add_argument('--vermeer-boot-microcode', action='store_true',
                      help='Use the exact original A21213 patch in existing BIOS type66 slots')
    pack.add_argument('--vermeer-abl',
                      help='Complete original F20 Vermeer ABL unit; exact hash and layout required')
    pack.add_argument('--native-backend', choices=('cezanne', 'matisse', 'vermeer'), default='cezanne')
    pack.add_argument('--cmos-options', action='store_true')
    pack.set_defaults(function=pack_command)
    args = parser.parse_args()
    args.function(args)


if __name__ == '__main__':
    main()
