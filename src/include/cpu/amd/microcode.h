/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef CPU_AMD_MICROCODE_H
#define CPU_AMD_MICROCODE_H

#include <stdint.h>

void amd_update_microcode_from_cbfs(void);
void amd_load_microcode_from_cbfs(void);
void amd_free_microcode(void);
void amd_apply_microcode_patch(void);
void *amd_microcode_find(void);
/* Revision of the already loaded patch; never starts a CBFS load on an AP. */
uint32_t amd_microcode_patch_id(void);

#endif /* CPU_AMD_MICROCODE_H */
