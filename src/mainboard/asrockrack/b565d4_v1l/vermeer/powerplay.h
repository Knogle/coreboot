/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef B565D4_VERMEER_POWERPLAY_H
#define B565D4_VERMEER_POWERPLAY_H

#include <stdbool.h>

/* Called once after the exact CPU/SMU identity check, before feature policy. */
bool b565d4_vermeer_powerplay_prepare(void);

#endif
