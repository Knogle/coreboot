#!/usr/bin/env python3
"""Host-only source contracts for the narrowly gated B06WJ -> B06WK AML delta.

The WK gate adds exactly 44 measured/vendor-correlated IOH root routes and two
legacy VGA producer windows.  Removing either gated block must reproduce the
corresponding released WJ source byte-for-byte.  These tests do not establish
that Windows accepts the tables or that an operating system boots.
"""

import hashlib
import re
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
PRT = (BOARD / "b06wi_prt.asl").read_text()
DSDT = (BOARD / "dsdt.asl").read_text()
IASL = COREBOOT / "util/crossgcc/xgcc/bin/iasl"

SYMBOL = "CONFIG_X58_PRO_E_B06WK_ACPI_REPAIR"
WJ_PRT_SOURCE_SHA256 = (
    "d7c1adde3db2734cc3d3e61b29f7ec6b28e55b3f14dc3e955c94311759e8cbea"
)
WJ_DSDT_SOURCE_SHA256 = (
    "81780157a74f3b7fbcb3239acbccbf552504a4a11a593eea7fd475d03d679cbf"
)
IOH_DEVICES = (0x00, 0x01, 0x02, 0x04, 0x05, 0x06,
               0x07, 0x08, 0x09, 0x0A, 0x16)


def gated_body(source: str) -> str:
    matches = re.findall(
        rf"#if {re.escape(SYMBOL)}\n(.*?)#endif\n", source, re.DOTALL
    )
    if len(matches) != 1:
        raise ValueError(f"expected one {SYMBOL} block, found {len(matches)}")
    return matches[0]


def without_gate(source: str) -> str:
    return re.sub(
        rf"#if {re.escape(SYMBOL)}\n.*?#endif\n", "", source,
        count=1, flags=re.DOTALL,
    )


def with_gate(source: str) -> str:
    return re.sub(
        rf"#if {re.escape(SYMBOL)}\n(.*?)#endif\n", r"\1", source,
        count=1, flags=re.DOTALL,
    )


def sha256_text(source: str) -> str:
    return hashlib.sha256(source.encode()).hexdigest()


class B06WKAcpiRepairAslTests(unittest.TestCase):
    def test_gate_off_is_byte_exact_released_wj_source(self) -> None:
        self.assertEqual(sha256_text(without_gate(PRT)), WJ_PRT_SOURCE_SHA256)
        self.assertEqual(sha256_text(without_gate(DSDT)), WJ_DSDT_SOURCE_SHA256)

    def test_gate_has_exact_44_ioh_routes(self) -> None:
        route_pattern = re.compile(
            r"Package \(\) \{ (0x[0-9a-f]{8}), "
            r"(Zero|One|0x02|0x03),\s+Zero, (\d+) \},"
        )
        actual = route_pattern.findall(gated_body(PRT))
        pin_names = ("Zero", "One", "0x02", "0x03")
        expected = [
            (f"0x{(device << 16) | 0xffff:08x}", pin, str(16 + pin_index))
            for device in IOH_DEVICES
            for pin_index, pin in enumerate(pin_names)
        ]
        self.assertEqual(actual, expected)
        self.assertEqual(len(actual), 44)
        self.assertEqual(len(set(actual)), 44)
        self.assertNotIn("0x000dffff", gated_body(PRT))

    def test_existing_wj_root_and_child_routes_stay_outside_wk_gate(self) -> None:
        wj = without_gate(PRT)
        root = wj.split("Name (RPRT, Package ()", 1)[1]
        root = root.split("Method (_PRT", 1)[0]
        self.assertEqual(root.count("Package () {"), 20)
        self.assertEqual(wj.count("Method (_PRT"), 1)
        self.assertEqual(wj.count("Name (_PRT"), 2)
        for token in (
            "Package () { 0x0003ffff, Zero, Zero, 16 }",
            "Package () { 0x001fffff, 0x02, Zero, 18 }",
            "Package () { 0x001dffff, Zero, Zero, 23 }",
            "Device (NPE3)",
            "Device (P0P8)",
        ):
            self.assertIn(token, wj)

    def test_gate_has_only_two_exact_legacy_vga_producers(self) -> None:
        body = gated_body(DSDT)
        compact = " ".join(body.split())
        self.assertEqual(body.count("DWordMemory (ResourceProducer"), 2)
        self.assertEqual(body.count("NonCacheable, ReadWrite"), 2)
        for minimum, maximum in (
            ("0x000a0000", "0x000bffff"),
            ("0x000c0000", "0x000dffff"),
        ):
            descriptor = (
                "DWordMemory (ResourceProducer, PosDecode, MinFixed, "
                "MaxFixed, NonCacheable, ReadWrite, 0x00000000, "
                f"{minimum}, {maximum}, 0x00000000, 0x00020000, ,,, "
                "AddressRangeMemory, TypeStatic)"
            )
            self.assertIn(descriptor, compact)
        for forbidden in (
            "Device (", "Method (", "OperationRegion", "_S0", "_S3", "_S4",
            "_S5", "IO (", "WordIO (", "Store (", "Sleep (", "Stall (",
        ):
            self.assertNotIn(forbidden, body)

    def test_vga_windows_are_inside_pci0_crs_before_high_mmio(self) -> None:
        gate = DSDT.index(f"#if {SYMBOL}")
        pci0 = DSDT.index("Device (PCI0)")
        crs = DSDT.index("Name (_CRS, ResourceTemplate ()", pci0)
        high_mmio = DSDT.index("B06WC_ACPI_PCI_MMIO_BASE", crs)
        prt = DSDT.index('#include "b06wi_prt.asl"', high_mmio)
        self.assertLess(pci0, crs)
        self.assertLess(crs, gate)
        self.assertLess(gate, high_mmio)
        self.assertLess(high_mmio, prt)

    @unittest.skipUnless(IASL.is_file(), "pinned coreboot IASL is absent")
    def test_route_packages_compile_with_gate_off_and_on(self) -> None:
        wrapper = """DefinitionBlock ("", "DSDT", 2, "COREv4", "COREBOOT", 0)
{
    Scope (\\_SB)
    {
        Device (PCI0)
        {
            Name (_HID, EisaId ("PNP0A08"))
%s
        }
    }
}
"""
        with tempfile.TemporaryDirectory(prefix="b06wk-asl-hosttest-") as temp:
            tempdir = Path(temp)
            for variant, source in (
                ("wj", without_gate(PRT)),
                ("wk", with_gate(PRT)),
            ):
                asl = tempdir / f"{variant}.asl"
                asl.write_text(wrapper % source)
                result = subprocess.run(
                    [str(IASL), "-we", "-p", str(tempdir / variant), str(asl)],
                    cwd=ROOT, capture_output=True, text=True, check=False,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertRegex(
                    result.stdout, r"Compilation successful\. 0 Errors, 0 Warnings"
                )

    def test_delta_does_not_add_sleep_devices_or_register_actions(self) -> None:
        delta = gated_body(PRT) + gated_body(DSDT)
        for forbidden in (
            "_S0", "_S3", "_S4", "_S5", "Device (", "OperationRegion",
            "SystemIO", "SystemMemory", "Store (", "Notify (", "Method (",
            "Sleep (", "Stall (",
        ):
            self.assertNotIn(forbidden, delta)


if __name__ == "__main__":
    unittest.main()
