/* SPDX-License-Identifier: GPL-2.0-only */

#include <console/console.h>
#include <console/uart.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ops.h>

extern struct device_operations amd_external_pcie_gpp_ops;

u32 pci_scan_vendor_id_hook(const struct device *dev)
{
	const struct device *parent = dev->upstream->dev;
	const bool trace = PCI_FUNC(dev->path.pci.devfn) == 0 && parent &&
		parent->ops == &amd_external_pcie_gpp_ops;

	if (trace) {
		printk(BIOS_INFO, "NATIVE104 CHILD ID BEGIN %s ECAM=%08x\n",
		       dev_path(dev), CONFIG_ECAM_MMCONF_BASE_ADDRESS + PCI_BDF(dev));
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	}
	u32 id = pci_read_config32(dev, PCI_VENDOR_ID);
	if (trace) {
		printk(BIOS_INFO, "NATIVE104 CHILD ID END %s id=%08x\n", dev_path(dev), id);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	}
	return id;
}
