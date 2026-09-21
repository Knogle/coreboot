/* SPDX-License-Identifier: GPL-2.0-only */

#include <console/console.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ops.h>
#include "i82801jx.h"

static void i82801jx_ehci_program_required_fields(struct device *dev)
{
	u32 value = pci_read_config32(dev, I82801JX_EHCI_FCREG);

	/*
	 * Intel ICH10 Datasheet 319973, section 17.1.36 (EHCIIR2):
	 * BIOS must set bits 3:2 to 10b and set bits 17 and 29.
	 */
	value &= ~I82801JX_EHCI_FCREG_REQUIRED_MASK;
	value |= I82801JX_EHCI_FCREG_REQUIRED_VALUE;
	pci_write_config32(dev, I82801JX_EHCI_FCREG, value);
}

void i82801jx_ehci_init(void)
{
	struct device *const ehci1 = pcidev_on_root(0x1d, 7);
	struct device *const ehci2 = pcidev_on_root(0x1a, 7);

	if (!ehci1)
		die("EHCI controller (00:1d.7) not listed in devicetree.\n");
	if (!ehci2)
		die("EHCI controller (00:1a.7) not listed in devicetree.\n");

	/* TODO: Maybe we have to save and restore these settings across S3. */
	i82801jx_ehci_program_required_fields(ehci1);
	i82801jx_ehci_program_required_fields(ehci2);
}
