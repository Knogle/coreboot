/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/cpu.h>
#include <amdblocks/smu.h>
#include <cbmem.h>
#include <console/console.h>
#include <cpu/x86/cache.h>
#include <device/device.h>
#include <soc/smu.h>
#include <string.h>
#include "powerplay.h"
#include "powerplay_contract.h"

#define POWERPLAY_CBMEM_ID 0x50505442
#define POWERPLAY_OWNER_BYTES 0x1000U

static bool attempted;
static bool prepared;

bool b565d4_vermeer_powerplay_prepare(void)
{
	uint32_t apic_ids[B565D4_POWERPLAY_MAX_CPUS];
	uint8_t table[B565D4_POWERPLAY_BYTES];
	const unsigned int expected = get_cpu_count();
	const struct cbmem_entry *entry;
	const struct device *cpu;
	unsigned int count = 0, nodes = 0;
	struct smu_payload payload = { .msg = {0} };
	void *owner;

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
	/* cbmem_add may otherwise reuse an existing ID without checking its size.
	 * Keep this exact fresh owner reserved even after an ambiguous SMU reply.
	 */
	if (cbmem_entry_find(POWERPLAY_CBMEM_ID))
		goto unavailable;
	owner = cbmem_add(POWERPLAY_CBMEM_ID, POWERPLAY_OWNER_BYTES);
	entry = cbmem_entry_find(POWERPLAY_CBMEM_ID);
	if (!owner || !entry || cbmem_entry_start(entry) != owner ||
	    cbmem_entry_size(entry) != POWERPLAY_OWNER_BYTES ||
	    (uintptr_t)owner > UINT32_MAX - POWERPLAY_OWNER_BYTES)
		goto unavailable;
	memset(owner, 0, POWERPLAY_OWNER_BYTES);
	memcpy(owner, table, sizeof(table));
	/* Only the BSP writes this fresh private staging interval. The original
	 * flush occurs between registering its address and requesting the import.
	 * No other CPU caches or uses it, and no OS allocation can reuse it.
	 */
	payload.msg[0] = (uintptr_t)owner;
	payload.msg[1] = 0;
	if (send_smu_message(SMC_MSG_SET_POWERPLAY_ADDRESS, &payload) != CB_SUCCESS)
		goto unavailable;
	wbinvd();
	payload = (struct smu_payload) { .msg = {B565D4_POWERPLAY_BYTES} };
	if (send_smu_message(SMC_MSG_IMPORT_POWERPLAY, &payload) != CB_SUCCESS)
		goto unavailable;
	prepared = true;
	printk(BIOS_NOTICE, "Vermeer PowerPlay: owner=%08x bytes=%u cpus=%u "
	       "original compiled defaults imported\n", (unsigned int)(uintptr_t)owner,
	       B565D4_POWERPLAY_BYTES, count);
	return true;

unavailable:
	printk(BIOS_WARNING, "Vermeer PowerPlay: optional table import unavailable\n");
	return false;
}
