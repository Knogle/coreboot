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

/* Native consumed fields from the original AM4 Matisse root constructor,
 * descriptor traversal and compiled PCD defaults. No retained descriptors or
 * runtime firmware pointers. This policy is linked only by the experimental desktop backend.
 *
 * Caller establishes the actual single-root AM4 Matisse topology and phase,
 * then may apply explicit native board overrides before beginning DXIO.
 * Optional vendor S3 register-record tables are not supplied by this policy.
 * The original compiled defaults are not evidence for later NVRAM/PEIM changes
 * or physical link training. Null returns false without stores.
 */
bool b565d4_matisse_build_link_policy(struct b565d4_matisse_link_policy *policy);

#endif /* B565D4_MATISSE_LINK_POLICY_H */
