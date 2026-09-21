/* SPDX-License-Identifier: GPL-2.0-only */

#include <console/console.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ops.h>
#include <device/pciexp.h>
#include <device/pci_ids.h>
#include <southbridge/intel/common/pciehp.h>
#include "chip.h"
#include "pcie_init.h"
#if CONFIG(SOUTHBRIDGE_INTEL_I82801JX_BOARD_OWNED_DEVICE_POLICY)
#include "board_policy.h"
#endif

static void pcie_init_sequence(struct device *dev,
	const struct southbridge_intel_i82801jx_config *config, bool enable_bus_master)
{
	/* Reject invalid callers before any write, including a hotplug-map index. */
	if (!dev || !config || dev->path.type != DEVICE_PATH_PCI ||
	    dev->path.pci.devfn < PCI_DEVFN(0x1c, 0) ||
	    dev->path.pci.devfn >= PCI_DEVFN(0x1c, I82801JX_PCIE_PORT_COUNT))
		die("ICH10 PCIe init requires a root port and board configuration.\n");

	printk(BIOS_DEBUG, "Initializing ICH10 PCIe root port.\n");

	/* The normal driver owns BME. Explicit callers may already own commands. */
	if (enable_bus_master)
		pci_or_config16(dev, PCI_COMMAND, PCI_COMMAND_MASTER);

	/* Set Cache Line Size to 0x10 */
	// This has no effect but the OS might expect it
	pci_write_config8(dev, 0x0c, 0x10);

	pci_and_config16(dev, PCI_BRIDGE_CONTROL, ~PCI_BRIDGE_CTL_PARITY);

	/* Enable IO xAPIC on this PCIe port */
	pci_or_config32(dev, 0xd8, 1 << 7);

	/* ICH10 Datasheet 20.1.51: RPDCGEN is an 8-bit register at E1h.
	 * Do not use a DWORD accessor: that can address E0h instead.
	 */
	pci_or_config8(dev, I82801JX_PCIE_RPDCGEN, I82801JX_PCIE_DYNAMIC_CLOCKS);

	/* Set VC0 transaction class */
	pci_update_config32(dev, 0x114, ~0x000000ff, 1);

	/* Mask completion timeouts */
	pci_or_config32(dev, 0x148, 1 << 14);

	/* Lock R/WO Correctable Error Mask. */
	pci_update_config32(dev, 0x154, ~0, 0);

	/* Clear errors in status registers */
	pci_update_config16(dev, 0x06, ~0, 0);
	pci_update_config16(dev, 0x1e, ~0, 0);

	/* Get configured ASPM state */
	const enum aspm_type apmc = pci_read_config32(dev, 0x50) & 3;

	/* If both L0s and L1 enabled then set root port 0xE8[1]=1 */
	if (apmc == PCIE_ASPM_BOTH)
		pci_or_config32(dev, 0xe8, 1 << 1);

	/* Enable expresscard hotplug events.  */
	if (config->pcie_hotplug_map[PCI_FUNC(dev->path.pci.devfn)]) {
		pci_or_config32(dev, 0xd8, 1 << 30);
		pci_write_config16(dev, 0x42, 0x142);
	}
}

void i82801jx_pcie_init_sequence(struct device *dev,
	const struct southbridge_intel_i82801jx_config *config)
{
	pcie_init_sequence(dev, config, true);
}

void i82801jx_pcie_init_sequence_preserve_command(struct device *dev,
	const struct southbridge_intel_i82801jx_config *config)
{
	pcie_init_sequence(dev, config, false);
}

#if CONFIG(SOUTHBRIDGE_INTEL_I82801JX) && !CONFIG(SOUTHBRIDGE_INTEL_I82801JX_DIRECT_DEVICE_MODEL)
static void pci_init(struct device *dev)
{
#if CONFIG(SOUTHBRIDGE_INTEL_I82801JX_BOARD_OWNED_DEVICE_POLICY)
	if (mainboard_ich10_pci_bridge_isolated(dev))
		return;
#endif
	i82801jx_pcie_init_sequence(dev, dev->chip_info);
}

static void pch_pciexp_scan_bridge(struct device *dev)
{
#if CONFIG(SOUTHBRIDGE_INTEL_I82801JX_BOARD_OWNED_DEVICE_POLICY)
	mainboard_ich10_pci_scan_bridge(dev);
#else
	struct southbridge_intel_i82801jx_config *config = dev->chip_info;

	if (CONFIG(PCIEXP_HOTPLUG) && config->pcie_hotplug_map[PCI_FUNC(dev->path.pci.devfn)]) {
		pciexp_hotplug_scan_bridge(dev);
	} else {
		/* Normal PCIe Scan */
		pciexp_scan_bridge(dev);
	}
#endif
}

static struct device_operations device_ops = {
	.read_resources		= pci_bus_read_resources,
	.set_resources		= pci_dev_set_resources,
	.enable_resources	= pci_bus_enable_resources,
	.init			= pci_init,
	.scan_bus		= pch_pciexp_scan_bridge,
	.ops_pci		= &pci_dev_ops_pci,
};

/* 82801lJx, ICH10  */
static const unsigned short pci_device_ids[] = {
	0x3a40, /* Port 1 */
	0x3a42, /* Port 2 */
	0x3a44, /* Port 3 */
	0x3a46, /* Port 4 */
	0x3a48, /* Port 5 */
	0x3a4a, /* Port 6 */

	0x3a70, /* Port 1 */
	0x3a72, /* Port 2 */
	0x3a74, /* Port 3 */
	0x3a76, /* Port 4 */
	0x3a78, /* Port 5 */
	0x3a7a, /* Port 6 */
	0
};

static const struct pci_driver ich10_pcie __pci_driver = {
	.ops		= &device_ops,
	.vendor		= PCI_VID_INTEL,
	.devices	= pci_device_ids,
};
#endif
