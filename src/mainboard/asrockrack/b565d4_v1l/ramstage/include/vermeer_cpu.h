/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef B565D4_VERMEER_CPU_H
#define B565D4_VERMEER_CPU_H

#define B565D4_VERMEER_CSTATE_IO_BASE 0x413
#define B565D4_VERMEER_C2_IO (B565D4_VERMEER_CSTATE_IO_BASE + 1)
#define B565D4_VERMEER_C2_LATENCY 18

void b565d4_vermeer_init_msrs(void);

#endif
