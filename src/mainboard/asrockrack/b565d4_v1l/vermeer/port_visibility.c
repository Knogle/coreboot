/* SPDX-License-Identifier: GPL-2.0-only */

#include "platform.h"
#include "nbif_map.h"
#include "nbio_memory.h"
#include <amdblocks/smn.h>
#include <console/console.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ops.h>

#define PORT_VISIBILITY_BASE (0x13b31004U + MATISSE_NBIO_SMN_OFFSET)
#define PORT_VISIBILITY_MASK 0x00040005U

void b565d4_matisse_hide_unused_ports(void)
{
	unsigned int hidden = 0, kept = 0;

	/* Original Matisse 21BE: visibility is indexed by the canonical DevFn
	 * pool, independently of the physical port remap. Its single-root
	 * constructor selects NBIO instance 3, rather than Cezanne's instance 0.
	 */
	for (struct device *dev = all_devices; dev; dev = dev->next) {
		uint32_t before, wanted, after, id, link_cap;
		uint16_t cap, status;
		int index;

		if (dev->path.type != DEVICE_PATH_PCI || !dev->upstream ||
		    dev->upstream->secondary || dev->upstream->segment_group)
			continue;
		index = matisse_nbif_devfn_index(dev->path.pci.devfn);
		if (index < 0)
			continue;
		id = pci_read_config32(dev, PCI_VENDOR_ID);
		if (id != 0x14831022)
			continue;
		cap = pci_find_capability(dev, PCI_CAP_ID_PCIE);
		if (!cap)
			continue;
		link_cap = pci_read_config32(dev, cap + PCI_EXP_LNKCAP);
		status = pci_read_config16(dev, cap + PCI_EXP_LNKSTA);
		/* Untrained Matisse ports retain a bogus negotiated width in 1101.
		 * Require DLL active reporting instead of interpreting that width.
		 * Preserve discovered children, training links and hotplug ports.
		 * Unknown capabilities or status must not hide a device.
		 */
		if (dev->hotplug_port || (dev->downstream && dev->downstream->children) ||
		    !(link_cap & PCI_EXP_LNKCAP_DLLLARC) || link_cap == UINT32_MAX ||
		    (status & (PCI_EXP_LNKSTA_DLLLA | PCI_EXP_LNKSTA_LT))) {
			kept++;
			continue;
		}
		const uint32_t address = PORT_VISIBILITY_BASE + (uint32_t)index * 0x400U;

		before = smn_read32(address);
		if (before == UINT32_MAX) {
			printk(BIOS_ERR, B565D4_DESKTOP_NAME " port %s visibility unavailable\n",
			       dev_path(dev));
			continue;
		}
		wanted = before | PORT_VISIBILITY_MASK;
		if (wanted != before)
			smn_write32(address, wanted);
		after = smn_read32(address);
		id = pci_read_config32(dev, PCI_VENDOR_ID);
		printk(BIOS_DEBUG, B565D4_DESKTOP_NAME " unused port %s: SMN%08x %08x -> %08x "
		       "link=%04x id=%08x\n", dev_path(dev), address, before, after, status, id);
		if (after != wanted || id != UINT32_MAX) {
			printk(BIOS_ERR, B565D4_DESKTOP_NAME " port visibility readback failed\n");
			return;
		}
		dev->hidden = true;
		dev->enabled = false;
		hidden++;
	}
	printk(BIOS_DEBUG, B565D4_DESKTOP_NAME " root visibility: hidden=%u live_or_hotplug=%u\n",
	       hidden, kept);
}
