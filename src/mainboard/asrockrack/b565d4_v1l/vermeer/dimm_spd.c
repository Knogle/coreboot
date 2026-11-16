/* SPDX-License-Identifier: GPL-2.0-only */

#include "dimm_spd.h"
#include <device/dram/common.h>
#include <limits.h>
#include <string.h>

#define APOB_HEADER_BYTES 0xf8
#define APOB_MAX_BYTES 0x28000
#define RECORD_HEADER_BYTES 48
#define SPD_RECORD_BYTES 0x2174
#define SPD_ROW_BYTES 532
#define SPD_ROW_HEADER_BYTES 20
#define SPD_ROWS 16

_Static_assert(SPD_RECORD_BYTES == RECORD_HEADER_BYTES + 4 + SPD_ROWS * SPD_ROW_BYTES,
	"Qualified Vermeer cached-SPD record extent");

static uint32_t le32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
		(uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static bool normalize_spd(const uint8_t *raw, uint8_t *spd)
{
	/* Actual supplier cache and independent EEPROM captures agree on these
	 * duplicated manufacturing fields. Do not guess other overwritten data.
	 */
	if (raw[2] != 0x0c || memcmp(raw + 80, raw + 325, 4) ||
	    memcmp(raw + 84, raw + 329, 20))
		return false;
	memcpy(spd, raw, B565D4_SPD_BYTES);
	memset(spd + 80, 0, 24);
	return ddr_crc16(spd, 126) == ((uint16_t)spd[126] | spd[127] << 8) &&
		ddr_crc16(spd + 128, 126) == ((uint16_t)spd[254] | spd[255] << 8);
}

bool b565d4_decode_dimm_spd(const uint8_t *apob, size_t bytes,
	struct b565d4_spd_cache *result)
{
	const uint8_t *record = NULL, *rows[B565D4_DIMM_COUNT];
	uint8_t spd[B565D4_SPD_BYTES];
	unsigned int count = 0, used = 0;
	size_t offset;

	if (!apob || !result || bytes < APOB_HEADER_BYTES || bytes > APOB_MAX_BYTES ||
	    (uintptr_t)apob > UINTPTR_MAX - bytes ||
	    (uintptr_t)result > UINTPTR_MAX - sizeof(*result))
		return false;
	if (le32(apob) != 0x424f5041 || le32(apob + 4) != 24 ||
	    le32(apob + 8) != bytes)
		return false;
	/* The caller already owns its result. Reject overlap with the read-only
	 * source so that a successful inventory cannot mutate the supplier copy.
	 */
	if ((uintptr_t)result < (uintptr_t)apob + bytes &&
	    (uintptr_t)apob < (uintptr_t)result + sizeof(*result))
		return false;
	offset = le32(apob + 12);
	if (offset < APOB_HEADER_BYTES || (offset & 3))
		return false;
	while (offset < bytes) {
		uint32_t length;

		if (bytes - offset < RECORD_HEADER_BYTES)
			return false;
		length = le32(apob + offset + 12);
		if (length < RECORD_HEADER_BYTES || length > bytes - offset)
			return false;
		if (le32(apob + offset) == 1 && le32(apob + offset + 4) == 17 &&
		    le32(apob + offset + 8) == 0) {
			if (record || length != SPD_RECORD_BYTES)
				return false;
			record = apob + offset;
		}
		offset += length;
	}
	if (offset != bytes || !record || le32(record + RECORD_HEADER_BYTES) != 0x202)
		return false;
	for (unsigned int i = 0; i < SPD_ROWS; i++) {
		const uint8_t *row = record + RECORD_HEADER_BYTES + 4 + i * SPD_ROW_BYTES;
		unsigned int index;

		/* Absent rows contain supplier sentinels, not valid socket IDs. */
		if (!row[4])
			continue;
		if (row[4] != 1 || row[0] || row[1] >= 2 || row[2] >= 2 ||
		    count == B565D4_DIMM_COUNT)
			return false;
		index = row[1] * 2 + row[2];
		if ((used & (1U << index)) ||
		    !normalize_spd(row + SPD_ROW_HEADER_BYTES, spd))
			return false;
		used |= 1U << index;
		rows[count++] = row;
	}
	if (!count)
		return false;
	/* All records and active rows passed. Commit only to the caller's copy. */
	memset(result, 0, sizeof(*result));
	result->count = count;
	for (unsigned int i = 0; i < count; i++) {
		result->dimm[i].channel = rows[i][1];
		result->dimm[i].slot = rows[i][2];
		memcpy(result->dimm[i].spd, rows[i] + SPD_ROW_HEADER_BYTES, B565D4_SPD_BYTES);
		memset(result->dimm[i].spd + 80, 0, 24);
	}
	return true;
}
