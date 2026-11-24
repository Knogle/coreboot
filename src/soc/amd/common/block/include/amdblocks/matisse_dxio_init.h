/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_DXIO_INIT_H
#define AMD_MATISSE_DXIO_INIT_H

#include <amdblocks/matisse_dxio_init_plan.h>
#include <amdblocks/matisse_dxio_poll.h>
#include <amdblocks/smu.h>

enum matisse_dxio_phase {
	MATISSE_DXIO_NOT_STARTED,
	MATISSE_DXIO_ADMITTING,
	MATISSE_DXIO_SENDING,
	MATISSE_DXIO_POLLING_FIRST,
	MATISSE_DXIO_ADVANCING,
	MATISSE_DXIO_POLLING_SECOND,
	MATISSE_DXIO_READING_BACK,
	MATISSE_DXIO_FINISHED,
	MATISSE_DXIO_FAILED,
};

enum matisse_dxio_failure {
	MATISSE_DXIO_NO_FAILURE,
	MATISSE_DXIO_ADMISSION_FAILED,
	MATISSE_DXIO_BUDGET_EXPIRED,
	MATISSE_DXIO_TRANSPORT_FAILED,
	MATISSE_DXIO_INITIAL_STATUS_FAILED,
	MATISSE_DXIO_LOAD_FAILED,
	MATISSE_DXIO_START_FAILED,
	MATISSE_DXIO_REPLY_INVALID,
	MATISSE_DXIO_FIRMWARE_ERROR,
	MATISSE_DXIO_HANDLER_FAILED,
	MATISSE_DXIO_PHASE_UNCONFIRMED,
	MATISSE_DXIO_BUFFER_FAILED,
	MATISSE_DXIO_PHASE_CLEANUP_FAILED,
	MATISSE_DXIO_ACCESS_DENIED,
};

enum matisse_dxio_event_result {
	MATISSE_DXIO_EVENT_FAILED = -1,
	MATISSE_DXIO_EVENT_HANDLED,
	MATISSE_DXIO_EVENT_PHASE_DONE,
};

/* Valid only during a handler call. Additional handler requests use this
 * interface to share the controller's budget, failure state and counters.
 */
struct matisse_dxio_transport {
	enum cb_err (*request)(void *context, uint32_t opcode,
			       const uint32_t arguments[5], struct smu_payload *reply);
	void *context;
};

struct matisse_dxio_init_hooks {
	enum cb_err (*admit)(const struct matisse_dxio_init_buffers *buffers, void *context);
	/* Uses 3/6 before loading platform/ancillary, and 14 before result DMA.
	 * Caller owns validation, cache maintenance and any required clearing.
	 */
	enum cb_err (*prepare)(const struct matisse_dxio_init_buffers *buffers,
			       unsigned int use, void *context);
	enum cb_err (*consume)(const struct matisse_dxio_init_buffers *buffers, void *context);
	enum matisse_dxio_event_result (*event)(const struct matisse_dxio_poll_result *result,
			unsigned int phase, const struct matisse_dxio_transport *transport,
			void *context);
	void *context;
	/* Required when event handlers retain temporary PCI routes. Called once
	 * when each poll phase returns, including completion without an event
	 * callback and failures. No controller transport remains available here.
	 * Cleanup itself must be bounded, retain exclusive PCI ownership and keep
	 * partial state if access is no longer admitted; never retry it implicitly.
	 */
	enum cb_err (*finish_phase)(const struct matisse_dxio_init_buffers *buffers,
				    unsigned int phase, bool completed, void *context);
	/* Optional enclosing ownership/budget check before every request and
	 * handler. A denied access stops further requests; phase cleanup remains
	 * responsible for its own bounded access and retained partial state.
	 */
	bool (*may_access)(void *context);
};

/* Initially zeroed persistent caller-owned workspace, not an early stack
 * allocation. Keep it after failure; even a failing request may have changed
 * firmware state. completed counts outer mailbox completions, not trained links.
 */
struct matisse_dxio_init_state {
	uint32_t phase, failure, planned, attempted, completed, polls, events;
	uint32_t completed_phases, nonfatal_statuses, last_opcode;
	uint32_t version_word2, version_word3;
	int32_t last_transport_status;
	bool legacy_zero_version;
	struct smu_payload last_reply;
	struct smu_payload version_reply, initial_reply;
	struct matisse_dxio_poll_result last_event;
	struct matisse_dxio_init_buffers buffers;
	struct matisse_dxio_init_request requests[MATISSE_DXIO_MAX_INIT_REQUESTS];
	/* Bits0/1 describe the first/second phase cleanup attempt and failure. */
	uint32_t phase_finish_attempted, phase_finish_failures;
	int32_t last_phase_finish_status;
};

/* Experimental desktop backend component. Caller supplies distinct,
 * stable policy/buffers/state/hooks, owns SMN/SMU exclusively and admits actual
 * reserved DMA backing. admit/prepare/consume are mandatory and bounded.
 *
 * Uses the unchanged common SMU driver, with a 120-second program budget checked
 * before every request, at most 1024 requests and 256 polls. In-flight calls
 * retain the common driver's bounds. No automatic retry/reentry after starting.
 * Required load/start errors stop; original nonfatal property/ACK replies are
 * recorded. Returning events5/13 require explicit handler PHASE_DONE; ordinary
 * unrecognized events still acknowledge/continue. Handler requests share all
 * limits. Optional finish_phase closes retained PCI routes before advancing or
 * final result DMA, including after failed polling. A cleanup error stops further
 * requests and preserves an earlier failure. It is not called before a poll
 * phase begins. CB_SUCCESS means both logical phases and required readback
 * completed, not actual PCIe enumeration/training or physical Matisse boot.
 */
enum cb_err matisse_dxio_run(const struct matisse_dxio_init_policy *policy,
			   const struct matisse_dxio_init_buffers *buffers,
			   struct matisse_dxio_init_state *state,
			   const struct matisse_dxio_init_hooks *hooks);

#endif /* AMD_MATISSE_DXIO_INIT_H */
