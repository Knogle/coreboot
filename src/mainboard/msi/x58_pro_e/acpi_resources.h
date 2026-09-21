/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef X58_UNQUALIFIED_RESOURCES_H
#define X58_UNQUALIFIED_RESOURCES_H

#include <stdbool.h>

/* Segment zero: the CPU uncore is not behind the IOH/ICH10 PCI root. */
#define X58_PLATFORM_PCI_BUS_END 0xfe
#define X58_PLATFORM_UNCORE_BUS 0xff

void x58_platform_resource_ssdt(void);

#endif
