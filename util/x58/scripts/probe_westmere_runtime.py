#!/usr/bin/env python3
"""Run narrowly scoped, destructive Westmere runtime register probes.

This helper is intended for a disposable initramfs system.  It deliberately
does not provide arbitrary PCI writes: each operation is tied to a documented
Westmere register and records the values surrounding the write.
"""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import struct
import sys
import time


PCI_ROOT = pathlib.Path("/sys/bus/pci/devices")
EDAC_ROOT = pathlib.Path("/sys/devices/system/edac/mc/mc0")


def emit(event: str, **values: object) -> None:
    record = {"monotonic_ns": time.monotonic_ns(), "event": event, **values}
    print(json.dumps(record, sort_keys=True), flush=True)


def config_path(bdf: str) -> pathlib.Path:
    return PCI_ROOT / f"0000:{bdf}" / "config"


def read32(bdf: str, offset: int) -> int:
    with config_path(bdf).open("rb", buffering=0) as config:
        value = os.pread(config.fileno(), 4, offset)
    if len(value) != 4:
        raise OSError(f"short PCI read from {bdf}+{offset:#x}")
    return struct.unpack("<I", value)[0]


def write32(bdf: str, offset: int, value: int) -> None:
    with config_path(bdf).open("r+b", buffering=0) as config:
        written = os.pwrite(config.fileno(), struct.pack("<I", value), offset)
    if written != 4:
        raise OSError(f"short PCI write to {bdf}+{offset:#x}")


def replace_field(value: int, mask: int, field_value: int, shift: int) -> int:
    return (value & ~mask) | ((field_value << shift) & mask)


def channel_powerdown(channel: int, rank_idle: int, hold_seconds: float) -> None:
    if not 0 <= channel <= 2:
        raise ValueError("channel must be 0..2")
    if not 0 <= rank_idle <= 0x3FF:
        raise ValueError("rank-idle must fit the documented 10-bit field")

    bdf = f"ff:{4 + channel:02x}.0"
    offset = 0x78
    mask = 0x7FE0
    original = read32(bdf, offset)
    requested = replace_field(original, mask, rank_idle, 5)
    emit(
        "channel_powerdown_before",
        bdf=bdf,
        offset=offset,
        original=original,
        original_rank_idle=(original & mask) >> 5,
        requested=requested,
        requested_rank_idle=rank_idle,
    )
    try:
        write32(bdf, offset, requested)
        readback = read32(bdf, offset)
        emit("channel_powerdown_readback", value=readback)
        time.sleep(hold_seconds)
        emit("channel_powerdown_after_hold", value=read32(bdf, offset))
    finally:
        write32(bdf, offset, original)
        emit("channel_powerdown_restored", value=read32(bdf, offset))


def write_text(path: pathlib.Path, value: str) -> None:
    path.write_text(value + "\n", encoding="ascii")


def read_text(path: pathlib.Path) -> str:
    return path.read_text(encoding="ascii").strip()


def error_inject(channel: int, inject_type: int, eccmask: int, megabytes: int) -> None:
    if not EDAC_ROOT.exists():
        raise RuntimeError("i7core_edac mc0 is absent; load i7core_edac first")
    if not 0 <= channel <= 2:
        raise ValueError("channel must be 0..2")
    if not 0 <= inject_type <= 7:
        raise ValueError("inject-type must be 0..7")

    match = EDAC_ROOT / "inject_addrmatch"
    enable = EDAC_ROOT / "inject_enable"
    emit(
        "error_inject_before",
        channel=channel,
        inject_type=inject_type,
        eccmask=eccmask,
        inject_enable=read_text(enable),
        ce_count=read_text(EDAC_ROOT / "ce_count"),
        ue_count=read_text(EDAC_ROOT / "ue_count"),
    )
    write_text(enable, "0")
    write_text(match / "channel", str(channel))
    for field in ("dimm", "rank", "bank", "page", "col"):
        write_text(match / field, "any")
    write_text(EDAC_ROOT / "inject_section", "0")
    write_text(EDAC_ROOT / "inject_type", str(inject_type))
    write_text(EDAC_ROOT / "inject_eccmask", str(eccmask))
    write_text(enable, "1")
    emit(
        "error_inject_armed",
        inject_enable=read_text(enable),
        addr_match_low=read32(f"ff:{4 + channel:02x}.0", 0xF0),
        addr_match_high=read32(f"ff:{4 + channel:02x}.0", 0xF4),
        error_mask=read32(f"ff:{4 + channel:02x}.0", 0xF8),
        error_inject=read32(f"ff:{4 + channel:02x}.0", 0xFC),
    )

    # A one-shot injection is consumed by a matching memory write.  Touching a
    # sufficiently large buffer crosses all three interleaved channels.
    payload = bytearray(megabytes * 1024 * 1024)
    for offset in range(0, len(payload), 64):
        payload[offset] = (offset >> 6) & 0xFF
    emit("error_inject_trigger_complete", bytes_touched=len(payload))
    write_text(enable, "0")
    emit(
        "error_inject_disabled",
        inject_enable=read_text(enable),
        ce_count=read_text(EDAC_ROOT / "ce_count"),
        ue_count=read_text(EDAC_ROOT / "ue_count"),
        error_inject=read32(f"ff:{4 + channel:02x}.0", 0xFC),
    )


def qpi_retrain(poll_seconds: float, trigger: bool = True) -> None:
    bdf = "ff:02.1"
    control_offset = 0x6C
    status_offset = 0x80
    original = read32(bdf, control_offset)
    before_status = read32(bdf, status_offset)
    emit(
        "qpi_retrain_before" if trigger else "qpi_observe_before",
        bdf=bdf,
        control=original,
        status=before_status,
        periodic_retrain=read32(bdf, 0xA4),
    )
    if trigger:
        write32(bdf, control_offset, original | (1 << 31))
        emit("qpi_retrain_write_returned", control=read32(bdf, control_offset))

    poll_start = time.monotonic_ns()
    deadline = time.monotonic() + poll_seconds
    previous = None
    samples = 0
    transitions = []
    while time.monotonic() < deadline:
        status = read32(bdf, status_offset)
        control = read32(bdf, control_offset)
        samples += 1
        pair = (control, status)
        if pair != previous:
            transitions.append(
                {
                    "elapsed_ns": time.monotonic_ns() - poll_start,
                    "control": control,
                    "status": status,
                }
            )
            previous = pair
    emit(
        "qpi_retrain_complete" if trigger else "qpi_observe_complete",
        samples=samples,
        transitions=transitions,
    )


def qpi_trials(trials: int, window_us: int, trigger: bool) -> None:
    """Measure whether L0R follows a RETRAIN_NOW write more often than baseline."""
    path = config_path("ff:02.1")
    hits = 0
    latencies_ns = []
    observed_states: dict[str, int] = {}
    with path.open("r+b", buffering=0) as config:
        fd = config.fileno()
        original = struct.unpack("<I", os.pread(fd, 4, 0x6C))[0]
        for _ in range(trials):
            # Begin each trial from the documented L0 tracking state on both
            # halves.  A periodic retrain in progress is simply waited out.
            while True:
                status = struct.unpack("<I", os.pread(fd, 4, 0x80))[0]
                if ((status >> 8) & 0xF) == 0xF and ((status >> 16) & 0xF) == 0xF:
                    break
            start = time.monotonic_ns()
            if trigger:
                os.pwrite(fd, struct.pack("<I", original | (1 << 31)), 0x6C)
            deadline = start + window_us * 1000
            hit = False
            while time.monotonic_ns() < deadline:
                status = struct.unpack("<I", os.pread(fd, 4, 0x80))[0]
                rx_state = (status >> 8) & 0xF
                tx_state = (status >> 16) & 0xF
                if rx_state == 0xE or tx_state == 0xE:
                    hits += 1
                    latencies_ns.append(time.monotonic_ns() - start)
                    key = f"0x{status:08x}"
                    observed_states[key] = observed_states.get(key, 0) + 1
                    hit = True
                    break
            # Avoid deliberately merging consecutive retraining requests.
            if hit:
                while True:
                    status = struct.unpack("<I", os.pread(fd, 4, 0x80))[0]
                    if ((status >> 8) & 0xF) == 0xF and ((status >> 16) & 0xF) == 0xF:
                        break
            time.sleep(0.001)
    emit(
        "qpi_trial_complete",
        trigger=trigger,
        trials=trials,
        window_us=window_us,
        l0r_hits=hits,
        latencies_ns=latencies_ns,
        observed_states=observed_states,
    )


def qpi_phy_reset(poll_seconds: float) -> None:
    """Assert the documented, post-L0-unlocked QPI PHY_RESET action bit."""
    path = config_path("ff:02.1")
    with path.open("r+b", buffering=0) as config:
        fd = config.fileno()
        original = struct.unpack("<I", os.pread(fd, 4, 0x6C))[0]
        status = struct.unpack("<I", os.pread(fd, 4, 0x80))[0]
        emit("qpi_phy_reset_before", control=original, status=status)
        start = time.monotonic_ns()
        os.pwrite(fd, struct.pack("<I", original | 1), 0x6C)
        emit("qpi_phy_reset_write_returned", elapsed_ns=time.monotonic_ns() - start)
        deadline = time.monotonic() + poll_seconds
        transitions = []
        previous = None
        while time.monotonic() < deadline:
            control = struct.unpack("<I", os.pread(fd, 4, 0x6C))[0]
            status = struct.unpack("<I", os.pread(fd, 4, 0x80))[0]
            pair = (control, status)
            if pair != previous:
                transitions.append(
                    {
                        "elapsed_ns": time.monotonic_ns() - start,
                        "control": control,
                        "status": status,
                    }
                )
                previous = pair
        emit("qpi_phy_reset_complete", transitions=transitions)


def qpi_action_trials(action: str, trials: int, window_us: int) -> None:
    """Compare immediate post-write status against an observation-only control."""
    masks = {"none": 0, "retrain": 1 << 31, "phy-reset": 1}
    mask = masks[action]
    path = config_path("ff:02.1")
    hits = 0
    first_latencies_ns = []
    states: dict[str, int] = {}
    with path.open("r+b", buffering=0) as config:
        fd = config.fileno()
        original = struct.unpack("<I", os.pread(fd, 4, 0x6C))[0]
        for _ in range(trials):
            while True:
                status = struct.unpack("<I", os.pread(fd, 4, 0x80))[0]
                if status == 0x070F0F03:
                    break
            start = time.monotonic_ns()
            if mask:
                os.pwrite(fd, struct.pack("<I", original | mask), 0x6C)
            deadline = start + window_us * 1000
            first = None
            while time.monotonic_ns() < deadline:
                status = struct.unpack("<I", os.pread(fd, 4, 0x80))[0]
                if status != 0x070F0F03:
                    first = status
                    break
            if first is not None:
                hits += 1
                first_latencies_ns.append(time.monotonic_ns() - start)
                key = f"0x{first:08x}"
                states[key] = states.get(key, 0) + 1
                while struct.unpack("<I", os.pread(fd, 4, 0x80))[0] != 0x070F0F03:
                    pass
            time.sleep(0.001)
    emit(
        "qpi_action_trial_complete",
        action=action,
        trials=trials,
        window_us=window_us,
        non_l0_hits=hits,
        first_latencies_ns=first_latencies_ns,
        states=states,
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="operation", required=True)

    power = sub.add_parser("channel-powerdown")
    power.add_argument("--channel", type=int, default=0)
    power.add_argument("--rank-idle", type=int, default=16)
    power.add_argument("--hold-seconds", type=float, default=2.0)

    inject = sub.add_parser("error-inject")
    inject.add_argument("--channel", type=int, default=0)
    inject.add_argument("--inject-type", type=int, required=True)
    inject.add_argument("--eccmask", type=lambda value: int(value, 0), default=1)
    inject.add_argument("--megabytes", type=int, default=64)

    retrain = sub.add_parser("qpi-retrain")
    retrain.add_argument("--poll-seconds", type=float, default=2.0)
    observe = sub.add_parser("qpi-observe")
    observe.add_argument("--poll-seconds", type=float, default=2.0)
    trials = sub.add_parser("qpi-trials")
    trials.add_argument("--trials", type=int, default=50)
    trials.add_argument("--window-us", type=int, default=1000)
    trials.add_argument("--trigger", action="store_true")
    phy_reset = sub.add_parser("qpi-phy-reset")
    phy_reset.add_argument("--poll-seconds", type=float, default=3.0)
    action_trials = sub.add_parser("qpi-action-trials")
    action_trials.add_argument("--action", choices=("none", "retrain", "phy-reset"), required=True)
    action_trials.add_argument("--trials", type=int, default=100)
    action_trials.add_argument("--window-us", type=int, default=200)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if os.geteuid() != 0:
        raise PermissionError("root is required")
    if args.operation == "channel-powerdown":
        channel_powerdown(args.channel, args.rank_idle, args.hold_seconds)
    elif args.operation == "error-inject":
        error_inject(args.channel, args.inject_type, args.eccmask, args.megabytes)
    elif args.operation == "qpi-retrain":
        qpi_retrain(args.poll_seconds)
    elif args.operation == "qpi-observe":
        qpi_retrain(args.poll_seconds, trigger=False)
    elif args.operation == "qpi-trials":
        qpi_trials(args.trials, args.window_us, args.trigger)
    elif args.operation == "qpi-phy-reset":
        qpi_phy_reset(args.poll_seconds)
    elif args.operation == "qpi-action-trials":
        qpi_action_trials(args.action, args.trials, args.window_us)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as error:
        emit("fatal", error=type(error).__name__, detail=str(error))
        raise
