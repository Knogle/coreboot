/* SPDX-License-Identifier: GPL-2.0-only */

#include <bootstate.h>
#include <console/console.h>
#include <delay.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ops.h>

#define ASM1142_ID 0x12421b21
#define ASM_XDATA_INDEX 0xe8
#define ASM_XDATA_WRITE 0xea
#define ASM_XDATA_READ 0xeb
#define ASM_HCSPARAMS2 0xf608
#define ASM_INITIAL_HCSPARAMS2 0x000000fa
#define ASM_RESTORE_HCSPARAMS2 0x840000fa

static uint32_t read_hcs2(const struct device *dev)
{
	uint32_t value = 0;

	for (unsigned int byte = 0; byte < sizeof(value); byte++) {
		pci_write_config16(dev, ASM_XDATA_INDEX, ASM_HCSPARAMS2 + byte);
		udelay(100);
		value |= (uint32_t)pci_read_config8(dev, ASM_XDATA_READ) << (byte * 8);
	}
	return value;
}

static void prepare_scratchpads(struct device *dev)
{
	uint16_t selector;
	uint32_t before, after;

	if (pci_read_config32(dev, PCI_VENDOR_ID) != ASM1142_ID ||
	    pci_read_config8(dev, PCI_REVISION_ID) != 0 ||
	    (pci_read_config32(dev, PCI_CLASS_REVISION) >> 8) != 0x0c0330 ||
	    pci_read_config32(dev, PCI_SUBSYSTEM_VENDOR_ID) != ASM1142_ID)
		return;

	selector = pci_read_config16(dev, ASM_XDATA_INDEX);
	before = read_hcs2(dev);
	after = before;
	/* This controller's cold descriptor omits the scratchpad buffers that its
	 * save/restore engine needs. Publish its observed restore descriptor before
	 * the payload or OS caches it and allocates the DCBAA. This changes only a
	 * volatile capability byte, never the controller firmware or EEPROM.
	 * Leave other descriptors untouched; an unknown card must not block boot.
	 */
	if (before == ASM_INITIAL_HCSPARAMS2) {
		pci_write_config16(dev, ASM_XDATA_INDEX, ASM_HCSPARAMS2 + 3);
		udelay(100);
		pci_write_config8(dev, ASM_XDATA_WRITE, ASM_RESTORE_HCSPARAMS2 >> 24);
		udelay(1000);
		after = read_hcs2(dev);
	}
	pci_write_config16(dev, ASM_XDATA_INDEX, selector);
	printk(after == ASM_RESTORE_HCSPARAMS2 ? BIOS_NOTICE : BIOS_WARNING,
	       "Board ASM1142 %s: HCSPARAMS2 %08x -> %08x, selector restored=%u\n",
	       dev_path(dev), before, after,
	       pci_read_config16(dev, ASM_XDATA_INDEX) == selector);
}

static void asmedia_scratchpads(void *unused)
{
	for (struct device *dev = all_devices; dev; dev = dev->next) {
		if (dev->enabled && dev->path.type == DEVICE_PATH_PCI &&
		    dev->vendor == 0x1b21 && dev->device == 0x1242)
			prepare_scratchpads(dev);
	}
}

BOOT_STATE_INIT_ENTRY(BS_DEV_INIT, BS_ON_EXIT, asmedia_scratchpads, NULL);
