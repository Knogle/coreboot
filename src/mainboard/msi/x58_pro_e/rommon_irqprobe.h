/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_ROMMON_IRQPROBE_H
#define MAINBOARD_MSI_X58_PRO_E_ROMMON_IRQPROBE_H

#include <stdbool.h>
#include <stdint.h>

enum x58_irqprobe_result {
	X58_IRQPROBE_OK,
	X58_IRQPROBE_PRECONDITION,
	X58_IRQPROBE_IDT_FAILED,
	X58_IRQPROBE_PIT_TIMEOUT,
	X58_IRQPROBE_IRQ_TIMEOUT,
	X58_IRQPROBE_BAD_ISR,
	X58_IRQPROBE_MULTIPLE_IRQ,
	X58_IRQPROBE_CLEANUP_FAILED,
};

struct x58_irqprobe_report {
	uint32_t lpc_id;
	uint32_t apic_base_lo;
	uint32_t eflags;
	uint32_t tpr_before;
	uint32_t svr_before;
	uint32_t lvt0_before;
	uint32_t lvt1_before;
	uint32_t svr_after;
	uint32_t lvt0_after;
	uint8_t master_mask_before;
	uint8_t slave_mask_before;
	uint8_t elcr1_before;
	uint8_t elcr2_before;
	uint8_t pic_irr_before;
	uint8_t pic_irr_armed;
	uint8_t pic_isr_at_entry;
	uint8_t master_mask_after;
	uint8_t slave_mask_after;
	uint32_t hits;
	uint32_t pit_polls;
	uint32_t irq_polls;
	bool mutated;
	bool idt_installed;
	bool cleanup_ok;
};

/* The backend is public solely so the bounded state machine can be host-tested. */
struct x58_irqprobe_ops {
	void *context;
	uint8_t (*io_read8)(void *context, uint16_t port);
	void (*io_write8)(void *context, uint16_t port, uint8_t value);
	uint32_t (*lapic_read)(void *context, uint16_t reg);
	void (*lapic_write)(void *context, uint16_t reg, uint32_t value);
	uint32_t (*read_lpc_id)(void *context);
	uint32_t (*read_apic_base_lo)(void *context);
	uint32_t (*read_eflags)(void *context);
	bool (*install_idt)(void *context);
	void (*restore_idt)(void *context);
	void (*wait_irq)(void *context, uint32_t limit, uint32_t *polls,
		uint32_t *hits, uint8_t *pic_isr);
};

enum x58_irqprobe_result x58_irqprobe_pit_run(
	const struct x58_irqprobe_ops *ops, struct x58_irqprobe_report *report);

#ifndef X58_IRQPROBE_HOST_TEST
enum x58_irqprobe_result x58_rommon_irqprobe_pit(
	struct x58_irqprobe_report *report);
#endif

const char *x58_irqprobe_result_name(enum x58_irqprobe_result result);

#endif
