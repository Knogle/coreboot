"""B06WK table-description contracts; not a hardware/Windows acceptance test."""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"


class B06WKFixedFeaturesTests(unittest.TestCase):
    def setUp(self):
        self.source = (BOARD / "acpi_tables.c").read_text()
        self.block = self.source.split(
            "#if CONFIG_X58_PRO_E_B06WK_ACPI_REPAIR", 1
        )[1].split("#endif", 1)[0]

    def test_only_fixed_button_flag_is_changed(self):
        assignments = re.findall(r"fadt->\w+\s*[&|^]?=\s*[^;]+;", self.block)
        self.assertEqual(assignments, ["fadt->flags &= ~ACPI_FADT_POWER_BUTTON;"])
        self.assertEqual(0x75 & ~(1 << 4), 0x65)
        self.assertIn("FIXED_PWRBTN=1", self.block)

    def test_change_is_table_only_and_preserves_hardware_ownership(self):
        self.assertNotRegex(
            self.block, r"\b(?:out[bwl]|write\d+p?|pci_\w*write\w*)\s*\("
        )
        for contract in (
            "pm1_en != 0", "gpe0_en_lo != 0", "gpe0_en_hi != 0",
            "smi_en != 0", "fadt->smi_cmd = 0;", "fadt->acpi_enable = 0;",
            "fadt->acpi_disable = 0;", "fadt->reset_value = 0;",
        ):
            self.assertIn(contract, self.source)

    def test_shared_consumer_fix_is_required_at_compile_time(self):
        self.assertIn("ADDR_SPACE_GENERAL_FLAG_CONSUMER == 0x01", self.block)
        header = (COREBOOT / "src/include/acpi/acpigen.h").read_text()
        self.assertRegex(
            header,
            r"#define ADDR_SPACE_GENERAL_FLAG_CONSUMER\s+\(0x1 << 0\)",
        )
        self.assertNotIn("CONFIG_X58_PRO_E", header)

    def test_native_generator_has_32_64_and_producer_regressions(self):
        test = (COREBOOT / "tests/acpi/acpigen-test.c").read_text()
        for name in (
            "test_acpigen_consumer_mmio32", "test_acpigen_consumer_mmio64",
            "test_acpigen_producer_mmio_flags",
        ):
            self.assertIn("static void " + name, test)
            self.assertIn("cmocka_unit_test_setup_teardown(" + name, test)


if __name__ == "__main__":
    unittest.main()
