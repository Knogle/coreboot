/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_DXIO_POLL_H
#define AMD_MATISSE_DXIO_POLL_H

#include <types.h>

enum matisse_dxio_poll_kind {
	MATISSE_DXIO_POLL_COMPLETE,
	MATISSE_DXIO_POLL_STATE,
	MATISSE_DXIO_POLL_EVENT,
	MATISSE_DXIO_POLL_ERROR,
};

struct matisse_dxio_poll_result {
	uint32_t raw[6];
	enum matisse_dxio_poll_kind kind;
	uint8_t severity;
	bool acknowledge;
	bool return_from_poll;
	bool needs_board_handler;
};

/* Pure preparation decoder for original operation9 replies. It performs no
 * I/O and follows no returned pointer. Direct reply status/type use their low
 * bytes, unlike the separate operation-wrapper status normalization.
 *
 * An acknowledgement is operation308 followed by another poll, after handling
 * any pending board event. Raw event numbers and all other words are retained.
 * Events5/13 return from the original poller; they are not by themselves proof
 * of link training. Events5/7/15 and state messages require caller-owned board/port
 * handling. Other type2 events retain the original acknowledge-and-continue
 * behavior, without a diagnostic stop for an unrecognized event number.
 *
 * Returns false without output stores for invalid status/type, null, wrapping
 * pointers or overlapping input/output. A recognized firmware error is a true
 * result with severity8 and return_from_poll set. The caller owns mailbox
 * transport, bounds on polling, returned buffer ownership and phase completion.
 */
bool matisse_decode_dxio_poll(const uint32_t reply[6],
			     struct matisse_dxio_poll_result *result);

#endif /* AMD_MATISSE_DXIO_POLL_H */
