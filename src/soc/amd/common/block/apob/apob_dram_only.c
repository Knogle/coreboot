/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <acpi/acpi.h>
#include <amdblocks/apob_cache.h>
#include <amdblocks/b565d4_fspm_firstcall.h>
#include <types.h>

/* APOB_BASE_HEADER and cold signature/dataSize contract factored from the
 * stock apob_cache.c getter. No NV region, thread, hash write or cache read. */
struct apob_base_header {
	uint32_t signature;
	uint32_t version;
	uint32_t size;
	uint32_t offset_of_first_entry;
};
_Static_assert(sizeof(struct apob_base_header) == 16, "APOB header");
_Static_assert(CONFIG_PSP_APOB_DRAM_ADDRESS == 0x0a200000, "Bounded cold APOB base");
_Static_assert(CONFIG_PSP_APOB_DRAM_SIZE == 0x10000, "Original APOB cache policy");
_Static_assert(CONFIG(SOC_AMD_COMMON_BLOCK_APOB_NV_DISABLE), "No NV machinery");

void *soc_fill_apob_cache(void)
{
	const uintptr_t address = CONFIG_PSP_APOB_DRAM_ADDRESS;
	const uint32_t hash_bytes = CONFIG(SOC_AMD_COMMON_BLOCK_APOB_HASH) ? 8 : 0;
	if (!b565d4_fspm_cold_apob_read_admitted(address, sizeof(struct apob_base_header)) ||
	    acpi_is_wakeup_s3())
		return NULL;
	const volatile struct apob_base_header *h = (const void *)address;
	uint32_t signature = h->signature;
	uint32_t size = h->size;
	/* Preserve stock accepted size+hash policy without unsigned wraparound. */
	if (signature != 0x424f5041 || !size ||
	    size > CONFIG_PSP_APOB_DRAM_SIZE - hash_bytes)
		return NULL;
	/* The supplier consumes an object, not just its header. A future receipt
	 * must admit its exact validated extent before the pointer can escape. */
	if (!b565d4_fspm_cold_apob_read_admitted(address, size))
		return NULL;
	if (h->signature != signature || h->size != size)
		return NULL;
	return (void *)address;
}
