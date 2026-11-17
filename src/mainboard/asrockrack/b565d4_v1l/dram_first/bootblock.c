/* SPDX-License-Identifier: GPL-2.0-only */
#include <arch/cpu.h>
#include <arch/exception.h>
#include <bootblock_common.h>
#include <console/console.h>
#include <console/uart.h>
#include <cpu/amd/microcode.h>
#include <cpu/amd/msr.h>
#include <cpu/x86/lapic_def.h>
#include <cpu/x86/msr.h>
#include <delay.h>
#include <halt.h>
#include <stdbool.h>
#include <stdint.h>
#include "../normal_uart2.h"

uint32_t b565d4_normal_reset_eax;

#if CONFIG(B565D4_MATISSE)
static void matisse_early_microcode(void)
{
	msr_t before, after, apic;
	uint32_t target;
	bool update;

	/* This first timing comparison covers the physically installed B0.
	 * Other admitted Matisse revisions retain their existing CPU path.
	 */
	if (cpuid_eax(1) != 0x00870f10)
		return;

	before = rdmsr(IA32_BIOS_SIGN_ID);
	apic = rdmsr(LAPIC_BASE_MSR);
	amd_load_microcode_from_cbfs();
	target = amd_microcode_patch_id();
	update = target && before.lo < target;
	if (update)
		amd_apply_microcode_patch();
	after = rdmsr(IA32_BIOS_SIGN_ID);
	printk(BIOS_NOTICE, "Matisse early microcode: before=%08x:%08x "
	       "after=%08x:%08x selected=%08x apply_requested=%u APIC=%08x:%08x\n",
	       before.hi, before.lo, after.hi, after.lo, target, update, apic.hi, apic.lo);
	amd_free_microcode();
}
#endif

void bootblock_mainboard_early_init(void)
{
	if (!b565d4_uart2_setup()) {
		post_code(0xe7);
		halt();
	}
	init_timer();
}

void bootblock_mainboard_init(void)
{
#if CONFIG(B565D4_MATISSE)
	/* The normal Matisse path establishes its CCX registers before the
	 * ordinary per-CPU microcode update in amd_cpu_init().
	 */
	if (CONFIG(B565D4_MATISSE_IRQ_DIAGNOSTIC) &&
	    !CONFIG(B565D4_MATISSE_SKIP_EARLY_MICROCODE))
		matisse_early_microcode();
#endif
	printk(BIOS_INFO, "B565D4 native bootblock complete ResetEAX=%08x\n",
	       b565d4_normal_reset_eax);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}
