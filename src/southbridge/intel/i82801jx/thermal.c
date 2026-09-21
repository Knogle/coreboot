/* SPDX-License-Identifier: GPL-2.0-only */

#include <device/mmio.h>
#include <device/pci_ops.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ids.h>
#include <console/console.h>

#include "i82801jx.h"
#include "thermal_init.h"

static void thermal_program(u8 *const tbar)
{
	u8 reg8;

	write32(tbar + 0x04, 0); /* Clear thermal trip points. */
	write32(tbar + 0x44, 0);

	write8(tbar + 0x01, 0xba); /* Enable sensor 0 + 1. */
	write8(tbar + 0x41, 0xba);

	reg8 = read8(tbar + 0x08); /* Lock thermal registers. */
	write8(tbar + 0x08, reg8 | (1 << 7));
	reg8 = read8(tbar + 0x48);
	write8(tbar + 0x48, reg8 | (1 << 7));
}

void i82801jx_thermal_init_assigned(struct device *dev)
{
	/* Normal PCI resource ownership must survive initialization: the legacy
	 * temporary fixed mapping would discard the allocator's BAR. All thermal
	 * programming below is unchanged, now at the assigned decoded resource.
	 */
	struct resource *const res = probe_resource(dev, PCI_BASE_ADDRESS_0);
	const u32 bar = pci_read_config32(dev, PCI_BASE_ADDRESS_0);
	if (!res || !res->base || res->size < 0x49 ||
	    (res->flags & (IORESOURCE_MEM | IORESOURCE_ASSIGNED | IORESOURCE_STORED)) !=
		(IORESOURCE_MEM | IORESOURCE_ASSIGNED | IORESOURCE_STORED) ||
	    (bar & PCI_BASE_ADDRESS_SPACE_IO) ||
	    (bar & ~PCI_BASE_ADDRESS_MEM_ATTR_MASK) != res->base ||
	    !(pci_read_config16(dev, PCI_COMMAND) & PCI_COMMAND_MEMORY))
		die("ICH1 thermal BAR admission failed before MMIO.\n");

	thermal_program(res2mmio(res, 0, 0));
}

#if CONFIG(SOUTHBRIDGE_INTEL_I82801JX) && !CONFIG(SOUTHBRIDGE_INTEL_I82801JX_DIRECT_DEVICE_MODEL)
static void thermal_init(struct device *dev)
{
#if CONFIG(SOUTHBRIDGE_INTEL_I82801JX_BOARD_OWNED_DEVICE_POLICY)
	i82801jx_thermal_init_assigned(dev);
#else
	pci_write_config32(dev, PCI_BASE_ADDRESS_0, (uintptr_t)DEFAULT_TBAR);
	pci_or_config32(dev, PCI_COMMAND, PCI_COMMAND_MEMORY);
	thermal_program(DEFAULT_TBAR);
	pci_and_config32(dev, PCI_COMMAND, ~PCI_COMMAND_MEMORY);
	pci_write_config32(dev, PCI_BASE_ADDRESS_0, 0);
#endif
}

static struct device_operations device_ops = {
	.read_resources		= pci_dev_read_resources,
	.set_resources		= pci_dev_set_resources,
	.enable_resources	= pci_dev_enable_resources,
	.init			= thermal_init,
	.ops_pci		= &pci_dev_ops_pci,
};

static const unsigned short pci_device_ids[] = {
	0x3a32,
	0x3a62,
	0
};

static const struct pci_driver ich10_thermal __pci_driver = {
	.ops	= &device_ops,
	.vendor	= PCI_VID_INTEL,
	.devices	= pci_device_ids,
};
#endif
