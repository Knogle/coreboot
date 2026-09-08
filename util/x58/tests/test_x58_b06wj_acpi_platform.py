"""Host-side contract tests; these do not establish Windows compatibility."""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
SYMBOL = "CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM"
BUILD_ID = "X58PROE-B06WJ-ACPI-PLATFORM-20260907"


def source(name):
    return (BOARD / name).read_text()


def config(name):
    return dict(re.findall(r"^(CONFIG_\w+)=(.*)$", (ROOT / name).read_text(), re.M))


class B06WJAcpiPlatformTests(unittest.TestCase):
    def test_only_config_delta_is_selector_and_identity(self):
        previous = config("configs/x58-pro-e-b06wi.config")
        current = config("configs/x58-pro-e-b06wj.config")
        self.assertEqual(current.pop(SYMBOL), "y")
        for key in ("CONFIG_MAINBOARD_PART_NUMBER", "CONFIG_LOCALVERSION"):
            previous.pop(key)
            current.pop(key)
        self.assertEqual(current, previous)

    def test_default_off_inherits_wi_and_ich10_hpet_policy(self):
        option = source("Kconfig").split("config X58_PRO_E_B06WJ_ACPI_PLATFORM", 1)[1]
        option = option.split("config X58_PRO_E_BRINGUP_STAGE", 1)[0]
        self.assertIn("depends on X58_PRO_E_B06WI_VENDOR_IRQ_ACPI", option)
        self.assertIn("default n", option)
        self.assertIn("default 0x80 if X58_PRO_E_B06WJ_ACPI_PLATFORM", option)

    def test_cpu_uid_matches_single_madt_and_has_no_power_claims(self):
        asl = source("b06wj_platform.asl")
        cpu = asl.split("Device (CP00)", 1)[1].split("}", 1)[0]
        self.assertIn('Name (_HID, "ACPI0007")', cpu)
        self.assertIn("Name (_UID, Zero)", cpu)
        self.assertIn("acpi_create_madt_one_lapic(current, 0, 0)", source("acpi_tables.c"))
        for forbidden in ("_PSS", "_CST", "_PCT", "_PRW", "_S3", "_S4", "PNP0303"):
            self.assertNotIn(forbidden, asl)

    def test_lpc_devices_and_parent_io_windows(self):
        asl = source("b06wj_platform.asl")
        for hid in ("PNP0000", "PNP0100", "PNP0B00", "PNP0103"):
            self.assertEqual(asl.count(hid), 1)
        self.assertIn("Name (_ADR, 0x001f0000)", asl)
        for irq in (0, 2, 8):
            self.assertIn(f"IRQNoFlags () {{ {irq} }}", asl)
        dsdt = source("dsdt.asl")
        self.assertIn("0x0000, 0x0000, 0x0cf7", dsdt)
        self.assertIn("0x0000, 0x0d00, 0xffff", dsdt)
        self.assertIn("0x0cf8, 0x0cf8, 0x01, 0x08", dsdt)
        self.assertIn("0x004e, 0x004e, 0x01, 0x02", dsdt)
        self.assertNotIn("0x002e, 0x002e", dsdt)

    def test_common_hpet_writer_is_table_only_and_hooked_once(self):
        acpi = source("acpi_tables.c")
        writer = acpi.split("unsigned long b06wj_write_acpi_tables", 1)[1]
        self.assertEqual(writer.count("acpi_write_hpet(dev, current, rsdp)"), 1)
        self.assertIn("HPET_BASE_ADDRESS == B06WC_ACPI_HPET_BASE", writer)
        self.assertIn("CONFIG_HPET_MIN_TICKS == 0x80", writer)
        self.assertNotRegex(writer, r"\b(?:write\d+p?|out[bwl]|pci_\w*write\w*)\s*\(")
        self.assertEqual(source("b06vn_pci.c").count(
            ".write_acpi_tables = b06wj_write_acpi_tables"), 1)
        self.assertIn(f"#if !{SYMBOL}", source("dsdt.asl"))

    def test_build_identity_in_all_stages(self):
        for name in ("bootblock.c", "romstage.c", "ramstage_rommon.c",
                     "b06wj_acpi.h", "b06wg_usb_auto.h"):
            with self.subTest(name=name):
                self.assertIn(BUILD_ID, source(name))

    def test_asl_is_pure_description_without_vendor_global_region(self):
        asl = source("b06wj_platform.asl")
        for operation in ("OperationRegion", "Store (", "Sleep (", "Stall (", "Method ("):
            self.assertNotIn(operation, asl)
        self.assertNotIn("0xffffff00", asl.lower())


if __name__ == "__main__":
    unittest.main()
