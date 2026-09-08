#!/usr/bin/env python3
"""Summarize and compare UEFIExtract text reports.

This parser consumes reports produced by a pinned UEFIExtract build.  It emits
only structural metadata (counts, GUIDs, sizes, and CRC32 values), never module
bodies.  Parser warnings printed by UEFIExtract must still be retained beside
the local analysis log; a successful exit does not make every DEPEX semantic.
"""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from dataclasses import dataclass
import json
from pathlib import Path
import re
from typing import Any


GUID_RE = re.compile(
    r"\b[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-"
    r"[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}\b"
)


@dataclass(frozen=True)
class ReportRow:
    item_type: str
    subtype: str
    base: int | None
    size: int | None
    crc32: str | None
    depth: int
    guid: str | None
    name: str | None


def _hex_or_none(value: str) -> int | None:
    value = value.strip()
    if not value or value == "N/A":
        return None
    try:
        return int(value, 16)
    except ValueError:
        return None


def parse_report(text: str) -> list[ReportRow]:
    rows: list[ReportRow] = []
    for line in text.splitlines():
        if "|" not in line:
            continue
        parts = line.split("|", 5)
        if len(parts) != 6:
            continue
        item_type = parts[0].strip()
        subtype = parts[1].strip()
        if not item_type or item_type == "Type" or set(item_type) == {"-"}:
            continue
        base = _hex_or_none(parts[2])
        size = _hex_or_none(parts[3])
        crc_value = parts[4].strip().upper()
        crc32 = crc_value if re.fullmatch(r"[0-9A-F]{8}", crc_value) else None

        raw_name = parts[5].strip()
        depth = len(raw_name) - len(raw_name.lstrip("-"))
        name_field = raw_name[depth:].strip()
        guid_match = GUID_RE.search(name_field)
        guid = guid_match.group(0).upper() if guid_match else None
        if "|" in name_field:
            display_name = name_field.split("|", 1)[1].strip() or None
        elif guid_match:
            remaining = name_field[guid_match.end() :].strip()
            display_name = remaining or None
        else:
            display_name = name_field or None
        rows.append(
            ReportRow(
                item_type=item_type,
                subtype=subtype,
                base=base,
                size=size,
                crc32=crc32,
                depth=depth,
                guid=guid,
                name=display_name,
            )
        )
    return rows


def report_summary(label: str, path: Path, rows: list[ReportRow]) -> dict[str, Any]:
    item_types = Counter(row.item_type for row in rows)
    subtypes = Counter(row.subtype for row in rows if row.subtype)
    files = [row for row in rows if row.item_type == "File" and row.guid]
    guid_occurrences = Counter(row.guid for row in files if row.guid)
    file_types: dict[str, Counter[str]] = defaultdict(Counter)
    names: dict[str, set[str]] = defaultdict(set)
    for row in files:
        assert row.guid is not None
        file_types[row.guid][row.subtype] += 1
        if row.name:
            names[row.guid].add(row.name)
    return {
        "label": label,
        "report": str(path),
        "rows": len(rows),
        "item_types": dict(sorted(item_types.items())),
        "section_subtypes": {
            key: value
            for key, value in sorted(subtypes.items())
            if key
            in {
                "PE32 image",
                "TE image",
                "PEI dependency",
                "DXE dependency",
                "SMM dependency",
                "MM dependency",
                "Compressed",
                "GUID defined",
                "Volume image",
            }
        },
        "file_occurrences": len(files),
        "unique_file_guids": len(guid_occurrences),
        "files": {
            guid: {
                "count": guid_occurrences[guid],
                "types": dict(sorted(file_types[guid].items())),
                "names": sorted(names[guid]),
            }
            for guid in sorted(guid_occurrences)
        },
    }


def compare_summaries(summaries: list[dict[str, Any]]) -> list[dict[str, Any]]:
    comparisons: list[dict[str, Any]] = []
    for left_index, left in enumerate(summaries):
        left_guids = set(left["files"])
        for right in summaries[left_index + 1 :]:
            right_guids = set(right["files"])
            shared = left_guids & right_guids
            comparisons.append(
                {
                    "left": left["label"],
                    "right": right["label"],
                    "shared_unique_guids": len(shared),
                    "left_only": len(left_guids - right_guids),
                    "right_only": len(right_guids - left_guids),
                    "jaccard": round(
                        len(shared) / len(left_guids | right_guids), 6
                    )
                    if left_guids or right_guids
                    else 1.0,
                }
            )
    return comparisons


def guid_matrix(summaries: list[dict[str, Any]]) -> dict[str, dict[str, int]]:
    matrix: dict[str, dict[str, int]] = {}
    all_guids = sorted({guid for summary in summaries for guid in summary["files"]})
    for guid in all_guids:
        matrix[guid] = {
            summary["label"]: summary["files"].get(guid, {}).get("count", 0)
            for summary in summaries
        }
    return matrix


def _parse_specification(value: str) -> tuple[str, Path]:
    if "=" not in value:
        raise argparse.ArgumentTypeError("expected LABEL=REPORT")
    label, raw_path = value.split("=", 1)
    if not label or not raw_path:
        raise argparse.ArgumentTypeError("expected non-empty LABEL=REPORT")
    return label, Path(raw_path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "reports", nargs="+", type=_parse_specification, metavar="LABEL=REPORT"
    )
    parser.add_argument(
        "--include-matrix",
        action="store_true",
        help="include the potentially large per-GUID occurrence matrix",
    )
    parser.add_argument(
        "--candidate",
        action="append",
        default=[],
        help="include full records for this candidate GUID (repeatable)",
    )
    args = parser.parse_args()

    summaries: list[dict[str, Any]] = []
    for label, path in args.reports:
        if not path.is_file():
            parser.error(f"not a regular report file: {path}")
        rows = parse_report(path.read_text(encoding="utf-8", errors="replace"))
        summaries.append(report_summary(label, path, rows))

    result: dict[str, Any] = {
        "reports": [
            {key: value for key, value in summary.items() if key != "files"}
            for summary in summaries
        ],
        "comparisons": compare_summaries(summaries),
    }
    candidates = [candidate.upper() for candidate in args.candidate]
    if candidates:
        result["candidates"] = {
            guid: {
                summary["label"]: summary["files"].get(guid)
                for summary in summaries
                if guid in summary["files"]
            }
            for guid in candidates
        }
    if args.include_matrix:
        result["guid_matrix"] = guid_matrix(summaries)
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
