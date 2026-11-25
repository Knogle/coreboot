/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef B565D4_MATISSE_LINK_POLICY_H
#define B565D4_MATISSE_LINK_POLICY_H

#include <amdblocks/matisse_dxio_configured.h>
#include <amdblocks/matisse_dxio_reconfigure.h>

#define B565D4_MATISSE_WRAPPERS 2
#define B565D4_MATISSE_TOPOLOGY_PORTS 16
#define B565D4_MATISSE_ACTIVE_PORTS 6

struct b565d4_matisse_link_policy {
	uint8_t wrappers[B565D4_MATISSE_WRAPPERS];
	struct matisse_dxio_result_slot slots[B565D4_MATISSE_TOPOLOGY_PORTS];
	struct matisse_dxio_link_controls controls[MATISSE_NBIF_MAP_SLOTS];
	struct matisse_dxio_link_overrides overrides[B565D4_MATISSE_ACTIVE_PORTS];
	struct matisse_dxio_reconfigure_policy reconfigure;
	struct matisse_dxio_configured_policy configured;
};

#endif /* B565D4_MATISSE_LINK_POLICY_H */
