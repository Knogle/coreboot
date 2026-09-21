/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef X58_UNQUALIFIED_PLATFORM_H
#define X58_UNQUALIFIED_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include <device/device.h>
#include <acpi/acpi.h>

#define X58_PLATFORM_EBDA_BASE 0x0009fc00u
#define X58_PLATFORM_EBDA_SIZE 0x400u
#define X58_PLATFORM_EXTRA_RESOURCES 2

bool x58_platform_command(int argc, char **argv);
void x58_platform_freeze_profile(void);
void x58_platform_status(void);
bool x58_platform_smp_requested(void);
/* Boot-local policy, not a claim that the CPU-side write has executed. */
bool x58_platform_msr2e2_requested(void);
uint16_t x58_platform_pmbase(void);
uint16_t x58_platform_gpiobase(void);
void x58_platform_reserve(struct device *dev);
void x58_platform_verify_reservations(struct device *dev);
/* True only after the complete resource contract has been verified. */
bool x58_platform_ebda_reserved(void);
void x58_platform_fill_ssdt(const struct device *dev);
unsigned long x58_platform_write_tables(unsigned long current, acpi_rsdp_t *rsdp);

#endif
