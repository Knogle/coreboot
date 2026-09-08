#!/usr/bin/env python3
"""Apply or verify the retired local-only B06V8 CSI-wrapper branch patch.

The input is a 4 MiB image already composed from a public coreboot base and
the user's hash-pinned MSI firmware.  This tool never contains or extracts the
proprietary wrapper.  It verifies that complete wrapper by hash, changes one
documented branch opcode, verifies the resulting wrapper, and writes only a
separate local output image.

Hardware erratum: B06V8's alleged extended-CMOS gate actually read standard
RTC clock/calendar bytes because ICH10 U128E was clear.  Keep this tool solely
for historical reproduction; do not flash its output.  B06V9 supersedes it.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path


def sha256(payload: bytes | bytearray) -> str:
    return hashlib.sha256(payload).hexdigest()


def fnv1a32(payload: bytes | bytearray) -> int:
    digest = 0x811C9DC5
    for value in payload:
        digest ^= value
        digest = (digest * 0x01000193) & 0xFFFFFFFF
    return digest


@dataclass(frozen=True)
class PatchLayout:
    image_size: int
    wrapper_offset: int
    wrapper_size: int
    patch_offset: int
    original_byte: int
    patched_byte: int
    original_wrapper_sha256: str
    patched_wrapper_sha256: str
    original_wrapper_fnv1a32: int
    patched_wrapper_fnv1a32: int

    @property
    def wrapper_end(self) -> int:
        return self.wrapper_offset + self.wrapper_size

    @property
    def wrapper_patch_offset(self) -> int:
        return self.patch_offset - self.wrapper_offset


B06V8_LAYOUT = PatchLayout(
    image_size=0x400000,
    wrapper_offset=0x3C04E2,
    wrapper_size=0x837,
    patch_offset=0x3C0BC2,
    original_byte=0x74,
    patched_byte=0xEB,
    original_wrapper_sha256=(
        "546f0c4d455ce250b3fc6047d3864b25d6fd16e7cf1c3bfaa6b61d69510af6c3"
    ),
    patched_wrapper_sha256=(
        "8ffd927db21a22614bf446264575216138ed012b49c90f135729bb38c153c9dc"
    ),
    original_wrapper_fnv1a32=0x65B20E50,
    patched_wrapper_fnv1a32=0x986153C5,
)


def validate_layout(layout: PatchLayout) -> None:
    if layout.image_size <= 0:
        raise ValueError("image size must be positive")
    if layout.wrapper_offset < 0 or layout.wrapper_size <= 0:
        raise ValueError("invalid wrapper range")
    if layout.wrapper_end > layout.image_size:
        raise ValueError("wrapper range exceeds image")
    if not layout.wrapper_offset <= layout.patch_offset < layout.wrapper_end:
        raise ValueError("patch offset is outside wrapper")
    for value in (layout.original_byte, layout.patched_byte):
        if not 0 <= value <= 0xFF:
            raise ValueError("patch values must be bytes")
    if layout.original_byte == layout.patched_byte:
        raise ValueError("original and patched bytes must differ")
    for name, value in (
        ("original wrapper", layout.original_wrapper_sha256),
        ("patched wrapper", layout.patched_wrapper_sha256),
    ):
        if len(value) != 64 or any(c not in "0123456789abcdef" for c in value):
            raise ValueError(f"invalid {name} SHA-256")


def wrapper(image: bytes | bytearray, layout: PatchLayout) -> bytes:
    return bytes(image[layout.wrapper_offset : layout.wrapper_end])


def verify_original_image(image: bytes, layout: PatchLayout) -> None:
    validate_layout(layout)
    if len(image) != layout.image_size:
        raise ValueError(
            f"image size 0x{len(image):x}, expected 0x{layout.image_size:x}"
        )
    original_wrapper = wrapper(image, layout)
    if sha256(original_wrapper) != layout.original_wrapper_sha256:
        raise ValueError("original wrapper SHA-256 mismatch")
    if fnv1a32(original_wrapper) != layout.original_wrapper_fnv1a32:
        raise ValueError("original wrapper FNV-1a-32 mismatch")
    if image[layout.patch_offset] != layout.original_byte:
        raise ValueError(
            f"source byte is 0x{image[layout.patch_offset]:02x}, "
            f"expected 0x{layout.original_byte:02x}"
        )


def verify_patched_image(image: bytes, layout: PatchLayout = B06V8_LAYOUT) -> None:
    validate_layout(layout)
    if len(image) != layout.image_size:
        raise ValueError(
            f"image size 0x{len(image):x}, expected 0x{layout.image_size:x}"
        )
    patched_wrapper = wrapper(image, layout)
    if sha256(patched_wrapper) != layout.patched_wrapper_sha256:
        raise ValueError("patched wrapper SHA-256 mismatch")
    if fnv1a32(patched_wrapper) != layout.patched_wrapper_fnv1a32:
        raise ValueError("patched wrapper FNV-1a-32 mismatch")
    if image[layout.patch_offset] != layout.patched_byte:
        raise ValueError(
            f"patched byte is 0x{image[layout.patch_offset]:02x}, "
            f"expected 0x{layout.patched_byte:02x}"
        )


def patch_verified_image(
    image: bytes,
    expected_input_sha256: str,
    layout: PatchLayout = B06V8_LAYOUT,
) -> bytes:
    expected_input_sha256 = expected_input_sha256.lower()
    if len(expected_input_sha256) != 64 or any(
        c not in "0123456789abcdef" for c in expected_input_sha256
    ):
        raise ValueError("expected input SHA-256 must be exactly 64 hex digits")
    actual_input_sha256 = sha256(image)
    if actual_input_sha256 != expected_input_sha256:
        raise ValueError(
            f"input SHA-256 is {actual_input_sha256}, "
            f"expected {expected_input_sha256}"
        )
    verify_original_image(image, layout)

    patched = bytearray(image)
    patched[layout.patch_offset] = layout.patched_byte
    differences = [
        offset for offset, (before, after) in enumerate(zip(image, patched))
        if before != after
    ]
    if differences != [layout.patch_offset]:
        raise ValueError("internal error: patch did not change exactly one byte")
    output = bytes(patched)
    verify_patched_image(output, layout)
    return output


def command_patch(args: argparse.Namespace) -> int:
    source = args.input.read_bytes()
    patched = patch_verified_image(source, args.expected_input_sha256)
    if args.output.exists() and not args.force:
        raise ValueError(f"output already exists: {args.output} (use --force)")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(patched)
    readback = args.output.read_bytes()
    if readback != patched:
        raise ValueError("output readback differs from generated image")

    print(json.dumps({
        "input": str(args.input),
        "input_sha256": sha256(source),
        "output": str(args.output),
        "output_sha256": sha256(readback),
        "image_size": f"0x{len(readback):x}",
        "wrapper_offset": f"0x{B06V8_LAYOUT.wrapper_offset:x}",
        "wrapper_size": f"0x{B06V8_LAYOUT.wrapper_size:x}",
        "wrapper_patch_offset": f"0x{B06V8_LAYOUT.wrapper_patch_offset:x}",
        "image_patch_offset": f"0x{B06V8_LAYOUT.patch_offset:x}",
        "runtime_patch_address": "0xfffc0bc2",
        "w25q128_patch_offset": "0xfc0bc2",
        "opcode_change": "74->eb",
        "patched_wrapper_sha256": B06V8_LAYOUT.patched_wrapper_sha256,
        "patched_wrapper_fnv1a32": (
            f"{B06V8_LAYOUT.patched_wrapper_fnv1a32:08x}"
        ),
    }, indent=2, sort_keys=True))
    return 0


def command_verify(args: argparse.Namespace) -> int:
    image = args.image.read_bytes()
    verify_patched_image(image)
    print(json.dumps({
        "image": str(args.image),
        "image_sha256": sha256(image),
        "image_size": f"0x{len(image):x}",
        "patched_wrapper_sha256": B06V8_LAYOUT.patched_wrapper_sha256,
        "patched_wrapper_fnv1a32": (
            f"{B06V8_LAYOUT.patched_wrapper_fnv1a32:08x}"
        ),
        "verification": "PASS",
    }, indent=2, sort_keys=True))
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    patch_parser = subparsers.add_parser("patch", help="create patched local image")
    patch_parser.add_argument("input", type=Path, help="unpatched 4 MiB composite")
    patch_parser.add_argument("output", type=Path, help="patched local 4 MiB image")
    patch_parser.add_argument(
        "--expected-input-sha256",
        required=True,
        help="required SHA-256 pin for the complete unpatched input",
    )
    patch_parser.add_argument(
        "--force", action="store_true", help="replace an existing output file"
    )
    patch_parser.set_defaults(handler=command_patch)

    verify_parser = subparsers.add_parser("verify", help="verify patched local image")
    verify_parser.add_argument("image", type=Path, help="patched 4 MiB image")
    verify_parser.set_defaults(handler=command_verify)

    args = parser.parse_args()
    try:
        return args.handler(args)
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    raise SystemExit(main())
