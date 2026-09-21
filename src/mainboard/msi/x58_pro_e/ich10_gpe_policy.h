/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef MSI_X58_PRO_E_ICH10_GPE_POLICY_H
#define MSI_X58_PRO_E_ICH10_GPE_POLICY_H

#include <stdbool.h>
#include <stdint.h>

/* Intel 319973-003, section 13.8.3.10, p.473: GPE0_EN is at PMBASE+28h;
 * bit10 is reserved on ICH10. The older BATLOW_EN name in the RTC summary
 * table is not an implemented-event definition for this chipset. Never
 * write the reserved bit merely to make a raw-value admission test pass.
 * Ignore only this observed reserved bit; all other bits remain strict. */
#define X58_ICH10_GPE0_RESERVED_BIT10 0x00000400u

/* Same register table: bits11/13 are writable PME/PME_B0 enables in the RTC
 * well. They need not clear on AC loss or CF9 reset. The optional cold-init
 * policy disables only these documented sources; it never relaxes quiet(). */
#define X58_ICH10_GPE0_PME_EN	0x00000800u
#define X58_ICH10_GPE0_PME_B0_EN	0x00002000u
#define X58_ICH10_GPE0_PME_MASK	(X58_ICH10_GPE0_PME_EN | X58_ICH10_GPE0_PME_B0_EN)

static inline bool x58_ich10_gpe0_low_quiet(uint32_t raw)
{
	return (raw & ~X58_ICH10_GPE0_RESERVED_BIT10) == 0;
}

static inline bool x58_ich10_gpe0_pme_prestate(uint32_t raw)
{
	return (raw & ~(X58_ICH10_GPE0_RESERVED_BIT10 | X58_ICH10_GPE0_PME_MASK)) == 0;
}

static inline uint32_t x58_ich10_gpe0_mask_pme(uint32_t raw)
{
	return raw & ~X58_ICH10_GPE0_PME_MASK;
}

#endif
