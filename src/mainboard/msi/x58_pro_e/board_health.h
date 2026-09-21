/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MSI_X58_PRO_E_BOARD_HEALTH_H
#define MSI_X58_PRO_E_BOARD_HEALTH_H

/* BSP ramstage, before/after standard LPC initialization. Log-only findings.
 * No RTC data accesses or fan/GPIO configuration writes. Configuration index
 * selection is a real effect; CF8 and Fintek LDN are restored before return.
 */
void x58_board_health_report(const char *phase);

#endif
