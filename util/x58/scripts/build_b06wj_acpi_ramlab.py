#!/usr/bin/env python3
"""Build hash-pinned B06WJ DSDT-only RAM-lab hypotheses, never firmware ROMs.

This host-only operation does not load AML into hardware, change coreboot
source, flash a chip, or modify a boot medium. Every output directory is new.
The unchanged control must recompile byte-identically to the released AML.
"""

from __future__ import annotations

import argparse
import difflib
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
from x58_paths import coreboot_root


ROOT = Path(__file__).resolve().parents[1]
RELEASE = ROOT / "builds/experimental/b06wj-release-20260907"
OUTPUT_ROOT = ROOT / "builds/experimental/acpi-ramlab"
AML_HASH = "63ba8a0d497cc6998bfffd35ea816d7bba5d85f811337ce0bfb21f7c854d7d03"
DSL_HASH = "ca21db13d6a409f7f13801b68d4f89a05d340029f920c5b6120be1fb2a1e5df1"
IASL_HASH = "4b1e5067ef40ec0dbbc4f4717e4fcf1596b0a39e195096444d16bbe108a122b9"

VGA = """                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x000A0000,         // Range Minimum
                    0x000BFFFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00020000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
"""
VGA_ANCHOR = """                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0xC0000000,         // Range Minimum
"""
LEGACY_SHADOW = """                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x000C0000,         // Range Minimum
                    0x000DFFFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00020000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
"""
S0 = """    Name (_S0, Package (0x04)
    {
        Zero,
        Zero,
        Zero,
        Zero
    })
"""
S5 = """    Name (_S5, Package (0x04)
    {
        0x07,
        Zero,
        Zero,
        Zero
    })
"""
STATE_ANCHOR = "    Name (OSYS, Zero)\n"
RPRT_ANCHOR = """            Name (RPRT, Package (0x14)
            {
"""
IOH_PRT_DEVICES = (0x00, 0x01, 0x02, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x16)


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def require_hash(path: Path, expected: str) -> bytes:
    data = path.read_bytes()
    if sha(data) != expected:
        raise ValueError(f"refusing changed pinned input: {path}")
    return data


def insert_once(source: str, anchor: str, addition: str) -> str:
    if source.count(anchor) != 1:
        raise ValueError("pinned DSL insertion anchor missing or ambiguous")
    return source.replace(anchor, addition + anchor, 1)


def add_ioh_root_prt(source: str) -> str:
    """Add only the MSI-vendor-correlated direct-GSI IOH root routes."""
    if source.count(RPRT_ANCHOR) != 1:
        raise ValueError("pinned RPRT anchor missing or ambiguous")
    pin_values = (("Zero", "0x10"), ("One", "0x11"),
                  ("0x02", "0x12"), ("0x03", "0x13"))
    entries = []
    for device in IOH_PRT_DEVICES:
        address = device << 16 | 0xFFFF
        for pin, gsi in pin_values:
            entries.append(
                "                Package (0x04)\n"
                "                {\n"
                f"                    0x{address:08X}, \n"
                f"                    {pin}, \n"
                "                    Zero, \n"
                f"                    {gsi}\n"
                "                }, \n\n"
            )
    replacement = """            Name (RPRT, Package (0x40)
            {
""" + "".join(entries)
    return source.replace(RPRT_ANCHOR, replacement, 1)


def run_logged(command: list[str], log: Path, compiling: bool = False) -> None:
    result = subprocess.run(command, check=False, capture_output=True, text=True, cwd=ROOT)
    with log.open("x") as stream:
        stream.write(result.stdout)
        stream.write(result.stderr)
    if result.returncode:
        raise ValueError(f"IASL failed; see {log}")
    if compiling and not re.search(r"Compilation successful\. 0 Errors, 0 Warnings", result.stdout):
        raise ValueError(f"IASL compile was not warning-free; see {log}")


def verify_header(aml: bytes, original: bytes) -> None:
    if len(aml) < 36 or aml[:4] != b"DSDT" or struct.unpack_from("<I", aml, 4)[0] != len(aml):
        raise ValueError("invalid generated DSDT header/length")
    if sum(aml) & 255:
        raise ValueError("invalid generated DSDT checksum")
    # Only length/checksum may differ in the ACPI table header.
    if aml[8:9] != original[8:9] or aml[10:36] != original[10:36]:
        raise ValueError("unexpected DSDT revision/OEM/compiler identity change")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True,
                        help="new directory beneath builds/experimental/acpi-ramlab")
    parser.add_argument("--iasl", type=Path,
                        default=coreboot_root() / "util/crossgcc/xgcc/bin/iasl")
    parser.add_argument("--include-s0-only", action="store_true",
                        help="also build an S0-only control to isolate S5 advertisement")
    parser.add_argument("--include-legacy-windows", action="store_true",
                        help="also build only the two legacy PCI memory windows A0000-BFFFF and C0000-DFFFF")
    parser.add_argument("--include-ioh-prt", action="store_true",
                        help="also build only the missing vendor-correlated IOH root direct-GSI routes")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(OUTPUT_ROOT.resolve()) or output == OUTPUT_ROOT.resolve():
        parser.error("output must be a new subdirectory of builds/experimental/acpi-ramlab")
    original = require_hash(RELEASE / "dsdt-from-rom.aml", AML_HASH)
    source = require_hash(RELEASE / "dsdt.dsl", DSL_HASH).decode()
    require_hash(args.iasl, IASL_HASH)
    if any(token in source for token in ("Name (_S0,", "Name (_S5,", "0x000A0000,")):
        raise ValueError("release unexpectedly already contains a proposed delta")
    variants = {
        "control": (source, "Unchanged release DSDT; loader-path control only."),
        "vga-only": (insert_once(source, VGA_ANCHOR, VGA),
                     "Only PCI0._CRS adds VGA producer A0000..BFFFF, length 20000, NonCacheable/ReadWrite."),
        "s0-s5-only": (insert_once(source, STATE_ANCHOR, S0 + S5),
                       "Only root _S0={0,0,0,0} and _S5={7,0,0,0}; no S3/S4/wake/SMI methods."),
    }
    if args.include_s0_only:
        variants["s0-only"] = (insert_once(source, STATE_ANCHOR, S0),
                               "Only root _S0={0,0,0,0}; does not advertise S5.")
    if args.include_legacy_windows:
        variants["legacy-windows"] = (insert_once(source, VGA_ANCHOR, VGA + LEGACY_SHADOW),
                                      "Only PCI0._CRS adds the two legacy producers A0000..BFFFF and C0000..DFFFF, each length 20000, NonCacheable/ReadWrite. No system-state package is added.")
    if args.include_ioh_prt:
        variants["ioh-prt-only"] = (
            add_ioh_root_prt(source),
            "Only PCI0.RPRT adds direct-GSI 16/17/18/19 routes for IOH devices "
            "D0,D1,D2,D4-D10,D22. Existing D3 and ICH routes stay unchanged; "
            "D13 is deliberately excluded.",
        )
    output.mkdir(parents=True, exist_ok=False)
    records = []
    for name, (text, hypothesis) in variants.items():
        directory = output / name
        directory.mkdir()
        dsl = directory / "dsdt.dsl"
        with dsl.open("x") as stream:
            stream.write(text)
        with (directory / "source.diff").open("x") as stream:
            stream.writelines(difflib.unified_diff(source.splitlines(True), text.splitlines(True),
                                                  fromfile="released-wj/dsdt.dsl", tofile=f"{name}/dsdt.dsl"))
        prefix = directory / "dsdt"
        run_logged([str(args.iasl), "-p", str(prefix), str(dsl)],
                   directory / "compile.log", compiling=True)
        aml_path = directory / "dsdt.aml"
        aml = aml_path.read_bytes()
        verify_header(aml, original)
        if name == "control" and aml != original:
            raise ValueError("unchanged baseline does not reproduce release AML exactly")
        if name != "control" and aml == original:
            raise ValueError("variant compiled without an actual AML change")
        decompiled = directory / "decompiled"
        run_logged([str(args.iasl), "-p", str(decompiled), "-d", str(aml_path)],
                   directory / "disassemble.log")
        recompiled = directory / "recompiled"
        run_logged([str(args.iasl), "-p", str(recompiled), str(directory / "decompiled.dsl")],
                   directory / "recompile.log", compiling=True)
        if (directory / "recompiled.aml").read_bytes() != aml:
            raise ValueError("compile/disassemble/recompile was not byte-identical")
        records.append({"variant": name, "hypothesis": hypothesis, "aml": f"{name}/dsdt.aml",
                        "bytes": len(aml), "sha256": sha(aml), "source_sha256": sha(text.encode()),
                        "roundtrip_byte_identical": True, "hardware_tested": False})
    manifest = {
        "baseline_aml_sha256": AML_HASH, "baseline_dsl_sha256": DSL_HASH,
        "iasl_sha256": IASL_HASH, "script_sha256": sha(Path(__file__).read_bytes()),
        "variants": records, "hardware_tested": False,
        "firmware_image_generated": False, "firmware_source_modified": False,
        "scope": "Host-built DSDT replacements for optional one-boot RAM-only loading; existing FADT/MADT/HPET/MCFG untouched.",
        "power_rationale": "S0-only is available to isolate parsing without S5 advertisement. The S0/S5 variant advertises soft-off; it does not implement board power-on, wake, S3/S4, _PTS/_WAK, SMM or reset. Loading it may allow a later OS shutdown request to write the existing PM1 control register.",
    }
    with (output / "manifest.json").open("x") as stream:
        json.dump(manifest, stream, indent=2, sort_keys=True)
        stream.write("\n")
    report = ["# B06WJ DSDT RAM-lab candidates", "", "NOT HARDWARE TESTED. These are ACPI DSDT tables, not flashable firmware images.", "",
              "| Candidate | Bytes | SHA-256 |", "|---|---:|---|"]
    report += [f"| {r['variant']} | {r['bytes']} | `{r['sha256']}` |" for r in records]
    report += ["", "The control recompiles exactly to the pinned WJ release. Every candidate passes checksum/header checks and an identical IASL compile/disassemble/recompile round trip.", "",
               "Load at most one candidate per fresh boot through the separately validated runtime loader. Do not stack hypotheses or flash these files. Loading and reverting the runtime ACPI table set is outside this host-only builder.", "",
               manifest["power_rationale"], "", "Original coreboot source, ROMs, and released AML remain unchanged. Each candidate retains its exact source.diff and IASL logs.", ""]
    with (output / "README.md").open("x") as stream:
        stream.write("\n".join(report))
    print(json.dumps(manifest, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
