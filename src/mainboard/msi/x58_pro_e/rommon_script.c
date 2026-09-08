/* SPDX-License-Identifier: GPL-2.0-only */

#include "rommon_script.h"

#define X58_RS_PROGRAM_MAGIC	0x31535258u /* "XRS1" */
#define X58_RS_TRANSACTION_MAGIC	0x31545258u /* "XRT1" */
#define X58_RS_FNV_OFFSET	2166136261u
#define X58_RS_FNV_PRIME	16777619u

struct x58_rs_state {
	struct x58_rs_op ops[X58_RS_MAX_OPS];
	struct x58_rs_trace traces[X58_RS_MAX_OPS];
	size_t op_count;
	size_t trace_count;
	uint32_t transaction_digest;
	enum x58_rs_result last_result;
	enum x58_rs_result rollback_result;
	bool transaction_valid;
	bool program_sealed;
	bool rollback_available;
	bool auto_rollback;
	bool rollback_attempted;
	bool rolled_back;
	bool running;
	uint32_t rollback_attempts;
	size_t mutations;
	size_t nonreversible_mutations;
};

static struct x58_rs_state state;

static void bytes_clear(void *buffer, size_t size)
{
	uint8_t *bytes = buffer;

	while (size--)
		*bytes++ = 0;
}

static uint32_t fnv_byte(uint32_t hash, uint8_t value)
{
	return (hash ^ value) * X58_RS_FNV_PRIME;
}

static uint32_t fnv_u32(uint32_t hash, uint32_t value)
{
	for (unsigned int shift = 0; shift < 32; shift += 8)
		hash = fnv_byte(hash, value >> shift);
	return hash;
}

static uint32_t fnv_value(uint32_t hash, const struct x58_rs_value *value)
{
	hash = fnv_u32(hash, value->lo);
	return fnv_u32(hash, value->hi);
}

static uint32_t fnv_op(uint32_t hash, const struct x58_rs_op *op)
{
	hash = fnv_u32(hash, op->kind);
	hash = fnv_u32(hash, op->space);
	hash = fnv_u32(hash, op->width);
	hash = fnv_u32(hash, op->flags);
	hash = fnv_u32(hash, op->target);
	hash = fnv_value(hash, &op->mask);
	hash = fnv_value(hash, &op->value);
	return fnv_u32(hash, op->limit);
}

static uint32_t program_digest(void)
{
	uint32_t hash = X58_RS_FNV_OFFSET;

	hash = fnv_u32(hash, X58_RS_PROGRAM_MAGIC);
	hash = fnv_u32(hash, X58_RS_FORMAT_VERSION);
	hash = fnv_u32(hash, state.op_count);
	for (size_t i = 0; i < state.op_count; i++)
		hash = fnv_op(hash, &state.ops[i]);
	return hash;
}

static uint32_t transaction_digest(void)
{
	uint32_t hash = X58_RS_FNV_OFFSET;

	hash = fnv_u32(hash, X58_RS_TRANSACTION_MAGIC);
	hash = fnv_u32(hash, X58_RS_FORMAT_VERSION);
	hash = fnv_u32(hash, program_digest());
	hash = fnv_u32(hash, state.trace_count);
	hash = fnv_u32(hash, state.last_result);
	hash = fnv_u32(hash, state.rollback_result);
	hash = fnv_u32(hash, state.auto_rollback);
	hash = fnv_u32(hash, state.rollback_attempted);
	hash = fnv_u32(hash, state.rolled_back);
	hash = fnv_u32(hash, state.rollback_attempts);
	hash = fnv_u32(hash, state.mutations);
	hash = fnv_u32(hash, state.nonreversible_mutations);
	for (size_t i = 0; i < state.trace_count; i++) {
		const struct x58_rs_trace *trace = &state.traces[i];

		hash = fnv_op(hash, &trace->op);
		hash = fnv_value(hash, &trace->before);
		hash = fnv_value(hash, &trace->observed);
		hash = fnv_value(hash, &trace->rollback_observed);
		hash = fnv_u32(hash, trace->iterations);
		hash = fnv_u32(hash, trace->status);
		hash = fnv_u32(hash, trace->rollback_status);
		hash = fnv_u32(hash, trace->write_performed);
	}
	return hash;
}

static bool value_is_zero(const struct x58_rs_value *value)
{
	return value->lo == 0 && value->hi == 0;
}

static bool value_fits(const struct x58_rs_value *value, unsigned int width)
{
	if (width == 8)
		return true;
	if (value->hi != 0)
		return false;
	if (width == 4)
		return true;
	if (width == 2)
		return value->lo <= 0xffff;
	return width == 1 && value->lo <= 0xff;
}

static struct x58_rs_value width_mask(unsigned int width)
{
	struct x58_rs_value value = { 0 };

	if (width == 8) {
		value.lo = UINT32_MAX;
		value.hi = UINT32_MAX;
	} else if (width == 4) {
		value.lo = UINT32_MAX;
	} else if (width == 2) {
		value.lo = 0xffff;
	} else {
		value.lo = 0xff;
	}
	return value;
}

static bool value_masked_equal(const struct x58_rs_value *actual,
	const struct x58_rs_value *expected, const struct x58_rs_value *mask)
{
	return (actual->lo & mask->lo) == expected->lo &&
		(actual->hi & mask->hi) == expected->hi;
}

static bool op_is_mutation(const struct x58_rs_op *op)
{
	return op->kind == X58_RS_OP_WRITE || op->kind == X58_RS_OP_MASK;
}

static bool target_valid(const struct x58_rs_op *op)
{
	if (op->space == X58_RS_SPACE_MSR)
		return op->width == 8;
	if (op->width != 1 && op->width != 2 && op->width != 4)
		return false;
	if (op->space == X58_RS_SPACE_IO)
		return op->target <= 0xffff &&
			op->target <= 0xffffu - ((uint32_t)op->width - 1);
	if (op->space == X58_RS_SPACE_MEM)
		return !(op->target & (op->width - 1)) &&
			op->target <= UINT32_MAX - (op->width - 1);
	if (op->space == X58_RS_SPACE_PCI) {
		const uint8_t dev = op->target >> 16;
		const uint8_t fn = op->target >> 8;
		const uint8_t reg = op->target;

		return dev <= 0x1f && fn <= 7 && !(reg & (op->width - 1));
	}
	return false;
}

static bool op_valid(const struct x58_rs_op *op)
{
	struct x58_rs_value allowed;

	if (op == NULL || op->kind > X58_RS_OP_ASSERT ||
	    op->flags & ~X58_RS_FLAG_REVERSIBLE)
		return false;
	if (op->kind == X58_RS_OP_DELAY)
		return op->space == X58_RS_SPACE_NONE && op->width == 0 &&
			op->flags == 0 && op->target == 0 &&
			value_is_zero(&op->mask) && value_is_zero(&op->value) &&
			op->limit > 0 && op->limit <= X58_RS_DELAY_LIMIT_MAX;
	if (!target_valid(op))
		return false;
	allowed = width_mask(op->width);
	if (!value_fits(&op->mask, op->width) ||
	    !value_fits(&op->value, op->width))
		return false;
	if (op->kind != X58_RS_OP_WRITE && op->kind != X58_RS_OP_MASK &&
	    op->flags != 0)
		return false;
	if (op->kind == X58_RS_OP_READ)
		return value_is_zero(&op->mask) && value_is_zero(&op->value) &&
			op->limit == 0;
	if (op->kind == X58_RS_OP_WRITE)
		return value_is_zero(&op->mask) && op->limit == 0;
	if (op->kind == X58_RS_OP_MASK)
		return op->limit == 0 &&
			(!value_is_zero(&op->mask) || !value_is_zero(&op->value));
	if (value_is_zero(&op->mask) ||
	    (op->value.lo & ~op->mask.lo) ||
	    (op->value.hi & ~op->mask.hi) ||
	    (op->mask.lo & ~allowed.lo) || (op->mask.hi & ~allowed.hi))
		return false;
	if (op->kind == X58_RS_OP_ASSERT)
		return op->limit == 0;
	return op->kind == X58_RS_OP_POLL && op->limit > 0 &&
		op->limit <= X58_RS_POLL_LIMIT_MAX;
}

static void clear_transaction(void)
{
	bytes_clear(state.traces, sizeof(state.traces));
	state.trace_count = 0;
	state.transaction_digest = 0;
	state.last_result = X58_RS_OK;
	state.rollback_result = X58_RS_OK;
	state.transaction_valid = false;
	state.rollback_available = false;
	state.auto_rollback = false;
	state.rollback_attempted = false;
	state.rolled_back = false;
	state.rollback_attempts = 0;
	state.mutations = 0;
	state.nonreversible_mutations = 0;
}

void x58_rs_initialize(void)
{
	bytes_clear(&state, sizeof(state));
}

enum x58_rs_result x58_rs_clear(void)
{
	if (state.running)
		return X58_RS_ERR_BUSY;
	if (state.transaction_valid)
		return X58_RS_ERR_TRANSACTION_PENDING;
	bytes_clear(state.ops, sizeof(state.ops));
	state.op_count = 0;
	state.program_sealed = false;
	return X58_RS_OK;
}

enum x58_rs_result x58_rs_add(const struct x58_rs_op *op)
{
	if (state.running)
		return X58_RS_ERR_BUSY;
	if (state.transaction_valid)
		return X58_RS_ERR_TRANSACTION_PENDING;
	if (state.program_sealed)
		return X58_RS_ERR_SEALED;
	if (!op_valid(op))
		return X58_RS_ERR_INVALID_OP;
	if (state.op_count >= X58_RS_MAX_OPS)
		return X58_RS_ERR_FULL;
	state.ops[state.op_count++] = *op;
	return X58_RS_OK;
}

enum x58_rs_result x58_rs_seal(void)
{
	if (state.running)
		return X58_RS_ERR_BUSY;
	if (state.transaction_valid)
		return X58_RS_ERR_TRANSACTION_PENDING;
	if (state.op_count == 0)
		return X58_RS_ERR_EMPTY;
	state.program_sealed = true;
	return X58_RS_OK;
}

static enum x58_rs_result trace_failure(const struct x58_rs_trace *trace)
{
	if (trace->status == X58_RS_TRACE_READ_FAILED)
		return X58_RS_ERR_READ;
	if (trace->status == X58_RS_TRACE_WRITE_FAILED)
		return X58_RS_ERR_WRITE;
	if (trace->status == X58_RS_TRACE_EVENT_FAILED)
		return X58_RS_ERR_EVENT;
	if (trace->status == X58_RS_TRACE_ASSERT_FAILED)
		return X58_RS_ERR_ASSERT;
	if (trace->status == X58_RS_TRACE_POLL_TIMEOUT)
		return X58_RS_ERR_POLL_TIMEOUT;
	return X58_RS_OK;
}

static void execute_trace(struct x58_rs_trace *trace,
	const struct x58_rs_backend *backend)
{
	const struct x58_rs_op *op = &trace->op;

	if (op->kind == X58_RS_OP_DELAY) {
		backend->delay(backend->context, op->limit);
		trace->iterations = op->limit;
		trace->status = X58_RS_TRACE_DONE;
		return;
	}
	if (op->kind == X58_RS_OP_READ) {
		if (!backend->read(backend->context, op, &trace->observed))
			trace->status = X58_RS_TRACE_READ_FAILED;
		else
			trace->status = X58_RS_TRACE_DONE;
		return;
	}
	if (op_is_mutation(op)) {
		struct x58_rs_value new_value;

		if (!backend->read(backend->context, op, &trace->before)) {
			trace->status = X58_RS_TRACE_READ_FAILED;
			return;
		}
		if (op->kind == X58_RS_OP_WRITE) {
			new_value = op->value;
		} else {
			new_value.lo = (trace->before.lo & ~op->mask.lo) |
				op->value.lo;
			new_value.hi = (trace->before.hi & ~op->mask.hi) |
				op->value.hi;
		}
		if (!backend->write(backend->context, op, &new_value)) {
			trace->status = X58_RS_TRACE_WRITE_FAILED;
			return;
		}
		trace->write_performed = true;
		state.mutations++;
		if (!(op->flags & X58_RS_FLAG_REVERSIBLE))
			state.nonreversible_mutations++;
		if (!backend->read(backend->context, op, &trace->observed)) {
			trace->status = X58_RS_TRACE_READ_FAILED;
			return;
		}
		trace->status = X58_RS_TRACE_DONE;
		return;
	}
	if (op->kind == X58_RS_OP_ASSERT) {
		trace->iterations = 1;
		if (!backend->read(backend->context, op, &trace->observed)) {
			trace->status = X58_RS_TRACE_READ_FAILED;
			return;
		}
		trace->status = value_masked_equal(&trace->observed, &op->value,
			&op->mask) ? X58_RS_TRACE_DONE : X58_RS_TRACE_ASSERT_FAILED;
		return;
	}
	for (uint32_t polls = 1; polls <= op->limit; polls++) {
		trace->iterations = polls;
		if (!backend->read(backend->context, op, &trace->observed)) {
			trace->status = X58_RS_TRACE_READ_FAILED;
			return;
		}
		if (value_masked_equal(&trace->observed, &op->value,
					&op->mask)) {
			trace->status = X58_RS_TRACE_DONE;
			return;
		}
	}
	trace->status = X58_RS_TRACE_POLL_TIMEOUT;
}

static enum x58_rs_result rollback_internal(const struct x58_rs_backend *backend)
{
	enum x58_rs_result result = X58_RS_OK;

	for (size_t remaining = state.trace_count; remaining > 0; remaining--) {
		const size_t index = remaining - 1;
		struct x58_rs_trace *trace = &state.traces[index];
		const struct x58_rs_value mask = width_mask(trace->op.width);

		if (!trace->write_performed)
			continue;
		if (!(trace->op.flags & X58_RS_FLAG_REVERSIBLE)) {
			if (result == X58_RS_OK)
				result = X58_RS_ERR_NOT_REVERSIBLE;
			continue;
		}
		trace->rollback_observed.lo = 0;
		trace->rollback_observed.hi = 0;
		trace->rollback_status = X58_RS_TRACE_STARTED;
		if (!backend->event(backend->context, X58_RS_EVENT_ROLLBACK_PRE,
				    index, trace)) {
			trace->rollback_status = X58_RS_TRACE_EVENT_FAILED;
			if (result == X58_RS_OK)
				result = X58_RS_ERR_EVENT;
			continue;
		}
		if (!backend->write(backend->context, &trace->op, &trace->before) ||
		    !backend->read(backend->context, &trace->op,
				   &trace->rollback_observed) ||
		    !value_masked_equal(&trace->rollback_observed, &trace->before,
					&mask)) {
			trace->rollback_status = X58_RS_TRACE_ROLLBACK_FAILED;
			result = X58_RS_ERR_ROLLBACK;
		} else {
			trace->rollback_status = X58_RS_TRACE_ROLLBACK_DONE;
		}
		if (!backend->event(backend->context, X58_RS_EVENT_ROLLBACK_POST,
				    index, trace)) {
			trace->rollback_status = X58_RS_TRACE_EVENT_FAILED;
			if (result == X58_RS_OK)
				result = X58_RS_ERR_EVENT;
		}
	}
	state.rollback_available = result != X58_RS_OK && state.mutations > 0 &&
		state.nonreversible_mutations == 0;
	state.rollback_attempted = true;
	state.rolled_back = result == X58_RS_OK;
	state.rollback_attempts++;
	state.rollback_result = result;
	return result;
}

enum x58_rs_result x58_rs_run(uint32_t expected_digest, bool auto_rollback,
	const struct x58_rs_backend *backend)
{
	enum x58_rs_result result = X58_RS_OK;

	if (state.running)
		return X58_RS_ERR_BUSY;
	if (state.transaction_valid)
		return X58_RS_ERR_TRANSACTION_PENDING;
	if (state.op_count == 0)
		return X58_RS_ERR_EMPTY;
	if (!state.program_sealed)
		return X58_RS_ERR_NOT_SEALED;
	if (expected_digest != program_digest())
		return X58_RS_ERR_DIGEST;
	if (backend == NULL || backend->read == NULL || backend->write == NULL ||
	    backend->delay == NULL || backend->event == NULL)
		return X58_RS_ERR_INVALID_OP;
	if (auto_rollback) {
		for (size_t i = 0; i < state.op_count; i++) {
			if (op_is_mutation(&state.ops[i]) &&
			    !(state.ops[i].flags & X58_RS_FLAG_REVERSIBLE))
				return X58_RS_ERR_NOT_REVERSIBLE;
		}
	}
	clear_transaction();
	state.running = true;
	state.auto_rollback = auto_rollback;
	for (size_t i = 0; i < state.op_count; i++) {
		struct x58_rs_trace *trace = &state.traces[i];

		bytes_clear(trace, sizeof(*trace));
		trace->op = state.ops[i];
		trace->status = X58_RS_TRACE_STARTED;
		state.trace_count = i + 1;
		if (!backend->event(backend->context, X58_RS_EVENT_PRE, i, trace)) {
			trace->status = X58_RS_TRACE_EVENT_FAILED;
			result = X58_RS_ERR_EVENT;
			break;
		}
		execute_trace(trace, backend);
		result = trace_failure(trace);
		if (!backend->event(backend->context, X58_RS_EVENT_POST, i, trace) &&
		    result == X58_RS_OK) {
			trace->status = X58_RS_TRACE_EVENT_FAILED;
			result = X58_RS_ERR_EVENT;
		}
		if (result != X58_RS_OK)
			break;
	}
	state.last_result = result;
	state.transaction_valid = true;
	state.rollback_available = state.mutations > 0 &&
		state.nonreversible_mutations == 0;
	if (result != X58_RS_OK && auto_rollback && state.rollback_available) {
		const enum x58_rs_result rollback = rollback_internal(backend);

		if (rollback != X58_RS_OK)
			result = X58_RS_ERR_ROLLBACK;
	}
	state.running = false;
	state.transaction_digest = transaction_digest();
	return result;
}

enum x58_rs_result x58_rs_rollback(uint32_t expected_digest,
	const struct x58_rs_backend *backend)
{
	enum x58_rs_result result;

	if (state.running)
		return X58_RS_ERR_BUSY;
	if (!state.transaction_valid)
		return X58_RS_ERR_NO_TRANSACTION;
	if (expected_digest != state.transaction_digest)
		return X58_RS_ERR_DIGEST;
	if (!state.rollback_available)
		return X58_RS_ERR_NOT_REVERSIBLE;
	if (backend == NULL || backend->read == NULL || backend->write == NULL ||
	    backend->event == NULL)
		return X58_RS_ERR_INVALID_OP;
	state.running = true;
	result = rollback_internal(backend);
	state.running = false;
	state.transaction_digest = transaction_digest();
	return result;
}

enum x58_rs_result x58_rs_discard(uint32_t expected_digest)
{
	if (state.running)
		return X58_RS_ERR_BUSY;
	if (!state.transaction_valid)
		return X58_RS_ERR_NO_TRANSACTION;
	if (expected_digest != state.transaction_digest)
		return X58_RS_ERR_DIGEST;
	clear_transaction();
	return X58_RS_OK;
}

void x58_rs_get_info(struct x58_rs_info *info)
{
	if (info == NULL)
		return;
	info->op_count = state.op_count;
	info->trace_count = state.trace_count;
	info->program_digest = state.op_count ? program_digest() : 0;
	info->transaction_digest = state.transaction_digest;
	info->last_result = state.last_result;
	info->rollback_result = state.rollback_result;
	info->transaction_valid = state.transaction_valid;
	info->program_sealed = state.program_sealed;
	info->rollback_available = state.rollback_available;
	info->auto_rollback = state.auto_rollback;
	info->rollback_attempted = state.rollback_attempted;
	info->rolled_back = state.rolled_back;
	info->rollback_attempts = state.rollback_attempts;
	info->mutations = state.mutations;
	info->nonreversible_mutations = state.nonreversible_mutations;
}

bool x58_rs_get_op(size_t index, struct x58_rs_op *op)
{
	if (op == NULL || index >= state.op_count)
		return false;
	*op = state.ops[index];
	return true;
}

bool x58_rs_get_trace(size_t index, struct x58_rs_trace *trace)
{
	if (trace == NULL || index >= state.trace_count)
		return false;
	*trace = state.traces[index];
	return true;
}

const char *x58_rs_result_name(enum x58_rs_result result)
{
	static const char *const names[] = {
		"ok", "empty", "full", "invalid-op", "transaction-pending",
		"no-transaction", "digest-mismatch", "busy", "not-reversible",
		"read-failed", "write-failed", "event-failed", "assert-failed",
		"poll-timeout", "rollback-failed", "sealed", "not-sealed",
	};

	return result >= X58_RS_OK && result <= X58_RS_ERR_NOT_SEALED ?
		names[result] :
		"invalid-result";
}

const char *x58_rs_kind_name(enum x58_rs_kind kind)
{
	static const char *const names[] = {
		"read", "write", "mask", "poll", "delay", "assert",
	};

	return kind >= X58_RS_OP_READ && kind <= X58_RS_OP_ASSERT ?
		names[kind] : "invalid";
}

const char *x58_rs_space_name(enum x58_rs_space space)
{
	static const char *const names[] = {
		"none", "io", "pci", "mem", "msr",
	};

	return space >= X58_RS_SPACE_NONE && space <= X58_RS_SPACE_MSR ?
		names[space] : "invalid";
}

const char *x58_rs_trace_name(enum x58_rs_trace_status status)
{
	static const char *const names[] = {
		"empty", "started", "done", "read-failed", "write-failed",
		"event-failed", "assert-failed", "poll-timeout",
		"rollback-done", "rollback-failed",
	};

	return status >= X58_RS_TRACE_EMPTY &&
		status <= X58_RS_TRACE_ROLLBACK_FAILED ? names[status] :
		"invalid";
}
