/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_DXIO_LINK_CONTROLS_H
#define AMD_MATISSE_DXIO_LINK_CONTROLS_H

#include "dxio_engines.h"

/* Explicit inputs additionally consumed by original09c7a76b. Original policy
 * offsets are30,38..39,40, respectively; no packed vendor policy is retained.
 */
struct matisse_dxio_link_overrides {
	uint8_t lane_control;
	uint16_t equalization;
	uint8_t equalization_extended;
};

/* Consumed link controls from original descriptor27..32. The numbered flags
 * name qualified input bytes without asserting undocumented electrical names.
 * Seed fields from caller-qualified topology defaults; no template address or
 * whole107-byte vendor descriptor is used by this preparation component.
 */
struct matisse_dxio_link_controls {
	uint8_t flags_27, flags_28, flags_29, flags_2a;
	uint16_t equalization;
	uint8_t equalization_extended;
	uint8_t flags_2e, flags_2f, flags_30, flags_31, flags_32;
};

/* Pure update after returned-engine matching. False leaves output unchanged.
 * Inputs and output need disjoint valid backing. This neither programs a link
 * nor performs the later flags29 preparation in original09c740bf.
 */
bool matisse_update_dxio_link_controls(const struct matisse_dxio_port_policy *policy,
			const struct matisse_dxio_link_overrides *overrides,
			const struct matisse_dxio_link_controls *before,
			struct matisse_dxio_link_controls *output);

#endif /* AMD_MATISSE_DXIO_LINK_CONTROLS_H */
