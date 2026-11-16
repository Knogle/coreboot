/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NATIVE87_DEVICES_H
#define NATIVE87_DEVICES_H

/* Retained ramstage only. Before generic enumeration, check the existing ECAM
 * mapping without programming it, and replace exactly the three selected PnP
 * nodes with resource-only operations. Any unexpected shape is terminal.
 */
void native87_check_ecam(void);
void native87_retain_pnp(void);

/* Entry2 only, after PCI enumeration and before generic resource sizing. */
void native138_prepare_pci_resources(void);

/* Exact ops identity for the later entry2 ACP-class-only preflight. */
struct device_operations;
extern struct device_operations native138_resource_only_ops;

#endif
