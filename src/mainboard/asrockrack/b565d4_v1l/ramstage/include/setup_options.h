/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef B565D4_SETUP_OPTIONS_H
#define B565D4_SETUP_OPTIONS_H

#include <types.h>

/* CMOS bytes 96..124 only. RTC and AMD's 0x7d..0x7f stay untouched. */
#if CONFIG(B565D4_DESKTOP_SETUP)
#define B565D4_SETUP_SCHEMA 6
#else
#define B565D4_SETUP_SCHEMA 5
#endif
#define B565D4_SETUP_BEGIN 96
#define B565D4_SETUP_END 122
#define B565D4_SETUP_CHECKSUM 123

enum b565d4_setup_option {
#define B565D4_SETUP_OPTION(id, name, byte, def, max) B565D4_OPT_##id,
#include "setup_options.inc"
#undef B565D4_SETUP_OPTION
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
