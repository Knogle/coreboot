#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Host-only tests of public vendor guard metadata; no firmware bytes needed."""

import re
import subprocess
import tempfile
import unittest
from pathlib import Path

from x58_test_paths import COREBOOT_ROOT


SOURCE = (COREBOOT_ROOT / "src/mainboard/msi/x58_pro_e/vendor_init.c").read_text()


class VendorSignatureDigestTests(unittest.TestCase):
    def test_public_source_retains_all_fourteen_range_guards(self):
        calls = re.findall(
            r"signature_digest_valid\(([^,]+), (\d+), (0x[0-9a-f]+u)\)", SOURCE
        )
        self.assertEqual(len(calls), 14)
        self.assertEqual(sum(int(size) for _, size, _ in calls), 199)
        self.assertNotIn("signature_equal(", SOURCE)
        for name in ("wrapper_entry", "helper_entry", "cmos_helper", "entry"):
            self.assertNotIn(f"static const uint8_t {name}[]", SOURCE)

    def test_whole_image_hash_and_pe_guards_remain_required(self):
        for symbol in ("CSI_WRAPPER", "CSI_HELPER", "CSI_IMAGE", "MINIT_IMAGE"):
            self.assertIn(f"X58_VENDOR_{symbol}_FNV1A", SOURCE)
        self.assertIn("pe32_header_valid(X58_VENDOR_CSI_IMAGE_BASE", SOURCE)
        self.assertIn("pe32_header_valid(X58_VENDOR_MINIT_IMAGE_BASE", SOURCE)

    def test_actual_c_digest_helper_with_synthetic_data(self):
        start = SOURCE.index("static bool signature_digest_valid(")
        end = SOURCE.index("\nstatic ", start + 1)
        helper = SOURCE[start:end]
        self.assertIn("const volatile uint8_t *actual", helper)
        program = """
            #include <assert.h>
            #include <stdbool.h>
            #include <stdint.h>
            #include <stddef.h>
        """ + helper + """
            int main(void)
            {
                uint8_t text[] = "foobar";
                assert(signature_digest_valid((uintptr_t)text, 0, 0x811c9dc5u));
                assert(signature_digest_valid((uintptr_t)text, 6, 0xbf9cf968u));
                assert(!signature_digest_valid((uintptr_t)text, 5, 0xbf9cf968u));
                assert(!signature_digest_valid((uintptr_t)text, 6, 0xbf9cf969u));
                for (size_t byte = 0; byte < 6; byte++) {
                    for (unsigned bit = 0; bit < 8; bit++) {
                        text[byte] ^= 1u << bit;
                        assert(!signature_digest_valid((uintptr_t)text, 6,
                                                       0xbf9cf968u));
                        text[byte] ^= 1u << bit;
                    }
                }
                return 0;
            }
        """
        with tempfile.TemporaryDirectory(prefix="x58-public-signature-") as temp:
            executable = Path(temp) / "guard-test"
            compiled = subprocess.run(
                ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-x", "c", "-",
                 "-o", str(executable)],
                input=program, text=True, capture_output=True, timeout=30,
            )
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            ran = subprocess.run([str(executable)], capture_output=True, timeout=30)
            self.assertEqual(ran.returncode, 0, ran.stderr)


if __name__ == "__main__":
    unittest.main()
