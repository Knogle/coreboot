/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_DXIO_EVENTS_H
#define AMD_MATISSE_DXIO_EVENTS_H

#include "dxio_mapped.h"
#include "dxio_early_link.h"
#include "dxio_configured_run.h"

struct matisse_dxio_events_input {
	const struct matisse_dxio_mapped_input *mapping;
	const struct matisse_dxio_configured_policy *configured;
};

struct matisse_dxio_events_io {
	enum cb_err (*admit)(const struct matisse_dxio_events_input *input, void *context);
	bool (*may_access)(void *context);
	enum cb_err (*prepare)(unsigned int use, void *context);
	enum cb_err (*consume)(void *context);
	enum cb_err (*smn_read)(uint32_t address, uint32_t *value, void *context);
	enum cb_err (*smn_write)(uint32_t address, uint32_t value, void *context);
	enum cb_err (*pci_read)(uint32_t address, unsigned int bytes, uint32_t *value,
			       void *context);
	enum cb_err (*pci_write)(uint32_t address, unsigned int bytes, uint32_t value,
				void *context);
	enum cb_err (*reset)(unsigned int group, unsigned int level, void *context);
	void (*report)(const struct matisse_dxio_poll_result *event, unsigned int phase,
		       void *context);
	void *context;
};

enum matisse_dxio_events_phase {
	MATISSE_DXIO_EVENTS_NOT_STARTED,
	MATISSE_DXIO_EVENTS_ADMITTING,
	MATISSE_DXIO_EVENTS_READY,
	MATISSE_DXIO_EVENTS_HANDLING,
	MATISSE_DXIO_EVENTS_FINISHING_PHASE,
	MATISSE_DXIO_EVENTS_FINISHED,
	MATISSE_DXIO_EVENTS_FAILED,
};

enum matisse_dxio_events_failure {
	MATISSE_DXIO_EVENTS_NO_FAILURE,
	MATISSE_DXIO_EVENTS_ADMISSION_FAILED,
	MATISSE_DXIO_EVENTS_ACCESS_DENIED,
	MATISSE_DXIO_EVENTS_INVALID_EVENT,
	MATISSE_DXIO_EVENTS_MAPPED_FAILED,
	MATISSE_DXIO_EVENTS_EARLY_FAILED,
	MATISSE_DXIO_EVENTS_PLAN_FAILED,
	MATISSE_DXIO_EVENTS_CONFIGURED_FAILED,
	MATISSE_DXIO_EVENTS_CLEANUP_FAILED,
	MATISSE_DXIO_EVENTS_RESET_FAILED,
};

/* Initially zeroed persistent caller storage, never an early stack object.
 * Successful later events reuse the last-event workspace; failed partial state
 * remains available and cannot restart. The record prefix spans both phases.
 */
struct matisse_dxio_events_state {
	uint32_t phase, failure, events, mapped_events, early_events, configured_events;
	uint32_t completed_phases, finish_attempted, cleanup_failures;
	uint32_t early_generation, duplicate_early_events;
	bool early_seen, route_valid;
	struct stopwatch budget;
	struct matisse_dxio_poll_result last_event;
	struct matisse_dxio_result result;
	struct matisse_dxio_link_controls controls[MATISSE_NBIF_MAP_SLOTS];
	struct matisse_dxio_mapped_state mapped;
	struct matisse_dxio_early_state early, route;
	struct matisse_dxio_configured_state configured;
	struct matisse_dxio_configured_step plan[MATISSE_DXIO_CONFIGURED_MAX_STEPS];
};

/* Begin before the controller, then call handle/finish from its event and
 * finish_phase hooks with the same stable inputs, I/O and state. The caller
 * admits actual single-root bus-zero policy, reserved DMA/cache backing,
 * pre-allocation PCI access, and exclusive SMN/SMU ownership. SMN callbacks
 * use the same common driver/root as the configured executor. All callbacks
 * must be bounded and may_access must include any enclosing controller budget.
 *
 * Fresh event15 reports remain repeatable; event5 also accepts an empty result.
 * A duplicate event7 for the same result generation is acknowledged without
 * repeating an already completed early-link operation. A fresh event15 starts
 * a new generation. Port IDs remain distinct from PCI configuration addresses.
 * Only one early root-port route is admitted in a poll phase; preserve it until
 * finish, including after a failed event. Cleanup is once per entered phase.
 *
 * A 120-second owner budget bounds all phases. PHASE_DONE follows the original
 * logical event5/13 return and does not establish training. The desktop
 * backend supplies DMA/cache and event-policy ownership. Physical execution
 * still requires separate qualification.
 */
enum cb_err matisse_begin_dxio_events(const struct matisse_dxio_events_input *input,
		struct matisse_dxio_events_state *state,
		const struct matisse_dxio_events_io *io);

enum matisse_dxio_event_result matisse_handle_dxio_event(
		const struct matisse_dxio_events_input *input,
		struct matisse_dxio_events_state *state,
		const struct matisse_dxio_poll_result *event, unsigned int phase,
		const struct matisse_dxio_transport *transport,
		const struct matisse_dxio_events_io *io);

enum cb_err matisse_finish_dxio_events_phase(const struct matisse_dxio_events_input *input,
		struct matisse_dxio_events_state *state, unsigned int phase, bool completed,
		const struct matisse_dxio_events_io *io);

#endif /* AMD_MATISSE_DXIO_EVENTS_H */
