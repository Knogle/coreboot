/* SPDX-License-Identifier: GPL-2.0-only */
#include <amdblocks/promontory.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <delay.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ids.h>
#include <device/pci_ops.h>
#include <device/pciexp.h>

static struct device *promontory_upstream(struct device *root)
{
	if (!root || !root->enabled || !root->downstream)
		return NULL;

	for (struct device *dev = root->downstream->children; dev; dev = dev->sibling) {
		if (dev->enabled && dev->path.type == DEVICE_PATH_PCI &&
		    dev->vendor == PCI_VID_AMD && dev->device == 0x43e9)
			return dev;
	}
	return NULL;
}

static unsigned int link_capability(const struct device *dev, unsigned int type)
{
	const unsigned int cap = pci_find_capability(dev, PCI_CAP_ID_PCIE);
	uint16_t flags;

	/* Link Control 2 must fit in the conventional capability space. */
	if (cap < 0x40 || cap > 0xcc)
		return 0;
	flags = pci_read_config16(dev, cap + PCI_EXP_FLAGS);
	if (flags == 0xffff || (flags & PCI_EXP_FLAGS_VERS) < 2 ||
	    ((flags & PCI_EXP_FLAGS_TYPE) >> 4) != type)
		return 0;
	return cap;
}

void amd_promontory_init_uplink(struct device *root, unsigned int max_speed)
{
	struct device *upstream = promontory_upstream(root);
	unsigned int root_cap, upstream_cap, speed;
	uint32_t root_link_cap, upstream_link_cap;
	uint16_t before, after, control, target;

	if (!upstream || pci_read_config16(root, PCI_VENDOR_ID) != PCI_VID_AMD ||
	    pci_read_config32(upstream, PCI_VENDOR_ID) != 0x43e91022)
		return;
	root_cap = link_capability(root, PCI_EXP_TYPE_ROOT_PORT);
	upstream_cap = link_capability(upstream, PCI_EXP_TYPE_UPSTREAM);
	if (!root_cap || !upstream_cap)
		return;

	root_link_cap = pci_read_config32(root, root_cap + PCI_EXP_LNKCAP);
	upstream_link_cap = pci_read_config32(upstream, upstream_cap + PCI_EXP_LNKCAP);
	if (root_link_cap == 0xffffffff || upstream_link_cap == 0xffffffff)
		return;
	speed = MIN(root_link_cap & PCI_EXP_LNKCAP_MLS,
		    upstream_link_cap & PCI_EXP_LNKCAP_MLS);
	/* Preserve an explicit platform limit across the ordinary uplink setup. */
	if (max_speed)
		speed = MIN(speed, max_speed);
	before = pci_read_config16(root, root_cap + PCI_EXP_LNKSTA);
	control = pci_read_config16(root, root_cap + PCI_EXP_LNKCTL);
	if (!speed || before == 0xffff || control == 0xffff ||
	    !(before & PCI_EXP_LNKSTA_NLW) || (before & PCI_EXP_LNKSTA_LT) ||
	    (control & PCI_EXP_LNKCTL_LD) ||
	    ((root_link_cap & PCI_EXP_LNKCAP_DLLLARC) &&
	     !(before & PCI_EXP_LNKSTA_DLLLA))) {
		printk(BIOS_WARNING, "Promontory PCIe uplink is not ready for speed setup\n");
		return;
	}

	/* Initial training leaves a Gen1 target on this link. Raise only the target
	 * speed after the chipset firmware is ready; preserve every other bit.
	 */
	target = pci_read_config16(root, root_cap + PCI_EXP_LNKCTL2);
	if (target == 0xffff)
		return;
	if ((target & PCI_EXP_LNKCTL2_TLS) != speed) {
		target = (target & ~PCI_EXP_LNKCTL2_TLS) | speed;
		pci_write_config16(root, root_cap + PCI_EXP_LNKCTL2, target);
		if (pci_read_config16(root, root_cap + PCI_EXP_LNKCTL2) != target) {
			printk(BIOS_WARNING, "Promontory PCIe target speed readback failed\n");
			return;
		}
	}
	if ((before & PCI_EXP_LNKSTA_CLS) != speed) {
		if (pciexp_retrain_link(root, root_cap)) {
			printk(BIOS_WARNING, "Promontory PCIe uplink retraining timed out\n");
			return;
		}
		mdelay(100);
	}
	after = pci_read_config16(root, root_cap + PCI_EXP_LNKSTA);
	if (after == 0xffff) {
		printk(BIOS_WARNING, "Promontory PCIe uplink status is unavailable\n");
		return;
	}
	printk(BIOS_NOTICE, "Promontory PCIe uplink: Gen%u x%u -> Gen%u x%u, target %u\n",
	       before & PCI_EXP_LNKSTA_CLS, (before & PCI_EXP_LNKSTA_NLW) >> 4,
	       after & PCI_EXP_LNKSTA_CLS, (after & PCI_EXP_LNKSTA_NLW) >> 4, speed);
	if ((after & PCI_EXP_LNKSTA_LT) ||
	    ((root_link_cap & PCI_EXP_LNKCAP_DLLLARC) &&
	     !(after & PCI_EXP_LNKSTA_DLLLA)) ||
	    (after & PCI_EXP_LNKSTA_CLS) != speed ||
	    (after & PCI_EXP_LNKSTA_NLW) != (before & PCI_EXP_LNKSTA_NLW))
		printk(BIOS_WARNING, "Promontory PCIe uplink did not retain the requested link\n");
}
