/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_DXIO_CONFIGURED_H
#define AMD_MATISSE_DXIO_CONFIGURED_H

#include "dxio_result.h"

enum matisse_dxio_configured_kind {
	MATISSE_DXIO_CONFIGURED_RMW,
	MATISSE_DXIO_CONFIGURED_WRITE,
	MATISSE_DXIO_CONFIGURED_RECORD,
};

struct matisse_dxio_configured_step {
	uint32_t kind;
	uint32_t address;
	uint32_t and_mask;
	uint32_t value;
	uint8_t encoding;
	uint8_t reserved[3];
};

/* Boolean inputs are the original PCD tokens, not inferred board defaults. */
struct matisse_dxio_configured_policy {
	uint8_t root_index;
	bool records;
	bool token_194;
	bool token_24c;
	bool token_2fb;
	bool token_231;
};

/* Pure ordered plan for original configured event09c75476. wrappers gives
 * actual traversal order; slots preserves original child order per wrapper.
 * result and ports must be the owned matching snapshots used by the decoder.
 * Each RMW reads fresh state and writes (old & and_mask) | value. WRITE emits
 * its value directly. RECORD reads first; only when bit0 is set and an owned
 * record table has fewer than128 entries may the caller append a snapshot
 * and write back the unchanged value. A missing table omits RECORD entirely.
 *
 * Register and indirect-selector operations keep original ordering. No I/O
 * or completion/phase decision occurs here. Caller owns execution, bus,
 * record backing, serialization, deadlines and partial-progress handling.
 * Invalid, overlapping or undersized output is unchanged and returns zero.
 */
size_t matisse_plan_dxio_configured(const uint8_t *wrappers, size_t wrapper_count,
		const struct matisse_dxio_result_slot *slots, size_t slot_count,
		const struct matisse_dxio_port *ports, size_t port_count,
		const struct matisse_dxio_result *result,
		const struct matisse_dxio_configured_policy *policy,
		struct matisse_dxio_configured_step *output, size_t capacity);

#endif /* AMD_MATISSE_DXIO_CONFIGURED_H */
