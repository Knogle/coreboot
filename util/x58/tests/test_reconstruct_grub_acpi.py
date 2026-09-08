"""Synthetic host-side tests; no real hardware or firmware writes."""

from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from reconstruct_grub_acpi import add_rows, parse_rows, read_exact, read_table


def memory_of(data, address=0x1000):
    return dict(enumerate(data, address))


def table(signature=b"DSDT", length=36):
    data = bytearray(length)
    data[:4] = signature
    struct.pack_into("<I", data, 4, length)
    data[8] = 2
    data[9] = (-sum(data)) & 255
    return data


class GrubAcpiReconstructionTests(unittest.TestCase):
    def test_ansi_crlf_and_non_row_text(self):
        raw = b"grub> hexdump\n\r\x1b[24;1H00001000  44 53 44 54  |DSDT|\n\rgrub> "
        self.assertEqual([(address, data) for _, address, data in parse_rows(raw)],
                         [(0x1000, b"DSDT")])

    def test_ascii_column_may_contain_pipe(self):
        self.assertEqual(parse_rows(b"00001000  7c 41  ||A|\r\n")[0][2], b"|A")

    def test_malformed_or_truncated_rows_rejected(self):
        for raw in (b"00001000  44 zz  |D.|", b"00001000  44 53  |D|",
                    b"00001000  44 53  "):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                parse_rows(raw)

    def test_missing_byte_is_not_zero_filled(self):
        with self.assertRaisesRegex(ValueError, "missing explicitly observed byte"):
            read_exact({0x1000: 1, 0x1002: 3}, 0x1000, 3)

    def test_repeated_matching_bytes_allowed_but_conflict_rejected(self):
        memory = {}
        add_rows(memory, [(1, 0x1000, b"AB"), (2, 0x1001, b"BC")])
        self.assertEqual(read_exact(memory, 0x1000, 3), b"ABC")
        with self.assertRaisesRegex(ValueError, "conflicting observed byte"):
            add_rows(memory, [(3, 0x1001, b"D")])

    def test_standard_table_signature_length_and_checksum(self):
        raw = table()
        reconstructed, metadata = read_table(memory_of(raw), "DSDT", 0x1000)
        self.assertEqual(reconstructed, raw)
        self.assertTrue(metadata["checksum_valid"])
        raw[10] ^= 1
        with self.assertRaisesRegex(ValueError, "checksum invalid"):
            read_table(memory_of(raw), "DSDT", 0x1000)
        with self.assertRaisesRegex(ValueError, "unexpected SSDT signature"):
            read_table(memory_of(table()), "SSDT", 0x1000)

    def test_truncated_table_rejected(self):
        with self.assertRaisesRegex(ValueError, "missing explicitly observed byte"):
            read_table(memory_of(table()[:-1]), "DSDT", 0x1000)

    def test_facs_does_not_invent_a_checksum(self):
        raw = bytearray(64)
        raw[:4] = b"FACS"
        struct.pack_into("<I", raw, 4, 64)
        _, metadata = read_table(memory_of(raw), "FACS", 0x1000)
        self.assertIsNone(metadata["checksum_valid"])

    def test_rsdp_checks_legacy_and_extended_checksums(self):
        raw = bytearray(36)
        raw[:8] = b"RSD PTR "
        raw[15] = 2
        struct.pack_into("<I", raw, 16, 0x2000)
        struct.pack_into("<I", raw, 20, 36)
        struct.pack_into("<Q", raw, 24, 0x3000)
        raw[8] = (-sum(raw[:20])) & 255
        raw[32] = (-sum(raw)) & 255
        _, metadata = read_table(memory_of(raw), "RSDP", 0x1000)
        self.assertEqual(metadata["rsdt_address"], "0x2000")
        self.assertEqual(metadata["xsdt_address"], "0x3000")
        raw[24] ^= 1
        with self.assertRaisesRegex(ValueError, "RSDP checksum invalid"):
            read_table(memory_of(raw), "RSDP", 0x1000)


if __name__ == "__main__":
    unittest.main()
