/* SPDX-License-Identifier: GPL-2.0-only */

#include <commonlib/endian.h>
#include <string.h>
#include "powerplay_contract.h"

bool b565d4_vermeer_powerplay_build(const uint32_t *apic_ids, unsigned int count,
				  uint8_t out[B565D4_POWERPLAY_BYTES])
{
	uint8_t table[B565D4_POWERPLAY_BYTES] = {0};

	if (!apic_ids || !out || !count || count > B565D4_POWERPLAY_MAX_CPUS)
		return false;
	for (unsigned int i = 0; i < count; i++) {
		if (apic_ids[i] >= UINT16_MAX)
			return false;
		for (unsigned int j = 0; j < i; j++)
			if (apic_ids[i] == apic_ids[j])
				return false;
	}
	/* Original SmuV11DxeVMR 38BCh with the board's compiled scalar PCD
	 * defaults. Zero retains stock automatic PPT/TDC/EDC, voltage and clock
	 * overrides. The nonzero calibration fields are not generic AM4 values.
	 */
	write_le32(table + 0x14, 1000); /* TDP_PlatformLimit, PCD180h */
	write_le32(table + 0x54, 140); /* CPU full-scale current, PCD0D7h */
	write_le32(table + 0x5c, 75); /* SoC full-scale current, PCD0D8h */
	write_le16(table + 0x6d, 0x0101); /* Original unconditional store3A76h */
	write_le16(table + 0x7c, count);
	memset(table + B565D4_POWERPLAY_APIC_OFFSET, 0xff,
	       2 * B565D4_POWERPLAY_APIC_SLOTS);
	for (unsigned int i = 0; i < count; i++) {
		unsigned int rank = 0;

		for (unsigned int j = 0; j < count; j++)
			rank += apic_ids[j] < apic_ids[i];
		write_le16(table + B565D4_POWERPLAY_APIC_OFFSET + 2 * rank, apic_ids[i]);
	}
	memcpy(out, table, sizeof(table));
	return true;
}
