/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_B06WJ_ACPI_H
#define MAINBOARD_MSI_X58_PRO_E_B06WJ_ACPI_H

#include <acpi/acpi.h>
#include <device/device.h>
#include "development_id.h"

#define B06WK_BUILD_ID "X58PROE-B06WK-ACPI-REPAIR-20260908"
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
#define B06WJ_BUILD_ID X58_DEVELOPMENT_BUILD_ID
#elif CONFIG_X58_PRO_E_B06WK_ACPI_REPAIR
#define B06WJ_BUILD_ID B06WK_BUILD_ID
#else
#define B06WJ_BUILD_ID "X58PROE-B06WJ-ACPI-PLATFORM-20260907"
#endif

unsigned long b06wj_write_acpi_tables(const struct device *dev,
	unsigned long current, acpi_rsdp_t *rsdp);

#endif
