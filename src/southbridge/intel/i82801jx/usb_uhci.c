/* SPDX-License-Identifier: GPL-2.0-only */

#include <console/console.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ops.h>
#include <device/pci_ids.h>
#include "usb_init.h"

/*
 * The pinned ICH10 driver lacked a UHCI-specific callback. Adopt the standard
 * BME-only sequence used by i82801hx/usb.c, not the ICH7-only CA erratum writes.
 * ICH10 319973-003, USB UHCI PCI Command: bus mastering enables host DMA.
 * The caller/payload owns resources, idle admission, reset and enumeration.
 */
void i82801jx_usb_uhci_init(struct device *dev)
{
	printk(BIOS_DEBUG, "UHCI: Setting up controller.. ");
	pci_or_config16(dev, PCI_COMMAND, PCI_COMMAND_MASTER);
	printk(BIOS_DEBUG, "done.\n");
}

void i82801jx_usb_uhci_set_subsystem(struct device *dev, unsigned int vendor,
	unsigned int device)
{
	/* 319973-003 16.1.12/13: once per core-well reset, single 16-bit cycles.
	 * Preserve the generic setter's fallback policy, but not its write width.
	 */
	if (!vendor || !device) {
		const u32 id = pci_read_config32(dev, PCI_VENDOR_ID);
		vendor = id & 0xffff;
		device = id >> 16;
	}
	pci_write_config16(dev, PCI_SUBSYSTEM_VENDOR_ID, vendor);
	pci_write_config16(dev, PCI_SUBSYSTEM_ID, device);
}

#if CONFIG(SOUTHBRIDGE_INTEL_I82801JX) && !CONFIG(SOUTHBRIDGE_INTEL_I82801JX_DIRECT_DEVICE_MODEL)
static struct pci_operations usb_uhci_pci_ops = {
	.set_subsystem = i82801jx_usb_uhci_set_subsystem,
};

static struct device_operations usb_uhci_ops = {
	.read_resources = pci_dev_read_resources,
	.set_resources = pci_dev_set_resources,
	.enable_resources = pci_dev_enable_resources,
	.scan_bus = scan_static_bus,
	.init = i82801jx_usb_uhci_init,
	.ops_pci = &usb_uhci_pci_ops,
};

static const unsigned short pci_uhci_device_ids[] = {
	0x3a34, 0x3a35, 0x3a36, 0x3a37, 0x3a38, 0x3a39, 0,
};

static const struct pci_driver ich10_usb_uhci __pci_driver = {
	.ops = &usb_uhci_ops,
	.vendor = PCI_VID_INTEL,
	.devices = pci_uhci_device_ids,
};
#endif
