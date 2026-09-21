/* SPDX-License-Identifier: GPL-2.0-only */

#include <device/azalia_device.h>

/*
 * Use the standard ICH10 HDA controller and codec discovery. Board-specific
 * jack routing and PC-beep verbs are not yet qualified; do not borrow another
 * board's pin defaults. The generic weak codec lookup leaves them unchanged.
 */
const u32 pc_beep_verbs[] = {};
const u32 pc_beep_verbs_size = ARRAY_SIZE(pc_beep_verbs);
