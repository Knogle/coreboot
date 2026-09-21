/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_SPD_TOPOLOGY_H
#define MAINBOARD_MSI_X58_PRO_E_SPD_TOPOLOGY_H

#include <stdbool.h>
#include <stdint.h>

/* Scan bitmap bit0 corresponds to SMBus address0x50, bit4 to target SPD0x54. */
#define X58_SPD_TARGET_MAP 0x10u
#define X58_SPD_EXTRA_50_MAP 0x01u

/*
 * A narrow admission exception, not identification of the extra device.
 * Preserve the raw scan, target profile checks, transport failure handling and
 * the prohibition on additional DDR3 responders. Never synthesize scan data.
 */
static inline bool x58_spd_topology_supported(uint8_t status, uint8_t ackmap,
		uint8_t ddr3map, bool allow_extra_50)
{
	if (status || ddr3map != X58_SPD_TARGET_MAP)
		return false;

	return ackmap == X58_SPD_TARGET_MAP ||
		(allow_extra_50 &&
		 ackmap == (X58_SPD_TARGET_MAP | X58_SPD_EXTRA_50_MAP));
}

#endif
