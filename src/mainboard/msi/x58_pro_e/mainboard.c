/* SPDX-License-Identifier: GPL-2.0-only */

#include <device/device.h>

#include "pci.h"

struct chip_operations mainboard_msi_x58_pro_e_ops = {
	.name = "MSI X58 Pro-E ICH10_TIMER standard ICH10 monotonic timer",
	.enable_dev = x58_pci_enable_dev,
};
