/* SPDX-License-Identifier: GPL-2.0-only */

#include "dxio_lanes.h"

bool matisse_map_dxio_lanes(const struct matisse_dxio_lane_group *group,
			   uint8_t first, uint8_t last,
			   struct matisse_dxio_lane_assignment *assignment)
{
	struct matisse_dxio_lane_assignment result;
	unsigned int low, high;

	if (!group || !assignment || group->physical_first > group->physical_last ||
	    group->dxio_first > group->dxio_last ||
	    group->physical_last - group->physical_first !=
	    group->dxio_last - group->dxio_first)
		return false;

	low = first < last ? first : last;
	high = first > last ? first : last;
	if (low < group->physical_first || high > group->physical_last)
		return false;

	low -= group->physical_first;
	high -= group->physical_first;
	if (group->reversed) {
		result.first = (uint8_t)(group->dxio_last - high);
		result.last = (uint8_t)(group->dxio_last - low);
	} else {
		result.first = (uint8_t)(group->dxio_first + low);
		result.last = (uint8_t)(group->dxio_first + high);
	}
	result.reversed = group->reversed != (first > last);
	*assignment = result;
	return true;
}
