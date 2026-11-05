/* SPDX-License-Identifier: GPL-2.0-only */

#include <limits.h>
#include "dxio_early_link.h"
#include "dxio_service.h"

static bool valid_range(const void *pointer, size_t bytes)
{
	return pointer && (uintptr_t)pointer <= UINTPTR_MAX - bytes;
}

static bool overlap(const void *a, size_t a_bytes, const void *b, size_t b_bytes)
{
	return (uintptr_t)a < (uintptr_t)b + b_bytes &&
		(uintptr_t)b < (uintptr_t)a + a_bytes;
}

static uint32_t read32(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
		((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static size_t engine_count(const struct matisse_dxio_early_input *input)
{
	return (size_t)input->platform[12] | ((size_t)input->platform[13] << 8);
}

static uint8_t *early_engine(const struct matisse_dxio_early_input *input)
{
	return input->platform + MATISSE_DXIO_PLATFORM_HEADER_BYTES +
		(size_t)input->result->early_engine * MATISSE_DXIO_ENGINE_BYTES;
}

static bool available(struct matisse_dxio_early_state *state,
		      const struct matisse_dxio_early_io *io)
{
	if (stopwatch_expired(&state->budget) || state->pci_attempted >= 32 ||
	    state->request_attempted >= 4 || !io->may_access(io->context)) {
		if (!state->failure)
			state->failure = MATISSE_DXIO_EARLY_ACCESS_DENIED;
		return false;
	}
	return true;
}

static bool pci_access(struct matisse_dxio_early_state *state,
		       const struct matisse_dxio_early_io *io, uint32_t address,
		       unsigned int bytes, uint32_t *value, bool write)
{
	if (!available(state, io))
		return false;
	state->last_address = address;
	state->pci_attempted++;
	if (write) {
		state->last_value = *value;
		state->last_status = io->write(address, bytes, *value, io->context);
	} else {
		state->last_status = io->read(address, bytes, value, io->context);
		state->last_value = *value;
	}
	if (state->last_status != CB_SUCCESS) {
		if (!state->failure)
			state->failure = MATISSE_DXIO_EARLY_PCI_FAILED;
		return false;
	}
	return available(state, io);
}

static bool dma_callback(struct matisse_dxio_early_state *state,
			 const struct matisse_dxio_early_io *io, unsigned int use)
{
	if (!available(state, io))
		return false;
	state->last_status = use ? io->prepare(use, io->context) : io->consume(io->context);
	if (state->last_status != CB_SUCCESS) {
		state->failure = MATISSE_DXIO_EARLY_DMA_FAILED;
		return false;
	}
	return available(state, io);
}

static bool request(struct matisse_dxio_early_state *state,
		    const struct matisse_dxio_transport *transport,
		    const struct matisse_dxio_early_io *io, uint32_t opcode,
		    const uint32_t arguments[5])
{
	if (!available(state, io))
		return false;
	state->request_attempted++;
	state->last_status = transport->request(transport->context, opcode, arguments,
					       &state->last_reply);
	if (state->last_status != CB_SUCCESS ||
	    matisse_dxio_status(state->last_reply.msg[0]) != 3) {
		state->failure = MATISSE_DXIO_EARLY_REQUEST_FAILED;
		return false;
	}
	return available(state, io);
}

static bool valid_input(const struct matisse_dxio_early_input *input,
		       const struct matisse_dxio_early_state *state,
		       const struct matisse_dxio_transport *transport,
		       const struct matisse_dxio_early_io *io)
{
	if (!valid_range(input, sizeof(*input)) || !valid_range(state, sizeof(*state)) ||
	    !valid_range(transport, sizeof(*transport)) || !valid_range(io, sizeof(*io)) ||
	    !valid_range(input->result, sizeof(*input->result)) ||
	    !valid_range(input->platform, input->platform_bytes) ||
	    input->platform_bytes < MATISSE_DXIO_PLATFORM_HEADER_BYTES ||
	    (input->platform_bytes & 3U) || !input->platform_physical ||
	    (input->platform_physical & 3U) ||
	    (uint64_t)input->platform_physical + input->platform_bytes > (1ULL << 32) ||
	    !transport->request || !io->admit || !io->may_access || !io->read ||
	    !io->write || !io->prepare || !io->consume ||
	    state->phase != MATISSE_DXIO_EARLY_NOT_STARTED ||
	    input->result->count > MATISSE_NBIF_MAP_SLOTS ||
	    overlap(state, sizeof(*state), input, sizeof(*input)) ||
	    overlap(state, sizeof(*state), transport, sizeof(*transport)) ||
	    overlap(state, sizeof(*state), io, sizeof(*io)) ||
	    overlap(state, sizeof(*state), input->result, sizeof(*input->result)) ||
	    overlap(state, sizeof(*state), input->platform, input->platform_bytes) ||
	    overlap(input->platform, input->platform_bytes, input, sizeof(*input)) ||
	    overlap(input->platform, input->platform_bytes, transport, sizeof(*transport)) ||
	    overlap(input->platform, input->platform_bytes, io, sizeof(*io)) ||
	    overlap(input->platform, input->platform_bytes,
		    input->result, sizeof(*input->result)))
		return false;
	if (engine_count(input) > (input->platform_bytes - MATISSE_DXIO_PLATFORM_HEADER_BYTES)
				 / MATISSE_DXIO_ENGINE_BYTES)
		return false;
	if (!input->result->has_early)
		return true;
	if (input->result->early_engine >= engine_count(input) ||
	    input->result->early_bdf > UINT8_MAX ||
	    matisse_nbif_devfn_index((uint8_t)input->result->early_bdf) < 0)
		return false;
	const uint8_t *engine = early_engine(input);

	return (engine[0] & 15U) == 1 && (engine[16] & 2U) &&
		read32(engine + 12) == input->result->early_bdf;
}

enum cb_err matisse_run_dxio_early_link(const struct matisse_dxio_early_input *input,
		struct matisse_dxio_early_state *state,
		const struct matisse_dxio_transport *transport,
		const struct matisse_dxio_early_io *io)
{
	uint32_t value = 0;

	if (!valid_input(input, state, transport, io))
		return CB_ERR;
	state->phase = MATISSE_DXIO_EARLY_RUNNING;
	stopwatch_init_msecs_expire(&state->budget, 120000);
	if (io->admit(input, io->context) != CB_SUCCESS) {
		state->failure = MATISSE_DXIO_EARLY_ADMISSION_FAILED;
		goto failed;
	}
	if (!available(state, io))
		goto failed;
	if (input->result->has_early) {
		state->bdf = input->result->early_bdf << 12;
		state->engine_key = read32(early_engine(input));
		state->cleanup_needed = true;
		if (!pci_access(state, io, state->bdf | 0x6aU, 2, &value, false))
			goto failed;
		if (value & (1U << 13)) {
			uint32_t args[5] = { 0, input->platform_physical,
				(uint32_t)(input->platform_bytes / 4), 0, 0 };

			if (!pci_access(state, io, state->bdf | 0x18U, 4, &value, false))
				goto failed;
			state->bus_before = value;
			value = (value & 0xff0000ffU) | 0x20200U;
			if (!pci_access(state, io, state->bdf | 0x18U, 4, &value, true) ||
			    !dma_callback(state, io, 14) ||
			    !request(state, transport, io, 0x14, args) ||
			    !dma_callback(state, io, 0) ||
			    !pci_access(state, io, 0x200000, 4, &state->downstream_id, false))
				goto failed;
			if ((state->downstream_id & 65535U) == 0x10b5) {
				state->failure = MATISSE_DXIO_EARLY_UNQUALIFIED_SWITCH;
				goto failed;
			}
			if (input->result->early_engine >= engine_count(input) ||
			    engine_count(input) > (input->platform_bytes -
				MATISSE_DXIO_PLATFORM_HEADER_BYTES) /
				MATISSE_DXIO_ENGINE_BYTES ||
			    read32(early_engine(input)) != state->engine_key) {
				state->failure = MATISSE_DXIO_EARLY_DMA_FAILED;
				goto failed;
			}
			uint8_t *engine = early_engine(input);

			engine[12] = 0;
			engine[13] = 2;
			engine[14] = 0;
			engine[15] = 0;
			state->frame_changed = true;
			args[4] = 3;
			if (!dma_callback(state, io, 3) ||
			    !request(state, transport, io, 0x23, args))
				goto failed;
		}
	}
	state->phase = MATISSE_DXIO_EARLY_PENDING_CLEANUP;
	return CB_SUCCESS;

failed:
	state->phase = MATISSE_DXIO_EARLY_FAILED;
	return CB_ERR;
}

enum cb_err matisse_finish_dxio_early_link(struct matisse_dxio_early_state *state,
					const struct matisse_dxio_early_io *io)
{
	uint32_t value = 0;

	if (!valid_range(state, sizeof(*state)) || !valid_range(io, sizeof(*io)) ||
	    !io->may_access || !io->read || !io->write || state->cleanup_attempted ||
	    (state->phase != MATISSE_DXIO_EARLY_PENDING_CLEANUP &&
	     state->phase != MATISSE_DXIO_EARLY_FAILED) ||
	    overlap(state, sizeof(*state), io, sizeof(*io)))
		return CB_ERR;
	state->cleanup_attempted = true;
	state->cleanup_status = CB_ERR;
	if (state->cleanup_needed) {
		if (!pci_access(state, io, state->bdf | 0x18U, 4, &value, false))
			return CB_ERR;
		value &= 0xff0000ffU;
		if (!pci_access(state, io, state->bdf | 0x18U, 4, &value, true))
			return CB_ERR;
	}
	if (!available(state, io))
		return CB_ERR;
	state->cleanup_needed = false;
	state->cleanup_status = CB_SUCCESS;
	if (state->phase != MATISSE_DXIO_EARLY_FAILED)
		state->phase = MATISSE_DXIO_EARLY_FINISHED;
	return CB_SUCCESS;
}
