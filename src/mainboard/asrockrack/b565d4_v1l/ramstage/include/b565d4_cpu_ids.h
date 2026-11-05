/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef B565D4_CPU_IDS_H
#define B565D4_CPU_IDS_H

/* Keep these usable by the pre-MSR assembly guard as well as C code. */
#define B565D4_CPUID_MODEL_MASK		0xfffffff0
#define B565D4_CPUID_SUMMIT_RIDGE		0x00800f10
#define B565D4_CPUID_PINNACLE_RIDGE	0x00800f80
#define B565D4_CPUID_MATISSE_A0		0x00870f00
#define B565D4_CPUID_MATISSE_B0		0x00870f10
#define B565D4_CPUID_VERMEER_A0		0x00a20f00
#define B565D4_CPUID_VERMEER_B0		0x00a20f10
#define B565D4_CPUID_CEZANNE		0x00a50f00

#ifndef __ASSEMBLER__
#include <stdint.h>

enum b565d4_cpu_family {
	B565D4_CPU_UNKNOWN,
	B565D4_CPU_SUMMIT_RIDGE,
	B565D4_CPU_PINNACLE_RIDGE,
	B565D4_CPU_MATISSE,
	B565D4_CPU_VERMEER,
	B565D4_CPU_CEZANNE,
};

/* Identification only, after checking AuthenticAMD. In particular this does
 * not select Cezanne register programming for another AM4 processor. CPUID
 * also cannot distinguish a Cezanne 5500 from every graphics-enabled SKU.
 */
static inline enum b565d4_cpu_family b565d4_cpu_family(uint32_t signature)
{
	switch (signature & B565D4_CPUID_MODEL_MASK) {
	case B565D4_CPUID_SUMMIT_RIDGE:
		return B565D4_CPU_SUMMIT_RIDGE;
	case B565D4_CPUID_PINNACLE_RIDGE:
		return B565D4_CPU_PINNACLE_RIDGE;
	case B565D4_CPUID_MATISSE_A0:
	case B565D4_CPUID_MATISSE_B0:
		return B565D4_CPU_MATISSE;
	case B565D4_CPUID_VERMEER_A0:
	case B565D4_CPUID_VERMEER_B0:
		return B565D4_CPU_VERMEER;
	case B565D4_CPUID_CEZANNE:
		return B565D4_CPU_CEZANNE;
	default:
		return B565D4_CPU_UNKNOWN;
	}
}
#endif /* !__ASSEMBLER__ */

#endif /* B565D4_CPU_IDS_H */
