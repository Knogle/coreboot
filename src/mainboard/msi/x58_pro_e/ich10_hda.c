/* SPDX-License-Identifier: GPL-2.0-only */

#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ops.h>
#include <southbridge/intel/i82801jx/azalia_init.h>

#include "hda_recovery.h"
#include "ich10_hda.h"

void x58_ich10_hda_init(struct device *dev)
{
	x58_ich1b_hda_init(dev);
}

struct device_operations x58_ich10_hda_ops = {
	.read_resources		= pci_dev_read_resources,
	.set_resources		= pci_dev_set_resources,
	.enable_resources	= pci_dev_enable_resources,
	.init			= x58_ich10_hda_init,
	.ops_pci		= &pci_dev_ops_pci,
};
