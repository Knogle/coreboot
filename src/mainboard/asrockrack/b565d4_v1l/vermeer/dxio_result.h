/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_DXIO_RESULT_H
#define AMD_MATISSE_DXIO_RESULT_H

#include "dxio_engines.h"
#include "nbif_port.h"

/* Caller-qualified topology fields, without packed vendor descriptors. */
struct matisse_dxio_result_slot {
	struct matisse_dxio_lane_group lanes;
	uint8_t encoding;
	uint8_t fallback;
};

struct matisse_dxio_engine_result {
	uint16_t engine_index;
	uint16_t policy_index;
	uint8_t slot;
	uint8_t encoding;
	uint8_t devfn;
	uint8_t pool_index;
	bool enabled;
	bool early;
	uint32_t bdf;
};

struct matisse_dxio_result {
	uint8_t assigned[MATISSE_NBIF_MAP_SLOTS];
	uint8_t count;
	/* Last early engine, matching the original context replacement order.
	 * This flag is not evidence of actual training or phase completion.
	 */
	bool has_early;
	uint16_t early_engine;
	uint32_t early_bdf;
	struct matisse_dxio_engine_result engines[MATISSE_NBIF_MAP_SLOTS];
};

/* Pure decoding of an owned returned platform snapshot. PCIe engines match
 * native policy by computed DXIO first lane, then a qualified topology slot
 * by wrapper lane range and returned local port ID at engine+0x19. Other
 * engine types are ignored, as in original09c7aad7. Disabled PCIe engines
 * retain a binding but do not allocate an NBIF value.
 *
 * Ambiguous policy/topology, reused slots, colliding assignments, truncated
 * frames, pointer wrap or output overlap return false before output stores.
 * Input backing stays unchanged. The caller owns DMA/cache ordering, policy,
 * actual topology, board controls and subsequent register configuration.
 * This decoder performs no I/O; its caller owns physical access.
 */
bool matisse_decode_dxio_result(const struct matisse_dxio_result_slot *slots,
			       size_t slot_count,
			       const struct matisse_dxio_port *ports,
			       size_t port_count, const uint8_t *frame,
			       size_t frame_bytes, uint8_t bus,
			       struct matisse_dxio_result *output);

#endif /* AMD_MATISSE_DXIO_RESULT_H */
