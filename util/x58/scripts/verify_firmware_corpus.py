#!/usr/bin/env python3
"""Verify local analysis inputs against the checked-in firmware corpus.

The script never downloads, extracts, or modifies firmware.  Missing local
files are reported separately from size/hash mismatches so a partial private
corpus remains useful without weakening provenance checks.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def verify_artifact(root: Path, artifact: dict[str, Any]) -> dict[str, Any]:
    path = root / artifact["local_path"]
    result = {
        "path": artifact["local_path"],
        "expected_size": artifact["size"],
        "expected_sha256": artifact["sha256"],
    }
    if not path.is_file():
        result["status"] = "missing"
        return result
    actual_size = path.stat().st_size
    actual_sha256 = sha256_file(path)
    result.update({"actual_size": actual_size, "actual_sha256": actual_sha256})
    if actual_size != artifact["size"]:
        result["status"] = "size-mismatch"
    elif actual_sha256 != artifact["sha256"].lower():
        result["status"] = "hash-mismatch"
    else:
        result["status"] = "verified"
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--manifest",
        type=Path,
        default=Path("research/firmware-corpus.json"),
    )
    parser.add_argument("--id", action="append", default=[], help="verify only this ID")
    parser.add_argument(
        "--allow-missing",
        action="store_true",
        help="return success when selected artifacts are absent",
    )
    args = parser.parse_args()
    if not args.manifest.is_file():
        parser.error(f"not a regular manifest: {args.manifest}")
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    root = args.manifest.resolve().parents[1]
    selected = [
        image
        for image in manifest["images"]
        if not args.id or image["id"] in set(args.id)
    ]
    missing_ids = set(args.id) - {image["id"] for image in selected}
    if missing_ids:
        parser.error("unknown corpus ID(s): " + ", ".join(sorted(missing_ids)))

    output = []
    failed = False
    for image in selected:
        artifacts = {
            kind: verify_artifact(root, image[kind]) for kind in ("archive", "payload")
        }
        output.append({"id": image["id"], "artifacts": artifacts})
        for result in artifacts.values():
            if result["status"] in {"size-mismatch", "hash-mismatch"}:
                failed = True
            elif result["status"] == "missing" and not args.allow_missing:
                failed = True
    print(json.dumps(output, indent=2, sort_keys=True))
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
