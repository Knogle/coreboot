/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/cpu.h>
#include <amdblocks/vermeer_smu_powerplay.h>
#include <commonlib/endian.h>
#include <console/console.h>
#include <device/device.h>
#include <string.h>
#include "powerplay.h"

#define POWERPLAY_BYTES 0x304U
#define POWERPLAY_MAX_CPUS 16U
#define POWERPLAY_APIC_OFFSET 0x80U
#define POWERPLAY_APIC_SLOTS 256U

static bool attempted;
static bool prepared;

static bool build_powerplay_table(const uint32_t *apic_ids, unsigned int count,
				  uint8_t out[POWERPLAY_BYTES])
{
	uint8_t table[POWERPLAY_BYTES] = {0};

	if (!apic_ids || !out || !count || count > POWERPLAY_MAX_CPUS)
		return false;
	for (unsigned int i = 0; i < count; i++) {
		if (apic_ids[i] >= UINT16_MAX)
			return false;
		for (unsigned int j = 0; j < i; j++)
			if (apic_ids[i] == apic_ids[j])
				return false;
	}
	/* Original SmuV11DxeVMR 38BCh with the board's compiled scalar PCD
	 * defaults. Zero retains stock automatic PPT/TDC/EDC, voltage and clock
	 * overrides. The nonzero calibration fields are not generic AM4 values.
	 */
	write_le32(table + 0x14, 1000); /* TDP_PlatformLimit, PCD180h */
	write_le32(table + 0x54, 140); /* CPU full-scale current, PCD0D7h */
	write_le32(table + 0x5c, 75); /* SoC full-scale current, PCD0D8h */
	write_le16(table + 0x6d, 0x0101); /* Original unconditional store3A76h */
	write_le16(table + 0x7c, count);
	memset(table + POWERPLAY_APIC_OFFSET, 0xff, 2 * POWERPLAY_APIC_SLOTS);
	for (unsigned int i = 0; i < count; i++) {
		unsigned int rank = 0;

		for (unsigned int j = 0; j < count; j++)
			rank += apic_ids[j] < apic_ids[i];
		write_le16(table + POWERPLAY_APIC_OFFSET + 2 * rank, apic_ids[i]);
	}
	memcpy(out, table, sizeof(table));
	return true;
}

bool b565d4_vermeer_powerplay_prepare(void)
{
	uint32_t apic_ids[POWERPLAY_MAX_CPUS];
	uint8_t table[POWERPLAY_BYTES];
	const unsigned int expected = get_cpu_count();
	const struct device *cpu;
	uintptr_t owner_address;
	unsigned int count = 0, nodes = 0;

	if (attempted)
		return prepared;
	attempted = true;
	if (!expected || expected > POWERPLAY_MAX_CPUS)
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
	if (count != expected || !build_powerplay_table(apic_ids, count, table))
		goto unavailable;
	if (!vermeer_smu_powerplay_import(table, sizeof(table), &owner_address))
		goto unavailable;
	prepared = true;
	printk(BIOS_NOTICE, "Vermeer PowerPlay: owner=%08x bytes=%u cpus=%u "
	       "original compiled defaults imported\n", (unsigned int)owner_address,
	       POWERPLAY_BYTES, count);
	return true;

unavailable:
	printk(BIOS_WARNING, "Vermeer PowerPlay: optional table import unavailable\n");
	return false;
}
