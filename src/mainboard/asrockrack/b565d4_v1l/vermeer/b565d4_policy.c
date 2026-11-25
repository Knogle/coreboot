/* SPDX-License-Identifier: GPL-2.0-only */

#include "b565d4_policy.h"

/* Original B565D4-V1L Matisse OEM-published A26 table, consumed fields only.
 * The original root constructor supplies the two unreversed16-lane groups.
 * The second port's descending endpoints retain its own lane reversal.
 */
static const struct matisse_dxio_port base_ports[] = {
	{
		.lanes = { 16, 31, 16, 31, false },
		.first = 16, .last = 31,
		.policy = {
			.type = 1, .reset_id = 8,
			.routing = { 0x19, 1 }, .link = { 0x20, 5 },
			.miscellaneous = 0x88,
		},
	}, {
		.lanes = { 0, 15, 0, 15, false },
		.first = 7, .last = 0,
		.policy = {
			.type = 1, .reset_id = 7,
			.routing = { 0x09, 1 }, .link = { 0x20, 2 },
			.miscellaneous = 0x88,
		},
	}, {
		.lanes = { 0, 15, 0, 15, false },
		.first = 8, .last = 11,
		.policy = {
			.type = 1, .reset_id = 2,
			.routing = { 0x09, 2 }, .link = { 0x20, 0 },
			.miscellaneous = 0x04,
		},
	},
};

size_t b565d4_matisse_build_ports(enum b565d4_matisse_slot_mode mode,
				struct matisse_dxio_port *ports, size_t capacity)
{
	const size_t count = mode == B565D4_MATISSE_PCIE_FOUR_X4 ? 6 : 3;

	if (!ports || (mode != B565D4_MATISSE_PCIE_X16 &&
		mode != B565D4_MATISSE_PCIE_FOUR_X4) || capacity < count)
		return 0;
	for (size_t i = 0; i < 3; i++)
		ports[i] = base_ports[i];
	if (mode == B565D4_MATISSE_PCIE_FOUR_X4) {
		ports[0].first = 28;
		for (unsigned int i = 0; i < 3; i++) {
			ports[i + 3] = base_ports[0];
			ports[i + 3].first = (uint8_t)(24 - 4 * i);
			ports[i + 3].last = (uint8_t)(27 - 4 * i);
			ports[i + 3].policy.routing[1] = (uint8_t)(i + 2);
		}
	}
	return count;
}

/* The OEM policy indices are independent of enumerated bus numbers:
 * 0 and split policies3..5 are PCIE6, 1 is PCIE4, 2 is the PCH uplink.
 */
unsigned int b565d4_matisse_cpu_slot_speed(unsigned int policy, size_t count,
		unsigned int ceiling, unsigned int gfx_speed, unsigned int gpp_speed)
{
	if ((count != 3 && count != 6) || policy >= count || policy == 2 ||
	    ceiling > 4 || gfx_speed > 3 || gpp_speed > 3)
		return 0;
	unsigned int requested = policy == 1 ? gpp_speed : gfx_speed;

	if (!requested)
		return ceiling;
	return ceiling && requested > ceiling ? ceiling : requested;
}

bool b565d4_matisse_set_slot_speeds(struct matisse_dxio_port *ports, size_t count,
		unsigned int ceiling, unsigned int gfx_speed, unsigned int gpp_speed)
{
	if (!ports || (count != 3 && count != 6) || ceiling > 4 ||
	    gfx_speed > 3 || gpp_speed > 3)
		return false;
	for (size_t i = 0; i < count; i++) {
		unsigned int speed = b565d4_matisse_cpu_slot_speed(i, count, ceiling,
								 gfx_speed, gpp_speed);

		if (speed)
			ports[i].policy.link[0] =
				(uint8_t)((ports[i].policy.link[0] & 0xf0) | speed);
	}
	return true;
}
