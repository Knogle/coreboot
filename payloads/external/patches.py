#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Manage owned patches for external payload source checkouts."""

# The historical owner token and log prefix remain unchanged so existing
# EDK2, SeaBIOS, CSMWrap and iPXE patch states can be removed safely.

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


if not __debug__:
    raise SystemExit("Patch lifecycle requires Python assertions; do not use -O")


def git(source, patch, reverse=False, check=False):
    command = ["git", "-C", str(source), "apply"]
    if reverse:
        command.append("--reverse")
    if check:
        command.append("--check")
    return subprocess.run(command + [str(patch)], capture_output=True, check=False)


def save(state, manifest):
    temporary = state / "manifest.tmp"
    temporary.write_text(json.dumps(manifest, indent=2) + "\n")
    temporary.replace(state / "manifest.json")


def undo(source, state):
    if not state.exists():
        return
    manifest = json.loads((state / "manifest.json").read_text())
    assert manifest["owner"] == "coreboot-edk2-patches-v1"
    assert manifest["source"] == str(source)
    while manifest["patches"]:
        row = manifest["patches"][-1]
        assert Path(row["snapshot"]).name == row["snapshot"]
        patch = state / row["snapshot"]
        assert hashlib.sha256(patch.read_bytes()).hexdigest() == row["sha256"]
        if git(source, patch, reverse=True, check=True).returncode == 0:
            result = git(source, patch, reverse=True)
            assert result.returncode == 0, result.stderr.decode()
        else:
            result = git(source, patch, check=True)
            assert result.returncode == 0, "Owned edk2 patch conflicts with source changes"
        manifest["patches"].pop()
        save(state, manifest)
        patch.unlink()
    shutil.rmtree(state)


def apply(source, state, patches):
    if not patches:
        return
    assert not state.exists(), "Remove the preceding owned patches before checkout"
    state.mkdir(parents=True)
    manifest = dict(owner="coreboot-edk2-patches-v1", source=str(source), patches=[])
    save(state, manifest)
    for index, patch in enumerate(patches):
        already_applied = git(source, patch, reverse=True, check=True).returncode == 0
        if not already_applied:
            result = git(source, patch, check=True)
            assert result.returncode == 0, result.stderr.decode()
        snapshot = "%04d.patch" % index
        data = patch.read_bytes()
        (state / snapshot).write_bytes(data)
        manifest["patches"].append(dict(snapshot=snapshot, sha256=hashlib.sha256(data).hexdigest()))
        save(state, manifest)
        if not already_applied:
            result = git(source, patch)
            assert result.returncode == 0, result.stderr.decode()
        print("edk2 source patch: " + str(patch), flush=True)


def adopt(source, state, patches):
    """Record an existing complete owned series after literal reconstruction.

    A later patch can change an earlier patch's reverse-check context. Verify
    the entire series against HEAD in a temporary tree, preserving unrelated
    source edits, before recording snapshots for reverse-order removal.
    """
    assert patches and not state.exists()
    paths = set()
    for patch in patches:
        result = subprocess.run(["git", "apply", "--numstat", "-z", str(patch)],
                                capture_output=True, check=True)
        for row in result.stdout.split(b"\0"):
            if not row:
                continue
            fields = row.split(b"\t", 2)
            assert len(fields) == 3 and fields[2], "Renames are not adopted"
            name = Path(fields[2].decode())
            assert not name.is_absolute() and ".." not in name.parts
            paths.add(name)
    with tempfile.TemporaryDirectory(prefix=".patch-adopt-", dir=state.parent) as tmp:
        tree = Path(tmp)
        # Isolate git apply from an enclosing coreboot worktree's prefix.
        subprocess.run(["git", "init", "-q", str(tree)], check=True)
        for name in paths:
            result = subprocess.run(["git", "-C", str(source), "show", "HEAD:" + str(name)],
                                    capture_output=True)
            if result.returncode == 0:
                target = tree / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(result.stdout)
        for patch in patches:
            result = git(tree, patch, check=True)
            assert result.returncode == 0, result.stderr.decode()
            result = git(tree, patch)
            assert result.returncode == 0, result.stderr.decode()
        for name in paths:
            expected, actual = tree / name, source / name
            assert not expected.is_symlink() and not actual.is_symlink()
            assert expected.exists() == actual.exists(), str(name)
            if expected.exists():
                assert expected.read_bytes() == actual.read_bytes(), str(name)
    state.mkdir(parents=True)
    manifest = dict(owner="coreboot-edk2-patches-v1", source=str(source), patches=[])
    save(state, manifest)
    for index, patch in enumerate(patches):
        snapshot = "%04d.patch" % index
        data = patch.read_bytes()
        (state / snapshot).write_bytes(data)
        manifest["patches"].append(dict(snapshot=snapshot,
                                        sha256=hashlib.sha256(data).hexdigest()))
        save(state, manifest)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["undo", "apply", "adopt"])
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--state", type=Path, required=True)
    parser.add_argument("--patch", dest="patches", type=Path, action="append", default=[])
    args = parser.parse_args()
    source, state = args.source.resolve(), args.state.resolve()
    assert source != state and source not in state.parents and state not in source.parents
    if args.action == "undo":
        assert not args.patches
        undo(source, state)
    elif args.action == "apply":
        apply(source, state, [p.resolve() for p in args.patches])
    else:
        adopt(source, state, [p.resolve() for p in args.patches])


if __name__ == "__main__":
    main()
