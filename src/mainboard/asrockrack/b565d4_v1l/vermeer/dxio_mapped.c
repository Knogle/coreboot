/* SPDX-License-Identifier: GPL-2.0-only */

#include <limits.h>
#include <timer.h>
#include "dxio_mapped.h"
#include "dxio_service.h"

struct mapped_session {
	const struct matisse_dxio_mapped_input *input;
	struct matisse_dxio_mapped_state *state;
	const struct matisse_dxio_mapped_io *io;
	struct stopwatch budget;
};

static bool valid_range(const void *pointer, size_t bytes)
{
	return pointer && (uintptr_t)pointer <= UINTPTR_MAX - bytes;
}

static bool overlap(const void *a, size_t a_bytes, const void *b, size_t b_bytes)
{
	return (uintptr_t)a < (uintptr_t)b + b_bytes &&
		(uintptr_t)b < (uintptr_t)a + a_bytes;
}

static bool available(struct mapped_session *session)
{
	if (stopwatch_expired(&session->budget) || session->state->accesses >= 32768 ||
	    !session->io->may_access(session->io->context)) {
		session->state->failure = MATISSE_DXIO_MAPPED_ACCESS_DENIED;
		return false;
	}
	return true;
}

static enum cb_err read_register(uint32_t address, uint32_t *value, void *context)
{
	struct mapped_session *session = context;
	struct matisse_dxio_mapped_state *state = session->state;

	if (!available(session))
		return CB_ERR;
	state->last_address = address;
	state->accesses++;
	state->last_status = session->io->read(address, value, session->io->context);
	if (state->last_status != CB_SUCCESS)
		return CB_ERR;
	state->last_value = *value;
	if (!available(session))
		return CB_ERR;
	state->reads++;
	return CB_SUCCESS;
}

static enum cb_err write_register(uint32_t address, uint32_t value, void *context)
{
	struct mapped_session *session = context;
	struct matisse_dxio_mapped_state *state = session->state;

	if (!available(session))
		return CB_ERR;
	state->last_address = address;
	state->last_value = value;
	state->accesses++;
	state->last_status = session->io->write(address, value, session->io->context);
	if (state->last_status != CB_SUCCESS || !available(session))
		return CB_ERR;
	state->writes++;
	return CB_SUCCESS;
}

static enum cb_err admit_reconfigure(const struct matisse_dxio_reconfigure_input *input,
		void *context)
{
	struct mapped_session *session = context;

	if (input->bindings != &session->state->result ||
	    input->controls != session->state->controls || !available(session))
		return CB_ERR;
	return CB_SUCCESS;
}

static bool valid_input(const struct matisse_dxio_mapped_input *in,
		struct matisse_dxio_mapped_state *state,
		const struct matisse_dxio_transport *transport,
		const struct matisse_dxio_mapped_io *io)
{
	if (!valid_range(in, sizeof(*in)) || !valid_range(state, sizeof(*state)) ||
	    !valid_range(transport, sizeof(*transport)) || !valid_range(io, sizeof(*io)) ||
	    !transport->request || !io->admit || !io->may_access || !io->prepare ||
	    !io->consume || !io->read || !io->write ||
	    state->phase != MATISSE_DXIO_MAPPED_NOT_STARTED || in->bus ||
	    !in->wrapper_count || in->wrapper_count > 16 ||
	    !in->slot_count || in->slot_count > MATISSE_NBIF_MAP_SLOTS ||
	    !in->port_count || in->port_count > MATISSE_NBIF_MAP_SLOTS ||
	    in->platform_bytes < 24 || in->platform_bytes > UINT32_MAX ||
	    (in->platform_bytes & 3) || !in->platform_physical ||
	    (in->platform_physical & 3) ||
	    (uint64_t)in->platform_physical + in->platform_bytes > UINT32_MAX)
		return false;
	const void *backing[] = { in, transport, io, in->wrappers, in->slots, in->ports,
		in->overrides, in->initial_controls, in->policy, in->platform };
	const size_t bytes[] = { sizeof(*in), sizeof(*transport), sizeof(*io),
		in->wrapper_count, in->slot_count * sizeof(*in->slots),
		in->port_count * sizeof(*in->ports), in->port_count * sizeof(*in->overrides),
		MATISSE_NBIF_MAP_SLOTS * sizeof(*in->initial_controls),
		sizeof(*in->policy), in->platform_bytes };

	for (size_t i = 0; i < sizeof(backing) / sizeof(backing[0]); i++)
		if (!valid_range(backing[i], bytes[i]) ||
		    overlap(state, sizeof(*state), backing[i], bytes[i]))
			return false;
	for (size_t i = 0; i + 1 < sizeof(backing) / sizeof(backing[0]); i++)
		if (overlap(in->platform, in->platform_bytes, backing[i], bytes[i]))
			return false;
	return true;
}

enum cb_err matisse_run_dxio_mapped(const struct matisse_dxio_mapped_input *input,
		struct matisse_dxio_mapped_state *state,
		const struct matisse_dxio_transport *transport,
		const struct matisse_dxio_mapped_io *io)
{
	if (!valid_input(input, state, transport, io))
		return CB_ERR;
	struct mapped_session session = { .input = input, .state = state, .io = io };
	struct matisse_dxio_link_controls next;
	const uint32_t args[5] = { 0, input->platform_physical,
		(uint32_t)(input->platform_bytes / 4), 0, 0 };
	uint32_t zero[MATISSE_NBIF_MAP_SLOTS] = { 0 };

	state->phase = MATISSE_DXIO_MAPPED_ADMITTING;
	stopwatch_init_msecs_expire(&session.budget, 120000);
	if (io->admit(input, io->context) != CB_SUCCESS) {
		state->failure = MATISSE_DXIO_MAPPED_ADMISSION_FAILED;
		goto failed;
	}
	state->phase = MATISSE_DXIO_MAPPED_RUNNING;
	if (!available(&session))
		goto failed;
	if (io->prepare(14, io->context) != CB_SUCCESS || !available(&session)) {
		if (!state->failure)
			state->failure = MATISSE_DXIO_MAPPED_DMA_FAILED;
		goto failed;
	}
	state->request_attempted = 1;
	state->last_status = transport->request(transport->context, 0x14, args, &state->reply);
	if (state->last_status != CB_SUCCESS || matisse_dxio_status(state->reply.msg[0]) != 3) {
		state->failure = MATISSE_DXIO_MAPPED_REQUEST_FAILED;
		goto failed;
	}
	if (!available(&session) || io->consume(io->context) != CB_SUCCESS ||
	    !available(&session)) {
		if (!state->failure)
			state->failure = MATISSE_DXIO_MAPPED_DMA_FAILED;
		goto failed;
	}
	if (!matisse_decode_dxio_result(input->slots, input->slot_count, input->ports,
			input->port_count, input->platform, input->platform_bytes,
			input->bus, &state->result)) {
		state->failure = MATISSE_DXIO_MAPPED_DECODE_FAILED;
		goto failed;
	}
	for (size_t s = 0; s < MATISSE_NBIF_MAP_SLOTS; s++)
		state->controls[s] = input->initial_controls[s];
	for (size_t b = 0; b < state->result.count; b++) {
		const struct matisse_dxio_engine_result *binding = &state->result.engines[b];

		if (!matisse_update_dxio_link_controls(&input->ports[binding->policy_index].policy,
				&input->overrides[binding->policy_index],
				&state->controls[binding->slot], &next)) {
			state->failure = MATISSE_DXIO_MAPPED_CONTROLS_FAILED;
			goto failed;
		}
		state->controls[binding->slot] = next;
	}
	if (!matisse_build_nbif_map(input->root_index, state->result.assigned,
				zero, state->map)) {
		state->failure = MATISSE_DXIO_MAPPED_MAP_FAILED;
		goto failed;
	}
	for (size_t s = 0; s < MATISSE_NBIF_MAP_SLOTS; s++) {
		if (read_register(state->map[s].address, &state->map_before[s], &session)
		    != CB_SUCCESS)
			goto map_failed;
		state->map[s].value |= state->map_before[s] & 0xffffff00U;
		if (write_register(state->map[s].address, state->map[s].value, &session)
		    != CB_SUCCESS)
			goto map_failed;
	}
	const struct matisse_dxio_reconfigure_input reconfigure = {
		.root_index = input->root_index, .wrappers = input->wrappers,
		.wrapper_count = input->wrapper_count, .slots = input->slots,
		.slot_count = input->slot_count, .bindings = &state->result,
		.controls = state->controls, .policy = input->policy,
	};
	const struct matisse_dxio_reconfigure_io reconfigure_io = {
		.admit = admit_reconfigure, .read = read_register,
		.write = write_register, .context = &session,
	};

	if (matisse_run_dxio_reconfigure(&reconfigure, &state->reconfigure,
					&reconfigure_io) != CB_SUCCESS || !available(&session)) {
		if (!state->failure)
			state->failure = MATISSE_DXIO_MAPPED_RECONFIGURE_FAILED;
		goto failed;
	}
	state->phase = MATISSE_DXIO_MAPPED_IO_RETURNED;
	return CB_SUCCESS;

map_failed:
	if (!state->failure)
		state->failure = MATISSE_DXIO_MAPPED_MAP_FAILED;
failed:
	state->phase = MATISSE_DXIO_MAPPED_FAILED;
	return CB_ERR;
}
