/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_DXIO_CONFIGURED_RUN_H
#define AMD_MATISSE_DXIO_CONFIGURED_RUN_H

#include "dxio_configured.h"

#define MATISSE_DXIO_CONFIGURED_MAX_STEPS 512
#define MATISSE_DXIO_CONFIGURED_MAX_RECORDS 128

enum matisse_dxio_configured_phase {
	MATISSE_DXIO_CONFIGURED_NOT_STARTED,
	MATISSE_DXIO_CONFIGURED_ADMITTING,
	MATISSE_DXIO_CONFIGURED_RUNNING,
	MATISSE_DXIO_CONFIGURED_IO_RETURNED,
	MATISSE_DXIO_CONFIGURED_FAILED,
};

enum matisse_dxio_configured_failure {
	MATISSE_DXIO_CONFIGURED_NO_FAILURE,
	MATISSE_DXIO_CONFIGURED_ADMISSION_FAILED,
	MATISSE_DXIO_CONFIGURED_BUDGET_EXPIRED,
	MATISSE_DXIO_CONFIGURED_ACCESS_DENIED,
};

struct matisse_dxio_register_record {
	uint32_t address, value;
};

/* Persistent caller workspace, initially zero except an admitted existing
 * record_count/records prefix. Keep after failure: a completed read or write
 * and even an interrupted current operation can have hardware effects.
 * Records retain the full register value, without a vendor packed table.
 */
struct matisse_dxio_configured_state {
	uint32_t phase, failure, planned, completed;
	uint32_t attempted, reads, writes, skipped_records;
	uint32_t last_kind, last_address, last_before, last_after;
	uint32_t record_count;
	struct matisse_dxio_register_record records[MATISSE_DXIO_CONFIGURED_MAX_RECORDS];
};

struct matisse_dxio_configured_hooks {
	enum cb_err (*admit)(const struct matisse_dxio_configured_step *plan,
			     size_t count, void *context);
	/* Mandatory before every access; caller checks continued ownership and
	 * any enclosing controller budget. Hooks must themselves be bounded.
	 */
	bool (*may_access)(uint32_t address, bool write, void *context);
	void *context;
};

/* Executes an owned native plan once through the unchanged common SMN driver.
 * Caller admits the actual single-root SOC_GNB_DEV/bus, exclusive SMN access,
 * plan provenance and backing, matching policy/topology and current phase.
 * The 120-second budget is checked before each access and after callbacks.
 * No automatic retry or reentry follows any started attempt, including failed
 * admission. Invalid pointers/aliases/plans fail before state changes or I/O.
 * CB_SUCCESS only means the requested common driver calls returned; no write
 * readback, training, event acknowledgement or physical boot is implied.
 * Experimental desktop backend: hardware execution still requires qualification.
 */
enum cb_err matisse_run_dxio_configured(const struct matisse_dxio_configured_step *plan,
				      size_t count,
				      struct matisse_dxio_configured_state *state,
				      const struct matisse_dxio_configured_hooks *hooks);

#endif /* AMD_MATISSE_DXIO_CONFIGURED_RUN_H */
