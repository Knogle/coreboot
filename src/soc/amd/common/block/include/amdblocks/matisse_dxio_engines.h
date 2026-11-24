/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_DXIO_ENGINES_H
#define AMD_MATISSE_DXIO_ENGINES_H

#include <amdblocks/matisse_dxio_lanes.h>

#define MATISSE_DXIO_PLATFORM_HEADER_BYTES 16
#define MATISSE_DXIO_ENGINE_BYTES 40
#define MATISSE_DXIO_PLATFORM_TAIL_BYTES 8

/* Desktop backend pure API. These are the consumed
 * control bytes of the original Matisse port policy, not a vendor structure
 * copied into firmware. Offsets below identify the independently qualified
 * original instruction inputs; names do not imply a complete board policy.
 *
 * Original offsets: engine_flags 5, reset_id 8, port_flags 11, routing 12/13,
 * link 14/15, equalization 17, power 19, miscellaneous 20, extended 29,
 * clock 37, auxiliary 41. Only bits used by the original builder affect serialization.
 */
struct matisse_dxio_port_policy {
	uint8_t type; /* Qualified engine types: PCIe 1 and SATA 3. */
	uint8_t engine_flags;
	uint8_t reset_id;
	uint8_t port_flags;
	uint8_t routing[2];
	uint8_t link[2];
	uint8_t equalization;
	uint8_t power;
	uint8_t miscellaneous;
	uint8_t extended;
	uint8_t clock;
	uint8_t auxiliary;
};

struct matisse_dxio_port {
	struct matisse_dxio_lane_group lanes;
	uint8_t first, last;
	struct matisse_dxio_port_policy policy;
	/* Offset into a separately owned ancillary buffer, in dwords. Zero when
	 * no per-engine ancillary data is supplied. The caller owns that buffer.
	 */
	uint16_t ancillary_offset;
};

/* Pure serialization: no SMU, PCI, allocation or retained firmware calls.
 * Returns the complete byte count, or zero before any output store on invalid
 * lanes/types, empty/oversized input, null, insufficient capacity or overlap.
 * Inputs and output must have disjoint backing. The original eight-byte zero
 * tail is included. pspp_policy and force_reset_id are explicit board policy.
 * The caller owns lane conflicts, actual board policy, ancillary construction,
 * DMA placement, initialization order and subsequent DXIO training.
 */
size_t matisse_build_dxio_platform(const struct matisse_dxio_port *ports,
				 size_t count, uint8_t pspp_policy,
				 bool force_reset_id, uint8_t *output,
				 size_t capacity);

#endif /* AMD_MATISSE_DXIO_ENGINES_H */
