/* SPDX-License-Identifier: BSD-2-Clause-Patent */
/* Complete-image, board-specific policy. No SPI or FMP operations live here. */
#include "FchCapsulePolicy.h"

struct extent { capsule_u32 begin, end; };

/* Order is stable for the audit and matches the two 16-MiB banks. */
static const struct extent windows[6] = {
	{ 0x00a5d508, 0x00d5f4d8 },
	{ 0x00d60000, 0x00de1000 },
	{ 0x00e01fd0, 0x00ebc888 },
	{ 0x01a5d508, 0x01d5f4d8 },
	{ 0x01d60000, 0x01de1000 },
	{ 0x01e01fd0, 0x01ebc888 },
};

/* These are preserved verbatim from the running flash, never copied from a
 * capsule: lower/upper EFI NVAR, PSP NVRAM and PSP RPMC NVRAM.
 */
static const struct extent stores[4] = {
	{ 0x00037000, 0x00057000 },
	{ 0x01037000, 0x01057000 },
	{ 0x01088000, 0x010a8000 },
	{ 0x010a8000, 0x010e8000 },
};

static capsule_u32 le32(const capsule_u8 *p)
{
	return (capsule_u32)p[0] | (capsule_u32)p[1] << 8 |
		(capsule_u32)p[2] << 16 | (capsule_u32)p[3] << 24;
}

static int padded_name(const capsule_u8 *p, capsule_size bytes, const char *name)
{
	capsule_size i = 0;

	while (name[i] && i < bytes) {
		if (p[i] != (capsule_u8)name[i])
			return 0;
		i++;
	}
	if (name[i])
		return 0;
	for (; i < bytes; i++)
		if (p[i] != 0)
			return 0;
	return 1;
}

static int fmap_area(const capsule_u8 *p, const char *name,
	capsule_u32 offset, capsule_u32 size)
{
	return le32(p) == offset && le32(p + 4) == size &&
		padded_name(p + 8, 32, name) && p[40] == 0 && p[41] == 0;
}

static int valid_fmap(const capsule_u8 *image)
{
	for (capsule_u32 bank = 0; bank < B565D4_CAPSULE_BYTES;
	     bank += B565D4_CAPSULE_BANK_BYTES) {
		const capsule_u8 *p = image + bank;
		const capsule_u8 magic[8] = {'_', '_', 'F', 'M', 'A', 'P', '_', '_'};

		for (capsule_size i = 0; i < 8; i++)
			if (p[i] != magic[i])
				return 0;
		if (p[8] != 1 || p[9] != 1 || le32(p + 10) != 0 ||
		    le32(p + 14) != 0 || le32(p + 18) != B565D4_CAPSULE_BYTES ||
		    !padded_name(p + 22, 32, "B565D4_FLASH") ||
		    p[54] != 4 || p[55] != 0)
			return 0;
		p += 56;
		if (!fmap_area(p, "BIOS", 0, B565D4_CAPSULE_BYTES) ||
		    !fmap_area(p + 42, "FMAP", 0, 0x1000) ||
		    !fmap_area(p + 84, "COREBOOT", 0x01d62000, 0x0007f000) ||
		    !fmap_area(p + 126, "PAYLOAD", 0x01a5e000, 0x003014d8))
			return 0;
	}
	return 1;
}

static int extent_index(capsule_u32 offset, const struct extent *list,
	capsule_size count)
{
	for (capsule_size i = 0; i < count; i++)
		if (offset >= list[i].begin && offset < list[i].end)
			return (int)i;
	return -1;
}

enum b565d4_capsule_result b565d4_capsule_validate(
	const capsule_u8 *current, const capsule_u8 *candidate,
	capsule_size bytes, struct b565d4_capsule_audit *audit)
{
	if (!current || !candidate || !audit || bytes != B565D4_CAPSULE_BYTES)
		return B565D4_CAPSULE_BAD_INPUT;
	audit->first_bad_offset = B565D4_CAPSULE_BYTES;
	for (capsule_size i = 0; i < 6; i++)
		audit->changed_bytes[i] = 0;
	if (!valid_fmap(current) || !valid_fmap(candidate))
		return B565D4_CAPSULE_BAD_FMAP;
	for (capsule_size w = 0; w < 3; w++) {
		for (capsule_u32 at = windows[w].begin; at < windows[w].end; at++) {
			capsule_u32 high = at + B565D4_CAPSULE_BANK_BYTES;

			if (current[at] != current[high] ||
			    candidate[at] != candidate[high]) {
				audit->first_bad_offset = at;
				return B565D4_CAPSULE_BAD_MIRROR;
			}
		}
	}
	for (capsule_u32 at = 0; at < B565D4_CAPSULE_BYTES; at++) {
		int window = extent_index(at, windows, 6);

		if (window >= 0) {
			audit->changed_bytes[window] += current[at] != candidate[at];
			continue;
		}
		if (extent_index(at, stores, 4) < 0 &&
		    current[at] != candidate[at]) {
			audit->first_bad_offset = at;
			return B565D4_CAPSULE_IMMUTABLE_CHANGE;
		}
	}
	return B565D4_CAPSULE_OK;
}

int b565d4_capsule_merge_sector(const capsule_u8 *current,
	const capsule_u8 *candidate, capsule_size bytes, capsule_u32 sector,
	capsule_u8 *merged)
{
	int changed = 0;

	if (!current || !candidate || !merged || bytes != B565D4_CAPSULE_BYTES ||
	    (sector & (B565D4_CAPSULE_SECTOR_BYTES - 1)) ||
	    sector > B565D4_CAPSULE_BYTES - B565D4_CAPSULE_SECTOR_BYTES)
		return -1;
	for (capsule_u32 i = 0; i < B565D4_CAPSULE_SECTOR_BYTES; i++) {
		capsule_u32 at = sector + i;
		capsule_u8 next = extent_index(at, windows, 6) >= 0 ?
			candidate[at] : current[at];

		merged[i] = next;
		changed |= next != current[at];
	}
	return changed;
}

int b565d4_capsule_sector_delta_allowed(capsule_u32 sector,
	const capsule_u8 *before, const capsule_u8 *after)
{
	if (!before || !after ||
	    (sector & (B565D4_CAPSULE_SECTOR_BYTES - 1)) ||
	    sector > B565D4_CAPSULE_BYTES - B565D4_CAPSULE_SECTOR_BYTES)
		return 0;
	for (capsule_u32 i = 0; i < B565D4_CAPSULE_SECTOR_BYTES; i++)
		if (before[i] != after[i] &&
		    extent_index(sector + i, windows, 6) < 0)
			return 0;
	return 1;
}
