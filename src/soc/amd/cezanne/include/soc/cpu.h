/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_CEZANNE_CPU_H
#define AMD_CEZANNE_CPU_H

#include <types.h>

#define CEZANNE_A0_CPUID	CPUID_FROM_FMS(0x19, 0x50, 0)
#define RENOIR_A1_CPUID		CPUID_FROM_FMS(0X17, 0X60, 1)

#define CEZANNE_VBIOS_VID_DID	0x10021638
#define BARCELO_VBIOS_VID_DID	0x100215e7

/* Required strong board provider when the native identity capability is selected. */
bool mainboard_amd_native_cpu_identity_valid(uint32_t signature);

#endif /* AMD_CEZANNE_CPU_H */
