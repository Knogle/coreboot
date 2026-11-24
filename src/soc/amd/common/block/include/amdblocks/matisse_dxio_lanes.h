/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_DXIO_LANES_H
#define AMD_MATISSE_DXIO_LANES_H

#include <types.h>

struct matisse_dxio_lane_group {
	uint8_t physical_first, physical_last;
	uint8_t dxio_first, dxio_last;
	bool reversed;
};

struct matisse_dxio_lane_assignment {
	uint8_t first, last;
	bool reversed;
};

/* Pure mapping within one caller-qualified NBIO lane group. The requested
 * endpoints may descend. Both endpoints must belong to the same group;
 * physical and DXIO ranges must have equal widths. On failure the output
 * is unchanged. This does not select the board's PCIe lane policy or train
 * a link.
 */
bool matisse_map_dxio_lanes(const struct matisse_dxio_lane_group *group,
			   uint8_t first, uint8_t last,
			   struct matisse_dxio_lane_assignment *assignment);

#endif /* AMD_MATISSE_DXIO_LANES_H */
