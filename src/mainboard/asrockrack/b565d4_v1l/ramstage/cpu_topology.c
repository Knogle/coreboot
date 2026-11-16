/* SPDX-License-Identifier: GPL-2.0-only */
#include "b565d4_cpu_topology.h"

bool b565d4_decode_cezanne_topology(const uint8_t *record, size_t bytes,
				  unsigned int logical_threads,
				  struct b565d4_cpu_topology *topology)
{
	struct b565d4_cpu_topology result = { 0 };
	uint32_t physical_seen = 0;

	if (!record || !topology || bytes != B565D4_CEZANNE_TOPOLOGY_BYTES ||
	    !logical_threads || logical_threads > 2 * B565D4_CEZANNE_MAX_CORES)
		return false;

	/* Version-24 APOB 3/3/0: Cezanne has one eight-core complex. The
	 * neighboring legacy complex must be absent; its launch-bit encoding
	 * overlaps the Zen 3 complex and cannot be treated as another eight cores.
	 */
	if (record[0x31] != 0 || record[0x31 + 25] != 0xff)
		return false;
	/* The vendor derives the launch-bit stride from the first core's
	 * enabled threads. It is one when SMT is disabled, rather than the
	 * processor's maximum of two hardware threads per core.
	 */
	if (record[0x33] != 1 || record[0x34] > 1)
		return false;
	result.threads_per_core = 1 + record[0x34];
	for (unsigned int core = 0; core < B565D4_CEZANNE_MAX_CORES; core++) {
		const uint8_t *entry = record + 0x32 + core * 3;
		const uint8_t *absent = entry + 25;
		const uint8_t physical = entry[0];

		if (absent[0] != 0xff || absent[1] || absent[2] ||
		    entry[1] > 1 || entry[2] > entry[1] ||
		    entry[2] >= result.threads_per_core)
			return false;
		result.physical_core[core] = physical;
		if (physical == 0xff) {
			if (entry[1] || entry[2])
				return false;
			continue;
		}
		if (physical >= B565D4_CEZANNE_MAX_CORES ||
		    (physical_seen & (1U << physical)))
			return false;
		physical_seen |= 1U << physical;
		for (unsigned int thread = 0; thread < result.threads_per_core; thread++) {
			if (!entry[thread + 1])
				continue;
			result.launch_mask |= 1U << (result.threads_per_core * core + thread);
			result.logical_threads++;
		}
	}
	if (result.logical_threads != logical_threads || !(result.launch_mask & 1))
		return false;
	*topology = result;
	return true;
}
