/* SPDX-License-Identifier: GPL-2.0-only */
#include "native84_profile.h"
#include "native84_parameters.h"
#include <amdblocks/memmap.h>
#include <arch/cpu.h>
#include <bootblock_common.h>
#include <cbmem.h>
#include <console/console.h>
#include <console/uart.h>
#include <delay.h>
#include <imd.h>
#include <program_loading.h>
#include <romstage_common.h>

#if ENV_BOOTBLOCK
asmlinkage void native84_late_entry(const struct native84_handoff *descriptor)
{
	/* Root15 copied exact future-qualified LOAD bytes and zeroed this
	 * bootblock's BSS. The Context2 bridge already owns CS08/data10, IDT,
	 * and the late stack. Never enter reset/pre_c or synthesize a reset EAX.
	 */
	if ((uintptr_t)descriptor != NATIVE84_A)
		native84_terminal();
	native84_stage_admit(descriptor);
	/* The common bootblock_with_basetime entry requires an initialized timer.
	 * Root separately admits genuine family19 P-state MSR/TSC reads; no cache
	 * or timer rate/accuracy claim follows from this function returning.
	 */
	init_timer();
	bootblock_main_with_basetime(0);
	native84_terminal();
}

/* Honest normal C/FLATENTRY alias, never a cold/reset-vector implementation. */
void _start(const struct native84_handoff *)
	__attribute__((alias("native84_late_entry"), regparm(0), noreturn));

void bootblock_soc_early_init(void)
{
	/* Retained FCH/LPC/DRAM contract, no fch_pre_init/AOAC/reset replay. */
	native84_stage_check();
}

void bootblock_mainboard_early_init(void)
{
	/* Preserve inherited AST UART2 routing. console_init's qualified wrapper
	 * checks LPC/NCT/AST and initializes only the selected UART later.
	 */
	native84_stage_check();
}

void bootblock_soc_init(void)
{
	/* No FCH/I2C/power-policy/S3/PSP-transfer or MSR-write reinitialization. */
	native84_stage_check();
}

void bootblock_mainboard_init(void)
{
	native84_stage_check();
	printk(BIOS_INFO, "NATIVE84 GENUINE BOOTBLOCK RETAINED HOOKS\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}
#endif

#if ENV_SEPARATE_ROMSTAGE
static unsigned int creator_attempted;

void __real_car_stage_entry(void);
void __wrap_car_stage_entry(void);
void __wrap_car_stage_entry(void)
{
	native84_stage_admit((const void *)(uintptr_t)NATIVE84_A);
	/* New stage-local timer BSS must be initialized before genuine console. */
	init_timer();
	__real_car_stage_entry();
	native84_terminal();
}

void native84_romstage_main(void)
{
	native84_stage_check();
	if (creator_attempted)
		native84_terminal();
	creator_attempted = 1;
	/* Genuine Glue3/IMD bounded create and its genuine CBFS/FMAP hooks. */
	cbmem_initialize_empty();
	if (!cbmem_online())
		native84_terminal();
	/* Genuine 8B early-DRAM usage record, not the whole-parent resource map. */
	memmap_stash_early_dram_usage();
	printk(BIOS_INFO, "NATIVE84 GENUINE ROMSTAGE BOUNDED CREATE\n");
	run_ramstage();
	native84_terminal();
}
#endif

#if ENV_HAS_CBMEM
uintptr_t cbmem_top_chipset(void)
{
	return native84_descriptor()->cbmem_top;
}

uintptr_t cbmem_bottom_chipset(void)
{
	const struct native84_handoff *d = native84_descriptor();

#if ENV_RAMSTAGE
	/* c_start must still receive top, separately from parameters56. */
	if (_cbmem_top_ptr != d->cbmem_top)
		native84_terminal();
#endif
	return d->cbmem_bottom;
}

struct cbmem_imd_handle_storage cbmem_imd_handle_storage_chipset(void)
{
	const struct native84_handoff *d = native84_descriptor();
	struct cbmem_imd_handle_storage storage;

	/* No CBMEM calls here. The exact immutable epoch/bank is rechecked on
	 * every public operation and nested hook. Each stage gets its own slot.
	 */
	storage.bank_base = d->handle_bank_base;
	storage.bank_bytes = d->handle_bank_bytes;
	storage.handle = (void *)(uintptr_t)(d->handle_bank_base +
		(ENV_RAMSTAGE ? sizeof(struct imd) : 0));
	return storage;
}
#endif

#if ENV_RAMSTAGE
static unsigned int recovery_attempted;
void __real_main(void);
void __wrap_main(void);
void __wrap_main(void)
{
	native84_stage_admit(&native84_handoff_parameters);
	if (_cbmem_top_ptr != native84_handoff_parameters.cbmem_top)
		native84_terminal();
	/* Genuine hardwaremain performs timer/console/post80/exception prefix.
	 * Checked CBMEM recovery returns; retained soc_init is the next terminal.
	 */
	__real_main();
	native84_terminal();
}

int __real_cbmem_initialize(void);
int __wrap_cbmem_initialize(void);
int __wrap_cbmem_initialize(void)
{
	native84_stage_check();
	if (recovery_attempted)
		native84_terminal();
	recovery_attempted = 1;
	if (__real_cbmem_initialize() || !cbmem_online())
		native84_terminal();
	native84_stage_check();
	printk(BIOS_INFO, "NATIVE85 STOCK C_START BOUNDED RECOVER RETURN\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	/* Permit PRE_DEVICE callbacks, then stop in retained soc_init before silicon. */
	return 0;
}
#endif
