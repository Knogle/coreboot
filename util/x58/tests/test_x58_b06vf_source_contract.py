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
HANDOFF_HEADER = (BOARD / "b06v6_handoff.h").read_text()
HANDOFF = (BOARD / "b06v6_handoff.c").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
MAINBOARD = (BOARD / "mainboard.c").read_text()
DEVTREE = (BOARD / "devicetree.cb").read_text()
PAYLOAD = (BOARD / "b06vf_payload.c").read_text()
SEABIOS_CONFIG = (BOARD / "config_seabios_b06vf").read_text()
DEFCONFIG = (ROOT / "configs/x58-pro-e-b06vf.config").read_text()

VF_SYMBOL = "CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY"
VE_SYMBOL = "CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF"
VF_ID = "X58PROE-B06VF-SEABIOS-ENTRY-PROBE-20260905"
VE_ID = "X58PROE-B06VE-HIGHQPI-AUTO-RAMSTAGE-ROMMON-20260905"


def between(source: str, first: str, last: str) -> str:
    begin = source.index(first)
    end = source.index(last, begin)
    return source[begin:end]


def function_text(source: str, name: str) -> str:
    """Return one C function, including its signature and balanced body."""
    definition = re.search(
        rf"\b{re.escape(name)}\s*\([^;{{}}]*\)\s*\{{", source, re.DOTALL
    )
    if definition is None:
        raise ValueError(f"no definition for function {name}")
    name_at = definition.start()
    signature_at = source.rfind("\n", 0, name_at) + 1
    body_at = definition.end() - 1
    depth = 0

    for offset in range(body_at, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                return source[signature_at:offset + 1]
    raise ValueError(f"unterminated function {name}")


def preprocessor_block(source: str, marker: str, start: int = 0) -> str:
    """Return a balanced #if block starting with the requested marker."""
    begin = source.index(marker, start)
    depth = 0
    position = begin

    while position < len(source):
        line_end = source.find("\n", position)
        if line_end < 0:
            line_end = len(source)
        line = source[position:line_end].lstrip()
        if re.match(r"#\s*(if|ifdef|ifndef)\b", line):
            depth += 1
        elif re.match(r"#\s*endif\b", line):
            depth -= 1
            if depth == 0:
                return source[begin:line_end]
        position = line_end + 1
    raise ValueError(f"unterminated preprocessor block {marker}")


def zero_ranges(source: str, name: str) -> list[tuple[int, int]]:
    declaration = between(source, f"{name}[] = {{", "};")
    return [
        (int(first, 16), int(last, 16))
        for first, last in re.findall(
            r"\{\s*(0x[0-9a-fA-F]+),\s*(0x[0-9a-fA-F]+)\s*\}",
            declaration,
        )
    ]


def workspace_pair_contract(
    raw_digest: int,
    bytes_2459_245c: tuple[int, int, int, int],
    byte_2461: int,
    canonical_digest: int,
) -> bool:
    if canonical_digest != 0xA6F9C2E6:
        return False
    return (
        raw_digest == 0x94299F43
        and bytes_2459_245c == (0x00, 0x00, 0x00, 0x00)
        and byte_2461 == 0xFE
    ) or (
        raw_digest == 0xB6346533
        and bytes_2459_245c == (0xFF, 0xFF, 0xFF, 0xFF)
        and byte_2461 == 0xFF
    )


class B06VFSourceContractTests(unittest.TestCase):
    def test_option_is_default_off_and_narrowly_depends_on_b06ve_seabios(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06VF_SEABIOS_ENTRY",
            "config X58_PRO_E_BRINGUP_STAGE",
        )

        self.assertIn("depends on X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF", option)
        self.assertIn("depends on PAYLOAD_SEABIOS", option)
        self.assertIn("default n", option)
        self.assertNotIn("default y", option)
        for contract in (
            "reduced SeaBIOS 1.17.0 payload",
            "entry probe, not a",
            "0x94299f43 with 00:00:00:00/fe",
            "0xb6346533",
            "0xa6f9c2e6",
            "complete 0-640-KiB payload",
            "default-off experiment",
        ):
            self.assertIn(contract, option)

    def test_defconfig_and_build_wiring_select_the_exact_vf_payload(self) -> None:
        for symbol in (
            "CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF=y",
            "CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS=y",
            "CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE=y",
            "CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT=y",
            "CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF=y",
            "CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY=y",
            "CONFIG_PAYLOAD_SEABIOS=y",
            "CONFIG_SEABIOS_STABLE=y",
        ):
            self.assertIn(symbol, DEFCONFIG)
        self.assertIn(
            'CONFIG_PAYLOAD_CONFIGFILE="$(top)/src/mainboard/'
            '$(MAINBOARDDIR)/config_seabios_b06vf"',
            DEFCONFIG,
        )
        self.assertIn(
            'CONFIG_MAINBOARD_PART_NUMBER="X58 Pro-E B06VF minimal '
            'SeaBIOS entry probe"',
            DEFCONFIG,
        )
        self.assertIn('CONFIG_LOCALVERSION="x58-pro-e-b06vf"', DEFCONFIG)
        self.assertNotIn("CONFIG_PAYLOAD_NONE=y", DEFCONFIG)
        self.assertIn(
            "ramstage-$(CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY) += "
            "b06vf_payload.c",
            MAKEFILE,
        )

        payload_default = between(
            KCONFIG, "config PAYLOAD_CONFIGFILE", "config MAINBOARD_PART_NUMBER"
        )
        self.assertIn("config_seabios_b06vf", payload_default)
        self.assertIn(VF_SYMBOL.removeprefix("CONFIG_"), payload_default)
        part_defaults = KCONFIG[KCONFIG.index("config MAINBOARD_PART_NUMBER"):]
        self.assertLess(part_defaults.index("B06VF"), part_defaults.index("B06VE"))

    def test_vf_identity_precedes_all_inherited_ve_identities(self) -> None:
        for source in (BOOTBLOCK, ROMSTAGE, RAMMON):
            self.assertIn(VF_ID, source)
            self.assertIn(VE_ID, source)
            self.assertLess(source.index(VF_ID), source.index(VE_ID))

        id_command = between(
            ROMSTAGE,
            'if (b06j_streq(argv[0], "id") && argc == 1)',
            'if (b06j_streq(argv[0], "unlock") && argc == 2)',
        )
        self.assertIn(VF_SYMBOL, id_command)
        self.assertLess(id_command.index(VF_ID), id_command.index(VE_ID))
        self.assertLess(MAINBOARD.index(VF_SYMBOL), MAINBOARD.index(VE_SYMBOL))
        self.assertLess(MAINBOARD.index("B06VF SeaBIOS"), MAINBOARD.index("B06VE High-QPI"))

    def test_handoff_v4_requires_lowmem_smoked_without_weakening_v3(self) -> None:
        version = preprocessor_block(HANDOFF_HEADER, f"#if {VE_SYMBOL}")
        self.assertIn(f"#if {VF_SYMBOL}", version)
        self.assertIn("#define X58_B06V6_HANDOFF_VERSION\t4u", version)
        self.assertIn("#define X58_B06V6_HANDOFF_VERSION\t3u", version)
        self.assertLess(version.index("\t4u"), version.index("\t3u"))

        flags = between(
            HANDOFF_HEADER, "enum x58_b06v6_handoff_flags {", "};"
        )
        self.assertIn("X58_B06VF_HANDOFF_LOWMEM_SMOKED = 1u << 6", flags)
        required = preprocessor_block(HANDOFF, f"#if {VE_SYMBOL}")
        vf_required = preprocessor_block(required, f"#if {VF_SYMBOL}")
        self.assertIn("X58_B06VE_HANDOFF_HIGH_QPI_POST_MINIT", vf_required)
        self.assertIn("X58_B06VF_HANDOFF_LOWMEM_SMOKED", vf_required)
        self.assertIn('"B06VF handoff wire size changed"', HANDOFF)

        exact = function_text(HANDOFF, "x58_b06v6_raminit_result_is_exact")
        self.assertIn("X58_B06VF_EXPECTED_WORKSPACE_RAW_A_FNV", exact)
        self.assertIn("X58_B06VF_EXPECTED_WORKSPACE_RAW_B_FNV", exact)
        self.assertIn("X58_B06VF_EXPECTED_WORKSPACE_CANONICAL_FNV", exact)
        verified = function_text(PAYLOAD, "b06vf_verified_handoff")
        self.assertIn("x58_b06v6_handoff_is_valid(handoff)", verified)
        self.assertIn("X58_B06VF_HANDOFF_LOWMEM_SMOKED", verified)

    def test_vf_canonical_ranges_are_old_ranges_plus_only_five_bytes(self) -> None:
        inherited = zero_ranges(ROMSTAGE, "b06v7_workspace_dynamic_ranges")
        vf_ranges = zero_ranges(ROMSTAGE, "b06vf_workspace_dynamic_ranges")
        additions = [(0x2459, 0x245C), (0x2461, 0x2461)]

        self.assertEqual(vf_ranges, sorted(inherited + additions))
        self.assertEqual(len(vf_ranges), len(inherited) + len(additions))
        self.assertEqual(set(vf_ranges) - set(inherited), set(additions))
        canonical = function_text(ROMSTAGE, "b06vf_workspace_canonical_digest")
        self.assertIn("b06vf_workspace_dynamic_ranges", canonical)
        self.assertIn("X58_VENDOR_MINIT_WORKSPACE_SIZE", canonical)
        self.assertIn(
            "X58_B06VF_EXPECTED_WORKSPACE_CANONICAL_FNV\t0xa6f9c2e6u",
            HANDOFF_HEADER,
        )

    def test_only_the_two_raw_digest_and_tuple_pairs_are_admitted(self) -> None:
        self.assertTrue(
            workspace_pair_contract(
                0x94299F43, (0x00, 0x00, 0x00, 0x00), 0xFE, 0xA6F9C2E6
            )
        )
        self.assertTrue(
            workspace_pair_contract(
                0xB6346533, (0xFF, 0xFF, 0xFF, 0xFF), 0xFF, 0xA6F9C2E6
            )
        )
        for raw, byte_range, byte_2461, canonical in (
            (0x94299F43, (0xFF, 0xFF, 0xFF, 0xFF), 0xFF, 0xA6F9C2E6),
            (0xB6346533, (0x00, 0x00, 0x00, 0x00), 0xFE, 0xA6F9C2E6),
            (0x94299F43, (0x00, 0x00, 0x00, 0x00), 0xFF, 0xA6F9C2E6),
            (0xB6346533, (0xFF, 0xFF, 0xFF, 0xFF), 0xFE, 0xA6F9C2E6),
            (0x94299F43, (0x00, 0x00, 0x00, 0x00), 0xFE, 0xDEADBEEF),
            (0xDEADBEEF, (0x00, 0x00, 0x00, 0x00), 0xFE, 0xA6F9C2E6),
        ):
            self.assertFalse(
                workspace_pair_contract(raw, byte_range, byte_2461, canonical)
            )

        predicate = function_text(ROMSTAGE, "b06vf_workspace_raw_pattern_exact")
        first_pair = between(
            predicate,
            "if (raw_digest == X58_B06VF_EXPECTED_WORKSPACE_RAW_A_FNV)",
            "if (raw_digest == X58_B06VF_EXPECTED_WORKSPACE_RAW_B_FNV)",
        )
        second_pair = predicate[predicate.index(
            "if (raw_digest == X58_B06VF_EXPECTED_WORKSPACE_RAW_B_FNV)"
        ):]
        for offset in range(0x2459, 0x245D):
            self.assertIn(f"workspace[0x{offset:x}] == 0x00", first_pair)
            self.assertIn(f"workspace[0x{offset:x}] == 0xff", second_pair)
        self.assertIn("workspace[0x2461] == 0xfe", first_pair)
        self.assertIn("workspace[0x2461] == 0xff", second_pair)
        self.assertTrue(second_pair.rstrip().endswith("return false;\n}"))

    def test_vf_promotion_gates_raw_pattern_canonical_and_all_endpoints(self) -> None:
        promotion = function_text(ROMSTAGE, "b06ve_promote_post_minit")
        for gate in (
            "b06vf_workspace_canonical_digest(workspace)",
            "b06vf_workspace_raw_pattern_exact(",
            "workspace_pattern_exact",
            "X58_B06VF_EXPECTED_WORKSPACE_RAW_B_FNV",
            "X58_B06VF_EXPECTED_WORKSPACE_CANONICAL_FNV",
            "X58_VENDOR_ERR_PLATFORM_STATE",
            "x58_vendor_b06ve_post_minit_probe(&info)",
            "b06ve_post_minit_tuple_exact",
            "x58_b06v6_handoff_record_success",
            ".workspace_canonical_fnv = workspace_canonical_fnv",
        ):
            self.assertIn(gate, promotion)
        self.assertIn("B06VF_POST_MINIT_CANONICAL_EXACT_GATE", promotion)
        self.assertIn("B06VF_AUTO_HANDOFF=READY", promotion)

    def test_complete_uncached_lowmem_test_precedes_all_cbmem_creation(self) -> None:
        self.assertIn("X58_B06VF_LOWMEM_BASE\t\t0x00000000u", HANDOFF_HEADER)
        self.assertIn("X58_B06VF_LOWMEM_TOP\t\t0x000a0000u", HANDOFF_HEADER)
        destructive = function_text(HANDOFF, "b06v6_destructive_window_test")
        loop = "for (uintptr_t address = base; address < end; address += 4)"
        self.assertEqual(destructive.count(loop), 6)
        self.assertEqual(destructive.count("write32p(address,"), 3)
        self.assertEqual(destructive.count("read32p(address)"), 3)
        self.assertEqual(destructive.count("b06v6_memory_barrier();"), 3)
        self.assertIn("~b06v6_window_pattern(address, seed)", destructive)
        self.assertIn("write32p(address, 0)", destructive)
        self.assertIn("full-window clear", destructive)
        self.assertIn('asm volatile ("mfence" ::: "memory")', HANDOFF)

        postmem = function_text(HANDOFF, "platform_romstage_post_mem")
        lowmem = postmem.index(
            "b06v6_destructive_window_test(X58_B06VF_LOWMEM_BASE"
        )
        vf_at = postmem.rfind(f"#if {VF_SYMBOL}", 0, lowmem)
        self.assertGreaterEqual(vf_at, 0)
        self.assertIn(
            "X58_B06VF_LOWMEM_SIZE",
            preprocessor_block(postmem, f"#if {VF_SYMBOL}", vf_at),
        )
        ordered = (
            "b06v6_mtrr_state_is_exact",
            "b06v6_destructive_window_test(X58_B06VF_LOWMEM_BASE",
            "b06v6_transactional_smoke",
            "b06v6_destructive_window_test(X58_B06V6_CBMEM_BASE",
            "cbmem_initialize_empty_id_size",
        )
        positions = [postmem.index(item) for item in ordered]
        self.assertEqual(positions, sorted(positions))

    def test_only_vf_returns_from_the_dram_rommon_before_its_command_loop(self) -> None:
        rammon = function_text(RAMMON, "b06v6_rammon")
        loop_at = rammon.rfind("\n\tfor (;;) {")
        self.assertGreaterEqual(loop_at, 0)
        vf_at = rammon.rfind(f"#if {VF_SYMBOL}", 0, loop_at)
        self.assertGreaterEqual(vf_at, 0)
        vf_return = preprocessor_block(rammon, f"#if {VF_SYMBOL}", vf_at)
        self.assertLess(vf_at + len(vf_return), loop_at)
        self.assertIn("post_code(POST_B06VF_RAMMON_RETURN)", vf_return)
        self.assertIn("leaving DRAM ROMMON callback", vf_return)
        self.assertIn("return;", vf_return)
        self.assertNotIn(VE_SYMBOL, vf_return)
        self.assertIn(
            "BOOT_STATE_INIT_ENTRY(BS_PRE_DEVICE, BS_ON_ENTRY, "
            "b06v6_rammon, NULL)",
            RAMMON,
        )

    def test_ramstage_reports_only_the_explicitly_tested_memory_ranges(self) -> None:
        resources = function_text(PAYLOAD, "b06vf_read_resources")
        for resource in (
            "ram_range(dev, 0, 0x00000000, 0x000a0000)",
            "mmio_range(dev, 1, 0x000a0000, 0x00020000)",
            "reserved_ram_range(dev, 2, 0x000c0000, 0x00040000)",
            "ram_range(dev, 3, X58_B06V6_CBMEM_BASE, X58_B06V6_CBMEM_SIZE)",
            "ram_range(dev, 4, X58_B06V6_OBJECT_BASE, X58_B06V6_OBJECT_SIZE)",
        ):
            self.assertIn(resource, resources)
        self.assertEqual(resources.count("ram_range("), 4)

        domain_ops = between(
            PAYLOAD,
            "static struct device_operations b06vf_domain_ops",
            "static struct device_operations b06vf_cpu_cluster_ops",
        )
        self.assertIn(".read_resources = b06vf_read_resources", domain_ops)
        self.assertIn(".set_resources = noop_set_resources", domain_ops)
        self.assertNotIn(".scan_bus", domain_ops)
        self.assertNotIn("pci_scan_bus", PAYLOAD)
        self.assertNotIn("device pci", DEVTREE)
        enable = function_text(PAYLOAD, "x58_b06vf_enable_dev")
        self.assertIn("DEVICE_PATH_DOMAIN", enable)
        self.assertIn("DEVICE_PATH_CPU_CLUSTER", enable)
        self.assertNotIn("DEVICE_PATH_PCI", enable)

    def test_sad_and_every_pam_write_are_exactly_read_back_or_fail_closed(self) -> None:
        self.assertIn("#define B06VF_SAD_EXPECTED_ID\t0x2d818086u", PAYLOAD)
        pam = between(PAYLOAD, "b06vf_pam_open[B06VF_PAM_COUNT] = {", "};")
        self.assertEqual(
            [int(value, 16) for value in re.findall(r"0x[0-9a-fA-F]+", pam)],
            [0x30, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33],
        )

        open_shadow = function_text(PAYLOAD, "b06vf_open_payload_shadow")
        ordered = (
            "b06vf_verified_handoff()",
            "pci_io_read_config32(B06VF_SAD_DEV, PCI_VENDOR_ID)",
            "id != B06VF_SAD_EXPECTED_ID",
            "before[i] = pci_io_read_config8",
            "pci_io_write_config8(B06VF_SAD_DEV",
            "after[i] = pci_io_read_config8",
            "memcmp(after, b06vf_pam_open, sizeof(after))",
            "b06vf_shadow_decode_probe()",
        )
        positions = [open_shadow.index(item) for item in ordered]
        self.assertEqual(positions, sorted(positions))
        self.assertGreaterEqual(open_shadow.count("b06vf_restore_pam(before)"), 2)
        self.assertIn("POST_B06VF_SAD_ID_FAIL", open_shadow)
        self.assertIn("POST_B06VF_PAM_FAIL", open_shadow)

        restore = function_text(PAYLOAD, "b06vf_restore_pam")
        self.assertIn("before[i]", restore)
        self.assertIn("== before[i]", restore)
        self.assertIn("b06vf_fence()", restore)

    def test_all_34_shadow_words_are_simultaneous_and_reverse_restored(self) -> None:
        self.assertIn("#define B06VF_SHADOW_BLOCK\t0x00004000u", PAYLOAD)
        self.assertIn("_Static_assert(B06VF_SHADOW_BLOCKS == 16", PAYLOAD)
        self.assertIn("#define B06VF_SEABIOS_LOAD_BASE\t0x000f8800u", PAYLOAD)
        self.assertIn("#define B06VF_SEABIOS_ENTRY_WORD\t0x000fecd4u", PAYLOAD)
        self.assertIn("#define B06VF_SHADOW_SPECIAL_WORDS\t2", PAYLOAD)
        self.assertIn(
            "#define B06VF_SHADOW_PROBE_WORDS\t(B06VF_SHADOW_BLOCKS * 2 + \\",
            PAYLOAD,
        )
        self.assertIn("B06VF_SHADOW_SPECIAL_WORDS)", PAYLOAD)
        shadow_addresses = [
            address
            for block in range(16)
            for address in (
                0xC0000 + block * 0x4000,
                0xC0000 + (block + 1) * 0x4000 - 4,
            )
        ] + [0xF8800, 0xFECD4]
        self.assertEqual(len(shadow_addresses), 34)
        self.assertEqual(len(set(shadow_addresses)), 34)
        self.assertEqual(0xFECD4 % 4, 0)
        shadow_patterns = [
            (0xB06F0000 ^ address ^ (((index + 1) * 0x01010101) & 0xFFFFFFFF))
            & 0xFFFFFFFF
            for index, address in enumerate(shadow_addresses)
        ]
        self.assertEqual(len(set(shadow_patterns)), 34)

        address = function_text(PAYLOAD, "b06vf_shadow_probe_address")
        address_order = (
            "index < B06VF_SHADOW_BLOCKS * 2",
            "const size_t block = index / 2",
            "const uintptr_t first = B06VF_SHADOW_BASE +",
            "return (index & 1) ?",
            "first + B06VF_SHADOW_BLOCK - sizeof(uint32_t) : first",
            "index == B06VF_SHADOW_BLOCKS * 2",
            "return B06VF_SEABIOS_LOAD_BASE",
            "return B06VF_SEABIOS_ENTRY_WORD",
        )
        positions = [address.index(item) for item in address_order]
        self.assertEqual(positions, sorted(positions))

        probe = function_text(PAYLOAD, "b06vf_shadow_decode_probe")
        self.assertIn(
            "struct b06vf_shadow_word words[B06VF_SHADOW_PROBE_WORDS]",
            probe,
        )
        transaction_order = (
            "words[i].address = b06vf_shadow_probe_address(i)",
            "words[i].original = *(volatile const uint32_t *)words[i].address",
            "words[i].pattern = 0xb06f0000u ^ (uint32_t)words[i].address",
            "*(volatile uint32_t *)words[i].address = words[i].pattern",
            "b06vf_fence()",
            "exact &= *(volatile const uint32_t *)words[i].address ==",
            "return b06vf_restore_shadow(words) && exact",
        )
        positions = [probe.index(item) for item in transaction_order]
        self.assertEqual(positions, sorted(positions))
        self.assertIn(
            "((uint32_t)(i + 1) * 0x01010101u)",
            probe,
        )
        self.assertEqual(
            probe.count(
                "for (size_t i = 0; i < B06VF_SHADOW_PROBE_WORDS; i++)"
            ),
            3,
        )

        restore = function_text(PAYLOAD, "b06vf_restore_shadow")
        reverse = (
            "for (size_t i = B06VF_SHADOW_PROBE_WORDS; i > 0; i--)",
            "words[i - 1].address = words[i - 1].original",
            "b06vf_fence()",
            "for (size_t i = 0; i < B06VF_SHADOW_PROBE_WORDS; i++)",
            "words[i].address ==",
            "words[i].original",
        )
        positions = [restore.index(item) for item in reverse]
        self.assertEqual(positions, sorted(positions))
        self.assertIn(
            "*(volatile uint32_t *)words[i - 1].address = "
            "words[i - 1].original;",
            restore,
        )
        self.assertIn("return exact;", restore)

    def test_payload_bootstate_hooks_preserve_shadow_load_and_entry_order(self) -> None:
        hooks = (
            "BOOT_STATE_INIT_ENTRY(BS_WRITE_TABLES, BS_ON_EXIT, "
            "b06vf_tables_written, NULL)",
            "BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_LOAD, BS_ON_ENTRY, "
            "b06vf_open_payload_shadow, NULL)",
            "BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_LOAD, BS_ON_EXIT, "
            "b06vf_payload_loaded, NULL)",
            "BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_BOOT, BS_ON_ENTRY, "
            "b06vf_payload_boot, NULL)",
        )
        positions = []
        for hook in hooks:
            self.assertIn(hook, PAYLOAD)
            positions.append(PAYLOAD.index(hook))
        self.assertEqual(positions, sorted(positions))
        self.assertIn(".enable_dev = x58_b06vf_enable_dev", MAINBOARD)

    def test_seabios_is_reduced_with_no_vga_rom_or_smbios_generation(self) -> None:
        for disabled in (
            "CONFIG_THREADS",
            "CONFIG_RELOCATE_INIT",
            "CONFIG_MALLOC_UPPERMEMORY",
            "CONFIG_DRIVES",
            "CONFIG_USB",
            "CONFIG_SERIAL",
            "CONFIG_SERCON",
            "CONFIG_PCIBIOS",
            "CONFIG_OPTIONROMS",
            "CONFIG_BOOT",
            "CONFIG_VGAHOOKS",
        ):
            self.assertIn(f"# {disabled} is not set", SEABIOS_CONFIG)
            self.assertNotIn(f"{disabled}=y", SEABIOS_CONFIG)

        self.assertIn("# CONFIG_VGA_ROM_RUN is not set", DEFCONFIG)
        self.assertIn("CONFIG_NO_GFX_INIT=y", DEFCONFIG)
        self.assertIn("# CONFIG_GENERATE_SMBIOS_TABLES is not set", DEFCONFIG)
        self.assertNotIn("CONFIG_VGA_ROM_RUN=y", DEFCONFIG)
        self.assertNotIn("CONFIG_GENERATE_SMBIOS_TABLES=y", DEFCONFIG)


if __name__ == "__main__":
    unittest.main()
