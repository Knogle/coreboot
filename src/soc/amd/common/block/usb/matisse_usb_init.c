/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/matisse_usb_init.h>
#include <amdblocks/matisse_usb_service.h>
#include <timer.h>

#define USB_POLICY_BUDGET_MS 120000

enum cb_err matisse_usb_run_policy(const struct matisse_usb_policy *policy,
				   struct matisse_usb_init_state *state)
{
	struct stopwatch budget;
	size_t count;

	if (!policy || !state || state->phase != MATISSE_USB_NOT_STARTED)
		return CB_ERR;

	count = matisse_build_usb_requests(policy, state->requests, MATISSE_USB_MAX_REQUESTS);
	if (!count)
		return CB_ERR;
	state->planned = (uint32_t)count;
	state->completed = 0;
	state->phase = MATISSE_USB_SENDING;
	stopwatch_init_msecs_expire(&budget, USB_POLICY_BUDGET_MS);

	for (size_t i = 0; i < count; i++) {
		const struct matisse_usb_request *request = &state->requests[i];
		enum cb_err status;

		if (stopwatch_expired(&budget))
			goto failed;
		/* Publish phase and completed progress before the next mailbox store. */
		asm volatile ("" : : : "memory");
		if (request->message == SMC_MSG_USB_BOOT) {
			status = matisse_usb_boot();
		} else {
			status = matisse_usb_config_update(request->args[0], request->args[1],
						  request->args[2], request->args[3]);
		}
		if (status != CB_SUCCESS)
			goto failed;
		state->completed++;
	}

	state->phase = MATISSE_USB_REQUESTS_SENT;
	return CB_SUCCESS;

failed:
	state->phase = MATISSE_USB_FAILED;
	return CB_ERR;
}
