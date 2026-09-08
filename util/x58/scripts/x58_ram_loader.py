#!/usr/bin/env python3
"""Build, inspect, or transmit an X58 ROMMON RAM-loader v1 object."""

from __future__ import annotations

import argparse
import binascii
import os
import secrets
import struct
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO


HEADER_MAGIC = b"XRL1"
STATUS_MAGIC = b"XRA1"
VERSION = 1
HEADER_SIZE = 32
STATUS_SIZE = 20
DEST_ALIGNMENT = 16
ABSOLUTE_MAX_SIZE = 4 * 1024 * 1024

FLAG_READBACK_VERIFY = 1 << 0
FLAG_EXECUTABLE = 1 << 1

STATUS_READY = 0x10
STATUS_HEADER_ACK = 0x11
STATUS_COMPLETE_ACK = 0x12
STATUS_NAK_BASE = 0x80

RESULT_NAMES = (
    "ok",
    "busy",
    "bad-magic",
    "bad-version",
    "bad-header-size",
    "header-crc",
    "flags",
    "object-id",
    "size",
    "alignment",
    "range",
    "entry",
    "sink",
    "overflow",
    "incomplete",
    "payload-crc",
    "readback",
    "not-loaded",
    "not-executable",
    "execute-mismatch",
    "execute-not-armed",
    "rx-timeout",
    "rx-fault",
    "tx",
)


def crc32(data: bytes) -> int:
    return binascii.crc32(data) & 0xFFFFFFFF


def parse_u32(text: str) -> int:
    value = int(text, 0)
    if not 0 <= value <= 0xFFFFFFFF:
        raise argparse.ArgumentTypeError("value must fit in uint32")
    return value


@dataclass(frozen=True)
class ObjectHeader:
    flags: int
    object_id: int
    destination: int
    length: int
    entry_offset: int
    payload_crc32: int

    def validate(self) -> None:
        allowed_flags = FLAG_READBACK_VERIFY | FLAG_EXECUTABLE
        if self.flags & ~allowed_flags:
            raise ValueError(f"unknown flags {self.flags:#x}")
        if self.object_id == 0:
            raise ValueError("object ID zero is reserved")
        if not 0 < self.length <= ABSOLUTE_MAX_SIZE:
            raise ValueError("payload length is outside the absolute limit")
        if self.destination % DEST_ALIGNMENT:
            raise ValueError(f"destination must be {DEST_ALIGNMENT}-byte aligned")
        if self.destination + self.length > 0x100000000:
            raise ValueError("destination plus length wraps 32-bit address space")
        if self.flags & FLAG_EXECUTABLE:
            if not self.flags & FLAG_READBACK_VERIFY:
                raise ValueError("executable objects require read-back verification")
            if not 0 <= self.entry_offset < self.length:
                raise ValueError("entry offset lies outside payload")
        elif self.entry_offset != 0:
            raise ValueError("data-only objects require entry offset zero")

    def pack(self) -> bytes:
        prefix = struct.pack(
            "<4sBBHIIIII",
            HEADER_MAGIC,
            VERSION,
            HEADER_SIZE,
            self.flags,
            self.object_id,
            self.destination,
            self.length,
            self.entry_offset,
            self.payload_crc32,
        )
        assert len(prefix) == HEADER_SIZE - 4
        return prefix + struct.pack("<I", crc32(prefix))

    @classmethod
    def unpack(cls, wire: bytes) -> "ObjectHeader":
        if len(wire) != HEADER_SIZE:
            raise ValueError(f"header must be {HEADER_SIZE} bytes")
        (
            magic,
            version,
            header_size,
            flags,
            object_id,
            destination,
            length,
            entry_offset,
            payload_crc,
            header_crc,
        ) = struct.unpack("<4sBBHIIIIII", wire)
        if magic != HEADER_MAGIC:
            raise ValueError("bad header magic")
        if version != VERSION:
            raise ValueError(f"unsupported version {version}")
        if header_size != HEADER_SIZE:
            raise ValueError(f"unsupported header size {header_size}")
        if header_crc != crc32(wire[:28]):
            raise ValueError("header CRC32 mismatch")
        result = cls(
            flags,
            object_id,
            destination,
            length,
            entry_offset,
            payload_crc,
        )
        result.validate()
        return result


@dataclass(frozen=True)
class Status:
    code: int
    object_id: int
    detail: int

    @classmethod
    def unpack(cls, wire: bytes) -> "Status":
        if len(wire) != STATUS_SIZE:
            raise ValueError(f"status must be {STATUS_SIZE} bytes")
        magic, version, code, reserved, object_id, detail, frame_crc = (
            struct.unpack("<4sBBHIII", wire)
        )
        if magic != STATUS_MAGIC:
            raise ValueError("bad status magic")
        if version != VERSION:
            raise ValueError(f"unsupported status version {version}")
        if reserved != 0:
            raise ValueError("nonzero status reserved field")
        if frame_crc != crc32(wire[:16]):
            raise ValueError("status CRC32 mismatch")
        return cls(code, object_id, detail)

    @property
    def is_nak(self) -> bool:
        return self.code >= STATUS_NAK_BASE

    @property
    def result_name(self) -> str:
        if not self.is_nak:
            return "ack"
        result = self.code - STATUS_NAK_BASE
        return RESULT_NAMES[result] if result < len(RESULT_NAMES) else "unknown"


def checked_object_id(requested: int | None) -> int:
    if requested is not None:
        if requested == 0:
            raise ValueError("object ID zero is reserved")
        return requested
    generated = 0
    while generated == 0:
        generated = secrets.randbits(32)
    return generated


def build_object(
    payload: bytes,
    destination: int,
    object_id: int,
    executable: bool,
    entry_offset: int,
    readback_verify: bool,
) -> tuple[ObjectHeader, bytes]:
    if not payload:
        raise ValueError("empty payload is not permitted")
    if len(payload) > ABSOLUTE_MAX_SIZE:
        raise ValueError(f"payload exceeds {ABSOLUTE_MAX_SIZE:#x} bytes")
    if destination % DEST_ALIGNMENT:
        raise ValueError(f"destination must be {DEST_ALIGNMENT}-byte aligned")
    if destination + len(payload) > 0x100000000:
        raise ValueError("destination plus length wraps 32-bit address space")
    if executable:
        if not readback_verify:
            raise ValueError("executable objects require read-back verification")
        if not 0 <= entry_offset < len(payload):
            raise ValueError("entry offset lies outside payload")
    elif entry_offset != 0:
        raise ValueError("data-only objects require entry offset zero")
    flags = FLAG_READBACK_VERIFY if readback_verify else 0
    if executable:
        flags |= FLAG_EXECUTABLE
    header = ObjectHeader(
        flags=flags,
        object_id=object_id,
        destination=destination,
        length=len(payload),
        entry_offset=entry_offset,
        payload_crc32=crc32(payload),
    )
    header.validate()
    return header, header.pack() + payload


def load_container(path: Path) -> tuple[ObjectHeader, bytes]:
    container = path.read_bytes()
    if len(container) < HEADER_SIZE:
        raise ValueError("container is shorter than its fixed header")
    header = ObjectHeader.unpack(container[:HEADER_SIZE])
    payload = container[HEADER_SIZE:]
    if len(payload) != header.length:
        raise ValueError(
            f"payload length mismatch: header={header.length:#x}, file={len(payload):#x}"
        )
    if crc32(payload) != header.payload_crc32:
        raise ValueError("payload CRC32 mismatch")
    return header, payload


def print_header(header: ObjectHeader) -> None:
    mode = "executable" if header.flags & FLAG_EXECUTABLE else "data"
    verify = "yes" if header.flags & FLAG_READBACK_VERIFY else "no"
    print(
        f"mode={mode} object_id={header.object_id:08x} "
        f"destination={header.destination:08x} length={header.length:08x} "
        f"entry_offset={header.entry_offset:08x} "
        f"payload_crc32={header.payload_crc32:08x} readback={verify}"
    )


def read_exact(port: BinaryIO, size: int, deadline: float) -> bytes:
    result = bytearray()
    while len(result) < size and time.monotonic() < deadline:
        result += port.read(size - len(result))
    return bytes(result)


def read_valid_status(port: BinaryIO, deadline: float) -> Status:
    matched = bytearray()
    while time.monotonic() < deadline:
        byte = port.read(1)
        if not byte:
            continue
        matched += byte
        while matched and not STATUS_MAGIC.startswith(matched):
            del matched[0]
        if bytes(matched) != STATUS_MAGIC:
            continue
        tail = read_exact(port, STATUS_SIZE - len(STATUS_MAGIC), deadline)
        if len(tail) != STATUS_SIZE - len(STATUS_MAGIC):
            raise TimeoutError("truncated status frame")
        wire = bytes(matched) + tail
        try:
            return Status.unpack(wire)
        except ValueError:
            matched.clear()
    raise TimeoutError("timed out waiting for a valid XRA1 status frame")


def expect_status(status: Status, expected: int, phase: str) -> None:
    if status.is_nak:
        raise RuntimeError(
            f"{phase}: NAK {status.result_name} ({status.code:#04x}), "
            f"object={status.object_id:08x}, detail={status.detail:08x}"
        )
    if status.code != expected:
        raise RuntimeError(f"{phase}: expected status {expected:#x}, got {status.code:#x}")


class PosixSerialPort:
    """Small pyserial-free 8N1 transport for Linux/Unix ROMMON hosts."""

    def __init__(
        self,
        path: str,
        baud: int,
        read_timeout: float,
        write_timeout: float,
    ) -> None:
        try:
            import select
            import termios
        except ImportError as error:
            raise RuntimeError("POSIX serial fallback requires termios") from error

        speed = getattr(termios, f"B{baud}", None)
        if speed is None:
            raise ValueError(f"baud rate {baud} is unsupported by termios")
        self._os = os
        self._select = select
        self._termios = termios
        self._read_timeout = read_timeout
        self._write_timeout = write_timeout
        self._fd = -1
        self._saved_attributes: list[object] | None = None
        try:
            self._fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            self._saved_attributes = termios.tcgetattr(self._fd)
            attributes = termios.tcgetattr(self._fd)
            attributes[0] = 0
            attributes[1] = 0
            attributes[2] &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB)
            if hasattr(termios, "CRTSCTS"):
                attributes[2] &= ~termios.CRTSCTS
            attributes[2] |= termios.CS8 | termios.CREAD | termios.CLOCAL
            attributes[3] = 0
            attributes[4] = speed
            attributes[5] = speed
            attributes[6][termios.VMIN] = 0
            attributes[6][termios.VTIME] = 0
            termios.tcsetattr(self._fd, termios.TCSANOW, attributes)
        except Exception:
            if self._fd >= 0:
                os.close(self._fd)
                self._fd = -1
            raise

    def __enter__(self) -> "PosixSerialPort":
        return self

    def __exit__(self, exc_type: object, exc: object, traceback: object) -> None:
        self.close()

    def read(self, size: int = 1) -> bytes:
        if self._fd < 0 or size <= 0:
            return b""
        readable, _writable, _exceptional = self._select.select(
            [self._fd], [], [], self._read_timeout
        )
        if not readable:
            return b""
        try:
            return self._os.read(self._fd, size)
        except BlockingIOError:
            return b""

    def write(self, data: bytes) -> int:
        if self._fd < 0:
            raise OSError("serial port is closed")
        view = memoryview(data)
        written = 0
        deadline = time.monotonic() + self._write_timeout
        while written < len(view):
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("timed out writing serial data")
            _readable, writable, _exceptional = self._select.select(
                [], [self._fd], [], remaining
            )
            if not writable:
                raise TimeoutError("timed out writing serial data")
            try:
                count = self._os.write(self._fd, view[written:])
            except BlockingIOError:
                continue
            if count <= 0:
                raise OSError("serial write made no progress")
            written += count
        return written

    def flush(self) -> None:
        if self._fd < 0:
            raise OSError("serial port is closed")

        # tcdrain() has no timeout and may block forever when a USB/UART
        # adapter disappears.  Linux exposes the pending output byte count,
        # which lets us retain drain semantics under the caller's deadline.
        # On POSIX systems without TIOCOUTQ, write() ordering plus the ROMMON
        # phase ACKs provide the synchronization, so avoid an unbounded drain.
        if not hasattr(self._termios, "TIOCOUTQ"):
            return

        import array
        import fcntl

        deadline = time.monotonic() + self._write_timeout
        queued = array.array("i", [0])
        while True:
            queued[0] = 0
            fcntl.ioctl(self._fd, self._termios.TIOCOUTQ, queued, True)
            if queued[0] == 0:
                return
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("timed out draining serial data")
            time.sleep(min(0.005, remaining))

    def close(self) -> None:
        if self._fd < 0:
            return
        try:
            if self._saved_attributes is not None:
                self._termios.tcsetattr(
                    self._fd, self._termios.TCSANOW, self._saved_attributes
                )
        finally:
            self._os.close(self._fd)
            self._fd = -1


def open_serial_port(
    path: str, baud: int, read_timeout: float, write_timeout: float
) -> BinaryIO | PosixSerialPort:
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError:
        if os.name != "posix":
            raise RuntimeError(
                "send requires pyserial on non-POSIX hosts (python3-serial)"
            )
        print("pyserial unavailable; using POSIX termios backend", file=sys.stderr)
        return PosixSerialPort(path, baud, read_timeout, write_timeout)

    return serial.Serial(
        path,
        baudrate=baud,
        bytesize=8,
        parity="N",
        stopbits=1,
        timeout=read_timeout,
        write_timeout=write_timeout,
        xonxoff=False,
        rtscts=False,
        dsrdtr=False,
    )


def transmit(args: argparse.Namespace, header: ObjectHeader, payload: bytes) -> None:
    if args.baud <= 0:
        raise ValueError("baud rate must be positive")
    if args.timeout <= 0:
        raise ValueError("timeout must be positive")
    timeout_seconds = args.timeout
    wire_seconds = (len(payload) + HEADER_SIZE + 3 * STATUS_SIZE) * 10 / args.baud
    write_timeout_seconds = timeout_seconds + wire_seconds
    with open_serial_port(
        args.port, args.baud, 0.1, write_timeout_seconds
    ) as port:
        if args.command:
            port.write(args.command.encode("ascii") + b"\r")
            port.flush()
        ready = read_valid_status(port, time.monotonic() + timeout_seconds)
        expect_status(ready, STATUS_READY, "ready")
        port.write(header.pack())
        port.flush()
        accepted = read_valid_status(port, time.monotonic() + timeout_seconds)
        expect_status(accepted, STATUS_HEADER_ACK, "header")
        if accepted.object_id != header.object_id or accepted.detail != header.length:
            raise RuntimeError("header ACK metadata does not match the object")
        port.write(payload)
        port.flush()
        complete = read_valid_status(port, time.monotonic() + timeout_seconds)
        expect_status(complete, STATUS_COMPLETE_ACK, "payload")
        if complete.object_id != header.object_id or complete.detail != header.payload_crc32:
            raise RuntimeError("complete ACK metadata does not match the object")


def add_object_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("payload", type=Path)
    parser.add_argument("--address", type=parse_u32, required=True)
    parser.add_argument("--object-id", type=parse_u32)
    parser.add_argument("--executable", action="store_true")
    parser.add_argument("--entry-offset", type=parse_u32, default=0)
    parser.add_argument(
        "--no-readback-verify",
        action="store_true",
        help="data objects only; firmware policy may reject this",
    )


def object_from_args(args: argparse.Namespace) -> tuple[ObjectHeader, bytes]:
    payload = args.payload.read_bytes()
    object_id = checked_object_id(args.object_id)
    return build_object(
        payload=payload,
        destination=args.address,
        object_id=object_id,
        executable=args.executable,
        entry_offset=args.entry_offset,
        readback_verify=not args.no_readback_verify,
    )


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="operation", required=True)

    pack = subparsers.add_parser("pack", help="create header+payload container")
    add_object_arguments(pack)
    pack.add_argument("--output", type=Path, required=True)

    inspect = subparsers.add_parser("inspect", help="validate a container")
    inspect.add_argument("container", type=Path)

    send = subparsers.add_parser("send", help="send payload to ROMMON over UART")
    add_object_arguments(send)
    send.add_argument("--port", default="/dev/ttyUSB1")
    send.add_argument("--baud", type=int, default=115200)
    send.add_argument("--timeout", type=float, default=10.0)
    send.add_argument(
        "--command",
        default="ramload",
        help="ROMMON line command; empty string sends no command",
    )

    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        if args.operation == "inspect":
            header, _payload = load_container(args.container)
            print_header(header)
            return 0
        header, container = object_from_args(args)
        payload = container[HEADER_SIZE:]
        print_header(header)
        if args.operation == "pack":
            args.output.write_bytes(container)
            print(f"wrote {len(container)} bytes to {args.output}")
            return 0
        transmit(args, header, payload)
        print("upload complete and firmware read-back verified")
        if header.flags & FLAG_EXECUTABLE:
            print(
                "object is marked executable, but execution depends on a future "
                "firmware integration; B06V6 intentionally exposes no arm/execute "
                f"command (object {header.object_id:08x}, CRC "
                f"{header.payload_crc32:08x})"
            )
        return 0
    except (OSError, ValueError, RuntimeError, TimeoutError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
