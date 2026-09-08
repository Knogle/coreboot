#!/usr/bin/env python3

import hashlib
import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

import x58_b06v8_wrapper_patch as wrapper_patch  # noqa: E402


def synthetic_layout(image: bytes, wrapper_offset: int, wrapper_size: int,
                     patch_offset: int) -> wrapper_patch.PatchLayout:
    original_wrapper = image[wrapper_offset : wrapper_offset + wrapper_size]
    patched = bytearray(original_wrapper)
    patched[patch_offset - wrapper_offset] = 0xEB
    return wrapper_patch.PatchLayout(
        image_size=len(image),
        wrapper_offset=wrapper_offset,
        wrapper_size=wrapper_size,
        patch_offset=patch_offset,
        original_byte=0x74,
        patched_byte=0xEB,
        original_wrapper_sha256=hashlib.sha256(original_wrapper).hexdigest(),
        patched_wrapper_sha256=hashlib.sha256(patched).hexdigest(),
        original_wrapper_fnv1a32=wrapper_patch.fnv1a32(original_wrapper),
        patched_wrapper_fnv1a32=wrapper_patch.fnv1a32(patched),
    )


class B06V8WrapperPatchTests(unittest.TestCase):
    def setUp(self) -> None:
        image = bytearray((index * 29 + 7) & 0xFF for index in range(0x100))
        image[0x4A] = 0x74
        self.image = bytes(image)
        self.layout = synthetic_layout(self.image, 0x30, 0x40, 0x4A)

    def test_b06v8_layout_is_the_documented_single_branch_opcode(self) -> None:
        layout = wrapper_patch.B06V8_LAYOUT
        self.assertEqual(layout.wrapper_patch_offset, 0x6E0)
        self.assertEqual(layout.patch_offset, 0x3C0BC2)
        self.assertEqual((layout.original_byte, layout.patched_byte), (0x74, 0xEB))
        self.assertEqual(layout.original_wrapper_fnv1a32, 0x65B20E50)
        self.assertEqual(layout.patched_wrapper_fnv1a32, 0x986153C5)
        self.assertEqual(
            layout.patched_wrapper_sha256,
            "8ffd927db21a22614bf446264575216138ed012b49c90f135729bb38c153c9dc",
        )

    def test_patch_changes_exactly_one_verified_byte(self) -> None:
        patched = wrapper_patch.patch_verified_image(
            self.image, wrapper_patch.sha256(self.image), self.layout
        )
        differences = [
            offset for offset, (before, after) in enumerate(zip(self.image, patched))
            if before != after
        ]
        self.assertEqual(differences, [self.layout.patch_offset])
        self.assertEqual(patched[self.layout.patch_offset], 0xEB)
        wrapper_patch.verify_patched_image(patched, self.layout)

    def test_complete_input_hash_is_mandatory(self) -> None:
        with self.assertRaisesRegex(ValueError, "input SHA-256"):
            wrapper_patch.patch_verified_image(self.image, "0" * 64, self.layout)
        with self.assertRaisesRegex(ValueError, "64 hex digits"):
            wrapper_patch.patch_verified_image(self.image, "abcd", self.layout)

    def test_wrong_wrapper_or_source_opcode_fails_closed(self) -> None:
        corrupted = bytearray(self.image)
        corrupted[self.layout.wrapper_offset] ^= 1
        with self.assertRaisesRegex(ValueError, "wrapper SHA-256"):
            wrapper_patch.patch_verified_image(
                bytes(corrupted), wrapper_patch.sha256(corrupted), self.layout
            )

        wrong_opcode = bytearray(self.image)
        wrong_opcode[self.layout.patch_offset] = 0x75
        wrong_layout = synthetic_layout(bytes(wrong_opcode), 0x30, 0x40, 0x4A)
        with self.assertRaisesRegex(ValueError, "source byte"):
            wrapper_patch.patch_verified_image(
                bytes(wrong_opcode), wrapper_patch.sha256(wrong_opcode), wrong_layout
            )

    def test_patched_wrapper_mutation_is_rejected(self) -> None:
        patched = bytearray(wrapper_patch.patch_verified_image(
            self.image, wrapper_patch.sha256(self.image), self.layout
        ))
        patched[self.layout.wrapper_offset + 1] ^= 1
        with self.assertRaisesRegex(ValueError, "patched wrapper SHA-256"):
            wrapper_patch.verify_patched_image(bytes(patched), self.layout)

    def test_invalid_layout_is_rejected(self) -> None:
        invalid = wrapper_patch.PatchLayout(
            image_size=0x20,
            wrapper_offset=0x10,
            wrapper_size=0x20,
            patch_offset=0x18,
            original_byte=0x74,
            patched_byte=0xEB,
            original_wrapper_sha256="0" * 64,
            patched_wrapper_sha256="1" * 64,
            original_wrapper_fnv1a32=0,
            patched_wrapper_fnv1a32=1,
        )
        with self.assertRaisesRegex(ValueError, "wrapper range exceeds image"):
            wrapper_patch.validate_layout(invalid)


if __name__ == "__main__":
    unittest.main()
