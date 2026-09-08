#!/usr/bin/env python3
"""Capture Nehalem/Westmere Uncore PCI configuration without writes.

The memory-controller device IDs are derived from Linux
drivers/edac/i7core_edac.c; CPU QPI/SAD IDs and the device/function/register
maps come from Intel datasheets 320835 and 321322 plus the Westmere supplement
323370.  The default 256-byte read covers every register named in this tool's
current map.
A 4096-byte extended-config read is available only as an explicit opt-in
because broad PCI configuration reads have historically exposed
firmware/device bugs on some systems.

This tool does not probe unenumerated buses, enable devices, map MMIO, access
MSRs, or write configuration space.  If firmware hides the Uncore functions,
the resulting report records them as absent rather than trying to expose them.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
import struct
from typing import Any


SOURCE = (
    "Linux drivers/edac/i7core_edac.c; Intel Core i7 datasheet 320835; "
    "Intel Xeon 5500 Series Datasheet Volume 2, document 321322; "
    "Intel Xeon 5600 Series Datasheet Volume 2 supplement, document 323370"
)
BDF_RE = re.compile(
    r"^(?P<domain>[0-9a-fA-F]{4}):(?P<bus>[0-9a-fA-F]{2}):"
    r"(?P<slot>[0-9a-fA-F]{2})\.(?P<function>[0-7])$"
)


@dataclass(frozen=True)
class DeviceSpec:
    profile: str
    device_id: int
    role: str
    slot: int
    function: int
    optional: bool = False


def _channel_specs(profile: str, base_ids: tuple[int, int, int]) -> list[DeviceSpec]:
    specs: list[DeviceSpec] = []
    for channel, base in enumerate(base_ids):
        for function, suffix in enumerate(("control", "address", "rank", "timing")):
            specs.append(
                DeviceSpec(
                    profile,
                    base + function,
                    f"channel{channel}_{suffix}",
                    4 + channel,
                    function,
                )
            )
    return specs


BLOOMFIELD_SPECS = [
    DeviceSpec(
        "bloomfield_gainestown", 0x2C01, "system_address_decoder", 0, 1, True
    ),
    DeviceSpec("bloomfield_gainestown", 0x2C10, "qpi_link0", 2, 0, True),
    DeviceSpec("bloomfield_gainestown", 0x2C11, "qpi_phy0", 2, 1, True),
    DeviceSpec("bloomfield_gainestown", 0x2C14, "qpi_link1", 2, 4, True),
    DeviceSpec("bloomfield_gainestown", 0x2C15, "qpi_phy1", 2, 5, True),
    DeviceSpec("bloomfield_gainestown", 0x2C18, "memory_controller", 3, 0),
    DeviceSpec("bloomfield_gainestown", 0x2C19, "target_address_decode", 3, 1),
    DeviceSpec(
        "bloomfield_gainestown", 0x2C1A, "memory_controller_ras", 3, 2, True
    ),
    DeviceSpec("bloomfield_gainestown", 0x2C1C, "memory_controller_test", 3, 4),
    *_channel_specs("bloomfield_gainestown", (0x2C20, 0x2C28, 0x2C30)),
    DeviceSpec("bloomfield_gainestown", 0x2C40, "generic_noncore", 0, 0),
    DeviceSpec("bloomfield_gainestown", 0x2C41, "generic_noncore", 0, 0),
]

WESTMERE_SPECS = [
    DeviceSpec(
        "gulftown_westmere_ep", 0x2D81, "system_address_decoder", 0, 1, True
    ),
    DeviceSpec("gulftown_westmere_ep", 0x2D90, "qpi_link0", 2, 0, True),
    DeviceSpec("gulftown_westmere_ep", 0x2D91, "qpi_phy0", 2, 1, True),
    DeviceSpec("gulftown_westmere_ep", 0x2D92, "qpi_mirror0", 2, 2, True),
    DeviceSpec("gulftown_westmere_ep", 0x2D93, "qpi_mirror1", 2, 3, True),
    DeviceSpec("gulftown_westmere_ep", 0x2D94, "qpi_link1", 2, 4, True),
    DeviceSpec("gulftown_westmere_ep", 0x2D95, "qpi_phy1", 2, 5, True),
    DeviceSpec("gulftown_westmere_ep", 0x2D98, "memory_controller", 3, 0),
    DeviceSpec("gulftown_westmere_ep", 0x2D99, "target_address_decode", 3, 1),
    DeviceSpec(
        "gulftown_westmere_ep", 0x2D9A, "memory_controller_ras", 3, 2, True
    ),
    DeviceSpec("gulftown_westmere_ep", 0x2D9C, "memory_controller_test", 3, 4),
    *_channel_specs("gulftown_westmere_ep", (0x2DA0, 0x2DA8, 0x2DB0)),
    DeviceSpec("gulftown_westmere_ep", 0x2C70, "generic_noncore", 0, 0),
]

SPECS = tuple(BLOOMFIELD_SPECS + WESTMERE_SPECS)
SPECS_BY_ID = {spec.device_id: spec for spec in SPECS}

# These two devices are useful context but are not CPU Uncore functions.
AUXILIARY_IDS = {
    0x3405: ("x58", "x58_ioh_host_bridge"),
    0x342E: ("x58", "x58_hub_management"),
}


REGISTER_MAPS: dict[str, dict[int, str]] = {
    "x58_ioh_host_bridge": {
        0x50: "DMIRCBAR",
    },
    "x58_hub_management": {
        0x88: "GENPROTRANGE0_BASE",
        0x90: "GENPROTRANGE0_LIMIT",
        0x98: "IOHMISCCTRL",
        0x9C: "IOHMISCSS",
        0xA8: "TSEGCTRL",
        0xB0: "GENPROTRANGE1_BASE",
        0xB8: "GENPROTRANGE1_LIMIT",
        0xC0: "GENPROTRANGE2_BASE",
        0xC8: "GENPROTRANGE2_LIMIT",
        0xD0: "TOLM",
        0xD4: "TOHM_LO",
        0xD8: "TOHM_HI",
        0xDC: "NCMEM_BASE_LO",
        0xE0: "NCMEM_BASE_HI",
        0xE4: "NCMEM_LIMIT_LO",
        0xE8: "NCMEM_LIMIT_HI",
        0xF0: "DEVHIDE1",
        0xF8: "DEVHIDE2",
        0x108: "LIO_BASE_LIMIT_IOHBUSNO",
        0x10C: "LMMIOL_BASE_LIMIT",
    },
    "generic_noncore": {
        0x40: "MAXREQUEST_LC",
        0x44: "MAXREQUEST_LS",
        0x48: "MAXREQUEST_LL",
        0x60: "MAX_RTIDS",
        0x80: "DESIRED_CORES",
        0x88: "MEMLOCK_STATUS",
        0x90: "MC_CFG_CONTROL",
        0xB0: "POWER_CNTRL_ERR_STATUS",
        0xC0: "CURRENT_UCLK_RATIO",
        0xD0: "MIRROR_PORT_CTL",
        0xE0: "MIP_PH_CTR_L0",
        0xE4: "MIP_PH_PRT_L0",
        0xF0: "MIP_PH_CTR_L1",
        0xF4: "MIP_PH_PRT_L1",
    },
    "system_address_decoder": {
        0x40: "SAD_PAM0123",
        0x44: "SAD_PAM456",
        0x48: "SAD_HEN",
        0x4C: "SAD_SMRAM",
        0x50: "SAD_PCIEXBAR_LO",
        0x54: "SAD_PCIEXBAR_HI",
        **{0x80 + index * 4: f"SAD_DRAM_RULE_{index}" for index in range(8)},
        **{
            0xC0 + index * 4: f"SAD_INTERLEAVE_LIST_{index}"
            for index in range(8)
        },
    },
    "qpi_link0": {
        0x40: "QPI_QPILCP_L0",
        0x48: "QPI_QPILCL_L0",
        0x50: "QPI_QPILS_L0",
        0x58: "QPI_DEF_RMT_VN_CREDITS_L0",
        **{0xC0 + index * 4: f"QPI_RMT_QPILP{index}_STAT_L0" for index in range(4)},
    },
    "qpi_link1": {
        0x40: "QPI_QPILCP_L1",
        0x48: "QPI_QPILCL_L1",
        0x50: "QPI_QPILS_L1",
        0x58: "QPI_DEF_RMT_VN_CREDITS_L1",
        **{0xC0 + index * 4: f"QPI_RMT_QPILP{index}_STAT_L1" for index in range(4)},
    },
    "qpi_phy0": {
        0x50: "QPI_0_PLL_STATUS",
        0x54: "QPI_0_PLL_RATIO",
        0x68: "QPI_0_PH_CPR",
        0x6C: "QPI_0_PH_CTR",
        0x80: "QPI_0_PH_PIS",
        0x94: "QPI_0_PH_PTV",
        0x9C: "QPI_0_PH_LDC",
        0xA4: "QPI_0_PH_PRT",
        0xD0: "QPI_0_PH_PMR0",
        0xE0: "QPI_0_EP_SR",
        0xF4: "QPI_0_EP_MCTR",
    },
    "qpi_phy1": {
        0x50: "QPI_1_PLL_STATUS",
        0x54: "QPI_1_PLL_RATIO",
        0x68: "QPI_1_PH_CPR",
        0x6C: "QPI_1_PH_CTR",
        0x80: "QPI_1_PH_PIS",
        0x94: "QPI_1_PH_PTV",
        0x9C: "QPI_1_PH_LDC",
        0xA4: "QPI_1_PH_PRT",
        0xD0: "QPI_1_PH_PMR0",
        0xE0: "QPI_1_EP_SR",
        0xF4: "QPI_1_EP_MCTR",
    },
    "memory_controller": {
        0x48: "MC_CONTROL",
        0x4C: "MC_STATUS",
        0x50: "MC_SMI_DIMM_ERROR_STATUS",
        0x54: "MC_SMI_CNTRL",
        0x5C: "MC_RESET_CONTROL",
        0x60: "MC_CHANNEL_MAPPER",
        0x64: "MC_MAX_DOD",
        0x70: "MC_RD_CRDT_INIT",
        0x74: "MC_CRDT_WR_THLD",
        0x78: "MC_SCRUBADDR_LO",
        0x7C: "MC_SCRUBADDR_HI",
    },
    "target_address_decode": {
        **{0x80 + index * 4: f"TAD_DRAM_RULE_{index}" for index in range(8)},
        **{
            0xC0 + index * 4: f"TAD_INTERLEAVE_LIST_{index}"
            for index in range(8)
        },
    },
    "memory_controller_ras": {
        0x48: "MC_SSRCONTROL",
        0x4C: "MC_SCRUB_CONTROL",
        0x50: "MC_RAS_ENABLES",
        0x54: "MC_RAS_STATUS",
        0x60: "MC_SSRSTATUS",
        0x80: "MC_COR_ECC_CNT_0",
        0x84: "MC_COR_ECC_CNT_1",
        0x88: "MC_COR_ECC_CNT_2",
        0x8C: "MC_COR_ECC_CNT_3",
        0x90: "MC_COR_ECC_CNT_4",
        0x94: "MC_COR_ECC_CNT_5",
    },
    "memory_controller_test": {
        0x50: "MC_DIMM_CLK_RATIO_STATUS",
        0x54: "MC_DIMM_CLK_RATIO",
        0x5C: "MC_TEST_OBSERVED_5C",
        0x60: "MC_TEST_ERR_RCV1",
        0x64: "MC_TEST_ERR_RCV0",
        0x6C: "MC_TEST_PH_CTR",
        0x80: "MC_TEST_PH_PIS",
        0xA8: "MC_TEST_PAT_GCTR",
        0xAC: "MC_TEST_OBSERVED_AC",
        0xB0: "MC_TEST_PAT_BA",
        0xBC: "MC_TEST_PAT_IS",
        0xC0: "MC_TEST_PAT_DCD",
        0xC4: "MC_TEST_OBSERVED_C4",
        0xC8: "MC_TEST_OBSERVED_C8",
        0xCC: "MC_TEST_OBSERVED_CC",
        0xF8: "MC_TEST_OBSERVED_F8",
        0xFC: "MC_TEST_OBSERVED_FC",
    },
}

for channel in range(3):
    REGISTER_MAPS[f"channel{channel}_control"] = {
        0x50: "MC_CHANNEL_DIMM_RESET_CMD",
        0x54: "MC_CHANNEL_DIMM_INIT_CMD",
        0x58: "MC_CHANNEL_DIMM_INIT_PARAMS",
        0x5C: "MC_CHANNEL_DIMM_INIT_STATUS",
        0x60: "MC_CHANNEL_DDR3CMD",
        0x68: "MC_CHANNEL_REFRESH_THROTTLE_SUPPORT",
        0x70: "MC_CHANNEL_MRS_VALUE_0_1",
        0x74: "MC_CHANNEL_MRS_VALUE_2",
        0x7C: "MC_CHANNEL_RANK_PRESENT",
        0x80: "MC_CHANNEL_RANK_TIMING_A",
        0x84: "MC_CHANNEL_RANK_TIMING_B",
        0x88: "MC_CHANNEL_BANK_TIMING",
        0x8C: "MC_CHANNEL_REFRESH_TIMING",
        0x90: "MC_CHANNEL_CKE_TIMING",
        0x94: "MC_CHANNEL_ZQ_TIMING",
        0x98: "MC_CHANNEL_RCOMP_PARAMS",
        0x9C: "MC_CHANNEL_ODT_PARAMS1",
        0xA0: "MC_CHANNEL_ODT_PARAMS2",
        0xA4: "MC_CHANNEL_ODT_MATRIX_RANK_0_3_RD",
        0xA8: "MC_CHANNEL_ODT_MATRIX_RANK_4_7_RD",
        0xAC: "MC_CHANNEL_ODT_MATRIX_RANK_0_3_WR",
        0xB0: "MC_CHANNEL_ODT_MATRIX_RANK_4_7_WR",
        0xB4: "MC_CHANNEL_WAQ_PARAMS",
        0xB8: "MC_CHANNEL_SCHEDULER_PARAMS",
        0xBC: "MC_CHANNEL_MAINTENANCE_OPS",
        0xC0: "MC_CHANNEL_TX_BG_SETTINGS",
        0xC8: "MC_CHANNEL_RX_BGF_SETTINGS",
        0xCC: "MC_CHANNEL_EW_BGF_SETTINGS",
        0xD0: "MC_CHANNEL_EW_BGF_OFFSET_SETTINGS",
        0xD4: "MC_CHANNEL_ROUND_TRIP_LATENCY",
        0xD8: "MC_CHANNEL_PAGETABLE_PARAMS1",
        0xE0: "MC_TX_BG_CMD_DATA_RATIO_SETTING",
        0xE4: "MC_TX_BG_CMD_OFFSET_SETTINGS",
        0xE8: "MC_TX_BG_DATA_OFFSET_SETTINGS",
        0xF0: "MC_CHANNEL_ADDR_MATCH",
        0xF8: "MC_CHANNEL_ECC_ERROR_MASK",
        0xFC: "MC_CHANNEL_ECC_ERROR_INJECT",
    }
    REGISTER_MAPS[f"channel{channel}_address"] = {
        0x48: "MC_DOD_CH_DIMM0",
        0x4C: "MC_DOD_CH_DIMM1",
        0x50: "MC_DOD_CH_DIMM2",
        **{0x80 + index * 4: f"MC_SAG_CH_{index}" for index in range(8)},
    }
    REGISTER_MAPS[f"channel{channel}_rank"] = {
        **{0x40 + index * 4: f"MC_RIR_LIMIT_CH_{index}" for index in range(8)},
        **{0x80 + index * 4: f"MC_RIR_WAY_CH_{index}" for index in range(32)},
    }
    REGISTER_MAPS[f"channel{channel}_timing"] = {
        0x48: "MC_THERMAL_CONTROL",
        0x4C: "MC_THERMAL_STATUS",
        0x50: "MC_THERMAL_DEFEATURE",
        0x60: "MC_THERMAL_PARAMS_A",
        0x64: "MC_THERMAL_PARAMS_B",
        0x80: "MC_COOLING_COEF",
        0x84: "MC_CLOSED_LOOP",
        0x88: "MC_THROTTLE_OFFSET",
        0x98: "MC_RANK_VIRTUAL_TEMP",
        0x9C: "MC_DDR_THERM_COMMAND",
        0xA4: "MC_DDR_THERM_STATUS",
    }


def _read_hex_value(path: Path) -> int:
    return int(path.read_text(encoding="ascii").strip(), 16)


def _dwords(config: bytes, register_map: dict[int, str]) -> list[dict[str, Any]]:
    registers = []
    for offset, name in sorted(register_map.items()):
        if offset + 4 <= len(config):
            value = struct.unpack_from("<I", config, offset)[0]
            registers.append(
                {
                    "offset": f"0x{offset:02x}",
                    "width_bytes": 4,
                    "name": name,
                    "value": f"0x{value:08x}",
                    "value_is_all_ones": value == 0xFFFFFFFF,
                }
            )
    return registers


def _spec_for_device(device_id: int) -> tuple[str, str, int | None, int | None, bool]:
    spec = SPECS_BY_ID.get(device_id)
    if spec:
        return spec.profile, spec.role, spec.slot, spec.function, spec.optional
    profile, role = AUXILIARY_IDS[device_id]
    return profile, role, None, None, False


def capture(sysfs_root: Path, config_bytes: int) -> dict[str, Any]:
    devices: list[dict[str, Any]] = []
    profiles_seen: set[tuple[str, str, str]] = set()
    if not sysfs_root.is_dir():
        raise ValueError(f"not a directory: {sysfs_root}")

    for path in sorted(sysfs_root.iterdir(), key=lambda item: item.name):
        match = BDF_RE.fullmatch(path.name)
        if not match:
            continue
        try:
            vendor_id = _read_hex_value(path / "vendor")
            device_id = _read_hex_value(path / "device")
        except (FileNotFoundError, PermissionError, ValueError, OSError):
            continue
        if vendor_id != 0x8086 or (
            device_id not in SPECS_BY_ID and device_id not in AUXILIARY_IDS
        ):
            continue

        profile, role, expected_slot, expected_function, optional = _spec_for_device(
            device_id
        )
        domain = match.group("domain").lower()
        bus = match.group("bus").lower()
        if profile != "x58":
            profiles_seen.add((profile, domain, bus))
        record: dict[str, Any] = {
            "bdf": path.name.lower(),
            "domain": domain,
            "bus": bus,
            "vendor_id": f"0x{vendor_id:04x}",
            "device_id": f"0x{device_id:04x}",
            "profile": profile,
            "role": role,
            "optional": optional,
        }
        if (path / "class").is_file():
            try:
                record["class"] = f"0x{_read_hex_value(path / 'class'):06x}"
            except (PermissionError, ValueError, OSError):
                record["class"] = None
        if expected_slot is not None:
            actual_slot = int(match.group("slot"), 16)
            actual_function = int(match.group("function"), 16)
            record["expected_device_function"] = (
                f"{expected_slot:02x}.{expected_function}"
            )
            record["location_matches_linux_table"] = (
                actual_slot == expected_slot and actual_function == expected_function
            )

        try:
            with (path / "config").open("rb", buffering=0) as stream:
                config = stream.read(config_bytes)
        except (FileNotFoundError, PermissionError, OSError) as error:
            record["config_error"] = f"{type(error).__name__}: {error}"
            devices.append(record)
            continue
        record.update(
            {
                "config_bytes_requested": config_bytes,
                "config_bytes_read": len(config),
                "config_sha256": hashlib.sha256(config).hexdigest(),
                "config_hex": config.hex(),
                "named_registers": _dwords(config, REGISTER_MAPS.get(role, {})),
            }
        )
        if len(config) >= 4:
            header_vendor, header_device = struct.unpack_from("<HH", config)
            record["header_matches_sysfs_ids"] = (
                header_vendor == vendor_id and header_device == device_id
            )
        devices.append(record)

    profile_status = []
    for profile, domain, bus in sorted(profiles_seen):
        expected_roles = {
            spec.role for spec in SPECS if spec.profile == profile and not spec.optional
        }
        present_roles = {
            device["role"]
            for device in devices
            if device["profile"] == profile
            and device["domain"] == domain
            and device["bus"] == bus
        }
        profile_status.append(
            {
                "profile": profile,
                "pci_segment_bus": f"{domain}:{bus}",
                "present_roles": sorted(present_roles),
                "missing_required_roles": sorted(expected_roles - present_roles),
            }
        )

    return {
        "schema_version": 1,
        "source": SOURCE,
        "access_model": "sysfs PCI configuration reads only; no hardware writes",
        "evidence_model": {
            "device_ids_and_published_register_names": "documented-upstream",
            "MC_TEST_OBSERVED_*_offsets": (
                "vendor-firmware-observed; no public field meaning assigned"
            ),
            "captured_values": "vendor-state-observation",
            "derived_meaning": "inference unless separately documented",
            "phy_and_training_semantics": "unknown unless separately established",
        },
        "config_bytes_requested": config_bytes,
        "sysfs_root": str(sysfs_root),
        "profiles": profile_status,
        "devices": devices,
        "limitations": [
            "Only PCI functions already enumerated by the running kernel are visible.",
            "The default 256-byte read does not include PCIe extended configuration.",
            "Named fields are an observation aid, not a cold-boot initialization sequence.",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--sysfs-root",
        type=Path,
        default=Path("/sys/bus/pci/devices"),
        help="PCI sysfs device directory (the alternate form is mainly for tests)",
    )
    parser.add_argument(
        "--config-bytes",
        type=int,
        choices=(256, 4096),
        default=256,
        help="bytes per recognized function; 4096 is an explicit extended-read opt-in",
    )
    parser.add_argument(
        "--require-target",
        action="store_true",
        help="return failure if no Bloomfield/Gulftown Uncore profile is detected",
    )
    args = parser.parse_args()
    try:
        report = capture(args.sysfs_root, args.config_bytes)
    except ValueError as error:
        parser.error(str(error))
    print(json.dumps(report, indent=2, sort_keys=True))
    if args.require_target and not report["profiles"]:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
