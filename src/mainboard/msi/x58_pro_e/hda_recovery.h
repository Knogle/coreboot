/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef X58_HDA_RECOVERY_H
#define X58_HDA_RECOVERY_H

#include <stdbool.h>

struct device;

/* RAM-only request; never writes CMOS or causes a reset. */
bool x58_hda_command(int argc, char **argv);
void x58_hda_freeze(void);
void x58_hda_status(void);

/* The ICH10 one-shot diagnostic path is a board operation, not an ICH10
 * driver policy. It preserves resource ownership when the default is defer. */
void x58_ich1b_hda_init(struct device *dev);

#endif
