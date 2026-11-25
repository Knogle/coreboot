/* SPDX-License-Identifier: GPL-2.0-only */

#include "platform.h"
#include "native_smm_memory.h"
#include "matisse_irq_probe.h"
#include "matisse_cpu.h"
#include "setup_options.h"
#include "vermeer_cpu.h"
#include <amdblocks/cpu.h>
#include <amdblocks/vermeer_cpu_name.h>
#include <amdblocks/vermeer_microcode.h>
#include <amdblocks/vermeer_smu_cc6.h>
#include <arch/cpu.h>
#include <console/console.h>
#include <device/device.h>
#include <device/resource.h>
#include <halt.h>

static bool overlaps_ram(uint64_t begin, uint64_t end)
{
	/* Resources were assigned before CPU initialization. Every supplier,
	 * firmware and BSP SMM exclusion is already reserved. A different AP
	 * SMM footprint must not invalidate that map silently.
	 */
	for (struct device *dev = all_devices; dev; dev = dev->next) {
		for (const struct resource *res = dev->resource_list; res; res = res->next) {
			if (!(res->flags & IORESOURCE_MEM) ||
			    !(res->flags & IORESOURCE_CACHEABLE) ||
			    (res->flags & IORESOURCE_RESERVE) || !res->size)
				continue;
			if (res->base < end && begin < res->base + res->size)
				return true;
		}
	}
	return false;
}

void b565d4_desktop_cpu_init(struct device *dev)
{
	struct b565d4_smm_state state;
	struct b565d4_smm_regions ranges;

	if (CONFIG(B565D4_MATISSE_ALL_CPU_MSR_TRIAL))
		b565d4_matisse_vendor_msr_cpu_trial();
	else if (CONFIG(B565D4_MATISSE) && !CONFIG(B565D4_MATISSE_IRQ_DIAGNOSTIC))
		b565d4_matisse_init_msrs();
	else if (CONFIG(B565D4_VERMEER)) {
		b565d4_vermeer_init_msrs();
		vermeer_cpu_name_apply();
		if (CONFIG(B565D4_VERMEER_MICROCODE_ADDRESS))
			vermeer_microcode_address_prepare();
	}
	amd_cpu_init(dev);
	if (CONFIG(B565D4_VERMEER_CORE_C6) && b565d4_setup_core_c6_enabled())
		vermeer_smu_cc6_cpu_prepare(B565D4_VERMEER_CSTATE_IO_BASE);
	if (!b565d4_read_smm_state(&state) ||
	    !b565d4_decode_smm_regions(state.smbase, state.smm_addr, state.smm_mask,
				      state.hwcr, cpuid_eax(0x80000008) & 0xff, &ranges) ||
	    overlaps_ram(ranges.smbase_begin, ranges.smbase_end) ||
	    ((ranges.flags & B565D4_SMM_TSEG_VALID) &&
	     overlaps_ram(ranges.tseg_begin, ranges.tseg_end)))
		die(B565D4_DESKTOP_NAME " per-CPU SMM state conflicts with published RAM");
	printk(BIOS_DEBUG, B565D4_DESKTOP_NAME " CPU%lu SMM exclusions validated, flags=%x\n",
	       cpu_index(), ranges.flags);
}
