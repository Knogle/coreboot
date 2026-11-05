/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef B565D4_VERMEER_CSTATES_H
#define B565D4_VERMEER_CSTATES_H

#include <stdbool.h>

#define B565D4_VERMEER_C2_IO 0x414
#define B565D4_VERMEER_C2_LATENCY 18

void b565d4_vermeer_cc6_cpu_prepare(void);
void b565d4_vermeer_cc6_enable(void);
bool b565d4_vermeer_cc6_ready(void);

#endif
