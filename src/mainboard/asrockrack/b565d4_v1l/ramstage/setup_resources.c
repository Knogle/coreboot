/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/data_fabric.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <cpu/amd/mtrr.h>
#include <cpu/cpu.h>
#include <cpu/x86/msr.h>
#include <device/device.h>
#include <device/resource.h>
#include "setup_options.h"

/* Preserve all DRAM/PSP/SMRAM and existing low/fixed MMIO resources. */
#define HIGH_MMIO_BYTES (64ULL * GiB)

static bool high_mmio_ready;

bool b565d4_setup_high_mmio_ready(void)
{
	return high_mmio_ready;
}

void b565d4_setup_high_mmio(void)
{
	struct { uint32_t base, limit, control; } before[DF_MMIO_REG_SET_COUNT];
	unsigned int selected = DF_MMIO_REG_SET_COUNT;
	union df_mmio_control disabled = { .raw = 0 };
	unsigned int bits = soc_phys_address_size();
	msr_t top = rdmsr(TOP_MEM2_MSR);
	uint64_t tom2 = (uint64_t)top.hi << 32 | top.lo;
	uint64_t begin = ALIGN_UP(MAX(tom2, 4ULL * GiB), HIGH_MMIO_BYTES);
	uint64_t end = begin + HIGH_MMIO_BYTES;

	if (!b565d4_setup_get(B565D4_OPT_ABOVE_4G))
		return;
	/* This Cezanne ABI encodes address bits47..16. The SoC reserves the
	 * physical top twelve GiB. No aperture crosses either boundary.
	 */
	if (bits < 37 || bits > 48 || begin < tom2 || end <= begin ||
	    end > (1ULL << bits) - DF_RESERVED_TOP_12GB_MMIO_SIZE)
		goto unavailable;
	disabled.dst_fabric_id = IOMS0_FABRIC_ID;
	for (unsigned int i = 0; i < ARRAY_SIZE(before); i++) {
		before[i].base = data_fabric_broadcast_read32(DF_MMIO_BASE(i));
		before[i].limit = data_fabric_broadcast_read32(DF_MMIO_LIMIT(i));
		before[i].control = data_fabric_broadcast_read32(DF_MMIO_CONTROL(i));
		union df_mmio_control ctrl = { .raw = before[i].control };

		if (ctrl.raw == UINT32_MAX)
			goto unavailable;
		if (ctrl.re || ctrl.we) {
			uint64_t lo = (uint64_t)before[i].base << DF_MMIO_SHIFT;
			uint64_t hi = ((uint64_t)before[i].limit + 1) << DF_MMIO_SHIFT;

			if (hi <= lo || (lo < end && begin < hi))
				goto unavailable;
		} else if (!before[i].base && !before[i].limit &&
			   (!ctrl.raw || ctrl.raw == disabled.raw) &&
			   selected == ARRAY_SIZE(before)) {
			selected = i;
		}
	}
	if (selected == ARRAY_SIZE(before))
		goto unavailable;
	for (unsigned int i = 0; i < ARRAY_SIZE(before); i++)
		if (data_fabric_broadcast_read32(DF_MMIO_BASE(i)) != before[i].base ||
		    data_fabric_broadcast_read32(DF_MMIO_LIMIT(i)) != before[i].limit ||
		    data_fabric_broadcast_read32(DF_MMIO_CONTROL(i)) != before[i].control)
			goto unavailable;

	data_fabric_broadcast_write32(DF_MMIO_BASE(selected), begin >> DF_MMIO_SHIFT);
	data_fabric_broadcast_write32(DF_MMIO_LIMIT(selected), (end >> DF_MMIO_SHIFT) - 1);
	if (data_fabric_broadcast_read32(DF_MMIO_BASE(selected)) != begin >> DF_MMIO_SHIFT ||
	    data_fabric_broadcast_read32(DF_MMIO_LIMIT(selected)) !=
		(end >> DF_MMIO_SHIFT) - 1)
		goto unavailable;
	disabled.re = 1;
	disabled.we = 1;
	data_fabric_broadcast_write32(DF_MMIO_CONTROL(selected), disabled.raw);
	if (data_fabric_broadcast_read32(DF_MMIO_CONTROL(selected)) != disabled.raw)
		goto unavailable;
	high_mmio_ready = true;
	printk(BIOS_NOTICE, "Board setup: high posted MMIO slot%u [%llx,%llx) IOMS0\n",
	       selected, (unsigned long long)begin, (unsigned long long)end);
	return;

unavailable:
	printk(BIOS_ERR, "Board setup: high MMIO unavailable; retain low BAR allocation\n");
}

void __real_allocate_resources(const struct device *root);
void __wrap_allocate_resources(const struct device *root);
void __wrap_allocate_resources(const struct device *root)
{
	/* The ordinary allocator and bridge sizing retain placement ownership.
	 * Only movable, prefetchable 64-bit BARs (including VF BARs) may go high.
	 */
	if (high_mmio_ready) {
		const unsigned long required = IORESOURCE_MEM | IORESOURCE_PCI64 |
			IORESOURCE_PREFETCH;

		for (struct device *dev = all_devices; dev; dev = dev->next) {
			if (!dev->enabled || dev->path.type != DEVICE_PATH_PCI)
				continue;
			for (struct resource *res = dev->resource_list; res; res = res->next)
				if ((res->flags & required) == required &&
				    !(res->flags & (IORESOURCE_FIXED | IORESOURCE_BRIDGE)))
					res->flags |= IORESOURCE_ABOVE_4G;
		}
	}
	__real_allocate_resources(root);
}
