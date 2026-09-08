#!/usr/bin/env python3
"""Validate and compile B06V1 register scripts into ROMMON commands."""

from __future__ import annotations

import argparse
import dataclasses
import pathlib
import sys

FORMAT_VERSION = 1
MAX_OPS = 32
POLL_LIMIT_MAX = 1_000_000
DELAY_LIMIT_MAX = 0x01000000
PROGRAM_MAGIC = 0x31535258
FNV_OFFSET = 2166136261
FNV_PRIME = 16777619

KIND = {
    "read": 0,
    "write": 1,
    "mask": 2,
    "poll": 3,
    "delay": 4,
    "assert": 5,
}
SPACE = {"none": 0, "io": 1, "pci": 2, "mem": 3, "msr": 4}
WIDTH = {"b": 1, "w": 2, "l": 4, "q": 8}
REVERSIBLE = 1


class ScriptError(ValueError):
    """A deterministic source-validation failure."""


@dataclasses.dataclass(frozen=True)
class Value:
    lo: int = 0
    hi: int = 0

    def token(self, width: int) -> str:
        if width == 8:
            return f"{self.hi:08x}:{self.lo:08x}"
        return f"{self.lo:0{width * 2}x}"


@dataclasses.dataclass(frozen=True)
class Operation:
    kind: int
    space: int = 0
    width: int = 0
    flags: int = 0
    target: int = 0
    mask: Value = Value()
    value: Value = Value()
    limit: int = 0


def parse_hex(token: str, *, bits: int = 32) -> int:
    text = token[2:] if token.lower().startswith("0x") else token
    if not text or any(char not in "0123456789abcdefABCDEF" for char in text):
        raise ScriptError(f"not hexadecimal: {token}")
    value = int(text, 16)
    if value >= 1 << bits:
        raise ScriptError(f"value exceeds {bits} bits: {token}")
    return value


def parse_value(token: str) -> Value:
    parts = token.split(":")
    if len(parts) == 1:
        return Value(lo=parse_hex(parts[0]))
    if len(parts) != 2:
        raise ScriptError(f"invalid HI:LO value: {token}")
    return Value(lo=parse_hex(parts[1]), hi=parse_hex(parts[0]))


def value_fits(value: Value, width: int) -> bool:
    if width == 8:
        return True
    return value.hi == 0 and value.lo < 1 << (width * 8)


def target_valid(space: int, target: int, width: int) -> bool:
    if space == SPACE["msr"]:
        return width == 8
    if width not in (1, 2, 4):
        return False
    if space == SPACE["io"]:
        return target <= 0xFFFF - (width - 1)
    if space == SPACE["mem"]:
        return not target & (width - 1) and target <= 0xFFFFFFFF - (width - 1)
    if space == SPACE["pci"]:
        device = target >> 16 & 0xFF
        function = target >> 8 & 0xFF
        register = target & 0xFF
        return device <= 0x1F and function <= 7 and not register & (width - 1)
    return False


def parse_operation(tokens: list[str], line_number: int) -> Operation:
    def fail(message: str) -> ScriptError:
        return ScriptError(f"line {line_number}: {message}")

    if not tokens or tokens[0] not in KIND:
        raise fail("expected read|write|mask|poll|delay|assert")
    name = tokens[0]
    if name == "delay":
        if len(tokens) != 2:
            raise fail("delay ITER")
        limit = parse_hex(tokens[1])
        if not 0 < limit <= DELAY_LIMIT_MAX:
            raise fail(f"delay must be 1..{DELAY_LIMIT_MAX:08x}")
        return Operation(kind=KIND[name], limit=limit)
    if len(tokens) < 4 or tokens[1] not in SPACE or tokens[1] == "none":
        raise fail("operation requires SPACE TARGET WIDTH")
    space = SPACE[tokens[1]]
    target = parse_hex(tokens[2])
    if tokens[3] not in WIDTH:
        raise fail("width must be b, w, l, or q")
    width = WIDTH[tokens[3]]
    if (space == SPACE["msr"]) != (width == 8):
        raise fail("MSR requires q; io/pci/mem require b, w, or l")
    if not target_valid(space, target, width):
        raise fail("invalid, overflowing, or unaligned target")
    base = dict(kind=KIND[name], space=space, width=width, target=target)
    if name == "read":
        if len(tokens) != 4:
            raise fail("read SPACE TARGET WIDTH")
        return Operation(**base)
    if name == "write":
        if len(tokens) != 6 or tokens[5] not in ("rev", "nr"):
            raise fail("write SPACE TARGET WIDTH VALUE rev|nr")
        value = parse_value(tokens[4])
        if not value_fits(value, width):
            raise fail("write value exceeds width")
        return Operation(**base, value=value,
                         flags=REVERSIBLE if tokens[5] == "rev" else 0)
    if name == "mask":
        if len(tokens) != 7 or tokens[6] not in ("rev", "nr"):
            raise fail("mask SPACE TARGET WIDTH CLEAR SET rev|nr")
        clear = parse_value(tokens[4])
        set_value = parse_value(tokens[5])
        if not value_fits(clear, width) or not value_fits(set_value, width):
            raise fail("mask value exceeds width")
        if not (clear.lo or clear.hi or set_value.lo or set_value.hi):
            raise fail("mask is a no-op")
        return Operation(**base, mask=clear, value=set_value,
                         flags=REVERSIBLE if tokens[6] == "rev" else 0)
    if name == "assert":
        if len(tokens) != 6:
            raise fail("assert SPACE TARGET WIDTH MASK EXPECT")
        mask = parse_value(tokens[4])
        expected = parse_value(tokens[5])
        limit = 0
    else:
        if len(tokens) != 7:
            raise fail("poll SPACE TARGET WIDTH MASK EXPECT LIMIT")
        mask = parse_value(tokens[4])
        expected = parse_value(tokens[5])
        limit = parse_hex(tokens[6])
        if not 0 < limit <= POLL_LIMIT_MAX:
            raise fail(f"poll limit must be 1..{POLL_LIMIT_MAX:08x}")
    if not value_fits(mask, width) or not value_fits(expected, width):
        raise fail("mask or expected value exceeds width")
    if not (mask.lo or mask.hi):
        raise fail("comparison mask may not be zero")
    if expected.lo & ~mask.lo or expected.hi & ~mask.hi:
        raise fail("EXPECT contains bits outside MASK")
    return Operation(**base, mask=mask, value=expected, limit=limit)


def parse_script(path: pathlib.Path) -> list[Operation]:
    operations: list[Operation] = []
    for line_number, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue
        try:
            operations.append(parse_operation(line.split(), line_number))
        except ScriptError as error:
            if str(error).startswith(f"line {line_number}:"):
                raise
            raise ScriptError(f"line {line_number}: {error}") from error
        if len(operations) > MAX_OPS:
            raise ScriptError(f"line {line_number}: more than {MAX_OPS} operations")
    if not operations:
        raise ScriptError("script is empty")
    return operations


def fnv_u32(digest: int, value: int) -> int:
    for shift in range(0, 32, 8):
        digest = ((digest ^ (value >> shift & 0xFF)) * FNV_PRIME) & 0xFFFFFFFF
    return digest


def program_digest(operations: list[Operation]) -> int:
    digest = FNV_OFFSET
    fields = (PROGRAM_MAGIC, FORMAT_VERSION, len(operations))
    for value in fields:
        digest = fnv_u32(digest, value)
    for op in operations:
        for value in (
            op.kind, op.space, op.width, op.flags, op.target,
            op.mask.lo, op.mask.hi, op.value.lo, op.value.hi, op.limit,
        ):
            digest = fnv_u32(digest, value)
    return digest


def operation_command(op: Operation) -> str:
    name = next(name for name, value in KIND.items() if value == op.kind)
    if name == "delay":
        return f"script add delay {op.limit:08x}"
    space = next(name for name, value in SPACE.items() if value == op.space)
    width = next(name for name, value in WIDTH.items() if value == op.width)
    prefix = f"script add {name} {space} {op.target:08x} {width}"
    if name == "read":
        return prefix
    if name == "write":
        return f"{prefix} {op.value.token(op.width)} " \
               f"{'rev' if op.flags & REVERSIBLE else 'nr'}"
    if name == "mask":
        return f"{prefix} {op.mask.token(op.width)} {op.value.token(op.width)} " \
               f"{'rev' if op.flags & REVERSIBLE else 'nr'}"
    if name == "poll":
        return f"{prefix} {op.mask.token(op.width)} {op.value.token(op.width)} " \
               f"{op.limit:08x}"
    return f"{prefix} {op.mask.token(op.width)} {op.value.token(op.width)}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("compile", "digest", "validate"))
    parser.add_argument("source", type=pathlib.Path)
    args = parser.parse_args()
    try:
        operations = parse_script(args.source)
    except (OSError, ScriptError) as error:
        parser.error(str(error))
    digest = program_digest(operations)
    reversible = all(op.kind not in (KIND["write"], KIND["mask"]) or
                     op.flags & REVERSIBLE for op in operations)
    if args.command == "compile":
        print("script clear")
        for op in operations:
            print(operation_command(op))
        print("script seal")
        print("script list")
    elif args.command == "digest":
        print(f"{digest:08x}")
    else:
        print(f"OK ops={len(operations):02x} program_fnv={digest:08x} "
              f"auto_rollback={'yes' if reversible else 'no'}")
    print(f"PROGRAM_FNV={digest:08x} OPS={len(operations):02x} "
          f"AUTO_ROLLBACK={'YES' if reversible else 'NO'}", file=sys.stderr)
    print("Review ROMMON 'script list' before: unlock SCRIPT; "
          f"script run {digest:08x} keep|auto", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
