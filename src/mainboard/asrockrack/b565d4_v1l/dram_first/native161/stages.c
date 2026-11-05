/* SPDX-License-Identifier: GPL-2.0-only */
#include "native84_profile.h"
#include <amdblocks/memmap.h>
#include <cbmem.h>
#include <console/console.h>
#include <console/uart.h>
#include <imd.h>
#include <program_loading.h>
#include <rmodule.h>

_Static_assert(ENV_SEPARATE_ROMSTAGE && CONFIG(SEPARATE_ROMSTAGE) &&
	CONFIG(ARCH_ROMSTAGE_X86_32), "Separate genuine IA32 ROMstage only");
_Static_assert(CONFIG_EARLYRAM_BSP_STACK_SIZE == 0x4000,
	"Separately qualified ordinary16KiB EARLYRAM stack");
_Static_assert(CONFIG(SOC_AMD_CEZANNE_NATIVE) &&
	CONFIG(CBMEM_BOUNDED_ARENA) && CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE) &&
	CONFIG(CBMEM_BOUNDED_SKIP_RAM_CHECK), "Existing admitted bounded CBMEM path");
_Static_assert(CONFIG(NO_STAGE_CACHE) && CONFIG(RELOCATABLE_MODULES) &&
	!0 && !CONFIG(HAVE_ACPI_RESUME) &&
	!CONFIG(VBOOT) && !CONFIG(TPM) && !CONFIG(COOP_MULTITASKING),
	"Native non-FSP, nonresume bounded RAMstage trial");

static unsigned int creator_attempted;

uintptr_t __wrap_cbmem_top_chipset(void);
int __wrap_rmodule_stage_load(struct rmod_stage_load *rsl);

uintptr_t __wrap_cbmem_top_chipset(void)
{
	return native84_descriptor()->cbmem_top;
}

uintptr_t cbmem_bottom_chipset(void)
{
	return native84_descriptor()->cbmem_bottom;
}

struct cbmem_imd_handle_storage cbmem_imd_handle_storage_chipset(void)
{
	const struct native84_handoff *d = native84_descriptor();
	struct cbmem_imd_handle_storage storage;

	/* First-ROM creator slot; immutable RAMstage still recovers with slot+16. */
	storage.bank_base = d->handle_bank_base;
	storage.bank_bytes = d->handle_bank_bytes;
	storage.handle = (void *)(uintptr_t)d->handle_bank_base;
	return storage;
}

int __wrap_rmodule_stage_load(struct rmod_stage_load *rsl)
{
	return native84_rmodule_stage_load(rsl);
}

void native84_romstage_main(void)
{
	native84_stage_check();
	if (creator_attempted)
		native84_terminal();
	creator_attempted = 1;
	cbmem_initialize_empty();
	if (!cbmem_online())
		native84_terminal();
	memmap_stash_early_dram_usage();
	printk(BIOS_INFO, "NATIVE161 SINGLE ROMSTAGE BOUNDED CREATE; RUN_RAMSTAGE\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	run_ramstage();
	native84_terminal();
}
