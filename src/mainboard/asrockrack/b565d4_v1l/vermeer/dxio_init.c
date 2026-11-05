/* SPDX-License-Identifier: GPL-2.0-only */

#include "dxio_init.h"
#include "dxio_service.h"
#include <timer.h>

#define DXIO_BUDGET_MS 120000
#define DXIO_MAX_REQUESTS 1024
#define DXIO_MAX_POLLS 256

struct dxio_session {
	struct matisse_dxio_init_state *state;
	struct stopwatch *budget;
	const struct matisse_dxio_init_hooks *hooks;
};

static bool available(struct dxio_session *session)
{
	struct matisse_dxio_init_state *s = session->state;

	if (s->failure)
		return false;
	if (stopwatch_expired(session->budget) || s->attempted >= DXIO_MAX_REQUESTS) {
		s->failure = MATISSE_DXIO_BUDGET_EXPIRED;
		return false;
	}
	if (session->hooks->may_access &&
	    !session->hooks->may_access(session->hooks->context)) {
		s->failure = MATISSE_DXIO_ACCESS_DENIED;
		return false;
	}
	return true;
}

static enum cb_err request(void *context, uint32_t opcode,
			   const uint32_t arguments[5], struct smu_payload *reply)
{
	struct dxio_session *session = context;
	struct matisse_dxio_init_state *s = session->state;
	struct smu_payload payload = { 0 };

	if (!available(session))
		return CB_ERR;
	for (size_t i = 0; i < 5; i++)
		payload.msg[i + 1] = arguments ? arguments[i] : 0;
	s->last_opcode = opcode;
	s->last_reply = payload;
	s->attempted++;
	asm volatile ("" : : : "memory");
	s->last_transport_status = matisse_dxio_request(opcode, &s->last_reply);
	if (s->last_transport_status != CB_SUCCESS) {
		s->failure = MATISSE_DXIO_TRANSPORT_FAILED;
		return CB_ERR;
	}
	s->completed++;
	if (reply)
		*reply = s->last_reply;
	return CB_SUCCESS;
}

static void record_nonfatal(struct matisse_dxio_init_state *s)
{
	if (matisse_dxio_status(s->last_reply.msg[0]) != 3)
		s->nonfatal_statuses++;
}

static bool poll_phase(struct dxio_session *session,
		       const struct matisse_dxio_init_hooks *hooks,
		       const struct matisse_dxio_transport *transport, unsigned int phase)
{
	struct matisse_dxio_init_state *s = session->state;

	while (s->polls < DXIO_MAX_POLLS) {
		enum matisse_dxio_event_result handled;

		if (request(session, 9, NULL, NULL) != CB_SUCCESS)
			return false;
		s->polls++;
		if (!matisse_decode_dxio_poll(s->last_reply.msg, &s->last_event)) {
			s->failure = MATISSE_DXIO_REPLY_INVALID;
			return false;
		}
		if (s->last_event.kind == MATISSE_DXIO_POLL_ERROR) {
			s->failure = MATISSE_DXIO_FIRMWARE_ERROR;
			return false;
		}
		if (s->last_event.kind == MATISSE_DXIO_POLL_COMPLETE)
			return true;
		s->events++;
		if (s->last_event.needs_board_handler || s->last_event.return_from_poll) {
			if (!hooks->event || !available(session)) {
				if (!s->failure)
					s->failure = MATISSE_DXIO_HANDLER_FAILED;
				return false;
			}
			handled = hooks->event(&s->last_event, phase, transport, hooks->context);
			if (s->failure)
				return false;
			if (stopwatch_expired(session->budget)) {
				s->failure = MATISSE_DXIO_BUDGET_EXPIRED;
				return false;
			}
			if (handled == MATISSE_DXIO_EVENT_FAILED) {
				s->failure = MATISSE_DXIO_HANDLER_FAILED;
				return false;
			}
			if (s->last_event.return_from_poll) {
				if (handled == MATISSE_DXIO_EVENT_PHASE_DONE)
					return true;
				s->failure = MATISSE_DXIO_PHASE_UNCONFIRMED;
				return false;
			}
			if (handled != MATISSE_DXIO_EVENT_HANDLED) {
				s->failure = MATISSE_DXIO_PHASE_UNCONFIRMED;
				return false;
			}
		}
		if (request(session, 0x308, NULL, NULL) != CB_SUCCESS)
			return false;
		record_nonfatal(s);
	}
	s->failure = MATISSE_DXIO_BUDGET_EXPIRED;
	return false;
}

static bool poll_and_finish_phase(struct dxio_session *session,
		const struct matisse_dxio_init_hooks *hooks,
		const struct matisse_dxio_transport *transport, unsigned int phase)
{
	struct matisse_dxio_init_state *s = session->state;
	bool completed = poll_phase(session, hooks, transport, phase);

	if (hooks->finish_phase) {
		uint32_t bit = 1U << (phase - 1);
		enum cb_err status;

		s->phase_finish_attempted |= bit;
		status = hooks->finish_phase(&s->buffers, phase, completed, hooks->context);
		s->last_phase_finish_status = status;
		if (status != CB_SUCCESS) {
			s->phase_finish_failures |= bit;
			if (!s->failure)
				s->failure = MATISSE_DXIO_PHASE_CLEANUP_FAILED;
			return false;
		}
	}
	return completed;
}

enum cb_err matisse_dxio_run(const struct matisse_dxio_init_policy *policy,
			   const struct matisse_dxio_init_buffers *buffers,
			   struct matisse_dxio_init_state *state,
			   const struct matisse_dxio_init_hooks *hooks)
{
	struct stopwatch budget;
	struct dxio_session session = { .state = state, .budget = &budget, .hooks = hooks };
	const struct matisse_dxio_transport transport = { .request = request, .context = &session };
	size_t count;

	if (!policy || !buffers || !state || !hooks || !hooks->admit || !hooks->prepare ||
	    !hooks->consume || state->phase != MATISSE_DXIO_NOT_STARTED)
		return CB_ERR;
	count = matisse_build_dxio_init_requests(policy, buffers, state->requests,
						MATISSE_DXIO_MAX_INIT_REQUESTS);
	if (!count)
		return CB_ERR;
	state->planned = (uint32_t)count;
	state->buffers = *buffers;
	state->phase = MATISSE_DXIO_ADMITTING;
	stopwatch_init_msecs_expire(&budget, DXIO_BUDGET_MS);
	if (hooks->admit(&state->buffers, hooks->context) != CB_SUCCESS) {
		state->failure = MATISSE_DXIO_ADMISSION_FAILED;
		goto failed;
	}
	state->phase = MATISSE_DXIO_SENDING;
	for (size_t i = 0; i < count; i++) {
		const struct matisse_dxio_init_request *r = &state->requests[i];
		uint8_t status;

		if (r->opcode == 0x23 && r->arguments[1]) {
			if (!available(&session))
				goto failed;
			if (hooks->prepare(&state->buffers, r->arguments[4], hooks->context)
			    != CB_SUCCESS) {
				state->failure = MATISSE_DXIO_BUFFER_FAILED;
				goto failed;
			}
		}
		if (request(&session, r->opcode, r->arguments, NULL) != CB_SUCCESS)
			goto failed;
		status = matisse_dxio_status(state->last_reply.msg[0]);
		if (r->opcode == 0x13) {
			state->version_reply = state->last_reply;
			state->version_word2 = state->last_reply.msg[2];
			state->version_word3 = state->last_reply.msg[3];
			state->legacy_zero_version = state->version_word3 == 0;
		} else if (r->opcode == 0) {
			state->initial_reply = state->last_reply;
			if (!state->legacy_zero_version && (uint8_t)state->last_reply.msg[0] != 3) {
				state->failure = MATISSE_DXIO_INITIAL_STATUS_FAILED;
				goto failed;
			}
			record_nonfatal(state);
		} else if (r->opcode == 0x23 && r->arguments[1] && status != 3) {
			state->failure = MATISSE_DXIO_LOAD_FAILED;
			goto failed;
		} else if (r->opcode == 0x307 && status != 3) {
			state->failure = MATISSE_DXIO_START_FAILED;
			goto failed;
		} else {
			record_nonfatal(state);
		}
	}
	state->phase = MATISSE_DXIO_POLLING_FIRST;
	if (!poll_and_finish_phase(&session, hooks, &transport, 1))
		goto failed;
	state->completed_phases = 1;
	state->phase = MATISSE_DXIO_ADVANCING;
	if (request(&session, 0x308, NULL, NULL) != CB_SUCCESS)
		goto failed;
	record_nonfatal(state);
	state->phase = MATISSE_DXIO_POLLING_SECOND;
	if (!poll_and_finish_phase(&session, hooks, &transport, 2))
		goto failed;
	state->completed_phases = 2;
	if (!state->legacy_zero_version) {
		const uint32_t args[5] = { 0, (uint32_t)state->buffers.platform,
			state->buffers.platform_dwords, 0, 0 };

		state->phase = MATISSE_DXIO_READING_BACK;
		if (!available(&session))
			goto failed;
		if (hooks->prepare(&state->buffers, 14, hooks->context) != CB_SUCCESS) {
			state->failure = MATISSE_DXIO_BUFFER_FAILED;
			goto failed;
		}
		if (request(&session, 0x14, args, NULL) != CB_SUCCESS)
			goto failed;
		if (matisse_dxio_status(state->last_reply.msg[0]) != 3 ||
		    hooks->consume(&state->buffers, hooks->context) != CB_SUCCESS) {
			state->failure = MATISSE_DXIO_BUFFER_FAILED;
			goto failed;
		}
	}
	if (stopwatch_expired(&budget)) {
		state->failure = MATISSE_DXIO_BUDGET_EXPIRED;
		goto failed;
	}
	state->phase = MATISSE_DXIO_FINISHED;
	return CB_SUCCESS;

failed:
	state->phase = MATISSE_DXIO_FAILED;
	return CB_ERR;
}
