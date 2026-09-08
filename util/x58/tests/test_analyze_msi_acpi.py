from __future__ import annotations

from pathlib import Path
import struct
import sys
import unittest


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))

from analyze_msi_acpi import scan_facs, scan_standard_tables


def header(signature: bytes, length: int, revision: int = 1) -> bytearray:
    table = bytearray(length)
    table[:4] = signature
    struct.pack_into("<I", table, 4, length)
    table[8] = revision
    table[10:16] = b"7522MS"
    table[16:24] = b"A7522800"
    struct.pack_into("<I", table, 24, 1)
    table[28:32] = b"TEST"
    struct.pack_into("<I", table, 32, 2)
    return table


def finish_checksum(table: bytearray) -> bytes:
    table[9] = (-sum(table)) & 0xFF
    return bytes(table)


class AnalyzeMsiAcpiTests(unittest.TestCase):
    def test_fadt_fields_and_checksum(self) -> None:
        table = header(b"FACP", 132, 2)
        struct.pack_into("<H", table, 46, 9)
        struct.pack_into("<I", table, 48, 0xB2)
        table[52:56] = bytes((0xE1, 0x1E, 0, 0xE2))
        struct.pack_into("<IIIII", table, 56, 0x800, 0, 0x804, 0, 0x850)
        struct.pack_into("<III", table, 76, 0x808, 0x820, 0)
        table[88:94] = bytes((4, 2, 1, 4, 16, 0))
        struct.pack_into("<H", table, 109, 3)
        struct.pack_into("<I", table, 112, 0x4A5)
        table[116:120] = bytes((1, 8, 0, 0))
        struct.pack_into("<Q", table, 120, 0xCF9)
        table[128] = 6

        records = scan_standard_tables(b"pad" + finish_checksum(table))
        self.assertEqual(len(records), 1)
        record = records[0]
        self.assertTrue(record["checksum_valid"])
        self.assertEqual(record["offset"], "0x3")
        self.assertEqual(record["decoded"]["sci_interrupt"], 9)
        self.assertEqual(record["decoded"]["pm_timer_block"], "0x808")
        self.assertEqual(record["decoded"]["reset_register"]["address"], "0xcf9")
        self.assertEqual(record["decoded"]["reset_value"], "0x6")

    def test_madt_mcfg_and_hpet(self) -> None:
        madt = header(b"APIC", 74)
        struct.pack_into("<II", madt, 36, 0xFEE00000, 1)
        madt[44:52] = bytes((0, 8, 1, 0x80, 1, 0, 0, 0))
        madt[52:64] = bytes((1, 12, 1, 0)) + struct.pack("<II", 0xFEC00000, 0)
        madt[64:74] = bytes((2, 10, 0, 9)) + struct.pack("<IH", 9, 0xD)

        mcfg = header(b"MCFG", 60)
        struct.pack_into("<QHBBI", mcfg, 44, 0xE0000000, 0, 0, 0xFF, 0)

        hpet = header(b"HPET", 56)
        struct.pack_into("<I", hpet, 36, 0x8086A201)
        hpet[40:44] = bytes((0, 64, 0, 0))
        struct.pack_into("<Q", hpet, 44, 0xFED00000)
        struct.pack_into("<BH", hpet, 52, 0, 14318)

        records = scan_standard_tables(bytes(madt) + bytes(mcfg) + bytes(hpet))
        self.assertEqual([item["signature"] for item in records], ["APIC", "MCFG", "HPET"])
        self.assertEqual(records[0]["decoded"]["entries"][0]["apic_id"], 0x80)
        self.assertEqual(records[0]["decoded"]["entries"][2]["flags"], "0xd")
        self.assertEqual(records[1]["decoded"]["allocations"][0]["end_bus"], 0xFF)
        self.assertEqual(records[2]["decoded"]["base_address"]["address"], "0xfed00000")
        self.assertEqual(records[2]["decoded"]["minimum_clock_tick"], 14318)

    def test_rsdt_xsdt_and_truncated_signature(self) -> None:
        rsdt = header(b"RSDT", 44)
        struct.pack_into("<II", rsdt, 36, 0x12340000, 0)
        xsdt = header(b"XSDT", 52)
        struct.pack_into("<QQ", xsdt, 36, 0x123456780000, 0)
        data = b"FACP\xff\xff\xff\xff" + bytes(rsdt) + bytes(xsdt)
        records = scan_standard_tables(data)
        self.assertEqual(len(records), 2)
        self.assertEqual(records[0]["decoded"]["entries"], ["0x12340000", "0x0"])
        self.assertEqual(records[1]["decoded"]["entries"], ["0x123456780000", "0x0"])

    def test_facs_is_not_treated_as_checksum_table(self) -> None:
        facs = bytearray(64)
        facs[:4] = b"FACS"
        struct.pack_into("<I", facs, 4, 64)
        struct.pack_into("<I", facs, 12, 0x11223344)
        struct.pack_into("<Q", facs, 24, 0x5566778899AABBCC)
        facs[32] = 1
        records = scan_facs(b"prefix" + bytes(facs))
        self.assertEqual(len(records), 1)
        self.assertEqual(records[0]["offset"], "0x6")
        self.assertEqual(records[0]["firmware_waking_vector"], "0x11223344")
        self.assertEqual(records[0]["x_firmware_waking_vector"], "0x5566778899aabbcc")


if __name__ == "__main__":
    unittest.main()
