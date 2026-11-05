/* SPDX-License-Identifier: GPL-2.0-only */

#include <acpi/acpi.h>
#include <amdblocks/iommu.h>
#include <console/console.h>
#include <device/device.h>
#include <device/pci.h>
#include <static.h>
#include "setup_options.h"

static bool iommu_disabled;

/* Use the shared architectural SVM policy after the ordinary per-CPU init.
 * The native desktop CPU table and its ordering remain independent.
 */
void __real_amd_cpu_init(struct device *dev);
void __wrap_amd_cpu_init(struct device *dev);
void __wrap_amd_cpu_init(struct device *dev)
{
	__real_amd_cpu_init(dev);
	b565d4_setup_cpu();
}

void b565d4_setup_iommu(void)
{
	if (b565d4_setup_get(B565D4_OPT_IOMMU))
		return;
	struct device *dev = DEV_PTR(iommu);
	uint32_t identity = pci_read_config32(dev, PCI_VENDOR_ID);
	uint32_t base = pci_read_config32(dev, IOMMU_CAP_BASE_LO);

	if (identity != 0x14811022 || base == UINT32_MAX) {
		printk(BIOS_WARNING, "Desktop setup: IOMMU identity unavailable\n");
		return;
	}

	/* This runs after native IOHC preparation and before PCI enumeration.
	 * Suppress the OS interface without retagging its reserved MMIO as RAM.
	 */
	pci_write_config32(dev, IOMMU_CAP_BASE_LO, base & ~IOMMU_ENABLE);
	uint32_t after = pci_read_config32(dev, IOMMU_CAP_BASE_LO);

	if (after == UINT32_MAX || (after & IOMMU_ENABLE)) {
		printk(BIOS_WARNING, "Desktop setup: cannot disable IOMMU decode\n");
		return;
	}
	dev->enabled = false;
	iommu_disabled = true;
	printk(BIOS_NOTICE, "Desktop setup: IOMMU disabled, cap base=%08x\n",
	       after);
}

unsigned long __real_acpi_add_ivrs_table(unsigned long current, acpi_rsdp_t *rsdp);
unsigned long __wrap_acpi_add_ivrs_table(unsigned long current, acpi_rsdp_t *rsdp);
unsigned long __wrap_acpi_add_ivrs_table(unsigned long current, acpi_rsdp_t *rsdp)
{
	if (iommu_disabled)
		return current;
	return __real_acpi_add_ivrs_table(current, rsdp);
}
