#!/usr/bin/env python3

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
DEVICETREE = (BOARD / "devicetree_b06vx.cb").read_text()
BASE_CONFIG = (ROOT / "configs/x58-pro-e-b06vw.config").read_text()
IMAGE_CONFIG = (ROOT / "configs/x58-pro-e-b06vx.config").read_text()
BUILDER = (ROOT / "scripts/build_x58_b06_sata_successor.sh").read_text()
WRAPPER = ROOT / "scripts/build_x58_b06vx.sh"

SYMBOL = "CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE"
ID = "X58PROE-B06VX-AHCI-USBTRACE-20260906"
BASE_ID = "X58PROE-B06VW-ICH10-AHCI-MMIO-20260906"
TRACE_COMMIT = "5497f43189374647b3b0f282aa497e71c891f3c5"


def between(source: str, first: str, last: str) -> str:
    begin = source.index(first)
    return source[begin : source.index(last, begin)]


def function_text(source: str, name: str) -> str:
    definition = re.search(
        rf"\b{re.escape(name)}\s*\([^;{{}}]*\)\s*\{{", source, re.DOTALL
    )
    if definition is None:
        raise ValueError(name)
    begin = source.rfind("\n", 0, definition.start()) + 1
    depth = 0
    for offset in range(definition.end() - 1, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[begin : offset + 1]
    raise ValueError(name)


def config_contract(source: str) -> dict[str, str]:
    result = {}
    for raw_line in source.splitlines():
        line = raw_line.strip()
        if line.startswith("CONFIG_") and "=" in line:
            result[line.split("=", 1)[0]] = line
        elif line.startswith("# CONFIG_") and line.endswith(" is not set"):
            result[line[2:].split(" ", 1)[0]] = line
    return result


class B06VXCombinedAhciUsbTraceSourceContractTests(unittest.TestCase):
    def test_variant_is_separate_default_off_read_only_trace_successor(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VX_AHCI_USB_TRACE",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        for fragment in (
            "depends on X58_PRO_E_B06VW_ICH10_AHCI_MMIO",
            "depends on PAYLOAD_SEABIOS",
            "depends on SEABIOS_REVISION",
            "default n",
            "already-proven B06VQ EHCI",
            "same hash-pinned, bounded deferred SeaBIOS",
            "separate diagnostic successor, not part of B06VV or B06VW",
            "adds no coreboot register write beyond B06VW",
            "does not enable the broad",
            "B06VQ-USBTRACE1 remains",
        ):
            self.assertIn(fragment, option)
        self.assertNotIn("select SOUTHBRIDGE", option)

    def test_identity_and_topology_precede_b06vw(self) -> None:
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON, PCI):
            self.assertLess(source.index(ID), source.index(BASE_ID))
        self.assertLess(MAINBOARD.index(SYMBOL), MAINBOARD.index("B06VW_ICH10_AHCI_MMIO"))
        self.assertIn('default "devicetree_b06vx.cb"', KCONFIG)
        self.assertEqual(DEVICETREE.count("device pci"), 13)
        self.assertIn("keeps B06VW unchanged", DEVICETREE)
        self.assertNotRegex(DEVICETREE, r"device pci 1f\.5\b")

    def test_outer_config_only_changes_trace_identity_and_payload(self) -> None:
        base = config_contract(BASE_CONFIG)
        image = config_contract(IMAGE_CONFIG)
        allowed = {
            SYMBOL,
            "CONFIG_MAINBOARD_PART_NUMBER",
            "CONFIG_LOCALVERSION",
            "CONFIG_SEABIOS_STABLE",
            "CONFIG_SEABIOS_REVISION",
            "CONFIG_SEABIOS_REVISION_ID",
            "CONFIG_PAYLOAD_CONFIGFILE",
        }
        self.assertEqual(
            {k: v for k, v in base.items() if k not in allowed},
            {k: v for k, v in image.items() if k not in allowed},
        )
        self.assertEqual(image[SYMBOL], f"{SYMBOL}=y")
        self.assertEqual(image["CONFIG_SEABIOS_REVISION_ID"],
                         f'CONFIG_SEABIOS_REVISION_ID="{TRACE_COMMIT}"')
        self.assertEqual(image["CONFIG_X58_PRO_E_B06VQ_USB_TRACE1"],
                         "# CONFIG_X58_PRO_E_B06VQ_USB_TRACE1 is not set")
        self.assertEqual(image["CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT"],
                         "CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT=y")

    def test_trace_composition_reuses_existing_payload_and_delay(self) -> None:
        payload = between(KCONFIG, "config PAYLOAD_CONFIGFILE", "config MAINBOARD_PART_NUMBER")
        self.assertLess(payload.index("B06VX_AHCI_USB_TRACE"),
                        payload.index("B06VW_ICH10_AHCI_MMIO"))
        self.assertIn("config_seabios_b06vq_usbtrace1", payload)
        self.assertIn("CONFIG_X58_PRO_E_B06VQ_USB_TRACE1) $(CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE)", MAKEFILE)
        self.assertEqual(MAKEFILE.count("add-int -i 1000 -n etc/usb-time-sigatt"), 1)

    def test_extra_coreboot_telemetry_is_shared_and_read_only(self) -> None:
        for occurrence in re.finditer(r"#if CONFIG_X58_PRO_E_B06VQ_USB_TRACE1", PCI):
            guard = PCI[occurrence.start() : PCI.find("\n", PCI.find("\n", occurrence.start()) + 1)]
            self.assertIn("CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE", guard)
        combined = "".join(
            function_text(PCI, name)
            for name in (
                "b06vq_log_ehci_runtime",
                "b06vq_log_uhci_runtime",
                "b06vq_log_usb_runtime_once",
            )
        )
        self.assertIn("[USB-GATE]", combined)
        self.assertNotRegex(
            combined, r"\b(?:pci_io_write|pci_write|write[0-9]*p|out[blw]|wrmsr)\s*\("
        )

    def test_b06vw_sata_path_is_not_modified_by_trace_selection(self) -> None:
        program = function_text(PCI, "b06vw_program_mmio_once")
        final = function_text(PCI, "b06vw_verify_final_once")
        self.assertNotIn(SYMBOL, program + final)
        self.assertEqual(program.count("write32p("), 1)
        self.assertEqual(program.count("write8p("), 1)
        self.assertEqual(program.count("pci_io_write_config16("), 1)
        self.assertNotRegex(final, r"\b(?:write8p|write16p|write32p|pci_io_write_config)\s*\(")

    def test_builder_pins_trace_and_keeps_standalone_variant(self) -> None:
        self.assertTrue(WRAPPER.is_file())
        self.assertIn("b06vx", WRAPPER.read_text())
        for fragment in (
            TRACE_COMMIT,
            "seabios-b52ca86-usb-deferred-trace.patch",
            "git clone --quiet --no-hardlinks",
            "non-deterministic SeaBIOS trace commit",
            "CONFIG_DEBUG_USB_TRACE=y",
            "int.from_bytes(d,\"little\")==1000",
            "clean builds are not byte-identical",
            "CONFIG_X58_PRO_E_B06VQ_USB_TRACE1 is not set",
            "CONFIG_SOUTHBRIDGE_INTEL_I82801JX=n",
        ):
            self.assertIn(fragment, BUILDER)


if __name__ == "__main__":
    unittest.main()
