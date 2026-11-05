/* SPDX-License-Identifier: GPL-2.0-only */
/* Future separately selected BSP-only fallback; not integrated or built. */
#include <commonlib/bsd/compiler.h>
#include "native84_profile.h"
#include "legacy_shadow_fallback.h"
#include <arch/cpu.h>
#include <console/console.h>
#include <console/uart.h>
#include <cpu/amd/mtrr.h>
#include <cpu/x86/cache.h>
#include <cpu/x86/cr.h>
#include <cpu/x86/msr.h>
#include <cpu/x86/mtrr.h>

static bool same_msr(msr_t a, msr_t b)
{
	return a.lo == b.lo && a.hi == b.hi;
}

static void stop(const char *reason) __noreturn;
static void stop(const char *reason)
{
	printk(BIOS_ERR, "NATIVE91 SHADOW REFUSAL %s\n", reason);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	native84_terminal();
}

/* This operation WRMSRs SYSCFG visibility only. It is NOT read-only. Fixed
 * attributes and every effective cache/routing control remain unchanged.
 */
void native91_log_visible_fixed_mtrrs(void)
{
	static const unsigned int indices[] = {
		0x250, 0x258, 0x259, 0x268, 0x269, 0x26a,
		0x26b, 0x26c, 0x26d, 0x26e, 0x26f,
	};
	msr_t values[11], saved, exposed;
	bool visible;

	native84_stage_check(); /* Existing selected IF=0, paging-off BSP context. */
	saved = rdmsr(SYSCFG_MSR);
	exposed = saved;
	exposed.lo |= SYSCFG_MSR_MtrrFixDramModEn;
	wrmsr(SYSCFG_MSR, exposed);
	visible = same_msr(rdmsr(SYSCFG_MSR), exposed);
	if (visible) {
		for (unsigned int i = 0; i < 11; i++)
			values[i] = rdmsr(indices[i]);
	}
	wrmsr(SYSCFG_MSR, saved);
	if (!same_msr(rdmsr(SYSCFG_MSR), saved))
		stop("SYSCFG visibility restore");
	if (!visible)
		stop("SYSCFG visibility unavailable");
	printk(BIOS_INFO, "NATIVE91 SYSCFG RESTORED %08x:%08x\n", saved.hi, saved.lo);
	for (unsigned int i = 0; i < 11; i++)
		printk(BIOS_INFO, "NATIVE91 VISIBLE FIXED %03x=%08x:%08x\n",
			indices[i], values[i].hi, values[i].lo);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

/* AMD APM Table7-13: reject reserved incoming encodings rather than copying
 * an unknown type. Preserve UC/WC/WT/WB when legal for read+write DRAM. The
 * separately selected fallback converts a legal WP shadow encoding to UC.
 */
static int writable_dram_byte(uint8_t old)
{
	unsigned int type = old & 7, route = old & 0x18;
	bool legal;

	if (old & 0xe0)
		return -1;
	if (route == 0)
		legal = type == 0 || type == 1 || type == 4 || type == 5;
	else if (route == 8)
		legal = type == 0 || type == 1;
	else if (route == 0x10)
		legal = type == 0 || type == 5;
	else
		legal = type == 0 || type == 1 || type == 4 || type == 6;
	if (!legal)
		return -1;
	return type == 5 ? 0x18 : type | 0x18;
}

/* Caller separately selects takeover of [dc000,100000), not just unaligned
 * payload bytes. Owned code/stack/tables remain outside it. No AP/SMM/DMAs are
 * stopped here; this is not an all-CPU cache-consistency claim or cold init.
 * No automatic call/retry from the Source22 payload mismatch path is implied.
 */
void native91_repair_legacy_shadow(void)
{
	msr_t saved_syscfg, exposed, saved_def, before[5], after[5];
	uint32_t saved_cr0;
	bool planned = true, changed = false, committed = true;

	_Static_assert(sizeof(msr_t) == 8, "IA32 low/high MSR layout");
	native84_stage_check();
	saved_cr0 = read_cr0();
	saved_syscfg = rdmsr(SYSCFG_MSR);
	saved_def = rdmsr(MTRR_DEF_TYPE_MSR);
	/* Keep all existing global enables as found, never create a new global
 * MTRR configuration or paging/TLB regime as part of this local repair.
 */
	if ((saved_cr0 & CR0_PG) ||
	    !(saved_syscfg.lo & SYSCFG_MSR_MtrrFixDramEn) ||
	    (saved_def.lo & (MTRR_DEF_TYPE_EN | MTRR_DEF_TYPE_FIX_EN)) !=
		(MTRR_DEF_TYPE_EN | MTRR_DEF_TYPE_FIX_EN))
		stop("existing fixed routing/enables/paging");
	exposed = saved_syscfg;
	exposed.lo |= SYSCFG_MSR_MtrrFixDramModEn;
	wrmsr(SYSCFG_MSR, exposed);
	if (!same_msr(rdmsr(SYSCFG_MSR), exposed)) {
		wrmsr(SYSCFG_MSR, saved_syscfg);
		if (!same_msr(rdmsr(SYSCFG_MSR), saved_syscfg))
			stop("planning visibility restore");
		stop("planning visibility unavailable");
	}
	for (unsigned int i = 0; i < 5; i++) {
		uint8_t *planned_bytes = (void *)&after[i];

		before[i] = rdmsr(0x26b + i);
		after[i] = before[i];
		for (unsigned int j = i == 0 ? 4 : 0; j < 8; j++) {
			int next = writable_dram_byte(planned_bytes[j]);

			if (next < 0)
				planned = false;
			else
				planned_bytes[j] = next;
		}
		changed |= !same_msr(before[i], after[i]);
	}
	if (!planned || !changed) {
		wrmsr(SYSCFG_MSR, saved_syscfg);
		if (!same_msr(rdmsr(SYSCFG_MSR), saved_syscfg))
			stop("planning restore");
		if (!planned)
			stop("reserved fixed encoding");
		printk(BIOS_INFO, "NATIVE91 SHADOW ALREADY READ/WRITE DRAM\n");
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		return;
	}
	/* Genuine coreboot commit_fixed_mtrrs keeps DEF_TYPE/global routing
 * enables and updates fixed registers while caching is disabled. Here
 * WBINVD is unconditional, including self-snooping CPUs, and exact CR0 is
 * restored rather than enable_cache() clearing arbitrary saved flags.
 * Paging is off, so no active translation/TLB mapping is changed.
 */
	write_cr0((saved_cr0 | CR0_CD) & ~CR0_NW);
	wbinvd();
	for (unsigned int i = 0; i < 5; i++) {
		if (!same_msr(before[i], after[i]))
			wrmsr(0x26b + i, after[i]);
	}
	for (unsigned int i = 0; i < 5; i++)
		committed &= same_msr(rdmsr(0x26b + i), after[i]);
	wbinvd();
	wrmsr(SYSCFG_MSR, saved_syscfg);
	write_cr0(saved_cr0);
	(void)cpuid(0);
	if (!same_msr(rdmsr(SYSCFG_MSR), saved_syscfg) ||
	    !same_msr(rdmsr(MTRR_DEF_TYPE_MSR), saved_def) || read_cr0() != saved_cr0)
		stop("control restore");
	for (unsigned int i = 0; i < 5; i++)
		printk(BIOS_INFO, "NATIVE91 SHADOW %03x %08x:%08x -> %08x:%08x\n",
			0x26b + i, before[i].hi, before[i].lo, after[i].hi, after[i].lo);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (!committed)
		stop("partial fixed write/readback; no retry/rollback");
}
