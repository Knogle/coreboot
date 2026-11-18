/* SPDX-License-Identifier: GPL-2.0-only */

#include <console/console.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ops.h>
#include <lib.h>

void pci_scan_pre_bridge_hook(struct bus *bus)
{
	struct device *dev;

	/* Vendor firmware left sibling bus routes live. The normal scan assigns
	 * fresh bus numbers; close ALL old Type1 routes on this bus first, even
	 * on statically disabled ports, before walking any one child. Otherwise
	 * a newly assigned secondary can still match an old sibling window.
	 * This is the genuine PCI_ROUTE_CLOSE value, without reset/BAR/COMMAND
	 * changes. Repeat at every bus depth to cover nested chipset switches.
	 */
	for (dev = bus->children; dev; dev = dev->sibling) {
		u32 id, before, after, expected;

		if (dev->path.type != DEVICE_PATH_PCI || !dev->vendor ||
		    dev->vendor == 0xffff || (dev->hdr_type & 0x7f) != PCI_HEADER_TYPE_BRIDGE)
			continue;
		id = pci_read_config32(dev, PCI_VENDOR_ID);
		if (id != (dev->vendor | (u32)dev->device << 16) ||
		    (pci_read_config8(dev, PCI_HEADER_TYPE) & 0x7f) != PCI_HEADER_TYPE_BRIDGE)
			die("NATIVE90 bridge identity changed before route close: %s\n",
			    dev_path(dev));
		before = pci_read_config32(dev, PCI_PRIMARY_BUS);
		expected = (before & 0xff000000U) | 0x00feff00U;
		pci_write_config8(dev, PCI_PRIMARY_BUS, 0);
		pci_write_config8(dev, PCI_SECONDARY_BUS, 0xff);
		pci_write_config8(dev, PCI_SUBORDINATE_BUS, 0xfe);
		after = pci_read_config32(dev, PCI_PRIMARY_BUS);
		printk(BIOS_INFO, "NATIVE90 ROUTE CLOSE %s id=%08x enabled=%u "
		       "old=%08x new=%08x expected=%08x\n", dev_path(dev), id,
		       dev->enabled, before, after, expected);
		if (after != expected)
			die("NATIVE90 bridge route close readback failed: %s\n", dev_path(dev));
	}
}
