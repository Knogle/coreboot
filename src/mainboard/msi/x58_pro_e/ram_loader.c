/* SPDX-License-Identifier: GPL-2.0-only */

#include "ram_loader.h"

#define X58_RL_CRC32_INITIAL	0xffffffffu
#define X58_RL_CRC32_POLYNOMIAL	0xedb88320u

_Static_assert(sizeof(struct x58_rl_exec_context_v1) == 32,
	"X58 RAM-loader v1 execute context must remain 32 bytes");

static uint16_t get_le16(const uint8_t *bytes)
{
	return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static uint32_t get_le32(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
		((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static void put_le16(uint8_t *bytes, uint16_t value)
{
	bytes[0] = value;
	bytes[1] = value >> 8;
}

static void put_le32(uint8_t *bytes, uint32_t value)
{
	bytes[0] = value;
	bytes[1] = value >> 8;
	bytes[2] = value >> 16;
	bytes[3] = value >> 24;
}

static void bytes_clear(void *buffer, size_t size)
{
	uint8_t *bytes = buffer;

	while (size--)
		*bytes++ = 0;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t size)
{
	while (size--) {
		crc ^= *data++;
		for (unsigned int bit = 0; bit < 8; bit++)
			crc = (crc >> 1) ^
				((0u - (crc & 1u)) & X58_RL_CRC32_POLYNOMIAL);
	}

	return crc;
}

uint32_t x58_rl_crc32(const void *data, size_t size)
{
	return crc32_update(X58_RL_CRC32_INITIAL, data, size) ^ UINT32_MAX;
}

void x58_rl_initialize(struct x58_rl_state *state)
{
	if (state != NULL)
		bytes_clear(state, sizeof(*state));
}

void x58_rl_encode_header(uint8_t wire[X58_RL_HEADER_SIZE],
			  const struct x58_rl_header *header)
{
	bytes_clear(wire, X58_RL_HEADER_SIZE);
	put_le32(wire + 0, X58_RL_HEADER_MAGIC);
	wire[4] = X58_RL_VERSION;
	wire[5] = X58_RL_HEADER_SIZE;
	put_le16(wire + 6, header->flags);
	put_le32(wire + 8, header->object_id);
	put_le32(wire + 12, header->destination);
	put_le32(wire + 16, header->length);
	put_le32(wire + 20, header->entry_offset);
	put_le32(wire + 24, header->payload_crc32);
	put_le32(wire + 28, x58_rl_crc32(wire, 28));
}

enum x58_rl_result x58_rl_decode_header(
	const uint8_t wire[X58_RL_HEADER_SIZE], struct x58_rl_header *header)
{
	if (wire == NULL || header == NULL)
		return X58_RL_ERR_BAD_HEADER_SIZE;
	header->magic = get_le32(wire + 0);
	header->version = wire[4];
	header->header_size = wire[5];
	header->flags = get_le16(wire + 6);
	header->object_id = get_le32(wire + 8);
	header->destination = get_le32(wire + 12);
	header->length = get_le32(wire + 16);
	header->entry_offset = get_le32(wire + 20);
	header->payload_crc32 = get_le32(wire + 24);
	header->header_crc32 = get_le32(wire + 28);

	if (header->magic != X58_RL_HEADER_MAGIC)
		return X58_RL_ERR_BAD_MAGIC;
	if (header->version != X58_RL_VERSION)
		return X58_RL_ERR_BAD_VERSION;
	if (header->header_size != X58_RL_HEADER_SIZE)
		return X58_RL_ERR_BAD_HEADER_SIZE;
	if (header->header_crc32 != x58_rl_crc32(wire, 28))
		return X58_RL_ERR_HEADER_CRC;

	return X58_RL_OK;
}

static enum x58_rl_result validate_header(const struct x58_rl_header *header,
	const struct x58_rl_policy *policy, const struct x58_rl_sink *sink)
{
	const uint16_t allowed_flags = X58_RL_FLAG_READBACK_VERIFY |
		X58_RL_FLAG_EXECUTABLE;
	uint32_t staging_end;
	uint32_t object_end;

	if (policy == NULL || sink == NULL || sink->write == NULL)
		return X58_RL_ERR_SINK;
	if (header->flags & ~allowed_flags)
		return X58_RL_ERR_FLAGS;
	if ((header->flags & X58_RL_FLAG_EXECUTABLE) &&
	    !(header->flags & X58_RL_FLAG_READBACK_VERIFY))
		return X58_RL_ERR_FLAGS;
	if (policy->require_readback_verify &&
	    !(header->flags & X58_RL_FLAG_READBACK_VERIFY))
		return X58_RL_ERR_FLAGS;
	if ((header->flags & X58_RL_FLAG_READBACK_VERIFY) && sink->read == NULL)
		return X58_RL_ERR_SINK;
	if (header->object_id == 0)
		return X58_RL_ERR_OBJECT_ID;
	if (header->length == 0 || policy->max_object_size == 0 ||
	    header->length > policy->max_object_size ||
	    header->length > X58_RL_ABSOLUTE_MAX_SIZE)
		return X58_RL_ERR_SIZE;
	if ((header->destination & (X58_RL_DEST_ALIGNMENT - 1)) != 0)
		return X58_RL_ERR_ALIGNMENT;
	if (policy->staging_size == 0 ||
	    policy->staging_base > UINT32_MAX - policy->staging_size)
		return X58_RL_ERR_RANGE;
	staging_end = policy->staging_base + policy->staging_size;
	if (header->destination > UINT32_MAX - header->length)
		return X58_RL_ERR_RANGE;
	object_end = header->destination + header->length;
	if (header->destination < policy->staging_base ||
	    object_end > staging_end)
		return X58_RL_ERR_RANGE;
	if (header->flags & X58_RL_FLAG_EXECUTABLE) {
		if (header->entry_offset >= header->length)
			return X58_RL_ERR_ENTRY;
	} else if (header->entry_offset != 0) {
		return X58_RL_ERR_ENTRY;
	}

	return X58_RL_OK;
}

void x58_rl_abort(struct x58_rl_state *state, enum x58_rl_result result)
{
	if (state == NULL)
		return;
	state->receiving = false;
	state->loaded = false;
	state->execute_armed = false;
	state->last_result = result;
}

enum x58_rl_result x58_rl_begin(struct x58_rl_state *state,
	const uint8_t wire[X58_RL_HEADER_SIZE], const struct x58_rl_policy *policy,
	const struct x58_rl_sink *sink)
{
	enum x58_rl_result result;
	struct x58_rl_header header;

	if (state == NULL)
		return X58_RL_ERR_BUSY;
	if (state->receiving)
		return X58_RL_ERR_BUSY;
	/* Any attempted replacement invalidates the old execute capability. */
	bytes_clear(&state->header, sizeof(state->header));
	state->received = 0;
	state->running_crc32 = 0;
	state->last_result = X58_RL_OK;
	state->loaded = false;
	state->execute_armed = false;
	result = x58_rl_decode_header(wire, &header);
	if (result == X58_RL_OK)
		result = validate_header(&header, policy, sink);
	if (result != X58_RL_OK) {
		x58_rl_abort(state, result);
		return result;
	}
	state->header = header;
	state->received = 0;
	state->running_crc32 = X58_RL_CRC32_INITIAL;
	state->last_result = X58_RL_OK;
	state->receiving = true;

	return X58_RL_OK;
}

enum x58_rl_result x58_rl_write(struct x58_rl_state *state,
	const uint8_t *data, size_t size, const struct x58_rl_sink *sink)
{
	uint32_t address;

	if (state == NULL || !state->receiving)
		return X58_RL_ERR_INCOMPLETE;
	if (data == NULL || size == 0)
		return X58_RL_ERR_INCOMPLETE;
	if (state->received > state->header.length ||
	    size > state->header.length - state->received) {
		x58_rl_abort(state, X58_RL_ERR_OVERFLOW);
		return X58_RL_ERR_OVERFLOW;
	}
	if (sink == NULL || sink->write == NULL) {
		x58_rl_abort(state, X58_RL_ERR_SINK);
		return X58_RL_ERR_SINK;
	}
	address = state->header.destination + state->received;
	if (!sink->write(sink->context, address, data, size)) {
		x58_rl_abort(state, X58_RL_ERR_SINK);
		return X58_RL_ERR_SINK;
	}
	state->running_crc32 = crc32_update(state->running_crc32, data, size);
	state->received += size;

	return X58_RL_OK;
}

static enum x58_rl_result verify_loaded(const struct x58_rl_state *state,
	const struct x58_rl_sink *sink)
{
	uint8_t chunk[X58_RL_VERIFY_CHUNK_SIZE];
	uint32_t crc = X58_RL_CRC32_INITIAL;
	uint32_t offset = 0;

	if (sink == NULL || sink->read == NULL)
		return X58_RL_ERR_SINK;
	while (offset < state->header.length) {
		size_t count = state->header.length - offset;

		if (count > sizeof(chunk))
			count = sizeof(chunk);
		if (!sink->read(sink->context, state->header.destination + offset,
			       chunk, count))
			return X58_RL_ERR_SINK;
		crc = crc32_update(crc, chunk, count);
		offset += count;
	}
	if ((crc ^ UINT32_MAX) != state->header.payload_crc32)
		return X58_RL_ERR_READBACK;

	return X58_RL_OK;
}

enum x58_rl_result x58_rl_finish(struct x58_rl_state *state,
	const struct x58_rl_policy *policy, const struct x58_rl_sink *sink)
{
	enum x58_rl_result result = X58_RL_OK;

	if (state == NULL || !state->receiving)
		return X58_RL_ERR_INCOMPLETE;
	if (state->received != state->header.length)
		result = X58_RL_ERR_INCOMPLETE;
	else if ((state->running_crc32 ^ UINT32_MAX) !=
		 state->header.payload_crc32)
		result = X58_RL_ERR_PAYLOAD_CRC;
	else if ((state->header.flags & X58_RL_FLAG_READBACK_VERIFY) ||
		 (policy != NULL && policy->require_readback_verify))
		result = verify_loaded(state, sink);
	state->receiving = false;
	state->loaded = result == X58_RL_OK;
	state->execute_armed = false;
	state->last_result = result;

	return result;
}

bool x58_rl_encode_status(uint8_t wire[X58_RL_STATUS_SIZE], uint8_t code,
	uint32_t object_id, uint32_t detail)
{
	if (wire == NULL)
		return false;
	bytes_clear(wire, X58_RL_STATUS_SIZE);
	put_le32(wire + 0, X58_RL_STATUS_MAGIC);
	wire[4] = X58_RL_VERSION;
	wire[5] = code;
	put_le32(wire + 8, object_id);
	put_le32(wire + 12, detail);
	put_le32(wire + 16, x58_rl_crc32(wire, 16));

	return true;
}

static bool send_status(const struct x58_rl_serial_io *io, uint8_t code,
	uint32_t object_id, uint32_t detail)
{
	uint8_t wire[X58_RL_STATUS_SIZE];

	if (io == NULL || io->transmit == NULL ||
	    !x58_rl_encode_status(wire, code, object_id, detail))
		return false;
	for (size_t i = 0; i < sizeof(wire); i++) {
		if (!io->transmit(io->context, wire[i]))
			return false;
	}

	return true;
}

static enum x58_rl_result receive_exact(const struct x58_rl_serial_io *io,
	uint8_t *buffer, size_t size, uint32_t poll_limit)
{
	if (io == NULL || io->receive == NULL || poll_limit == 0)
		return X58_RL_ERR_RX_TIMEOUT;
	for (size_t i = 0; i < size; i++) {
		const enum x58_rl_io_result io_result =
			io->receive(io->context, &buffer[i], poll_limit);

		if (io_result == X58_RL_IO_TIMEOUT)
			return X58_RL_ERR_RX_TIMEOUT;
		if (io_result != X58_RL_IO_BYTE)
			return X58_RL_ERR_RX_FAULT;
	}

	return X58_RL_OK;
}

static uint8_t nak_code(enum x58_rl_result result)
{
	uint32_t code = X58_RL_STATUS_NAK_BASE + (uint32_t)result;

	return code <= UINT8_MAX ? code : UINT8_MAX;
}

enum x58_rl_result x58_rl_receive_serial(struct x58_rl_state *state,
	const struct x58_rl_policy *policy, const struct x58_rl_sink *sink,
	const struct x58_rl_serial_io *io,
	const struct x58_rl_serial_limits *limits)
{
	uint8_t wire[X58_RL_HEADER_SIZE];
	uint8_t byte;
	enum x58_rl_result result;
	uint32_t object_id = 0;

	if (state == NULL)
		return X58_RL_ERR_SINK;
	if (state->receiving)
		return X58_RL_ERR_BUSY;
	/*
	 * Entering a new receive operation is itself a replacement attempt.  Do
	 * this before READY so even an output failure cannot leave an older object
	 * armed, and so a pre-header NAK never reports a stale byte count.
	 */
	bytes_clear(&state->header, sizeof(state->header));
	state->received = 0;
	state->running_crc32 = 0;
	state->last_result = X58_RL_OK;
	state->loaded = false;
	state->execute_armed = false;
	if (limits == NULL || io == NULL || io->receive == NULL ||
	    io->transmit == NULL) {
		x58_rl_abort(state, X58_RL_ERR_SINK);
		return X58_RL_ERR_SINK;
	}
	if (!send_status(io, X58_RL_STATUS_READY, 0,
			policy != NULL ? policy->max_object_size : 0)) {
		x58_rl_abort(state, X58_RL_ERR_TX);
		return X58_RL_ERR_TX;
	}
	result = receive_exact(io, wire, sizeof(wire),
			       limits->header_byte_poll_limit);
	if (result != X58_RL_OK)
		goto fail;
	object_id = get_le32(wire + 8);
	result = x58_rl_begin(state, wire, policy, sink);
	if (result != X58_RL_OK)
		goto fail;
	if (!send_status(io, X58_RL_STATUS_HEADER_ACK,
			state->header.object_id, state->header.length)) {
		result = X58_RL_ERR_TX;
		goto fail;
	}
	while (state->received < state->header.length) {
		result = receive_exact(io, &byte, 1,
				       limits->payload_byte_poll_limit);
		if (result != X58_RL_OK)
			goto fail;
		result = x58_rl_write(state, &byte, 1, sink);
		if (result != X58_RL_OK)
			goto fail;
	}
	result = x58_rl_finish(state, policy, sink);
	if (result != X58_RL_OK)
		goto fail;
	if (!send_status(io, X58_RL_STATUS_COMPLETE_ACK,
			state->header.object_id, state->header.payload_crc32)) {
		x58_rl_abort(state, X58_RL_ERR_TX);
		return X58_RL_ERR_TX;
	}

	return X58_RL_OK;

fail:
	x58_rl_abort(state, result);
	if (!send_status(io, nak_code(result), object_id,
			state != NULL ? state->received : 0))
		return X58_RL_ERR_TX;
	return result;
}

enum x58_rl_result x58_rl_arm_execute(struct x58_rl_state *state,
	uint32_t object_id, uint32_t payload_crc32)
{
	if (state == NULL || !state->loaded)
		return X58_RL_ERR_NOT_LOADED;
	state->execute_armed = false;
	if (!(state->header.flags & X58_RL_FLAG_EXECUTABLE))
		return X58_RL_ERR_NOT_EXECUTABLE;
	if (!(state->header.flags & X58_RL_FLAG_READBACK_VERIFY))
		return X58_RL_ERR_NOT_EXECUTABLE;
	if (state->header.object_id != object_id ||
	    state->header.payload_crc32 != payload_crc32)
		return X58_RL_ERR_EXEC_MISMATCH;
	state->execute_armed = true;

	return X58_RL_OK;
}

enum x58_rl_result x58_rl_prepare_execute(struct x58_rl_state *state,
	uint32_t object_id, uint32_t payload_crc32,
	const struct x58_rl_sink *sink, uint32_t *entry_address,
	struct x58_rl_exec_context_v1 *context)
{
	enum x58_rl_result result;

	if (state == NULL || !state->loaded)
		return X58_RL_ERR_NOT_LOADED;
	if (!state->execute_armed)
		return X58_RL_ERR_EXEC_NOT_ARMED;
	/* Consume the capability before any validation or payload-controlled call. */
	state->execute_armed = false;
	if (state->header.object_id != object_id ||
	    state->header.payload_crc32 != payload_crc32)
		return X58_RL_ERR_EXEC_MISMATCH;
	if (!(state->header.flags & X58_RL_FLAG_EXECUTABLE))
		return X58_RL_ERR_NOT_EXECUTABLE;
	result = verify_loaded(state, sink);
	if (result != X58_RL_OK) {
		state->loaded = false;
		state->last_result = result;
		return result;
	}
	if (entry_address == NULL || context == NULL)
		return X58_RL_ERR_EXEC_MISMATCH;
	*entry_address = state->header.destination + state->header.entry_offset;
	context->magic = X58_RL_EXEC_CONTEXT_MAGIC;
	context->abi_version = X58_RL_EXEC_ABI_VERSION;
	context->structure_size = sizeof(*context);
	context->object_id = state->header.object_id;
	context->image_base = state->header.destination;
	context->image_length = state->header.length;
	context->entry_offset = state->header.entry_offset;
	context->payload_crc32 = state->header.payload_crc32;
	context->service_table = 0;

	return X58_RL_OK;
}

void x58_rl_cancel_execute(struct x58_rl_state *state)
{
	if (state != NULL)
		state->execute_armed = false;
}

const char *x58_rl_result_name(enum x58_rl_result result)
{
	static const char *const names[] = {
		"ok", "busy", "bad-magic", "bad-version", "bad-header-size",
		"header-crc", "flags", "object-id", "size", "alignment",
		"range", "entry", "sink", "overflow", "incomplete",
		"payload-crc", "readback", "not-loaded", "not-executable",
		"execute-mismatch", "execute-not-armed", "rx-timeout",
		"rx-fault", "tx",
	};

	return result >= X58_RL_OK && result <= X58_RL_ERR_TX ? names[result] :
		"invalid-result";
}
