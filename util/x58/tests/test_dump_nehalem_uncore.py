import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).parents[1] / "scripts" / "dump_nehalem_uncore.py"
SPEC = importlib.util.spec_from_file_location("dump_nehalem_uncore", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def add_device(root: Path, bdf: str, vendor: int, device: int, values=None):
    directory = root / bdf
    directory.mkdir()
    (directory / "vendor").write_text(f"0x{vendor:04x}\n", encoding="ascii")
    (directory / "device").write_text(f"0x{device:04x}\n", encoding="ascii")
    (directory / "class").write_text("0x088000\n", encoding="ascii")
    config = bytearray(256)
    struct.pack_into("<HH", config, 0, vendor, device)
    for offset, value in (values or {}).items():
        struct.pack_into("<I", config, offset, value)
    (directory / "config").write_bytes(config)


class UncoreDumpTests(unittest.TestCase):
    def test_recognizes_target_and_named_registers(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            add_device(root, "0000:ff:03.0", 0x8086, 0x2C18, {0x48: 0x12345678})
            add_device(root, "0000:ff:04.0", 0x8086, 0x2C20, {0x58: 0xAABBCCDD})
            add_device(root, "0000:00:01.0", 0x1234, 0x5678)

            report = MODULE.capture(root, 256)

        self.assertEqual(len(report["devices"]), 2)
        self.assertEqual(report["profiles"][0]["profile"], "bloomfield_gainestown")
        self.assertEqual(report["profiles"][0]["pci_segment_bus"], "0000:ff")
        controller = report["devices"][0]
        self.assertTrue(controller["location_matches_linux_table"])
        self.assertEqual(controller["expected_device_function"], "03.0")
        registers = {item["name"]: item["value"] for item in controller["named_registers"]}
        self.assertEqual(registers["MC_CONTROL"], "0x12345678")
        self.assertTrue(controller["header_matches_sysfs_ids"])

    def test_x58_context_does_not_claim_cpu_profile(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            add_device(root, "0000:00:00.0", 0x8086, 0x3405, {0x50: 0xFED1_0001})
            add_device(root, "0000:00:14.0", 0x8086, 0x342E, {0xF0: 0x3FFF_EF74})
            report = MODULE.capture(root, 256)
        self.assertEqual(report["profiles"], [])
        self.assertEqual(report["devices"][0]["role"], "x58_ioh_host_bridge")
        host_registers = {
            item["name"]: item for item in report["devices"][0]["named_registers"]
        }
        self.assertEqual(host_registers["DMIRCBAR"]["value"], "0xfed10001")
        hub = report["devices"][1]
        registers = {item["name"]: item for item in hub["named_registers"]}
        self.assertEqual(registers["DEVHIDE1"]["value"], "0x3fffef74")

    def test_qpi_and_vendor_observed_test_offsets_are_named(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            add_device(root, "0000:ff:02.0", 0x8086, 0x2C10, {0x50: 0x60000000})
            add_device(root, "0000:ff:03.4", 0x8086, 0x2C1C, {0xF8: 0x40000000})
            report = MODULE.capture(root, 256)

        by_role = {device["role"]: device for device in report["devices"]}
        qpi = {item["name"]: item for item in by_role["qpi_link0"]["named_registers"]}
        test = {
            item["name"]: item
            for item in by_role["memory_controller_test"]["named_registers"]
        }
        self.assertEqual(qpi["QPI_QPILS_L0"]["value"], "0x60000000")
        self.assertEqual(test["MC_TEST_OBSERVED_F8"]["value"], "0x40000000")

    def test_recognizes_westmere_sad_qpi_and_mirror_functions(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            add_device(root, "0000:ff:00.1", 0x8086, 0x2D81, {0x50: 0xE0000001})
            add_device(root, "0000:ff:02.0", 0x8086, 0x2D90, {0x50: 0x60000000})
            add_device(root, "0000:ff:02.2", 0x8086, 0x2D92, {0x40: 0x12345678})
            report = MODULE.capture(root, 256)

        by_role = {device["role"]: device for device in report["devices"]}
        self.assertEqual(by_role["system_address_decoder"]["device_id"], "0x2d81")
        self.assertEqual(by_role["qpi_link0"]["device_id"], "0x2d90")
        self.assertEqual(by_role["qpi_mirror0"]["named_registers"], [])


if __name__ == "__main__":
    unittest.main()
