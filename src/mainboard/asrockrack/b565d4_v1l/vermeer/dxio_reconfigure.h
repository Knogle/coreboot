/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_DXIO_RECONFIGURE_H
#define AMD_MATISSE_DXIO_RECONFIGURE_H

#include "dxio_link_controls.h"
#include "dxio_result.h"

/* Explicit consumed scalar policy, including byte sentinels. Values must
 * represent the intended native board policy, not arbitrary zero defaults.
 */
struct matisse_dxio_reconfigure_policy {
	uint8_t token_4a, token_e6, token_fc, token_155, token_165;
	uint8_t token_181, token_182, token_1b5, token_1b7, token_1d3;
	uint8_t token_1fa, token_21c, token_22d, token_22f, token_231;
	uint8_t token_2b5, token_2c9, token_2cb, token_2e6, token_30a, token_319;
};

struct matisse_dxio_reconfigure_input {
	uint8_t root_index;
	const uint8_t *wrappers;
	size_t wrapper_count;
	const struct matisse_dxio_result_slot *slots;
	size_t slot_count;
	const struct matisse_dxio_result *bindings;
	const struct matisse_dxio_link_controls *controls; /* Indexed by map slot. */
	const struct matisse_dxio_reconfigure_policy *policy;
};

enum matisse_dxio_reconfigure_phase {
	MATISSE_DXIO_RECONFIGURE_NOT_STARTED,
	MATISSE_DXIO_RECONFIGURE_ADMITTING,
	MATISSE_DXIO_RECONFIGURE_RUNNING,
	MATISSE_DXIO_RECONFIGURE_IO_RETURNED,
	MATISSE_DXIO_RECONFIGURE_FAILED,
};

enum matisse_dxio_reconfigure_failure {
	MATISSE_DXIO_RECONFIGURE_NO_FAILURE,
	MATISSE_DXIO_RECONFIGURE_ADMISSION_FAILED,
	MATISSE_DXIO_RECONFIGURE_BUDGET_EXPIRED,
	MATISSE_DXIO_RECONFIGURE_IO_FAILED,
};

/* Persistent workspace; initially zero. Controls and partial progress survive
 * failure. A failing I/O callback may already have changed hardware state.
 */
struct matisse_dxio_reconfigure_state {
	uint32_t phase, failure, attempted, reads, writes, completed_wrappers;
	uint32_t last_address, last_value;
	int32_t last_status;
	struct matisse_dxio_link_controls controls[MATISSE_NBIF_MAP_SLOTS];
};

struct matisse_dxio_reconfigure_io {
	enum cb_err (*admit)(const struct matisse_dxio_reconfigure_input *input, void *context);
	/* Bounded callbacks must check continuing bus/SMN ownership and any
	 * enclosing controller budget before using the common SMN driver.
	 */
	enum cb_err (*read)(uint32_t address, uint32_t *value, void *context);
	enum cb_err (*write)(uint32_t address, uint32_t value, void *context);
	void *context;
};

/* Native preparation for original09c74fc8, port09c740bf/wrapper09c74b63.
 * Runs allocated ports in actual topology order, preserves ordered indirect
 * writes and makes conditional decisions from fresh reads. No vendor IDS
 * extension is invoked. Caller admits matching topology, controls, policy,
 * root/bus, buffer lifetime and correct initialization phase.
 *
 * Checks a120s budget around bounded callbacks, caps accesses at16384 and
 * refuses reentry after any started attempt. Invalid inputs leave state
 * unchanged. Success means I/O callbacks returned, not training or event ACK.
 * Outside the active firmware build; no physical Matisse qualification yet.
 */
enum cb_err matisse_run_dxio_reconfigure(const struct matisse_dxio_reconfigure_input *input,
				       struct matisse_dxio_reconfigure_state *state,
				       const struct matisse_dxio_reconfigure_io *io);

#endif /* AMD_MATISSE_DXIO_RECONFIGURE_H */
