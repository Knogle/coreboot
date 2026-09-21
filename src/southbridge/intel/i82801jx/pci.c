/* SPDX-License-Identifier: GPL-2.0-only */

#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ops.h>
#include <device/pci_ids.h>
#include "i82801jx.h"
#include "pci_bridge_init.h"
#if CONFIG(SOUTHBRIDGE_INTEL_I82801JX)
#include "board_policy.h"
#endif

void i82801jx_pci_bridge_init(struct device *dev)
{
	u16 reg16;

	/* This device has no interrupt */
	pci_write_config8(dev, PCI_INTERRUPT_LINE, 0xff);

	/* Master Latency Count must be set to 0x04! */
	pci_update_config8(dev, D30F0_SMLT, 0x07, 0x04 << 3);

	/* Clear errors in status registers. FIXME: Do something? */
	reg16 = pci_read_config16(dev, PCI_STATUS);
	//reg16 |= 0xf900;
	pci_write_config16(dev, PCI_STATUS, reg16);

	reg16 = pci_read_config16(dev, PCI_SEC_STATUS);
	// reg16 |= 0xf900;
	pci_write_config16(dev, PCI_SEC_STATUS, reg16);
}

#if CONFIG(SOUTHBRIDGE_INTEL_I82801JX) && !CONFIG(SOUTHBRIDGE_INTEL_I82801JX_DIRECT_DEVICE_MODEL)
static void pci_init(struct device *dev)
{
	/* A board-owned policy may retain bounded root-port admission. Keep that
	 * decision in the generic-driver adapter, not in the reusable bridge
	 * initialization sequence above. */
	if (CONFIG(SOUTHBRIDGE_INTEL_I82801JX_BOARD_OWNED_DEVICE_POLICY) &&
	    mainboard_ich10_pci_bridge_isolated(dev))
		return;

	i82801jx_pci_bridge_init(dev);
}

static struct device_operations device_ops = {
	.read_resources		= pci_bus_read_resources,
	.set_resources		= pci_dev_set_resources,
	.enable_resources	= pci_bus_enable_resources,
	.init			= pci_init,
#if CONFIG(SOUTHBRIDGE_INTEL_I82801JX_BOARD_OWNED_DEVICE_POLICY)
	.scan_bus		= mainboard_ich10_pci_scan_bridge,
#else
	.scan_bus		= pci_scan_bridge,
#endif
	.reset_bus		= pci_bus_reset,
	.ops_pci		= &pci_dev_ops_pci,
};

static const unsigned short pci_device_ids[] = {
	0x244e,
	0
};

static const struct pci_driver ich10_pci __pci_driver = {
	.ops		= &device_ops,
	.vendor		= PCI_VID_INTEL,
	.devices	= pci_device_ids,
};
#endif
