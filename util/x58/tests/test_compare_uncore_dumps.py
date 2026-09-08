import importlib.util
from pathlib import Path
import struct
import unittest


SCRIPT = Path(__file__).parents[1] / "scripts" / "compare_uncore_dumps.py"
SPEC = importlib.util.spec_from_file_location("compare_uncore_dumps", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def snapshot(value: int):
    config = bytearray(256)
    struct.pack_into("<I", config, 0x48, value)
    return {
        "schema_version": 1,
        "devices": [
            {
                "bdf": "0000:ff:03.0",
                "vendor_id": "0x8086",
                "device_id": "0x2c18",
                "role": "memory_controller",
                "config_hex": config.hex(),
                "named_registers": [
                    {"offset": "0x48", "name": "MC_CONTROL", "value": hex(value)}
                ],
            }
        ],
    }


class CompareUncoreTests(unittest.TestCase):
    def test_named_dword_change(self):
        result = MODULE.compare_snapshots(snapshot(1), snapshot(5), only_known=True)
        change = result["changed_devices"][0]["dword_changes"][0]
        self.assertEqual(change["offset"], "0x048")
        self.assertEqual(change["width_bytes"], 4)
        self.assertEqual(change["name"], "MC_CONTROL")
        self.assertEqual(change["xor"], "0x00000004")

    def test_capture_failure_transition_is_not_silent(self):
        before = snapshot(1)
        before["devices"][0]["config_bytes_requested"] = 256
        before["devices"][0]["config_bytes_read"] = 256
        after = snapshot(1)
        after["devices"][0].pop("config_hex")
        after["devices"][0]["config_error"] = "PermissionError: denied"

        result = MODULE.compare_snapshots(before, after)

        changed = result["changed_devices"][0]
        self.assertEqual(changed["dword_changes"], [])
        self.assertEqual(
            changed["capture_changes"]["config_error"]["after"],
            "PermissionError: denied",
        )
        self.assertEqual(
            changed["capture_changes"]["config_bytes_read"]["before"], 256
        )

    def test_device_id_change_is_remove_and_add(self):
        before = snapshot(1)
        after = snapshot(1)
        after["devices"][0]["device_id"] = "0x2d98"

        result = MODULE.compare_snapshots(before, after)

        self.assertEqual(result["changed_devices"], [])
        self.assertEqual(result["removed_devices"][0]["device_id"], "0x2c18")
        self.assertEqual(result["added_devices"][0]["device_id"], "0x2d98")


if __name__ == "__main__":
    unittest.main()
