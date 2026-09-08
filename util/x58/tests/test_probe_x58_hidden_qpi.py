import importlib.util
from pathlib import Path
import sys
import unittest


SCRIPTS = Path(__file__).parents[1] / "scripts"
sys.path.insert(0, str(SCRIPTS))
SCRIPT = SCRIPTS / "probe_x58_hidden_qpi.py"
SPEC = importlib.util.spec_from_file_location("probe_x58_hidden_qpi", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class HiddenQpiProbeTests(unittest.TestCase):
    def test_describe_visible_and_hidden_config(self):
        visible = MODULE.describe_config(bytes.fromhex("86800134") + bytes(252))
        hidden = MODULE.describe_config(bytes((0xFF,)) * 256)
        self.assertEqual(visible["vendor_id"], "0x8086")
        self.assertEqual(visible["device_id"], "0x3401")
        self.assertTrue(visible["accessible"])
        self.assertFalse(hidden["accessible"])

    def test_mask_covers_only_device_16_and_17_functions(self):
        self.assertEqual(MODULE.QPI_HIDE_MASK, sum(1 << bit for bit in range(26, 30)))


if __name__ == "__main__":
    unittest.main()
