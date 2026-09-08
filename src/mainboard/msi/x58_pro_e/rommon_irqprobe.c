/* SPDX-License-Identifier: GPL-2.0-only */

#include "rommon_irqprobe.h"

#include <stddef.h>

#define IRQPROBE_LPC_ID		0x3a168086U
#define IRQPROBE_APIC_BASE	0xfee00900U
#define IRQPROBE_EFLAGS_IF	(1U << 9)

#define PIC_MASTER_CMD		0x20
#define PIC_MASTER_DATA		0x21
#define PIC_SLAVE_CMD		0xa0
#define PIC_SLAVE_DATA		0xa1
#define PIC_ELCR1		0x4d0
#define PIC_ELCR2		0x4d1
#define PIC_OCW3_IRR		0x0a

#define PIT_COUNTER0		0x40
#define PIT_MODE		0x43
#define PIT_MODE0_LOHI		0x30
#define PIT_COUNT		0x4000

#define LAPIC_TPR		0x080
#define LAPIC_SVR		0x0f0
#define LAPIC_LVT0		0x350
#define LAPIC_LVT1		0x360
#define LAPIC_SVR_DISABLED	0x000000ffU
#define LAPIC_SVR_ENABLED	0x0000010fU
#define LAPIC_LVT_MASKED	0x00010000U
#define LAPIC_LVT_EXTINT	0x00000700U

#define PIC_MASTER_MASK_BASE	0xfb
#define PIC_MASTER_MASK_IRQ0	0xfa
#define PIC_SLAVE_MASK_BASE	0xff
#define IRQPROBE_POLL_LIMIT	1000000U

static void pic_initialize(const struct x58_irqprobe_ops *ops)
{
	void *ctx = ops->context;

	ops->io_write8(ctx, PIC_MASTER_CMD, 0x11);
	ops->io_write8(ctx, PIC_SLAVE_CMD, 0x11);
	ops->io_write8(ctx, PIC_MASTER_DATA, 0x20);
	ops->io_write8(ctx, PIC_SLAVE_DATA, 0x28);
	ops->io_write8(ctx, PIC_MASTER_DATA, 0x04);
	ops->io_write8(ctx, PIC_SLAVE_DATA, 0x02);
	ops->io_write8(ctx, PIC_MASTER_DATA, 0x01);
	ops->io_write8(ctx, PIC_SLAVE_DATA, 0x01);
	ops->io_write8(ctx, PIC_SLAVE_DATA, PIC_SLAVE_MASK_BASE);
	ops->io_write8(ctx, PIC_MASTER_DATA, PIC_MASTER_MASK_BASE);
}

static bool ops_valid(const struct x58_irqprobe_ops *ops)
{
	return ops && ops->io_read8 && ops->io_write8 && ops->lapic_read &&
		ops->lapic_write && ops->read_lpc_id && ops->read_apic_base_lo &&
		ops->read_eflags && ops->install_idt && ops->restore_idt &&
		ops->wait_irq;
}

enum x58_irqprobe_result x58_irqprobe_pit_run(
	const struct x58_irqprobe_ops *ops, struct x58_irqprobe_report *r)
{
	enum x58_irqprobe_result result = X58_IRQPROBE_PRECONDITION;
	void *ctx;
	uint32_t poll;

	if (!ops_valid(ops) || !r)
		return X58_IRQPROBE_PRECONDITION;
	ctx = ops->context;
	*r = (struct x58_irqprobe_report){ 0 };

	r->lpc_id = ops->read_lpc_id(ctx);
	r->apic_base_lo = ops->read_apic_base_lo(ctx);
	r->eflags = ops->read_eflags(ctx);
	r->master_mask_before = ops->io_read8(ctx, PIC_MASTER_DATA);
	r->slave_mask_before = ops->io_read8(ctx, PIC_SLAVE_DATA);
	r->elcr1_before = ops->io_read8(ctx, PIC_ELCR1);
	r->elcr2_before = ops->io_read8(ctx, PIC_ELCR2);
	r->tpr_before = ops->lapic_read(ctx, LAPIC_TPR);
	r->svr_before = ops->lapic_read(ctx, LAPIC_SVR);
	r->lvt0_before = ops->lapic_read(ctx, LAPIC_LVT0);
	r->lvt1_before = ops->lapic_read(ctx, LAPIC_LVT1);

	/* Select IRR while IRQ0 is still masked, then reject a stale request. */
	ops->io_write8(ctx, PIC_MASTER_CMD, PIC_OCW3_IRR);
	r->pic_irr_before = ops->io_read8(ctx, PIC_MASTER_CMD);
	if (r->lpc_id != IRQPROBE_LPC_ID ||
	    r->apic_base_lo != IRQPROBE_APIC_BASE ||
	    (r->eflags & IRQPROBE_EFLAGS_IF) ||
	    r->master_mask_before != PIC_MASTER_MASK_BASE ||
	    r->slave_mask_before != PIC_SLAVE_MASK_BASE ||
	    r->elcr1_before != 0x00 || r->elcr2_before != 0x02 ||
	    r->tpr_before != 0 || r->svr_before != LAPIC_SVR_DISABLED ||
	    r->lvt0_before != LAPIC_LVT_MASKED ||
	    r->lvt1_before != LAPIC_LVT_MASKED ||
	    (r->pic_irr_before & 1))
		return X58_IRQPROBE_PRECONDITION;

	if (!ops->install_idt(ctx))
		return X58_IRQPROBE_IDT_FAILED;
	r->idt_installed = true;

	/* Mode 0 is a terminal one-shot, avoiding a continuing periodic source. */
	r->mutated = true;
	ops->io_write8(ctx, PIT_MODE, PIT_MODE0_LOHI);
	ops->io_write8(ctx, PIT_COUNTER0, PIT_COUNT & 0xff);
	ops->io_write8(ctx, PIT_COUNTER0, PIT_COUNT >> 8);

	for (poll = 0; poll < IRQPROBE_POLL_LIMIT; poll++) {
		ops->io_write8(ctx, PIC_MASTER_CMD, PIC_OCW3_IRR);
		r->pic_irr_armed = ops->io_read8(ctx, PIC_MASTER_CMD);
		if (r->pic_irr_armed & 1)
			break;
	}
	r->pit_polls = poll + (poll != IRQPROBE_POLL_LIMIT);
	if (!(r->pic_irr_armed & 1)) {
		result = X58_IRQPROBE_PIT_TIMEOUT;
		goto cleanup;
	}

	ops->lapic_write(ctx, LAPIC_SVR, LAPIC_SVR_ENABLED);
	ops->lapic_write(ctx, LAPIC_LVT0, LAPIC_LVT_EXTINT);
	if (ops->lapic_read(ctx, LAPIC_SVR) != LAPIC_SVR_ENABLED ||
	    ops->lapic_read(ctx, LAPIC_LVT0) != LAPIC_LVT_EXTINT) {
		result = X58_IRQPROBE_PRECONDITION;
		goto cleanup;
	}
	ops->io_write8(ctx, PIC_MASTER_DATA, PIC_MASTER_MASK_IRQ0);
	if (ops->io_read8(ctx, PIC_MASTER_DATA) != PIC_MASTER_MASK_IRQ0) {
		result = X58_IRQPROBE_PRECONDITION;
		goto cleanup;
	}

	ops->wait_irq(ctx, IRQPROBE_POLL_LIMIT, &r->irq_polls, &r->hits,
		&r->pic_isr_at_entry);
	if (r->hits == 0)
		result = X58_IRQPROBE_IRQ_TIMEOUT;
	else if (r->hits != 1)
		result = X58_IRQPROBE_MULTIPLE_IRQ;
	else if (!(r->pic_isr_at_entry & 1))
		result = X58_IRQPROBE_BAD_ISR;
	else
		result = X58_IRQPROBE_OK;

cleanup:
	/* Quiesce the source before disabling virtual wire or restoring the IDT. */
	ops->io_write8(ctx, PIC_MASTER_DATA, 0xff);
	ops->io_write8(ctx, PIC_SLAVE_DATA, 0xff);
	ops->lapic_write(ctx, LAPIC_LVT0, LAPIC_LVT_MASKED);
	pic_initialize(ops);
	ops->lapic_write(ctx, LAPIC_SVR, LAPIC_SVR_DISABLED);
	ops->restore_idt(ctx);
	r->idt_installed = false;
	r->master_mask_after = ops->io_read8(ctx, PIC_MASTER_DATA);
	r->slave_mask_after = ops->io_read8(ctx, PIC_SLAVE_DATA);
	r->svr_after = ops->lapic_read(ctx, LAPIC_SVR);
	r->lvt0_after = ops->lapic_read(ctx, LAPIC_LVT0);
	r->cleanup_ok = r->master_mask_after == PIC_MASTER_MASK_BASE &&
		r->slave_mask_after == PIC_SLAVE_MASK_BASE &&
		r->svr_after == LAPIC_SVR_DISABLED &&
		r->lvt0_after == LAPIC_LVT_MASKED;
	if (!r->cleanup_ok)
		return X58_IRQPROBE_CLEANUP_FAILED;
	return result;
}

const char *x58_irqprobe_result_name(enum x58_irqprobe_result result)
{
	switch (result) {
	case X58_IRQPROBE_OK: return "OK";
	case X58_IRQPROBE_PRECONDITION: return "PRECONDITION";
	case X58_IRQPROBE_IDT_FAILED: return "IDT_FAILED";
	case X58_IRQPROBE_PIT_TIMEOUT: return "PIT_TIMEOUT";
	case X58_IRQPROBE_IRQ_TIMEOUT: return "IRQ_TIMEOUT";
	case X58_IRQPROBE_BAD_ISR: return "BAD_ISR";
	case X58_IRQPROBE_MULTIPLE_IRQ: return "MULTIPLE_IRQ";
	case X58_IRQPROBE_CLEANUP_FAILED: return "CLEANUP_FAILED";
	}
	return "UNKNOWN";
}

#ifndef X58_IRQPROBE_HOST_TEST

#include <arch/io.h>
#include <arch/pci_io_cfg.h>
#include <cpu/x86/msr.h>
#include <device/pci_type.h>

#define ICH10R_LPC_DEV PCI_DEV(0, 0x1f, 0)
#define IA32_APIC_BASE 0x1b

struct irqprobe_gate {
	uint16_t offset_low;
	uint16_t selector;
	uint16_t flags;
	uint16_t offset_high;
} __attribute__((packed));

struct irqprobe_idtr {
	uint16_t limit;
	uint32_t base;
} __attribute__((packed));

extern uint8_t x58_rommon_irq0_entry[];
extern uint8_t x58_rommon_irq_fatal_entry[];
volatile uint32_t x58_irqprobe_hits;
volatile uint8_t x58_irqprobe_pic_isr;

static struct irqprobe_gate irqprobe_idt[0x21] __attribute__((aligned(8)));
static struct irqprobe_idtr irqprobe_saved_idtr;

static uint8_t hw_io_read8(void *context, uint16_t port)
{
	(void)context;
	return inb(port);
}

static void hw_io_write8(void *context, uint16_t port, uint8_t value)
{
	(void)context;
	outb(value, port);
}

static uint32_t hw_lapic_read(void *context, uint16_t reg)
{
	(void)context;
	return *(volatile uint32_t *)(uintptr_t)(0xfee00000U + reg);
}

static void hw_lapic_write(void *context, uint16_t reg, uint32_t value)
{
	(void)context;
	*(volatile uint32_t *)(uintptr_t)(0xfee00000U + reg) = value;
}

static uint32_t hw_read_lpc_id(void *context)
{
	(void)context;
	return pci_io_read_config32(ICH10R_LPC_DEV, 0);
}

static uint32_t hw_read_apic_base_lo(void *context)
{
	(void)context;
	return rdmsr(IA32_APIC_BASE).lo;
}

static uint32_t hw_read_eflags(void *context)
{
	uint32_t flags;
	(void)context;
	__asm__ volatile("pushfl; popl %0" : "=r" (flags));
	return flags;
}

static void set_gate(struct irqprobe_gate *gate, uintptr_t entry,
	uint16_t selector)
{
	gate->offset_low = entry;
	gate->selector = selector;
	gate->flags = 0x8e00;
	gate->offset_high = entry >> 16;
}

static bool hw_install_idt(void *context)
{
	struct irqprobe_idtr temporary;
	uint16_t cs;
	unsigned int vector;
	(void)context;

	__asm__ volatile("sidt %0" : "=m" (irqprobe_saved_idtr));
	__asm__ volatile("mov %%cs, %0" : "=r" (cs));
	for (vector = 0; vector < 0x20; vector++)
		set_gate(&irqprobe_idt[vector],
			(uintptr_t)x58_rommon_irq_fatal_entry, cs);
	set_gate(&irqprobe_idt[0x20], (uintptr_t)x58_rommon_irq0_entry, cs);
	temporary.limit = sizeof(irqprobe_idt) - 1;
	temporary.base = (uintptr_t)irqprobe_idt;
	__asm__ volatile("lidt %0" : : "m" (temporary) : "memory");
	return true;
}

static void hw_restore_idt(void *context)
{
	(void)context;
	__asm__ volatile("lidt %0" : : "m" (irqprobe_saved_idtr) : "memory");
}

static void hw_wait_irq(void *context, uint32_t limit, uint32_t *polls,
	uint32_t *hits, uint8_t *pic_isr)
{
	uint32_t poll;
	(void)context;

	x58_irqprobe_hits = 0;
	x58_irqprobe_pic_isr = 0;
	__asm__ volatile("sti; nop" : : : "memory");
	for (poll = 0; poll < limit && x58_irqprobe_hits == 0; poll++)
		__asm__ volatile("pause");
	__asm__ volatile("cli" : : : "memory");
	*polls = poll;
	*hits = x58_irqprobe_hits;
	*pic_isr = x58_irqprobe_pic_isr;
}

enum x58_irqprobe_result x58_rommon_irqprobe_pit(
	struct x58_irqprobe_report *report)
{
	const struct x58_irqprobe_ops ops = {
		.io_read8 = hw_io_read8,
		.io_write8 = hw_io_write8,
		.lapic_read = hw_lapic_read,
		.lapic_write = hw_lapic_write,
		.read_lpc_id = hw_read_lpc_id,
		.read_apic_base_lo = hw_read_apic_base_lo,
		.read_eflags = hw_read_eflags,
		.install_idt = hw_install_idt,
		.restore_idt = hw_restore_idt,
		.wait_irq = hw_wait_irq,
	};

	__asm__ volatile("cli" : : : "memory");
	return x58_irqprobe_pit_run(&ops, report);
}

#endif
