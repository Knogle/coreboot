/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_B06WI_IRQ_ROUTE_H
#define MAINBOARD_MSI_X58_PRO_E_B06WI_IRQ_ROUTE_H

#include <stdbool.h>

#define B06WI_BUILD_ID "X58PROE-B06WI-VENDOR-IRQ-ACPI-20260907"
#define B06WI_STAGE_ID "B06WI-IRQ-ACPI1"

void b06wi_program_vendor_irq_once(void);
bool b06wi_vendor_irq_ready(void);

#endif
