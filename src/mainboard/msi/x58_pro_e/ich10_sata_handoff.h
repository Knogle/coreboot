/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_ICH10_SATA_HANDOFF_H
#define MAINBOARD_MSI_X58_PRO_E_ICH10_SATA_HANDOFF_H

struct southbridge_intel_i82801jx_config;

/*
 * Board-owned pre-enumeration SATA admission for the inherited ICH10 state.
 * It is called exactly once before PCI resource discovery. The routine is not
 * a replacement for the standard post-allocation SATA device initializer.
 */
void mainboard_ich10_prepare_sata(
	const struct southbridge_intel_i82801jx_config *config);

#endif /* MAINBOARD_MSI_X58_PRO_E_ICH10_SATA_HANDOFF_H */
