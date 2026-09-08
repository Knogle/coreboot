/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_B06VM_PCI_H
#define MAINBOARD_MSI_X58_PRO_E_B06VM_PCI_H

#include <device/device.h>

extern struct device_operations b06vm_root_port_ops;
extern struct device_operations b06vm_endpoint_ops;

void x58_b06vm_enable_dev(struct device *dev);

#endif /* MAINBOARD_MSI_X58_PRO_E_B06VM_PCI_H */
