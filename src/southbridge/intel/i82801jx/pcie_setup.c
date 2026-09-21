/* SPDX-License-Identifier: GPL-2.0-only */

#include <console/console.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ops.h>
#include "chip.h"
#include "pcie_init.h"

void i82801jx_pcie_required_fields(struct device *dev)
{
	/* Intel ICH10 Datasheet 20.1.71: preserve all other PECR2 bits. */
	pci_or_config32(dev, I82801JX_PCIE_PECR2, I82801JX_PCIE_PECR2_REQUIRED);
	/* 20.1.73: only PEC1 bits 7:0 are writable; preserve bits 31:8. */
	pci_write_config8(dev, I82801JX_PCIE_PEC1, I82801JX_PCIE_PEC1_REQUIRED);
}

static void validate_ports(struct device *const ports[I82801JX_PCIE_PORT_COUNT],
	bool require_enabled)
{
	if (!ports)
		die("ICH10 PCIe setup requires root-port devices.\n");
	for (unsigned int i = 0; i < I82801JX_PCIE_PORT_COUNT; i++) {
		if (!ports[i] || ports[i]->path.type != DEVICE_PATH_PCI ||
		    ports[i]->path.pci.devfn != PCI_DEVFN(0x1c, i))
			die("ICH10 PCIe setup requires six ordered root-port devices.\n");
		if (require_enabled && !ports[i]->enabled)
			die("ICH10 PCIe retained setup requires six enabled root ports.\n");
	}
}

void i82801jx_pcie_setup(struct device *const ports[I82801JX_PCIE_PORT_COUNT],
	const struct southbridge_intel_i82801jx_config *config)
{
	unsigned int slot_number = 1; /* Reserve slot number 0 for northbridge PEG. */

	/* Admit the whole caller-supplied graph before the first hardware write. */
	if (!config)
		die("ICH10 PCIe setup requires board configuration.\n");
	validate_ports(ports, false);

	for (unsigned int i = 0; i < I82801JX_PCIE_PORT_COUNT; i++)
		i82801jx_pcie_required_fields(ports[i]);

	/* Retained reference policy for the contiguous highest disabled ports.
	 * This is deliberately NOT part of the required-fields-only helper.
	 */
	for (int i = I82801JX_PCIE_PORT_COUNT - 1; i >= 0 && !ports[i]->enabled; i--)
		pci_or_config32(ports[i], I82801JX_PCIE_PECR2, 0x3 << 16);

	for (unsigned int i = 0; i < I82801JX_PCIE_PORT_COUNT; i++) {
		struct device *const dev = ports[i];
		/* Datasheet 20.1.24: XCAP is a WORD at 42h, not a DWORD.
		 * A DWORD accessor can round down to the capability header at 40h.
		 */
		u16 xcap = pci_read_config16(dev, I82801JX_PCIE_XCAP);
		if (config->pcie_slot_implemented & (1 << i))
			xcap |= PCI_EXP_FLAGS_SLOT;
		else
			xcap &= ~PCI_EXP_FLAGS_SLOT;
		pci_write_config16(dev, I82801JX_PCIE_XCAP, xcap);

		if (config->pcie_slot_implemented & (1 << i)) {
			u32 slcap = pci_read_config32(dev, I82801JX_PCIE_SLCAP);
			slcap &= ~(0x1fffU << 19);
			slcap |= slot_number++ << 19;
			/* Datasheet 20.1.31: scale is bits 16:15; bit 17 is reserved. */
			slcap &= ~(3U << I82801JX_PCIE_SLCAP_POWER_SCALE_SHIFT);
			slcap |= config->pcie_power_limits[i].scale <<
				I82801JX_PCIE_SLCAP_POWER_SCALE_SHIFT;
			slcap &= ~(0x00ff << 7);
			slcap |= config->pcie_power_limits[i].value << 7;
			pci_write_config32(dev, I82801JX_PCIE_SLCAP, slcap);
		}
	}

	/* Retained reference write-once policy; only after all slot configuration. */
	for (unsigned int i = 0; i < I82801JX_PCIE_PORT_COUNT; i++)
		pci_update_config32(ports[i], I82801JX_PCIE_LCAP, ~0, 0);
}

void i82801jx_pcie_setup_retained(
	struct device *const ports[I82801JX_PCIE_PORT_COUNT])
{
	u16 xcap[I82801JX_PCIE_PORT_COUNT];
	u32 slcap[I82801JX_PCIE_PORT_COUNT], lcap[I82801JX_PCIE_PORT_COUNT];

	validate_ports(ports, true);
	/* All snapshots precede the first write, including required fields. */
	for (unsigned int i = 0; i < I82801JX_PCIE_PORT_COUNT; i++) {
		xcap[i] = pci_read_config16(ports[i], I82801JX_PCIE_XCAP);
		slcap[i] = pci_read_config32(ports[i], I82801JX_PCIE_SLCAP);
		lcap[i] = pci_read_config32(ports[i], I82801JX_PCIE_LCAP);
		if (xcap[i] == 0xffff || slcap[i] == 0xffffffff || lcap[i] == 0xffffffff)
			die("ICH10 PCIe retained capabilities are inaccessible.\n");
	}

	for (unsigned int i = 0; i < I82801JX_PCIE_PORT_COUNT; i++)
		i82801jx_pcie_required_fields(ports[i]);

	/* Datasheet 20.1.24/31: preserve slot implementation, physical number,
	 * power limits and hotplug capability; do not infer board wiring.
	 */
	for (unsigned int i = 0; i < I82801JX_PCIE_PORT_COUNT; i++) {
		pci_write_config16(ports[i], I82801JX_PCIE_XCAP, xcap[i]);
		pci_write_config32(ports[i], I82801JX_PCIE_SLCAP, slcap[i]);
	}

	/* 20.1.28: retain/lock advertised ASPM support, not the live LCTL policy. */
	for (unsigned int i = 0; i < I82801JX_PCIE_PORT_COUNT; i++)
		pci_write_config32(ports[i], I82801JX_PCIE_LCAP, lcap[i]);
}
