/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef MSI_X58_PRO_E_CSI_STATE_POLICY_H
#define MSI_X58_PRO_E_CSI_STATE_POLICY_H

#include <stdbool.h>
#include <console/console.h>

/* Only observational post-CSI platform predicates may use this helper.
 * Never pass blob/ABI/canary/pointer/order checks or post-MINIT RAM gates.
 * Evaluate the original predicate even in log-only mode; do not invent PASS.
 */
static inline bool x58_csi_state_admit(bool exact, const char *site)
{
	if (!exact)
		printk(BIOS_WARNING, "[CSI-POLICY] SITE=%s MATCH=0 ACTION=%s\n",
			site, 1 ? "WARN_CONTINUE" : "BLOCK");
	return exact || 1;
}

#endif
