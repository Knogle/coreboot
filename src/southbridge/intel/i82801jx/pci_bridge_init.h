/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef SOUTHBRIDGE_INTEL_I82801JX_PCI_BRIDGE_INIT_H
#define SOUTHBRIDGE_INTEL_I82801JX_PCI_BRIDGE_INIT_H

struct device;

/* Program the normal ICH10 conventional PCI bridge device state. The caller
 * owns identity, resource and scan-policy admission. */
void i82801jx_pci_bridge_init(struct device *dev);

#endif /* SOUTHBRIDGE_INTEL_I82801JX_PCI_BRIDGE_INIT_H */
