/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef B565D4_SETUP_OPTIONS_H
#define B565D4_SETUP_OPTIONS_H

#include <types.h>

/* CMOS bytes 96..124 only. RTC and AMD's 0x7d..0x7f stay untouched. */
#if CONFIG(B565D4_DESKTOP_SETUP)
#define B565D4_SETUP_SCHEMA 6
#define B565D4_SETUP_PLATFORM_OPTION(X) \
	X(CORE_C6, "cpu_core_c6", 120, 1, 1)
#else
#define B565D4_SETUP_SCHEMA 5
#define B565D4_SETUP_PLATFORM_OPTION(X) \
	X(WATCHDOG, "watchdog", 120, 0, 2)
#endif
#define B565D4_SETUP_BEGIN 96
#define B565D4_SETUP_END 122
#define B565D4_SETUP_CHECKSUM 123

/* id, CMOS name, byte, default, maximum. Keep the original lane byte. */
#define B565D4_SETUP_OPTIONS(X) \
	X(BIFURCATION, "pcie_slot_bifurcation", 96, 0, 3) \
	X(SCHEMA, "setup_schema", 97, B565D4_SETUP_SCHEMA, B565D4_SETUP_SCHEMA) \
	X(POWER_FAILURE, "power_failure", 98, 1, 2) \
	X(SVM, "svm", 99, 1, 1) \
	X(IOMMU, "iommu", 100, 1, 1) \
	X(ACS, "pcie_acs", 101, 1, 1) \
	X(CPU_SPEED, "pcie_cpu_speed", 102, 0, 3) \
	X(PCIE3_SPEED, "pcie3_speed", 103, 0, 3) \
	X(PCIE4_SPEED, "pcie4_speed", 104, 0, 3) \
	X(PCIE5_SPEED, "pcie5_speed", 105, 0, 3) \
	X(CPU_ASPM, "pcie_cpu_aspm", 106, 0, 2) \
	X(PCIE3_ASPM, "pcie3_aspm", 107, 0, 2) \
	X(PCIE4_ASPM, "pcie4_aspm", 108, 0, 2) \
	X(PCIE5_ASPM, "pcie5_aspm", 109, 0, 2) \
	X(ABOVE_4G, "above_4g", 110, 0, 1) \
	X(ARI, "pcie_ari", 111, 1, 1) \
	X(SERIAL, "serial_console", 112, 1, 1) \
	X(LOGLEVEL, "debug_level", 113, 5, 8) \
	X(PXE, "pxe", 114, 1, 1) \
	X(SAS_ROM, "sas_option_rom", 115, 0, 1) \
	X(BOOT_POLICY, "boot_policy", 116, 0, 2) \
	X(BOOT_TIMEOUT, "boot_timeout", 117, 5, 60) \
	X(WOL, "wake_on_lan", 118, 1, 1) \
	X(RTC_WAKE, "rtc_wake", 119, 1, 1) \
	B565D4_SETUP_PLATFORM_OPTION(X) \
	X(DEFAULTS, "restore_defaults", 121, 0, 1) \
	X(PSTATE_LIMIT, "cpu_performance_limit", 122, 0, 2)

enum b565d4_setup_option {
#define OPTION_ENUM(id, name, byte, def, max) B565D4_OPT_##id,
	B565D4_SETUP_OPTIONS(OPTION_ENUM)
#undef OPTION_ENUM
	B565D4_OPT_COUNT,
};

/* Capture once on the BSP before PCI enumeration/AP initialization. */
void b565d4_setup_init(void);
unsigned int b565d4_setup_get(enum b565d4_setup_option option);
bool b565d4_setup_ready(void);
bool b565d4_setup_core_c6_enabled(void);
void b565d4_setup_power_failure(void);
void b565d4_setup_wake(void);
void b565d4_setup_watchdog(void);
bool b565d4_setup_cpu_power_ready(void);
struct device;
void b565d4_setup_acpi_wake(const struct device *domain);
void b565d4_setup_cpu(void);
void b565d4_setup_iommu(void);
void b565d4_setup_high_mmio(void);
bool b565d4_setup_high_mmio_ready(void);

#endif
