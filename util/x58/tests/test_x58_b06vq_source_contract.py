#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
ICH10 = COREBOOT / "src/southbridge/intel/i82801jx"
KCONFIG = (BOARD / "Kconfig").read_text()
BOOTBLOCK = (BOARD / "bootblock.c").read_text()
ROMSTAGE = (BOARD / "romstage.c").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
PCI_SOURCE = (BOARD / "b06vn_pci.c").read_text()
ICH10_KCONFIG = (ICH10 / "Kconfig").read_text()
ICH10_MAKE = (ICH10 / "Makefile.mk").read_text()
ICH10_HEADER = (ICH10 / "i82801jx.h").read_text()
ICH10_CHIP = (ICH10 / "i82801jx.c").read_text()
ICH10_EHCI = (ICH10 / "ehci_init.c").read_text()
SEABIOS_CONFIG = (BOARD / "config_seabios_b06vm").read_text()
VP_CONFIG = (ROOT / "configs/x58-pro-e-b06vp.config").read_text()
VQ_CONFIG = (ROOT / "configs/x58-pro-e-b06vq.config").read_text()
VQ_BUILD_PATH = ROOT / "scripts/build_x58_b06vq.sh"

VQ_SYMBOL = "CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT"
VP_SYMBOL = "CONFIG_X58_PRO_E_B06VP_LAPIC_EXTINT"
VQ_ID = "X58PROE-B06VQ-ICH10-EHCI-INIT-20260906"
VP_ID = "X58PROE-B06VP-LAPIC-EXTINT-PROBE-20260906"


def between(source: str, first: str, last: str) -> str:
    begin = source.index(first)
    end = source.index(last, begin)
    return source[begin:end]


def function_text(source: str, name: str) -> str:
    definition = re.search(
        rf"\b{re.escape(name)}\s*\([^;{{}}]*\)\s*\{{", source, re.DOTALL
    )
    if definition is None:
        raise ValueError(f"no definition for function {name}")
    begin = source.rfind("\n", 0, definition.start()) + 1
    body = definition.end() - 1
    depth = 0
    for offset in range(body, len(source)):
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


class B06VQSourceContractTests(unittest.TestCase):
    def test_successor_is_default_off_and_selects_only_ehci_helper(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VQ_ICH10_EHCI_INIT",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        for fragment in (
            "depends on X58_PRO_E_B06VP_LAPIC_EXTINT",
            "select SOUTHBRIDGE_INTEL_I82801JX_EHCI_INIT",
            "default n",
            "masked read-modify-write",
            "SeaBIOS still owns controller",
            "not change memory, QPI, IOH, GPU, SATA, LPC, ACPI",
        ):
            self.assertIn(fragment, option)
        self.assertNotIn("SOUTHBRIDGE_INTEL_I82801JX\n", option)

    def test_identity_precedes_b06vp_everywhere(self) -> None:
        tree = between(KCONFIG, "config DEVICETREE", "config X58_PRO_E_B06M")
        payload = between(KCONFIG, "config PAYLOAD_CONFIGFILE", "config MAINBOARD_PART_NUMBER")
        part = between(KCONFIG, "config MAINBOARD_PART_NUMBER", "endif")
        for source in (tree, payload, part):
            self.assertLess(source.index("B06VQ"), source.index("B06VP"))
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON, MAINBOARD, PCI_SOURCE):
            vq_marker = VQ_SYMBOL if source == MAINBOARD else VQ_ID
            vp_marker = VP_SYMBOL if source == MAINBOARD else VP_ID
            self.assertIn(vq_marker, source)
            self.assertLess(source.index(vq_marker), source.index(vp_marker))

    def test_config_is_b06vp_plus_identity_and_one_option(self) -> None:
        vp = config_contract(VP_CONFIG)
        vq = config_contract(VQ_CONFIG)
        allowed = {VQ_SYMBOL, "CONFIG_MAINBOARD_PART_NUMBER", "CONFIG_LOCALVERSION"}
        self.assertEqual(
            {key: value for key, value in vp.items() if key not in allowed},
            {key: value for key, value in vq.items() if key not in allowed},
        )
        self.assertEqual(vq[VQ_SYMBOL], f"{VQ_SYMBOL}=y")
        self.assertEqual(
            vq["CONFIG_MAINBOARD_PART_NUMBER"],
            'CONFIG_MAINBOARD_PART_NUMBER="X58 Pro-E B06VQ ICH10 EHCI init"',
        )
        self.assertEqual(
            vq["CONFIG_LOCALVERSION"], 'CONFIG_LOCALVERSION="x58-pro-e-b06vq"'
        )

    def test_existing_ich10_helper_is_reusable_and_behavior_preserved(self) -> None:
        self.assertIn("config SOUTHBRIDGE_INTEL_I82801JX_EHCI_INIT", ICH10_KCONFIG)
        full = between(
            ICH10_KCONFIG,
            "config SOUTHBRIDGE_INTEL_I82801JX\n",
            "if SOUTHBRIDGE_INTEL_I82801JX",
        )
        self.assertIn("select SOUTHBRIDGE_INTEL_I82801JX_EHCI_INIT", full)
        self.assertIn(
            "ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_EHCI_INIT) += ehci_init.c",
            ICH10_MAKE,
        )
        self.assertEqual(ICH10_CHIP.count("i82801jx_ehci_init();"), 1)
        self.assertEqual(PCI_SOURCE.count("i82801jx_ehci_init();"), 1)
        self.assertIn("void i82801jx_ehci_init(void);", ICH10_HEADER)

        # ICH10 Datasheet 319973, section 17.1.36: EHCIIR2 is at 0xfc;
        # required fields are bit 29, bit 17 and bits 3:2 = 10b.
        self.assertRegex(
            ICH10_HEADER,
            r"(?m)^#define[ \t]+I82801JX_EHCI_FCREG[ \t]+0xfc$",
        )
        for fragment in (
            "I82801JX_EHCI_FCREG_FIELD_3_2_MASK\t(3u << 2)",
            "I82801JX_EHCI_FCREG_FIELD_3_2_VALUE\t(2u << 2)",
            "I82801JX_EHCI_FCREG_BIT_17\t\t(1u << 17)",
            "I82801JX_EHCI_FCREG_BIT_29\t\t(1u << 29)",
        ):
            self.assertIn(fragment, ICH10_HEADER)
        self.assertEqual((3 << 2) | (1 << 17) | (1 << 29), 0x2002000C)
        self.assertEqual((2 << 2) | (1 << 17) | (1 << 29), 0x20020008)

        helper = function_text(ICH10_EHCI, "i82801jx_ehci_program_required_fields")
        self.assertEqual(helper.count("pci_read_config32("), 1)
        self.assertEqual(helper.count("pci_write_config32("), 1)
        self.assertIn("~I82801JX_EHCI_FCREG_REQUIRED_MASK", helper)
        self.assertIn("I82801JX_EHCI_FCREG_REQUIRED_VALUE", helper)
        self.assertNotRegex(helper, r"\b(?:write|out|wrmsr)")

        public = function_text(ICH10_EHCI, "i82801jx_ehci_init")
        self.assertEqual(public.count("i82801jx_ehci_program_required_fields("), 2)
        self.assertIn("pcidev_on_root(0x1d, 7)", public)
        self.assertIn("pcidev_on_root(0x1a, 7)", public)

    def test_b06vq_write_is_one_shot_exact_and_before_scan(self) -> None:
        program = function_text(PCI_SOURCE, "b06vq_program_ehci_once")
        for fragment in (
            "b06vq_ehci_init_attempted",
            "POST_B06VQ_EHCI_BEGIN",
            "i82801jx_ehci_init();",
            "after[i] != b06vq_ehci_target(before[i])",
            "POST_B06VQ_EHCI_FAIL",
            "POST_B06VQ_EHCI_OK",
        ):
            self.assertIn(fragment, program)
        self.assertEqual(program.count("i82801jx_ehci_init();"), 1)
        self.assertNotRegex(program, r"\b(?:pci_write|write[0-9]*p|out[blw]|wrmsr)\s*\(")

        scan = function_text(PCI_SOURCE, "b06vn_domain_scan_bus")
        ordered = (
            "b06vn_raw_root_preflight();",
            "b06vo_program_ioh_bus_number_once();",
            "b06vq_program_ehci_once();",
            "b06vn_start_iou0_once();",
            "pci_host_bridge_scan_bus(dev);",
        )
        positions = [scan.index(fragment) for fragment in ordered]
        self.assertEqual(positions, sorted(positions))

    def test_pre_seabios_telemetry_is_read_only_and_bounded(self) -> None:
        ehci = function_text(PCI_SOURCE, "b06vq_log_ehci_runtime")
        uhci = function_text(PCI_SOURCE, "b06vq_log_uhci_runtime")
        global_usb = function_text(PCI_SOURCE, "b06vq_log_usb_runtime_once")
        combined = ehci + uhci
        for fragment in (
            "LEGACY=%08x/%08x/%08x",
            "CAPLEN=%02x",
            "HCIVER=%04x",
            "HCSPARAMS=%08x",
            "USBCMD=%08x",
            "USBSTS=%08x",
            "CONFIGFLAG=%08x",
            "PORTSC%u=%08x",
            "PORTSC1=%04x",
            "PORTSC2=%04x",
            "LEGKEY=%04x",
        ):
            self.assertIn(fragment, combined)
        self.assertIn("ports > B06VQ_EHCI_PORT_MAX", ehci)
        self.assertLess(
            ehci.index("if (!(command & PCI_COMMAND_MEMORY) || (pmcsr & 3))"),
            ehci.index("caplength = read8p(base)"),
        )
        self.assertLess(
            uhci.index("if (!(command & PCI_COMMAND_IO))"),
            uhci.index("inw(base + B06VQ_UHCI_USBCMD)"),
        )
        for fragment in (
            "PPO=%04x",
            "MAP=%08x",
            "UPRWC=%04x",
            "GPIO_USE=%08x/%08x",
            "CONFIG_FIXED_RCBA_MMIO_BASE + B06VQ_RCBA_PPO",
            "CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_MAP",
            "DEFAULT_PMBASE + B06VQ_PMBASE_UPRWC",
            "DEFAULT_GPIOBASE + GP_IO_USE_SEL",
            "DEFAULT_GPIOBASE + GP_IO_USE_SEL2",
        ):
            self.assertIn(fragment, global_usb)
        self.assertIn(
            "pci_io_read_config16(controller->dev, B06VQ_UHCI_LEGKEY)", uhci
        )
        self.assertIn(
            "read16p(CONFIG_FIXED_RCBA_MMIO_BASE + B06VQ_RCBA_PPO)", global_usb
        )
        self.assertNotRegex(
            combined + global_usb,
            r"\b(?:pci_write|write[0-9]*p|out[blw]|wrmsr)\s*\(",
        )
        self.assertEqual(PCI_SOURCE.count('"EHCI2 00:1a.7"'), 1)
        self.assertEqual(PCI_SOURCE.count('"EHCI1 00:1d.7"'), 1)
        self.assertEqual(len(re.findall(r'"UHCI[1-6] 00:1[ad]\.[0-2]"', PCI_SOURCE)), 6)

        enabled = function_text(PCI_SOURCE, "b06vn_resources_enabled")
        self.assertIn("b06vq_log_usb_runtime_once();", enabled)
        self.assertLess(
            enabled.index("b06vq_log_usb_runtime_once();"),
            enabled.index("post_code(POST_B06VN_ENABLE_OK);"),
        )

    def test_payload_usb_stack_remains_enabled(self) -> None:
        for fragment in (
            "CONFIG_USB=y",
            "CONFIG_USB_UHCI=y",
            "CONFIG_USB_EHCI=y",
            "CONFIG_USB_MSC=y",
            "CONFIG_USB_HUB=y",
            "CONFIG_USB_KEYBOARD=y",
        ):
            self.assertIn(fragment, SEABIOS_CONFIG)

    def test_builder_contract(self) -> None:
        self.assertTrue(VQ_BUILD_PATH.exists())
        builder = VQ_BUILD_PATH.read_text()
        for fragment in (
            "configs/x58-pro-e-b06vq.config",
            "CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT=y",
            "CONFIG_SOUTHBRIDGE_INTEL_I82801JX_EHCI_INIT=y",
            "CONFIG_SOUTHBRIDGE_INTEL_I82801JX=n",
            'cd -- "${repo_root}"',
            "unset PYTHONOPTIMIZE",
            "CONFIG_USB_MSC=y",
            "CONFIG_USB_HUB=y",
            "CONFIG_USB_KEYBOARD=y",
            "tests.test_x58_b06vq_source_contract",
            "msi-x58-pro-e-b06vq-coreboot-base-4MiB.rom",
            "msi-x58-pro-e-b06vq-deterministic-w25q128-16MiB.rom",
            "cmp -s -- \"${first_rom}\" \"${second_rom}\"",
            "int.from_bytes(data, \"little\") == 1",
        ):
            self.assertIn(fragment, builder)


if __name__ == "__main__":
    unittest.main()
