/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef B565D4_CPU_H
#define B565D4_CPU_H

#include <arch/cpu.h>
#include <soc/cpu.h>
#include <b565d4_cpu_ids.h>

_Static_assert(B565D4_CPUID_CEZANNE == CEZANNE_A0_CPUID &&
	       B565D4_CPUID_MODEL_MASK == CPUID_ALL_STEPPINGS_MASK,
	       "Board and SoC CPU matching must agree");

/* Match the SoC CPU driver, rather than one bring-up processor stepping.
 * Other AM4 SoCs need their own register and pre-x86 firmware integration.
 */
static inline bool b565d4_is_cezanne(uint32_t signature)
{
	return cpuid_match(signature, B565D4_CPUID_CEZANNE, B565D4_CPUID_MODEL_MASK);
}

/* Only architectural/board-common callers use this predicate. Cezanne
 * device initialization must retain its narrower check.
 */
static inline bool b565d4_is_selected_cpu(uint32_t signature)
{
	if (CONFIG(B565D4_MATISSE))
		return b565d4_cpu_family(signature) == B565D4_CPU_MATISSE;
	if (CONFIG(B565D4_VERMEER))
		return b565d4_cpu_family(signature) == B565D4_CPU_VERMEER;
	return b565d4_is_cezanne(signature);
}

/* The retained Matisse cohort and both real desktop references establish
 * these two container headers. Record and memory extents are checked by the
 * individual consumers before any payload access or resource publication.
 */
static inline bool b565d4_apob_header_valid(uint32_t version, uint32_t first,
					 uint32_t bytes, uint32_t capacity)
{
	if (bytes < 16 || bytes > capacity || ((bytes | first) & 3) || first > bytes)
		return false;
	if (CONFIG(B565D4_MATISSE))
		return version == 23 && first == 0x108;
	return version == 24 && first == 0xf8;
}

#endif /* B565D4_CPU_H */
