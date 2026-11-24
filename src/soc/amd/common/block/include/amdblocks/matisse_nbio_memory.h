/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MATISSE_NBIO_MEMORY_H
#define MATISSE_NBIO_MEMORY_H

#include <types.h>

/* Original single-root Matisse topology selects physical NBIO instance 3. */
#define MATISSE_NBIO_INSTANCE	3
#define MATISSE_NBIO_SMN_OFFSET	(MATISSE_NBIO_INSTANCE << 20)
#define MATISSE_NBIO_ROOT_TOM	0x90 /* root PCI configuration */
#define MATISSE_NBIO_TOM2_LOW	(0x13b10064 + MATISSE_NBIO_SMN_OFFSET)
#define MATISSE_NBIO_TOM2_HIGH	(0x13b10068 + MATISSE_NBIO_SMN_OFFSET)
#define MATISSE_NBIO_TOM3		(0x13b10138 + MATISSE_NBIO_SMN_OFFSET)
#define MATISSE_NBIO_TOM_CONTROL	(0x13b0004c + MATISSE_NBIO_SMN_OFFSET)

struct matisse_nbio_memory {
	uint32_t root_tom;
	uint32_t tom2_low;
	uint32_t tom2_high;
	uint32_t tom3;
	uint32_t control;
};

/* Experimental desktop backend component.
 *
 * Encode the original single-I/O-die NBIO register updates from admitted
 * Matisse CPU MSRs and existing register attributes. This performs no I/O
 * and does not discover, reserve or advertise memory. The caller must still
 * establish the complete memory/resource contract and own any later stores.
 */
bool matisse_plan_nbio_memory(const struct matisse_nbio_memory *before,
			      uint64_t top_mem, uint64_t top_mem2, uint64_t syscfg,
			      struct matisse_nbio_memory *after);

#endif /* MATISSE_NBIO_MEMORY_H */
