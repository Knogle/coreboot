/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_ROMMON_SCRIPT_H
#define MAINBOARD_MSI_X58_PRO_E_ROMMON_SCRIPT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define X58_RS_FORMAT_VERSION		1u
#define X58_RS_MAX_OPS			32u
#define X58_RS_POLL_LIMIT_MAX		1000000u
#define X58_RS_DELAY_LIMIT_MAX		0x01000000u

enum x58_rs_space {
	X58_RS_SPACE_NONE,
	X58_RS_SPACE_IO,
	X58_RS_SPACE_PCI,
	X58_RS_SPACE_MEM,
	X58_RS_SPACE_MSR,
};

enum x58_rs_kind {
	X58_RS_OP_READ,
	X58_RS_OP_WRITE,
	X58_RS_OP_MASK,
	X58_RS_OP_POLL,
	X58_RS_OP_DELAY,
	X58_RS_OP_ASSERT,
};

enum x58_rs_flags {
	X58_RS_FLAG_REVERSIBLE = 1u << 0,
};

struct x58_rs_value {
	uint32_t lo;
	uint32_t hi;
};

struct x58_rs_op {
	uint8_t kind;
	uint8_t space;
	uint8_t width;
	uint8_t flags;
	uint32_t target;
	struct x58_rs_value mask;
	struct x58_rs_value value;
	uint32_t limit;
};

enum x58_rs_trace_status {
	X58_RS_TRACE_EMPTY,
	X58_RS_TRACE_STARTED,
	X58_RS_TRACE_DONE,
	X58_RS_TRACE_READ_FAILED,
	X58_RS_TRACE_WRITE_FAILED,
	X58_RS_TRACE_EVENT_FAILED,
	X58_RS_TRACE_ASSERT_FAILED,
	X58_RS_TRACE_POLL_TIMEOUT,
	X58_RS_TRACE_ROLLBACK_DONE,
	X58_RS_TRACE_ROLLBACK_FAILED,
};

struct x58_rs_trace {
	struct x58_rs_op op;
	struct x58_rs_value before;
	struct x58_rs_value observed;
	struct x58_rs_value rollback_observed;
	uint32_t iterations;
	uint8_t status;
	uint8_t rollback_status;
	bool write_performed;
	bool reserved;
};

enum x58_rs_result {
	X58_RS_OK,
	X58_RS_ERR_EMPTY,
	X58_RS_ERR_FULL,
	X58_RS_ERR_INVALID_OP,
	X58_RS_ERR_TRANSACTION_PENDING,
	X58_RS_ERR_NO_TRANSACTION,
	X58_RS_ERR_DIGEST,
	X58_RS_ERR_BUSY,
	X58_RS_ERR_NOT_REVERSIBLE,
	X58_RS_ERR_READ,
	X58_RS_ERR_WRITE,
	X58_RS_ERR_EVENT,
	X58_RS_ERR_ASSERT,
	X58_RS_ERR_POLL_TIMEOUT,
	X58_RS_ERR_ROLLBACK,
	X58_RS_ERR_SEALED,
	X58_RS_ERR_NOT_SEALED,
};

enum x58_rs_event {
	X58_RS_EVENT_PRE,
	X58_RS_EVENT_POST,
	X58_RS_EVENT_ROLLBACK_PRE,
	X58_RS_EVENT_ROLLBACK_POST,
};

struct x58_rs_backend {
	void *context;
	bool (*read)(void *context, const struct x58_rs_op *op,
		struct x58_rs_value *value);
	bool (*write)(void *context, const struct x58_rs_op *op,
		const struct x58_rs_value *value);
	void (*delay)(void *context, uint32_t iterations);
	bool (*event)(void *context, enum x58_rs_event event, size_t index,
		const struct x58_rs_trace *trace);
};

struct x58_rs_info {
	size_t op_count;
	size_t trace_count;
	uint32_t program_digest;
	uint32_t transaction_digest;
	enum x58_rs_result last_result;
	enum x58_rs_result rollback_result;
	bool transaction_valid;
	bool program_sealed;
	bool rollback_available;
	bool auto_rollback;
	bool rollback_attempted;
	bool rolled_back;
	uint32_t rollback_attempts;
	size_t mutations;
	size_t nonreversible_mutations;
};

void x58_rs_initialize(void);
enum x58_rs_result x58_rs_clear(void);
enum x58_rs_result x58_rs_add(const struct x58_rs_op *op);
enum x58_rs_result x58_rs_seal(void);
enum x58_rs_result x58_rs_run(uint32_t expected_digest, bool auto_rollback,
	const struct x58_rs_backend *backend);
enum x58_rs_result x58_rs_rollback(uint32_t expected_digest,
	const struct x58_rs_backend *backend);
enum x58_rs_result x58_rs_discard(uint32_t expected_digest);
void x58_rs_get_info(struct x58_rs_info *info);
bool x58_rs_get_op(size_t index, struct x58_rs_op *op);
bool x58_rs_get_trace(size_t index, struct x58_rs_trace *trace);
const char *x58_rs_result_name(enum x58_rs_result result);
const char *x58_rs_kind_name(enum x58_rs_kind kind);
const char *x58_rs_space_name(enum x58_rs_space space);
const char *x58_rs_trace_name(enum x58_rs_trace_status status);

#endif
