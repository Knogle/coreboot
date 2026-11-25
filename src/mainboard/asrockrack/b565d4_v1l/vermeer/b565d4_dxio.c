/* SPDX-License-Identifier: GPL-2.0-only */

#include <limits.h>
#include "b565d4_dxio.h"
#include <amdblocks/matisse_dxio_init_plan.h>
#include <amdblocks/matisse_nbio_memory.h>
#include <string.h>

static const struct matisse_dxio_link_controls initial_control = {
	.flags_27 = 1,
	.flags_2a = 4,
	.equalization = 0x1bb7,
	.equalization_extended = 0x57,
	.flags_2e = 0x0f,
	.flags_30 = 0xf0,
	.flags_31 = 1,
};

static void init_link_policy(struct b565d4_matisse_link_policy *policy)
{
	memset(policy, 0, sizeof(*policy));
	for (unsigned int wrapper = 0; wrapper < B565D4_MATISSE_WRAPPERS; wrapper++) {
		policy->wrappers[wrapper] = (uint8_t)wrapper;
		for (unsigned int child = 0; child < 8; child++) {
			unsigned int local = 7 - child;
			unsigned int slot = wrapper * 8 + local;
			struct matisse_dxio_result_slot *port =
				&policy->slots[wrapper * 8 + child];
			uint8_t first = (uint8_t)(16 * wrapper);

			port->lanes = (struct matisse_dxio_lane_group) {
				first, (uint8_t)(first + 15), first,
				(uint8_t)(first + 15), false,
			};
			port->encoding = (uint8_t)((local << 4) | wrapper);
			port->fallback = local == 7 ? (uint8_t)(0x22 + 2 * wrapper) :
				(uint8_t)(((local + 1) << 5) | (1 + 2 * wrapper));
			policy->controls[slot] = initial_control;
		}
	}
	/* The remaining two NBIF slots have no original port descriptor. */
	policy->reconfigure.token_e6 = 1;
	policy->reconfigure.token_155 = 0xff;
	policy->reconfigure.token_1b5 = 1;
	policy->reconfigure.token_1fa = 0xff;
	policy->reconfigure.token_2b5 = 1;
	policy->reconfigure.token_2c9 = 1;
	policy->reconfigure.token_319 = 0xff;
	policy->configured.root_index = MATISSE_NBIO_INSTANCE;
	policy->configured.token_194 = true;
	policy->configured.token_2fb = true;
}

/* Original compiled PCD/CBS defaults, expressed as native consumed fields. */
static const struct matisse_dxio_init_policy dxio_init_policy = {
	.select_enabled = true,
	.property1f = 1,
	.property14 = 1,
	.property0d = true,
	.property13 = 0xff,
	.tuning_enabled = true,
	.tuning = { 200000, 200000, 600000 },
	.features = 1U << 9,
	.property23 = true,
};

struct board_session {
	struct b565d4_matisse_dxio_state *state;
	const struct b565d4_matisse_dxio_io *io;
	struct stopwatch budget;
	struct matisse_dxio_dma_io dma_io;
	struct matisse_dxio_events_io event_io;
};

static bool valid_range(const void *pointer, size_t bytes)
{
	return pointer && (uintptr_t)pointer <= UINTPTR_MAX - bytes;
}

static bool overlap(uint64_t a, uint64_t a_bytes, uint64_t b, uint64_t b_bytes)
{
	return a < b + b_bytes && b < a + a_bytes;
}

static bool valid_input(const struct b565d4_matisse_dxio_input *input,
		const struct b565d4_matisse_dxio_state *state,
		const struct b565d4_matisse_dxio_io *io)
{
	if (!valid_range(input, sizeof(*input)) || !valid_range(state, sizeof(*state)) ||
	    !valid_range(io, sizeof(*io)) || !io->admit || !io->may_access ||
	    !io->smn_read || !io->smn_write || !io->pci_read || !io->pci_write ||
	    input->max_cpu_link_speed > 4 || input->cpu_slot_speed > 3 ||
	    input->cpu_gpp_speed > 3 ||
	    state->phase != B565D4_MATISSE_DXIO_NOT_STARTED ||
	    state->dma.phase != MATISSE_DXIO_DMA_NOT_STARTED ||
	    state->events.phase != MATISSE_DXIO_EVENTS_NOT_STARTED ||
	    state->controller.phase != MATISSE_DXIO_NOT_STARTED ||
	    (input->slot_mode != B565D4_MATISSE_PCIE_X16 &&
	     input->slot_mode != B565D4_MATISSE_PCIE_FOUR_X4))
		return false;
	size_t ports = input->slot_mode == B565D4_MATISSE_PCIE_X16 ? 3 : 6;
	size_t bytes = MATISSE_DXIO_PLATFORM_HEADER_BYTES +
		ports * MATISSE_DXIO_ENGINE_BYTES + MATISSE_DXIO_PLATFORM_TAIL_BYTES;
	size_t ancillary_bytes = (size_t)input->ancillary_dwords * 4;

	if (!valid_range(input->platform, bytes) || ((uintptr_t)input->platform & 3) ||
	    input->platform_capacity < bytes || !input->platform_physical ||
	    (input->platform_physical & 3) ||
	    (uint64_t)input->platform_physical + bytes > UINT32_MAX ||
	    input->ancillary_dwords > 256)
		return false;
	if (ancillary_bytes) {
		if (input->ancillary_dwords < 3 || !valid_range(input->ancillary, ancillary_bytes) ||
		    ((uintptr_t)input->ancillary & 3) || !input->ancillary_physical ||
		    (input->ancillary_physical & 3) ||
		    (uint64_t)input->ancillary_physical + ancillary_bytes > UINT32_MAX ||
		    overlap(input->platform_physical, bytes,
			    input->ancillary_physical, ancillary_bytes) ||
		    overlap((uintptr_t)input->platform, bytes,
			    (uintptr_t)input->ancillary, ancillary_bytes))
			return false;
	} else if (input->ancillary || input->ancillary_physical) {
		return false;
	}
	const void *metadata[] = { input, state, io };
	const size_t metadata_bytes[] = { sizeof(*input), sizeof(*state), sizeof(*io) };

	for (size_t i = 0; i < 3; i++) {
		for (size_t j = i + 1; j < 3; j++)
			if (overlap((uintptr_t)metadata[i], metadata_bytes[i],
				    (uintptr_t)metadata[j], metadata_bytes[j]))
				return false;
		if (overlap((uintptr_t)input->platform, bytes,
			    (uintptr_t)metadata[i], metadata_bytes[i]) ||
		    (ancillary_bytes && overlap((uintptr_t)input->ancillary, ancillary_bytes,
					(uintptr_t)metadata[i], metadata_bytes[i])))
			return false;
	}
	return true;
}

static bool available(void *context)
{
	struct board_session *session = context;

	return !stopwatch_expired(&session->budget) &&
		session->io->may_access(session->io->context);
}

static bool same_backing(const struct matisse_dxio_dma_input *backing,
		const struct board_session *session)
{
	const struct matisse_dxio_dma_input *expected = &session->state->backing;

	return backing->platform == expected->platform &&
		backing->ancillary == expected->ancillary &&
		backing->wire.platform == expected->wire.platform &&
		backing->wire.platform_dwords == expected->wire.platform_dwords &&
		backing->wire.ancillary == expected->wire.ancillary &&
		backing->wire.ancillary_dwords == expected->wire.ancillary_dwords;
}

static bool dma_available(const struct matisse_dxio_dma_input *backing, void *context)
{
	return same_backing(backing, context) && available(context);
}

static enum cb_err dma_admit(const struct matisse_dxio_dma_input *backing, void *context)
{
	return dma_available(backing, context) ? CB_SUCCESS : CB_ERR;
}

static enum cb_err prepare(unsigned int use, void *context)
{
	struct board_session *session = context;

	return available(session) ? matisse_prepare_dxio_dma(&session->state->dma, use,
							   &session->dma_io) : CB_ERR;
}

static enum cb_err consume(void *context)
{
	struct board_session *session = context;

	return available(session) ? matisse_consume_dxio_dma(&session->state->dma,
							 &session->dma_io) : CB_ERR;
}

static enum cb_err event_admit(const struct matisse_dxio_events_input *input, void *context)
{
	struct board_session *session = context;

	return input == &session->state->event_input && available(session) ? CB_SUCCESS : CB_ERR;
}

static enum cb_err smn_read(uint32_t address, uint32_t *value, void *context)
{
	struct board_session *session = context;

	return available(session) ?
		session->io->smn_read(address, value, session->io->context) : CB_ERR;
}

static enum cb_err smn_write(uint32_t address, uint32_t value, void *context)
{
	struct board_session *session = context;

	return available(session) ?
		session->io->smn_write(address, value, session->io->context) : CB_ERR;
}

static enum cb_err pci_read(uint32_t address, unsigned int bytes, uint32_t *value,
			    void *context)
{
	struct board_session *session = context;

	return available(session) ?
		session->io->pci_read(address, bytes, value, session->io->context) : CB_ERR;
}

static enum cb_err pci_write(uint32_t address, unsigned int bytes, uint32_t value,
			     void *context)
{
	struct board_session *session = context;

	return available(session) ?
		session->io->pci_write(address, bytes, value, session->io->context) : CB_ERR;
}

static void report(const struct matisse_dxio_poll_result *event, unsigned int phase,
		   void *context)
{
	struct board_session *session = context;

	if (session->io->report)
		session->io->report(event, phase, session->io->context);
}

static enum cb_err reset(unsigned int group, unsigned int level, void *context)
{
	struct board_session *session = context;

	return available(session) && session->io->reset ?
		session->io->reset(group, level, session->io->context) : CB_ERR;
}

static bool same_wire(const struct matisse_dxio_init_buffers *wire,
		const struct board_session *session)
{
	const struct matisse_dxio_init_buffers *expected = &session->state->backing.wire;

	return wire->platform == expected->platform &&
		wire->platform_dwords == expected->platform_dwords &&
		wire->ancillary == expected->ancillary &&
		wire->ancillary_dwords == expected->ancillary_dwords;
}

static enum cb_err controller_admit(const struct matisse_dxio_init_buffers *wire,
				  void *context)
{
	return same_wire(wire, context) && available(context) ? CB_SUCCESS : CB_ERR;
}

static enum cb_err controller_prepare(const struct matisse_dxio_init_buffers *wire,
				     unsigned int use, void *context)
{
	return same_wire(wire, context) ? prepare(use, context) : CB_ERR;
}

static enum cb_err controller_consume(const struct matisse_dxio_init_buffers *wire,
				     void *context)
{
	return same_wire(wire, context) ? consume(context) : CB_ERR;
}

static enum matisse_dxio_event_result event(const struct matisse_dxio_poll_result *result,
		unsigned int phase, const struct matisse_dxio_transport *transport, void *context)
{
	struct board_session *session = context;

	return matisse_handle_dxio_event(&session->state->event_input, &session->state->events,
					result, phase, transport, &session->event_io);
}

static enum cb_err finish(const struct matisse_dxio_init_buffers *wire,
			 unsigned int phase, bool completed, void *context)
{
	struct board_session *session = context;

	return same_wire(wire, session) ?
		matisse_finish_dxio_events_phase(&session->state->event_input,
			&session->state->events, phase, completed, &session->event_io) : CB_ERR;
}

enum cb_err b565d4_matisse_run_dxio(const struct b565d4_matisse_dxio_input *input,
		struct b565d4_matisse_dxio_state *state,
		const struct b565d4_matisse_dxio_io *io)
{
	if (!valid_input(input, state, io))
		return CB_ERR;
	struct board_session session = { .state = state, .io = io };

	stopwatch_init_msecs_expire(&session.budget, 120000);
	state->phase = B565D4_MATISSE_DXIO_ADMITTING;
	if (io->admit(input, state, io->context) != CB_SUCCESS || !available(&session))
		goto failed;
	state->phase = B565D4_MATISSE_DXIO_PREPARING;
	state->port_count = b565d4_matisse_build_ports(input->slot_mode, state->ports,
						    B565D4_MATISSE_ACTIVE_PORTS);
	if (!state->port_count)
		goto failed;
	init_link_policy(&state->link);
	state->link.reconfigure.token_1d3 = input->acs;
	state->link.reconfigure.token_2e6 = input->reference_link_feature;
	state->link.reconfigure.token_165 = input->reference_capability_features;
	state->link.reconfigure.token_231 = input->reference_capability_features;
	if (!b565d4_matisse_set_slot_speeds(state->ports, state->port_count,
			input->max_cpu_link_speed, input->cpu_slot_speed, input->cpu_gpp_speed))
		goto failed;
	state->platform_bytes = matisse_build_dxio_platform(state->ports, state->port_count,
			input->pspp_policy, input->force_reset_id,
			input->platform, input->platform_capacity);
	if (!state->platform_bytes)
		goto failed;
	state->backing = (struct matisse_dxio_dma_input) {
		.wire = { .platform = input->platform_physical,
			.platform_dwords = (uint32_t)(state->platform_bytes / 4),
			.ancillary = input->ancillary_physical,
			.ancillary_dwords = input->ancillary_dwords },
		.platform = input->platform, .ancillary = input->ancillary,
	};
	state->mapping = (struct matisse_dxio_mapped_input) {
		.root_index = 3, .bus = 0, .wrappers = state->link.wrappers,
		.wrapper_count = B565D4_MATISSE_WRAPPERS,
		.slots = state->link.slots, .slot_count = B565D4_MATISSE_TOPOLOGY_PORTS,
		.ports = state->ports, .port_count = state->port_count,
		.overrides = state->link.overrides, .initial_controls = state->link.controls,
		.policy = &state->link.reconfigure, .platform = input->platform,
		.platform_bytes = state->platform_bytes,
		.platform_physical = input->platform_physical,
	};
	state->event_input = (struct matisse_dxio_events_input) {
		.mapping = &state->mapping, .configured = &state->link.configured,
	};
	session.dma_io = (struct matisse_dxio_dma_io) {
		.admit = dma_admit, .may_access = dma_available, .context = &session,
	};
	session.event_io = (struct matisse_dxio_events_io) {
		.admit = event_admit, .may_access = available, .prepare = prepare,
		.consume = consume, .smn_read = smn_read, .smn_write = smn_write,
		.pci_read = pci_read, .pci_write = pci_write, .report = report, .context = &session,
		.reset = reset,
	};
	if (matisse_begin_dxio_dma(&state->backing, &state->dma, &session.dma_io) != CB_SUCCESS ||
	    matisse_begin_dxio_events(&state->event_input, &state->events,
				      &session.event_io) != CB_SUCCESS)
		goto failed;
	const struct matisse_dxio_init_hooks hooks = {
		.admit = controller_admit, .prepare = controller_prepare,
		.consume = controller_consume, .event = event, .finish_phase = finish,
		.context = &session, .may_access = available,
	};

	state->phase = B565D4_MATISSE_DXIO_RUNNING;
	if (matisse_dxio_run(&dxio_init_policy, &state->backing.wire,
			     &state->controller, &hooks) != CB_SUCCESS || !available(&session))
		goto failed;
	state->phase = B565D4_MATISSE_DXIO_FINISHED;
	return CB_SUCCESS;

failed:
	state->phase = B565D4_MATISSE_DXIO_FAILED;
	return CB_ERR;
}
