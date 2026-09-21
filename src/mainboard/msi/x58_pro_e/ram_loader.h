/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_RAM_LOADER_H
#define MAINBOARD_MSI_X58_PRO_E_RAM_LOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Little-endian wire magic: "XRL1" and "XRA1". */
#define X58_RL_HEADER_MAGIC		0x314c5258u
#define X58_RL_STATUS_MAGIC		0x31415258u
#define X58_RL_VERSION			1u
#define X58_RL_HEADER_SIZE		32u
#define X58_RL_STATUS_SIZE		20u
#define X58_RL_DEST_ALIGNMENT		16u
#define X58_RL_VERIFY_CHUNK_SIZE	64u
#define X58_RL_ABSOLUTE_MAX_SIZE	(4u * 1024u * 1024u)

enum x58_rl_flags {
	X58_RL_FLAG_READBACK_VERIFY = 1u << 0,
	X58_RL_FLAG_EXECUTABLE = 1u << 1,
};

struct x58_rl_header {
	uint32_t magic;
	uint8_t version;
	uint8_t header_size;
	uint16_t flags;
	uint32_t object_id;
	uint32_t destination;
	uint32_t length;
	uint32_t entry_offset;
	uint32_t payload_crc32;
	uint32_t header_crc32;
};

struct x58_rl_policy {
	uint32_t staging_base;
	uint32_t staging_size;
	uint32_t max_object_size;
	bool require_readback_verify;
};

struct x58_rl_sink {
	void *context;
	bool (*write)(void *context, uint32_t address, const uint8_t *data,
		      size_t size);
	bool (*read)(void *context, uint32_t address, uint8_t *data,
		     size_t size);
};

enum x58_rl_io_result {
	X58_RL_IO_BYTE,
	X58_RL_IO_TIMEOUT,
	X58_RL_IO_FAULT,
};

struct x58_rl_serial_io {
	void *context;
	enum x58_rl_io_result (*receive)(void *context, uint8_t *value,
					 uint32_t poll_limit);
	bool (*transmit)(void *context, uint8_t value);
};

struct x58_rl_serial_limits {
	uint32_t header_byte_poll_limit;
	uint32_t payload_byte_poll_limit;
};

enum x58_rl_result {
	X58_RL_OK,
	X58_RL_ERR_BUSY,
	X58_RL_ERR_BAD_MAGIC,
	X58_RL_ERR_BAD_VERSION,
	X58_RL_ERR_BAD_HEADER_SIZE,
	X58_RL_ERR_HEADER_CRC,
	X58_RL_ERR_FLAGS,
	X58_RL_ERR_OBJECT_ID,
	X58_RL_ERR_SIZE,
	X58_RL_ERR_ALIGNMENT,
	X58_RL_ERR_RANGE,
	X58_RL_ERR_ENTRY,
	X58_RL_ERR_SINK,
	X58_RL_ERR_OVERFLOW,
	X58_RL_ERR_INCOMPLETE,
	X58_RL_ERR_PAYLOAD_CRC,
	X58_RL_ERR_READBACK,
	X58_RL_ERR_NOT_LOADED,
	X58_RL_ERR_NOT_EXECUTABLE,
	X58_RL_ERR_EXEC_MISMATCH,
	X58_RL_ERR_EXEC_NOT_ARMED,
	X58_RL_ERR_RX_TIMEOUT,
	X58_RL_ERR_RX_FAULT,
	X58_RL_ERR_TX,
};

enum x58_rl_status_code {
	X58_RL_STATUS_READY = 0x10,
	X58_RL_STATUS_HEADER_ACK = 0x11,
	X58_RL_STATUS_COMPLETE_ACK = 0x12,
	X58_RL_STATUS_NAK_BASE = 0x80,
};

struct x58_rl_state {
	struct x58_rl_header header;
	uint32_t received;
	uint32_t running_crc32;
	enum x58_rl_result last_result;
	bool receiving;
	bool loaded;
	bool execute_armed;
	bool reserved;
};

/*
 * ABI for an explicitly armed raw 32-bit payload.  The entry point is called
 * in 32-bit protected mode using cdecl as:
 *
 *   uint32_t entry(const struct x58_rl_exec_context_v1 *context);
 *
 * It runs at firmware privilege, on the caller's current stack, and may
 * return EAX to ROMMON.  Interrupt, cache, paging and hardware state are not
 * changed by the loader.  service_table is zero until an independently
 * versioned service ABI is implemented.
 */
#define X58_RL_EXEC_CONTEXT_MAGIC	0x31435258u /* "XRC1" */
#define X58_RL_EXEC_ABI_VERSION		1u

struct x58_rl_exec_context_v1 {
	uint32_t magic;
	uint16_t abi_version;
	uint16_t structure_size;
	uint32_t object_id;
	uint32_t image_base;
	uint32_t image_length;
	uint32_t entry_offset;
	uint32_t payload_crc32;
	uint32_t service_table;
};

void x58_rl_initialize(struct x58_rl_state *state);
uint32_t x58_rl_crc32(const void *data, size_t size);
void x58_rl_encode_header(uint8_t wire[X58_RL_HEADER_SIZE],
			  const struct x58_rl_header *header);
enum x58_rl_result x58_rl_decode_header(
	const uint8_t wire[X58_RL_HEADER_SIZE], struct x58_rl_header *header);

/* Transport-neutral streaming object sink used by UART now and TFTP later. */
enum x58_rl_result x58_rl_begin(struct x58_rl_state *state,
	const uint8_t wire[X58_RL_HEADER_SIZE], const struct x58_rl_policy *policy,
	const struct x58_rl_sink *sink);
enum x58_rl_result x58_rl_write(struct x58_rl_state *state,
	const uint8_t *data, size_t size, const struct x58_rl_sink *sink);
enum x58_rl_result x58_rl_finish(struct x58_rl_state *state,
	const struct x58_rl_policy *policy, const struct x58_rl_sink *sink);
void x58_rl_abort(struct x58_rl_state *state, enum x58_rl_result result);

/* Fixed framed UART adapter.  It never allocates and every wait is bounded. */
enum x58_rl_result x58_rl_receive_serial(struct x58_rl_state *state,
	const struct x58_rl_policy *policy, const struct x58_rl_sink *sink,
	const struct x58_rl_serial_io *io,
	const struct x58_rl_serial_limits *limits);

/* Two distinct calls are mandatory before a raw entry address is released. */
enum x58_rl_result x58_rl_arm_execute(struct x58_rl_state *state,
	uint32_t object_id, uint32_t payload_crc32);
enum x58_rl_result x58_rl_prepare_execute(struct x58_rl_state *state,
	uint32_t object_id, uint32_t payload_crc32,
	const struct x58_rl_sink *sink, uint32_t *entry_address,
	struct x58_rl_exec_context_v1 *context);
void x58_rl_cancel_execute(struct x58_rl_state *state);

bool x58_rl_encode_status(uint8_t wire[X58_RL_STATUS_SIZE], uint8_t code,
	uint32_t object_id, uint32_t detail);
const char *x58_rl_result_name(enum x58_rl_result result);

#endif
