#!/usr/bin/env python3

import hashlib
import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

import x58_b06v9_wrapper_patch as wrapper_patch  # noqa: E402


def synthetic_layout(
    image: bytes, wrapper_offset: int, wrapper_size: int,
    patches: tuple[wrapper_patch.Patch, ...],
) -> wrapper_patch.PatchLayout:
    original_wrapper = image[wrapper_offset:wrapper_offset + wrapper_size]
    patched_image = bytearray(image)
    for item in patches:
        patched_image[item.offset:item.end] = item.patched
    patched_wrapper = patched_image[wrapper_offset:wrapper_offset + wrapper_size]
    return wrapper_patch.PatchLayout(
        image_size=len(image),
        wrapper_offset=wrapper_offset,
        wrapper_size=wrapper_size,
        patches=patches,
        original_wrapper_sha256=hashlib.sha256(original_wrapper).hexdigest(),
        patched_wrapper_sha256=hashlib.sha256(patched_wrapper).hexdigest(),
        original_wrapper_fnv1a32=wrapper_patch.fnv1a32(original_wrapper),
        patched_wrapper_fnv1a32=wrapper_patch.fnv1a32(patched_wrapper),
    )


class B06V9WrapperPatchTests(unittest.TestCase):
    def setUp(self) -> None:
        image = bytearray((index * 29 + 7) & 0xFF for index in range(0x100))
        image[0x45] = 0x11
        image[0x46] = 0x22
        image[0x58] = 0x74
        image[0x59] = 0x09
        self.image = bytes(image)
        self.patches = (
            wrapper_patch.Patch(0x45, b"\x11\x22", b"\x33\x44", "pair"),
            wrapper_patch.Patch(0x58, b"\x74\x09", b"\x90\x90", "branch"),
        )
        self.layout = synthetic_layout(self.image, 0x30, 0x40, self.patches)

    def test_production_layout_is_exact(self) -> None:
        layout = wrapper_patch.B06V9_LAYOUT
        self.assertEqual((layout.wrapper_offset, layout.wrapper_size),
                         (0x3C04E2, 0x837))
        self.assertEqual(
            [(item.offset, item.original.hex(), item.patched.hex())
             for item in layout.patches],
            [
                (0x3C0516, "00", "01"),
                (0x3C0578, "01", "00"),
                (0x3C066B, "01", "00"),
                (0x3C083F, "01", "00"),
                (0x3C0A0C, "01", "00"),
                (0x3C0BC2, "7409", "9090"),
            ],
        )
        self.assertEqual(wrapper_patch.changed_offsets(layout), [
            0x3C0516, 0x3C0578, 0x3C066B, 0x3C083F,
            0x3C0A0C, 0x3C0BC2, 0x3C0BC3,
        ])
        self.assertEqual(layout.original_wrapper_fnv1a32, 0x65B20E50)
        self.assertEqual(layout.patched_wrapper_fnv1a32, 0x98E2F3DE)
        self.assertEqual(
            layout.patched_wrapper_sha256,
            "d38c093271f18cfa5f399421654fcab2e61fa07efca85323904dcb74b89e0fa9",
        )

    def test_multiple_regions_change_only_the_documented_bytes(self) -> None:
        patched = wrapper_patch.patch_verified_image(
            self.image, wrapper_patch.sha256(self.image), self.layout
        )
        differences = [
            offset for offset, (before, after) in enumerate(zip(self.image, patched))
            if before != after
        ]
        self.assertEqual(differences, wrapper_patch.changed_offsets(self.layout))
        wrapper_patch.verify_patched_image(patched, self.layout)

    def test_complete_input_hash_is_mandatory(self) -> None:
        with self.assertRaisesRegex(ValueError, "input SHA-256"):
            wrapper_patch.patch_verified_image(self.image, "0" * 64, self.layout)
        with self.assertRaisesRegex(ValueError, "64 hex digits"):
            wrapper_patch.patch_verified_image(self.image, "abcd", self.layout)

    def test_original_wrapper_mutation_is_rejected(self) -> None:
        corrupted = bytearray(self.image)
        corrupted[self.layout.wrapper_offset] ^= 1
        with self.assertRaisesRegex(ValueError, "wrapper SHA-256"):
            wrapper_patch.patch_verified_image(
                bytes(corrupted), wrapper_patch.sha256(corrupted), self.layout
            )

    def test_wrong_source_sequence_with_matching_hash_is_rejected(self) -> None:
        wrong_patch = wrapper_patch.Patch(0x45, b"\xff\x22", b"\x33\x44", "bad")
        layout = synthetic_layout(self.image, 0x30, 0x40, (wrong_patch,))
        with self.assertRaisesRegex(ValueError, "source sequence"):
            wrapper_patch.patch_verified_image(
                self.image, wrapper_patch.sha256(self.image), layout
            )

    def test_patched_wrapper_mutation_is_rejected(self) -> None:
        patched = bytearray(wrapper_patch.patch_verified_image(
            self.image, wrapper_patch.sha256(self.image), self.layout
        ))
        patched[self.layout.wrapper_offset + 1] ^= 1
        with self.assertRaisesRegex(ValueError, "patched wrapper SHA-256"):
            wrapper_patch.verify_patched_image(bytes(patched), self.layout)

    def test_mixed_b06v8_wrapper_is_not_an_original(self) -> None:
        mixed = bytearray(self.image)
        mixed[0x58] = 0xEB
        with self.assertRaisesRegex(ValueError, "wrapper SHA-256"):
            wrapper_patch.patch_verified_image(
                bytes(mixed), wrapper_patch.sha256(mixed), self.layout
            )

    def test_invalid_patch_layouts_fail_closed(self) -> None:
        base = dict(
            image_size=0x100,
            wrapper_offset=0x30,
            wrapper_size=0x40,
            original_wrapper_sha256="0" * 64,
            patched_wrapper_sha256="1" * 64,
            original_wrapper_fnv1a32=0,
            patched_wrapper_fnv1a32=1,
        )
        invalid_sets = (
            (),
            (wrapper_patch.Patch(0x45, b"", b"", "empty"),),
            (wrapper_patch.Patch(0x45, b"\x01", b"\x02\x03", "length"),),
            (wrapper_patch.Patch(0x45, b"\x01", b"\x01", "unchanged"),),
            (wrapper_patch.Patch(0x45, b"\x01\x02", b"\x03\x04", "a"),
             wrapper_patch.Patch(0x46, b"\x02", b"\x05", "overlap")),
            (wrapper_patch.Patch(0x50, b"\x01", b"\x02", "later"),
             wrapper_patch.Patch(0x45, b"\x01", b"\x02", "earlier")),
            (wrapper_patch.Patch(0x70, b"\x01", b"\x02", "outside"),),
        )
        for patches in invalid_sets:
            with self.subTest(patches=patches), self.assertRaises(ValueError):
                wrapper_patch.validate_layout(
                    wrapper_patch.PatchLayout(patches=patches, **base)
                )


if __name__ == "__main__":
    unittest.main()
