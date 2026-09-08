/* SPDX-License-Identifier: GPL-2.0-only */

#include <assert.h>
#include <stdio.h>

#include "rommon_script.h"

struct fake_backend {
	struct x58_rs_value values[256];
	uint32_t write_targets[64];
	struct x58_rs_value write_values[64];
	unsigned int reads;
	unsigned int writes;
	unsigned int delays;
	unsigned int events;
	unsigned int fail_read;
	unsigned int fail_event;
	unsigned int fail_write;
};

static bool fake_read(void *context, const struct x58_rs_op *op,
	struct x58_rs_value *value)
{
	struct fake_backend *fake = context;

	fake->reads++;
	if (fake->fail_read != 0 && fake->reads == fake->fail_read)
		return false;
	*value = fake->values[op->target & 0xff];
	return true;
}

static bool fake_write(void *context, const struct x58_rs_op *op,
	const struct x58_rs_value *value)
{
	struct fake_backend *fake = context;

	fake->writes++;
	assert(fake->writes <= sizeof(fake->write_targets) /
		sizeof(fake->write_targets[0]));
	fake->write_targets[fake->writes - 1] = op->target;
	fake->write_values[fake->writes - 1] = *value;
	if (fake->fail_write != 0 && fake->writes == fake->fail_write)
		return false;
	fake->values[op->target & 0xff] = *value;
	return true;
}

static void fake_delay(void *context, uint32_t iterations)
{
	struct fake_backend *fake = context;

	fake->delays += iterations;
}

static bool fake_event(void *context, enum x58_rs_event event, size_t index,
	const struct x58_rs_trace *trace)
{
	struct fake_backend *fake = context;

	(void)event;
	(void)index;
	(void)trace;
	fake->events++;
	return fake->fail_event == 0 || fake->events != fake->fail_event;
}

static struct x58_rs_backend backend(struct fake_backend *fake)
{
	const struct x58_rs_backend result = {
		.context = fake,
		.read = fake_read,
		.write = fake_write,
		.delay = fake_delay,
		.event = fake_event,
	};

	return result;
}

static struct x58_rs_op mem_op(enum x58_rs_kind kind, uint32_t target)
{
	const struct x58_rs_op op = {
		.kind = kind,
		.space = X58_RS_SPACE_MEM,
		.width = 4,
		.target = target,
	};

	return op;
}

static uint32_t current_program_digest(void)
{
	struct x58_rs_info info;

	x58_rs_get_info(&info);
	return info.program_digest;
}

static uint32_t current_transaction_digest(void)
{
	struct x58_rs_info info;

	x58_rs_get_info(&info);
	return info.transaction_digest;
}

static void test_validation_and_stable_digest(void)
{
	struct x58_rs_op read = mem_op(X58_RS_OP_READ, 0x1000);
	struct x58_rs_op mask = mem_op(X58_RS_OP_MASK, 0x1004);
	struct x58_rs_op check = mem_op(X58_RS_OP_ASSERT, 0x1004);
	struct x58_rs_op invalid = mem_op(X58_RS_OP_DELAY, 0);
	uint32_t first;

	mask.flags = X58_RS_FLAG_REVERSIBLE;
	mask.mask.lo = 0x0000ff00;
	mask.value.lo = 0x00000012;
	check.mask.lo = 0x000000ff;
	check.value.lo = 0x00000012;
	assert(x58_rs_add(&invalid) == X58_RS_ERR_INVALID_OP);
	assert(x58_rs_add(&read) == X58_RS_OK);
	assert(x58_rs_add(&mask) == X58_RS_OK);
	assert(x58_rs_add(&check) == X58_RS_OK);
	first = current_program_digest();
	assert(first != 0);
	assert(x58_rs_seal() == X58_RS_OK);
	assert(x58_rs_add(&read) == X58_RS_ERR_SEALED);
	assert(x58_rs_seal() == X58_RS_OK);

	x58_rs_initialize();
	assert(x58_rs_add(&read) == X58_RS_OK);
	assert(x58_rs_add(&mask) == X58_RS_OK);
	assert(x58_rs_add(&check) == X58_RS_OK);
	assert(current_program_digest() == first);
	printf("stable-program-digest=%08x\n", first);
}

static void test_manual_rollback(void)
{
	struct fake_backend fake = { 0 };
	struct x58_rs_backend hooks = backend(&fake);
	struct x58_rs_op write0 = mem_op(X58_RS_OP_WRITE, 0);
	struct x58_rs_op write1 = mem_op(X58_RS_OP_WRITE, 4);
	struct x58_rs_info info;
	uint32_t digest;

	x58_rs_initialize();
	fake.values[0].lo = 0xaaaaaaaa;
	fake.values[4].lo = 0xbbbbbbbb;
	write0.flags = X58_RS_FLAG_REVERSIBLE;
	write0.value.lo = 0x11111111;
	write1.flags = X58_RS_FLAG_REVERSIBLE;
	write1.value.lo = 0x22222222;
	assert(x58_rs_add(&write0) == X58_RS_OK);
	assert(x58_rs_add(&write1) == X58_RS_OK);
	digest = current_program_digest();
	assert(x58_rs_run(digest, false, &hooks) == X58_RS_ERR_NOT_SEALED);
	assert(x58_rs_seal() == X58_RS_OK);
	assert(x58_rs_run(digest ^ 1, false, &hooks) == X58_RS_ERR_DIGEST);
	assert(fake.writes == 0);
	assert(x58_rs_run(digest, false, &hooks) == X58_RS_OK);
	assert(fake.values[0].lo == 0x11111111);
	assert(fake.values[4].lo == 0x22222222);
	x58_rs_get_info(&info);
	assert(info.transaction_valid && info.rollback_available);
	assert(info.mutations == 2 && info.nonreversible_mutations == 0);
	assert(x58_rs_add(&write0) == X58_RS_ERR_TRANSACTION_PENDING);
	assert(x58_rs_rollback(info.transaction_digest ^ 1, &hooks) ==
		X58_RS_ERR_DIGEST);
	assert(x58_rs_rollback(info.transaction_digest, &hooks) == X58_RS_OK);
	assert(fake.values[0].lo == 0xaaaaaaaa);
	assert(fake.values[4].lo == 0xbbbbbbbb);
	assert(fake.writes == 4);
	assert(fake.write_targets[0] == 0);
	assert(fake.write_targets[1] == 4);
	assert(fake.write_targets[2] == 4);
	assert(fake.write_targets[3] == 0);
	x58_rs_get_info(&info);
	assert(info.transaction_valid && !info.rollback_available);
	assert(info.rollback_attempted && info.rolled_back);
	assert(x58_rs_discard(info.transaction_digest) == X58_RS_OK);
	assert(x58_rs_clear() == X58_RS_OK);
}

static void test_auto_rollback(void)
{
	struct fake_backend fake = { 0 };
	struct x58_rs_backend hooks = backend(&fake);
	struct x58_rs_op write = mem_op(X58_RS_OP_WRITE, 0);
	struct x58_rs_op check = mem_op(X58_RS_OP_ASSERT, 4);
	struct x58_rs_info info;

	x58_rs_initialize();
	fake.values[0].lo = 0xdeadbeef;
	write.flags = X58_RS_FLAG_REVERSIBLE;
	write.value.lo = 0x12345678;
	check.mask.lo = UINT32_MAX;
	check.value.lo = 1;
	assert(x58_rs_add(&write) == X58_RS_OK);
	assert(x58_rs_add(&check) == X58_RS_OK);
	assert(x58_rs_seal() == X58_RS_OK);
	assert(x58_rs_run(current_program_digest(), true, &hooks) ==
		X58_RS_ERR_ASSERT);
	assert(fake.values[0].lo == 0xdeadbeef);
	x58_rs_get_info(&info);
	assert(info.auto_rollback && info.rollback_attempted && info.rolled_back);
	assert(info.last_result == X58_RS_ERR_ASSERT);
	assert(info.rollback_result == X58_RS_OK);
	assert(x58_rs_discard(info.transaction_digest) == X58_RS_OK);
}

static void test_mask_and_readback_failure_rollback(void)
{
	struct fake_backend fake = { 0 };
	struct x58_rs_backend hooks = backend(&fake);
	struct x58_rs_op mask = mem_op(X58_RS_OP_MASK, 0);
	struct x58_rs_info info;
	struct x58_rs_trace trace;

	x58_rs_initialize();
	fake.values[0].lo = 0xffff0000;
	fake.fail_read = 2;
	mask.flags = X58_RS_FLAG_REVERSIBLE;
	mask.mask.lo = 0x00ffff00;
	mask.value.lo = 0x00001200;
	assert(x58_rs_add(&mask) == X58_RS_OK);
	assert(x58_rs_seal() == X58_RS_OK);
	assert(x58_rs_run(current_program_digest(), true, &hooks) ==
		X58_RS_ERR_READ);
	assert(fake.write_values[0].lo == 0xff001200);
	assert(fake.values[0].lo == 0xffff0000);
	assert(x58_rs_get_trace(0, &trace));
	assert(trace.before.lo == 0xffff0000);
	assert(trace.status == X58_RS_TRACE_READ_FAILED);
	assert(trace.write_performed);
	assert(trace.rollback_status == X58_RS_TRACE_ROLLBACK_DONE);
	x58_rs_get_info(&info);
	assert(info.rollback_attempted && info.rolled_back);
	assert(x58_rs_discard(info.transaction_digest) == X58_RS_OK);
}

static void test_rollback_failure_is_not_success(void)
{
	struct fake_backend fake = { 0 };
	struct x58_rs_backend hooks = backend(&fake);
	struct x58_rs_op write = mem_op(X58_RS_OP_WRITE, 0);
	struct x58_rs_info info;

	x58_rs_initialize();
	fake.values[0].lo = 0xaaaaaaaa;
	write.flags = X58_RS_FLAG_REVERSIBLE;
	write.value.lo = 0x55555555;
	assert(x58_rs_add(&write) == X58_RS_OK);
	assert(x58_rs_seal() == X58_RS_OK);
	assert(x58_rs_run(current_program_digest(), false, &hooks) == X58_RS_OK);
	x58_rs_get_info(&info);
	fake.fail_write = fake.writes + 1;
	assert(x58_rs_rollback(info.transaction_digest, &hooks) ==
		X58_RS_ERR_ROLLBACK);
	x58_rs_get_info(&info);
	assert(info.transaction_valid && info.rollback_available);
	assert(info.rollback_attempted && !info.rolled_back);
	assert(info.rollback_attempts == 1);
	assert(info.rollback_result == X58_RS_ERR_ROLLBACK);
	assert(fake.values[0].lo == 0x55555555);
	fake.fail_write = 0;
	assert(x58_rs_rollback(info.transaction_digest, &hooks) == X58_RS_OK);
	x58_rs_get_info(&info);
	assert(!info.rollback_available && info.rolled_back);
	assert(info.rollback_attempts == 2);
	assert(fake.values[0].lo == 0xaaaaaaaa);
	assert(x58_rs_discard(info.transaction_digest) == X58_RS_OK);
}

static void test_nonreversible_and_poll_bound(void)
{
	struct fake_backend fake = { 0 };
	struct x58_rs_backend hooks = backend(&fake);
	struct x58_rs_op write = mem_op(X58_RS_OP_WRITE, 0);
	struct x58_rs_op poll = mem_op(X58_RS_OP_POLL, 4);
	struct x58_rs_info info;

	x58_rs_initialize();
	write.value.lo = 0x55;
	assert(x58_rs_add(&write) == X58_RS_OK);
	assert(x58_rs_seal() == X58_RS_OK);
	assert(x58_rs_run(current_program_digest(), true, &hooks) ==
		X58_RS_ERR_NOT_REVERSIBLE);
	assert(fake.writes == 0);
	assert(x58_rs_run(current_program_digest(), false, &hooks) == X58_RS_OK);
	x58_rs_get_info(&info);
	assert(info.nonreversible_mutations == 1 && !info.rollback_available);
	assert(x58_rs_rollback(info.transaction_digest, &hooks) ==
		X58_RS_ERR_NOT_REVERSIBLE);
	assert(x58_rs_discard(info.transaction_digest) == X58_RS_OK);

	assert(x58_rs_clear() == X58_RS_OK);
	poll.mask.lo = 1;
	poll.value.lo = 1;
	poll.limit = 3;
	fake.reads = 0;
	assert(x58_rs_add(&poll) == X58_RS_OK);
	assert(x58_rs_seal() == X58_RS_OK);
	assert(x58_rs_run(current_program_digest(), false, &hooks) ==
		X58_RS_ERR_POLL_TIMEOUT);
	assert(fake.reads == 3);
	assert(x58_rs_discard(current_transaction_digest()) == X58_RS_OK);
}

static void test_delay_and_event_failure(void)
{
	struct fake_backend fake = { 0 };
	struct x58_rs_backend hooks = backend(&fake);
	struct x58_rs_op delay = {
		.kind = X58_RS_OP_DELAY,
		.space = X58_RS_SPACE_NONE,
		.limit = 7,
	};
	struct x58_rs_info info;

	x58_rs_initialize();
	assert(x58_rs_add(&delay) == X58_RS_OK);
	assert(x58_rs_seal() == X58_RS_OK);
	assert(x58_rs_run(current_program_digest(), false, &hooks) == X58_RS_OK);
	assert(fake.delays == 7);
	x58_rs_get_info(&info);
	assert(x58_rs_discard(info.transaction_digest) == X58_RS_OK);

	fake.fail_event = fake.events + 1;
	assert(x58_rs_run(current_program_digest(), false, &hooks) ==
		X58_RS_ERR_EVENT);
	assert(fake.delays == 7);
	x58_rs_get_info(&info);
	assert(info.trace_count == 1 && info.last_result == X58_RS_ERR_EVENT);
	assert(x58_rs_discard(info.transaction_digest) == X58_RS_OK);
}

static void test_operation_limit(void)
{
	struct x58_rs_op read = mem_op(X58_RS_OP_READ, 0);

	x58_rs_initialize();
	for (uint32_t i = 0; i < X58_RS_MAX_OPS; i++) {
		read.target = i * 4;
		assert(x58_rs_add(&read) == X58_RS_OK);
	}
	read.target = X58_RS_MAX_OPS * 4;
	assert(x58_rs_add(&read) == X58_RS_ERR_FULL);
	assert(x58_rs_seal() == X58_RS_OK);
	assert(x58_rs_clear() == X58_RS_OK);
	assert(current_program_digest() == 0);
}

int main(void)
{
	x58_rs_initialize();
	test_validation_and_stable_digest();
	test_manual_rollback();
	test_auto_rollback();
	test_mask_and_readback_failure_rollback();
	test_rollback_failure_is_not_success();
	test_nonreversible_and_poll_bound();
	test_delay_and_event_failure();
	test_operation_limit();
	puts("x58 ROMMON script engine tests: PASS");
	return 0;
}
