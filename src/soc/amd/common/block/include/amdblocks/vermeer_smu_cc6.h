/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_BLOCK_VERMEER_SMU_CC6_H
#define AMD_BLOCK_VERMEER_SMU_CC6_H

#include <stdbool.h>
#include <stdint.h>

void vermeer_smu_cc6_cpu_prepare(uint16_t cstate_io_base);
void vermeer_smu_cc6_enable(void);
bool vermeer_smu_cc6_ready(void);

#endif /* AMD_BLOCK_VERMEER_SMU_CC6_H */
