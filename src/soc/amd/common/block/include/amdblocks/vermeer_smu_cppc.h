/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AMD_BLOCK_VERMEER_SMU_CPPC_H
#define AMD_BLOCK_VERMEER_SMU_CPPC_H

#include <stdbool.h>
#include <stdint.h>

struct vermeer_smu_cppc_info {
	uint32_t information;
	unsigned int nominal_mhz;
	unsigned int lowest_mhz;
};

bool vermeer_smu_cppc_prepare(uint32_t smu_version,
			      struct vermeer_smu_cppc_info *info);

#endif
