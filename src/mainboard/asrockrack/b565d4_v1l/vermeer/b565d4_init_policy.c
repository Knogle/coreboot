/* SPDX-License-Identifier: GPL-2.0-only */

#include "b565d4_init_policy.h"

const struct matisse_dxio_init_policy b565d4_matisse_dxio_init_policy = {
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

const struct matisse_usb_policy b565d4_matisse_usb_policy = {
	.overcurrent_map = 0xffffffffU,
	.overcurrent_map_ext = 0xff,
	.force_gen1 = 0xff,
	.enabled = true,
	.ras = true,
};
