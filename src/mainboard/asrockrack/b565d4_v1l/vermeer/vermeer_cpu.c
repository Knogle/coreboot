/* SPDX-License-Identifier: GPL-2.0-only */

#include <arch/cpu.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <cpu/x86/msr.h>
#include "vermeer_cpu.h"

struct vermeer_vendor_msr {
	uint32_t index;
	uint64_t data;
	uint64_t mask;
};

/* Original B565D4 Zen3 CCX PEI masks selected by its own logical-ID
 * routines for CPUID 00a20f12: SoC 100h/20h and CCX 2h/40h.
 * Platform-policy records and the already initialized APIC are omitted.
 * Preserve the original per-thread, per-core, per-CCX order.
 */
const struct vermeer_vendor_msr vermeer_vendor_registers[] = {
	{ 0xc0010015, 0x0000000008000000ULL, 0x0000000008000000ULL },
	{ 0xc0010015, 0x0000000100000000ULL, 0x0000000100000000ULL },
	{ 0xc0010400, 0x0000000000381000ULL, 0x0000000000381000ULL },
	{ 0xc0010400, 0x0000000000000000ULL, 0x0000000000001004ULL },
	{ 0xc0010402, 0x0000000000000008ULL, 0x0000000000000008ULL },
	{ 0xc0010406, 0x0000000000000040ULL, 0x0000000000000040ULL },
	{ 0xc0011002, 0x0000000000000000ULL, 0x0000000000000810ULL },
	{ 0xc00110dc, 0x0000000000000000ULL, 0x0000000000400000ULL },
	{ 0xc00110de, 0x0000000000000000ULL, 0x0000000000000010ULL },
	{ 0xc0011020, 0x0004400000000000ULL, 0x001c400000000000ULL },
	{ 0xc0011020, 0x0000080000000000ULL, 0x0000080000000000ULL },
	{ 0xc0011021, 0x0018000000000000ULL, 0x0038000000000000ULL },
	{ 0xc0011021, 0x0000000000000080ULL, 0x0006000000000180ULL },
	{ 0xc0011029, 0x0000000310000000ULL, 0x0000000310000000ULL },
	{ 0xc0011029, 0x0001000000000000ULL, 0x0001000000000000ULL },
	{ 0xc001102a, 0x0000000000018080ULL, 0x0000000000018080ULL },
	{ 0xc001102c, 0x0400000000000000ULL, 0x0400000000000000ULL },
	{ 0xc001102d, 0x0000001800000000ULL, 0x0218801800000000ULL },
	{ 0xc001102e, 0x0000000000000000ULL, 0x000000000000001cULL },
	{ 0xc001102e, 0x0000000000000060ULL, 0x0000000000004062ULL },
	{ 0xc00110e2, 0x0002000000080018ULL, 0x0002000100080018ULL },
	{ 0xc00110e5, 0x5300000040002000ULL, 0x5300000040002000ULL },
	{ 0xc00110e5, 0x0000000004002100ULL, 0x4000040004002100ULL },
	{ 0xc00110e7, 0x0000000000000088ULL, 0x0000000000000088ULL },
	{ 0xc00110e7, 0x0000000000000000ULL, 0x0000000000000008ULL },
	{ 0xc00110e8, 0x0000000000000ff0ULL, 0x0000000000000ff0ULL },
	{ 0xc00110e9, 0x0000000001000000ULL, 0x0000000001000000ULL },
	{ 0xc00110ea, 0x0000000000400000ULL, 0x0000000000c000f0ULL },
	{ 0xc0011093, 0x0000000000501680ULL, 0x0000000000dc1f80ULL },
	{ 0xc0011097, 0x0000000000001106ULL, 0x00000000000011c6ULL },
};

_Static_assert(sizeof(struct vermeer_vendor_msr) == 20,
	"IA32 Vermeer MSR record layout");

void b565d4_vermeer_init_msrs(void)
{
	const struct cpuid_result topology = cpuid(0x8000001e);
	const uint32_t id = cpuid(1).ebx >> 24;
	const uint32_t threads = ((topology.ebx >> 8) & 0xff) + 1;
	unsigned int selected = 0, changed = 0, exact = 0;

	if (cpuid_eax(1) != 0x00a20f12 || cpuid_eax(0x80000001) != 0x00a20f12)
		return;

	for (size_t row = 0; row < ARRAY_SIZE(vermeer_vendor_registers); row++) {
		const struct vermeer_vendor_msr *r = &vermeer_vendor_registers[row];
		msr_t before, requested, after;

		if ((row >= 9 && row < 28 && id % threads) ||
		    (row >= 28 && (id & (threads > 1 ? 15U : 7U))))
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
	       "Vermeer CPU%u MSR init: threads=%u selected=%u changed=%u "
	       "exact=%u patch=%08x MCG=%016llx\n",
	       id, threads, selected, changed, exact, rdmsr(0x8b).lo, rdmsr(0x17a).raw);
}
