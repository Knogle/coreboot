#!/usr/bin/env python3

import hashlib
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
PCI_SOURCE = (BOARD / "b06vn_pci.c").read_text()
BASE_SEABIOS_CONFIG = (BOARD / "config_seabios_b06vm").read_text()
TRACE_SEABIOS_CONFIG = (BOARD / "config_seabios_b06vq_usbtrace1").read_text()
BASE_CONFIG = (ROOT / "configs/x58-pro-e-b06vq.config").read_text()
TRACE_CONFIG = (ROOT / "configs/x58-pro-e-b06vq-usbtrace1.config").read_text()
PATCH_PATH = ROOT / "patches/seabios-b52ca86-usb-deferred-trace.patch"
BUILDER_PATH = ROOT / "scripts/build_x58_b06vq_usbtrace1.sh"

TRACE_SYMBOL = "CONFIG_X58_PRO_E_B06VQ_USB_TRACE1"
TRACE_ID = "X58PROE-B06VQ-USBTRACE1-20260906"
BASE_ID = "X58PROE-B06VQ-ICH10-EHCI-INIT-20260906"
TRACE_COMMIT = "5497f43189374647b3b0f282aa497e71c891f3c5"
PATCH_SHA256 = "f222796ea8029444106ca009045b1528b9a325fa5b84706ca252739d353c232f"


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


class B06VQUsbTrace1SourceContractTests(unittest.TestCase):
    def test_variant_is_default_off_isolated_and_read_only(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VQ_USB_TRACE1",
            "config X58_PRO_E_B06VQ_PLATRO1",
        )
        for fragment in (
            "depends on X58_PRO_E_B06VQ_ICH10_EHCI_INIT",
            "depends on !X58_PRO_E_B06VR_ICH10_AHCI_MAP",
            "depends on PAYLOAD_SEABIOS",
            "depends on SEABIOS_REVISION",
            "default n",
            "bounded 96-entry USB event",
            "adds no coreboot hardware write beyond inherited B06VQ",
        ):
            self.assertIn(fragment, option)
        self.assertNotIn("select SOUTHBRIDGE", option)

    def test_identity_precedes_all_inherited_identifiers(self) -> None:
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON, MAINBOARD, PCI_SOURCE):
            marker = TRACE_SYMBOL if source == MAINBOARD else TRACE_ID
            inherited = (
                "CONFIG_X58_PRO_E_B06VT_ICH10_SATA_CLOCK"
                if source == MAINBOARD
                else BASE_ID
            )
            self.assertIn(marker, source)
            self.assertIn(inherited, source)
            self.assertLess(source.index(marker), source.index(inherited))
        for section in (
            between(KCONFIG, "config DEVICETREE", "config X58_PRO_E_B06M"),
            between(KCONFIG, "config PAYLOAD_CONFIGFILE", "config MAINBOARD_PART_NUMBER"),
            between(KCONFIG, "config MAINBOARD_PART_NUMBER", "endif"),
        ):
            self.assertLess(section.index("B06VQ_USB_TRACE1"), section.index("B06VT"))

    def test_outer_config_diff_is_only_trace_identity_and_payload_revision(self) -> None:
        base = config_contract(BASE_CONFIG)
        trace = config_contract(TRACE_CONFIG)
        allowed = {
            TRACE_SYMBOL,
            "CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP",
            "CONFIG_MAINBOARD_PART_NUMBER",
            "CONFIG_LOCALVERSION",
            "CONFIG_SEABIOS_STABLE",
            "CONFIG_SEABIOS_REVISION",
            "CONFIG_SEABIOS_REVISION_ID",
            "CONFIG_PAYLOAD_CONFIGFILE",
        }
        self.assertEqual(
            {key: value for key, value in base.items() if key not in allowed},
            {key: value for key, value in trace.items() if key not in allowed},
        )
        self.assertEqual(trace[TRACE_SYMBOL], f"{TRACE_SYMBOL}=y")
        self.assertEqual(
            trace["CONFIG_SEABIOS_REVISION_ID"],
            f'CONFIG_SEABIOS_REVISION_ID="{TRACE_COMMIT}"',
        )
        self.assertEqual(
            trace["CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP"],
            "# CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP is not set",
        )

    def test_seabios_profile_only_enables_deferred_trace(self) -> None:
        base = config_contract(BASE_SEABIOS_CONFIG)
        trace = config_contract(TRACE_SEABIOS_CONFIG)
        self.assertEqual(
            base,
            {key: value for key, value in trace.items() if key != "CONFIG_DEBUG_USB_TRACE"},
        )
        self.assertEqual(trace["CONFIG_DEBUG_USB_TRACE"], "CONFIG_DEBUG_USB_TRACE=y")
        self.assertEqual(trace["CONFIG_THREADS"], "# CONFIG_THREADS is not set")
        self.assertEqual(trace["CONFIG_DEBUG_LEVEL"], "CONFIG_DEBUG_LEVEL=9")

    def test_patch_is_pinned_bounded_and_has_no_hot_path_printing(self) -> None:
        data = PATCH_PATH.read_bytes()
        self.assertEqual(hashlib.sha256(data).hexdigest(), PATCH_SHA256)
        patch = data.decode()
        for fragment in (
            "depends on USB && !THREADS && DEBUG_LEVEL != 0",
            "#define USB_TRACE_MAX_EVENTS 96",
            "_Static_assert(sizeof(struct usb_trace_entry) == 16",
            "static struct usb_trace_entry UsbTrace[USB_TRACE_MAX_EVENTS]",
            "UsbTraceDropped++",
            "usb_trace_dump();",
            "USB_TRACE_DESCRIPTOR8",
            "USB_TRACE_HID_PIPE",
        ):
            self.assertIn(fragment, patch)
        added = "\n".join(
            line[1:]
            for line in patch.splitlines()
            if line.startswith("+") and not line.startswith("+++")
        )
        self.assertEqual(added.count("dprintf("), 3)
        record = function_text(added, "usb_trace_record")
        self.assertNotIn("dprintf(", record)

    def test_extended_platform_telemetry_is_read_only(self) -> None:
        global_log = function_text(PCI_SOURCE, "b06vq_log_usb_runtime_once")
        ehci_log = function_text(PCI_SOURCE, "b06vq_log_ehci_runtime")
        uhci_log = function_text(PCI_SOURCE, "b06vq_log_uhci_runtime")
        combined = global_log + ehci_log + uhci_log
        for fragment in (
            "B06VQ_USB_FD_DISABLE_MASK",
            "B06VQ_USB_CG_DISABLE",
            "B06VQ_USB_PPO_MASK",
            "B06VQ_USB_GPIO1_OC_MASK",
            "HCCPARAMS=%08x",
            "CFG61=%02x",
            "SMI6C=%04x",
            "EXT70=%04x",
            "CFG84=%08x",
            "CFGC8=%04x",
            "CFGCA=%04x",
        ):
            self.assertIn(fragment, combined)
        self.assertNotRegex(
            combined, r"\b(?:pci_write|write[0-9]*p|out[blw]|wrmsr)\s*\("
        )

    def test_attach_delay_and_builder_contract(self) -> None:
        self.assertIn("CONFIG_X58_PRO_E_B06VQ_USB_TRACE1", MAKEFILE)
        self.assertIn("add-int -i 1000 -n etc/usb-time-sigatt", MAKEFILE)
        builder = BUILDER_PATH.read_text()
        for fragment in (
            PATCH_SHA256,
            TRACE_COMMIT,
            "git clone --quiet --no-hardlinks",
            "git -C \"${trace_clone}\" apply --check",
            "GIT_AUTHOR_DATE=2026-09-06T00:00:00Z",
            "non-deterministic SeaBIOS trace commit",
            "CONFIG_DEBUG_USB_TRACE=y",
            "int.from_bytes(data, \"little\") == 1000",
            "python3 -m unittest discover -s tests -v",
            "cmp -s -- \"${first_rom}\" \"${second_rom}\"",
            "msi-x58-pro-e-b06vq-usbtrace1-deterministic-w25q128-16MiB.rom",
        ):
            self.assertIn(fragment, builder)


if __name__ == "__main__":
    unittest.main()
