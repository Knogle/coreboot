/* SPDX-License-Identifier: GPL-2.0-only */

#include "b565d4_memory.h"
#include <arch/cpuid.h>
#include <cpu/amd/mtrr.h>
#include <cpu/x86/cr.h>

bool b565d4_direct_memory_valid(uint64_t syscfg)
{
	if (!(syscfg & SYSCFG_MSR_SMEE))
		return true;
	if (!CONFIG(B565D4_MATISSE))
		return false;

	/* The Matisse reference enables SME in SYSCFG. This alone does not
	 * encrypt memory: APM vol2 7.10.3 requires paging and CR4.PAE. Preserve
	 * that supplier bit while using unpaged IA32 and below-4GiB addresses
	 * with a clear C-bit. Never clear SME or convert encrypted mappings.
	 */
	if ((read_cr0() & CR0_PG) || (read_cr4() & CR4_PAE) ||
	    cpuid_eax(0x80000000) < 0x8000001f)
		return false;
	const struct cpuid_result encryption = cpuid(0x8000001f);
	const unsigned int physical_bits = cpuid_eax(0x80000008) & 0xff;
	const unsigned int cbit = encryption.ebx & 0x3f;
	const unsigned int reduction = (encryption.ebx >> 6) & 0x3f;

	return (encryption.eax & 1) && physical_bits <= 52 &&
		cbit >= 32 && cbit < physical_bits &&
		reduction <= physical_bits && physical_bits - reduction >= 32;
}
