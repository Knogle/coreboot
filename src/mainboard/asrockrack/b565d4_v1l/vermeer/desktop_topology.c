/* SPDX-License-Identifier: GPL-2.0-only */
#include "desktop_topology.h"

#define APOB_CCD_OFFSET 48
#define APOB_CCD_BYTES 51
#define APOB_CCD_SLOTS AM4_DESKTOP_PHYSICAL_CCD_SLOTS
#define APOB_CCX_BYTES 25
#define APOB_CCX_SLOTS 2
#define APOB_CORE_SLOTS 8

static uint32_t le32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
		(uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

bool am4_desktop_decode_topology(enum b565d4_cpu_family family,
				 const uint8_t *record, size_t bytes,
				 unsigned int logical_threads,
				 struct am4_desktop_topology *topology)
{
	struct am4_desktop_topology result = { 0 };
	unsigned int cores_per_ccx, complexes_per_ccd;

	switch (family) {
	case B565D4_CPU_MATISSE:
		cores_per_ccx = 4;
		complexes_per_ccd = 2;
		break;
	case B565D4_CPU_VERMEER:
		cores_per_ccx = 8;
		complexes_per_ccd = 1;
		break;
	default:
		return false;
	}

	if (!record || !topology || bytes != AM4_DESKTOP_TOPOLOGY_BYTES ||
	    !logical_threads || logical_threads > AM4_DESKTOP_MAX_THREADS)
		return false;
	if (le32(record) != 3 || le32(record + 4) != 3 ||
	    le32(record + 8) != 0 || le32(record + 12) != bytes)
		return false;

	/* GetCoreTopology uses the first core's enabled thread flags globally. */
	if (record[APOB_CCD_OFFSET] == 0xff || record[APOB_CCD_OFFSET + 1] == 0xff ||
	    record[APOB_CCD_OFFSET + 2] == 0xff || record[APOB_CCD_OFFSET + 3] != 1 ||
	    record[APOB_CCD_OFFSET + 4] > 1)
		return false;
	result.threads_per_core = 1 + record[APOB_CCD_OFFSET + 4];
	for (unsigned int ccd = 0; ccd < APOB_CCD_SLOTS; ccd++) {
		const uint8_t *die = record + APOB_CCD_OFFSET + ccd * APOB_CCD_BYTES;
		const bool die_present = die[0] != 0xff;
		unsigned int die_slot = result.ccd_count;
		uint8_t complex_seen = 0;

		if (die_present) {
			if (die_slot == AM4_DESKTOP_MAX_CCDS ||
			    die[0] >= AM4_DESKTOP_PHYSICAL_CCD_SLOTS)
				return false;
			for (unsigned int i = 0; i < die_slot; i++) {
				if (result.physical_ccd[i] == die[0])
					return false;
			}
			result.logical_ccd[die_slot] = (uint8_t)ccd;
			result.physical_ccd[die_slot] = die[0];
			result.ccd_count++;
		}
		for (unsigned int ccx = 0; ccx < APOB_CCX_SLOTS; ccx++) {
			const uint8_t *complex = die + 1 + ccx * APOB_CCX_BYTES;
			const bool complex_present = complex[0] != 0xff;
			uint8_t core_seen = 0;

			if (complex_present) {
				if (!die_present || ccx >= complexes_per_ccd ||
				    complex[0] >= complexes_per_ccd ||
				    (complex_seen & (1U << complex[0])))
					return false;
				complex_seen |= 1U << complex[0];
			}
			for (unsigned int core = 0; core < APOB_CORE_SLOTS; core++) {
				const uint8_t *entry = complex + 1 + core * 3;
				const uint8_t physical = entry[0];

				if (entry[1] > 1 || entry[2] > 1 ||
				    (result.threads_per_core == 1 && entry[2]))
					return false;
				if (physical == 0xff) {
					if (entry[1] || entry[2])
						return false;
					continue;
				}
				if (!complex_present || core >= cores_per_ccx ||
				    physical >= cores_per_ccx ||
				    (core_seen & (1U << physical)))
					return false;
				core_seen |= 1U << physical;
				for (unsigned int thread = 0; thread < result.threads_per_core;
				     thread++) {
					struct am4_desktop_thread *target;
					unsigned int bit;

					if (!entry[1 + thread])
						continue;
					if (result.thread_count == AM4_DESKTOP_MAX_THREADS)
						return false;
					bit = ccx * 8 + core * result.threads_per_core + thread;
					result.launch_mask[die_slot] |= 1U << bit;
					target = &result.threads[result.thread_count++];
					*target = (struct am4_desktop_thread) {
						.ccd = (uint8_t)ccd, .ccx = (uint8_t)ccx,
						.core = (uint8_t)core, .thread = (uint8_t)thread,
						.physical_ccd = die[0], .physical_ccx = complex[0],
						.physical_core = physical, .launch_bit = (uint8_t)bit,
					};
				}
			}
		}
	}
	if (result.thread_count != logical_threads)
		return false;
	*topology = result;
	return true;
}
