#!/usr/bin/env python3
"""Capture a read-only X58/ICH10 vendor-firmware platform snapshot.

The script is intended to run as root on an already booted reference system.
It writes only capture files below the explicitly supplied output directory.
Hardware access is read-only: PCI config space, /dev/mem, and /dev/port are
opened without write access.  In particular, the IOAPIC selector is not
changed because doing so can race with a running operating system.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import time


PCI_ROOT = Path("/sys/bus/pci/devices")
ACPI_ROOT = Path("/sys/firmware/acpi/tables")
LPC_BDF = "0000:00:1f.0"
X58_HOST_BDF = "0000:00:00.0"
EXPECTED_LPC_ID = 0x3A168086
EXPECTED_X58_HOST_ID = 0x34058086

PROC_FILES = (
    "cmdline",
    "cpuinfo",
    "interrupts",
    "ioports",
    "iomem",
    "meminfo",
    "modules",
    "version",
)


def read_exact_fd(fd: int, offset: int, length: int) -> bytes:
    result = bytearray()
    while len(result) < length:
        chunk = os.pread(fd, length - len(result), offset + len(result))
        if not chunk:
            raise OSError(
                f"short read at 0x{offset:x}: requested {length}, got {len(result)}"
            )
        result.extend(chunk)
    return bytes(result)


def read_exact(path: Path, offset: int, length: int) -> bytes:
    with path.open("rb", buffering=0) as stream:
        return read_exact_fd(stream.fileno(), offset, length)


def le16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def le32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_bytes(root: Path, relative: str, data: bytes) -> Path:
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return path


def copy_tree_files(source: Path, root: Path, relative: str) -> None:
    if not source.is_dir():
        return
    for path in sorted(source.rglob("*")):
        if path.is_file():
            write_bytes(root, str(Path(relative) / path.relative_to(source)), path.read_bytes())


def run_capture(root: Path, relative: str, argv: list[str]) -> None:
    executable = shutil.which(argv[0])
    if executable is None:
        return
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as stream:
        completed = subprocess.run(
            [executable, *argv[1:]],
            stdout=stream,
            stderr=subprocess.STDOUT,
            check=False,
        )
    if completed.returncode != 0:
        raise RuntimeError(f"{' '.join(argv)} exited {completed.returncode}")


def capture_pci(root: Path) -> dict[str, bytes]:
    captured: dict[str, bytes] = {}
    for device in sorted(PCI_ROOT.iterdir()):
        config = device / "config"
        if not config.is_file():
            continue
        data = config.read_bytes()
        captured[device.name] = data
        write_bytes(root, f"pci-config/{device.name}.bin", data)
    return captured


def capture_proc(root: Path) -> None:
    for name in PROC_FILES:
        path = Path("/proc") / name
        if path.is_file():
            write_bytes(root, f"proc/{name}", path.read_bytes())


def capture_platform_raw(root: Path, lpc: bytes) -> dict[str, object]:
    rcba_raw = le32(lpc, 0xF0)
    pmbase_raw = le32(lpc, 0x40)
    gpiobase_raw = le32(lpc, 0x48)
    rcba = rcba_raw & 0xFFFFC000
    pmbase = pmbase_raw & 0xFF80
    gpiobase = gpiobase_raw & 0xFFC0

    if not rcba_raw & 1:
        raise RuntimeError(f"ICH10 RCBA is disabled: 0x{rcba_raw:08x}")
    if not (pmbase_raw & 1) or not pmbase:
        raise RuntimeError(f"ICH10 PMBASE is disabled: 0x{pmbase_raw:08x}")
    if not (gpiobase_raw & 1) or not gpiobase:
        raise RuntimeError(f"ICH10 GPIOBASE is disabled: 0x{gpiobase_raw:08x}")

    mem_fd = os.open("/dev/mem", os.O_RDONLY | os.O_CLOEXEC | os.O_SYNC)
    try:
        rcba_data = read_exact_fd(mem_fd, rcba, 0x4000)
        write_bytes(root, "hardware/rcba-4000.bin", rcba_data)
        hptc = le32(rcba_data, 0x3404)
        hpet_base = 0xFED00000 + ((hptc & 0x3) << 12)
        if hptc & 0x80:
            hpet_data = read_exact_fd(mem_fd, hpet_base, 0x400)
            write_bytes(root, "hardware/hpet-400.bin", hpet_data)
    finally:
        os.close(mem_fd)

    port_fd = os.open("/dev/port", os.O_RDONLY | os.O_CLOEXEC)
    try:
        pm_data = read_exact_fd(port_fd, pmbase, 0x80)
        gpio_data = read_exact_fd(port_fd, gpiobase, 0x40)
        elcr = read_exact_fd(port_fd, 0x4D0, 2)
        pic_masks = bytes((
            read_exact_fd(port_fd, 0x21, 1)[0],
            read_exact_fd(port_fd, 0xA1, 1)[0],
        ))
    finally:
        os.close(port_fd)
    write_bytes(root, "hardware/pmbase-80.bin", pm_data)
    write_bytes(root, "hardware/gpiobase-40.bin", gpio_data)
    write_bytes(root, "hardware/elcr.bin", elcr)
    write_bytes(root, "hardware/pic-imr.bin", pic_masks)

    rcba_fields = {
        "D31IP": le32(rcba_data, 0x3100),
        "D30IP": le32(rcba_data, 0x3104),
        "D29IP": le32(rcba_data, 0x3108),
        "D28IP": le32(rcba_data, 0x310C),
        "D27IP": le32(rcba_data, 0x3110),
        "D26IP": le32(rcba_data, 0x3114),
        "D31IR": le16(rcba_data, 0x3140),
        "D30IR": le16(rcba_data, 0x3142),
        "D29IR": le16(rcba_data, 0x3144),
        "D28IR": le16(rcba_data, 0x3146),
        "D27IR": le16(rcba_data, 0x3148),
        "D26IR": le16(rcba_data, 0x314C),
        "OIC": rcba_data[0x31FF],
        "RTC_CONF": le32(rcba_data, 0x3400),
        "HPTC": hptc,
        "GCS": le32(rcba_data, 0x3410),
        "BUC": le32(rcba_data, 0x3414),
        "FD": le32(rcba_data, 0x3418),
        "CG": le32(rcba_data, 0x341C),
        "FDSW": le32(rcba_data, 0x3420),
        "CIR8": le32(rcba_data, 0x3430),
        "CIR9": le32(rcba_data, 0x350C),
        "PPO": le16(rcba_data, 0x3524),
        "CIR10": le32(rcba_data, 0x352C),
        "MAP": le32(rcba_data, 0x35F0),
    }
    gpio_fields = {
        "USE_SEL": le32(gpio_data, 0x00),
        "IO_SEL": le32(gpio_data, 0x04),
        "LVL": le32(gpio_data, 0x0C),
        "BLINK": le32(gpio_data, 0x18),
        "INV": le32(gpio_data, 0x2C),
        "USE_SEL2": le32(gpio_data, 0x30),
        "IO_SEL2": le32(gpio_data, 0x34),
        "LVL2": le32(gpio_data, 0x38),
    }
    pm_fields = {
        "PM1_STS": le16(pm_data, 0x00),
        "PM1_EN": le16(pm_data, 0x02),
        "PM1_CNT": le32(pm_data, 0x04),
        "PM_TMR": le32(pm_data, 0x08),
        "GPE0_STS_LO": le32(pm_data, 0x20),
        "GPE0_STS_HI": le32(pm_data, 0x24),
        "GPE0_EN_LO": le32(pm_data, 0x28),
        "GPE0_EN_HI": le32(pm_data, 0x2C),
        "SMI_EN": le32(pm_data, 0x30),
        "SMI_STS": le32(pm_data, 0x34),
        "ALT_GP_SMI_EN": le16(pm_data, 0x38),
        "ALT_GP_SMI_STS": le16(pm_data, 0x3A),
        "UPRWC": le16(pm_data, 0x3C),
    }
    return {
        "rcba_raw": rcba_raw,
        "rcba_base": rcba,
        "pmbase_raw": pmbase_raw,
        "pmbase": pmbase,
        "gpiobase_raw": gpiobase_raw,
        "gpiobase": gpiobase,
        "acpi_cntl": lpc[0x44],
        "gpio_cntl": lpc[0x4C],
        "serirq_cntl": lpc[0x64],
        "lpc_io_dec": le16(lpc, 0x80),
        "lpc_en": le16(lpc, 0x82),
        "pirq_route": [lpc[offset] for offset in (0x60, 0x61, 0x62, 0x63, 0x68, 0x69, 0x6A, 0x6B)],
        "gpio_rout": le32(lpc, 0xB8),
        "pmir": le32(lpc, 0xAC),
        "rcba": rcba_fields,
        "pm": pm_fields,
        "gpio": gpio_fields,
        "elcr": list(elcr),
        "pic_imr": list(pic_masks),
        "ioapic_note": "Not read directly: IOREGSEL writes can race with the running OS; use APIC and /proc/interrupts.",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    if os.geteuid() != 0:
        parser.error("root is required")
    if args.output.exists():
        parser.error(f"output already exists: {args.output}")
    args.output.mkdir(parents=True, mode=0o700)

    pci = capture_pci(args.output)
    try:
        lpc = pci[LPC_BDF]
        host = pci[X58_HOST_BDF]
    except KeyError as error:
        parser.error(f"required PCI function is absent: {error.args[0]}")
    if le32(lpc, 0) != EXPECTED_LPC_ID:
        parser.error(f"unexpected ICH10 LPC ID: 0x{le32(lpc, 0):08x}")
    if le32(host, 0) != EXPECTED_X58_HOST_ID:
        parser.error(f"unexpected X58 host ID: 0x{le32(host, 0):08x}")

    copy_tree_files(ACPI_ROOT, args.output, "acpi-tables")
    capture_proc(args.output)
    run_capture(args.output, "commands/lspci-nnxxxx.txt", ["lspci", "-D", "-nnxxxx"])
    run_capture(args.output, "commands/lspci-tree.txt", ["lspci", "-D", "-tvnn"])
    run_capture(args.output, "commands/dmidecode.txt", ["dmidecode"])

    metadata = {
        "capture_unix_ns": time.time_ns(),
        "hostname": os.uname().nodename,
        "kernel": " ".join(os.uname()),
        "hardware_access": "read-only",
        "platform": capture_platform_raw(args.output, lpc),
    }
    artifacts = {}
    for path in sorted(args.output.rglob("*")):
        if path.is_file():
            artifacts[str(path.relative_to(args.output))] = {
                "bytes": path.stat().st_size,
                "sha256": sha256(path),
            }
    metadata["artifacts"] = artifacts
    (args.output / "metadata.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps(metadata["platform"], indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
