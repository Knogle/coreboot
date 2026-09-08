#!/usr/bin/env python3
"""Run the recovered channel-2/rank-0 RCVEN sweep through B06J SerialICE.

This is a volatile, no-SPI experiment for the pinned MSI X58 Pro-E setup.
It expects the base state established in the current bring-up session and is
intended to run after the MSI-order coarse-RD loop.
"""

import argparse
import json
import time

from live_x58_rd_sweep import (
    CHANNEL2,
    EXPECTED_QPI,
    MC_INIT_CMD,
    MC_INIT_STATUS,
    SerialICE,
    TRAIN_ACTION,
    TRAIN_COMPLETE,
    X58Experiment,
)


IOH_SCRATCH = (0x00, 0x14, 1)
IOH_SCRATCH_9C = 0x9C
MC_ODT_PARAMS2 = 0xA0
RCVEN_COMMAND = 0x00027301
RCVEN_PASS = 1 << 4
RCVEN_GLOBAL = (0x09FA, 8)
RCVEN_LANES = (
    ((0x13BD, 9), (0x13C6, 11)),
    ((0x116E, 9), (0x1177, 11)),
    ((0x0F1F, 9), (0x0F28, 11)),
    ((0x0CD0, 9), (0x0CD9, 11)),
    ((0x06F6, 9), (0x06FF, 11)),
    ((0x04A7, 9), (0x04B0, 11)),
    ((0x0258, 9), (0x0261, 11)),
    ((0x0009, 9), (0x0012, 11)),
)


def prepare_ioh_scratch(experiment: X58Experiment) -> tuple[int, int]:
    before = experiment.pci_read32(IOH_SCRATCH, IOH_SCRATCH_9C)
    after = (before & 0xFF000000) | 0x00021000
    experiment.pci_write32(IOH_SCRATCH, IOH_SCRATCH_9C, after)
    if experiment.pci_read32(IOH_SCRATCH, IOH_SCRATCH_9C) != after:
        raise RuntimeError("00:14.1:9c did not retain MSI RCVEN setup value")
    return before, after


def rcven_point(experiment: X58Experiment, coarse: int,
                precommand: bool) -> dict:
    if not 1 <= coarse <= 0x3F:
        raise ValueError("RCVEN coarse value must be within 1..0x3f")

    # MSI fffcc039 uses a word write here, then seeds the global field and
    # all non-ECC primary/secondary result descriptors.
    experiment.pci_write16(CHANNEL2, MC_ODT_PARAMS2, 0x0100)
    experiment.phy_write(*RCVEN_GLOBAL, coarse, 1)
    for primary, secondary in RCVEN_LANES:
        experiment.phy_write(*primary, 0, 0)
        experiment.phy_write(*secondary, 0x15E, 0)

    # fffc8065 always issues the FIFO reset.  Its model-dependent branch can
    # additionally call fffc4f41 (0x30200); keep that choice explicit.
    experiment.pci_write32(CHANNEL2, MC_INIT_CMD, 0x00020600)
    time.sleep(0.0001)
    if precommand:
        experiment.issue_init_command(0x00030200, 1 << 9)

    experiment.pci_write32(CHANNEL2, MC_INIT_CMD, RCVEN_COMMAND)
    deadline = time.monotonic() + 0.5
    while time.monotonic() < deadline:
        command = experiment.pci_read32(CHANNEL2, MC_INIT_CMD)
        status = experiment.pci_read32(CHANNEL2, MC_INIT_STATUS)
        if not command & TRAIN_ACTION and status & TRAIN_COMPLETE:
            break
    else:
        raise TimeoutError(f"RCVEN timeout at coarse {coarse:#x}")

    lanes = []
    for primary, secondary in RCVEN_LANES:
        lanes.append([experiment.phy_read(*primary),
                      experiment.phy_read(*secondary)])
    return {
        "coarse": coarse,
        "command": command,
        "status": status,
        "pass": bool(status & RCVEN_PASS),
        "lanes": lanes,
        "qpi_ph_pis": experiment.pci_read32((0xFF, 2, 1), 0x80),
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--device", default="/dev/ttyUSB1")
    parser.add_argument("--start", type=lambda value: int(value, 0), default=1)
    parser.add_argument("--stop", type=lambda value: int(value, 0), default=0x3F)
    parser.add_argument("--byte-delay", type=float, default=0.0015)
    parser.add_argument("--timeout", type=float, default=1.0)
    parser.add_argument("--precommand", action="store_true",
                        help="issue model-dependent 0x30200 before RCVEN")
    parser.add_argument("--ioh-scratch", action="store_true",
                        help="apply MSI caller's temporary 00:14.1:9c value")
    args = parser.parse_args()

    serial = SerialICE(args.device, args.byte_delay, args.timeout)
    experiment = None
    scratch_before = None
    try:
        version = serial.command("vi")
        if "SerialICE v1.5 B06J-X58" not in version:
            raise RuntimeError(f"unexpected endpoint: {version!r}")
        experiment = X58Experiment(serial)
        base_state = experiment.gate_base_state()
        event = {"event": "start", **base_state,
                 "precommand": args.precommand,
                 "ioh_scratch": args.ioh_scratch}
        if args.ioh_scratch:
            scratch_before, event["ioh_scratch_after"] = (
                prepare_ioh_scratch(experiment))
            event["ioh_scratch_before"] = scratch_before
        print(json.dumps(event), flush=True)

        for coarse in range(args.start, args.stop + 1):
            result = rcven_point(experiment, coarse, args.precommand)
            print(json.dumps(result, separators=(",", ":")), flush=True)
            if result["qpi_ph_pis"] != EXPECTED_QPI:
                raise RuntimeError("QPI state changed; stopping sweep")
            if result["pass"]:
                print(json.dumps({"event": "pass", "coarse": coarse}),
                      flush=True)
                break
    finally:
        if experiment is not None:
            # fffcc039 clears the temporary ODT word after the channel loop.
            experiment.pci_write16(CHANNEL2, MC_ODT_PARAMS2, 0)
            if scratch_before is not None:
                experiment.pci_write32(IOH_SCRATCH, IOH_SCRATCH_9C,
                                       scratch_before)
        serial.close()


if __name__ == "__main__":
    main()
