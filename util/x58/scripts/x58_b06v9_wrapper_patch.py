#!/usr/bin/env python3
"""Apply or verify the local-only B06V9 deterministic CSI-wrapper patch.

The input is a 4 MiB image already composed from a public coreboot base and
the user's hash-pinned MSI firmware.  This tool contains no proprietary
module.  It verifies the complete original wrapper, changes six documented
regions (seven bytes), verifies the complete result, and writes a separate
local output image.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path


RUNTIME_IMAGE_BASE = 0xFFC00000
W25Q128_TOP_OFFSET = 0xC00000


def sha256(payload: bytes | bytearray) -> str:
    return hashlib.sha256(payload).hexdigest()


def fnv1a32(payload: bytes | bytearray) -> int:
    digest = 0x811C9DC5
    for value in payload:
        digest ^= value
        digest = (digest * 0x01000193) & 0xFFFFFFFF
    return digest


@dataclass(frozen=True)
class Patch:
    offset: int
    original: bytes
    patched: bytes
    purpose: str

    @property
    def end(self) -> int:
        return self.offset + len(self.original)


@dataclass(frozen=True)
class PatchLayout:
    image_size: int
    wrapper_offset: int
    wrapper_size: int
    patches: tuple[Patch, ...]
    original_wrapper_sha256: str
    patched_wrapper_sha256: str
    original_wrapper_fnv1a32: int
    patched_wrapper_fnv1a32: int

    @property
    def wrapper_end(self) -> int:
        return self.wrapper_offset + self.wrapper_size


B06V9_LAYOUT = PatchLayout(
    image_size=0x400000,
    wrapper_offset=0x3C04E2,
    wrapper_size=0x837,
    patches=(
        Patch(0x3C0516, bytes.fromhex("00"), bytes.fromhex("01"),
              "initialize CSI state byte 07 to one"),
        Patch(0x3C0578, bytes.fromhex("01"), bytes.fromhex("00"),
              "initialize CSI state pair 1c/1d to zero/one"),
        Patch(0x3C066B, bytes.fromhex("01"), bytes.fromhex("00"),
              "initialize CSI state pair 70/71 to zero/one"),
        Patch(0x3C083F, bytes.fromhex("01"), bytes.fromhex("00"),
              "initialize CSI state pair cd/ce to zero/one"),
        Patch(0x3C0A0C, bytes.fromhex("01"), bytes.fromhex("00"),
              "initialize CSI state pair 121/122 to zero/one"),
        Patch(0x3C0BC2, bytes.fromhex("7409"), bytes.fromhex("9090"),
              "bypass time-varying RTC input parser"),
    ),
    original_wrapper_sha256=(
        "546f0c4d455ce250b3fc6047d3864b25d6fd16e7cf1c3bfaa6b61d69510af6c3"
    ),
    patched_wrapper_sha256=(
        "d38c093271f18cfa5f399421654fcab2e61fa07efca85323904dcb74b89e0fa9"
    ),
    original_wrapper_fnv1a32=0x65B20E50,
    patched_wrapper_fnv1a32=0x98E2F3DE,
)


def validate_sha256(name: str, value: str) -> None:
    if len(value) != 64 or any(c not in "0123456789abcdef" for c in value):
        raise ValueError(f"invalid {name} SHA-256")


def validate_layout(layout: PatchLayout) -> None:
    if layout.image_size <= 0:
        raise ValueError("image size must be positive")
    if layout.wrapper_offset < 0 or layout.wrapper_size <= 0:
        raise ValueError("invalid wrapper range")
    if layout.wrapper_end > layout.image_size:
        raise ValueError("wrapper range exceeds image")
    if not layout.patches:
        raise ValueError("patch list must not be empty")

    previous_end = layout.wrapper_offset
    for patch in layout.patches:
        if not patch.original or len(patch.original) != len(patch.patched):
            raise ValueError("patch byte sequences must be non-empty and equal length")
        if any(before == after for before, after in zip(
                patch.original, patch.patched)):
            raise ValueError("every documented patch byte must change")
        if patch.offset < previous_end:
            raise ValueError("patches must be sorted and non-overlapping")
        if patch.offset < layout.wrapper_offset or patch.end > layout.wrapper_end:
            raise ValueError("patch range is outside wrapper")
        previous_end = patch.end

    validate_sha256("original wrapper", layout.original_wrapper_sha256)
    validate_sha256("patched wrapper", layout.patched_wrapper_sha256)


def wrapper(image: bytes | bytearray, layout: PatchLayout) -> bytes:
    return bytes(image[layout.wrapper_offset:layout.wrapper_end])


def changed_offsets(layout: PatchLayout) -> list[int]:
    return [
        patch.offset + index
        for patch in layout.patches
        for index in range(len(patch.original))
    ]


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
    for patch in layout.patches:
        actual = image[patch.offset:patch.end]
        if actual != patch.original:
            raise ValueError(
                f"source sequence at 0x{patch.offset:x} is {actual.hex()}, "
                f"expected {patch.original.hex()}"
            )


def verify_patched_image(
    image: bytes, layout: PatchLayout = B06V9_LAYOUT
) -> None:
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
    for patch in layout.patches:
        actual = image[patch.offset:patch.end]
        if actual != patch.patched:
            raise ValueError(
                f"patched sequence at 0x{patch.offset:x} is {actual.hex()}, "
                f"expected {patch.patched.hex()}"
            )


def patch_verified_image(
    image: bytes,
    expected_input_sha256: str,
    layout: PatchLayout = B06V9_LAYOUT,
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
    for item in layout.patches:
        patched[item.offset:item.end] = item.patched
    differences = [
        offset for offset, (before, after) in enumerate(zip(image, patched))
        if before != after
    ]
    if differences != changed_offsets(layout):
        raise ValueError("internal error: changed-byte set is not exact")
    output = bytes(patched)
    verify_patched_image(output, layout)
    return output


def patch_metadata(layout: PatchLayout) -> list[dict[str, object]]:
    return [
        {
            "purpose": item.purpose,
            "wrapper_offset": f"0x{item.offset - layout.wrapper_offset:x}",
            "image_offset": f"0x{item.offset:x}",
            "runtime_address": f"0x{RUNTIME_IMAGE_BASE + item.offset:x}",
            "w25q128_offset": f"0x{W25Q128_TOP_OFFSET + item.offset:x}",
            "original": item.original.hex(),
            "patched": item.patched.hex(),
        }
        for item in layout.patches
    ]


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
        "wrapper_offset": f"0x{B06V9_LAYOUT.wrapper_offset:x}",
        "wrapper_size": f"0x{B06V9_LAYOUT.wrapper_size:x}",
        "changed_byte_count": len(changed_offsets(B06V9_LAYOUT)),
        "patches": patch_metadata(B06V9_LAYOUT),
        "patched_wrapper_sha256": B06V9_LAYOUT.patched_wrapper_sha256,
        "patched_wrapper_fnv1a32": f"{B06V9_LAYOUT.patched_wrapper_fnv1a32:08x}",
    }, indent=2, sort_keys=True))
    return 0


def command_verify(args: argparse.Namespace) -> int:
    image = args.image.read_bytes()
    verify_patched_image(image)
    print(json.dumps({
        "image": str(args.image),
        "image_sha256": sha256(image),
        "image_size": f"0x{len(image):x}",
        "changed_byte_count": len(changed_offsets(B06V9_LAYOUT)),
        "patched_wrapper_sha256": B06V9_LAYOUT.patched_wrapper_sha256,
        "patched_wrapper_fnv1a32": f"{B06V9_LAYOUT.patched_wrapper_fnv1a32:08x}",
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
        "--expected-input-sha256", required=True,
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
