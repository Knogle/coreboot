/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef MAINBOARD_MSI_X58_PRO_E_COLD_COOKIE_POLICY_H
#define MAINBOARD_MSI_X58_PRO_E_COLD_COOKIE_POLICY_H

#include <stdbool.h>
#include <stdint.h>

/* Lab observation, not a definition of CMOS diagnostic bit meanings.
 * Admit only the observed 6c byte with a fresh all-zero I801 signature.
 * This predicate neither writes CMOS nor consumes/clears a phase marker.
 */
static inline bool x58_cold_cookie_compat(bool enabled, uint8_t cookie,
	uint8_t control, uint8_t command, uint8_t address,
	uint8_t data0, uint8_t data1)
{
	return enabled && cookie == 0x6c &&
		!(control | command | address | data0 | data1);
}

#endif
