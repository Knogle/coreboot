/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * B06WI is an exact, board-only experiment derived from two byte-identical
 * read-only vendor snapshots.  It copies only fields that describe interrupt
 * pin/routing policy.  PIRQ, INT_LINE, IOAPIC redirection and every GPIO
 * register deliberately remain untouched.
 */

#include <arch/pci_io_cfg.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <device/mmio.h>
#include <device/pci_def.h>
#include <device/pci_type.h>
#include <halt.h>
#include <southbridge/intel/i82801jx/i82801jx.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "b06wi_irq_route.h"

#define B06WI_LPC_DEV		PCI_DEV(0, 0x1f, 0)
#define B06WI_LPC_ID		0x3a168086u
#define B06WI_RCBA_ENABLED	(CONFIG_FIXED_RCBA_MMIO_BASE | RCBA_ENABLE)
#define B06WI_OIC_TARGET	0x03u
#define B06WI_PIRQ_DISABLED	0x80u

#define B06WI_IOAPIC_BASE	0xfec00000u
#define B06WI_IOREGSEL		0x00u
#define B06WI_IOWIN		0x10u
#define B06WI_IOAPIC_ID_REG	0x00u
#define B06WI_IOAPIC_VER_REG	0x01u
#define B06WI_IOAPIC_VER	0x00170020u
#define B06WI_IOAPIC_ID_MASK	0x0f000000u
#define B06WI_IOAPIC_ID_PRE	0x00000000u
#define B06WI_IOAPIC_ID_TARGET	0x01000000u
#define B06WI_IOAPIC_REDIR_BASE 0x10u
#define B06WI_IOAPIC_REDIR_COUNT 24u
#define B06WI_IOAPIC_LOW_MASKED 0x00010000u
#define B06WI_IOAPIC_HIGH_ZERO	0x00000000u

#define B06WI_D26IP_F2_MASK	0x00000f00u
#define B06WI_D26IP_F2_PIN_D	0x00000400u

#define POST_B06WI_BEGIN	0xa6u
#define POST_B06WI_WRITTEN	0xa7u
#define POST_B06WI_READY	0xa8u
#define POST_B06WI_FAIL		0xa9u

struct b06wi_route32 {
	uint16_t reg;
	uint32_t pre;
	uint32_t mask;
	uint32_t bits;
	uint32_t target;
	const char *name;
};

struct b06wi_route16 {
	uint16_t reg;
	uint16_t pre;
	uint16_t mask;
	uint16_t bits;
	uint16_t target;
	const char *name;
};

struct b06wi_pin_gate {
	pci_devfn_t dev;
	uint32_t id;
	uint8_t pre_pin;
	uint8_t target_pin;
	const char *name;
};

static const struct b06wi_route32 b06wi_ip_routes[] = {
	{ D31IP, 0x03243200u, 0, 0, 0x03243200u, "D31IP" },
	{ D30IP, 0x00000000u, 0, 0, 0x00000000u, "D30IP" },
	{ D29IP, 0x10004321u, 0, 0, 0x10004321u, "D29IP" },
	{ D28IP, 0x00214321u, 0, 0, 0x00214321u, "D28IP" },
	{ D27IP, 0x00000001u, 0, 0, 0x00000001u, "D27IP" },
	{ D26IP, 0x30000321u, B06WI_D26IP_F2_MASK,
	  B06WI_D26IP_F2_PIN_D, 0x30000421u, "D26IP" },
};

static const struct b06wi_route16 b06wi_ir_routes[] = {
	{ D31IR, 0x3210u, 0xf0ffu, 0x0032u, 0x0232u, "D31IR" },
	{ D30IR, 0x0000u, 0x0000u, 0x0000u, 0x0000u, "D30IR" },
	{ D29IR, 0x3210u, 0xf0ffu, 0x0037u, 0x0237u, "D29IR" },
	{ D28IR, 0x3210u, 0x00ffu, 0x0001u, 0x3201u, "D28IR" },
	{ D27IR, 0x3210u, 0x000fu, 0x0006u, 0x3216u, "D27IR" },
	{ D26IR, 0x3210u, 0x00f0u, 0x0050u, 0x3250u, "D26IR" },
};

static const uint8_t b06wi_pirq_regs[] = {
	PIRQA_ROUT, PIRQB_ROUT, PIRQC_ROUT, PIRQD_ROUT,
	PIRQE_ROUT, PIRQF_ROUT, PIRQG_ROUT, PIRQH_ROUT,
};

/* Pin values are PCI 0x3d encodings: A=1, B=2, C=3 and D=4. */
static const struct b06wi_pin_gate b06wi_pci_pins[] = {
	{ PCI_DEV(0, 0x03, 0), 0x340a8086u, 1, 1, "D03F0" },
	{ PCI_DEV(0, 0x1a, 0), 0x3a378086u, 1, 1, "D26F0" },
	{ PCI_DEV(0, 0x1a, 1), 0x3a388086u, 2, 2, "D26F1" },
	{ PCI_DEV(0, 0x1a, 2), 0x3a398086u, 3, 4, "D26F2" },
	{ PCI_DEV(0, 0x1a, 7), 0x3a3c8086u, 3, 3, "D26F7" },
	{ PCI_DEV(0, 0x1c, 4), 0x3a488086u, 1, 1, "D28F4" },
	{ PCI_DEV(0, 0x1d, 0), 0x3a348086u, 1, 1, "D29F0" },
	{ PCI_DEV(0, 0x1d, 1), 0x3a358086u, 2, 2, "D29F1" },
	{ PCI_DEV(0, 0x1d, 2), 0x3a368086u, 3, 3, "D29F2" },
	{ PCI_DEV(0, 0x1d, 7), 0x3a3a8086u, 1, 1, "D29F7" },
	{ PCI_DEV(0, 0x1f, 2), 0x3a228086u, 2, 2, "D31F2" },
};

_Static_assert(((0x30000321u & ~B06WI_D26IP_F2_MASK) |
	B06WI_D26IP_F2_PIN_D) == 0x30000421u,
	"B06WI D26:F2 masked pin transform changed");
_Static_assert(((0x3210u & ~0xf0ffu) | 0x0032u) == 0x0232u,
	"B06WI D31IR transform changed");
_Static_assert(((0x3210u & ~0xf0ffu) | 0x0037u) == 0x0237u,
	"B06WI D29IR transform changed");
_Static_assert(((0x3210u & ~0x00ffu) | 0x0001u) == 0x3201u,
	"B06WI D28IR transform changed");
_Static_assert(((0x3210u & ~0x000fu) | 0x0006u) == 0x3216u,
	"B06WI D27IR transform changed");
_Static_assert(((0x3210u & ~0x00f0u) | 0x0050u) == 0x3250u,
	"B06WI D26IR transform changed");

static bool b06wi_attempted;
static bool b06wi_ready;

static bool b06wi_ioapic_select(uint8_t reg)
{
	write32p(B06WI_IOAPIC_BASE + B06WI_IOREGSEL, reg);
	return read32p(B06WI_IOAPIC_BASE + B06WI_IOREGSEL) == reg;
}

static bool b06wi_ioapic_read(uint8_t reg, uint32_t *value)
{
	if (!b06wi_ioapic_select(reg))
		return false;
	*value = read32p(B06WI_IOAPIC_BASE + B06WI_IOWIN);
	return true;
}

static bool b06wi_ioapic_write(uint8_t reg, uint32_t value)
{
	if (!b06wi_ioapic_select(reg))
		return false;
	write32p(B06WI_IOAPIC_BASE + B06WI_IOWIN, value);
	return read32p(B06WI_IOAPIC_BASE + B06WI_IOWIN) == value;
}

static bool b06wi_pirq_is_disabled(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(b06wi_pirq_regs); i++) {
		if (pci_io_read_config8(B06WI_LPC_DEV, b06wi_pirq_regs[i]) !=
		    B06WI_PIRQ_DISABLED)
			return false;
	}
	return true;
}

static bool b06wi_ioapic_is_canonical(uint32_t expected_id)
{
	uint32_t value;

	if (!b06wi_ioapic_read(B06WI_IOAPIC_ID_REG, &value) ||
	    value != expected_id ||
	    !b06wi_ioapic_read(B06WI_IOAPIC_VER_REG, &value) ||
	    value != B06WI_IOAPIC_VER)
		return false;
	for (unsigned int entry = 0; entry < B06WI_IOAPIC_REDIR_COUNT; entry++) {
		const uint8_t low = B06WI_IOAPIC_REDIR_BASE + 2 * entry;

		if (!b06wi_ioapic_read(low, &value) ||
		    value != B06WI_IOAPIC_LOW_MASKED ||
		    !b06wi_ioapic_read(low + 1, &value) ||
		    value != B06WI_IOAPIC_HIGH_ZERO)
			return false;
	}
	return b06wi_ioapic_select(B06WI_IOAPIC_ID_REG);
}

static bool b06wi_pin_tuple(bool target)
{
	for (size_t i = 0; i < ARRAY_SIZE(b06wi_pci_pins); i++) {
		const struct b06wi_pin_gate *pin = &b06wi_pci_pins[i];
		const uint8_t expected = target ? pin->target_pin : pin->pre_pin;

		if (pci_io_read_config32(pin->dev, PCI_VENDOR_ID) != pin->id ||
		    pci_io_read_config8(pin->dev, PCI_INTERRUPT_PIN) != expected)
			return false;
	}
	return true;
}

static bool b06wi_route_tuple(bool target)
{
	for (size_t i = 0; i < ARRAY_SIZE(b06wi_ip_routes); i++) {
		const uint32_t expected = target ? b06wi_ip_routes[i].target :
			b06wi_ip_routes[i].pre;

		if (read32p(CONFIG_FIXED_RCBA_MMIO_BASE + b06wi_ip_routes[i].reg) !=
		    expected)
			return false;
	}
	for (size_t i = 0; i < ARRAY_SIZE(b06wi_ir_routes); i++) {
		const uint16_t expected = target ? b06wi_ir_routes[i].target :
			b06wi_ir_routes[i].pre;

		if (read16p(CONFIG_FIXED_RCBA_MMIO_BASE + b06wi_ir_routes[i].reg) !=
		    expected)
			return false;
	}
	return true;
}

static bool b06wi_write_targets(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(b06wi_ip_routes); i++) {
		const struct b06wi_route32 *route = &b06wi_ip_routes[i];
		uint32_t value;

		if (!route->mask)
			continue;
		value = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + route->reg);
		if (value != route->pre)
			return false;
		value = (value & ~route->mask) | route->bits;
		write32p(CONFIG_FIXED_RCBA_MMIO_BASE + route->reg, value);
		if (read32p(CONFIG_FIXED_RCBA_MMIO_BASE + route->reg) != route->target)
			return false;
	}
	for (size_t i = 0; i < ARRAY_SIZE(b06wi_ir_routes); i++) {
		const struct b06wi_route16 *route = &b06wi_ir_routes[i];
		uint16_t value;

		if (!route->mask)
			continue;
		value = read16p(CONFIG_FIXED_RCBA_MMIO_BASE + route->reg);
		if (value != route->pre)
			return false;
		value = (value & ~route->mask) | route->bits;
		write16p(CONFIG_FIXED_RCBA_MMIO_BASE + route->reg, value);
		if (read16p(CONFIG_FIXED_RCBA_MMIO_BASE + route->reg) != route->target)
			return false;
	}
	return b06wi_ioapic_write(B06WI_IOAPIC_ID_REG,
		B06WI_IOAPIC_ID_TARGET);
}

static bool b06wi_rollback(void)
{
	bool ok = true;

	for (size_t i = ARRAY_SIZE(b06wi_ir_routes); i-- > 0;) {
		const struct b06wi_route16 *route = &b06wi_ir_routes[i];
		uint16_t value;

		if (!route->mask)
			continue;
		value = read16p(CONFIG_FIXED_RCBA_MMIO_BASE + route->reg);
		value = (value & ~route->mask) | (route->pre & route->mask);
		write16p(CONFIG_FIXED_RCBA_MMIO_BASE + route->reg, value);
		ok &= read16p(CONFIG_FIXED_RCBA_MMIO_BASE + route->reg) == route->pre;
	}
	for (size_t i = ARRAY_SIZE(b06wi_ip_routes); i-- > 0;) {
		const struct b06wi_route32 *route = &b06wi_ip_routes[i];
		uint32_t value;

		if (!route->mask)
			continue;
		value = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + route->reg);
		value = (value & ~route->mask) | (route->pre & route->mask);
		write32p(CONFIG_FIXED_RCBA_MMIO_BASE + route->reg, value);
		ok &= read32p(CONFIG_FIXED_RCBA_MMIO_BASE + route->reg) == route->pre;
	}
	ok &= b06wi_ioapic_write(B06WI_IOAPIC_ID_REG, B06WI_IOAPIC_ID_PRE);
	ok &= b06wi_ioapic_select(B06WI_IOAPIC_ID_REG);
	return ok;
}

static __noreturn void b06wi_fail(const char *reason, bool mutated)
{
	const bool rolled_back = !mutated || b06wi_rollback();

	/* IOREGSEL is an address selector, not a route; always restore index zero. */
	write32p(B06WI_IOAPIC_BASE + B06WI_IOREGSEL, B06WI_IOAPIC_ID_REG);
	printk(BIOS_ERR,
	       "[IRQ] %s FAIL reason=%s MUTATED=%u ROLLBACK=%u "
	       "PIRQ_WRITE=0 GPIO_WRITE=0 INT_LINE_WRITE=0 RTE_WRITE=0\n",
	       B06WI_STAGE_ID, reason, mutated, rolled_back);
	die_with_post_code(POST_B06WI_FAIL,
		"[IRQ] B06WI exact route experiment blocked before PCI scan\n");
}

void b06wi_program_vendor_irq_once(void)
{
	const uint32_t lpc_id = pci_io_read_config32(B06WI_LPC_DEV,
		PCI_VENDOR_ID);
	const uint32_t rcba = pci_io_read_config32(B06WI_LPC_DEV, RCBA);
	const uint32_t gpio_rout = pci_io_read_config32(B06WI_LPC_DEV,
		D31F0_GPIO_ROUT);
	const uint8_t oic = read8p(CONFIG_FIXED_RCBA_MMIO_BASE + OIC);

	if (b06wi_attempted)
		b06wi_fail("SECOND_ATTEMPT", false);
	b06wi_attempted = true;
	post_code(POST_B06WI_BEGIN);
	printk(BIOS_NOTICE,
	       "[IRQ] %s PRE ID=%08x RCBA=%08x OIC=%02x GPIO_ROUT=%08x\n",
	       B06WI_STAGE_ID, lpc_id, rcba, oic, gpio_rout);

	if (lpc_id != B06WI_LPC_ID || rcba != B06WI_RCBA_ENABLED)
		b06wi_fail("LPC_RCBA", false);
	if (oic != B06WI_OIC_TARGET || gpio_rout != 0)
		b06wi_fail("DECODE_OR_GPIO_ROUT", false);
	if (!b06wi_pirq_is_disabled())
		b06wi_fail("PIRQ_NOT_ALL_DISABLED", false);
	if (read32p(B06WI_IOAPIC_BASE + B06WI_IOREGSEL) !=
	    B06WI_IOAPIC_ID_REG)
		b06wi_fail("IOAPIC_SELECTOR_PRE", false);
	if (!b06wi_ioapic_is_canonical(B06WI_IOAPIC_ID_PRE))
		b06wi_fail("IOAPIC_PRE", false);
	if (!b06wi_route_tuple(false))
		b06wi_fail("RCBA_ROUTE_PRE", false);
	if (!b06wi_pin_tuple(false))
		b06wi_fail("PCI_PIN_PRE", false);

	if (!b06wi_write_targets())
		b06wi_fail("WRITE_READBACK", true);
	post_code(POST_B06WI_WRITTEN);
	if (!b06wi_route_tuple(true) || !b06wi_pin_tuple(true))
		b06wi_fail("ROUTE_OR_PIN_POST", true);
	if (!b06wi_ioapic_is_canonical(B06WI_IOAPIC_ID_TARGET))
		b06wi_fail("IOAPIC_POST", true);
	if (!b06wi_pirq_is_disabled() ||
	    pci_io_read_config32(B06WI_LPC_DEV, D31F0_GPIO_ROUT) != 0)
		b06wi_fail("FORBIDDEN_STATE_CHANGED", true);

	b06wi_ready = true;
	post_code(POST_B06WI_READY);
	printk(BIOS_NOTICE,
	       "[IRQ] %s READY D31IR=0232 D29IR=0237 D28IR=3201 "
	       "D27IR=3216 D26IR=3250 D26F2_PIN=D IOAPIC_ID=1 "
	       "RTE_MASKED=24 PIRQ_DISABLED=8 PIRQ_WRITE=0 GPIO_WRITE=0 "
	       "INT_LINE_WRITE=0 RTE_WRITE=0\n", B06WI_STAGE_ID);
}

bool b06wi_vendor_irq_ready(void)
{
	return b06wi_ready && b06wi_route_tuple(true) && b06wi_pin_tuple(true) &&
		b06wi_pirq_is_disabled() &&
		pci_io_read_config32(B06WI_LPC_DEV, D31F0_GPIO_ROUT) == 0 &&
		b06wi_ioapic_is_canonical(B06WI_IOAPIC_ID_TARGET);
}
