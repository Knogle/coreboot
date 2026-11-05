#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Prepare pinned AM4 inputs and run the ordinary coreboot build, without hardware."""

import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
LOCK = json.loads((HERE / "sources.lock.json").read_text())


def digest(path):
    value = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(data)
    return value.hexdigest()


def relative(name):
    path = PurePosixPath(name)
    if not name or path.is_absolute() or ".." in path.parts or str(path) != name:
        raise ValueError(f"Noncanonical relative path: {name!r}")
    return path


def run(argv, **kwargs):
    return subprocess.run([str(x) for x in argv], check=True, **kwargs)


def revision(path):
    return subprocess.check_output(["git", "-C", str(path), "rev-parse", "HEAD"], text=True).strip()


def checked_file(path, expected):
    path = Path(path)
    if not path.is_file() or path.is_symlink():
        raise ValueError(f"Expected regular file: {path}")
    if path.stat().st_size != expected["bytes"] or digest(path) != expected["sha256"]:
        raise ValueError(f"Input hash or length mismatch: {path}")


def prepare(args):
    cache = args.offline_cache.resolve(strict=True) if args.offline_cache else None
    for item in LOCK["repositories"]:
        destination = ROOT / relative(item["path"])
        if (destination / ".git").exists():
            if revision(destination) != item["commit"]:
                raise ValueError(f"Preserve the unexpected existing checkout: {destination}")
            continue
        if destination.exists() and any(destination.iterdir()):
            raise ValueError(f"Refusing to replace a populated directory: {destination}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        if cache:
            artifact = cache / relative(item["cache_artifact"]["path"])
            checked_file(artifact, item["cache_artifact"])
            boundary = item["cache_shallow_boundary"]
            if boundary:
                shallow = cache / relative(boundary["path"])
                checked_file(shallow, boundary)
                run(["git", "init", "--quiet", destination])
                shutil.copyfile(shallow, destination / ".git/shallow")
                with artifact.open("rb") as stream:
                    run(["git", "-C", destination, "index-pack", "--stdin"], stdin=stream,
                        stdout=subprocess.DEVNULL)
            else:
                run(["git", "clone", "--quiet", "--no-checkout", artifact, destination])
        else:
            run(["git", "init", "--quiet", destination])
            run(["git", "-C", destination, "remote", "add", "origin", item["url"]])
            run(["git", "-C", destination, "fetch", "--depth=1", "origin", item["commit"]])
        if cache:
            remotes = subprocess.check_output(["git", "-C", str(destination), "remote"], text=True).split()
            action = "set-url" if "origin" in remotes else "add"
            run(["git", "-C", destination, "remote", action, "origin", item["url"]])
        run(["git", "-C", destination, "checkout", "--quiet", "--detach", item["commit"]])
        if revision(destination) != item["commit"]:
            raise ValueError(f"Unexpected revision: {destination}")
        print("Prepared", item["path"], item["commit"], flush=True)


def input_files(root):
    root = root.resolve(strict=True)
    result = {}
    for name, expected in LOCK["private_inputs"].items():
        path = root / relative(name)
        if path.resolve(strict=True) != path:
            raise ValueError(f"Symlink traversal in binary input: {name}")
        checked_file(path, expected)
        result[name] = path
    return result


def pack_inputs(args):
    inputs = input_files(args.source_root)
    manifest = json.dumps({"schema": 1, "files": LOCK["private_inputs"]},
                          sort_keys=True, indent=2).encode() + b"\n"
    with args.output.open("xb") as raw:
        with gzip.GzipFile(filename="", fileobj=raw, mode="wb", mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w", format=tarfile.USTAR_FORMAT) as archive:
                for name in sorted(["manifest.json", *inputs]):
                    data = manifest if name == "manifest.json" else inputs[name].read_bytes()
                    info = tarfile.TarInfo(name)
                    info.size, info.mode = len(data), 0o400
                    archive.addfile(info, io.BytesIO(data))
    print("Private input package:", args.output, "SHA256", digest(args.output))


def import_inputs(args):
    expected = LOCK["private_inputs"]
    with tarfile.open(args.archive, "r:gz") as archive:
        members = archive.getmembers()
        names = [m.name for m in members]
        if len(names) != len(set(names)) or set(names) != {"manifest.json", *expected}:
            raise ValueError("Archive membership does not match the pinned binary inputs")
        for member in members:
            relative(member.name)
            if not member.isfile():
                raise ValueError("Only regular input files are accepted")
            maximum = 1024 * 1024 if member.name == "manifest.json" else expected[member.name]["bytes"]
            if member.size > maximum:
                raise ValueError(f"Unexpected archive member length: {member.name}")
        manifest = json.load(archive.extractfile("manifest.json"))
        if manifest != {"schema": 1, "files": expected}:
            raise ValueError("Archive manifest differs from the public input lock")
        contents = {}
        # Validate the complete package before creating any destination file.
        for name, row in expected.items():
            data = archive.extractfile(name).read()
            if len(data) != row["bytes"] or hashlib.sha256(data).hexdigest() != row["sha256"]:
                raise ValueError(f"Corrupt binary input: {name}")
            path = ROOT / relative(name)
            if path.is_symlink() or path.resolve(strict=False) != path:
                raise ValueError(f"Symlink traversal in input destination: {name}")
            if path.exists():
                checked_file(path, row)
            contents[name] = data
        for name, data in contents.items():
            path = ROOT / name
            path.parent.mkdir(parents=True, exist_ok=True)
            if not path.exists():
                with path.open("xb") as output:
                    output.write(data)
    input_files(ROOT)
    print("Imported and verified", len(expected), "private binary inputs")


def verify_tools(tools):
    expected = json.loads((HERE / "sdk.lock.json").read_text())
    for name, row in expected["files"].items():
        path = tools / relative(name)
        if "symlink" in row:
            if not path.is_symlink() or str(path.readlink()) != row["symlink"]:
                raise ValueError(f"SDK symlink mismatch: {name}")
        else:
            checked_file(path, row)
    print("Verified pinned SDK files:", len(expected["files"]), flush=True)


def build(args):
    input_files(ROOT)
    for item in LOCK["repositories"]:
        if revision(ROOT / item["path"]) != item["commit"]:
            raise ValueError(f"Run prepare for the pinned dependency: {item['path']}")
    environment = os.environ.copy()
    environment.update(SOURCE_DATE_EPOCH=str(LOCK["source_date_epoch"]),
                       GIT_ALLOW_PROTOCOL="none", LC_ALL="C", TZ="UTC")
    if args.tools:
        tools = args.tools.resolve(strict=True)
        verify_tools(tools)
        crossgcc = tools / "crossgcc-own/bin"
        environment.update(CPATH=str(tools / "ipxe-host-tools/usr/include"),
                           LIBRARY_PATH=str(tools / "ipxe-host-tools/usr/lib/x86_64-linux-gnu"),
                           PATH=str(tools / "edk2-host-tools/bin") + os.pathsep
                           + str(tools / "ipxe-host-tools/usr/bin") + os.pathsep + environment["PATH"])
    else:
        crossgcc = ROOT / "util/crossgcc/xgcc/bin"
    if not (crossgcc / "i386-elf-gcc").is_file():
        raise ValueError("Build crossgcc-i386 or supply the pinned --tools SDK")
    cases = ["seabios", "edk2"] if args.payload == "all" else [args.payload]
    reports = []
    for payload in cases:
        obj = "build/b565d4-" + payload
        config = ".config.b565d4-" + payload
        profile = ROOT / LOCK["default_profiles"][payload]
        directory = ROOT / obj
        if directory.exists() or (ROOT / config).exists():
            raise ValueError(f"Use a fresh checkout; preserve the existing build: {obj}")
        directory.mkdir(parents=True)
        command = ["make", "obj=" + obj, "DOTCONFIG=" + config,
                   "XGCCPATH=" + str(crossgcc) + "/", "BUILD_TIMELESS=0",
                   "PYTHON=python3", "UPDATED_SUBMODULES=1",
                   "HOSTCC=clang-19", "HOSTCXX=clang++-19"]
        for phase, extra in [("defconfig", ["KBUILD_DEFCONFIG=" + str(profile), "defconfig"]),
                             ("build", ["-j" + str(args.jobs)])]:
            print("Building", payload, phase, flush=True)
            with (directory / (phase + ".log")).open("xb") as output:
                run(command + extra, cwd=ROOT, env=environment,
                    stdout=output, stderr=subprocess.STDOUT)
        rom = directory / "coreboot.rom"
        if rom.stat().st_size != 33554432:
            raise ValueError("Expected complete32-MiB image")
        # Independent physical FV, payload placement and paired ABL checks.
        run(["python3", ROOT / "porting/asrock_am4/tests/check_native_payload_layout.py",
             "--obj", directory, "--config", ROOT / config,
             "--template", ROOT / "3rdparty/blobs/mainboard/asrockrack/b565d4_v1l/pre-x86-template.rom",
             "--out", directory / "layout-check"], cwd=ROOT, env=environment)
        workspace_command = ["python3", ROOT / "porting/asrock_am4/tests/check_native_workspace.py",
                             "--obj", directory, "--out", directory / "workspace-check"]
        if payload == "seabios":
            workspace_command.append("--seabios")
        run(workspace_command, cwd=ROOT, env=environment)
        report = {"payload": payload, "source_commit": revision(ROOT),
                  "profile": str(profile.relative_to(ROOT)), "source_date_epoch": LOCK["source_date_epoch"],
                  "rom": str(rom.relative_to(ROOT)), "bytes": rom.stat().st_size,
                  "sha256": digest(rom), "normal_make": True, "hardware_selected": False}
        (directory / "build-result.json").write_text(json.dumps(report, indent=2) + "\n")
        reports.append(report)
        print(json.dumps(report), flush=True)
    (ROOT / "build/b565d4-results.json").write_text(json.dumps(reports, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    sources = commands.add_parser("prepare", help="Fetch exact public source commits")
    sources.add_argument("--offline-cache", type=Path,
                         help="Optional preserved source package with verified repository packs")
    sources.set_defaults(function=prepare)
    pack = commands.add_parser("pack-inputs", help="Export private qualified binary inputs")
    pack.add_argument("--source-root", type=Path, required=True)
    pack.add_argument("--output", type=Path, required=True)
    pack.set_defaults(function=pack_inputs)
    unpack = commands.add_parser("import-inputs", help="Verify and import a private input package")
    unpack.add_argument("archive", type=Path)
    unpack.set_defaults(function=import_inputs)
    make = commands.add_parser("build", help="Build and check normal profiles; no hardware access")
    make.add_argument("--payload", choices=["all", "edk2", "seabios"], default="all")
    make.add_argument("--tools", type=Path, help="Pinned relocatable SDK tools directory")
    make.add_argument("-j", "--jobs", type=int, default=4)
    make.set_defaults(function=build)
    args = parser.parse_args()
    if getattr(args, "jobs", 1) < 1:
        parser.error("jobs must be positive")
    args.function(args)


if __name__ == "__main__":
    main()
