#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
KCONFIG = (BOARD / "Kconfig").read_text()
BOOTBLOCK = (BOARD / "bootblock.c").read_text()
ROMSTAGE = (BOARD / "romstage.c").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
PCI_SOURCE = (BOARD / "b06vn_pci.c").read_text()
BASE_CONFIG = (ROOT / "configs/x58-pro-e-b06vq.config").read_text()
PLATRO_CONFIG = (ROOT / "configs/x58-pro-e-b06vq-platro1.config").read_text()
BUILDER = (ROOT / "scripts/build_x58_b06vq_platro1.sh").read_text()

PLATRO_SYMBOL = "CONFIG_X58_PRO_E_B06VQ_PLATRO1"
PLATRO_ID = "X58PROE-B06VQ-PLATRO1-20260906"
BASE_ID = "X58PROE-B06VQ-ICH10-EHCI-INIT-20260906"


def between(source: str, first: str, last: str) -> str:
    begin = source.index(first)
    return source[begin : source.index(last, begin)]


def function_text(source: str, name: str) -> str:
    definition = re.search(
        rf"\b{re.escape(name)}\s*\([^;{{}}]*\)\s*\{{", source, re.DOTALL
    )
    if definition is None:
        raise ValueError(f"no definition for function {name}")
    begin = source.rfind("\n", 0, definition.start()) + 1
    depth = 0
    for offset in range(definition.end() - 1, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[begin : offset + 1]
    raise ValueError(f"unterminated function {name}")


def config_contract(source: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for raw_line in source.splitlines():
        line = raw_line.strip()
        if line.startswith("CONFIG_") and "=" in line:
            result[line.split("=", 1)[0]] = line
        elif line.startswith("# CONFIG_") and line.endswith(" is not set"):
            result[line[2:].split(" ", 1)[0]] = line
    return result


class B06VQPlatformReadOnlySourceContractTests(unittest.TestCase):
    def test_variant_is_default_off_and_isolated(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VQ_PLATRO1",
            "config X58_PRO_E_B06VQ_ICHBASE1",
        )
        for fragment in (
            "depends on X58_PRO_E_B06VQ_ICH10_EHCI_INIT",
            "depends on !X58_PRO_E_B06VQ_USB_TRACE1",
            "depends on !X58_PRO_E_B06VR_ICH10_AHCI_MAP",
            "default n",
            "exactly one non-fatal, read-only platform census",
            "creates no resource",
            "and emits no ACPI table",
        ):
            self.assertIn(fragment, option)
        self.assertNotIn("select SOUTHBRIDGE", option)

    def test_identity_precedes_inherited_identifiers(self) -> None:
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON, PCI_SOURCE):
            self.assertIn(PLATRO_ID, source)
            self.assertIn(BASE_ID, source)
            self.assertLess(source.index(PLATRO_ID), source.index(BASE_ID))
        self.assertIn(PLATRO_SYMBOL, MAINBOARD)
        self.assertLess(
            MAINBOARD.index(PLATRO_SYMBOL),
            MAINBOARD.index("CONFIG_X58_PRO_E_B06VQ_USB_TRACE1"),
        )
        for section in (
            between(KCONFIG, "config DEVICETREE", "config X58_PRO_E_B06M"),
            between(KCONFIG, "config PAYLOAD_CONFIGFILE", "config MAINBOARD_PART_NUMBER"),
            between(KCONFIG, "config MAINBOARD_PART_NUMBER", "endif"),
        ):
            self.assertLess(
                section.index("B06VQ_PLATRO1"),
                section.index("B06VQ_USB_TRACE1"),
            )

    def test_outer_config_diff_is_only_census_identity(self) -> None:
        base = config_contract(BASE_CONFIG)
        census = config_contract(PLATRO_CONFIG)
        allowed = {
            PLATRO_SYMBOL,
            "CONFIG_X58_PRO_E_B06VQ_USB_TRACE1",
            "CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP",
            "CONFIG_MAINBOARD_PART_NUMBER",
            "CONFIG_LOCALVERSION",
        }
        self.assertEqual(
            {key: value for key, value in base.items() if key not in allowed},
            {key: value for key, value in census.items() if key not in allowed},
        )
        self.assertEqual(census[PLATRO_SYMBOL], f"{PLATRO_SYMBOL}=y")
        self.assertEqual(
            census["CONFIG_X58_PRO_E_B06VQ_USB_TRACE1"],
            "# CONFIG_X58_PRO_E_B06VQ_USB_TRACE1 is not set",
        )
        self.assertEqual(
            census["CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP"],
            "# CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP is not set",
        )

    def test_census_functions_have_no_hardware_or_resource_writes(self) -> None:
        names = (
            "b06vqp_log_lpc_state",
            "b06vqp_log_pm_state",
            "b06vqp_log_rcba_hpet_state",
            "b06vqp_log_resource_state",
            "b06vqp_log_platform_census_once",
        )
        combined = "\n".join(function_text(PCI_SOURCE, name) for name in names)
        self.assertNotRegex(
            combined,
            r"\b(?:pci_(?:(?:io_)?write|update|or|and)_config(?:8|16|32)|"
            r"write(?:8|16|32|64)?p|out[blw]|wrmsr|new_resource|"
            r"(?:setbits|clrbits|clrsetbits)(?:8|16|32|64)?|"
            r"(?:mmio|ram|reserved_ram)_range|"
            r"ioapic_(?:read|write|set|lock|register))\s*\(",
        )
        self.assertNotRegex(combined, r"\bRCBA(?:8|16|32|64)\s*\(")
        self.assertNotIn("volatile", combined)
        calls = set(re.findall(r"\b([A-Za-z_]\w*)\s*\(", combined))
        allowed_calls = {
            *names,
            "ARRAY_SIZE",
            "b06vqp_ranges_overlap",
            "for",
            "if",
            "inl",
            "inw",
            "pci_io_read_config8",
            "pci_io_read_config16",
            "pci_io_read_config32",
            "printk",
            "probe_resource",
            "read8p",
            "read16p",
            "read32p",
            "sizeof",
        }
        self.assertEqual(calls - allowed_calls, set())
        for fragment in (
            "pci_io_read_config32",
            "pci_io_read_config16",
            "pci_io_read_config8",
            "read32p",
            "read8p",
            "inl(",
            "inw(",
            "probe_resource",
            "TARGET_STATE_READ_ONLY=1",
            "NON_FATAL=1",
            "IOAPIC_MMIO_SKIPPED=1",
        ):
            self.assertIn(fragment, combined)

    def test_pm_timer_is_gated_24_bit_and_bounded(self) -> None:
        pm = function_text(PCI_SOURCE, "b06vqp_log_pm_state")
        for fragment in (
            "id != B06VQP_LPC_ID",
            "pmbase_reg != (DEFAULT_PMBASE | 1)",
            "acpi_cntl != 0x80",
            "B06VQP_PM_TIMER_MASK",
            "B06VQP_PM_TIMER_SAMPLES",
            "changes != 0",
        ):
            self.assertIn(fragment, pm)
        self.assertIn("#define B06VQP_PM_TIMER_MASK\t0x00ffffffu", PCI_SOURCE)
        self.assertIn("#define B06VQP_PM_TIMER_SAMPLES\t4096u", PCI_SOURCE)

    def test_hpet_mmio_is_strictly_decode_gated(self) -> None:
        hpet = function_text(PCI_SOURCE, "b06vqp_log_rcba_hpet_state")
        rcba_gate = hpet.index("if (id != B06VQP_LPC_ID")
        first_rcba_mmio = hpet.index(
            "read8p(CONFIG_FIXED_RCBA_MMIO_BASE + OIC)"
        )
        gate = hpet.index("if (!(hptc & B06VQP_HPET_DECODE_ENABLE))")
        first_hpet_mmio = hpet.index(
            "read32p(hpet_base + B06VQP_HPET_MAIN_COUNTER)"
        )
        self.assertLess(rcba_gate, first_rcba_mmio)
        self.assertLess(first_rcba_mmio, gate)
        self.assertLess(gate, first_hpet_mmio)
        for fragment in (
            "CONFIG_FIXED_RCBA_MMIO_BASE + OIC",
            "CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_HPTC",
            "B06VQP_HPET_ADDRESS_MASK",
            "B06VQP_HPET_CAP_ID",
            "B06VQP_HPET_GEN_CFG",
            "B06VQP_HPET_MAIN_COUNTER",
            "MMIO_SKIPPED=1",
        ):
            self.assertIn(fragment, hpet)
        self.assertNotRegex(hpet, r"\bioapic_")

    def test_resource_census_is_observational(self) -> None:
        resources = function_text(PCI_SOURCE, "b06vqp_log_resource_state")
        for fragment in (
            "B06VQP_DOMAIN_RESOURCE_COUNT",
            "probe_resource(b06vn_domain",
            "B06VN_PCI_IO_BASE",
            "B06VN_PCI_MMIO_BASE",
            "B06VN_ECAM_BASE",
            '"ROM16M"',
            "fixed_io[i].reserved",
            "fixed_mmio[i].reserved",
            "PLATFORM_RESOURCES_MODELLED=0",
        ):
            self.assertIn(fragment, resources)
        for fragment in (
            "acpi_cntl == 0x80",
            "gpio_cntl == 0x10",
            "smbus_command & PCI_COMMAND_IO",
            "smbus_hostc & HST_EN",
            "RESERVED=%u",
        ):
            self.assertIn(fragment, resources)
        for definition in (
            "#define B06VQP_GPIO_IO_SIZE\t0x40u",
            "#define B06VQP_HPET_SIZE\t\t0x0400u",
            "#define B06VQP_ROM_BASE\t\t0xff000000u",
        ):
            self.assertIn(definition, PCI_SOURCE)
        self.assertNotIn("new_resource", resources)
        self.assertNotIn("die_with_post_code", resources)

    def test_exactly_one_post_enable_census_call(self) -> None:
        enabled = function_text(PCI_SOURCE, "b06vn_resources_enabled")
        self.assertEqual(enabled.count("b06vqp_log_platform_census_once();"), 1)
        self.assertLess(
            enabled.index("b06vq_log_usb_runtime_once();"),
            enabled.index("b06vqp_log_platform_census_once();"),
        )
        self.assertLess(
            enabled.index("b06vqp_log_platform_census_once();"),
            enabled.index("post_code(POST_B06VN_ENABLE_OK)"),
        )
        census = function_text(PCI_SOURCE, "b06vqp_log_platform_census_once")
        self.assertNotIn("die_with_post_code", census)
        self.assertLess(
            census.index("b06vqp_platform_census_emitted = true;"),
            census.index("b06vqp_log_lpc_state();"),
        )

    def test_builder_rejects_acpi_and_adjacent_experiments(self) -> None:
        for fragment in (
            "'CONFIG_HAVE_ACPI_TABLES=n'",
            "'CONFIG_X58_PRO_E_B06VQ_USB_TRACE1=n'",
            "'CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP=n'",
            "'CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS=n'",
            "'CONFIG_X58_PRO_E_B06VT_ICH10_SATA_CLOCK=n'",
            "'CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_AHCI_MAP=n'",
            "'CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_PORT_ENABLE=n'",
            "'CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_CLOCK_FIELD=n'",
            "require_line \"${auto_config}\"",
            "verify_seabios_source",
            "CBFS iPXE ROM differs from the pinned source artifact",
        ):
            self.assertIn(fragment, BUILDER)


if __name__ == "__main__":
    unittest.main()
