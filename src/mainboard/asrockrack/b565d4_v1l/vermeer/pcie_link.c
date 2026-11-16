/* SPDX-License-Identifier: GPL-2.0-only */

#include "platform.h"
#include "board_config.h"
#include <amdblocks/amd_pci_util.h>
#include <arch/pci_io_cfg.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <console/uart.h>
#include <cpu/x86/msr.h>
#include <delay.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ops.h>
#include <device/pciexp.h>
#include <static.h>

static void log_config_access(struct device *dev, unsigned int cap, const char *phase)
{
	const pci_devfn_t bdf = pcidev_bdf(dev);

	printk(BIOS_NOTICE, "%s PCIe access %s %s bdf=%08x: "
	       "id=%08x/%08x cap=%08x/%08x cap2=%08x/%08x "
	       "ctl2word=%04x ctl2dword=%08x/%08x (ECAM/CF8)\n",
	       B565D4_DESKTOP_NAME, phase, dev_path(dev), bdf,
	       pci_read_config32(dev, PCI_VENDOR_ID), pci_io_read_config32(bdf, PCI_VENDOR_ID),
	       pci_read_config32(dev, cap + PCI_EXP_LNKCAP),
	       pci_io_read_config32(bdf, cap + PCI_EXP_LNKCAP),
	       pci_read_config32(dev, cap + PCI_EXP_LNKCAP2),
	       pci_io_read_config32(bdf, cap + PCI_EXP_LNKCAP2),
	       pci_read_config16(dev, cap + PCI_EXP_LNKCTL2),
	       pci_read_config32(dev, cap + PCI_EXP_LNKCTL2),
	       pci_io_read_config32(bdf, cap + PCI_EXP_LNKCTL2));
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

static void log_config_memory_type(void)
{
	static bool logged;
	static const unsigned int indices[] = {
		0x2ff, 0xc0010010, 0xc001001a, 0xc0010058, 0x277,
	};

	if (logged)
		return;
	logged = true;
	for (unsigned int i = 0; i < ARRAY_SIZE(indices); i++) {
		const msr_t value = rdmsr(indices[i]);

		printk(BIOS_NOTICE, "%s PCIe access MSR%08x=%08x:%08x\n",
		       B565D4_DESKTOP_NAME, indices[i], value.hi, value.lo);
	}
	const unsigned int count = rdmsr(0xfe).lo & 0xff;

	for (unsigned int i = 0; i < count && i < 8; i++) {
		const msr_t base = rdmsr(0x200 + 2 * i);
		const msr_t mask = rdmsr(0x201 + 2 * i);

		printk(BIOS_NOTICE, "%s PCIe access MTRR%u=%08x:%08x/%08x:%08x\n",
		       B565D4_DESKTOP_NAME, i, base.hi, base.lo, mask.hi, mask.lo);
	}
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

static bool link_stable(struct device *dev, unsigned int cap)
{
	/* A single DLL-active sample can occur during repeated recovery. */
	for (unsigned int i = 0; i < 6; i++) {
		const uint16_t status = pci_read_config16(dev, cap + PCI_EXP_LNKSTA);

		if (status == UINT16_MAX || !(status & PCI_EXP_LNKSTA_DLLLA) ||
		    (status & PCI_EXP_LNKSTA_LT))
			return false;
		if (i != 5)
			mdelay(20);
	}
	return true;
}

static bool limit_port(struct device *dev, unsigned int speed)
{
	const uint32_t id = pci_read_config32(dev, PCI_VENDOR_ID);

	printk(BIOS_NOTICE, "%s PCIe limit probe %s: id=%08x target=Gen%u\n",
	       B565D4_DESKTOP_NAME, dev_path(dev), id, speed);
	if (id != 0x14831022)
		return false;
	const unsigned int cap = pci_find_capability(dev, PCI_CAP_ID_PCIE);

	if (cap < 0x40 || cap > 0xcc) {
		printk(BIOS_WARNING, "%s: PCIe limit capability unavailable: %02x\n",
		       dev_path(dev), cap);
		return false;
	}
	const uint16_t flags = pci_read_config16(dev, cap + PCI_EXP_FLAGS);
	const uint32_t link_cap = pci_read_config32(dev, cap + PCI_EXP_LNKCAP);
	const uint16_t status = pci_read_config16(dev, cap + PCI_EXP_LNKSTA);
	const uint32_t speeds = pci_read_config32(dev, cap + PCI_EXP_LNKCAP2);
	const unsigned int negotiated = status & PCI_EXP_LNKSTA_CLS;

	log_config_memory_type();
	log_config_access(dev, cap, "before");
	printk(BIOS_NOTICE, "%s PCIe limit capabilities %s: cap=%02x flags=%04x "
	       "linkcap=%08x link=%04x speeds=%08x\n", B565D4_DESKTOP_NAME,
	       dev_path(dev), cap, flags, link_cap, status, speeds);
	if ((flags & PCI_EXP_FLAGS_VERS) < 2 ||
	    (flags & PCI_EXP_FLAGS_TYPE) != (PCI_EXP_TYPE_ROOT_PORT << 4) ||
	    link_cap == UINT32_MAX || !(link_cap & PCI_EXP_LNKCAP_MLW) ||
	    !(link_cap & PCI_EXP_LNKCAP_DLLLARC) || status == UINT16_MAX ||
	    !(status & PCI_EXP_LNKSTA_NLW) || speeds == UINT32_MAX)
		return false;
	if (!(speeds & (1U << speed))) {
		/* These CPU ports can advertise Gen1 while already negotiating Gen3.
		 * Use that observed generation only for a downward speed request.
		 */
		if ((speeds & 0xfeU) != 2 || negotiated < speed || negotiated > 4)
			return false;
		printk(BIOS_WARNING, "%s: PCIe advertised speeds=%08x, observed Gen%u; "
		       "applying configured lower Gen%u limit\n", dev_path(dev), speeds,
		       negotiated, speed);
	}
	const uint16_t before = pci_read_config16(dev, cap + PCI_EXP_LNKCTL2);
	const uint16_t wanted = (before & ~PCI_EXP_LNKCTL2_TLS) | speed;

	if (before == UINT16_MAX)
		return false;
	if (before == wanted && negotiated == speed && link_stable(dev, cap)) {
		printk(BIOS_NOTICE, "%s: PCIe Gen%u already stable before scan\n",
		       dev_path(dev), speed);
		return true;
	}
	printk(BIOS_NOTICE, "%s PCIe limit %s: Gen%u control=%04x -> %04x link=%04x\n",
	       B565D4_DESKTOP_NAME, dev_path(dev), speed, before, wanted, status);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	pci_write_config16(dev, cap + PCI_EXP_LNKCTL2, wanted);
	const uint16_t after = pci_read_config16(dev, cap + PCI_EXP_LNKCTL2);

	if (after != wanted) {
		printk(BIOS_WARNING, "%s: PCIe speed limit readback failed: %04x != %04x\n",
		       dev_path(dev), after, wanted);
		log_config_access(dev, cap, "readback");
		return true;
	}
	/* Use the ordinary coreboot LT-clear/retrain/DLL-active sequence once. */
	const int result = pciexp_retrain_link(dev, cap);
	const bool stable = !result && link_stable(dev, cap);

	printk(stable ? BIOS_NOTICE : BIOS_WARNING,
	       "%s PCIe limit %s: retrain=%d stable=%u link=%04x control=%04x\n",
	       B565D4_DESKTOP_NAME, dev_path(dev), result, stable,
	       pci_read_config16(dev, cap + PCI_EXP_LNKSTA),
	       pci_read_config16(dev, cap + PCI_EXP_LNKCTL2));
	return true;
}

void mainboard_pcie_gpp_pre_scan(struct device *dev)
{
	const unsigned int speed = CONFIG_B565D4_DESKTOP_PCIE_MAX_SPEED;
	static bool prepared[5];

	if (!speed)
		return;
	printk(BIOS_DEBUG, "%s PCIe pre-scan %s target=Gen%u\n",
	       B565D4_DESKTOP_NAME, dev_path(dev), speed);
	struct device *ports[] = {
		DEV_PTR(gpp_bridge_1), DEV_PTR(gpp_gfx_bridge_0),
		DEV_PTR(gpp_gfx_bridge_1), DEV_PTR(gpp_gfx_bridge_2), DEV_PTR(gpp_bridge_2),
	};
	const unsigned int count = b565d4_pcie_topology() == CZN_PCIE_FOUR_X4 ?
		ARRAY_SIZE(ports) : 2;

	for (unsigned int i = 0; i < count; i++) {
		if (dev != ports[i])
			continue;
		if (!prepared[i])
			prepared[i] = limit_port(dev, speed);
		return;
	}
}
