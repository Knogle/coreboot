/* SPDX-License-Identifier: GPL-2.0-only */

#include <limits.h>
#include "dxio_poll.h"

bool matisse_decode_dxio_poll(const uint32_t reply[6],
			     struct matisse_dxio_poll_result *result)
{
	struct matisse_dxio_poll_result decoded = { 0 };
	uintptr_t input = (uintptr_t)reply;
	uintptr_t output = (uintptr_t)result;
	uint8_t status, type;

	if (!reply || !result || input > UINTPTR_MAX - sizeof(decoded.raw) ||
	    output > UINTPTR_MAX - sizeof(*result) ||
	    (input < output + sizeof(*result) && output < input + sizeof(decoded.raw)))
		return false;
	status = (uint8_t)reply[0];
	type = (uint8_t)reply[1];
	if (status == 2 && type == 2) {
		decoded.kind = MATISSE_DXIO_POLL_ERROR;
		decoded.severity = 8;
		decoded.return_from_poll = true;
	} else if (status == 3 && type == 0) {
		decoded.kind = MATISSE_DXIO_POLL_COMPLETE;
		decoded.return_from_poll = true;
	} else if (status == 3 && type == 4) {
		decoded.kind = MATISSE_DXIO_POLL_STATE;
		decoded.acknowledge = true;
		decoded.needs_board_handler = true;
	} else if (status == 3 && type == 2) {
		decoded.kind = MATISSE_DXIO_POLL_EVENT;
		decoded.return_from_poll = reply[2] == 5 || reply[2] == 13;
		decoded.acknowledge = !decoded.return_from_poll;
		decoded.needs_board_handler = reply[2] == 5 || reply[2] == 7 ||
			reply[2] == 15;
	} else {
		return false;
	}
	for (size_t i = 0; i < 6; i++)
		decoded.raw[i] = reply[i];
	*result = decoded;
	return true;
}
