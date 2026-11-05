/* SPDX-License-Identifier: GPL-2.0-only */

#include "b565d4_link_policy.h"
#include "nbio_memory.h"
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

bool b565d4_matisse_build_link_policy(struct b565d4_matisse_link_policy *policy)
{
	if (!policy)
		return false;
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
	/* The remaining two NBIF slots have no original port descriptor.
	 * Their controls and the six active-port overrides remain zero.
	 */
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
	return true;
}
