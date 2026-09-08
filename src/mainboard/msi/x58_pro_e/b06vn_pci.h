/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_B06VN_PCI_H
#define MAINBOARD_MSI_X58_PRO_E_B06VN_PCI_H

#include <device/device.h>
#include <stdbool.h>

extern struct device_operations b06vn_root_port_ops;
extern struct device_operations b06vn_endpoint_ops;

void x58_b06vn_enable_dev(struct device *dev);

#if CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM
bool b06wc_acpi_mode_ready(void);
#endif

#endif /* MAINBOARD_MSI_X58_PRO_E_B06VN_PCI_H */
