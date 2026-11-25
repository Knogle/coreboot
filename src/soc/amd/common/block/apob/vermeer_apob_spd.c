/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/vermeer_apob_spd.h>
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
	"Vermeer cached-SPD record extent");

static uint32_t le32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
		(uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

bool vermeer_apob_decode_spd(const uint8_t *apob, size_t bytes,
			    struct vermeer_apob_spd_cache *result)
{
	const uint8_t *record = NULL;
	const uint8_t *rows[VERMEER_APOB_MAX_DIMMS];
	unsigned int count = 0;
	unsigned int used = 0;
	size_t offset;

	if (!apob || !result || bytes < APOB_HEADER_BYTES || bytes > APOB_MAX_BYTES ||
	    (uintptr_t)apob > UINTPTR_MAX - bytes ||
	    (uintptr_t)result > UINTPTR_MAX - sizeof(*result))
		return false;
	if (le32(apob) != 0x424f5041 || le32(apob + 4) != 24 || le32(apob + 8) != bytes)
		return false;
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

		if (!row[4])
			continue;
		if (row[4] != 1 || row[0] || row[1] >= 2 || row[2] >= 2 ||
		    count == VERMEER_APOB_MAX_DIMMS)
			return false;
		index = row[1] * 2 + row[2];
		if (used & (1U << index))
			return false;
		used |= 1U << index;
		rows[count++] = row;
	}
	if (!count)
		return false;

	memset(result, 0, sizeof(*result));
	result->count = count;
	for (unsigned int i = 0; i < count; i++) {
		result->dimm[i].channel = rows[i][1];
		result->dimm[i].slot = rows[i][2];
		memcpy(result->dimm[i].spd, rows[i] + SPD_ROW_HEADER_BYTES,
		       VERMEER_APOB_SPD_BYTES);
	}
	return true;
}
