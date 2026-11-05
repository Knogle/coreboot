#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Check the physical payload slot and both complete firmware volumes."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct


def firmware_volume(image, start, size):
    assert image[start + 40:start + 44] == b'_FVH'
    assert struct.unpack_from('<Q', image, start + 32)[0] == size
    header_size = struct.unpack_from('<H', image, start + 48)[0]
    assert sum(struct.unpack_from('<%dH' % (header_size // 2), image, start)) % 65536 == 0
    position = (start + header_size + 7) & ~7
    files = []
    while position + 24 <= start + size:
        header = bytearray(image[position:position + 24])
        if header == b'\xff' * 24:
            assert image[position:start + size] == b'\xff' * (start + size - position)
            break
        count = int.from_bytes(header[20:23], 'little')
        assert 24 <= count <= start + size - position
        checksum = header[17]
        header[17] = header[23] = 0
        assert sum(header) % 256 == 0, hex(position)
        body = image[position + 24:position + count]
        if header[19] & 0x40:
            assert (sum(body) + checksum) % 256 == 0
        else:
            assert checksum == 0xaa
        files.append(dict(offset=hex(position), bytes=count, type=header[18],
                          body_SHA256=hashlib.sha256(body).hexdigest()))
        position = (position + count + 7) & ~7
    assert position <= start + size
    return files


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--obj', type=Path, required=True)
    parser.add_argument('--template', type=Path, required=True)
    parser.add_argument('--config', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    image = (args.obj / 'coreboot.rom').read_bytes()
    template = args.template.read_bytes()
    assert hashlib.sha256(template).hexdigest() == \
        'aa9cbc87e545fef85c2ed754c2ec3d5ef1755c07ff9a5cfc3ab858dc2e7cf86d'
    assert len(image) == len(template) == 0x2000000
    assert image[:0x1000000] == image[0x1000000:]
    config = set(args.config.read_text().splitlines())
    large = 'CONFIG_B565D4_LARGE_PAYLOAD=y' in config
    boot_microcode = 'CONFIG_B565D4_VERMEER_BOOT_MICROCODE=y' in config
    vermeer_abl = 'CONFIG_B565D4_VERMEER_ABL_F20=y' in config
    start, end, source = (0x1a5d508, 0x1d5f4d8, 0x1a5e000) if large else \
                        (0x1ebc888, 0x2000000, 0x1ec1000)
    regions = [(0, 0x1000), (0x1d60000, 0x1de1000),
               (0x1e01fd0, 0x1ebc888), (start, end)]
    regions += [(a - 0x1000000, z - 0x1000000) for a, z in regions[1:]]
    regions.append((0x1000000, 0x1001000))
    if boot_microcode:
        assert 'CONFIG_B565D4_VERMEER=y' in config
        assert 'CONFIG_B565D4_DESKTOP_CPU_PREPARATION=y' in config
        for a, z in ((0x6e1b00, 0x6e30c0), (0x16e1b00, 0x16e30c0)):
            assert hashlib.sha256(template[a:z]).hexdigest() == \
                'e86d516a8e72f3fe6e16bc62594017e96d3d0e6ba7ace368f755f013bb22f641'
            assert hashlib.sha256(image[a:z]).hexdigest() == \
                '72089265a025f253079dc1831d6c7b263f1bd20482b78ecb799b88a87569f6d0'
            assert struct.unpack_from('<I', image, a + 4)[0] == 0x0a201213
            assert struct.unpack_from('<H', image, a + 24)[0] == 0xa212
            regions.append((a, z))
    abl_allocations = []
    abl_headers = []
    if vermeer_abl:
        assert large and boot_microcode
        for bank in (0, 0x1000000):
            a, z = bank + 0xec1000, bank + 0xf3dec0
            assert template[a:z] == b'\xff' * (z - a)
            assert hashlib.sha256(image[a:z]).hexdigest() == \
                'f67f88a0bcf1949dae9c1f74cb9a889e4c645f83d9eb4ebe7bbd36b741862f5a'
            regions.append((a, z))
            abl_allocations.append((a, z))
            for relative, size, expected in (
                    (0xe9000, 400, 'a579bb67060f95ba6bc38e9495f3016a80dd465a97bbe17a53f5e632f11e6439'),
                    (0x4d1000, 496, '0ab9563f946c896940acfd90975dc5a9de9cef559e7d77e05262a059b28a5533')):
                a, z = bank + relative, bank + relative + size
                assert hashlib.sha256(image[a:z]).hexdigest() == expected
                regions.append((a, z))
            for offset in (0xebc888, 0xec1000 - 24, 0xf3dec0):
                a, z = bank + offset, bank + offset + 24
                assert template[a:z] == b'\xff' * 24
                regions.append((a, z))
                abl_headers.append((a, z))
    cursor = 0
    for a, z in sorted(regions):
        assert cursor <= a < z
        assert image[cursor:a] == template[cursor:a]
        cursor = z
    assert image[cursor:] == template[cursor:]
    board = args.obj / 'mainboard/asrockrack/b565d4_v1l'
    payload = (board / 'payload.self').read_bytes()
    stored = (len(payload) + 63) & ~63
    assert source + stored + 24 <= end
    assert image[source:source + len(payload)] == payload
    assert image[source + len(payload):source + stored] == b'\xff' * (stored - len(payload))
    contract = (board / 'payload_contract.h').read_text()
    assert int(re.search(r'#define NATIVE253_SPI_OFFSET (0x[0-9a-f]+)U', contract)[1], 16) == source
    assert int(re.search(r'#define NATIVE253_SPI_BYTES ([0-9]+)U', contract)[1]) == stored
    volumes = []
    for bank in (0, 0x1000000):
        for offset, size in ((0xa21000, 0x3df000), (0xe00000, 0x200000)):
            files = firmware_volume(image, bank + offset, size)
            volumes.append(dict(offset=hex(bank + offset), bytes=size, files=files))
        signature, major, minor, base, size, name, count = struct.unpack_from(
            '<8sBBQI32sH', image, bank)
        assert signature == b'__FMAP__' and (major, minor, base, size, count) == \
            (1, 1, 0, 0x2000000, 4)
        areas = [struct.unpack_from('<II32sH', image, bank + 56 + i * 42)
                 for i in range(count)]
        payload_area, = [row for row in areas if row[2].split(b'\0')[0] == b'PAYLOAD']
        assert payload_area[:2] == (source, end - source)
    payload_files = [row for volume in volumes for row in volume['files']
                     if int(row['offset'], 16) == source - 24]
    assert len(payload_files) == 1 and payload_files[0]['type'] == 1
    assert payload_files[0]['bytes'] == stored + 24
    if vermeer_abl:
        for bank in (0, 0x1000000):
            abl_file, = [row for volume in volumes for row in volume['files']
                         if int(row['offset'], 16) == bank + 0xec1000 - 24]
            assert abl_file['type'] == 1 and abl_file['bytes'] == 511680 + 24
            assert abl_file['body_SHA256'] == \
                'f67f88a0bcf1949dae9c1f74cb9a889e4c645f83d9eb4ebe7bbd36b741862f5a'
    if large:
        # The compact slot is preserved except for the explicitly qualified
        # original ABL allocation. It cannot also contain a compact payload.
        cursor = 0x1ebc888
        for a, z in sorted(abl_allocations + abl_headers):
            if a < cursor:
                continue
            assert image[cursor:a] == template[cursor:a]
            cursor = z
        assert image[cursor:] == template[cursor:]
    result = dict(status='PASS_HOST_PHYSICAL_PAYLOAD_LAYOUT', hardware_execution=False,
                  large_payload=large, image_SHA256=hashlib.sha256(image).hexdigest(),
                  original_vermeer_boot_microcode=boot_microcode,
                  original_vermeer_ABL=vermeer_abl,
                  payload_source=hex(source), payload_bytes=len(payload), SPI_read_bytes=stored,
                  storage_region=[hex(start), hex(end)], unchanged_outside_regions=True,
                  both_banks_identical=True, complete_firmware_volumes=volumes)
    (args.out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({key: value for key, value in result.items()
                      if key != 'complete_firmware_volumes'}))


if __name__ == '__main__':
    main()
