/* SPDX-License-Identifier: GPL-2.0-only */

#include <assert.h>
#include <stdio.h>

#include "rommon_irqprobe.h"

struct fake_irqprobe {
	uint8_t ports[0x10000];
	uint32_t lapic[0x400 / 4];
	uint32_t lpc_id;
	uint32_t apic_base;
	uint32_t eflags;
	uint32_t irq_hits;
	uint8_t irq_isr;
	unsigned int pit_reads_until_irq;
	unsigned int pit_reads;
	unsigned int pit_data_writes;
	unsigned int writes;
	uint16_t write_port[64];
	uint8_t write_value[64];
	bool idt_installed;
	bool idt_restored;
	bool ignore_svr_cleanup;
};

static uint8_t fake_io_read(void *context, uint16_t port)
{
	struct fake_irqprobe *f = context;

	if (port == 0x20 && f->pit_data_writes == 2 &&
	    f->pit_reads_until_irq &&
	    ++f->pit_reads >= f->pit_reads_until_irq)
		return 1;
	return f->ports[port];
}

static void fake_io_write(void *context, uint16_t port, uint8_t value)
{
	struct fake_irqprobe *f = context;

	if (f->writes < 64) {
		f->write_port[f->writes] = port;
		f->write_value[f->writes] = value;
	}
	f->writes++;
	f->ports[port] = value;
	if (port == 0x40)
		f->pit_data_writes++;
}

static uint32_t fake_lapic_read(void *context, uint16_t reg)
{
	struct fake_irqprobe *f = context;
	return f->lapic[reg / 4];
}

static void fake_lapic_write(void *context, uint16_t reg, uint32_t value)
{
	struct fake_irqprobe *f = context;
	if (f->ignore_svr_cleanup && reg == 0xf0 && value == 0xff)
		return;
	f->lapic[reg / 4] = value;
}

static uint32_t fake_read_lpc(void *context)
{
	return ((struct fake_irqprobe *)context)->lpc_id;
}

static uint32_t fake_read_apic_base(void *context)
{
	return ((struct fake_irqprobe *)context)->apic_base;
}

static uint32_t fake_read_eflags(void *context)
{
	return ((struct fake_irqprobe *)context)->eflags;
}

static bool fake_install_idt(void *context)
{
	((struct fake_irqprobe *)context)->idt_installed = true;
	return true;
}

static void fake_restore_idt(void *context)
{
	struct fake_irqprobe *f = context;
	assert(f->idt_installed);
	f->idt_installed = false;
	f->idt_restored = true;
}

static void fake_wait_irq(void *context, uint32_t limit, uint32_t *polls,
	uint32_t *hits, uint8_t *pic_isr)
{
	struct fake_irqprobe *f = context;
	(void)limit;
	*polls = f->irq_hits ? 1 : limit;
	*hits = f->irq_hits;
	*pic_isr = f->irq_isr;
}

static struct fake_irqprobe baseline(void)
{
	struct fake_irqprobe f = { 0 };
	f.lpc_id = 0x3a168086;
	f.apic_base = 0xfee00900;
	f.eflags = 0x46;
	f.ports[0x21] = 0xfb;
	f.ports[0xa1] = 0xff;
	f.ports[0x4d0] = 0x00;
	f.ports[0x4d1] = 0x02;
	f.lapic[0x080 / 4] = 0;
	f.lapic[0x0f0 / 4] = 0xff;
	f.lapic[0x350 / 4] = 0x10000;
	f.lapic[0x360 / 4] = 0x10000;
	return f;
}

static struct x58_irqprobe_ops backend(struct fake_irqprobe *f)
{
	const struct x58_irqprobe_ops ops = {
		.context = f,
		.io_read8 = fake_io_read,
		.io_write8 = fake_io_write,
		.lapic_read = fake_lapic_read,
		.lapic_write = fake_lapic_write,
		.read_lpc_id = fake_read_lpc,
		.read_apic_base_lo = fake_read_apic_base,
		.read_eflags = fake_read_eflags,
		.install_idt = fake_install_idt,
		.restore_idt = fake_restore_idt,
		.wait_irq = fake_wait_irq,
	};
	return ops;
}

static void assert_clean(const struct fake_irqprobe *f,
	const struct x58_irqprobe_report *r)
{
	assert(f->idt_restored && !f->idt_installed);
	assert(f->ports[0x21] == 0xfb && f->ports[0xa1] == 0xff);
	assert(f->lapic[0x0f0 / 4] == 0xff);
	assert(f->lapic[0x350 / 4] == 0x10000);
	assert(r->cleanup_ok);
}

static void test_precondition_has_no_pit_write(void)
{
	struct fake_irqprobe f = baseline();
	struct x58_irqprobe_ops ops = backend(&f);
	struct x58_irqprobe_report r;
	f.ports[0x21] = 0xff;
	assert(x58_irqprobe_pit_run(&ops, &r) == X58_IRQPROBE_PRECONDITION);
	assert(f.writes == 1 && f.write_port[0] == 0x20 &&
		f.write_value[0] == 0x0a);
	assert(!r.mutated && !f.idt_installed && !f.idt_restored);
}

static void test_success_and_cleanup(void)
{
	struct fake_irqprobe f = baseline();
	struct x58_irqprobe_ops ops = backend(&f);
	struct x58_irqprobe_report r;
	f.pit_reads_until_irq = 3;
	f.irq_hits = 1;
	f.irq_isr = 1;
	assert(x58_irqprobe_pit_run(&ops, &r) == X58_IRQPROBE_OK);
	assert(r.pic_irr_armed & 1);
	assert(r.hits == 1 && (r.pic_isr_at_entry & 1));
	assert_clean(&f, &r);
}

static void test_pit_timeout_cleanup(void)
{
	struct fake_irqprobe f = baseline();
	struct x58_irqprobe_ops ops = backend(&f);
	struct x58_irqprobe_report r;
	assert(x58_irqprobe_pit_run(&ops, &r) == X58_IRQPROBE_PIT_TIMEOUT);
	assert(r.pit_polls == 1000000);
	assert_clean(&f, &r);
}

static void test_irq_results_and_cleanup_failure(void)
{
	struct fake_irqprobe f = baseline();
	struct x58_irqprobe_ops ops = backend(&f);
	struct x58_irqprobe_report r;
	f.pit_reads_until_irq = 1;
	assert(x58_irqprobe_pit_run(&ops, &r) == X58_IRQPROBE_IRQ_TIMEOUT);
	assert(r.irq_polls == 1000000);
	assert_clean(&f, &r);

	f = baseline();
	ops = backend(&f);
	f.pit_reads_until_irq = 1;
	f.irq_hits = 1;
	assert(x58_irqprobe_pit_run(&ops, &r) == X58_IRQPROBE_BAD_ISR);
	assert_clean(&f, &r);

	f = baseline();
	ops = backend(&f);
	f.pit_reads_until_irq = 1;
	f.irq_hits = 2;
	f.irq_isr = 1;
	assert(x58_irqprobe_pit_run(&ops, &r) == X58_IRQPROBE_MULTIPLE_IRQ);
	assert_clean(&f, &r);

	f = baseline();
	ops = backend(&f);
	f.pit_reads_until_irq = 1;
	f.irq_hits = 1;
	f.irq_isr = 1;
	f.ignore_svr_cleanup = true;
	assert(x58_irqprobe_pit_run(&ops, &r) == X58_IRQPROBE_CLEANUP_FAILED);
	assert(!r.cleanup_ok);
}

int main(void)
{
	test_precondition_has_no_pit_write();
	test_success_and_cleanup();
	test_pit_timeout_cleanup();
	test_irq_results_and_cleanup_failure();
	puts("x58 rommon irqprobe tests: PASS");
	return 0;
}
