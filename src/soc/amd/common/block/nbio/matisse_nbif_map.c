/* SPDX-License-Identifier: GPL-2.0-only */

#include <limits.h>
#include <amdblocks/matisse_nbif_map.h>

static const uint8_t devfn_pool[MATISSE_NBIF_MAP_SLOTS] = {
	9, 10, 11, 12, 13, 14, 15, 17, 25, 26, 27, 28, 29, 30, 31, 33, 41, 42,
};

int matisse_nbif_devfn_index(uint8_t devfn)
{
	for (size_t i = 0; i < MATISSE_NBIF_MAP_SLOTS; i++)
		if (devfn_pool[i] == devfn)
			return (int)i;
	return -1;
}

static bool valid_range(const void *pointer, size_t bytes)
{
	return pointer && (uintptr_t)pointer <= UINTPTR_MAX - bytes;
}

static bool overlap(const void *first, size_t first_bytes,
		    const void *second, size_t second_bytes)
{
	return (uintptr_t)first < (uintptr_t)second + second_bytes &&
		(uintptr_t)second < (uintptr_t)first + first_bytes;
}

bool matisse_build_nbif_map(uint8_t root_index,
			   const uint8_t assigned[MATISSE_NBIF_MAP_SLOTS],
			   const uint32_t before[MATISSE_NBIF_MAP_SLOTS],
			   struct matisse_nbif_map_update output[MATISSE_NBIF_MAP_SLOTS])
{
	const size_t assigned_bytes = MATISSE_NBIF_MAP_SLOTS * sizeof(*assigned);
	const size_t before_bytes = MATISSE_NBIF_MAP_SLOTS * sizeof(*before);
	const size_t output_bytes = MATISSE_NBIF_MAP_SLOTS * sizeof(*output);
	uint32_t used = 0;
	size_t next = 0;

	if (!valid_range(assigned, assigned_bytes) || !valid_range(before, before_bytes) ||
	    !valid_range(output, output_bytes) ||
	    overlap(assigned, assigned_bytes, output, output_bytes) ||
	    overlap(before, before_bytes, output, output_bytes))
		return false;
	for (size_t i = 0; i < MATISSE_NBIF_MAP_SLOTS; i++) {
		size_t index;

		if (!assigned[i])
			continue;
		for (index = 0; index < MATISSE_NBIF_MAP_SLOTS; index++)
			if (devfn_pool[index] == assigned[i])
				break;
		if (index == MATISSE_NBIF_MAP_SLOTS || (used & (1U << index)))
			return false;
		used |= 1U << index;
	}
	for (size_t i = 0; i < MATISSE_NBIF_MAP_SLOTS; i++) {
		uint8_t devfn = assigned[i];

		if (!devfn) {
			while (used & (1U << next))
				next++;
			devfn = devfn_pool[next];
			used |= 1U << next;
		}
		output[i].address = 0x13b100b8U + ((uint32_t)root_index << 20) +
			(uint32_t)i * 4U;
		output[i].value = (before[i] & 0xffffff00U) | devfn;
	}
	return true;
}
