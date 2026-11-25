/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/smu.h>
#include <amdblocks/vermeer_smu_powerplay.h>
#include <cbmem.h>
#include <cpu/x86/cache.h>
#include <soc/smu.h>
#include <stdint.h>
#include <string.h>

#define POWERPLAY_CBMEM_ID 0x50505442
#define POWERPLAY_OWNER_BYTES 0x1000U

static bool attempted;
static bool prepared;
static uintptr_t registered_owner;

bool vermeer_smu_powerplay_import(const void *table, size_t table_bytes,
				  uintptr_t *owner_address)
{
	const struct cbmem_entry *entry;
	struct smu_payload payload = { .msg = {0} };
	void *owner;

	if (!owner_address)
		return false;
	if (attempted) {
		*owner_address = registered_owner;
		return prepared;
	}
	attempted = true;
	if (!table || !table_bytes || table_bytes > POWERPLAY_OWNER_BYTES)
		return false;
	/* cbmem_add may otherwise reuse an existing ID without checking its size.
	 * Keep this fresh owner reserved even after an ambiguous SMU reply.
	 */
	if (cbmem_entry_find(POWERPLAY_CBMEM_ID))
		return false;
	owner = cbmem_add(POWERPLAY_CBMEM_ID, POWERPLAY_OWNER_BYTES);
	entry = cbmem_entry_find(POWERPLAY_CBMEM_ID);
	if (!owner || !entry || cbmem_entry_start(entry) != owner ||
	    cbmem_entry_size(entry) != POWERPLAY_OWNER_BYTES ||
	    (uintptr_t)owner > UINT32_MAX - POWERPLAY_OWNER_BYTES)
		return false;
	memset(owner, 0, POWERPLAY_OWNER_BYTES);
	memcpy(owner, table, table_bytes);
	/* Only the BSP writes this private staging interval. Flush after
	 * registering its address and before requesting the import.
	 */
	payload.msg[0] = (uintptr_t)owner;
	payload.msg[1] = 0;
	if (send_smu_message(SMC_MSG_SET_POWERPLAY_ADDRESS, &payload) != CB_SUCCESS)
		return false;
	wbinvd();
	payload = (struct smu_payload) { .msg = {(uint32_t)table_bytes} };
	if (send_smu_message(SMC_MSG_IMPORT_POWERPLAY, &payload) != CB_SUCCESS)
		return false;
	registered_owner = (uintptr_t)owner;
	*owner_address = registered_owner;
	prepared = true;
	return true;
}
