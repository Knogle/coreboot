import importlib.util
from pathlib import Path
import sys
import unittest


SCRIPT = Path(__file__).parents[1] / "scripts" / "read_spd_i2c.py"
SPEC = importlib.util.spec_from_file_location("read_spd_i2c", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class SpdReadTests(unittest.TestCase):
    def test_decodes_basic_ddr3_udimm_geometry(self):
        payload = bytearray(256)
        payload[2] = 0x0B
        payload[3] = 0x02
        payload[4] = 0x03
        payload[5] = 0x19
        payload[7] = (1 << 3) | 1  # two ranks, x8 devices
        payload[8] = 0x03  # 64-bit primary bus, no extension
        payload[10] = 1
        payload[11] = 8
        payload[12] = 12
        payload[128:146] = b"CMX8GX3M2A1333C9".ljust(18, b" ")
        decoded = MODULE.basic_ddr3_fields(bytes(payload))
        self.assertEqual(decoded["memory_type"], "DDR3 SDRAM")
        self.assertEqual(decoded["module_type"], "UDIMM")
        self.assertEqual(decoded["rank_count"], 2)
        self.assertEqual(decoded["device_width_bits"], 8)
        self.assertEqual(decoded["primary_bus_width_bits"], 64)
        self.assertEqual(decoded["bus_width_extension_bits"], 0)
        self.assertEqual(decoded["module_capacity_mib"], 4096)
        self.assertEqual(decoded["tck_min_ns"], 1.5)
        self.assertEqual(decoded["maximum_transfer_rate_mt_s"], 1333)
        self.assertEqual(decoded["part_number"], "CMX8GX3M2A1333C9")


if __name__ == "__main__":
    unittest.main()
