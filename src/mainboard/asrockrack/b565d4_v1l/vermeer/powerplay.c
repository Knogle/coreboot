/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/cpu.h>
#include <amdblocks/vermeer_smu_powerplay.h>
#include <console/console.h>
#include <device/device.h>
#include "powerplay.h"
#include "powerplay_contract.h"

static bool attempted;
static bool prepared;

bool b565d4_vermeer_powerplay_prepare(void)
{
	uint32_t apic_ids[B565D4_POWERPLAY_MAX_CPUS];
	uint8_t table[B565D4_POWERPLAY_BYTES];
	const unsigned int expected = get_cpu_count();
	const struct device *cpu;
	uintptr_t owner_address;
	unsigned int count = 0, nodes = 0;

	if (attempted)
		return prepared;
	attempted = true;
	if (!expected || expected > B565D4_POWERPLAY_MAX_CPUS)
		goto unavailable;
	for (cpu = all_devices; cpu; cpu = cpu->next) {
		if (++nodes > 256)
			goto unavailable;
		if (!is_enabled_cpu(cpu))
			continue;
		if (!cpu->initialized || count >= expected)
			goto unavailable;
		apic_ids[count++] = cpu->path.apic.apic_id;
	}
	if (count != expected || !b565d4_vermeer_powerplay_build(apic_ids, count, table))
		goto unavailable;
	if (!vermeer_smu_powerplay_import(table, sizeof(table), &owner_address))
		goto unavailable;
	prepared = true;
	printk(BIOS_NOTICE, "Vermeer PowerPlay: owner=%08x bytes=%u cpus=%u "
	       "original compiled defaults imported\n", (unsigned int)owner_address,
	       B565D4_POWERPLAY_BYTES, count);
	return true;

unavailable:
	printk(BIOS_WARNING, "Vermeer PowerPlay: optional table import unavailable\n");
	return false;
}
