/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/smu.h>
#include <amdblocks/vermeer_smu_telemetry.h>
#include <arch/cpu.h>
#include <cbmem.h>
#include <console/console.h>
#include <cpu/x86/cache.h>
#include <soc/smu.h>
#include <string.h>

#define SMU_TELEMETRY_CBMEM_ID 0x504d5442
#define SMU_TELEMETRY_OWNER_BYTES 0x4000U
#define QUALIFIED_SMU_VERSION 0x00384c00U

static bool attempted;
static bool registered;

bool vermeer_smu_telemetry_prepare(uint32_t smu_version)
{
	const struct cbmem_entry *entry;
	struct smu_payload payload = { .msg = {0} };
	void *owner;

	if (attempted)
		return registered;
	attempted = true;
	if (cpuid_eax(1) != 0x00a20f12 || smu_version != QUALIFIED_SMU_VERSION)
		goto unavailable;
	/* Qualified Vermeer firmware reserves four pages below 4 GiB, clears
	 * them, then registers the low/high address through BIOS message 6.
	 * Keep the full owner, including after an ambiguous reply.
	 */
	if (cbmem_entry_find(SMU_TELEMETRY_CBMEM_ID))
		goto unavailable;
	owner = cbmem_add(SMU_TELEMETRY_CBMEM_ID, SMU_TELEMETRY_OWNER_BYTES);
	entry = cbmem_entry_find(SMU_TELEMETRY_CBMEM_ID);
	if (!owner || !entry || cbmem_entry_start(entry) != owner ||
	    cbmem_entry_size(entry) != SMU_TELEMETRY_OWNER_BYTES ||
	    (uintptr_t)owner > UINT32_MAX - SMU_TELEMETRY_OWNER_BYTES)
		goto unavailable;
	memset(owner, 0, SMU_TELEMETRY_OWNER_BYTES);
	wbinvd();
	payload.msg[0] = (uintptr_t)owner;
	payload.msg[1] = 0;
	if (send_smu_message(SMC_MSG_SET_PMTABLE_ADDRESS, &payload) != CB_SUCCESS)
		goto unavailable;
	registered = true;
	printk(BIOS_NOTICE, "Vermeer SMU telemetry: owner=%08x bytes=%u registered\n",
	       (unsigned int)(uintptr_t)owner, SMU_TELEMETRY_OWNER_BYTES);
	return true;

unavailable:
	printk(BIOS_WARNING, "Vermeer SMU telemetry: optional owner unavailable\n");
	return false;
}
