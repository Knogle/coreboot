#!/usr/bin/env python3

import unittest
from pathlib import Path
from x58_test_paths import COREBOOT_ROOT as COREBOOT


ROOT = Path(__file__).resolve().parents[1]
BOARD = COREBOOT / "src/mainboard/msi/x58_pro_e"
ROMSTAGE = (BOARD / "romstage.c").read_text()
VENDOR_INIT = (BOARD / "vendor_init.c").read_text()
BOOTBLOCK = (BOARD / "bootblock.c").read_text()
RAMMON = (BOARD / "ramstage_rommon.c").read_text()
KCONFIG = (BOARD / "Kconfig").read_text()
DEFCONFIG = (ROOT / "configs/x58-pro-e-b06v9.config").read_text()


def between(source: str, first: str, last: str) -> str:
    begin = source.index(first)
    end = source.index(last, begin)
    return source[begin:end]


class B06V9SourceContractTests(unittest.TestCase):
    def test_config_is_explicit_and_default_off(self) -> None:
        option = between(
            KCONFIG,
            "config X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS",
            "config X58_PRO_E_BRINGUP_STAGE",
        )
        self.assertIn("depends on X58_PRO_E_B06V8_HIGH_QPI_3PASS", option)
        self.assertIn("default n", option)
        for generation in range(0, 10):
            symbol = ("CONFIG_X58_PRO_E_B06V0_VENDOR_INIT" if generation == 0
                      else f"CONFIG_X58_PRO_E_B06V{generation}_" )
            self.assertIn(symbol, DEFCONFIG)
        self.assertIn("CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS=y", DEFCONFIG)

    def test_wrapper_digest_and_six_signature_ranges_are_pinned(self) -> None:
        v9_digest = VENDOR_INIT.index(
            "X58_VENDOR_CSI_WRAPPER_FNV1A\t0x98e2f3deu"
        )
        v8_digest = VENDOR_INIT.index(
            "X58_VENDOR_CSI_WRAPPER_FNV1A\t0x986153c5u"
        )
        self.assertLess(v9_digest, v8_digest)
        checks = (
            "0xfffc0510u, 10, 0x9321081du",
            "0xfffc0574u, 9, 0xcc90302eu",
            "0xfffc0667u, 9, 0x06f2f37eu",
            "0xfffc0838u, 15, 0x9b2cef9au",
            "0xfffc0a05u, 15, 0xc2398590u",
            "0xfffc0bbeu, 16, 0x18b7b7e9u",
        )
        for arguments in checks:
            self.assertIn(f"signature_digest_valid({arguments})", VENDOR_INIT)

    def test_rtc_upper_bank_write_is_exact_and_precedes_vendor_path(self) -> None:
        helper = between(
            ROMSTAGE,
            "static void b06v9_enable_rtc_upper_bank(",
            "static bool b06v9_report_rtc_upper_bank(",
        )
        self.assertIn("state->before = RCBA32(B06V9_ICH10_RC)", helper)
        self.assertIn(
            "state->before != 0 && state->before != B06V9_ICH10_RC_U128E",
            helper,
        )
        self.assertIn(
            "RCBA32(B06V9_ICH10_RC) = state->before | B06V9_ICH10_RC_U128E",
            helper,
        )
        self.assertIn("state->after != B06V9_ICH10_RC_U128E", helper)
        self.assertIn("POST_B06V9_RTC_PRECHECK", helper)
        self.assertIn("POST_B06V9_RTC_ENABLED", helper)

        startup = between(
            ROMSTAGE,
            "/* Existing coreboot ICH10 early BAR setup; no GPIO pins are touched. */",
            "outb(POST_B04_UART_SENT, CONFIG_POST_IO_PORT);",
        )
        setup = startup.index("i82801jx_setup_bars()")
        validate = startup.index("ich10_readback_error")
        enable = startup.index("b06v9_enable_rtc_upper_bank")
        report = startup.index("b06v9_report_rtc_upper_bank")
        self.assertLess(setup, validate)
        self.assertLess(validate, enable)
        self.assertLess(enable, report)

    def test_v9_ignores_rtc_payload_and_uses_deterministic_state(self) -> None:
        legacy = between(
            ROMSTAGE,
            "#if !CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS\n"
            "#define B06V8_EXT_CMOS_INDEX_PORT",
            "static void b06v8_capture_qpi_tuple",
        )
        self.assertIn("b06v8_capture_cmos_inputs", legacy)
        self.assertIn("b06v8_cmos_inputs_exact", legacy)

        probe = between(
            ROMSTAGE,
            "static enum b06v6_auto_result b06v8_high_qpi_probe(",
            "static enum b06v6_auto_result __maybe_unused b06v6_auto_handoff(",
        )
        rtc_gate = probe.index("B06V9_RTC_UPPER_BANK_GATE")
        profile = probe.index("B06V9_PROFILE=STATE06/07:01/01")
        arm = probe.index("x58_vendor_arm_csi_wrapper")
        call = probe.index("x58_vendor_call_csi_wrapper")
        self.assertLess(rtc_gate, profile)
        self.assertLess(profile, arm)
        self.assertLess(arm, call)
        self.assertIn("RTC_INPUT_DEPENDENCY=NONE", probe)
        self.assertIn("POST_B06V9_PROFILE_READY", probe)
        self.assertNotIn("x58_vendor_call_minit", probe)

    def test_v9_identity_is_visible_in_every_stage(self) -> None:
        identity = "X58PROE-B06V9-DETERMINISTIC-HIGHQPI-3PASS-ROMMON-20260904"
        self.assertIn(identity, BOOTBLOCK)
        self.assertGreaterEqual(ROMSTAGE.count(identity), 2)
        self.assertIn("X58PROE-B06V9-UNEXPECTED-RAMSTAGE-20260904", RAMMON)
        self.assertIn("B06V9 deterministic High-QPI three-pass ROMMON", KCONFIG)


if __name__ == "__main__":
    unittest.main()
