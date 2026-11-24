/* SPDX-License-Identifier: GPL-2.0-only */

#include <limits.h>
#include <string.h>
#include <amdblocks/matisse_dxio_events.h>

struct event_session {
	const struct matisse_dxio_events_input *input;
	struct matisse_dxio_events_state *state;
	const struct matisse_dxio_events_io *io;
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

static void fail(struct matisse_dxio_events_state *state, uint32_t failure)
{
	if (!state->failure)
		state->failure = failure;
	state->phase = MATISSE_DXIO_EVENTS_FAILED;
}

static bool available(struct event_session *session)
{
	if (stopwatch_expired(&session->state->budget) ||
	    !session->io->may_access(session->io->context)) {
		if (!session->state->failure)
			session->state->failure = MATISSE_DXIO_EVENTS_ACCESS_DENIED;
		return false;
	}
	return true;
}

static bool valid_input(const struct matisse_dxio_events_input *input,
		const struct matisse_dxio_events_state *state,
		const struct matisse_dxio_events_io *io)
{
	if (!valid_range(input, sizeof(*input)) || !valid_range(state, sizeof(*state)) ||
	    !valid_range(io, sizeof(*io)) ||
	    !valid_range(input->mapping, sizeof(*input->mapping)) ||
	    !valid_range(input->configured, sizeof(*input->configured)) ||
	    !io->admit || !io->may_access || !io->prepare || !io->consume ||
	    !io->smn_read || !io->smn_write || !io->pci_read || !io->pci_write)
		return false;
	const struct matisse_dxio_mapped_input *in = input->mapping;

	if (in->bus || in->root_index > 15 || input->configured->root_index != in->root_index ||
	    !in->wrapper_count || in->wrapper_count > 16 || !in->slot_count ||
	    in->slot_count > MATISSE_NBIF_MAP_SLOTS || !in->port_count ||
	    in->port_count > MATISSE_NBIF_MAP_SLOTS || in->platform_bytes < 24 ||
	    in->platform_bytes > UINT32_MAX || (in->platform_bytes & 3) ||
	    !in->platform_physical || (in->platform_physical & 3) ||
	    (uint64_t)in->platform_physical + in->platform_bytes > UINT32_MAX)
		return false;
	const void *backing[] = { input, io, in, input->configured, in->wrappers,
		in->slots, in->ports, in->overrides, in->initial_controls, in->policy,
		in->platform };
	const size_t bytes[] = { sizeof(*input), sizeof(*io), sizeof(*in),
		sizeof(*input->configured), in->wrapper_count,
		in->slot_count * sizeof(*in->slots), in->port_count * sizeof(*in->ports),
		in->port_count * sizeof(*in->overrides),
		MATISSE_NBIF_MAP_SLOTS * sizeof(*in->initial_controls), sizeof(*in->policy),
		in->platform_bytes };

	for (size_t i = 0; i < sizeof(backing) / sizeof(backing[0]); i++)
		if (!valid_range(backing[i], bytes[i]) ||
		    overlap(state, sizeof(*state), backing[i], bytes[i]))
			return false;
	for (size_t i = 0; i + 1 < sizeof(backing) / sizeof(backing[0]); i++)
		if (overlap(in->platform, in->platform_bytes, backing[i], bytes[i]))
			return false;
	return true;
}

static enum cb_err admit_mapped(const struct matisse_dxio_mapped_input *input, void *context)
{
	struct event_session *session = context;

	return input->platform == session->input->mapping->platform &&
		available(session) ? CB_SUCCESS : CB_ERR;
}

static bool may_access(void *context)
{
	return available(context);
}

static enum cb_err prepare(unsigned int use, void *context)
{
	struct event_session *session = context;

	return available(session) ? session->io->prepare(use, session->io->context) : CB_ERR;
}

static enum cb_err consume(void *context)
{
	struct event_session *session = context;

	return available(session) ? session->io->consume(session->io->context) : CB_ERR;
}

static enum cb_err smn_read(uint32_t address, uint32_t *value, void *context)
{
	struct event_session *session = context;

	return available(session) ?
		session->io->smn_read(address, value, session->io->context) : CB_ERR;
}

static enum cb_err smn_write(uint32_t address, uint32_t value, void *context)
{
	struct event_session *session = context;

	return available(session) ?
		session->io->smn_write(address, value, session->io->context) : CB_ERR;
}

static enum cb_err admit_early(const struct matisse_dxio_early_input *input, void *context)
{
	struct event_session *session = context;

	return input->result == &session->state->result && available(session) ?
		CB_SUCCESS : CB_ERR;
}

static enum cb_err pci_read(uint32_t address, unsigned int bytes, uint32_t *value,
			    void *context)
{
	struct event_session *session = context;

	return available(session) ?
		session->io->pci_read(address, bytes, value, session->io->context) : CB_ERR;
}

static enum cb_err pci_write(uint32_t address, unsigned int bytes, uint32_t value,
			     void *context)
{
	struct event_session *session = context;

	return available(session) ?
		session->io->pci_write(address, bytes, value, session->io->context) : CB_ERR;
}

static struct matisse_dxio_early_io early_io(struct event_session *session)
{
	return (struct matisse_dxio_early_io) {
		.admit = admit_early, .may_access = may_access, .read = pci_read,
		.write = pci_write, .prepare = prepare, .consume = consume, .context = session,
	};
}

static bool mapped(struct event_session *session, const struct matisse_dxio_transport *transport)
{
	struct matisse_dxio_events_state *state = session->state;
	struct matisse_dxio_mapped_input input = *session->input->mapping;
	const struct matisse_dxio_mapped_io io = {
		.admit = admit_mapped, .may_access = may_access, .prepare = prepare,
		.consume = consume, .read = smn_read, .write = smn_write, .context = session,
	};

	input.initial_controls = state->controls;
	memset(&state->mapped, 0, sizeof(state->mapped));
	state->mapped_events++;
	if (matisse_run_dxio_mapped(&input, &state->mapped, transport, &io) != CB_SUCCESS)
		return false;
	state->result = state->mapped.result;
	for (size_t s = 0; s < MATISSE_NBIF_MAP_SLOTS; s++)
		state->controls[s] = state->mapped.reconfigure.controls[s];
	return true;
}

static bool early(struct event_session *session, const struct matisse_dxio_transport *transport)
{
	struct matisse_dxio_events_state *state = session->state;
	const struct matisse_dxio_mapped_input *mapping = session->input->mapping;
	const struct matisse_dxio_early_input input = {
		.platform = mapping->platform, .platform_bytes = mapping->platform_bytes,
		.platform_physical = mapping->platform_physical, .result = &state->result,
	};
	struct matisse_dxio_early_io io = early_io(session);

	if (state->early_seen && state->early_generation == state->mapped_events) {
		state->duplicate_early_events++;
		return available(session);
	}
	if (state->route_valid && state->route.cleanup_needed && state->result.has_early &&
	    state->route.bdf != state->result.early_bdf << 12)
		return false;
	memset(&state->early, 0, sizeof(state->early));
	state->early_events++;
	enum cb_err status = matisse_run_dxio_early_link(&input, &state->early, transport, &io);

	if (!state->route_valid || !state->route.cleanup_needed)
		state->route = state->early;
	state->route_valid = true;
	if (status != CB_SUCCESS)
		return false;
	state->early_seen = true;
	state->early_generation = state->mapped_events;
	return true;
}

static enum cb_err admit_configured(const struct matisse_dxio_configured_step *plan,
		size_t count, void *context)
{
	struct event_session *session = context;

	return plan == session->state->plan && count && available(session) ? CB_SUCCESS : CB_ERR;
}

static bool configured_access(uint32_t address, bool write, void *context)
{
	(void)address;
	(void)write;
	return available(context);
}

static bool configured(struct event_session *session)
{
	struct matisse_dxio_events_state *state = session->state;
	const struct matisse_dxio_mapped_input *in = session->input->mapping;
	const struct matisse_dxio_configured_hooks hooks = {
		.admit = admit_configured, .may_access = configured_access, .context = session,
	};
	size_t count = matisse_plan_dxio_configured(in->wrappers, in->wrapper_count,
		in->slots, in->slot_count, in->ports, in->port_count, &state->result,
		session->input->configured, state->plan, MATISSE_DXIO_CONFIGURED_MAX_STEPS);

	if (!count) {
		fail(state, MATISSE_DXIO_EVENTS_PLAN_FAILED);
		return false;
	}
	/* Keep the admitted record prefix between successful event executions. */
	state->configured.phase = MATISSE_DXIO_CONFIGURED_NOT_STARTED;
	state->configured_events++;
	return matisse_run_dxio_configured(state->plan, count, &state->configured, &hooks)
		== CB_SUCCESS;
}

enum cb_err matisse_begin_dxio_events(const struct matisse_dxio_events_input *input,
		struct matisse_dxio_events_state *state, const struct matisse_dxio_events_io *io)
{
	if (!valid_input(input, state, io) || state->phase != MATISSE_DXIO_EVENTS_NOT_STARTED)
		return CB_ERR;
	struct event_session session = { input, state, io };

	state->phase = MATISSE_DXIO_EVENTS_ADMITTING;
	stopwatch_init_msecs_expire(&state->budget, 120000);
	if (io->admit(input, io->context) != CB_SUCCESS) {
		fail(state, MATISSE_DXIO_EVENTS_ADMISSION_FAILED);
		return CB_ERR;
	}
	if (!available(&session)) {
		fail(state, MATISSE_DXIO_EVENTS_ACCESS_DENIED);
		return CB_ERR;
	}
	for (size_t s = 0; s < MATISSE_NBIF_MAP_SLOTS; s++)
		state->controls[s] = input->mapping->initial_controls[s];
	state->phase = MATISSE_DXIO_EVENTS_READY;
	return CB_SUCCESS;
}

enum matisse_dxio_event_result matisse_handle_dxio_event(
		const struct matisse_dxio_events_input *input,
		struct matisse_dxio_events_state *state,
		const struct matisse_dxio_poll_result *event, unsigned int phase,
		const struct matisse_dxio_transport *transport,
		const struct matisse_dxio_events_io *io)
{
	if (!valid_input(input, state, io) || state->phase != MATISSE_DXIO_EVENTS_READY ||
	    !valid_range(event, sizeof(*event)) ||
	    !valid_range(transport, sizeof(*transport)) || !transport->request ||
	    phase < 1 || phase > 2 || phase != state->completed_phases + 1 ||
	    state->events >= 256 || overlap(state, sizeof(*state), event, sizeof(*event)) ||
	    overlap(state, sizeof(*state), transport, sizeof(*transport)))
		return MATISSE_DXIO_EVENT_FAILED;
	struct event_session session = { input, state, io };
	struct matisse_dxio_poll_result decoded;
	enum matisse_dxio_event_result result = MATISSE_DXIO_EVENT_HANDLED;

	state->phase = MATISSE_DXIO_EVENTS_HANDLING;
	state->last_event = *event;
	state->events++;
	if (!available(&session))
		goto failed;
	if (!matisse_decode_dxio_poll(event->raw, &decoded) || decoded.kind != event->kind ||
	    (decoded.kind != MATISSE_DXIO_POLL_EVENT && decoded.kind != MATISSE_DXIO_POLL_STATE)) {
		fail(state, MATISSE_DXIO_EVENTS_INVALID_EVENT);
		goto failed;
	}
	if (io->report)
		io->report(event, phase, io->context);
	if (!available(&session))
		goto failed;
	if (decoded.kind == MATISSE_DXIO_POLL_STATE &&
	    (decoded.raw[2] > 1 || decoded.raw[3] > UINT8_MAX || !io->reset ||
	     io->reset(decoded.raw[3], decoded.raw[2], io->context) != CB_SUCCESS)) {
		fail(state, MATISSE_DXIO_EVENTS_RESET_FAILED);
		goto failed;
	}
	if (decoded.kind == MATISSE_DXIO_POLL_EVENT) {
		switch (decoded.raw[2]) {
		case 15:
			if (!mapped(&session, transport)) {
				fail(state, MATISSE_DXIO_EVENTS_MAPPED_FAILED);
				goto failed;
			}
			break;
		case 7:
			if (!early(&session, transport)) {
				fail(state, MATISSE_DXIO_EVENTS_EARLY_FAILED);
				goto failed;
			}
			break;
		case 5:
			if (!configured(&session)) {
				fail(state, MATISSE_DXIO_EVENTS_CONFIGURED_FAILED);
				goto failed;
			}
			result = MATISSE_DXIO_EVENT_PHASE_DONE;
			break;
		case 13:
			result = MATISSE_DXIO_EVENT_PHASE_DONE;
			break;
		}
	}
	if (!available(&session))
		goto failed;
	state->phase = MATISSE_DXIO_EVENTS_READY;
	return result;

failed:
	state->phase = MATISSE_DXIO_EVENTS_FAILED;
	return MATISSE_DXIO_EVENT_FAILED;
}

enum cb_err matisse_finish_dxio_events_phase(const struct matisse_dxio_events_input *input,
		struct matisse_dxio_events_state *state, unsigned int phase, bool completed,
		const struct matisse_dxio_events_io *io)
{
	if (!valid_input(input, state, io) || phase < 1 || phase > 2 ||
	    phase != state->completed_phases + 1 ||
	    (state->phase != MATISSE_DXIO_EVENTS_READY &&
	     state->phase != MATISSE_DXIO_EVENTS_FAILED) ||
	    (state->finish_attempted & (1U << (phase - 1))))
		return CB_ERR;
	struct event_session session = { input, state, io };
	struct matisse_dxio_early_io access = early_io(&session);

	state->phase = MATISSE_DXIO_EVENTS_FINISHING_PHASE;
	state->finish_attempted |= 1U << (phase - 1);
	if (state->route_valid &&
	    matisse_finish_dxio_early_link(&state->route, &access) != CB_SUCCESS)
		goto failed;
	if (!available(&session))
		goto failed;
	state->route_valid = false;
	state->completed_phases = phase;
	state->phase = state->failure || !completed ? MATISSE_DXIO_EVENTS_FAILED :
		phase == 2 ? MATISSE_DXIO_EVENTS_FINISHED : MATISSE_DXIO_EVENTS_READY;
	return CB_SUCCESS;

failed:
	state->cleanup_failures |= 1U << (phase - 1);
	fail(state, MATISSE_DXIO_EVENTS_CLEANUP_FAILED);
	return CB_ERR;
}
