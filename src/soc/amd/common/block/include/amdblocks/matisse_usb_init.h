/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_USB_INIT_H
#define AMD_MATISSE_USB_INIT_H

#include <amdblocks/matisse_usb_policy.h>
#include <types.h>

enum matisse_usb_phase {
	MATISSE_USB_NOT_STARTED,
	MATISSE_USB_SENDING,
	MATISSE_USB_REQUESTS_SENT,
	MATISSE_USB_FAILED,
};

/* Caller-owned, initially zeroed persistent workspace, not an early stack
 * allocation. Keep it after failure: completed counts acknowledged requests,
 * and a failing request may already have changed firmware state.
 */
struct matisse_usb_init_state {
	uint32_t phase, planned, completed;
	struct matisse_usb_request requests[MATISSE_USB_MAX_REQUESTS];
};

/* Experimental desktop backend. The caller selects
 * the board policy and phase, owns SMN/SMU exclusively and supplies distinct
 * policy/state objects. Sends each request at most once, stops on the first
 * error, checks a whole-program budget before each submission and refuses
 * reentry without I/O. The common driver's polls bound an in-flight call.
 * CB_SUCCESS establishes mailbox acknowledgements, not USB enumeration.
 */
enum cb_err matisse_usb_run_policy(const struct matisse_usb_policy *policy,
				   struct matisse_usb_init_state *state);

#endif /* AMD_MATISSE_USB_INIT_H */
