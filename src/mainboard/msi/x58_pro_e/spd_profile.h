/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_SPD_PROFILE_H
#define MAINBOARD_MSI_X58_PRO_E_SPD_PROFILE_H

#include <stddef.h>
#include <stdint.h>

#define X58_SPD_PROFILE_SIZE		256u
#define X58_SPD_PROFILE_SERIAL_OFFSET	122u
#define X58_SPD_PROFILE_SERIAL_SIZE	4u
/* HW07/HW12 BLS4G3D1609DS1S00., with only serial bytes hashed as zero. */
#define X58_SPD_PROFILE_FNV		0x5194e521u

/*
 * DDR3 module serial occupies bytes 122..125 (SPD_DDR3_SERIAL_NUM/LEN).
 * HW07 and the known-good replacement read in HW12 differ only at byte125.
 * Keep all 252 other bytes, including CRC/manufacturer/date/part number/XMP,
 * in the compatibility fingerprint. This does not replace read/CRC/profile
 * validation, change the SPD buffer, or write the EEPROM. The real full-SPD
 * digest remains separate telemetry and the manual confirmation token.
 */
static inline uint32_t x58_spd_profile_digest(const uint8_t *spd, size_t size)
{
	uint32_t digest = 2166136261u;

	if (spd == NULL || size != X58_SPD_PROFILE_SIZE)
		return 0;

	for (size_t i = 0; i < size; i++) {
		const uint8_t value =
			(i >= X58_SPD_PROFILE_SERIAL_OFFSET &&
			 i < X58_SPD_PROFILE_SERIAL_OFFSET + X58_SPD_PROFILE_SERIAL_SIZE) ?
			0 : spd[i];

		digest = (digest ^ value) * 16777619u;
	}

	return digest;
}

#endif
