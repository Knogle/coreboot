/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_BLOCK_VERMEER_SMU_POWER_H
#define AMD_BLOCK_VERMEER_SMU_POWER_H

#include <stdbool.h>
#include <stdint.h>

bool vermeer_smu_power_qualify(uint32_t *smu_version);
bool vermeer_smu_power_enable(uint32_t features, unsigned int tctl_limit);
bool vermeer_smu_power_ready(void);

#endif /* AMD_BLOCK_VERMEER_SMU_POWER_H */
