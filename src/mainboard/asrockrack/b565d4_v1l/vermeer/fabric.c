/* SPDX-License-Identifier: GPL-2.0-only */

#include "platform.h"
#include "pci_irq.h"
#include "native_apob_resources.h"
#include <acpi/acpigen.h>
#include <acpi/acpigen_pci.h>
#include <amdblocks/data_fabric.h>
#include <amdblocks/root_complex.h>
#include <console/console.h>
#include <cpu/amd/mtrr.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ops.h>
#include <halt.h>

/* The first posted MMIO window is sufficient for the initial below-4GiB
 * allocator. Its encoding comes from ASRock FabricSsp; do not infer the total
 * register count or optional attributes from the Cezanne header. No disabled
 * high window is probed for writability or used as scratch.
 */
#define DF_MMIO0_POSTED_CONTROL 0x43U
static uint64_t low_mmio;

static void write_checked(uint16_t reg, uint32_t value)
{
	data_fabric_broadcast_write32(reg, value);
	if (data_fabric_broadcast_read32(reg) != value)
		die(B565D4_DESKTOP_NAME " DF write readback failed");
}

void b565d4_desktop_fabric_prepare(void)
{
	const uint32_t id = pci_s_read_config32(PCI_DEV(0, 0x18, 0), PCI_VENDOR_ID);
	const uint32_t control = data_fabric_broadcast_read32(DF_MMIO_CONTROL(0));

	low_mmio = get_top_of_mem_below_4gb();
	if (id != 0x14401022 || low_mmio < 0x10000000ULL ||
	    low_mmio >= 0xf0000000ULL || (low_mmio & 0xffff) ||
	    native_apob_require_pci_hole(low_mmio, 0x100000000ULL) != NATIVE_APOB_OK ||
	    (control != 0x40 && control != DF_MMIO0_POSTED_CONTROL && control))
		die(B565D4_DESKTOP_NAME " DF identity/low PCI aperture unavailable");

	/* Disable the known window while changing its bounds, then publish its
	 * read/write route to desktop IOMS0=4. Preserve all other windows.
	 */
	write_checked(DF_MMIO_CONTROL(0), 0x40);
	write_checked(DF_MMIO_BASE(0), low_mmio >> DF_MMIO_SHIFT);
	write_checked(DF_MMIO_LIMIT(0), 0xfeff);
	write_checked(DF_MMIO_CONTROL(0), DF_MMIO0_POSTED_CONTROL);
	write_checked(DF_IO_BASE(0), 0);
	write_checked(DF_IO_LIMIT(0), 0x0000f004);
	write_checked(DF_IO_BASE(0), 3);
	write_checked(DF_PCI_CFG_MAP(0), 0x7f000043);
	printk(BIOS_NOTICE, B565D4_DESKTOP_NAME " DF: posted [%llx,ff000000), IO0..ffff, buses0..127\n",
	       low_mmio);
}

bool b565d4_desktop_posted_route(uint64_t begin, uint64_t end)
{
	return begin < end && begin >= low_mmio && end <= 0xff000000ULL &&
		data_fabric_broadcast_read32(DF_MMIO_BASE(0)) == low_mmio >> 16 &&
		data_fabric_broadcast_read32(DF_MMIO_LIMIT(0)) == 0xfeff &&
		data_fabric_broadcast_read32(DF_MMIO_CONTROL(0)) == DF_MMIO0_POSTED_CONTROL;
}

static void desktop_domain_read_resources(struct device *domain)
{
	unsigned long index = 0;
	struct resource *res;

	res = new_resource(domain, index++);
	res->base = 0;
	res->limit = 0xffff;
	res->flags = IORESOURCE_IO | IORESOURCE_ASSIGNED;
	res = new_resource(domain, index++);
	res->base = low_mmio;
	res->limit = 0xfdffffff;
	res->flags = IORESOURCE_MEM | IORESOURCE_ASSIGNED;
	read_non_pci_resources(domain, &index);
	fixed_io_range_reserved(domain, index++, PCI_IO_CONFIG_INDEX, PCI_IO_CONFIG_PORT_COUNT);
	mmconf_resource(domain, index++);
	/* Original FabricSsp's fixed high-MMIO GCD reservation, independent of
	 * the supplier's DRAM holes. The low arena/SMM/APOB are published by the
	 * already shared checked memory provider.
	 */
	mmio_range(domain, index++, 0xfe000000, 0x02000000);
	read_soc_memmap_resources(domain, &index);
}

static void desktop_domain_scan_bus(struct device *domain)
{
	domain->downstream->secondary = 0;
	domain->downstream->subordinate = 0;
	domain->downstream->max_subordinate = 127;
	domain->downstream->segment_group = 0;
	pci_host_bridge_scan_bus(domain);
}

static const char *domain_name(const struct device *dev)
{
	return "PCI0";
}

static void domain_ssdt(const struct device *dev)
{
	pci_domain_fill_ssdt(dev);
	if (CONFIG(B565D4_DESKTOP_INTX))
		b565d4_desktop_pci_irq_ssdt(dev);
}

struct device_operations cezanne_pci_domain_ops = {
	.read_resources = desktop_domain_read_resources,
	.set_resources = pci_domain_set_resources,
	.scan_bus = desktop_domain_scan_bus,
	.init = amd_pci_domain_init,
	.acpi_name = domain_name,
	.acpi_fill_ssdt = domain_ssdt,
};

struct device_operations amd_data_fabric_ops = {
	.read_resources = noop_read_resources,
	.set_resources = noop_set_resources,
};
