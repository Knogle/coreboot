/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef B565D4_VERMEER_CPPC_PCC_H
#define B565D4_VERMEER_CPPC_PCC_H

#include <stdbool.h>
#include <stdint.h>

/* Optional, finite firmware probe. This does not publish an OS interface. */
struct b565d4_cppc_pcc_state {
	uint64_t shared_base;
	uint32_t information;
	unsigned int nominal_mhz;
	unsigned int lowest_mhz;
	unsigned int cpu_count;
};

/* Available only after fresh allocation, all-CPU UC, original registration
 * and one completed read with valid capabilities for every CPU. No retry.
 */
bool b565d4_vermeer_cppc_pcc_get_state(struct b565d4_cppc_pcc_state *out);

void b565d4_vermeer_cppc_pcc_prepare(uint32_t information,
				 unsigned int nominal_mhz, unsigned int lowest_mhz);

#endif
