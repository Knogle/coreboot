#!/usr/bin/env python3

import os
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
KCONFIG = (BOARD / "Kconfig").read_text()
MAKEFILE = (BOARD / "Makefile.mk").read_text()
BOOTBLOCK = (BOARD / "bootblock.c").read_text()
ROMSTAGE = (BOARD / "romstage.c").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
PCI = (BOARD / "b06vn_pci.c").read_text()
BASE_TREE = (BOARD / "devicetree_b06wb.cb").read_text()
IMAGE_TREE = (BOARD / "devicetree_b06wc.cb").read_text()
BASE_CONFIG = (ROOT / "configs/x58-pro-e-b06wb.config").read_text()
IMAGE_CONFIG = (ROOT / "configs/x58-pro-e-b06wc.config").read_text()
BUILDER = (ROOT / "scripts/build_x58_b06_sata_successor.sh").read_text()
WRAPPER = ROOT / "scripts/build_x58_b06wc.sh"

SYMBOL = "CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM"
IRQPROBE = "CONFIG_X58_PRO_E_ROMMON_IRQPROBE_PIT"
ID = "X58PROE-B06WC-INTEGRATED-PLATFORM-20260906"
BASE_ID = "X58PROE-B06WB-ICH10-TCO-HALT-20260906"


def between(source: str, first: str, last: str) -> str:
    begin = source.index(first)
    return source[begin : source.index(last, begin)]


def function_text(source: str, name: str) -> str:
    definition = re.search(
        rf"\b{re.escape(name)}\s*\([^;{{}}]*\)\s*\{{", source, re.DOTALL
    )
    if definition is None:
        raise ValueError(f"no definition for {name}")
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
    result = {}
    for raw_line in source.splitlines():
        line = raw_line.strip()
        if line.startswith("CONFIG_") and "=" in line:
            result[line.split("=", 1)[0]] = line
        elif line.startswith("# CONFIG_") and line.endswith(" is not set"):
            result[line[2:].split(" ", 1)[0]] = line
    return result


def uncommented_devicetree(source: str) -> str:
    lines = []
    for raw_line in source.splitlines():
        code = raw_line.split("#", 1)[0].rstrip()
        if code:
            lines.append(code)
    return "\n".join(lines)


class B06WCIntegratedSourceContractTests(unittest.TestCase):
    def test_kconfig_symbol_and_defaults_have_highest_precedence(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06WC_INTEGRATED_PLATFORM",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        compact = " ".join(option.split())
        for fragment in (
            "depends on X58_PRO_E_B06WB_TCO_HALT",
            "select X58_PRO_E_ROMMON_IRQPROBE_PIT",
            "default n",
            "integrated experimental successor",
            "without separate B06VY/B06VZ/B06WA/B06WB qualification",
            "socketed flash recovery path",
        ):
            self.assertIn(fragment, compact)

        devicetree = between(KCONFIG, "config DEVICETREE", "config X58_PRO_E_B06M")
        self.assertLess(
            devicetree.index(
                'default "devicetree_b06wc.cb" if '
                "X58_PRO_E_B06WC_INTEGRATED_PLATFORM"
            ),
            devicetree.index(
                'default "devicetree_b06wb.cb" if X58_PRO_E_B06WB_TCO_HALT'
            ),
        )

        payload = between(KCONFIG, "config PAYLOAD_CONFIGFILE", "config MAINBOARD_PART_NUMBER")
        self.assertLess(
            payload.index("X58_PRO_E_B06WC_INTEGRATED_PLATFORM"),
            payload.index("X58_PRO_E_B06WB_TCO_HALT"),
        )
        self.assertIn("config_seabios_b06vq_usbtrace1", payload)
        part_number = KCONFIG[KCONFIG.index("config MAINBOARD_PART_NUMBER") :]
        self.assertIn(
            'default "X58 Pro-E B06WC integrated experimental platform" '
            "if X58_PRO_E_B06WC_INTEGRATED_PLATFORM",
            part_number,
        )

    def test_config_inherits_complete_b06wb_chain(self) -> None:
        base = config_contract(BASE_CONFIG)
        image = config_contract(IMAGE_CONFIG)
        identity_keys = {"CONFIG_MAINBOARD_PART_NUMBER", "CONFIG_LOCALVERSION"}
        for key, value in base.items():
            if key not in identity_keys:
                self.assertEqual(image.get(key), value, key)

        self.assertEqual(image[SYMBOL], f"{SYMBOL}=y")
        self.assertEqual(image[IRQPROBE], f"{IRQPROBE}=y")
        self.assertEqual(
            image["CONFIG_MAINBOARD_PART_NUMBER"],
            'CONFIG_MAINBOARD_PART_NUMBER="X58 Pro-E B06WC integrated experimental platform"',
        )
        self.assertEqual(
            image["CONFIG_LOCALVERSION"],
            'CONFIG_LOCALVERSION="x58-pro-e-b06wc"',
        )
        for inherited in (
            "CONFIG_X58_PRO_E_B06VP_LAPIC_EXTINT",
            "CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT",
            "CONFIG_X58_PRO_E_B06VQ_ICHBASE1",
            "CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE",
            "CONFIG_X58_PRO_E_B06VV_ICH10_PCS_SCLK",
            "CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO",
            "CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE",
            "CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK",
            "CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES",
            "CONFIG_X58_PRO_E_B06WA_HPET_DECODE",
            "CONFIG_X58_PRO_E_B06WB_TCO_HALT",
            "CONFIG_PAYLOAD_SEABIOS",
            "CONFIG_SEABIOS_HARDWARE_IRQ",
            "CONFIG_BUILD_IPXE",
        ):
            self.assertEqual(image[inherited], f"{inherited}=y")

    def test_devicetree_preserves_b06wb_topology(self) -> None:
        self.assertEqual(IMAGE_TREE.count("device pci"), 13)
        self.assertNotRegex(IMAGE_TREE, r"device pci 1f\.5\b")
        self.assertEqual(
            uncommented_devicetree(IMAGE_TREE),
            uncommented_devicetree(BASE_TREE),
        )

    def test_identity_is_wired_into_every_executable_stage(self) -> None:
        expected_counts = {
            "bootblock": (BOOTBLOCK, 1),
            "romstage": (ROMSTAGE, 2),
            "ramstage monitor": (RAMMON, 1),
            "ramstage PCI": (PCI, 1),
        }
        for stage, (source, count) in expected_counts.items():
            with self.subTest(stage=stage):
                self.assertEqual(source.count(ID), count)
                self.assertLess(source.index(ID), source.index(BASE_ID))
        self.assertLess(MAINBOARD.index(SYMBOL), MAINBOARD.index("B06WB_TCO_HALT"))
        self.assertIn("MSI X58 Pro-E experimental B06WC integrated platform", MAINBOARD)

    def test_makefile_composes_usb_and_irqprobe_objects(self) -> None:
        for line in (
            "ramstage-$(CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM) += "
            "b06wc_usb_admit.c",
            "romstage-$(CONFIG_X58_PRO_E_ROMMON_IRQPROBE_PIT) += "
            "rommon_irqprobe.c",
            "romstage-$(CONFIG_X58_PRO_E_ROMMON_IRQPROBE_PIT) += "
            "rommon_irqprobe_entry.S",
        ):
            self.assertIn(line, MAKEFILE)
        self.assertIn(
            "$(CONFIG_X58_PRO_E_B06VQ_USB_TRACE1) "
            "$(CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE)",
            MAKEFILE,
        )
        self.assertEqual(MAKEFILE.count("add-int -i 1000 -n etc/usb-time-sigatt"), 1)

    def test_pic_and_usb_stage_order_is_integrated(self) -> None:
        scan = function_text(PCI, "b06vn_domain_scan_bus")
        ordered_scan = (
            "b06vn_require_platform_state(\"pre-scan\");",
            "b06vn_require_static_topology();",
            "b06wb_halt_tco_once();",
            "b06wc_initialize_pic_once();",
            "b06vqi_program_baseline_once();",
            "b06vu_program_ahci_route_once();",
            "b06vy_decode_and_mask_ioapic_once();",
            "b06wa_decode_and_gate_hpet_once();",
            "b06vn_raw_root_preflight();",
            "b06vo_program_ioh_bus_number_once();",
            "b06vq_program_ehci_once();",
            "pci_host_bridge_scan_bus(dev);",
        )
        positions = [scan.index(item) for item in ordered_scan]
        self.assertEqual(positions, sorted(positions))

        enabled = function_text(PCI, "b06vn_resources_enabled")
        ordered_enable = (
            'b06vn_require_platform_state("post-enable");',
            "b06vw_verify_final_once();",
            "b06vq_log_usb_runtime_once();",
            "if (!b06wc_pic_ready)",
            "b06wc_usb_admit();",
            "post_code(POST_B06VN_ENABLE_OK);",
        )
        positions = [enabled.index(item) for item in ordered_enable]
        self.assertEqual(positions, sorted(positions))
        self.assertEqual(PCI.count("b06wc_initialize_pic_once();"), 1)
        self.assertEqual(PCI.count("b06wc_usb_admit();"), 1)

    def test_irqprobe_is_default_off_but_reachable_in_b06wc_rommon(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_ROMMON_IRQPROBE_PIT",
            "config X58_PRO_E_B06VQ_ICH10_EHCI_INIT",
        )
        for fragment in (
            "depends on X58_PRO_E_B06VP_LAPIC_EXTINT",
            "depends on X58_PRO_E_B06V1_REG_SCRIPT",
            "default n",
            "one-operation WRITE unlock",
            "cold reset is required",
        ):
            self.assertIn(fragment, option)
        self.assertIn("#include \"rommon_irqprobe.h\"", ROMSTAGE)
        for fragment in (
            "irqprobe pit  (destructive diagnostic; cold reset required)",
            'b06j_streq(argv[0], "irqprobe")',
            'b06j_streq(argv[1], "pit")',
            "x58_rs_get_info(&script_info)",
            "if (script_info.transaction_valid)",
            "if (!b06j_require_write(state))",
            "x58_rommon_irqprobe_pit(&r)",
            "[IRQPROBE] MUTATES PIT/PIC/LAPIC; COLD RESET REQUIRED",
        ):
            self.assertIn(fragment, ROMSTAGE)

    def test_builder_supports_reproducible_w25q128_release(self) -> None:
        self.assertTrue(WRAPPER.is_file())
        self.assertTrue(os.access(WRAPPER, os.X_OK))
        self.assertRegex(
            WRAPPER.read_text(),
            r'exec .*build_x58_b06_sata_successor\.sh" b06wc\s*$',
        )
        for fragment in (
            "b06vv|b06vw|b06vx|b06vy|b06vz|b06wa|b06wb|b06wc",
            f'build_id="{ID}"',
            "B06WC is an integrated EXPERIMENTAL image",
            "predecessor qualification is intentionally not required",
            "CONFIG_X58_PRO_E_B06WB_TCO_HALT=y",
            "CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM=y",
            "CONFIG_X58_PRO_E_ROMMON_IRQPROBE_PIT=y",
            "CONFIG_USB_KEYBOARD=y",
            "CONFIG_HARDWARE_IRQ=y",
            "python3 -m unittest discover -s tests -v",
            'build_once "${first_rom}"',
            'build_once "${second_rom}"',
            'cmp -s -- "${first_rom}" "${second_rom}"',
            'cmp -s -- "${first_ipxe}"',
            "msi_vendor_blobs.py\" compose",
            "msi_vendor_blobs.py\" verify-composite",
            "x58_b06v9_wrapper_patch.py\" patch",
            "x58_b06v9_wrapper_patch.py\" verify",
            "place_firmware_at_flash_top.py",
            '"${full_chip}" --flash-size 0x1000000',
            "deterministic-w25q128-16MiB.rom",
        ):
            self.assertIn(fragment, BUILDER)

    def test_builder_constraints_match_b06wc_acpi_selection(self) -> None:
        # ACPI table contents are audited separately.  This only prevents the
        # release builder from rejecting the Kconfig value it has just resolved.
        board_options = between(KCONFIG, "config BOARD_SPECIFIC_OPTIONS", "config MAINBOARD_DIR")
        common_checks = between(BUILDER, "auto_config=", 'case "${variant}" in')
        wc_begin = BUILDER.rindex("\tb06wc)")
        wc_checks = BUILDER[wc_begin : BUILDER.index("\t\t;;\n", wc_begin)]
        if "select HAVE_ACPI_TABLES if X58_PRO_E_B06WC_INTEGRATED_PLATFORM" in board_options:
            self.assertNotIn("CONFIG_HAVE_ACPI_TABLES=n", common_checks)
            self.assertIn("CONFIG_HAVE_ACPI_TABLES=y", wc_checks)


if __name__ == "__main__":
    unittest.main()
