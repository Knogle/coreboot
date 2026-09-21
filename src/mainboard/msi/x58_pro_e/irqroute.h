/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_X58_IRQ_ROUTE_IRQ_ROUTE_H
#define MAINBOARD_MSI_X58_PRO_E_X58_IRQ_ROUTE_IRQ_ROUTE_H

#include <stdbool.h>

#define X58_IRQ_ROUTE_STAGE_ID "X58_IRQ_ROUTE-IRQ-NATIVE_ACPI"

void x58_irq_route_program_vendor_irq_once(void);
bool x58_irq_route_vendor_irq_ready(void);

#endif
