/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AMD_BLOCK_PROMONTORY_H
#define AMD_BLOCK_PROMONTORY_H

struct device;

/* A maximum speed of zero uses the highest generation supported by both ends. */
void amd_promontory_init_uplink(struct device *root, unsigned int max_speed);

#endif
