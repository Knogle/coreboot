#!/usr/bin/env python3
"""Exercise statically recovered MSI/Intel B7 PHY-prefix writes.

This is a volatile B06J SerialICE experiment.  It does not access SPI flash.
The original channel selector and PHY payload values are restored in ``finally``.
Only the pinned channel-2/rank-0 target configuration is supported.
"""

from __future__ import annotations

import argparse
import json

from live_x58_rd_sweep import CHANNEL2, PHY_BUSY, PHY_COMMAND, PHY_COMMON, SerialICE
from live_x58_rd_sweep import X58Experiment
from live_x58_rcven_sweep import MC_ODT_PARAMS2, prepare_ioh_scratch, rcven_point


CHANNEL_MASK = 3 << 25
COMMON_SELECTOR = 3
COMMON_CHAIN_BASE = 0x02FB

# MSI MINIT fffca65a..fffca6d0, reached from B7 at fffcb6cb on the
# cold/non-resume path.  Descriptor indices are resolved from the pinned
# fffd9310 template and fffd9ae8 special-descriptor table.
B7_PREFIX = (
    (0x00AB, 5, 0x07, 1),
    (0x0086, 5, 0x07, 1),
    (0x00C2, 6, 0x0A, 1),
    (0x00BC, 6, 0x04, 1),
)

# The next unconditional writes at fffcaa45..fffcac7d for PCI device
# 8086:2c70 revision 2.  Conditional registered-DIMM/topology fields are
# intentionally excluded until their workspace predicates are reconstructed.
B7_REV2_BASE = B7_PREFIX + (
    (0x0043, 4, 0x02, 1),
    (0x021B, 8, 0x21, 1),
    (0x00DB, 5, 0x02, 1),
    (0x01E9, 10, 0x01, 0),
    (0x0209, 8, 0x01, 0),
    (0x00E0, 9, 0x12, 1),
    (0x0197, 8, 0x06, 1),
)

# Fixed channel-local prefix at fffcacbc..fffcaf51 for the present channel 2,
# cold path and unbuffered/non-ECC hypothesis.  The 0x0a0a value follows from
# workspace e7c == 0 and the normal channel flag lacking bit 3; keep this
# isolated from the less conditional rev2-base profile.
B7_CHANNEL2_PREFIX = (
    (0x09C3, 4, 0x03, 1),
    (0x09AD, 4, 0x03, 1),
    (0x0A5C, 6, 0x0F, 1),
    (0x0988, 8, 0x3F, 1),
    (0x0973, 14, 0xFFF, 1),
    (0x0A62, 3, 0x01, 1),
    (0x0A0A, 5, 0x03, 1),
    (0x099A, 9, 0x00, 1),
    (0x09D9, 9, 0x00, 1),
    (0x0A1C, 9, 0x00, 1),
    (0x0A53, 9, 0x00, 1),
)


def emit(event: str, **fields: object) -> None:
    print(json.dumps({"event": event, **fields}, separators=(",", ":")),
          flush=True)


def select(experiment: X58Experiment, original: int, selector: int) -> int:
    value = (original & ~CHANNEL_MASK) | selector << 25
    experiment.poll_clear(PHY_COMMON, PHY_COMMAND, PHY_BUSY)
    experiment.pci_write32(PHY_COMMON, 0x5C, value)
    if experiment.pci_read32(PHY_COMMON, 0x5C) != value:
        raise RuntimeError(f"PHY selector {selector} readback mismatch")
    return value


def common_phy_read(experiment: X58Experiment, start: int, width: int) -> int:
    end = start + width - 1
    mask = (1 << (width - 2)) - 1
    experiment.poll_clear(PHY_COMMON, PHY_COMMAND, PHY_BUSY)
    experiment.pci_write32(PHY_COMMON, PHY_COMMAND,
                           0x80000000 | (COMMON_CHAIN_BASE - end))
    experiment.poll_clear(PHY_COMMON, PHY_COMMAND, 0x80000000)
    return experiment.pci_read32(PHY_COMMON, 0xFC) & mask


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", default="/dev/ttyUSB1")
    parser.add_argument("--sweep", type=lambda value: int(value, 0),
                        default=0x20, help="first/equal RD sweep value")
    parser.add_argument("--stop", type=lambda value: int(value, 0),
                        help="optional inclusive final RD sweep value")
    parser.add_argument("--profile",
                        choices=("prefix", "rev2-base", "rev2-channel2"),
                        default="prefix")
    parser.add_argument("--channel-delay", type=lambda value: int(value, 0),
                        help=("also program recovered channel PHY field "
                              "0x0a02/8 to this value (0..0xff)"))
    parser.add_argument("--operation", choices=("rd", "rcven"), default="rd")
    parser.add_argument("--byte-delay", type=float, default=0.0005)
    parser.add_argument("--timeout", type=float, default=1.0)
    parser.add_argument(
        "--special-bank4", type=lambda value: int(value, 0),
        help=("explicit MSI-only bank-4 low word; omit for the Intel "
              "MRS2/3/1/0 sequence"))
    args = parser.parse_args()
    if args.channel_delay is not None and not 0 <= args.channel_delay <= 0xFF:
        parser.error("--channel-delay must be within 0x00..0xff")
    if args.channel_delay is not None and args.profile != "rev2-channel2":
        parser.error("--channel-delay requires --profile rev2-channel2")

    serial = SerialICE(args.device, args.byte_delay, args.timeout)
    experiment = None
    selector_before = None
    saved: list[tuple[int, int, int]] = []
    initial_qpi = None
    scratch_before = None
    common_profile = (B7_PREFIX if args.profile == "prefix"
                      else B7_REV2_BASE)
    channel_profile = (B7_CHANNEL2_PREFIX
                       if args.profile == "rev2-channel2" else ())
    if args.channel_delay is not None:
        # MSI fffc80f9 / Intel ffe5861c calculate this value from the
        # memory-frequency table, channel class and a channel flag.  Keep the
        # still-inferred value explicit on the command line and in the log.
        channel_profile += ((0x0A02, 8, args.channel_delay, 1),)
    stop = args.sweep if args.stop is None else args.stop
    if args.operation == "rd":
        if (args.sweep & 1 or stop & 1 or
                not 0 <= args.sweep <= stop <= 0x80):
            parser.error("RD range must be even and within 0x00..0x80")
        points = range(args.sweep, stop + 1, 2)
    else:
        if not 1 <= args.sweep <= stop <= 0x3F:
            parser.error("RCVEN range must be within 1..0x3f")
        points = range(args.sweep, stop + 1)
    try:
        version = serial.command("vi")
        if "SerialICE v1.5 B06J-X58" not in version:
            raise RuntimeError(f"unexpected endpoint: {version!r}")
        experiment = X58Experiment(serial)
        base = experiment.gate_base_state()
        initial_qpi = base["qpi_ph_pis"]

        experiment.poll_clear(PHY_COMMON, PHY_COMMAND, PHY_BUSY)
        selector_before = experiment.pci_read32(PHY_COMMON, 0x5C)
        selector_common = select(experiment, selector_before, COMMON_SELECTOR)

        for start, width, _, _ in common_profile:
            saved.append((start, width,
                          common_phy_read(experiment, start, width)))
        emit("start", base=base, selector_before=selector_before,
             selector_common=selector_common, profile=args.profile,
             operation=args.operation, channel_delay=args.channel_delay,
             special_bank4=args.special_bank4,
             saved=saved,
             sweep_start=args.sweep, sweep_stop=stop)

        for start, width, value, mode in common_profile:
            experiment.phy_write(start, width, value, mode)
        applied = [common_phy_read(experiment, start, width)
                   for start, width, _, _ in common_profile]
        emit("applied", values=applied)

        select(experiment, selector_before, 2)
        channel_saved = [(start, width, experiment.phy_read(start, width))
                         for start, width, _, _ in channel_profile]
        for start, width, value, mode in channel_profile:
            experiment.phy_write(start, width, value, mode)
        if channel_profile:
            emit("channel_applied", saved=channel_saved,
                 values=[experiment.phy_read(start, width)
                         for start, width, _, _ in channel_profile])
        if args.operation == "rcven":
            scratch_before, scratch_after = prepare_ioh_scratch(experiment)
            emit("ioh_scratch", before=scratch_before, after=scratch_after)
        for point in points:
            if args.operation == "rd":
                result = experiment.rd_point(
                    point, True, True, args.special_bank4)
                emit("rd_result", **result)
                passed = bool(result["pass_bits"])
            else:
                result = rcven_point(experiment, point, True)
                emit("rcven_result", **result)
                passed = result["pass"]
            if result["qpi_ph_pis"] != initial_qpi:
                raise RuntimeError("QPI state changed during B7-prefix trial")
            if passed:
                emit("pass", operation=args.operation, point=point)
                break
    finally:
        if experiment is not None:
            if args.operation == "rcven":
                experiment.pci_write16(CHANNEL2, MC_ODT_PARAMS2, 0)
                if scratch_before is not None:
                    experiment.pci_write32((0x00, 0x14, 1), 0x9C,
                                           scratch_before)
            if 'channel_saved' in locals() and channel_saved:
                select(experiment, selector_before, 2)
                for start, width, value in reversed(channel_saved):
                    experiment.phy_write(start, width, value, 1)
                emit("channel_restored",
                     values=[experiment.phy_read(start, width)
                             for start, width, _ in channel_saved])
            if saved:
                select(experiment, selector_before, COMMON_SELECTOR)
                for start, width, value in reversed(saved):
                    experiment.phy_write(start, width, value, 1)
                restored = [common_phy_read(experiment, start, width)
                            for start, width, _ in saved]
                qpi_after = experiment.pci_read32((0xFF, 2, 1), 0x80)
                emit("restored", values=restored, qpi_ph_pis=qpi_after,
                     qpi_unchanged=(initial_qpi is None or
                                    qpi_after == initial_qpi))
            if selector_before is not None:
                experiment.poll_clear(PHY_COMMON, PHY_COMMAND, PHY_BUSY)
                experiment.pci_write32(PHY_COMMON, 0x5C, selector_before)
        serial.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
