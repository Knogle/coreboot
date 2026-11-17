#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Build the pinned SeaBIOS CSM EFI application used by the AM4 test port."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


REPOSITORY = "https://github.com/CSMWrap/CSMWrap.git"
REVISION = "008f141f419db566f6f4ee23bfe394f6a4e638d7"
SEABIOS_REVISION = "578d260b94f62150bf6ab9149784287bd1154f06"


def run(argv, cwd=None, **kwargs):
    return subprocess.run(argv, cwd=cwd, check=True, **kwargs)


def revision(source):
    return subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()


def patch_series(here, action, source, state, patches=()):
    tool = here.parent / "patches.py"
    argv = ["python3", str(tool), action, "--source", str(source), "--state", str(state)]
    for patch in patches:
        argv.extend(["--patch", str(patch)])
    run(argv)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=8)
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    source = args.workspace.resolve() / "csmwrap"
    source.parent.mkdir(parents=True, exist_ok=True)
    if not source.exists():
        run(["git", "clone", "--no-checkout", REPOSITORY, str(source)])
        run(["git", "checkout", "--detach", REVISION], cwd=source)
    if revision(source) != REVISION:
        raise SystemExit("CSMWrap checkout is not the pinned revision; preserve it before replacing")
    state = source.parent / "patches-csmwrap"
    seabios_state = source.parent / "patches-csmwrap-seabios"
    patch_series(here, "undo", source / "seabios", seabios_state)
    patch_series(here, "undo", source, state)
    run(["git", "submodule", "update", "--init", "--checkout", "--recursive"], cwd=source)
    if revision(source / "seabios") != SEABIOS_REVISION:
        raise SystemExit("Unexpected SeaBIOS CSM revision")
    patch_series(here, "apply", source, state, [
        here / "patches/0001-am4-memory-serial-and-boot-policy.patch",
        here / "patches/0003-preserve-coreboot-cmos-option-contract.patch",
        here / "patches/0005-reclaim-temporary-csm-helper.patch",
    ])
    cmos_policy = (here.parents[2] /
                   "src/mainboard/asrockrack/b565d4_v1l/seabios/"
                   "0001-coreboot-cmos-payload-policy.patch")
    patch_series(here, "apply", source / "seabios", seabios_state, [
        here / "patches/0002-seabios-csm-serial.patch", cmos_policy,
        here / "patches/0004-reproducible-seabios-version.patch",
        here / "patches/0006-seabios-csm-thunks-resources-console.patch",
        here / "patches/0007-preserve-firmware-console-UART.patch",
    ])

    # The retained coreboot SIPI allocation at 0x30000 must remain untouched.
    # Both wrapper diagnostics and the BIOS console use AST2500 UART2.
    env = os.environ.copy()
    # SeaBIOS has its own Kconfig output tree. Inherited coreboot paths can
    # otherwise make a clean or repeated CSM build write outside that tree.
    for name in tuple(env):
        if name.startswith("KCONFIG_"):
            del env[name]
    env["PATH"] = str(here / "tools") + os.pathsep + env["PATH"]
    run(["make", "-j" + str(args.jobs), "HOST_CC=cc", "CC=gcc",
         "CPPFLAGS=-DLOW_STUB_BASE=0x40000 -DCSMWRAP_SERIAL_PORT=0x2f8 "
         "-DCSMWRAP_SERIAL_DEBUG=true", "BUILD_VERSION=008f141-am4-csm-abi2"],
        cwd=source, env=env)
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source / "bin-x86_64/csmwrap.efi", output)
    data = output.read_bytes()
    manifest = dict(repository=REPOSITORY, revision=REVISION,
                    seabios_revision=SEABIOS_REVISION, bytes=len(data),
                    sha256=hashlib.sha256(data).hexdigest(),
                    patches={p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                             for p in sorted((here / "patches").glob("*.patch"))},
                    seabios_cmos_policy_sha256=hashlib.sha256(cmos_policy.read_bytes()).hexdigest(),
                    low_stub_base="0x40000", serial_port="0x2f8",
                    reserved_os_logical_cpus=0, temporary_helper_logical_cpus=1,
                    proxy_mailbox_abi=2, real_mode_calls_on_caller=True,
                    v86_requires_firmware_helper_ownership=True,
                    serial_debug_port_advertised_while_console_owned=False,
                    requires_secure_boot_disabled=True)
    output.with_suffix(".json").write_text(json.dumps(manifest, indent=2) + "\n")
    print("CSMWrap: " + str(output) + " sha256=" + manifest["sha256"], flush=True)


if __name__ == "__main__":
    main()
