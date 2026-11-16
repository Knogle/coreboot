/* SPDX-License-Identifier: GPL-2.0-only */
#include <arch/cpu.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <console/uart.h>
#include <cpu/x86/lapic.h>
#include <cpu/x86/lapic_def.h>
#include <cpu/x86/msr.h>
#include <halt.h>
#include <stdint.h>
#include <string.h>
#include <types.h>
#include "native84_context.h"
#include "native84_profile.h"
#include "matisse_irq_probe.h"
#include "matisse_cpu.h"

struct matisse_idtr {
	uint16_t limit;
	uint32_t base;
} __packed;

struct matisse_gate {
	uint16_t low;
	uint16_t selector;
	uint8_t zero;
	uint8_t type;
	uint16_t high;
} __packed;

_Static_assert(sizeof(struct matisse_gate) == 8 && sizeof(struct matisse_idtr) == 6,
	"IA32 interrupt descriptor layout");

volatile uint32_t b565d4_matisse_irq_count[4];
static struct matisse_gate probe_idt[256] __aligned(16);
void b565d4_matisse_irq_handler0(void);
void b565d4_matisse_irq_handler1(void);
void b565d4_matisse_irq_handler2(void);
void b565d4_matisse_irq_handler3(void);

void b565d4_matisse_vendor_msr_cpu_trial(void)
{
	const struct cpuid_result topology = cpuid(0x8000001e);
	const uint32_t initial_id = cpuid(1).ebx >> 24;
	const uint32_t threads = ((topology.ebx >> 8) & 0xff) + 1;
	const msr_t patch = rdmsr(0x8b);
	unsigned int selected = 0, changed = 0, exact = 0;

	native84_stage_check();
	if (cpuid_eax(1) != 0x00870f10 || cpuid_eax(0x80000001) != 0x00870f10 ||
	    topology.eax != initial_id || initial_id >= 16 || threads != 2 ||
	    rdmsr(0x17a).raw) {
		printk(BIOS_ERR, "Matisse CPU60 MSR trial refused id=%u threads=%u\n",
		       initial_id, threads);
		die("Matisse private per-CPU MSR trial refused");
	}
	/* The original selector helper accepts every thread for selector 0,
	 * initial APIC ID modulo threads-per-core zero for selector 1, and
	 * ID & (threads > 1 ? 7 : 3) zero for selector 2. The selected table
	 * contains four selector-0, twenty selector-1 and two selector-2 rows.
	 * This private same-board trial retains the already qualified masks;
	 * it is not a general implementation of the original PCD predicates.
	 */
	for (size_t row = 0; row < ARRAY_SIZE(matisse_vendor_registers); row++) {
		const struct matisse_vendor_msr *r = &matisse_vendor_registers[row];
		msr_t before, requested, after;

		if ((row >= 4 && row < 24 && initial_id % threads) ||
		    (row >= 24 && (initial_id & (threads > 1 ? 7U : 3U))))
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
	printk(BIOS_NOTICE, "Matisse CPU60 MSR id=%u threads=%u patch=%08x "
	       "selected=%u changed=%u exact=%u MCG=%016llx\n",
	       initial_id, threads, patch.lo, selected, changed, exact,
	       rdmsr(0x17a).raw);
	if (exact != selected || rdmsr(0x17a).raw)
		die("Matisse private per-CPU MSR readback mismatch");
}

void b565d4_matisse_vendor_msr_trial(unsigned int phase)
{
	msr_t before, requested, after;
	struct cpuid_result topology;

	native84_stage_check();
	topology = cpuid(0x8000001e);
	if (phase > 1 || cpuid_eax(1) != 0x00870f10 ||
	    cpuid_eax(0x80000001) != 0x00870f10 || topology.eax != 0 ||
	    !(rdmsr(LAPIC_BASE_MSR).lo & (1U << 8)))
		return;
	/*
	 * Private BSP comparison of the original tables in selector order.
	 * Original ASRock and MSI logical-ID decoders map 870F10 to CPU
	 * family 4, SoC family 1 and revision 0x10. Include matching type-4/5
	 * masks. Also compare the original 10DC[14]-clear record with the
	 * observed stock-board state; its PCD condition is not generalized.
	 * Omit the A0-only, other platform-dependent and APIC_BASE records.
	 * The BSP satisfies selectors 0/1/2. This is not full CPU init.
	 */
	printk(BIOS_NOTICE, "Matisse MSR50 phase=%u CPUID1=%08x EXT1=%08x "
	       "TOPOLOGY=%08x:%08x:%08x:%08x records=%zu\n", phase,
	       cpuid_eax(1), cpuid_eax(0x80000001), topology.eax,
	       topology.ebx, topology.ecx, topology.edx,
	       ARRAY_SIZE(matisse_vendor_registers));
	for (size_t row = 0; row < ARRAY_SIZE(matisse_vendor_registers); row++) {
		const struct matisse_vendor_msr *r = &matisse_vendor_registers[row];

		printk(BIOS_NOTICE, "Matisse MSR50 phase=%u row=%zu MSR=%08x BEGIN\n",
		       phase, row, r->index);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		before = rdmsr(r->index);
		requested.raw = (before.raw & ~r->mask) | r->data;
		if (requested.raw != before.raw)
			wrmsr(r->index, requested);
		after = rdmsr(r->index);
		printk(BIOS_NOTICE, "Matisse MSR50 phase=%u row=%zu MSR=%08x "
		       "before=%016llx requested=%016llx after=%016llx changed=%u "
		       "mask_exact=%u\n", phase, row, r->index, before.raw,
		       requested.raw, after.raw, requested.raw != before.raw,
		       (after.raw & r->mask) == r->data);
	}
	/* Match the documented vendor ordering: combine SMT CR0.CD only
	 * after ordinary MP initialization has completed on all threads.
	 */
	if (!phase)
		return;
	before = rdmsr(0xc0011023);
	requested.raw = before.raw | (1ULL << 49);
	if (requested.raw != before.raw)
		wrmsr(0xc0011023, requested);
	after = rdmsr(0xc0011023);
	printk(BIOS_NOTICE, "Matisse MSR45 phase=1 MSR=c0011023 before=%016llx "
	       "requested=%016llx after=%016llx changed=%u exact=%u\n",
	       before.raw, requested.raw, after.raw, requested.raw != before.raw,
	       after.raw == requested.raw);
	/* Private BSP comparison with the stock-board readback. Both original
	 * ASRock and MSI CCX DXE drivers finish their indexed-table sequence
	 * by setting this bit. This isolated trial does not claim that the
	 * vendor's dynamic table or complete CPU initialization is reproduced.
	 */
	printk(BIOS_NOTICE, "Matisse CFG50 phase=1 MSR=c0011074 BEGIN\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	before = rdmsr(0xc0011074);
	requested.raw = before.raw | (1ULL << 63);
	if (requested.raw != before.raw)
		wrmsr(0xc0011074, requested);
	after = rdmsr(0xc0011074);
	printk(BIOS_NOTICE, "Matisse CFG50 phase=1 MSR=c0011074 before=%016llx "
	       "requested=%016llx after=%016llx changed=%u exact=%u CPUID1=%08x\n",
	       before.raw, requested.raw, after.raw, requested.raw != before.raw,
	       after.raw == requested.raw, cpuid_eax(1));
}

static void set_gate(unsigned int vector, void (*handler)(void))
{
	const uintptr_t entry = (uintptr_t)handler;

	probe_idt[vector] = (struct matisse_gate) {
		.low = entry, .selector = 8, .type = 0x8e, .high = entry >> 16,
	};
}

void b565d4_matisse_irq_probe(unsigned int phase)
{
	const struct native84_context_image *image =
		(const void *)(uintptr_t)NATIVE84_CONTEXT_BEGIN;
	struct matisse_idtr old_idtr, new_idtr;
	msr_t base, enabled, patch, mcg;
	uint32_t flags, during_flags, tpr, svr, lint0, lint1, ier, icr_high;
	uint32_t delay, vector, bit, pending, in_service, busy, count0, count1;
	uint32_t count2, count3;
	uint32_t controls_restored, idtr_restored, restored_flags;
	uint16_t cs, ds;

	native84_stage_check();
	asm volatile ("pushfl; popl %0" : "=r" (flags) : : "memory");
	asm volatile ("sidt %0" : "=m" (old_idtr));
	asm volatile ("mov %%cs,%0; mov %%ds,%1" : "=r" (cs), "=r" (ds));
	base = rdmsr(LAPIC_BASE_MSR);
	if (phase > 3 || cpuid_eax(1) != 0x00870f10 || (flags & (1U << 9)) ||
	    cs != 8 || ds != 0x10 || base.hi || !(base.lo & (1U << 8)) ||
	    (base.lo != 0xfee00100 && base.lo != 0xfee00900) ||
	    old_idtr.limit != NATIVE84_IDT_BYTES - 1 ||
	    old_idtr.base != (uintptr_t)image->idt_08) {
		printk(BIOS_ERR, "Matisse IRQ43 phase=%u SKIP context\n", phase);
		return;
	}
	patch = rdmsr(0x8b);
	mcg = rdmsr(0x17a);
	if (mcg.raw || b565d4_matisse_irq_count[phase]) {
		printk(BIOS_ERR, "Matisse IRQ43 phase=%u SKIP MCG/count\n", phase);
		return;
	}
	enabled = base;
	enabled.lo |= LAPIC_BASE_MSR_ENABLE;
	wrmsr(LAPIC_BASE_MSR, enabled);
	if (rdmsr(LAPIC_BASE_MSR).raw != enabled.raw || lapicid() != 0 ||
	    lapic_read(LAPIC_LVR) != 0x80050010) {
		wrmsr(LAPIC_BASE_MSR, base);
		printk(BIOS_ERR, "Matisse IRQ43 phase=%u SKIP APIC\n", phase);
		return;
	}
	vector = 0xf1 + phase;
	bit = 1U << (vector & 31);
	for (unsigned int bank = 0; bank < 8; bank++) {
		const uint32_t allowed = !CONFIG(B565D4_MATISSE_SINGLE_MSR_IRQ_TRIAL) &&
			phase && bank == 7 ? 0x001e0000 & ~bit : 0;

		if (!lapic_read(0x100 + bank * 16) &&
		    !(lapic_read(0x200 + bank * 16) & ~allowed))
			continue;
		wrmsr(LAPIC_BASE_MSR, base);
		printk(BIOS_ERR, "Matisse IRQ43 phase=%u SKIP occupied vector\n", phase);
		return;
	}
	if (lapic_read(0x270) & bit) {
		wrmsr(LAPIC_BASE_MSR, base);
		return;
	}
	tpr = lapic_read(LAPIC_TASKPRI);
	svr = lapic_read(LAPIC_SPIV);
	lint0 = lapic_read(LAPIC_LVT0);
	lint1 = lapic_read(LAPIC_LVT1);
	ier = lapic_read(0x4f0);
	icr_high = lapic_read(LAPIC_ICR2);
	memcpy(probe_idt, (const void *)(uintptr_t)old_idtr.base, sizeof(probe_idt));
	set_gate(0xf1, b565d4_matisse_irq_handler0);
	set_gate(0xf2, b565d4_matisse_irq_handler1);
	set_gate(0xf3, b565d4_matisse_irq_handler2);
	set_gate(0xf4, b565d4_matisse_irq_handler3);
	new_idtr = (struct matisse_idtr) {
		.limit = sizeof(probe_idt) - 1, .base = (uintptr_t)probe_idt,
	};
	printk(BIOS_NOTICE, "Matisse IRQ43 phase=%u BEGIN vector=%02x base=%08x "
	       "patch=%08x ECTL=%08x IER7=%08x flags=%08x IRR7_before=%08x\n",
	       phase, vector, base.lo, patch.lo, lapic_read(0x410), ier, flags,
	       lapic_read(0x270));
	asm volatile ("lidt %0" : : "m" (new_idtr) : "memory");
	lapic_write(LAPIC_LVT0, lint0 | LAPIC_LVT_MASKED);
	lapic_write(LAPIC_LVT1, lint1 | LAPIC_LVT_MASKED);
	lapic_write(LAPIC_TASKPRI, 0);
	lapic_write(LAPIC_SPIV, (svr & ~0xffU) | 0x1ff);
	lapic_write(0x4f0, ier | 0x001e0000);
	lapic_write(LAPIC_ICR2, 0);
	lapic_write(LAPIC_ICR, 0x4000 | vector);
	for (delay = 0; delay < 100000 && (lapic_read(LAPIC_ICR) & LAPIC_ICR_BUSY);
	     delay++)
		asm volatile ("pause");
	busy = lapic_read(LAPIC_ICR) & LAPIC_ICR_BUSY;
	asm volatile ("sti; nop; pushfl; popl %0" : "=r" (during_flags) : : "memory");
	for (delay = 0; delay < 1000000 && !b565d4_matisse_irq_count[phase]; delay++)
		asm volatile ("pause" : : : "memory");
	asm volatile ("cli" : : : "memory");
	pending = lapic_read(0x270);
	in_service = lapic_read(0x170);
	count0 = b565d4_matisse_irq_count[0];
	count1 = b565d4_matisse_irq_count[1];
	count2 = b565d4_matisse_irq_count[2];
	count3 = b565d4_matisse_irq_count[3];
	lapic_write(LAPIC_LVT0, lint0);
	lapic_write(LAPIC_LVT1, lint1);
	lapic_write(LAPIC_TASKPRI, tpr);
	lapic_write(LAPIC_SPIV, svr);
	lapic_write(0x4f0, ier);
	lapic_write(LAPIC_ICR2, icr_high);
	controls_restored = lapic_read(LAPIC_LVT0) == lint0 &&
		lapic_read(LAPIC_LVT1) == lint1 && lapic_read(LAPIC_TASKPRI) == tpr &&
		lapic_read(LAPIC_SPIV) == svr && lapic_read(0x4f0) == ier &&
		lapic_read(LAPIC_ICR2) == icr_high;
	asm volatile ("lidt %0" : : "m" (old_idtr) : "memory");
	asm volatile ("sidt %0" : "=m" (new_idtr));
	idtr_restored = !memcmp(&old_idtr, &new_idtr, sizeof(old_idtr));
	wrmsr(LAPIC_BASE_MSR, base);
	asm volatile ("pushl %0; popfl" : : "r" (flags) : "memory", "cc");
	asm volatile ("pushfl; popl %0" : "=r" (restored_flags) : : "memory");
	printk(BIOS_NOTICE, "Matisse IRQ43 phase=%u RETURN count0=%u count1=%u "
	       "count2=%u count3=%u "
	       "delay=%u busy=%08x IRR7=%08x ISR7=%08x IFflags=%08x MCG=%016llx\n",
	       phase, count0, count1, count2, count3, delay, busy, pending, in_service,
	       during_flags,
	       rdmsr(0x17a).raw);
	printk(BIOS_NOTICE, "Matisse IRQ43 phase=%u RESTORE controls=%u IDTR=%u "
	       "base=%016llx IFbefore=%u IFafter=%u\n", phase, controls_restored,
	       idtr_restored, rdmsr(LAPIC_BASE_MSR).raw, !!(flags & (1U << 9)),
	       !!(restored_flags & (1U << 9)));
}
