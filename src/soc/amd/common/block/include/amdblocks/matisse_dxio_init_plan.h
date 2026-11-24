/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_DXIO_INIT_PLAN_H
#define AMD_MATISSE_DXIO_INIT_PLAN_H

#include <types.h>

#define MATISSE_DXIO_MAX_INIT_REQUESTS 26

struct matisse_dxio_init_request {
	uint32_t opcode;
	uint32_t arguments[5];
};

struct matisse_dxio_init_policy {
	bool select_enabled;
	bool select_value;
	bool property11;
	bool property12;
	uint8_t property1f;
	uint8_t property14;
	bool property0d;
	uint8_t property13; /* UINT8_MAX omits the original optional property. */
	bool tuning_enabled;
	uint32_t tuning[3];
	/* Feature bits 3/4/5/7/8 request the corresponding operation-3a option.
	 * Bit 9 is its value; the original sends feature 9 in both states.
	 */
	uint16_t features;
	bool property23;
};

struct matisse_dxio_init_buffers {
	uint64_t platform;
	uint32_t platform_dwords;
	uint64_t ancillary;
	uint32_t ancillary_dwords;
};

/* Pure calculation for the desktop backend. Builds the ordered
 * original Matisse single-I/O-die requests through operation307, without any
 * I/O. Addresses are explicit physical addresses in the qualified 32-bit wire
 * fields; the caller owns stable, populated and DMA-accessible backing.
 * Policy fields name the original property numbers, not inferred semantics.
 *
 * Returns the request count, or zero without output stores for invalid buffer
 * geometry/ranges/overlap, unsupported feature bits, null or wrapping pointers,
 * insufficient capacity or metadata/output overlap. Capacity counts requests.
 *
 * A plan is not an unconditional execution program. The caller must consume
 * query13 response words2/3, check initialization operation0 and distinguish
 * direct low-byte replies from normalized wrapper statuses. Preserve raw
 * replies and stop on transport/required-load failure. Polling operation9,
 * event handling, operation308, result readback14, timeouts and actual PCIe
 * configuration remain caller responsibilities. A zero vendor return alone
 * does not establish that initialization or both training phases ran.
 */
size_t matisse_build_dxio_init_requests(
	const struct matisse_dxio_init_policy *policy,
	const struct matisse_dxio_init_buffers *buffers,
	struct matisse_dxio_init_request *output, size_t capacity);

#endif /* AMD_MATISSE_DXIO_INIT_PLAN_H */
