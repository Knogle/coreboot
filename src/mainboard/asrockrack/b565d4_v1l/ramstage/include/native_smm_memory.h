/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef B565D4_NATIVE_SMM_MEMORY_H
#define B565D4_NATIVE_SMM_MEMORY_H

#include <types.h>

struct b565d4_smm_state {
	uint64_t smbase, smm_addr, smm_mask, hwcr;
};

enum b565d4_smm_flags {
	B565D4_SMM_ASEG_VALID = 1U << 0,
	B565D4_SMM_TSEG_VALID = 1U << 1,
	B565D4_SMM_LOCKED = 1U << 2,
};

struct b565d4_smm_regions {
	uint64_t smbase_begin, smbase_end;
	uint64_t tseg_begin, tseg_end;
	uint32_t flags, reserved;
};

/* Pure range calculation from an already qualified MSR snapshot. Include
 * encryption-reserved bits in total_physical_bits, as common AMD SMM does.
 * AValid additionally requires reserving the legacy A0000..BFFFF range.
 * Non-contiguous active masks cannot be represented by one TSEG interval
 * and return false, leaving output unchanged. No SMM content is accessed.
 * The caller still establishes pre-x86 state, backing and memory ownership.
 */
bool b565d4_decode_smm_regions(uint64_t smbase, uint64_t smm_addr,
				uint64_t smm_mask, uint64_t hwcr,
				unsigned int total_physical_bits,
				struct b565d4_smm_regions *regions);

/* Read-only BSP snapshot. SMM registers and HWCR.SmmLock must agree;
 * unrelated HWCR changes do not block memory admission. A mismatch leaves
 * output unchanged.
 */
bool b565d4_read_smm_state(struct b565d4_smm_state *state);
bool b565d4_same_smm_state(const struct b565d4_smm_state *a,
			 const struct b565d4_smm_state *b);

#endif /* B565D4_NATIVE_SMM_MEMORY_H */
