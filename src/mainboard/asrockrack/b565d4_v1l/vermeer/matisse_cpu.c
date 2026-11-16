/* SPDX-License-Identifier: GPL-2.0-only */

#include <arch/cpu.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <cpu/amd/msr.h>
#include <cpu/x86/cr.h>
#include <cpu/x86/mp.h>
#include <cpu/x86/msr.h>
#include "matisse_cpu.h"

/* B565D4 Matisse B0 CCX masks, qualified against the original board PEI
 * and the installed 3700X. Preserve all bits outside the selected masks.
 * The 10DC[14] policy follows this board's observed original default.
 */
const struct matisse_vendor_msr matisse_vendor_registers[MATISSE_MSR_COUNT] = {
	{ 0xc0010015, 0x0000000108000000ULL, 0x0000000108000000ULL },
	{ 0xc00110dc, 0x0000000000008000ULL, 0x0000000000028000ULL },
	{ 0xc00110dc, 0x0000000000000000ULL, 0x0000000000000100ULL },
	{ 0xc00110dc, 0x0000000000000000ULL, 0x0000000000004000ULL },
	{ 0xc0010400, 0x0000000000000600ULL, 0x0000000000000600ULL },
	{ 0xc0010401, 0x0000000000002c00ULL, 0x0000000000002c00ULL },
	{ 0xc0010402, 0x0000000000000008ULL, 0x0000000000000008ULL },
	{ 0xc0010406, 0x0000000000000040ULL, 0x0000000000000040ULL },
	{ 0xc0011020, 0x0000404000000000ULL, 0x0001c0c000000000ULL },
	{ 0xc0011022, 0x0000000002000000ULL, 0x0000000002000000ULL },
	{ 0xc0011023, 0x0000000000000020ULL, 0x0000000000000020ULL },
	{ 0xc0011028, 0x0000000000000014ULL, 0x0000000000000014ULL },
	{ 0xc0011029, 0x0003000310000000ULL, 0x04037e0310000000ULL },
	{ 0xc0011029, 0x0000000000000000ULL, 0x0000001000000000ULL },
	{ 0xc001102a, 0x0000000000018080ULL, 0x0000000000018080ULL },
	{ 0xc001102c, 0x0308000000000000ULL, 0x0308000000000000ULL },
	{ 0xc001102d, 0x0000000400000000ULL, 0x0000000400000000ULL },
	{ 0xc001102d, 0x0000000000000010ULL, 0x0000000000000010ULL },
	{ 0xc001102e, 0x0002024000000000ULL, 0x0002024000000000ULL },
	{ 0xc001102e, 0x0010000000000000ULL, 0x0010000000000000ULL },
	{ 0xc00110e2, 0x00002afa00082018ULL, 0x0000fefa00082018ULL },
	{ 0xc00110e2, 0x0000000100000000ULL, 0x0000000100000000ULL },
	{ 0xc00110e3, 0x0000000000000001ULL, 0x0000000000000001ULL },
	{ 0xc0011074, 0x0000000000000000ULL, 0x1000000000000000ULL },
	{ 0xc0011093, 0x0000000000715870ULL, 0x0000000000ff5c71ULL },
	{ 0xc0011097, 0x0000000000005dbeULL, 0x0000000000005ffeULL },
};


_Static_assert(sizeof(struct matisse_vendor_msr) == 20,
	"IA32 Matisse MSR record layout");

void b565d4_matisse_init_msrs(void)
{
	const struct cpuid_result topology = cpuid(0x8000001e);
	const uint32_t id = cpuid(1).ebx >> 24;
	const uint32_t threads = ((topology.ebx >> 8) & 0xff) + 1;
	unsigned int selected = 0, changed = 0, exact = 0;

	/* These stepping-specific masks are qualified for Matisse B0.
	 * Other admitted revisions retain their architectural CPU path.
	 */
	if (cpuid_eax(1) != 0x00870f10 || cpuid_eax(0x80000001) != 0x00870f10)
		return;

	/* Original selectors: every thread, primary thread of each core,
	 * then the primary thread of each CCX. Do not assume a core count
	 * or require SMT to be enabled.
	 */
	for (size_t row = 0; row < ARRAY_SIZE(matisse_vendor_registers); row++) {
		const struct matisse_vendor_msr *r = &matisse_vendor_registers[row];
		msr_t before, requested, after;

		if ((row >= MATISSE_THREAD_MSR_END && row < MATISSE_CORE_MSR_END &&
		     id % threads) ||
		    (row >= MATISSE_CORE_MSR_END && (id & (threads > 1 ? 7U : 3U))))
			continue;
		before = rdmsr(r->index);
		requested.raw = (before.raw & ~r->mask) | r->data;
		if (requested.raw != before.raw) {
			wrmsr(r->index, requested);
			changed++;
		}
		after = rdmsr(r->index);
		selected++;
		exact += (after.raw & r->mask) == r->data;
	}
	printk(exact == selected ? BIOS_DEBUG : BIOS_ERR,
	       "Matisse CPU%u MSR init: threads=%u selected=%u changed=%u "
	       "exact=%u patch=%08x MCG=%016llx\n",
	       id, threads, selected, changed, exact, rdmsr(0x8b).lo, rdmsr(0x17a).raw);
}

static void desktop_enable_smt_cache(void *unused)
{
	msr_t before = rdmsr(BU_CFG_MSR);
	msr_t requested = before;
	msr_t after;

	(void)unused;
	/* The original Matisse and Vermeer CCX PEI set TwCfgCombineCr0Cd
	 * after initializing all threads. Do not combine the siblings' cache
	 * controls while an AP still has its reset-time CR0.CD set.
	 */
	requested.raw |= COMBINE_CR0_CD;
	if (requested.raw != before.raw)
		wrmsr(BU_CFG_MSR, requested);
	after = rdmsr(BU_CFG_MSR);
	printk(after.raw == requested.raw ? BIOS_DEBUG : BIOS_ERR,
	       "%s CPU%u post-MP TW_CFG: before=%016llx after=%016llx "
	       "exact=%u CR0=%08x MCG=%016llx\n",
	       CONFIG(B565D4_MATISSE) ? "Matisse" : "Vermeer",
	       cpuid(1).ebx >> 24, before.raw, after.raw, after.raw == requested.raw,
	       (uint32_t)read_cr0(), rdmsr(0x17a).raw);
}

enum cb_err b565d4_desktop_finalize_mp(void)
{
	const uint32_t signature = cpuid_eax(1);

	if (!((CONFIG(B565D4_MATISSE) && signature == 0x00870f10) ||
	      (CONFIG(B565D4_VERMEER) && signature == 0x00a20f12)))
		return CB_SUCCESS;
	return mp_run_on_all_cpus_synchronously(desktop_enable_smt_cache, NULL);
}
