/* SPDX-License-Identifier: GPL-2.0-only */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ram_loader.h"

#define TEST_BASE	0x02000000u
#define TEST_SIZE	0x00010000u

static uint8_t memory[TEST_SIZE];
static unsigned int failures;

#define CHECK(expression) do { \
	if (!(expression)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, \
			#expression); \
		failures++; \
	} \
} while (0)

struct sink_context {
	bool fail_write;
	bool fail_read;
};

static bool sink_write(void *context, uint32_t address, const uint8_t *data,
		       size_t size)
{
	struct sink_context *sink = context;

	if (sink->fail_write || address < TEST_BASE ||
	    size > TEST_SIZE - (address - TEST_BASE))
		return false;
	memcpy(memory + address - TEST_BASE, data, size);
	return true;
}

static bool sink_read(void *context, uint32_t address, uint8_t *data,
		      size_t size)
{
	struct sink_context *sink = context;

	if (sink->fail_read || address < TEST_BASE ||
	    size > TEST_SIZE - (address - TEST_BASE))
		return false;
	memcpy(data, memory + address - TEST_BASE, size);
	return true;
}

static struct x58_rl_policy policy(void)
{
	const struct x58_rl_policy result = {
		.staging_base = TEST_BASE,
		.staging_size = TEST_SIZE,
		.max_object_size = 0x8000,
		.require_readback_verify = true,
	};

	return result;
}

static struct x58_rl_header header_for(const uint8_t *payload, size_t size,
				       uint16_t flags)
{
	const struct x58_rl_header result = {
		.flags = flags,
		.object_id = 0x12345678,
		.destination = TEST_BASE + 0x100,
		.length = size,
		.entry_offset = flags & X58_RL_FLAG_EXECUTABLE ? 4 : 0,
		.payload_crc32 = x58_rl_crc32(payload, size),
	};

	return result;
}

static void encode(uint8_t wire[X58_RL_HEADER_SIZE],
		   const struct x58_rl_header *header)
{
	x58_rl_encode_header(wire, header);
}

static void test_crc_and_header(void)
{
	static const uint8_t check[] = "123456789";
	static const uint8_t payload[] = { 1, 2, 3, 4, 5 };
	struct x58_rl_header decoded;
	struct x58_rl_header header = header_for(payload, sizeof(payload),
		X58_RL_FLAG_READBACK_VERIFY);
	uint8_t wire[X58_RL_HEADER_SIZE];

	CHECK(x58_rl_crc32(check, sizeof(check) - 1) == 0xcbf43926);
	encode(wire, &header);
	CHECK(x58_rl_decode_header(wire, &decoded) == X58_RL_OK);
	CHECK(decoded.object_id == header.object_id);
	CHECK(decoded.destination == header.destination);
	CHECK(decoded.length == header.length);
	CHECK(decoded.payload_crc32 == header.payload_crc32);
	wire[7] ^= 0x80;
	CHECK(x58_rl_decode_header(wire, &decoded) == X58_RL_ERR_HEADER_CRC);
	wire[7] ^= 0x80;
	wire[0] ^= 1;
	CHECK(x58_rl_decode_header(wire, &decoded) == X58_RL_ERR_BAD_MAGIC);
}

static void test_stream_and_range(void)
{
	static const uint8_t payload[] = { 9, 8, 7, 6, 5, 4, 3, 2, 1 };
	struct sink_context sink_context = { 0 };
	const struct x58_rl_sink sink = {
		.context = &sink_context,
		.write = sink_write,
		.read = sink_read,
	};
	struct x58_rl_policy current_policy = policy();
	struct x58_rl_header header = header_for(payload, sizeof(payload),
		X58_RL_FLAG_READBACK_VERIFY);
	struct x58_rl_state state;
	uint8_t wire[X58_RL_HEADER_SIZE];

	x58_rl_initialize(&state);
	encode(wire, &header);
	CHECK(x58_rl_begin(&state, wire, &current_policy, &sink) == X58_RL_OK);
	CHECK(x58_rl_write(&state, payload, 3, &sink) == X58_RL_OK);
	CHECK(x58_rl_write(&state, payload + 3, sizeof(payload) - 3, &sink) ==
		X58_RL_OK);
	CHECK(x58_rl_finish(&state, &current_policy, &sink) == X58_RL_OK);
	CHECK(state.loaded);
	CHECK(memcmp(memory + 0x100, payload, sizeof(payload)) == 0);

	/* A malformed replacement invalidates the previously loaded capability. */
	header.destination = TEST_BASE + TEST_SIZE;
	encode(wire, &header);
	CHECK(x58_rl_begin(&state, wire, &current_policy, &sink) == X58_RL_ERR_RANGE);
	CHECK(!state.loaded);
	CHECK(!state.execute_armed);
	CHECK(state.received == 0);
	CHECK(state.header.object_id == 0);
	header.destination = TEST_BASE + 3;
	encode(wire, &header);
	CHECK(x58_rl_begin(&state, wire, &current_policy, &sink) ==
		X58_RL_ERR_ALIGNMENT);
	header.destination = TEST_BASE + 0x100;
	header.object_id = 0;
	encode(wire, &header);
	CHECK(x58_rl_begin(&state, wire, &current_policy, &sink) ==
		X58_RL_ERR_OBJECT_ID);
	header.object_id = 1;
	header.flags = X58_RL_FLAG_EXECUTABLE;
	header.entry_offset = 0;
	encode(wire, &header);
	CHECK(x58_rl_begin(&state, wire, &current_policy, &sink) == X58_RL_ERR_FLAGS);
}

static void test_failures(void)
{
	static const uint8_t payload[] = { 0xaa, 0xbb, 0xcc, 0xdd };
	struct sink_context sink_context = { 0 };
	const struct x58_rl_sink sink = {
		.context = &sink_context,
		.write = sink_write,
		.read = sink_read,
	};
	struct x58_rl_policy current_policy = policy();
	struct x58_rl_header header = header_for(payload, sizeof(payload),
		X58_RL_FLAG_READBACK_VERIFY);
	struct x58_rl_state state;
	uint8_t wire[X58_RL_HEADER_SIZE];
	uint8_t extra[5] = { 0 };

	x58_rl_initialize(&state);
	encode(wire, &header);
	CHECK(x58_rl_begin(&state, wire, &current_policy, &sink) == X58_RL_OK);
	CHECK(x58_rl_finish(&state, &current_policy, &sink) ==
		X58_RL_ERR_INCOMPLETE);
	CHECK(!state.loaded);

	CHECK(x58_rl_begin(&state, wire, &current_policy, &sink) == X58_RL_OK);
	CHECK(x58_rl_write(&state, extra, sizeof(extra), &sink) ==
		X58_RL_ERR_OVERFLOW);

	/* Corrupt state must fail closed without an unsigned subtraction wrap. */
	CHECK(x58_rl_begin(&state, wire, &current_policy, &sink) == X58_RL_OK);
	state.received = state.header.length + 1;
	CHECK(x58_rl_write(&state, payload, 1, &sink) ==
		X58_RL_ERR_OVERFLOW);
	CHECK(!state.loaded);
	CHECK(!state.execute_armed);

	CHECK(x58_rl_begin(&state, wire, &current_policy, &sink) == X58_RL_OK);
	CHECK(x58_rl_write(&state, payload, sizeof(payload), &sink) == X58_RL_OK);
	memory[0x100] ^= 1;
	CHECK(x58_rl_finish(&state, &current_policy, &sink) ==
		X58_RL_ERR_READBACK);
	memory[0x100] ^= 1;

	header.payload_crc32 ^= 1;
	encode(wire, &header);
	CHECK(x58_rl_begin(&state, wire, &current_policy, &sink) == X58_RL_OK);
	CHECK(x58_rl_write(&state, payload, sizeof(payload), &sink) == X58_RL_OK);
	CHECK(x58_rl_finish(&state, &current_policy, &sink) ==
		X58_RL_ERR_PAYLOAD_CRC);

	header.payload_crc32 ^= 1;
	encode(wire, &header);
	sink_context.fail_write = true;
	CHECK(x58_rl_begin(&state, wire, &current_policy, &sink) == X58_RL_OK);
	CHECK(x58_rl_write(&state, payload, sizeof(payload), &sink) ==
		X58_RL_ERR_SINK);
}

struct serial_context {
	uint8_t input[256];
	size_t input_size;
	size_t input_offset;
	uint8_t output[256];
	size_t output_size;
	bool fault;
	bool fail_tx;
};

static enum x58_rl_io_result serial_receive(void *context, uint8_t *value,
					    uint32_t poll_limit)
{
	struct serial_context *serial = context;

	if (poll_limit == 0 || serial->input_offset >= serial->input_size)
		return X58_RL_IO_TIMEOUT;
	if (serial->fault)
		return X58_RL_IO_FAULT;
	*value = serial->input[serial->input_offset++];
	return X58_RL_IO_BYTE;
}

static bool serial_transmit(void *context, uint8_t value)
{
	struct serial_context *serial = context;

	if (serial->fail_tx || serial->output_size >= sizeof(serial->output))
		return false;
	serial->output[serial->output_size++] = value;
	return true;
}

static uint32_t le32(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
		((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static void test_serial(void)
{
	static const uint8_t payload[] = { 0x10, 0x20, 0x30, 0x40, 0x50 };
	struct sink_context sink_context = { 0 };
	const struct x58_rl_sink sink = {
		.context = &sink_context,
		.write = sink_write,
		.read = sink_read,
	};
	const struct x58_rl_policy current_policy = policy();
	const struct x58_rl_serial_limits limits = { 10, 10 };
	struct x58_rl_header header = header_for(payload, sizeof(payload),
		X58_RL_FLAG_READBACK_VERIFY);
	struct serial_context serial = { 0 };
	const struct x58_rl_serial_io io = {
		.context = &serial,
		.receive = serial_receive,
		.transmit = serial_transmit,
	};
	struct x58_rl_state state;

	encode(serial.input, &header);
	memcpy(serial.input + X58_RL_HEADER_SIZE, payload, sizeof(payload));
	serial.input_size = X58_RL_HEADER_SIZE + sizeof(payload);
	x58_rl_initialize(&state);
	CHECK(x58_rl_receive_serial(&state, &current_policy, &sink, &io, &limits) ==
		X58_RL_OK);
	CHECK(state.loaded);
	CHECK(serial.output_size == 3 * X58_RL_STATUS_SIZE);
	CHECK(serial.output[5] == X58_RL_STATUS_READY);
	CHECK(serial.output[X58_RL_STATUS_SIZE + 5] == X58_RL_STATUS_HEADER_ACK);
	CHECK(serial.output[2 * X58_RL_STATUS_SIZE + 5] ==
		X58_RL_STATUS_COMPLETE_ACK);
	for (size_t offset = 0; offset < serial.output_size;
	     offset += X58_RL_STATUS_SIZE)
		CHECK(le32(serial.output + offset + 16) ==
			x58_rl_crc32(serial.output + offset, 16));

	memset(&serial, 0, sizeof(serial));
	state.header.object_id = 0xdeadbeef;
	state.received = 0xa5a5;
	state.loaded = true;
	state.execute_armed = true;
	CHECK(x58_rl_receive_serial(&state, &current_policy, &sink, &io, &limits) ==
		X58_RL_ERR_RX_TIMEOUT);
	CHECK(serial.output_size == 2 * X58_RL_STATUS_SIZE);
	CHECK(serial.output[X58_RL_STATUS_SIZE + 5] ==
		X58_RL_STATUS_NAK_BASE + X58_RL_ERR_RX_TIMEOUT);
	CHECK(le32(serial.output + X58_RL_STATUS_SIZE + 12) == 0);
	CHECK(!state.loaded);
	CHECK(!state.execute_armed);
	CHECK(state.received == 0);

	memset(&serial, 0, sizeof(serial));
	serial.fault = true;
	serial.input_size = 1;
	x58_rl_initialize(&state);
	CHECK(x58_rl_receive_serial(&state, &current_policy, &sink, &io, &limits) ==
		X58_RL_ERR_RX_FAULT);

	memset(&serial, 0, sizeof(serial));
	serial.fail_tx = true;
	state.header.object_id = 0xdeadbeef;
	state.received = 0xa5a5;
	state.loaded = true;
	state.execute_armed = true;
	CHECK(x58_rl_receive_serial(&state, &current_policy, &sink, &io, &limits) ==
		X58_RL_ERR_TX);
	CHECK(!state.loaded);
	CHECK(!state.execute_armed);
	CHECK(state.received == 0);
	CHECK(state.header.object_id == 0);
	CHECK(state.last_result == X58_RL_ERR_TX);
}

static void test_execute_gate(void)
{
	static const uint8_t payload[] = {
		0x90, 0x90, 0x90, 0x90, 0xc3, 0x90, 0x90, 0x90,
	};
	struct sink_context sink_context = { 0 };
	const struct x58_rl_sink sink = {
		.context = &sink_context,
		.write = sink_write,
		.read = sink_read,
	};
	const struct x58_rl_policy current_policy = policy();
	struct x58_rl_header header = header_for(payload, sizeof(payload),
		X58_RL_FLAG_READBACK_VERIFY | X58_RL_FLAG_EXECUTABLE);
	struct x58_rl_exec_context_v1 context;
	struct x58_rl_state state;
	uint8_t wire[X58_RL_HEADER_SIZE];
	uint32_t entry;

	encode(wire, &header);
	x58_rl_initialize(&state);
	CHECK(x58_rl_begin(&state, wire, &current_policy, &sink) == X58_RL_OK);
	CHECK(x58_rl_write(&state, payload, sizeof(payload), &sink) == X58_RL_OK);
	CHECK(x58_rl_finish(&state, &current_policy, &sink) == X58_RL_OK);
	CHECK(x58_rl_prepare_execute(&state, header.object_id,
		header.payload_crc32, &sink, &entry, &context) ==
		X58_RL_ERR_EXEC_NOT_ARMED);
	CHECK(x58_rl_arm_execute(&state, header.object_id, header.payload_crc32) ==
		X58_RL_OK);
	CHECK(x58_rl_prepare_execute(&state, header.object_id + 1,
		header.payload_crc32, &sink, &entry, &context) ==
		X58_RL_ERR_EXEC_MISMATCH);
	CHECK(!state.execute_armed);
	CHECK(x58_rl_arm_execute(&state, header.object_id, header.payload_crc32) ==
		X58_RL_OK);
	CHECK(x58_rl_prepare_execute(&state, header.object_id,
		header.payload_crc32, &sink, &entry, &context) == X58_RL_OK);
	CHECK(entry == header.destination + header.entry_offset);
	CHECK(context.magic == X58_RL_EXEC_CONTEXT_MAGIC);
	CHECK(context.structure_size == sizeof(context));
	CHECK(context.image_base == header.destination);

	CHECK(x58_rl_arm_execute(&state, header.object_id, header.payload_crc32) ==
		X58_RL_OK);
	memory[0x100] ^= 1;
	CHECK(x58_rl_prepare_execute(&state, header.object_id,
		header.payload_crc32, &sink, &entry, &context) ==
		X58_RL_ERR_READBACK);
	CHECK(!state.loaded);
}

int main(void)
{
	test_crc_and_header();
	test_stream_and_range();
	test_failures();
	test_serial();
	test_execute_gate();

	if (failures != 0) {
		fprintf(stderr, "%u ram-loader test(s) failed\n", failures);
		return 1;
	}
	puts("x58 ram-loader tests: PASS");
	return 0;
}
