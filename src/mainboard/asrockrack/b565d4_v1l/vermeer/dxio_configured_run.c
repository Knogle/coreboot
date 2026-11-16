/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/smn.h>
#include <limits.h>
#include <timer.h>
#include "dxio_configured_run.h"

#define CONFIGURED_BUDGET_MS 120000

struct execution {
	struct matisse_dxio_configured_state *state;
	const struct matisse_dxio_configured_hooks *hooks;
	struct stopwatch budget;
};

static bool valid_range(const void *pointer, size_t bytes)
{
	return pointer && (uintptr_t)pointer <= UINTPTR_MAX - bytes;
}

static bool overlap(const void *first, size_t first_bytes,
		    const void *second, size_t second_bytes)
{
	return (uintptr_t)first < (uintptr_t)second + second_bytes &&
		(uintptr_t)second < (uintptr_t)first + first_bytes;
}

static bool may_access(struct execution *execution, uint32_t address, bool write)
{
	struct matisse_dxio_configured_state *state = execution->state;

	if (stopwatch_expired(&execution->budget)) {
		state->failure = MATISSE_DXIO_CONFIGURED_BUDGET_EXPIRED;
		return false;
	}
	if (!execution->hooks->may_access(address, write, execution->hooks->context)) {
		state->failure = MATISSE_DXIO_CONFIGURED_ACCESS_DENIED;
		return false;
	}
	if (stopwatch_expired(&execution->budget)) {
		state->failure = MATISSE_DXIO_CONFIGURED_BUDGET_EXPIRED;
		return false;
	}
	state->last_address = address;
	state->attempted++;
	asm volatile ("" : : : "memory");
	return true;
}

static bool read_register(struct execution *execution, uint32_t address,
			  uint32_t *value)
{
	if (!may_access(execution, address, false))
		return false;
	*value = smn_read32(address);
	execution->state->last_before = *value;
	execution->state->reads++;
	return true;
}

static bool write_register(struct execution *execution, uint32_t address, uint32_t value)
{
	if (!may_access(execution, address, true))
		return false;
	execution->state->last_after = value;
	asm volatile ("" : : : "memory");
	smn_write32(address, value);
	execution->state->writes++;
	return true;
}

enum cb_err matisse_run_dxio_configured(const struct matisse_dxio_configured_step *plan,
				      size_t count,
				      struct matisse_dxio_configured_state *state,
				      const struct matisse_dxio_configured_hooks *hooks)
{
	struct execution execution = { .state = state, .hooks = hooks };
	size_t plan_bytes;

	if (!count || count > MATISSE_DXIO_CONFIGURED_MAX_STEPS ||
	    count > SIZE_MAX / sizeof(*plan))
		return CB_ERR;
	plan_bytes = count * sizeof(*plan);
	if (!valid_range(plan, plan_bytes) || !valid_range(state, sizeof(*state)) ||
	    !valid_range(hooks, sizeof(*hooks)) ||
	    overlap(state, sizeof(*state), plan, plan_bytes) ||
	    overlap(state, sizeof(*state), hooks, sizeof(*hooks)) ||
	    state->phase != MATISSE_DXIO_CONFIGURED_NOT_STARTED ||
	    state->record_count > MATISSE_DXIO_CONFIGURED_MAX_RECORDS ||
	    !hooks->admit || !hooks->may_access)
		return CB_ERR;
	for (size_t i = 0; i < count; i++)
		if (plan[i].kind > MATISSE_DXIO_CONFIGURED_RECORD || (plan[i].address & 3U))
			return CB_ERR;
	state->planned = (uint32_t)count;
	state->completed = 0;
	state->attempted = 0;
	state->reads = 0;
	state->writes = 0;
	state->skipped_records = 0;
	state->failure = MATISSE_DXIO_CONFIGURED_NO_FAILURE;
	state->phase = MATISSE_DXIO_CONFIGURED_ADMITTING;
	stopwatch_init_msecs_expire(&execution.budget, CONFIGURED_BUDGET_MS);
	if (hooks->admit(plan, count, hooks->context) != CB_SUCCESS) {
		state->failure = MATISSE_DXIO_CONFIGURED_ADMISSION_FAILED;
		goto failed;
	}
	state->phase = MATISSE_DXIO_CONFIGURED_RUNNING;
	for (size_t i = 0; i < count; i++) {
		const struct matisse_dxio_configured_step *step = &plan[i];
		uint32_t value = step->value;

		state->last_kind = step->kind;
		if (step->kind != MATISSE_DXIO_CONFIGURED_WRITE) {
			if (!read_register(&execution, step->address, &value))
				goto failed;
			if (step->kind == MATISSE_DXIO_CONFIGURED_RMW) {
				value = (value & step->and_mask) | step->value;
			} else if (!(value & 1U) ||
				   state->record_count == MATISSE_DXIO_CONFIGURED_MAX_RECORDS) {
				state->skipped_records++;
				state->completed++;
				continue;
			} else {
				struct matisse_dxio_register_record *record =
					&state->records[state->record_count++];

				record->address = step->address;
				record->value = value;
			}
		}
		if (!write_register(&execution, step->address, value))
			goto failed;
		state->completed++;
	}
	if (stopwatch_expired(&execution.budget)) {
		state->failure = MATISSE_DXIO_CONFIGURED_BUDGET_EXPIRED;
		goto failed;
	}
	state->phase = MATISSE_DXIO_CONFIGURED_IO_RETURNED;
	return CB_SUCCESS;

failed:
	state->phase = MATISSE_DXIO_CONFIGURED_FAILED;
	return CB_ERR;
}
