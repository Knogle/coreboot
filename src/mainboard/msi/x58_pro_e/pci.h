/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_X58_PCI_H
#define MAINBOARD_MSI_X58_PRO_E_X58_PCI_H

#include <device/device.h>
#include <stdbool.h>

extern struct device_operations x58_pci_root_port_ops;
extern struct device_operations x58_pci_endpoint_ops;
extern struct device_operations x58_pci_standard_ehci_ops;
extern struct device_operations x58_pci_standard_uhci_ops;
void x58_pci_standard_usb_verify(void);
extern struct device_operations x58_pci_standard_sata_ops;
extern struct device_operations x58_pci_standard_lpc_ops;

void x58_pci_enable_dev(struct device *dev);
void x58_pci_admit_domain(struct device *dev);

bool x58_acpi_mode_ready(void);

#endif /* MAINBOARD_MSI_X58_PRO_E_X58_PCI_H */
