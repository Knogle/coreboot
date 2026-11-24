/* SPDX-License-Identifier: GPL-2.0-only */

#include <limits.h>
#include <amdblocks/matisse_dxio_link_controls.h>

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

bool matisse_update_dxio_link_controls(const struct matisse_dxio_port_policy *policy,
			const struct matisse_dxio_link_overrides *overrides,
			const struct matisse_dxio_link_controls *before,
			struct matisse_dxio_link_controls *output)
{
	struct matisse_dxio_link_controls controls;

	if (!valid_range(policy, sizeof(*policy)) ||
	    !valid_range(overrides, sizeof(*overrides)) ||
	    !valid_range(before, sizeof(*before)) || !valid_range(output, sizeof(*output)) ||
	    overlap(output, sizeof(*output), policy, sizeof(*policy)) ||
	    overlap(output, sizeof(*output), overrides, sizeof(*overrides)) ||
	    overlap(output, sizeof(*output), before, sizeof(*before)))
		return false;
	controls = *before;
	controls.flags_27 = (controls.flags_27 & 0xf0U) | (policy->link[0] & 3U) |
		((policy->auxiliary >> 1) & 4U) | ((policy->clock << 2) & 8U);
	controls.flags_28 = (controls.flags_28 & 0xf3U) |
		((policy->auxiliary >> 2) & 4U) | ((policy->clock << 1) & 8U);
	controls.flags_29 = (controls.flags_29 & 0xf0U) | (policy->link[1] & 15U);
	controls.flags_2e = (controls.flags_2e & 0x3fU) | ((policy->extended << 6) & 0xc0U);
	controls.flags_31 = (controls.flags_31 & 0xcfU) | ((policy->link[0] >> 2) & 0x30U);
	if (policy->equalization & 3U)
		controls.flags_31 = (controls.flags_31 & 0x3fU) |
			((policy->equalization << 6) & 0xc0U);
	controls.flags_32 = (controls.flags_32 & 0x0dU) |
		((policy->equalization >> 2) & 2U) |
		((policy->extended & 0x3cU) << 2) | ((policy->extended & 0xc0U) >> 4);
	if (policy->extended & 0x40U)
		controls.flags_27 = (controls.flags_27 & 15U) |
			((overrides->lane_control << 4) & 0xf0U);
	if (policy->extended & 0x80U)
		controls.flags_28 = (controls.flags_28 & 15U) |
			(overrides->lane_control & 0xf0U);
	if (policy->clock & 0x40U)
		controls.equalization = (controls.equalization & 0xc000U) |
			(overrides->equalization & 0x3fffU);
	if (policy->clock & 0x80U)
		controls.equalization_extended = overrides->equalization_extended;
	*output = controls;
	return true;
}
