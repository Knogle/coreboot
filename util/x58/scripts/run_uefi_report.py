#!/usr/bin/env python3
"""Generate a UEFIExtract report with an input-bound provenance sidecar.

UEFIExtract writes ``IMAGE.report.txt`` itself.  This wrapper additionally
captures both diagnostic streams and records hashes for the extractor, input,
and generated report.  The sidecar belongs beside private firmware artifacts;
it is not a redistribution mechanism.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
from typing import Any


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def artifact(path: Path) -> dict[str, Any]:
    return {
        "path": str(path),
        "size": path.stat().st_size,
        "sha256": sha256_file(path),
    }


def generate_report(
    extractor: Path, image: Path, tool_commit: str, metadata_path: Path | None = None
) -> tuple[dict[str, Any], Path]:
    if not extractor.is_file():
        raise ValueError(f"not a regular extractor: {extractor}")
    if not image.is_file():
        raise ValueError(f"not a regular input image: {image}")
    extractor = extractor.resolve()
    image = image.resolve()

    version = subprocess.run(
        [str(extractor), "--version"],
        check=False,
        capture_output=True,
        text=True,
    )
    command = [str(extractor), str(image), "report"]
    completed = subprocess.run(command, check=False, capture_output=True, text=True)
    report_path = Path(f"{image}.report.txt")
    metadata_path = metadata_path or Path(f"{image}.report-meta.json")

    metadata: dict[str, Any] = {
        "schema_version": 1,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "tool_commit": tool_commit,
        "extractor": artifact(extractor),
        "extractor_version_stdout": version.stdout,
        "extractor_version_stderr": version.stderr,
        "command": command,
        "returncode": completed.returncode,
        "stdout": completed.stdout,
        "stderr": completed.stderr,
        "input": artifact(image),
        "report": artifact(report_path) if report_path.is_file() else None,
    }
    metadata_path.write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return metadata, metadata_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("extractor", type=Path)
    parser.add_argument("image", type=Path)
    parser.add_argument(
        "--tool-commit",
        required=True,
        help="source revision used to build the extractor",
    )
    parser.add_argument(
        "--metadata",
        type=Path,
        help="sidecar path (default: IMAGE.report-meta.json)",
    )
    args = parser.parse_args()
    try:
        metadata, metadata_path = generate_report(
            args.extractor, args.image, args.tool_commit, args.metadata
        )
    except (OSError, ValueError) as error:
        parser.error(str(error))

    print(
        json.dumps(
            {
                "metadata": str(metadata_path),
                "returncode": metadata["returncode"],
                "report": metadata["report"],
            },
            indent=2,
            sort_keys=True,
        )
    )
    return 0 if metadata["returncode"] == 0 and metadata["report"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
