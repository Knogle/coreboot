/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_NBIF_MAP_H
#define AMD_MATISSE_NBIF_MAP_H

#include <types.h>

#define MATISSE_NBIF_MAP_SLOTS 18

struct matisse_nbif_map_update {
	uint32_t address;
	uint32_t value;
};

/* Return the qualified original device/function pool index, or -1. */
int matisse_nbif_devfn_index(uint8_t devfn);

/* Pure preparation helper for original Matisse root-port map programming.
 * assigned contains 18 qualified slot assignments; zero means fill from the
 * original device/function pool. Nonzero values must belong to that pool
 * and be unique. before contains actual register snapshots for this root.
 *
 * Builds all 18 SMN address/value updates, preserving each register's upper 24
 * bits. The single-root original constructor supplies root_index 3; other
 * representable indices do not imply a physically present or qualified root.
 *
 * Returns false without output stores for invalid/duplicate assignments,
 * pointers, wrap or output/input overlap. Caller owns topology qualification,
 * exclusive live SMN access, snapshot freshness, write verification and rollback.
 * A completed map does not identify which slots have actual allocated links.
 */
bool matisse_build_nbif_map(uint8_t root_index,
			   const uint8_t assigned[MATISSE_NBIF_MAP_SLOTS],
			   const uint32_t before[MATISSE_NBIF_MAP_SLOTS],
			   struct matisse_nbif_map_update output[MATISSE_NBIF_MAP_SLOTS]);

#endif /* AMD_MATISSE_NBIF_MAP_H */
