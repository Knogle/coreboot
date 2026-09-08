#!/usr/bin/env python3
"""Record a completed local development build; never read or write hardware.

Inputs are generated artifacts, source metadata and public tool versions.
The user-supplied vendor input is identified only by its pinned digest.
No absolute host path, username, network configuration or vendor bytes is
written to this manifest. Existing differing manifests are never replaced.
"""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def run(*args):
    return subprocess.check_output(args, stderr=subprocess.STDOUT).decode().strip()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_manifest(root, output, epoch, build_id):
    def git(*args):
        return run("git", "-C", str(root), *args)

    required = (
        "msi-x58-pro-e-development-spd10-coreboot-base-4MiB.rom",
        "msi-x58-pro-e-development-spd10-unpatched-4MiB.rom",
        "msi-x58-pro-e-development-spd10-deterministic-4MiB.rom",
        "msi-x58-pro-e-development-spd10-deterministic-w25q128-16MiB.rom",
        "coreboot.config", "seabios.config",
    )
    artifacts = {}
    for name in required:
        path = output / name
        artifacts[name] = {"bytes": path.stat().st_size, "sha256": digest(path)}
    if artifacts[required[3]]["bytes"] != 0x1000000:
        raise ValueError("full-chip artifact is not 16 MiB")
    source_diff = subprocess.check_output(["git", "-C", str(root), "diff", "HEAD", "--binary"])
    # Include changed source files which do not yet belong to the commit.
    untracked = subprocess.check_output([
        "git", "-C", str(root), "ls-files", "--others", "--exclude-standard", "-z"
    ]).split(b"\0")
    untracked_hashes = {}
    for name in untracked:
        if name:
            relative = name.decode()
            path = root / relative
            if path.is_file() and not path.is_symlink():
                untracked_hashes[relative] = digest(path)
    payloads = {}
    for name, directory in (("seabios", "payloads/external/SeaBIOS/seabios"),
                            ("ipxe", "payloads/external/iPXE/ipxe")):
        payloads[name] = {"commit": run("git", "-C", str(root / directory), "rev-parse", "HEAD")}
    payloads["ipxe"]["rom_sha256"] = digest(root / "payloads/external/iPXE/ipxe/ipxe.rom")
    toolbin = root / "util/crossgcc/xgcc/bin"
    return {
        "schema": 1,
        "build_id": build_id,
        "hardware_tested": False,
        "archived_wk_reproduction": False,
        "clean_rebuilds_compared": 2,
        "source": {
            "commit": git("rev-parse", "HEAD"),
            "tree": git("rev-parse", "HEAD^{tree}"),
            "commit_timestamp": int(git("show", "-s", "--format=%ct", "HEAD")),
            "tracked_diff_sha256": hashlib.sha256(source_diff).hexdigest(),
            "untracked_file_sha256": untracked_hashes,
        },
        "payload_source_date_epoch": epoch,
        "coreboot_build_metadata": "Git commit/tree/version retained; epoch is not a historical-ROM identity guarantee",
        "payloads": payloads,
        "microcode_commit": run("git", "-C", str(root / "3rdparty/intel-microcode"), "rev-parse", "HEAD"),
        "microcode_sha256": digest(root / "3rdparty/intel-microcode/intel-ucode/06-2c-02"),
        "seabios_patch_sha256": digest(root / "util/x58/patches/seabios-b52ca86-usb-deferred-trace.patch"),
        "vendor_rom_sha256": "ee7912da81a3cf47b6bedef7dedfa56cbc31723f5cdd7f0d75a7f64e336c04a8",
        "toolchain": {
            "gcc": run(str(toolbin / "i386-elf-gcc"), "--version").splitlines()[0],
            "iasl": run(str(toolbin / "iasl"), "-v"),
            "python": run("python3", "--version"),
        },
        "artifacts": artifacts,
        "distribution": "Local composite firmware contains proprietary user-supplied modules; do not redistribute",
    }


def write_exact(path, value):
    encoded = (json.dumps(value, indent=2, sort_keys=True) + "\n").encode()
    if path.exists():
        if path.read_bytes() != encoded:
            raise ValueError("refusing to replace a differing build manifest")
        return
    with path.open("xb") as stream:
        stream.write(encoded)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--coreboot-root", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--epoch", required=True, type=int)
    parser.add_argument("--build-id", required=True)
    args = parser.parse_args()
    if args.epoch < 0 or args.build_id != "X58PROE-DEVELOPMENT-SPD10":
        parser.error("expected a nonnegative epoch and the distinct development identity")
    manifest = build_manifest(args.coreboot_root, args.output_dir, args.epoch, args.build_id)
    write_exact(args.output_dir / "build-manifest.json", manifest)


if __name__ == "__main__":
    main()
