#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Check local B565 firmware files against source-only reference metadata."""

import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source-root", type=Path, default=Path(__file__).resolve().parents[2],
        help="checkout root containing 3rdparty/blobs",
    )
    parser.add_argument(
        "--all", action="store_true",
        help="require optional desktop and EDK2 inputs too",
    )
    args = parser.parse_args()
    root = args.source_root.resolve()
    manifest = Path(__file__).resolve().parents[2] / (
        "Documentation/mainboard/asrockrack/"
        "b565d4_v1l_external_inputs.json"
    )
    data = json.loads(manifest.read_text())
    if data.get("schema") != 1:
        parser.error("unsupported input manifest")
    required = {
        "pre-x86-template.rom", "promontory19.bin", "cpu_microcode_a500.bin"
    }
    checked = 0
    failed = 0
    for entry in data["inputs"]:
        rel = Path(entry["path"])
        if rel.is_absolute() or ".." in rel.parts:
            parser.error("unsafe manifest path")
        needed = args.all or rel.name in required
        path = root / rel
        if not path.is_file() or path.is_symlink():
            if needed:
                print(f"MISSING {rel}")
                failed += 1
            continue
        digest = hashlib.sha256()
        with path.open("rb") as stream:
            for block in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(block)
        if path.stat().st_size != entry["size"] or digest.hexdigest() != entry["sha256"]:
            print(f"MISMATCH {rel}")
            failed += 1
        else:
            print(f"OK {rel}")
            checked += 1
    print(f"verified={checked} failures={failed}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
