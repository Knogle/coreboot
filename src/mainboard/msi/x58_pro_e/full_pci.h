/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef X58_FULL_PCI_H
#define X58_FULL_PCI_H

#include <device/device.h>
#include <stdbool.h>

extern struct device_operations x58_full_ioh_ops;
extern struct device_operations x58_full_endpoint_ops;
void x58_full_pci_enable_dev(struct device *dev);
void x58_full_pci_preflight(void);
void x58_full_pci_scan_bridge(struct device *dev);
bool x58_full_pci_root_isolated(const struct device *dev);
bool x58_full_ich10_pci_admitted(const struct device *dev);

/* The nearest vendor _PRT is on the fixed root bridge. Only intervening
 * add-in bridges swizzle; the direct child's slot is the _PRT address. */
struct x58_full_intx_route {
	unsigned int root_devfn, slot, pin, gsi;
};
bool x58_full_pci_intx_route(const struct device *dev, unsigned int pin,
	struct x58_full_intx_route *route);

/* Existing board memory/IOH contracts, not another ICH10 owner. */
void x58_full_platform_verify(const char *phase);
void x58_full_ioh_prepare(void);

#endif
