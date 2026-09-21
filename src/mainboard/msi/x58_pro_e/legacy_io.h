/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef X58_UNQUALIFIED_LEGACY_H
#define X58_UNQUALIFIED_LEGACY_H

#include <stdbool.h>
#include <stdint.h>

bool x58_platform_legacy_command(int argc, char **argv, bool frozen);
void x58_platform_legacy_status(void);
void x58_platform_legacy_enable_serirq(void);
void x58_platform_legacy_admit_ps2(void);
bool x58_platform_legacy_ps2_present(void);
uint16_t x58_platform_legacy_iost(void);

#endif
