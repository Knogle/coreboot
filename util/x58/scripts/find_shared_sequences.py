#!/usr/bin/env python3
"""Find long exact byte sequences shared by two extracted binary sections.

The rolling-hash index preserves bounded one-to-many seed relationships.  Very
repetitive and low-entropy windows are deliberately skipped, so the result is a
set of useful analysis anchors rather than a proof of binary similarity.  Every
reported match is byte-verified and maximal on its source/destination diagonal;
assigning hardware meaning still requires disassembly.
"""

from __future__ import annotations

import argparse
from pathlib import Path


HASH_BASE = 257
HASH_MASK = (1 << 64) - 1


def rolling_hashes(data: bytes, window: int):
    if window <= 0 or len(data) < window:
        return
    power = pow(HASH_BASE, window - 1, 1 << 64)
    value = 0
    for byte in data[:window]:
        value = ((value * HASH_BASE) + byte) & HASH_MASK
    yield 0, value
    for offset in range(1, len(data) - window + 1):
        value = (value - data[offset - 1] * power) & HASH_MASK
        value = ((value * HASH_BASE) + data[offset + window - 1]) & HASH_MASK
        yield offset, value


def _seed_index(
    data: bytes, seed_size: int, max_occurrences: int
) -> dict[int, list[int] | None]:
    index: dict[int, list[int] | None] = {}
    for offset, fingerprint in rolling_hashes(data, seed_size):
        positions = index.get(fingerprint)
        if positions is None and fingerprint in index:
            continue
        if positions is None:
            index[fingerprint] = [offset]
        elif len(positions) < max_occurrences:
            positions.append(offset)
        else:
            index[fingerprint] = None
    return index


def find_matches(
    first: bytes,
    second: bytes,
    seed_size: int,
    max_seed_occurrences: int = 8,
    min_unique_bytes: int = 4,
) -> list[tuple[int, int, int]]:
    if len(first) < seed_size or len(second) < seed_size:
        return []
    index = _seed_index(second, seed_size, max_seed_occurrences)

    matches: set[tuple[int, int, int]] = set()
    covered_until: dict[int, int] = {}
    for first_offset, fingerprint in rolling_hashes(first, seed_size):
        seed = first[first_offset : first_offset + seed_size]
        if len(set(seed)) < min_unique_bytes:
            continue
        second_offsets = index.get(fingerprint)
        if not second_offsets:
            continue
        for second_offset in second_offsets:
            if seed != second[second_offset : second_offset + seed_size]:
                continue
            diagonal = second_offset - first_offset
            if first_offset < covered_until.get(diagonal, 0):
                continue

            left = 0
            while (
                first_offset - left > 0
                and second_offset - left > 0
                and first[first_offset - left - 1]
                == second[second_offset - left - 1]
            ):
                left += 1

            right = seed_size
            while (
                first_offset + right < len(first)
                and second_offset + right < len(second)
                and first[first_offset + right] == second[second_offset + right]
            ):
                right += 1

            start_first = first_offset - left
            start_second = second_offset - left
            length = left + right
            matches.add((start_first, start_second, length))
            covered_until[diagonal] = max(
                covered_until.get(diagonal, 0), start_first + length
            )

    # Remove matches wholly contained in an already selected longer match.
    selected: list[tuple[int, int, int]] = []
    for candidate in sorted(matches, key=lambda item: (-item[2], item[0], item[1])):
        a, b, length = candidate
        if any(
            a >= kept_a
            and b >= kept_b
            and a + length <= kept_a + kept_length
            and b + length <= kept_b + kept_length
            for kept_a, kept_b, kept_length in selected
        ):
            continue
        selected.append(candidate)
    return selected


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("first", type=Path, help="first extracted section")
    parser.add_argument("second", type=Path, help="second extracted section")
    parser.add_argument("--seed-size", type=int, default=32)
    parser.add_argument("--limit", type=int, default=20)
    parser.add_argument(
        "--max-seed-occurrences",
        type=int,
        default=8,
        help="skip seed hashes occurring more often in the second input",
    )
    parser.add_argument(
        "--min-unique-bytes",
        type=int,
        default=4,
        help="skip seed windows containing fewer distinct byte values",
    )
    args = parser.parse_args()
    if args.seed_size < 8:
        parser.error("--seed-size must be at least 8")
    if args.max_seed_occurrences < 1:
        parser.error("--max-seed-occurrences must be positive")
    if args.min_unique_bytes < 1 or args.min_unique_bytes > 256:
        parser.error("--min-unique-bytes must be between 1 and 256")
    first = args.first.read_bytes()
    second = args.second.read_bytes()
    for first_offset, second_offset, length in find_matches(
        first,
        second,
        args.seed_size,
        args.max_seed_occurrences,
        args.min_unique_bytes,
    )[: args.limit]:
        print(f"first=0x{first_offset:x} second=0x{second_offset:x} length={length}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
