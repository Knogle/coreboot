#!/usr/bin/env python3
"""Run the recovered MSI-order coarse-RD sweep through B06J SerialICE.

This is a volatile hardware experiment.  It never accesses SPI flash.  The
target must already be stopped in B06J's permanent SerialICE stream mode.
Only the pinned channel-2/rank-0/non-ECC descriptor set is supported.
"""

import argparse
import json
import os
import re
import select
import termios
import time


PCI_CONFIG_ADDRESS = 0xCF8
PCI_CONFIG_DATA = 0xCFC
PHY_COMMON = (0xFF, 3, 4)
CHANNEL2 = (0xFF, 6, 0)
MC_COMMON = (0xFF, 3, 0)
PHY_COMMAND = 0xF8
PHY_DATA = 0xFC
PHY_CHAIN_BASE = 0x162A
PHY_BUSY = 0xC0000000
TRAIN_ACTION = 1
TRAIN_COMPLETE = 1 << 8
MRS_RANK_SHIFT = 20
MRS_BANK_SHIFT = 16
ASSERT_CKE = 1 << 17
DO_ZQCL = 1 << 15
IGNORE_RX = 1 << 9
STOP_ON_FAIL = 1 << 8
ZQCL_COMPLETE = 1 << 7

MC_INIT_CMD = 0x54
MC_INIT_STATUS = 0x5C
MC_DDR3_CMD = 0x60
MC_MRS_0_1 = 0x70
MC_MRS_2 = 0x74
MC_BASE_TIMING = 0x78
MC_RANK_PRESENT = 0x7C
MC_CONTROL = 0x48

EXPECTED_RANKS = 0x03
EXPECTED_MRS_0_1 = 0x08061528
EXPECTED_MRS_2 = 0x0000
EXPECTED_INIT_PARAMS = 0x063F4031
EXPECTED_BASE_TIMING = 0x00000643
EXPECTED_QPI = 0x030F0F03

RD_PULSE = (0x15FD, 0x13AE, 0x115F, 0x0F10, 0x0936,
            0x06E7, 0x0498, 0x0249, 0x0CB7)
RD_LANES = (
    ((0x13F9, 12), (0x1405, 11)),
    ((0x11AA, 12), (0x11B6, 11)),
    ((0x0F5B, 12), (0x0F67, 11)),
    ((0x0D0C, 12), (0x0D18, 11)),
    ((0x0732, 12), (0x073E, 11)),
    ((0x04E3, 12), (0x04EF, 11)),
    ((0x0294, 12), (0x02A0, 11)),
    ((0x0045, 12), (0x0051, 11)),
)


class SerialICE:
    def __init__(self, path: str, byte_delay: float, timeout: float):
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        self.byte_delay = byte_delay
        self.timeout = timeout
        attrs = termios.tcgetattr(self.fd)
        attrs[0] = 0
        attrs[1] = 0
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        attrs[3] = 0
        attrs[4] = termios.B115200
        attrs[5] = termios.B115200
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 0
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        termios.tcflush(self.fd, termios.TCIFLUSH)

    def close(self):
        os.close(self.fd)

    def command(self, body: str) -> str:
        wire = ("*" + body).encode("ascii")
        for byte in wire:
            os.write(self.fd, bytes((byte,)))
            time.sleep(self.byte_delay)

        deadline = time.monotonic() + self.timeout
        data = bytearray()
        while time.monotonic() < deadline:
            ready, _, _ = select.select((self.fd,), (), (), 0.02)
            if not ready:
                continue
            chunk = os.read(self.fd, 4096)
            if chunk:
                data.extend(chunk)
                if data.endswith(b"\r\n> ") or data.endswith(b"\n> "):
                    break
        else:
            raise TimeoutError(f"SerialICE timeout for {body!r}: {data!r}")

        text = data.decode("ascii", "replace")
        if "ERROR" in text:
            raise RuntimeError(f"SerialICE rejected {body!r}: {text!r}")
        return text

    def io_write32(self, port: int, value: int):
        self.command(f"wi{port:04x}.l={value & 0xffffffff:08x}")

    def io_write8(self, port: int, value: int):
        self.command(f"wi{port:04x}.b={value & 0xff:02x}")

    def io_write16(self, port: int, value: int):
        self.command(f"wi{port:04x}.w={value & 0xffff:04x}")

    def io_read32(self, port: int) -> int:
        text = self.command(f"ri{port:04x}.l")
        values = re.findall(r"(?m)^([0-9a-fA-F]{8})\r?$", text)
        if len(values) != 1:
            raise RuntimeError(f"ambiguous read response: {text!r}")
        return int(values[0], 16)


def config_address(bus: int, device: int, function: int, register: int) -> int:
    if register & 3:
        raise ValueError("only aligned dword PCI accesses are supported")
    return (0x80000000 | (bus << 16) | (device << 11) |
            (function << 8) | register)


class X58Experiment:
    def __init__(self, serial: SerialICE):
        self.serial = serial

    def pci_read32(self, target, register: int) -> int:
        self.serial.io_write32(PCI_CONFIG_ADDRESS,
                               config_address(*target, register))
        return self.serial.io_read32(PCI_CONFIG_DATA)

    def pci_write32(self, target, register: int, value: int):
        self.serial.io_write32(PCI_CONFIG_ADDRESS,
                               config_address(*target, register))
        self.serial.io_write32(PCI_CONFIG_DATA, value)

    def pci_write8(self, target, register: int, value: int):
        aligned = register & ~3
        self.serial.io_write32(PCI_CONFIG_ADDRESS,
                               config_address(*target, aligned))
        self.serial.io_write8(PCI_CONFIG_DATA + (register & 3), value)

    def pci_write16(self, target, register: int, value: int):
        if register & 1:
            raise ValueError("unaligned PCI word write")
        aligned = register & ~3
        self.serial.io_write32(PCI_CONFIG_ADDRESS,
                               config_address(*target, aligned))
        self.serial.io_write16(PCI_CONFIG_DATA + (register & 2), value)

    def poll_clear(self, target, register: int, mask: int,
                   timeout: float = 0.5) -> int:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            value = self.pci_read32(target, register)
            if not value & mask:
                return value
        raise TimeoutError(f"poll timeout target={target} reg={register:#x}")

    def phy_write(self, start: int, width: int, value: int, mode: int):
        if not 2 <= width <= 30 or mode not in (0, 1):
            raise ValueError("invalid PHY descriptor")
        end = start + width - 1
        mask = (1 << (width - 2)) - 1
        payload = (value & mask) | ((mode + 2) << (width - 2))
        self.poll_clear(PHY_COMMON, PHY_COMMAND, PHY_BUSY)
        self.pci_write32(PHY_COMMON, PHY_DATA, payload)
        self.pci_write32(PHY_COMMON, PHY_COMMAND, 0x40000000 | end)
        self.poll_clear(PHY_COMMON, PHY_COMMAND, 0x40000000)

    def phy_read(self, start: int, width: int) -> int:
        end = start + width - 1
        mask = (1 << (width - 2)) - 1
        self.poll_clear(PHY_COMMON, PHY_COMMAND, PHY_BUSY)
        self.pci_write32(PHY_COMMON, PHY_COMMAND,
                         0x80000000 | (PHY_CHAIN_BASE - end))
        self.poll_clear(PHY_COMMON, PHY_COMMAND, 0x80000000)
        return self.pci_read32(PHY_COMMON, PHY_DATA) & mask

    def issue_mrs(self, rank: int, bank: int, value: int):
        command = ((1 << 23) | (rank << MRS_RANK_SHIFT) |
                   (bank << MRS_BANK_SHIFT) | value)
        self.pci_write32(CHANNEL2, MC_DDR3_CMD, command)
        # MSI fffcc43a writes the command and delays; bit 23 remains set in
        # both the live target and a successful vendor-initialized capture.
        # It is a command encoding bit, not a completion bit to poll clear.
        time.sleep(0.0001)

    def issue_init_command(self, command: int, complete_mask: int):
        self.pci_write32(CHANNEL2, MC_INIT_CMD, command)
        deadline = time.monotonic() + 0.5
        while time.monotonic() < deadline:
            status = self.pci_read32(CHANNEL2, MC_INIT_STATUS)
            if status & complete_mask:
                return status
        raise TimeoutError(
            f"init command {command:#x} timeout, mask={complete_mask:#x}")

    def issue_zqcl(self, rank: int):
        command = ASSERT_CKE | DO_ZQCL | IGNORE_RX | (rank << 5)
        self.issue_init_command(command, ZQCL_COMPLETE)
        self.pci_write32(CHANNEL2, MC_INIT_CMD,
                           ASSERT_CKE | IGNORE_RX | STOP_ON_FAIL)

    def gate_base_state(self):
        ranks = self.pci_read32(CHANNEL2, MC_RANK_PRESENT) & 0xff
        mrs_0_1 = self.pci_read32(CHANNEL2, MC_MRS_0_1)
        mrs_2 = self.pci_read32(CHANNEL2, MC_MRS_2) & 0xffff
        init_params = self.pci_read32(CHANNEL2, 0x58)
        base_timing = self.pci_read32(CHANNEL2, MC_BASE_TIMING)
        qpi = self.pci_read32((0xFF, 2, 1), 0x80)
        observed = {
            "ranks": ranks,
            "mrs_0_1": mrs_0_1,
            "mrs_2": mrs_2,
            "init_params": init_params,
            "base_timing": base_timing,
            "qpi_ph_pis": qpi,
        }
        if observed != {
                "ranks": EXPECTED_RANKS,
                "mrs_0_1": EXPECTED_MRS_0_1,
                "mrs_2": EXPECTED_MRS_2,
                "init_params": EXPECTED_INIT_PARAMS,
                "base_timing": EXPECTED_BASE_TIMING,
                "qpi_ph_pis": EXPECTED_QPI}:
            raise RuntimeError(f"unexpected base state: {observed!r}")
        return observed

    def prepare_rd_point(self, controller_preamble: bool,
                         special_bank4: int | None = None):
        # MSI fffc8065 + fffd2bbd, correlated with Intel ffe37125:
        # FIFO reset, CKE state, MRS2/3/1/0 and 0x30200 after each rank,
        # then ZQCL per rank.  MSI conditionally precedes the ordinary MRS
        # series with a bank-4 command whose low word is selected from a
        # runtime DIMM table.  Intel omits that command.  Do not invent its
        # value: only issue it when an explicitly reconstructed value is
        # supplied by a caller.
        if controller_preamble:
            # Exact leading writes from MSI fffc5f0b.  Offset 0x5c is an
            # intentional byte write in the vendor code; do not replace it
            # with a dword write that could acknowledge unrelated bits.
            self.pci_write32(CHANNEL2, MC_INIT_CMD, 0x00000200)
            self.pci_write8(CHANNEL2, MC_INIT_STATUS, 0x01)
            self.pci_write32(CHANNEL2, 0x58, EXPECTED_INIT_PARAMS)
            self.pci_write32(CHANNEL2, 0x50, 0x00000001)
            time.sleep(0.0001)
        self.pci_write32(CHANNEL2, MC_INIT_CMD, 0x00020600)
        time.sleep(0.0001)
        self.pci_write32(CHANNEL2, MC_INIT_CMD, ASSERT_CKE | IGNORE_RX)
        self.pci_read32(CHANNEL2, MC_INIT_CMD)

        if special_bank4 is not None:
            if not 0 <= special_bank4 <= 0xffff:
                raise ValueError("special bank-4 value must fit in 16 bits")
            self.issue_mrs(0, 4, special_bank4)
        for rank in range(2):
            self.issue_mrs(rank, 2, EXPECTED_MRS_2)
            self.issue_mrs(rank, 3, 0)
            self.issue_mrs(rank, 1, 0x0806)
            self.issue_mrs(rank, 0, 0x1528)
            self.issue_init_command(0x00030200, 1 << 9)
        for rank in range(2):
            self.issue_zqcl(rank)

    def set_rd_pulses(self, value: int):
        for start in RD_PULSE:
            self.phy_write(start, 7, value, 1)

    def rd_point(self, sweep: int, program_pulses: bool,
                 controller_preamble: bool,
                 special_bank4: int | None = None) -> dict:
        if sweep & 1 or not 0 <= sweep <= 0x80:
            raise ValueError("RD sweep must be even and within 0x00..0x80")
        if program_pulses:
            self.set_rd_pulses(2)
        self.phy_write(0x09C7, 9, sweep, 1)
        for lane in RD_LANES:
            for start, width in lane:
                self.phy_write(start, width, 0, 0)

        self.prepare_rd_point(controller_preamble, special_bank4)
        # MSI fffd5fc5 issues RD directly; there is no intervening 0x20600.
        self.pci_write32(CHANNEL2, MC_INIT_CMD, 0x00026B01)
        deadline = time.monotonic() + 0.5
        while time.monotonic() < deadline:
            command = self.pci_read32(CHANNEL2, MC_INIT_CMD)
            status = self.pci_read32(CHANNEL2, MC_INIT_STATUS)
            if not command & TRAIN_ACTION and status & TRAIN_COMPLETE:
                break
        else:
            raise TimeoutError(f"RD training timeout at sweep {sweep:#x}")

        lanes = []
        for pair in RD_LANES:
            lanes.append([self.phy_read(*pair[0]), self.phy_read(*pair[1])])
        return {
            "sweep": sweep,
            "command": command,
            "status": status,
            "pass_bits": (status >> 3) & 0xf,
            "lanes": lanes,
            "qpi_ph_pis": self.pci_read32((0xFF, 2, 1), 0x80),
        }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--device", default="/dev/ttyUSB1")
    parser.add_argument("--start", type=lambda value: int(value, 0), default=2)
    parser.add_argument("--stop", type=lambda value: int(value, 0), default=0x80)
    parser.add_argument("--byte-delay", type=float, default=0.0015)
    parser.add_argument("--timeout", type=float, default=1.0)
    parser.add_argument("--controller-preamble", action="store_true",
                        help="apply the leading fffc5f0b controller writes")
    parser.add_argument("--pulse-once", action="store_true",
                        help="hold the nine vendor pulse fields across sweep")
    parser.add_argument("--mc-control", type=lambda value: int(value, 0),
                        help="temporarily apply ff:03.0:48 and restore it")
    parser.add_argument(
        "--special-bank4", type=lambda value: int(value, 0),
        help=("explicit MSI-only bank-4 low word; omit for the Intel "
              "MRS2/3/1/0 sequence"))
    args = parser.parse_args()

    serial = SerialICE(args.device, args.byte_delay, args.timeout)
    experiment = None
    mc_control_before = None
    pulses_set = False
    try:
        # A protocol and target-state gate before the first hardware write.
        version = serial.command("vi")
        if "SerialICE v1.5 B06J-X58" not in version:
            raise RuntimeError(f"unexpected endpoint: {version!r}")
        experiment = X58Experiment(serial)
        base_state = experiment.gate_base_state()
        initial_qpi = base_state["qpi_ph_pis"]
        if args.mc_control is not None:
            mc_control_before = experiment.pci_read32(MC_COMMON, MC_CONTROL)
            experiment.pci_write32(MC_COMMON, MC_CONTROL, args.mc_control)
            mc_control_after = experiment.pci_read32(MC_COMMON, MC_CONTROL)
            if mc_control_after != args.mc_control:
                raise RuntimeError(
                    f"MC_CONTROL readback {mc_control_after:#x} != "
                    f"requested {args.mc_control:#x}")
        else:
            mc_control_after = None
        print(json.dumps({"event": "start", **base_state,
                          "controller_preamble": args.controller_preamble,
                          "pulse_once": args.pulse_once,
                          "mc_control_before": mc_control_before,
                          "mc_control_after": mc_control_after}), flush=True)
        if args.pulse_once:
            experiment.set_rd_pulses(2)
            pulses_set = True
        for sweep in range(args.start, args.stop + 1, 2):
            result = experiment.rd_point(
                sweep, not args.pulse_once, args.controller_preamble,
                args.special_bank4)
            print(json.dumps(result, separators=(",", ":")), flush=True)
            if result["qpi_ph_pis"] != initial_qpi:
                raise RuntimeError("QPI state changed; stopping sweep")
            if result["pass_bits"]:
                print(json.dumps({"event": "pass", "sweep": sweep}),
                      flush=True)
                break
    finally:
        if experiment is not None and pulses_set:
            experiment.set_rd_pulses(0)
        if experiment is not None and mc_control_before is not None:
            experiment.pci_write32(MC_COMMON, MC_CONTROL, mc_control_before)
        serial.close()


if __name__ == "__main__":
    main()
