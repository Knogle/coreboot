/* SPDX-License-Identifier: GPL-2.0-only */

#include <limits.h>
#include <timer.h>
#include "dxio_reconfigure.h"

struct reconfigure_session {
	const struct matisse_dxio_reconfigure_input *input;
	struct matisse_dxio_reconfigure_state *state;
	const struct matisse_dxio_reconfigure_io *io;
	struct stopwatch budget;
};

static bool valid_range(const void *pointer, size_t bytes)
{
	return (!bytes || pointer) && (uintptr_t)pointer <= UINTPTR_MAX - bytes;
}

static bool overlap(const void *first, size_t first_bytes,
		    const void *second, size_t second_bytes)
{
	return first_bytes && second_bytes &&
		(uintptr_t)first < (uintptr_t)second + second_bytes &&
		(uintptr_t)second < (uintptr_t)first + first_bytes;
}

static unsigned int map_slot(uint8_t encoding)
{
	return ((encoding & 15U) << 3) + (encoding >> 4);
}

static bool available(struct reconfigure_session *session)
{
	if (stopwatch_expired(&session->budget) || session->state->attempted >= 16384) {
		session->state->failure = MATISSE_DXIO_RECONFIGURE_BUDGET_EXPIRED;
		return false;
	}
	return true;
}

static bool access_register(struct reconfigure_session *session, uint32_t address,
			    uint32_t *value, bool write)
{
	struct matisse_dxio_reconfigure_state *state = session->state;

	if (!available(session))
		return false;
	state->last_address = address;
	state->attempted++;
	asm volatile ("" : : : "memory");
	if (write) {
		state->last_value = *value;
		state->last_status = session->io->write(address, *value, session->io->context);
	} else {
		state->last_status = session->io->read(address, value, session->io->context);
		state->last_value = *value;
	}
	if (state->last_status != CB_SUCCESS) {
		state->failure = MATISSE_DXIO_RECONFIGURE_IO_FAILED;
		return false;
	}
	if (write)
		state->writes++;
	else
		state->reads++;
	return available(session);
}

static bool rmw(struct reconfigure_session *session, uint32_t address,
		uint32_t and_mask, uint32_t value)
{
	uint32_t before = 0;

	if (!access_register(session, address, &before, false))
		return false;
	value |= before & and_mask;
	return access_register(session, address, &value, true);
}

static bool select_value(struct reconfigure_session *session, unsigned int wrapper,
			 unsigned int number, uint32_t value)
{
	unsigned int block = (session->input->root_index + 4U * wrapper) & 255U;
	uint32_t address = 0x090467acU + ((uint32_t)block << 12);
	uint32_t index = (number & 65535U) - 0x20000U;

	return access_register(session, address, &index, true) &&
		access_register(session, address + 4, &value, true);
}

static bool broadcast_port_selectors(struct reconfigure_session *session,
				    unsigned int wrapper, bool extended)
{
	static const uint16_t numbers[] = { 0x14, 0x15, 0x89, 0x8a, 0x8c, 0x8f };
	static const uint8_t values[] = { 1, 1, 1, 2, 7, 5 };

	for (size_t i = extended ? 0 : 2; i < sizeof(values); i++)
		if (!select_value(session, wrapper, numbers[i], values[i]))
			return false;
	for (unsigned int port = 0; port < 8; port++)
		if (!select_value(session, wrapper, 0x112U + 0x61U * port, 1))
			return false;
	return true;
}

static bool configure_port(struct reconfigure_session *session, uint8_t encoding)
{
	const struct matisse_dxio_reconfigure_policy *policy = session->input->policy;
	struct matisse_dxio_link_controls *c = &session->state->controls[map_slot(encoding)];
	unsigned int wrapper = encoding & 15U, port = encoding >> 4;
	uint32_t wrapper_offset = (session->input->root_index + 4U * wrapper) << 20;
	uint32_t offset = wrapper_offset + ((uint32_t)port << 12);
	uint32_t port_selector = 0x61U * port;
	uint16_t eq = c->equalization;
	uint8_t eqx = c->equalization_extended;

	c->flags_29 = (c->flags_29 & 15U) | ((c->flags_27 & 3U) << 4);
	if (!select_value(session, wrapper, 0x9f, (eq >> 4) & 7U) ||
	    !select_value(session, wrapper, 0xa0, (eq >> 11) & 7U) ||
	    !select_value(session, wrapper, 0xa1, eq & 15U) ||
	    !select_value(session, wrapper, 0xa2, (eq >> 7) & 15U) ||
	    !select_value(session, wrapper, 0xa3, eqx & 15U) ||
	    !select_value(session, wrapper, 0xa4, eqx >> 4))
		return false;
	if (!rmw(session, 0x111402d8U + offset, 0xfffeffffU, (c->flags_27 & 4U) << 14) ||
	    !rmw(session, 0x11140374U + offset, 0xfffff7ffU, (c->flags_28 & 4U) << 9) ||
	    !rmw(session, 0x111402d8U + offset, 0xfffff7ffU, (c->flags_27 & 8U) << 8) ||
	    !rmw(session, 0x11140374U + offset, 0xfffffeffU, (c->flags_28 & 8U) << 5) ||
	    !rmw(session, 0x111402d8U + offset, 0xfffffcffU, (c->flags_27 & 3U) << 8) ||
	    !rmw(session, 0x11140374U + offset, 0xfffffffcU, c->flags_28 & 3U))
		return false;
	if (policy->token_fc == 1 && (c->flags_2e & 0x80U) &&
	    !broadcast_port_selectors(session, wrapper, true))
		return false;
	if (!rmw(session, 0x11140390U + offset, 0xffffdfffU,
		 (uint32_t)(policy->token_1b5 == 1) << 13))
		return false;
	if ((c->flags_2e & 0x80U) && !broadcast_port_selectors(session, wrapper, false))
		return false;
	if (!select_value(session, wrapper, 0x10aU + port_selector, 1) ||
	    !select_value(session, wrapper, 0x10bU + port_selector, 1))
		return false;
	if (c->flags_31 & 0x30U) {
		if ((c->flags_31 & 0x20U) &&
		    (!select_value(session, wrapper, 0x125U + port_selector, 1) ||
		     !select_value(session, wrapper, (port + 3U) * 0x61U, 1)))
			return false;
		if ((c->flags_31 & 0x10U) &&
		    (!select_value(session, wrapper, 0x126U + port_selector, 1) ||
		     !select_value(session, wrapper, 0x124U + port_selector, 1)))
			return false;
		if (!select_value(session, wrapper, 0x129U + port_selector, 10) ||
		    !select_value(session, wrapper, 0x12aU + port_selector, 1) ||
		    !select_value(session, wrapper, 0x12cU + port_selector, 15))
			return false;
	}
	if ((c->flags_2e & 0x40U) &&
	    !rmw(session, 0x11180604U + wrapper_offset, 0xfffffffeU, 1))
		return false;
	if (policy->token_155 != 255)
		c->flags_32 = (c->flags_32 & 0xfeU) | (policy->token_155 & 1U);
	if ((c->flags_32 & 1U) && !select_value(session, wrapper, 0x5a, 1))
		return false;
	if (policy->token_319 != 255 &&
	    ((c->flags_2e & 15U) == 15 || (policy->token_182 & 4U))) {
		c->flags_2e = (c->flags_2e & 0xf0U) | (policy->token_319 & 15U);
		c->flags_30 = (c->flags_30 & 0xf3U) | ((policy->token_1b7 & 3U) << 2);
		c->flags_2f = (policy->token_181 & 15U) | ((policy->token_4a << 4) & 0xf0U);
	}
	if (policy->token_1fa != 255) {
		c->flags_30 = (c->flags_30 & 15U) | ((policy->token_1fa << 4) & 0xf0U);
		c->flags_31 = (c->flags_31 & 0xfcU) | (policy->token_2c9 & 3U);
		c->flags_31 = (c->flags_31 & 0xf3U) | ((policy->token_22d & 3U) << 2);
	}
	if ((c->flags_2e & 15U) == 1 &&
	    !select_value(session, wrapper, 0xfbU + port_selector,
			  (c->flags_30 & 0xf0U) != 0x10))
		return false;
	if ((c->flags_30 & 0xf0U) == 0x10 &&
	    !select_value(session, wrapper, 0xfcU + port_selector, 1))
		return false;
	return select_value(session, wrapper, 0xf9U + port_selector, (c->flags_2a >> 2) & 3U);
}

static bool configure_wrapper(struct reconfigure_session *session, unsigned int wrapper)
{
	const struct matisse_dxio_reconfigure_policy *p = session->input->policy;
	uint32_t offset = (session->input->root_index + 4U * wrapper) << 20;

	if (!select_value(session, wrapper, 0x75, wrapper + 2U))
		return false;
	if (wrapper != 2) {
		if (p->token_30a == 1)
			for (unsigned int port = 0; port < 8; port++)
				if (!select_value(session, wrapper, 0x110U + 0x61U * port, 1) ||
				    !select_value(session, wrapper, 0x111U + 0x61U * port, 1))
					return false;
		if (p->token_165) {
			if (!select_value(session, wrapper, 0x10, 1))
				return false;
			for (unsigned int port = 0; port < 8; port++) {
				uint32_t value = 0;

				if (!access_register(session, 0x11100064U + offset +
						     ((uint32_t)port << 12), &value, false))
					return false;
				if ((value & 15U) == 4 &&
				    !select_value(session, wrapper, 0x12eU + 0x61U * port, 1))
					return false;
			}
		}
		if (p->token_231)
			for (unsigned int port = 0; port < 8; port++) {
				uint32_t value = 0;

				if (!access_register(session, 0x11100064U + offset +
						     ((uint32_t)port << 12), &value, false))
					return false;
				if ((value & 15U) == 4 &&
				    (!select_value(session, wrapper, 0x12fU + 0x61U * port, 1) ||
				     !select_value(session, wrapper, 0x130U + 0x61U * port, 1)))
					return false;
			}
	}
	if (p->token_21c)
		for (unsigned int port = 0; port < 8; port++)
			if (!rmw(session, 0x11140288U + offset + ((uint32_t)port << 12),
				 0xff9fffffU, (p->token_21c & 3U) << 21))
				return false;
	if (wrapper != 2) {
		if (!select_value(session, wrapper, 0x7a, 0) ||
		    !select_value(session, wrapper, 0x84, 0))
			return false;
	} else if (!rmw(session, 0x11980300U + ((uint32_t)session->input->root_index << 20),
			0xfffffff8U, 0) ||
		   !rmw(session, 0x11180304U + offset, 0xffffffd7U, 0)) {
		return false;
	}
	if (!select_value(session, wrapper, 0x58, 0) ||
	    !select_value(session, wrapper, 2, p->token_2cb != 0))
		return false;
	if ((p->token_2b5 || p->token_2cb) &&
	    (!select_value(session, wrapper, 0x45, 1) ||
	     !select_value(session, wrapper, 0x46, 1)))
		return false;
	if (p->token_1d3)
		for (unsigned int n = 0x6e; n <= 0x74; n++)
			if (!select_value(session, wrapper, n, 1))
				return false;
	for (unsigned int n = 0xaf; n <= 0xb8; n++)
		if (!select_value(session, wrapper, n, 1))
			return false;
	if (!select_value(session, wrapper, 0x0b, 1))
		return false;
	if (p->token_e6 && !select_value(session, wrapper, 0x1d, 1))
		return false;
	if (!select_value(session, wrapper, 0x17, p->token_2e6 == 1))
		return false;
	return p->token_22f != 1 || select_value(session, wrapper, 0x0f, 1);
}

static bool valid_input(const struct matisse_dxio_reconfigure_input *in,
			struct matisse_dxio_reconfigure_state *state,
			const struct matisse_dxio_reconfigure_io *io)
{
	uint32_t wrappers = 0, slots = 0, allocated = 0;
	const size_t controls_bytes = MATISSE_NBIF_MAP_SLOTS * sizeof(*in->controls);

	if (!in || !state || !io || !valid_range(in, sizeof(*in)) ||
	    !valid_range(state, sizeof(*state)) || !valid_range(io, sizeof(*io)) ||
	    in->wrapper_count > 16 || in->slot_count > MATISSE_NBIF_MAP_SLOTS ||
	    !valid_range(in->wrappers, in->wrapper_count) ||
	    !valid_range(in->slots, in->slot_count * sizeof(*in->slots)) ||
	    !in->bindings || !valid_range(in->bindings, sizeof(*in->bindings)) ||
	    !in->controls || !valid_range(in->controls, controls_bytes) ||
	    !in->policy || !valid_range(in->policy, sizeof(*in->policy)) ||
	    !io->admit || !io->read || !io->write ||
	    state->phase != MATISSE_DXIO_RECONFIGURE_NOT_STARTED ||
	    in->bindings->count > MATISSE_NBIF_MAP_SLOTS ||
	    overlap(state, sizeof(*state), in, sizeof(*in)) ||
	    overlap(state, sizeof(*state), io, sizeof(*io)) ||
	    overlap(state, sizeof(*state), in->wrappers, in->wrapper_count) ||
	    overlap(state, sizeof(*state), in->slots, in->slot_count * sizeof(*in->slots)) ||
	    overlap(state, sizeof(*state), in->bindings, sizeof(*in->bindings)) ||
	    overlap(state, sizeof(*state), in->controls, controls_bytes) ||
	    overlap(state, sizeof(*state), in->policy, sizeof(*in->policy)))
		return false;
	for (size_t w = 0; w < in->wrapper_count; w++) {
		if (in->wrappers[w] > 15 || (wrappers & (1U << in->wrappers[w])))
			return false;
		wrappers |= 1U << in->wrappers[w];
	}
	for (size_t s = 0; s < in->slot_count; s++) {
		unsigned int slot = map_slot(in->slots[s].encoding);

		if ((in->slots[s].encoding >> 4) > 7 || slot >= MATISSE_NBIF_MAP_SLOTS ||
		    !(wrappers & (1U << (in->slots[s].encoding & 15U))) ||
		    (slots & (1U << slot)))
			return false;
		slots |= 1U << slot;
	}
	for (size_t b = 0; b < in->bindings->count; b++) {
		const struct matisse_dxio_engine_result *binding = &in->bindings->engines[b];
		unsigned int slot = map_slot(binding->encoding);

		if ((binding->encoding >> 4) > 7 || slot >= MATISSE_NBIF_MAP_SLOTS ||
		    slot != binding->slot || !(slots & (1U << slot)) ||
		    (allocated & (1U << slot)))
			return false;
		allocated |= 1U << slot;
	}
	return true;
}

enum cb_err matisse_run_dxio_reconfigure(const struct matisse_dxio_reconfigure_input *input,
				       struct matisse_dxio_reconfigure_state *state,
				       const struct matisse_dxio_reconfigure_io *io)
{
	struct reconfigure_session session = { .input = input, .state = state, .io = io };

	if (!valid_input(input, state, io))
		return CB_ERR;
	for (size_t s = 0; s < MATISSE_NBIF_MAP_SLOTS; s++)
		state->controls[s] = input->controls[s];
	state->phase = MATISSE_DXIO_RECONFIGURE_ADMITTING;
	state->failure = MATISSE_DXIO_RECONFIGURE_NO_FAILURE;
	state->attempted = 0;
	state->reads = 0;
	state->writes = 0;
	state->completed_wrappers = 0;
	stopwatch_init_msecs_expire(&session.budget, 120000);
	if (io->admit(input, io->context) != CB_SUCCESS) {
		state->failure = MATISSE_DXIO_RECONFIGURE_ADMISSION_FAILED;
		goto failed;
	}
	state->phase = MATISSE_DXIO_RECONFIGURE_RUNNING;
	for (size_t w = 0; w < input->wrapper_count; w++) {
		for (size_t s = 0; s < input->slot_count; s++) {
			uint8_t encoding = input->slots[s].encoding;

			if ((encoding & 15U) != input->wrappers[w])
				continue;
			for (size_t b = 0; b < input->bindings->count; b++)
				if (input->bindings->engines[b].encoding == encoding &&
				    !configure_port(&session, encoding))
					goto failed;
		}
		if (!configure_wrapper(&session, input->wrappers[w]))
			goto failed;
		state->completed_wrappers++;
	}
	if (!available(&session))
		goto failed;
	state->phase = MATISSE_DXIO_RECONFIGURE_IO_RETURNED;
	return CB_SUCCESS;

failed:
	state->phase = MATISSE_DXIO_RECONFIGURE_FAILED;
	return CB_ERR;
}
