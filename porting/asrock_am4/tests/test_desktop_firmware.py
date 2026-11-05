#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Exercise real retained CPU firmware and malformed directory/ABL inputs."""

import copy
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
BOARD = ROOT / 'src/mainboard/asrockrack/b565d4_v1l'
FIXTURE = ROOT / '3rdparty/blobs/mainboard/asrockrack/b565d4_v1l/pre-x86-template.rom'
MODERN_UCODE = FIXTURE.parent / 'desktop-microcode-vermeer/cpu_microcode_a212.bin'
MODERN_ABL = FIXTURE.parent / 'vermeer-f20-abl.bin'
spec = importlib.util.spec_from_file_location('firmware_contract', BOARD / 'firmware_contract.py')
fw = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fw)


@unittest.skipUnless(FIXTURE.exists(), 'requires the private qualified board template')
class DesktopFirmwareTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.image = FIXTURE.read_bytes()
        assert fw.digest(cls.image) == fw.TEMPLATE_SHA256
        cls.contract = fw.audit(cls.image)

    def mirrored_change(self, offset, data):
        image = bytearray(self.image)
        image[offset:offset + len(data)] = data
        alias = offset - fw.BANK_SIZE
        image[alias:alias + len(data)] = data
        return image

    def fix_directory(self, image, offset):
        count = struct.unpack_from('<I', image, offset + 8)[0]
        end = offset + 16 + count * 16
        struct.pack_into('<I', image, offset + 4, fw.fletcher32(image[offset + 8:end]))
        image[offset - fw.BANK_SIZE:end - fw.BANK_SIZE] = image[offset:end]
        return image

    def test_actual_family_routes_and_stages(self):
        self.assertEqual(set(self.contract['families']), {'matisse', 'vermeer'})
        for family, subprogram, apcb_bytes in [('matisse', 0, 2684), ('vermeer', 1, 472)]:
            row = self.contract['families'][family]
            self.assertEqual(row['psp_id'], 0xbc0b0500)
            self.assertEqual(row['subprogram'], subprogram)
            self.assertEqual(len(row['abl_stages']), 20)
            self.assertEqual(row['apcb_declared_bytes'], apcb_bytes)
            self.assertEqual(row['max_threads'], 32)
            self.assertFalse(row['stage_backend_linked'])
            self.assertFalse(row['target_boot_qualified'])
        self.assertFalse(self.contract['signatures_or_hardware_execution_verified'])
        self.assertFalse(self.contract['legacy_1000_2000_route_qualified'])

    def test_real_microcode_processor_bindings(self):
        expected = {
            0x00870f00: (0x8700, 0x08700004),
            0x00870f10: (0x8710, 0x08701034),
            0x00a20f00: (0xa200, 0x0a200025),
            0x00a20f10: (0xa210, 0x0a20102b),
            0x00a20f12: (0xa212, 0x0a201210),
        }
        patches = {r['processor_revision']: r for f in self.contract['families'].values()
                   for r in f['microcode']}
        for signature, (revision, patch) in expected.items():
            # Formula used by the unchanged common AMD microcode loader.
            self.assertEqual(((signature & 0xff0000) >> 8) | (signature & 0xff), revision)
            self.assertEqual(patches[revision]['patch_id'], patch)
            self.assertEqual(patches[revision]['cbfs_name'], 'cpu_microcode_%04x.bin' % revision)

    def microcode_directory(self, directory):
        for family in self.contract['families'].values():
            for patch in family['microcode']:
                data = self.image[patch['source']:patch['source'] + patch['bytes']]
                (directory / patch['cbfs_name']).write_bytes(data)

    def test_explicit_original_microcode_directory(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.microcode_directory(directory)
            default = fw.microcode_inputs(self.contract)
            selected = fw.microcode_inputs(self.contract, directory)
            self.assertEqual([(r['cbfs_name'], r['sha256']) for r in selected],
                             [(r['cbfs_name'], r['sha256']) for r in default])

    def original13_image(self):
        selected = fw.microcode_inputs(self.contract, MODERN_UCODE.parent)
        updates = fw.vermeer_boot_microcode_updates(self.contract, selected)
        self.assertEqual([row['offset'] for row in updates], [0x6e1b00, 0x16e1b00])
        data = bytearray(self.image)
        patch = MODERN_UCODE.read_bytes()
        for row in updates:
            self.assertEqual(row['bytes'], len(patch))
            data[row['offset']:row['offset'] + row['bytes']] = patch
        return data

    def original_abl_image(self):
        data = self.original13_image()
        updates = fw.vermeer_abl_updates(self.image, self.contract, MODERN_ABL)
        self.assertEqual(len(updates), 6)
        for row in updates:
            block = (bytes.fromhex(row['data_hex']) if 'data_hex' in row else
                     Path(row['input_file']).read_bytes())
            data[row['offset']:row['offset'] + row['bytes']] = block
        return data

    @unittest.skipUnless(MODERN_ABL.exists() and MODERN_UCODE.exists(),
                         'requires original qualified Vermeer ABL and A21213')
    def test_original_abl_exact_update_and_default_guard(self):
        data = self.original_abl_image()
        with self.assertRaisesRegex(ValueError, 'retained firmware'):
            fw.check_preserved(data, self.contract, True)
        fw.check_preserved(data, self.contract, True, True)
        # Reparse actual routes independently of the preservation checker.
        updated = fw.audit(data)
        family = updated['families']['vermeer']
        self.assertEqual(family['abl_sha256'], fw.VERMEER_ABL_SHA256)
        self.assertEqual(family['abl_source'], fw.BANK_SIZE + fw.VERMEER_ABL_SOURCE)
        self.assertEqual(len(family['abl_stages']), 20)
        self.assertEqual(updated['families']['matisse'], self.contract['families']['matisse'])

    @unittest.skipUnless(MODERN_ABL.exists() and MODERN_UCODE.exists(),
                         'requires original qualified Vermeer ABL and A21213')
    def test_original_abl_corruption_routes_padding_and_other_firmware(self):
        original = self.original_abl_image()
        # Each directory copy, both complete signed units, the declared type40
        # padding, retained old ABL bytes and another firmware component matter.
        offsets = [0xe9004, 0x10e9004, 0x4d1004, 0x14d1004,
                   0xec1100, 0x1ec1100, 0x14d1200, 0x156e800]
        offsets.append(self.contract['families']['vermeer']['smu_source'] + 256)
        for offset in offsets:
            with self.subTest(offset=hex(offset)):
                damaged = bytearray(original)
                damaged[offset] ^= 1
                with self.assertRaises(ValueError):
                    fw.check_preserved(damaged, self.contract, True, True)

    @unittest.skipUnless(MODERN_ABL.exists(), 'requires original qualified Vermeer ABL')
    def test_original_abl_rejects_modified_or_partial_signed_input(self):
        unit = MODERN_ABL.read_bytes()
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'abl.bin'
            for bad in (unit[:-1], unit + b'\0', unit[:-1] + bytes([unit[-1] ^ 1])):
                path.write_bytes(bad)
                with self.assertRaisesRegex(ValueError, 'complete original'):
                    fw.vermeer_abl_updates(self.image, self.contract, path)

    @unittest.skipUnless(MODERN_UCODE.exists(), 'requires original qualified A21213 input')
    def test_explicit_original13_update_keeps_default_guard(self):
        data = self.original13_image()
        with self.assertRaisesRegex(ValueError, 'retained firmware'):
            fw.check_preserved(data, self.contract)
        fw.check_preserved(data, self.contract, vermeer_boot_microcode=True)

    @unittest.skipUnless(MODERN_UCODE.exists(), 'requires original qualified A21213 input')
    def test_original13_update_rejects_other_component_change(self):
        data = self.original13_image()
        row = self.contract['components'][0]
        for offset in (row['offset'], row['offset'] - fw.BANK_SIZE):
            data[offset] ^= 1
        with self.assertRaisesRegex(ValueError, 'retained firmware'):
            fw.check_preserved(data, self.contract, vermeer_boot_microcode=True)

    @unittest.skipUnless(MODERN_UCODE.exists(), 'requires original qualified A21213 input')
    def test_original13_update_checks_lower_mirror(self):
        data = self.original13_image()
        data[0x6e1b80] ^= 1
        with self.assertRaisesRegex(ValueError, 'Lower Vermeer'):
            fw.check_preserved(data, self.contract, vermeer_boot_microcode=True)

    @unittest.skipUnless(MODERN_UCODE.exists(), 'requires original qualified A21213 input')
    def test_original13_requires_exact_patch_and_slot(self):
        selected = fw.microcode_inputs(self.contract, MODERN_UCODE.parent)
        forged = copy.deepcopy(selected)
        next(row for row in forged if row['processor_revision'] == 0xa212)['sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'original qualified F20'):
            fw.vermeer_boot_microcode_updates(self.contract, forged)
        wrong = copy.deepcopy(self.contract)
        next(row for row in wrong['families']['vermeer']['microcode']
             if row['processor_revision'] == 0xa212)['source'] += 0x100
        with self.assertRaisesRegex(ValueError, 'original Vermeer'):
            fw.vermeer_boot_microcode_updates(wrong, selected)
        with self.assertRaisesRegex(ValueError, 'original qualified F20'):
            fw.vermeer_boot_microcode_updates(self.contract, fw.microcode_inputs(self.contract))

    @unittest.skipUnless(MODERN_ABL.exists() and MODERN_UCODE.exists(),
                         'requires original qualified Vermeer cohort')
    def test_packer_rejects_unpaired_vermeer_cohort_before_stage_access(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            contract = directory / 'contract.json'
            contract.write_text(json.dumps(self.contract))
            argv = [sys.executable, str(BOARD / 'image.py'), 'pack',
                    '--template', str(FIXTURE), '--cpu-contract', str(contract),
                    '--desktop-microcode', '--desktop-microcode-dir', str(MODERN_UCODE.parent),
                    '--native-backend', 'vermeer', '--large-payload']
            for name in ('bootblock', 'romstage', 'medium', 'payload', 'fmap',
                         'cbfstool', 'prefix', 'output', 'report'):
                argv += ['--' + name, str(directory / name)]
            cases = [([], 'requires the paired F20 ABL cohort'),
                     (['--vermeer-boot-microcode'], 'must be supplied together'),
                     (['--vermeer-abl', str(MODERN_ABL)], 'must be supplied together'),
                     (['--vermeer-boot-microcode', '--vermeer-abl', str(MODERN_ABL)],
                      'FileNotFoundError')]
            for flags, reason in cases:
                with self.subTest(flags=flags):
                    result = subprocess.run(argv + flags, capture_output=True, text=True)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(reason, result.stderr)
                    if reason == 'FileNotFoundError':
                        self.assertIn(str(directory / 'bootblock'), result.stderr)
                    self.assertFalse((directory / 'output').exists())
                    self.assertFalse((directory / 'report').exists())

    def test_newer_matching_patch_is_separate_from_preserved_firmware(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.microcode_directory(directory)
            path = directory / 'cpu_microcode_a212.bin'
            data = bytearray(path.read_bytes())
            struct.pack_into('<I', data, 4, 0x0a201213)
            path.write_bytes(data)
            selected = fw.microcode_inputs(self.contract, directory)
            patch, = [r for r in selected if r['processor_revision'] == 0xa212]
            self.assertEqual(patch['patch_id'], 0x0a201213)
            self.assertEqual(patch['template_patch_id'], 0x0a201210)
            fw.check_preserved(self.image, self.contract)

    def test_explicit_microcode_rejects_wrong_cpu_downgrade_and_truncation(self):
        for revision, patch_id, size, reason in ((0xa210, 0x0a201213, None, 'binding'),
                                               (0xa212, 0x0a20120f, None, 'downgrade'),
                                               (0xa212, 0x0a201213, 31, 'Truncated')):
            with tempfile.TemporaryDirectory() as temporary:
                directory = Path(temporary)
                self.microcode_directory(directory)
                path = directory / 'cpu_microcode_a212.bin'
                data = bytearray(path.read_bytes())
                struct.pack_into('<I', data, 4, patch_id)
                struct.pack_into('<H', data, 24, revision)
                path.write_bytes(data if size is None else data[:size])
                with self.assertRaisesRegex(ValueError, reason):
                    fw.microcode_inputs(self.contract, directory)

    def test_directory_checksum_corruption(self):
        data = self.mirrored_change(0x14d1004, b'\0\0\0\0')
        with self.assertRaisesRegex(ValueError, 'checksum'):
            fw.audit(data)

    def test_valid_checksum_out_of_bounds_pointer(self):
        table = self.contract['directories'][1]
        index = next(i for i, r in enumerate(table['entries']) if r['type'] == 0x30)
        offset = table['offset'] + 16 + 16 * index + 8
        data = self.mirrored_change(offset, struct.pack('<Q', fw.FLASH_SIZE))
        data = self.fix_directory(data, table['offset'])
        with self.assertRaisesRegex(ValueError, 'bank reference'):
            fw.audit(data)

    def test_abl_stored_payload_digest_corruption(self):
        family = self.contract['families']['vermeer']
        offset = family['abl_source'] + 0x3a0 + 0xd0
        data = self.mirrored_change(offset, bytes((self.image[offset] ^ 1,)))
        with self.assertRaisesRegex(ValueError, 'payload digest'):
            fw.audit(data)

    def test_abl_manifest_out_of_bounds(self):
        family = self.contract['families']['vermeer']
        offset = family['abl_source'] + 0x100 + 19 * 8 + 4
        data = self.mirrored_change(offset, struct.pack('<I', 0xffffffff))
        with self.assertRaises(ValueError):
            fw.audit(data)

    def test_apcb_checksum_corruption(self):
        family = self.contract['families']['vermeer']
        offset = family['apcb_source'] + family['apcb_declared_bytes'] - 1
        data = self.mirrored_change(offset, bytes((self.image[offset] ^ 1,)))
        with self.assertRaisesRegex(ValueError, 'APCB'):
            fw.audit(data)

    def test_wrong_subprogram_with_valid_directory_checksum(self):
        table = self.contract['directories'][1]
        index = next(i for i, r in enumerate(table['entries'])
                     if r['type'] == 0x30 and r['subprogram'] == 1)
        data = self.mirrored_change(table['offset'] + 16 + 16 * index + 1, b'\2')
        data = self.fix_directory(data, table['offset'])
        with self.assertRaisesRegex(ValueError, 'component'):
            fw.audit(data)

    def test_truncated_and_nonmirrored_images(self):
        for size in [0, 16, fw.BANK_SIZE, fw.FLASH_SIZE - 1]:
            with self.assertRaises(ValueError):
                fw.audit(self.image[:size])
        data = bytearray(self.image)
        data[0] ^= 1
        with self.assertRaisesRegex(ValueError, 'mirror'):
            fw.audit(data)

    def test_preservation_excludes_only_replaced_x86(self):
        data = self.mirrored_change(0x1e02100, b'native x86')
        fw.check_preserved(data, self.contract)
        row = self.contract['components'][0]
        data = self.mirrored_change(row['offset'], bytes((self.image[row['offset']] ^ 1,)))
        with self.assertRaisesRegex(ValueError, 'retained firmware'):
            fw.check_preserved(data, self.contract)
        with self.assertRaisesRegex(ValueError, 'component changed'):
            fw.audit(data, self.image)


if __name__ == '__main__':
    unittest.main()
