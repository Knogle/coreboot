/* SPDX-License-Identifier: GPL-2.0-only */

#include "native_smm_memory.h"
#include <cpu/amd/msr.h>
#include <cpu/x86/msr.h>

bool b565d4_decode_smm_regions(uint64_t smbase, uint64_t smm_addr,
				uint64_t smm_mask, uint64_t hwcr,
				unsigned int total_physical_bits,
				struct b565d4_smm_regions *regions)
{
	struct b565d4_smm_regions result = { 0 };
	uint64_t physical_mask, address_mask, bytes;

	if (!regions || total_physical_bits < 17 || total_physical_bits > 52 ||
	    smbase > 0xffff0000ULL)
		return false;
	physical_mask = (1ULL << total_physical_bits) - 1;
	if (smbase + 0x10000 > physical_mask + 1)
		return false;

	result.smbase_begin = smbase;
	result.smbase_end = smbase + 0x10000;
	result.flags = (uint32_t)smm_mask & (B565D4_SMM_ASEG_VALID | B565D4_SMM_TSEG_VALID);
	if (hwcr & 1)
		result.flags |= B565D4_SMM_LOCKED;

	if (result.flags & B565D4_SMM_TSEG_VALID) {
		address_mask = smm_mask & physical_mask & ~0x1ffffULL;
		bytes = (physical_mask ^ address_mask) + 1;
		if ((bytes & (bytes - 1)) || (smm_addr & ~physical_mask))
			return false;
		/* The hardware compares masked addresses; ignored base bits do not
		 * alter the protected interval. Low mask bits are SMM attributes.
		 */
		result.tseg_begin = smm_addr & address_mask;
		result.tseg_end = result.tseg_begin + bytes;
	}

	*regions = result;
	return true;
}

/* No SMM state is changed. Publish only after both reads agree on the SMM
 * registers and SmmLock. Other HWCR bits do not determine SMM ownership.
 */
bool b565d4_read_smm_state(struct b565d4_smm_state *state)
{
	struct b565d4_smm_state first, second;

	if (!state)
		return false;
	first.smbase = rdmsr(SMM_BASE_MSR).raw;
	first.smm_addr = rdmsr(SMM_ADDR_MSR).raw;
	first.smm_mask = rdmsr(SMM_MASK_MSR).raw;
	first.hwcr = rdmsr(HWCR_MSR).raw;
	asm volatile ("" : : : "memory");
	second.smbase = rdmsr(SMM_BASE_MSR).raw;
	second.smm_addr = rdmsr(SMM_ADDR_MSR).raw;
	second.smm_mask = rdmsr(SMM_MASK_MSR).raw;
	second.hwcr = rdmsr(HWCR_MSR).raw;
	if (!b565d4_same_smm_state(&first, &second))
		return false;
	*state = first;
	return true;
}

bool b565d4_same_smm_state(const struct b565d4_smm_state *a,
			 const struct b565d4_smm_state *b)
{
	return a && b && a->smbase == b->smbase && a->smm_addr == b->smm_addr &&
		a->smm_mask == b->smm_mask && !((a->hwcr ^ b->hwcr) & 1);
}
