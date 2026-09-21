/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_X58_ACPI_PLATFORM_ACPI_H
#define MAINBOARD_MSI_X58_PRO_E_X58_ACPI_PLATFORM_ACPI_H

#include <acpi/acpi.h>
#include <device/device.h>
unsigned long x58_acpi_platform_write_acpi_tables(const struct device *dev,
	unsigned long current, acpi_rsdp_t *rsdp);

#endif
