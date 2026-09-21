/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SOUTHBRIDGE_INTEL_I82801JX_BOARD_POLICY_H
#define SOUTHBRIDGE_INTEL_I82801JX_BOARD_POLICY_H

#include <stdbool.h>

struct device;
struct southbridge_intel_i82801jx_config;

/*
 * Board-owned policy hooks used by the registered ICH10 device model.
 *
 * These are deliberately not weak fallbacks: a selected board policy must
 * fail the build when incomplete, rather than silently start a timer, send
 * APMC without an SMI handler, or admit an unbounded PCI topology.
 */
void mainboard_ich10_pre_init(void);
void mainboard_ich10_lpc_acpi_mode(struct device *dev);
void mainboard_ich10_prepare_sata(
	const struct southbridge_intel_i82801jx_config *config);

/*
 * A board may retain bounded bridge admission under MINIMAL_PCI_SCANNING.
 * Chipset PCI drivers still own resource assignment and initialization.
 *
 * This is deliberately named for a bridge rather than a root port: the
 * board policy applies to both ICH10 PCIe ports and its conventional PCI
 * bridge.
 */
void mainboard_ich10_pci_scan_bridge(struct device *dev);
bool mainboard_ich10_pci_bridge_isolated(const struct device *dev);

#endif
