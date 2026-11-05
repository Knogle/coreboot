/* SPDX-License-Identifier: GPL-2.0-only */

#include "nbio_memory.h"

bool matisse_plan_nbio_memory(const struct matisse_nbio_memory *before,
			      uint64_t top_mem, uint64_t top_mem2, uint64_t syscfg,
			      struct matisse_nbio_memory *after)
{
	struct matisse_nbio_memory result;
	uint32_t low, high;

	if (!before || !after)
		return false;

	result = *before;
	result.root_tom = (before->root_tom & 0x007fffffU) |
		((uint32_t)top_mem & 0xff800000U);
	result.tom3 = 0;

	if (syscfg & (1ULL << 21)) {
		low = (uint32_t)top_mem2 & 0xffc00000U;
		high = (uint32_t)(top_mem2 >> 32) & 0xffffU;
		if (high > 0x100 || (high == 0x100 && low)) {
			uint64_t limit = (top_mem2 - 1) & 0x0000ffffffc00000ULL;

			result.tom2_low = (before->tom2_low & 0x007ffffeU) | 0xff800001U;
			result.tom2_high = (before->tom2_high & 0xfffffe00U) | 0x1ff;
			result.tom3 = (uint32_t)(limit >> 22) | 0x80000000U;
		} else if (low || (high & 0xff)) {
			result.tom2_low = (before->tom2_low & 0x007ffffeU) |
				(low & 0xff800000U) | 1;
			result.tom2_high = (before->tom2_high & 0xfffffe00U) | (high & 0xff);
		}
	}

	result.control |= 1U << 23;
	*after = result;
	return true;
}
