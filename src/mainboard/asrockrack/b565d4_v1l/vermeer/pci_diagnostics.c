/* SPDX-License-Identifier: GPL-2.0-only */

#include <arch/io.h>
#include <bootstate.h>
#include <console/console.h>
#include <console/uart.h>
#include <cpu/x86/msr.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ops.h>
#include <static.h>

static uint32_t read_cf8(pci_devfn_t bdf, unsigned int reg, unsigned int bytes)
{
	const uint32_t saved = inl(PCI_IO_CONFIG_INDEX);
	uint32_t value;

	if (bytes == 1)
		value = pci_io_read_config8(bdf, reg);
	else if (bytes == 2)
		value = pci_io_read_config16(bdf, reg);
	else
		value = pci_io_read_config32(bdf, reg);
	outl(saved, PCI_IO_CONFIG_INDEX);
	return value;
}

static bool selected(const struct device *dev)
{
	if (dev == DEV_PTR(gpp_bridge_0) || dev == DEV_PTR(gpp_bridge_a))
		return true;
	if (dev->vendor != 0x1022)
		return dev->vendor == 0x8086 && dev->device == 0x1533;
	return dev->device == 0x43ef || dev->device == 0x43eb ||
		dev->device == 0x43e9 || dev->device == 0x43ea;
}

static void log_pci_configuration(void *unused)
{
	static const unsigned int registers[] = { 0, 4, 0x10, 0x18, 0x24 };
	const uint32_t original_cf8 = inl(PCI_IO_CONFIG_INDEX);
	unsigned int devices = 0, differences = 0;

	(void)unused;
	printk(BIOS_NOTICE, "Matisse PCI diagnostics BEGIN CF8=%08x MC22=%016llx\n",
	       original_cf8, rdmsr(0xc0002161).raw);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	for (const struct device *dev = all_devices; dev; dev = dev->next) {
		if (dev->path.type != DEVICE_PATH_PCI || !dev->enabled ||
		    !dev->upstream || dev->upstream->segment_group || !selected(dev))
			continue;
		const pci_devfn_t bdf = pcidev_bdf(dev);
		const uintptr_t ecam = CONFIG_ECAM_MMCONF_BASE_ADDRESS + bdf;

		devices++;
		for (size_t i = 0; i < ARRAY_SIZE(registers); i++) {
			const unsigned int reg = registers[i];
			uint32_t mmio[3] = { 0 }, io[3] = { 0 };

			printk(BIOS_NOTICE, "Matisse PCI diagnostics READ %s reg=%02x\n",
			       dev_path(dev), reg);
			uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
			mmio[0] = *(volatile uint32_t *)(ecam + reg);
			io[0] = read_cf8(bdf, reg, 4);
			for (unsigned int n = 0; n < 2; n++) {
				mmio[1] |= (uint32_t)*(volatile uint16_t *)(ecam + reg + 2 * n)
					<< (16 * n);
				io[1] |= read_cf8(bdf, reg + 2 * n, 2) << (16 * n);
			}
			for (unsigned int n = 0; n < 4; n++) {
				mmio[2] |= (uint32_t)*(volatile uint8_t *)(ecam + reg + n)
					<< (8 * n);
				io[2] |= read_cf8(bdf, reg + n, 1) << (8 * n);
			}
			const bool same = mmio[0] == io[0] && mmio[0] == mmio[1] &&
				mmio[0] == io[1] && mmio[0] == mmio[2] && mmio[0] == io[2];

			differences += !same;
			printk(BIOS_NOTICE, "Matisse PCI diagnostics RESULT %s reg=%02x "
			       "ECAM32=%08x CF8_32=%08x ECAM16=%08x CF8_16=%08x "
			       "ECAM8=%08x CF8_8=%08x same=%u CF8_restored=%u\n",
			       dev_path(dev), reg, mmio[0], io[0], mmio[1], io[1],
			       mmio[2], io[2], same, inl(PCI_IO_CONFIG_INDEX) == original_cf8);
		}
	}
	printk(BIOS_NOTICE, "Matisse PCI diagnostics END devices=%u differences=%u "
	       "CF8_restored=%u MC22=%016llx\n", devices, differences,
	       inl(PCI_IO_CONFIG_INDEX) == original_cf8, rdmsr(0xc0002161).raw);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_BOOT, BS_ON_ENTRY, log_pci_configuration, NULL);
