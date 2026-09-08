import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).parents[1] / "scripts" / "read_mmconfig.py"
SPEC = importlib.util.spec_from_file_location("read_mmconfig", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class MmconfigReadTests(unittest.TestCase):
    def test_address_formula_matches_x58_pciexbar(self):
        bdf = MODULE.parse_bdf("0000:ff:03.4")
        self.assertEqual(
            MODULE.mmconfig_address(0xE0000000, *bdf, 0xF8),
            0xEFF1C0F8,
        )

    def test_rejects_nonzero_segment_and_out_of_range_device(self):
        with self.assertRaises(ValueError):
            MODULE.parse_bdf("0001:00:00.0")
        with self.assertRaises(ValueError):
            MODULE.parse_bdf("00:20.0")

    def test_read_config_uses_read_only_file_offset(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "memory"
            path.write_bytes(bytes(range(64)))
            data = MODULE.read_config(path, 0, (0, 0, 0), 4)
        self.assertEqual(data, bytes((0, 1, 2, 3)))


if __name__ == "__main__":
    unittest.main()
