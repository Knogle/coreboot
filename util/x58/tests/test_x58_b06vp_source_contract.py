#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
from x58_test_paths import COREBOOT_ROOT as COREBOOT
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
KCONFIG = (BOARD / "Kconfig").read_text()
BOARD_MAKE = (BOARD / "Makefile.mk").read_text()
BOOTBLOCK = (BOARD / "bootblock.c").read_text()
ROMSTAGE = (BOARD / "romstage.c").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
PCI_SOURCE = (BOARD / "b06vn_pci.c").read_text()
VO_DEVTREE = (BOARD / "devicetree_b06vo.cb").read_text()
SEABIOS_CONFIG = (BOARD / "config_seabios_b06vm").read_text()
LAPIC_SOURCE = (COREBOOT / "src/cpu/x86/lapic/lapic.c").read_text()
LAPIC_MAKE = (COREBOOT / "src/cpu/x86/lapic/Makefile.mk").read_text()
VO_CONFIG = (ROOT / "configs/x58-pro-e-b06vo.config").read_text()
VP_CONFIG = (ROOT / "configs/x58-pro-e-b06vp.config").read_text()
VP_BUILD = (ROOT / "scripts/build_x58_b06vp.sh").read_text()

VP_SYMBOL = "CONFIG_X58_PRO_E_B06VP_LAPIC_EXTINT"
VO_SYMBOL = "CONFIG_X58_PRO_E_B06VO_IOHBUSNO_ROUTE"
VP_ID = "X58PROE-B06VP-LAPIC-EXTINT-PROBE-20260906"
VO_ID = "X58PROE-B06VO-IOHBUSNO-ROUTE-PROBE-20260906"


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
            key = line.split("=", 1)[0]
            result[key] = line
        elif line.startswith("# CONFIG_") and line.endswith(" is not set"):
            key = line[2:].split(" ", 1)[0]
            result[key] = line
    return result


class B06VPSourceContractTests(unittest.TestCase):
    def test_separate_default_off_successor_is_lapic_only(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VP_LAPIC_EXTINT",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        for fragment in (
            "depends on X58_PRO_E_B06VO_IOHBUSNO_ROUTE",
            "depends on !SMP",
            "depends on XAPIC_ONLY",
            "depends on !NO_PCAT_8259",
            "depends on SEABIOS_HARDWARE_IRQ",
            "depends on BUILD_IPXE",
            "depends on !IPXE_NO_PROMPT",
            "default n",
            "setup_lapic_interrupts() exactly",
            "initialize APs, the PIC, PIT or IOAPIC",
            "GPU, SeaBIOS or iPXE policy",
        ):
            self.assertIn(fragment, option)
        self.assertNotIn("default y", option)

    def test_identity_and_inherited_tree_priorities_precede_b06vo(self) -> None:
        tree = between(KCONFIG, "config DEVICETREE", "config X58_PRO_E_B06M")
        self.assertLess(
            tree.index('"devicetree_b06vo.cb" if X58_PRO_E_B06VP'),
            tree.index('"devicetree_b06vo.cb" if X58_PRO_E_B06VO'),
        )
        payload = between(KCONFIG, "config PAYLOAD_CONFIGFILE", "config MAINBOARD_PART_NUMBER")
        self.assertLess(payload.index("B06VP"), payload.index("B06VO"))
        part = between(KCONFIG, "config MAINBOARD_PART_NUMBER", "endif")
        self.assertLess(part.index("B06VP"), part.index("B06VO"))

        for source in (BOOTBLOCK, ROMSTAGE, RAMMON, MAINBOARD, PCI_SOURCE):
            vp_marker = VP_SYMBOL if source == MAINBOARD else VP_ID
            vo_marker = VO_SYMBOL if source == MAINBOARD else VO_ID
            self.assertIn(vp_marker, source)
            self.assertIn(vo_marker, source)
            self.assertLess(source.index(vp_marker), source.index(vo_marker))

        pci_lines = re.findall(
            r"^\s*device pci ([0-9a-f]{2}\.[0-7]) mandatory ops (\w+)",
            VO_DEVTREE,
            re.MULTILINE,
        )
        self.assertEqual(len(pci_lines), 14)

    def test_b06vp_config_is_b06vo_plus_identity_and_one_option(self) -> None:
        vo = config_contract(VO_CONFIG)
        vp = config_contract(VP_CONFIG)
        allowed = {
            VP_SYMBOL,
            "CONFIG_MAINBOARD_PART_NUMBER",
            "CONFIG_LOCALVERSION",
        }
        self.assertEqual(
            {key: value for key, value in vo.items() if key not in allowed},
            {key: value for key, value in vp.items() if key not in allowed},
        )
        self.assertEqual(vp[VP_SYMBOL], f"{VP_SYMBOL}=y")
        self.assertEqual(
            vp["CONFIG_MAINBOARD_PART_NUMBER"],
            'CONFIG_MAINBOARD_PART_NUMBER="X58 Pro-E B06VP LAPIC ExtINT probe"',
        )
        self.assertEqual(
            vp["CONFIG_LOCALVERSION"], 'CONFIG_LOCALVERSION="x58-pro-e-b06vp"'
        )

    def test_payload_hardware_irq_and_prompt_contract_is_unchanged(self) -> None:
        for fragment in (
            "CONFIG_PAYLOAD_SEABIOS=y",
            "CONFIG_SEABIOS_HARDWARE_IRQ=y",
            'CONFIG_PXE_ROM_ID="10ec,8168"',
            "# CONFIG_IPXE_SERIAL_CONSOLE is not set",
            "# CONFIG_IPXE_NO_PROMPT is not set",
            "# CONFIG_VGA_ROM_RUN is not set",
            "CONFIG_NO_GFX_INIT=y",
        ):
            self.assertIn(fragment, VP_CONFIG)

        for fragment in (
            "CONFIG_HARDWARE_IRQ=y",
            "CONFIG_RTC_TIMER=y",
            "CONFIG_PMTIMER=y",
            "CONFIG_TSC_TIMER=y",
            "CONFIG_OPTIONROMS=y",
            "CONFIG_PCIBIOS=y",
            "CONFIG_KEYBOARD=y",
            "CONFIG_SERCON=y",
        ):
            self.assertIn(fragment, SEABIOS_CONFIG)
        self.assertNotIn("# CONFIG_HARDWARE_IRQ is not set", SEABIOS_CONFIG)

    def test_existing_coreboot_helper_is_used_without_a_private_clone(self) -> None:
        helper = function_text(LAPIC_SOURCE, "setup_lapic_interrupts")
        for fragment in (
            "lapic_update32(LAPIC_TASKPRI, ~LAPIC_TPRI_MASK, 0)",
            "LAPIC_SPIV_ENABLE | 0xF",
            "LAPIC_LVT_MASKED | LAPIC_LVT_LEVEL_TRIGGER",
            "LAPIC_INPUT_POLARITY",
            "LAPIC_DELIVERY_MODE_MASK",
            "boot_cpu() && !CONFIG(NO_PCAT_8259)",
            "LAPIC_DELIVERY_MODE_EXTINT",
            "LAPIC_DELIVERY_MODE_NMI",
        ):
            self.assertIn(fragment, helper)
        # Four writes execute: TPR, SPIV, one of the two LVT0 branches, LVT1.
        self.assertEqual(helper.count("lapic_update32("), 5)
        self.assertIn("all_x86-y += lapic.c", LAPIC_MAKE)
        self.assertEqual(PCI_SOURCE.count("setup_lapic_interrupts();"), 1)
        self.assertNotIn("lapic.c", BOARD_MAKE)

    def test_apic_base_is_gated_before_any_lapic_mmio(self) -> None:
        base_gate = function_text(PCI_SOURCE, "b06vp_apic_base_is_expected")
        for fragment in (
            "apic_base.hi == 0",
            "LAPIC_BASE_MSR_ADDR_MASK",
            "LAPIC_DEFAULT_BASE",
            "LAPIC_BASE_MSR_ENABLE",
            "LAPIC_BASE_MSR_X2APIC_MODE",
            "LAPIC_BASE_MSR_BOOTSTRAP_PROCESSOR",
        ):
            self.assertIn(fragment, base_gate)

        init = function_text(PCI_SOURCE, "b06vp_cpu_cluster_init")
        self.assertLess(
            init.index("rdmsr(LAPIC_BASE_MSR)"),
            init.index("b06vp_read_lapic_snapshot"),
        )
        self.assertLess(
            init.index("b06vp_apic_base_is_expected"),
            init.index("b06vp_read_lapic_snapshot"),
        )

    def test_pre_post_snapshot_and_exact_selected_field_gates(self) -> None:
        snapshot = function_text(PCI_SOURCE, "b06vp_read_lapic_snapshot")
        for register in ("LAPIC_TASKPRI", "LAPIC_SPIV", "LAPIC_LVT0", "LAPIC_LVT1"):
            self.assertEqual(snapshot.count(f"lapic_read({register})"), 1)

        logger = function_text(PCI_SOURCE, "b06vp_log_lapic_snapshot")
        self.assertIn("TPR=%08x SVR=%08x LVT0=%08x LVT1=%08x", logger)

        gate = function_text(PCI_SOURCE, "b06vp_lapic_state_is_expected")
        for fragment in (
            "LAPIC_TPRI_MASK",
            "LAPIC_SPIV_ENABLE",
            "LAPIC_VECTOR_MASK",
            "LAPIC_SPIV_ENABLE | 0x0f",
            "LAPIC_LVT_MASKED",
            "LAPIC_LVT_LEVEL_TRIGGER",
            "LAPIC_INPUT_POLARITY",
            "LAPIC_DELIVERY_MODE_MASK",
            "LAPIC_DELIVERY_MODE_EXTINT",
            "LAPIC_DELIVERY_MODE_NMI",
        ):
            self.assertIn(fragment, gate)

        base_logger = function_text(PCI_SOURCE, "b06vp_log_apic_base")
        self.assertIn("IA32_APIC_BASE=%08x:%08x", base_logger)

    def test_cpu_cluster_calls_helper_once_on_bsp_and_fails_closed(self) -> None:
        init = function_text(PCI_SOURCE, "b06vp_cpu_cluster_init")
        for fragment in (
            'b06vp_log_lapic_snapshot("PRE"',
            "b06vp_lapic_setup_attempted || !boot_cpu()",
            "b06vp_lapic_setup_attempted = true;",
            "post_code(POST_B06VP_LAPIC_BEGIN);",
            "setup_lapic_interrupts();",
            'b06vp_log_lapic_snapshot("POST"',
            "!b06vp_lapic_state_is_expected",
            "POST_B06VP_LAPIC_FAIL",
            "post_code(POST_B06VP_LAPIC_OK);",
        ):
            self.assertIn(fragment, init)
        self.assertEqual(init.count("setup_lapic_interrupts();"), 1)
        self.assertEqual(init.count("rdmsr(LAPIC_BASE_MSR)"), 2)
        self.assertEqual(init.count("b06vp_read_lapic_snapshot("), 2)
        self.assertEqual(init.count("b06vp_log_lapic_snapshot("), 2)
        self.assertEqual(init.count("b06vp_log_apic_base("), 2)
        ordered = (
            'b06vp_log_apic_base("PRE"',
            "b06vp_apic_base_is_expected(apic_base)",
            'b06vp_log_lapic_snapshot("PRE"',
            "b06vp_lapic_setup_attempted = true;",
            "post_code(POST_B06VP_LAPIC_BEGIN);",
            "setup_lapic_interrupts();",
            'b06vp_log_lapic_snapshot("POST"',
            "!b06vp_lapic_state_is_expected",
            "post_code(POST_B06VP_LAPIC_OK);",
        )
        positions = [init.index(fragment) for fragment in ordered]
        self.assertEqual(positions, sorted(positions))
        self.assertNotRegex(
            init,
            r"\b(?:wrmsr|write(?:8|16|32|64)p?|out[blw]|pci_\w*write\w*)\s*\(",
        )

        cpu_ops = between(
            PCI_SOURCE,
            "static struct device_operations b06vn_cpu_cluster_ops",
            "struct device_operations b06vn_root_port_ops",
        )
        self.assertIn("#if CONFIG_X58_PRO_E_B06VP_LAPIC_EXTINT", cpu_ops)
        self.assertIn(".init = b06vp_cpu_cluster_init", cpu_ops)
        self.assertNotIn("mp_cpu_bus_init", cpu_ops)

        values = re.findall(
            r"#define POST_B06VP_LAPIC_(?:BEGIN|OK|FAIL)\s+(0x[0-9a-f]+)",
            PCI_SOURCE,
        )
        self.assertEqual(len(values), 3)
        self.assertEqual(len(set(values)), 3)

    def test_no_memory_qpi_ioh_gpu_or_pci_path_delta(self) -> None:
        for function in (
            "b06vo_program_ioh_bus_number_once",
            "b06vn_start_iou0_once",
            "b06vn_domain_scan_bus",
            "b06vn_probe_gate",
            "b06vn_resources_assigned",
            "b06vn_resources_enabled",
        ):
            self.assertNotIn("B06VP", function_text(PCI_SOURCE, function))

        vp_functions = "".join(
            function_text(PCI_SOURCE, name)
            for name in (
                "b06vp_apic_base_is_expected",
                "b06vp_read_lapic_snapshot",
                "b06vp_log_apic_base",
                "b06vp_log_lapic_snapshot",
                "b06vp_lapic_state_is_expected",
                "b06vp_cpu_cluster_init",
            )
        )
        for forbidden in (
            "pci_io_",
            "B06VN_ECAM",
            "B06VO_IOHBUSNO",
            "write16p",
            "write32p",
            "MTRR",
            "QPI",
            "MINIT",
            "GPU",
        ):
            self.assertNotIn(forbidden, vp_functions)

    def test_builder_is_deterministic_and_audits_final_configs(self) -> None:
        for fragment in (
            "configs/x58-pro-e-b06vp.config",
            "msi-x58-pro-e-b06vp-coreboot-base-4MiB.rom",
            "CONFIG_X58_PRO_E_B06VP_LAPIC_EXTINT=y",
            "CONFIG_SEABIOS_HARDWARE_IRQ=y",
            "# CONFIG_IPXE_NO_PROMPT is not set",
            "CONFIG_SMP=n",
            "CONFIG_NO_PCAT_8259=n",
            "CONFIG_HARDWARE_IRQ=y",
            "verify_prompted_ipxe_source",
            "tests.test_x58_b06vp_source_contract",
            "X58_B06VP_SOURCE_DATE_EPOCH",
            'build_once "${first_rom}"',
            'build_once "${second_rom}"',
            'cmp -s -- "${first_rom}" "${second_rom}"',
            "--include-csi-wrapper-support",
            "--flash-size 0x1000000",
        ):
            self.assertIn(fragment, VP_BUILD)


if __name__ == "__main__":
    unittest.main()
