/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AMD_BLOCK_VERMEER_SMU_POWERPLAY_H
#define AMD_BLOCK_VERMEER_SMU_POWERPLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool vermeer_smu_powerplay_import(const void *table, size_t table_bytes,
				  uintptr_t *owner_address);

#endif
