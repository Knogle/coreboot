#!/usr/bin/env python3
"""Recover literal PCI configuration accesses from MSI X58 CSI_INITDLL.

This is a profile wrapper around ``analyze_minit_pci``.  CSI_INITDLL uses the
same cdecl MMCONFIG argument order, but has its own helper addresses.  Dynamic
arguments remain unresolved; in particular, this tool does not infer the CPU
Uncore bus or IOH bus from surrounding control flow.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import analyze_minit_pci as analyzer


CSI_TEXT_SHA256 = "37b0fef95af7c176f2c7d310b67aee7b4e761cfa4173abe859d50313a9091a61"
TRACKED_REGISTERS = {"eax", "ebx", "ecx", "edx", "esi", "edi", "ebp"}
CALLER_SAVED = {"eax", "ecx", "edx"}
REGISTER_PARENT = {
	"al": "eax",
	"ah": "eax",
	"ax": "eax",
	"bl": "ebx",
	"bh": "ebx",
	"bx": "ebx",
	"cl": "ecx",
	"ch": "ecx",
	"cx": "ecx",
	"dl": "edx",
	"dh": "edx",
	"dx": "edx",
	"si": "esi",
	"di": "edi",
	"bp": "ebp",
}
CSI_WRAPPERS = {
	0xFFFEBC68: analyzer.Wrapper("read", 1),
	0xFFFEBCAE: analyzer.Wrapper("read", 2),
	0xFFFEBCF5: analyzer.Wrapper("read", 4),
	0xFFFEBD81: analyzer.Wrapper("write", 1),
	0xFFFEBDC5: analyzer.Wrapper("write", 2),
	0xFFFEBE09: analyzer.Wrapper("write", 4),
}


def _resolved_literal(operand: str, constants: dict[str, int]) -> int | None:
	value = analyzer._literal(operand)
	if value is not None:
		return value
	return constants.get(operand.strip())


def parse_csi_disassembly(text: str) -> list[dict[str, object]]:
	"""Decode arguments with conservative straight-line register constants."""
	accesses: list[dict[str, object]] = []
	pushes: list[tuple[int, str, int | None]] = []
	constants: dict[str, int] = {}

	for line in text.splitlines():
		match = analyzer.INSTRUCTION_RE.match(line)
		if not match:
			continue
		address = int(match.group("address"), 16)
		mnemonic = match.group("mnemonic")
		operands = (match.group("operands") or "").strip()
		parts = [part.strip() for part in operands.split(",", 1)]

		if mnemonic == "push":
			pushes.append((address, operands, _resolved_literal(operands, constants)))
			pushes = pushes[-12:]
			continue

		if mnemonic == "call":
			target_match = analyzer.CALL_TARGET_RE.search(operands)
			target = int(target_match.group("target"), 16) if target_match else None
			wrapper = CSI_WRAPPERS.get(target)
			if wrapper is not None:
				count = len(wrapper.argument_names)
				selected = pushes[-count:]
				arguments: dict[str, dict[str, object]] = {}
				if len(selected) == count:
					for name, (push_address, operand, value) in zip(
						wrapper.argument_names, selected, strict=True
					):
						argument: dict[str, object] = {
							"push_address": f"0x{push_address:08x}",
							"operand": operand,
							"literal": analyzer._literal(operand) is not None,
							"constant_propagated": (
								value is not None and analyzer._literal(operand) is None
							),
						}
						if value is not None:
							argument["value"] = f"0x{value:x}"
						arguments[name] = argument
				device = _resolved_literal(
					str(arguments.get("device", {}).get("value", "")), {}
				)
				function = _resolved_literal(
					str(arguments.get("function", {}).get("value", "")), {}
				)
				offset = _resolved_literal(
					str(arguments.get("offset", {}).get("value", "")), {}
				)
				role = analyzer.BDF_ROLES.get((device, function))
				accesses.append(
					{
						"call_address": f"0x{address:08x}",
						"helper_address": f"0x{target:08x}",
						"operation": wrapper.operation,
						"width_bytes": wrapper.width_bytes,
						"arguments_complete": len(selected) == count,
						"arguments": arguments,
						"device": device,
						"function": function,
						"offset": f"0x{offset:x}" if offset is not None else None,
						"role": role,
						"register": analyzer._register_name(role, offset),
						"decoded_uncore_target": role is not None,
					}
				)
			pushes = []
			for register in CALLER_SAVED:
				constants.pop(register, None)
			continue

		if mnemonic.startswith("j") or mnemonic in {"ret", "retf", "iret"}:
			pushes = []
			constants.clear()
			continue

		if mnemonic == "mov" and len(parts) == 2 and parts[0] in TRACKED_REGISTERS:
			value = _resolved_literal(parts[1], constants)
			if value is None:
				constants.pop(parts[0], None)
			else:
				constants[parts[0]] = value
			continue
		if (
			mnemonic == "xor"
			and len(parts) == 2
			and parts[0] == parts[1]
			and parts[0] in TRACKED_REGISTERS
		):
			constants[parts[0]] = 0
			continue

		# Any unmodelled write to a tracked destination invalidates that value.
		if parts and mnemonic not in {"cmp", "test", "push"}:
			destination = REGISTER_PARENT.get(parts[0], parts[0])
			if destination in TRACKED_REGISTERS:
				constants.pop(destination, None)

	return accesses


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("image", type=Path, help="locally extracted CSI_INITDLL PE")
	parser.add_argument("--objdump", default="objdump")
	parser.add_argument(
		"--only-literal-targets",
		action="store_true",
		help="omit calls whose device, function, or offset is dynamic",
	)
	args = parser.parse_args()

	analyzer.PROFILE = "msi-x58-pro-e-csi-initdll-text-37b0fef9"
	analyzer.EXPECTED_TEXT_SHA256 = CSI_TEXT_SHA256
	analyzer.LEGACY_NORMALIZED_SHA256 = ""
	analyzer.parse_disassembly = parse_csi_disassembly

	try:
		report = analyzer.analyze(args.image, args.objdump)
	except (OSError, RuntimeError, ValueError) as error:
		parser.error(str(error))

	report["profile"] = analyzer.PROFILE
	report["evidence_limit"] = (
		"Only literal cdecl device/function/offset arguments are decoded; "
		"dynamic targets and register semantics are not inferred."
	)
	if args.only_literal_targets:
		report["accesses"] = [
			item
			for item in report["accesses"]
			if item["device"] is not None
			and item["function"] is not None
			and item["offset"] is not None
		]
		report["output_filter"] = "literal device/function/offset targets only"

	print(json.dumps(report, indent=2, sort_keys=True))
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
