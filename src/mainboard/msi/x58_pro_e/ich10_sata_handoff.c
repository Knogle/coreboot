/* SPDX-License-Identifier: GPL-2.0-only */

#include <console/console.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ops.h>
#include <option.h>
#include <southbridge/intel/i82801jx/chip.h>
#include <southbridge/intel/i82801jx/i82801jx.h>
#include <southbridge/intel/i82801jx/sata_init.h>

#include "ich10_sata_handoff.h"

/* Observed X58 dual-IDE reset tuple and its measured AHCI transition.
 * The generic helpers implement ICH10 datasheet 14.1.16/30 and 10.1.77;
 * admission below prevents applying that transition to a running controller.
 * A failed/partial write requires the established full cold recovery, never
 * a retry. This file supplies no normal SATA initialization or resource ops.
 */
#define X58_SATA1_IDE_ID		0x3a208086u
#define X58_SATA2_IDE_ID		0x3a268086u
#define X58_SATA1_AHCI_ID		0x3a228086u
#define X58_SATA1_IDE_CLASSREV	0x01018a00u
#define X58_SATA2_IDE_CLASSREV	0x01018500u
#define X58_SATA1_AHCI_CLASSREV	0x01060100u

static bool sata_tuple(struct device *dev, u32 id, u32 classrev, u16 map, u32 bar5)
{
	return dev && pci_read_config32(dev, PCI_VENDOR_ID) == id &&
		pci_read_config32(dev, PCI_CLASS_REVISION) == classrev &&
		pci_read_config16(dev, PCI_COMMAND) == 0 &&
		pci_read_config8(dev, PCI_HEADER_TYPE) == PCI_HEADER_TYPE_NORMAL &&
		pci_read_config32(dev, I82801JX_SATA_ABAR) == bar5 &&
		pci_read_config16(dev, I82801JX_SATA_MAP) == map;
}

static bool sata1_idle(struct device *dev)
{
	return pci_read_config16(dev, I82801JX_SATA_PCS) == 0 &&
		pci_read_config32(dev, I82801JX_SATA_SCLKCG) == 0;
}

void mainboard_ich10_prepare_sata(
	const struct southbridge_intel_i82801jx_config *config)
{
	struct device *const sata1 = pcidev_on_root(0x1f, 2);
	struct device *const sata2 = pcidev_on_root(0x1f, 5);
	static bool attempted;

	if (attempted)
		die("ICH10 SATA mode setup cannot be retried; cold recovery required.\n");
	attempted = true;

	/* This compatibility graph deliberately supports only all-six-port AHCI.
	 * A CMOS IDE request must not silently change the function/resource graph.
	 */
	if (!config || config->sata_port_map != 0x3f ||
	    get_uint_option("sata_mode", 0) != 0 ||
	    !sata1 || !sata1->enabled || !sata2 || sata2->enabled ||
	    !sata_tuple(sata1, X58_SATA1_IDE_ID, X58_SATA1_IDE_CLASSREV, 0, 1) ||
	    !sata_tuple(sata2, X58_SATA2_IDE_ID, X58_SATA2_IDE_CLASSREV, 0, 1) ||
	    !sata1_idle(sata1) || RCBA32(RCBA_FD) != I82801JX_FD_REQUIRED_BIT_0)
		die("ICH10 SATA reset/config admission failed before MAP write.\n");

	i82801jx_sata_select_ahci(PCI_DEV(0, 0x1f, 2));
	if (!sata_tuple(sata1, X58_SATA1_AHCI_ID, X58_SATA1_AHCI_CLASSREV,
		I82801JX_SATA_MAP_AHCI_D31F2_VALUE, 0) || !sata1_idle(sata1) ||
	    !sata_tuple(sata2, X58_SATA2_IDE_ID, X58_SATA2_IDE_CLASSREV, 0, 1) ||
	    RCBA32(RCBA_FD) != I82801JX_FD_REQUIRED_BIT_0)
		die("ICH10 SATA MAP/BAR5 readback failed; cold recovery required.\n");

	/* Only the explicitly disabled, admitted F5 owns this FD change. */
	i82801jx_disable_sata2();
	if (RCBA32(RCBA_FD) != (I82801JX_FD_REQUIRED_BIT_0 | FD_SAD2) ||
	    pci_read_config32(sata2, PCI_VENDOR_ID) != 0xffffffffu ||
	    !sata_tuple(sata1, X58_SATA1_AHCI_ID, X58_SATA1_AHCI_CLASSREV,
		I82801JX_SATA_MAP_AHCI_D31F2_VALUE, 0) || !sata1_idle(sata1))
		die("ICH10 SATA F5 hide readback failed; cold recovery required.\n");

	printk(BIOS_NOTICE, "[ICH10] SATA AHCI MAP/BAR5/F5 admission PASS; "
	       "normal PCI driver owns subsequent resources/init\n");
}
