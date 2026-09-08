#!/usr/bin/env python3
"""Regression tests for x58_ram_loader.py."""

from __future__ import annotations

import io
import os
import struct
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

import x58_ram_loader as loader


class FragmentedReader(io.BytesIO):
    def read(self, size: int = -1) -> bytes:
        if size < 0:
            size = 1
        return super().read(min(size, 1))


def status_frame(code: int, object_id: int, detail: int) -> bytes:
    prefix = struct.pack(
        "<4sBBHII", loader.STATUS_MAGIC, loader.VERSION, code, 0, object_id, detail
    )
    return prefix + struct.pack("<I", loader.crc32(prefix))


class RamLoaderToolTests(unittest.TestCase):
    def test_crc_vector(self) -> None:
        self.assertEqual(loader.crc32(b"123456789"), 0xCBF43926)

    def test_round_trip_data(self) -> None:
        payload = bytes(range(251))
        header, container = loader.build_object(
            payload, 0x02000000, 0x12345678, False, 0, True
        )
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "object.xrl"
            path.write_bytes(container)
            decoded, decoded_payload = loader.load_container(path)
        self.assertEqual(decoded, header)
        self.assertEqual(decoded_payload, payload)

    def test_executable_requires_verify_and_bounded_entry(self) -> None:
        with self.assertRaisesRegex(ValueError, "read-back"):
            loader.build_object(b"1234", 0x02000000, 1, True, 0, False)
        with self.assertRaisesRegex(ValueError, "entry"):
            loader.build_object(b"1234", 0x02000000, 1, True, 4, True)

    def test_data_rejects_entry(self) -> None:
        with self.assertRaisesRegex(ValueError, "data-only"):
            loader.build_object(b"1234", 0x02000000, 1, False, 1, True)

    def test_address_and_size_checks(self) -> None:
        with self.assertRaisesRegex(ValueError, "aligned"):
            loader.build_object(b"x", 0x02000001, 1, False, 0, True)
        with self.assertRaisesRegex(ValueError, "wraps"):
            loader.build_object(bytes(32), 0xFFFFFFF0, 1, False, 0, True)
        with self.assertRaisesRegex(ValueError, "empty"):
            loader.build_object(b"", 0x02000000, 1, False, 0, True)

    def test_container_corruption(self) -> None:
        _header, container = loader.build_object(
            b"payload", 0x02000000, 1, False, 0, True
        )
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.xrl"
            path.write_bytes(container[:-1] + bytes([container[-1] ^ 1]))
            with self.assertRaisesRegex(ValueError, "payload CRC"):
                loader.load_container(path)

    def test_semantically_invalid_header_is_rejected(self) -> None:
        header = loader.ObjectHeader(
            flags=0x8000,
            object_id=1,
            destination=0x02000000,
            length=1,
            entry_offset=0,
            payload_crc32=loader.crc32(b"x"),
        )
        with self.assertRaisesRegex(ValueError, "unknown flags"):
            loader.ObjectHeader.unpack(header.pack())

    def test_status_scanner_and_nak(self) -> None:
        frame = status_frame(loader.STATUS_HEADER_ACK, 0x11223344, 0x55667788)
        status = loader.read_valid_status(
            io.BytesIO(b"rommon text\r\n" + frame), float("inf")
        )
        self.assertEqual(status.object_id, 0x11223344)
        self.assertEqual(status.detail, 0x55667788)
        nak = loader.Status(loader.STATUS_NAK_BASE + 15, 1, 2)
        self.assertTrue(nak.is_nak)
        self.assertEqual(nak.result_name, "payload-crc")

    def test_fragmented_status_is_reassembled(self) -> None:
        frame = status_frame(loader.STATUS_READY, 0, 0x1000)
        status = loader.read_valid_status(FragmentedReader(frame), float("inf"))
        self.assertEqual(status.code, loader.STATUS_READY)
        self.assertEqual(status.detail, 0x1000)

    @unittest.skipUnless(os.name == "posix", "termios transport is POSIX-only")
    def test_posix_serial_transport(self) -> None:
        import pty
        import select

        master, slave = pty.openpty()
        try:
            path = os.ttyname(slave)
            with loader.PosixSerialPort(path, 115200, 0.05, 0.25) as port:
                os.write(master, b"abc")
                self.assertEqual(port.read(3), b"abc")
                self.assertEqual(port.read(1), b"")
                self.assertEqual(port.write(b"xyz"), 3)
                port.flush()
                readable, _writable, _exceptional = select.select(
                    [master], [], [], 0.25
                )
                self.assertTrue(readable, "serial write did not reach PTY master")
                self.assertEqual(os.read(master, 3), b"xyz")
        finally:
            os.close(master)
            os.close(slave)

    @unittest.skipUnless(os.name == "posix", "termios transport is POSIX-only")
    def test_posix_serial_flush_has_deadline(self) -> None:
        port = object.__new__(loader.PosixSerialPort)
        port._fd = 123
        port._write_timeout = 0.01
        port._termios = SimpleNamespace(TIOCOUTQ=0x5411)

        def report_stuck_queue(
            _fd: int, _request: int, queued: object, _mutate: bool
        ) -> int:
            queued[0] = 1
            return 0

        with mock.patch("fcntl.ioctl", side_effect=report_stuck_queue):
            with self.assertRaisesRegex(TimeoutError, "draining"):
                port.flush()


if __name__ == "__main__":
    unittest.main()
