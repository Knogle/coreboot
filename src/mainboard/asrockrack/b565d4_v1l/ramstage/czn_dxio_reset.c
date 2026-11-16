/* SPDX-License-Identifier: GPL-2.0-only */
#include "czn_dxio.h"
#include <arch/io.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <console/uart.h>
#include <device/mmio.h>
#include <amdblocks/smbus.h>

/* Original board register ABI: desktop OEM 2E3C, Cezanne OEM 588D.
 * The package-specific pin numbers and mux functions differ even on AM4.
 * This is not an expander part-number claim.
 * Root selects this intrusive trial. Other bus agents must not use ASF while
 * these transactions own it. Register restoration cannot undo a reset edge,
 * expander output/direction change, or acknowledged W1C status.
 */
#define PM300 0xfed80300U
#define PM302 0xfed80302U
#define MUX(gpio) (0xfed80d00U + (gpio))
#define GPIO_BYTE(gpio) (0xfed81502U + 4U * (gpio))
#define GPIO26_CONTROL 0xfed81568U
#define READY_POLLS 100U
#define COMPLETE_POLLS 100000U
#define DIRECTION_POLLS 16U

static unsigned int preflight_state, prelude_state, reset_failed, detect_level;
static uint16_t asf_base;

static unsigned int detect_gpio(void)
{
	return CONFIG(B565D4_DESKTOP_NATIVE) ? 143 : 70;
}

static unsigned int reset_gpio(unsigned int group)
{
	if (CONFIG(B565D4_DESKTOP_NATIVE)) {
		switch (group) {
		case 2: return 140;
		case 8: return 136;
		case 7: return 138;
		case 6: return 141;
		case 5: return 137;
		}
	} else {
		switch (group) {
		case 2: return 140;
		case 8: return 69;
		case 7: return 143;
		case 6: return 142;
		case 5: return 141;
		}
	}
	return 0;
}

static unsigned int gpio_mux(unsigned int gpio)
{
	if (!CONFIG(B565D4_DESKTOP_NATIVE))
		return 0;
	return gpio == 140 || gpio == 137 ? 2 : 1;
}

static int reset_error(const char *why)
{
	reset_failed = 1;
	printk(BIOS_ERR, "NATIVE109 RESET REFUSE %s preflight=%u once=%u\n",
	       why, preflight_state, prelude_state);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	return 0;
}

static int mmio_byte(const char *label, uint32_t address, uint8_t value,
		     uint8_t mask)
{
	printk(BIOS_INFO, "NATIVE109 MMIO BEGIN %s address=%08x value=%02x\n",
	       label, address, value);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	uint8_t before = read8p(address);
	write8p(address, value);
	uint8_t after = read8p(address);
	native106_dxio_io(label, address, before, after);
	return (after & mask) == (value & mask);
}

static int desktop_gpio_init(void)
{
	/* Selected original 2E3C $A10 rows: detector143=0982, reset outputs
	 * 136/141/138=0981 and 140/137=0a81. CPM writes the control byte before
	 * the mux byte. Do not use a DWORD RMW over GPIO's W1C status bits.
	 */
	static const unsigned int pins[] = { 143, 136, 141, 140, 138, 137 };

	for (unsigned int i = 0; i < ARRAY_SIZE(pins); i++) {
		unsigned int gpio = pins[i];
		uint8_t before = read8p(GPIO_BYTE(gpio));
		uint8_t mux = read8p(MUX(gpio));

		if (!native106_dxio_budget_ok() || before == 0xff || mux == 0xff)
			return reset_error("desktop GPIO initialization access");
		uint8_t value = gpio == detect_gpio() ? before & 0x47 :
			(before & 0xc7) | 0xc0;

		if (!mmio_byte("desktop GPIO initial control", GPIO_BYTE(gpio), value, 0xfe) ||
		    !mmio_byte("desktop GPIO initial mux", MUX(gpio), gpio_mux(gpio), 0xff))
			return reset_error("desktop GPIO initialization readback");
	}
	return 1;
}

static int io_byte(const char *label, uint16_t port, uint8_t value, uint8_t mask)
{
	uint8_t before = inb(port);
	outb(value, port);
	uint8_t after = inb(port);
	native106_dxio_io(label, port, before, after);
	return (after & mask) == (value & mask);
}

static int poll_io(uint16_t port, uint8_t mask, uint8_t expected,
		   unsigned int maximum)
{
	for (unsigned int i = 0; i < maximum; i++) {
		if (!native106_dxio_budget_ok())
			return 0;
		uint8_t value = inb(port);
		if (value == 0xff)
			return 0;
		if ((value & mask) == expected)
			return 1;
		if (!native106_dxio_delay(1))
			return 0;
	}
	return 0;
}

static int asf_ready(void)
{
	uint16_t decode = read16p(PM300);
	uint8_t mux19 = read8p(MUX(19)), mux20 = read8p(MUX(20));
	uint16_t base = (decode & 0xff00U) + 0x20U;
	if (decode == 0xffff || !(decode & 0xff00U) || !(decode & 0x10) ||
	    base != asf_base || mux19 != 0 || mux20 != 0)
		return reset_error("ASF decode/base or SCL1/SDA1 mux");
	uint8_t host = inb(base + SMBHSTSTAT), slave = inb(base + SMBSLVSTAT);
	uint8_t control = inb(base + SMBHSTCTRL);
	printk(BIOS_INFO, "NATIVE109 ASF base=%04x PM300=%04x mux19/20=%02x/%02x "
	       "host/slave/ctrl=%02x/%02x/%02x\n",
	       base, decode, mux19, mux20, host, slave, control);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	/* Do not clear somebody else's pending/busy state to create readiness.
	 * Reject saved START/KILL so cleanup cannot replay an old transaction.
	 */
	if (host == 0xff || slave == 0xff || control == 0xff ||
	    (host & 0x1f) || (slave & 0x3f) || (control & 0x42))
		return reset_error("ASF controller not idle before transaction");
	return 1;
}

static int asf_byte(unsigned int read, uint8_t command, uint8_t *value)
{
	const uint16_t b = asf_base;
	uint8_t saved_pm, saved_control, saved_slave, saved_command, saved_address, saved_data;
	int ok = 0, restored = 1, started = 0, claimed = 0;

	printk(BIOS_INFO, "NATIVE109 ASF %s reg%u CHECK base=%04x\n",
	       read ? "READ" : "WRITE", command, b);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (!native106_dxio_budget_ok() || !asf_ready())
		return 0;
	saved_pm = read8p(PM302);
	saved_control = inb(b + SMBHSTCTRL);
	saved_slave = inb(b + SMBSLVCTRL);
	saved_command = inb(b + SMBHSTCMD);
	saved_address = inb(b + SMBHSTADDR);
	saved_data = inb(b + SMBHSTDAT0);
	printk(BIOS_INFO, "NATIVE109 ASF %s slave20 reg%u PM302=%02x "
	       "ctrl=%02x slvctrl=%02x BEGIN\n", read ? "READ" : "WRITE",
	       command, saved_pm, saved_control, saved_slave);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (!mmio_byte("PM302 set bit0", PM302, saved_pm | 1, 0xff))
		goto out;
	if (!io_byte("ASF ctrl clear bit7", b + SMBHSTCTRL, saved_control & 0x7f, 0xff))
		goto out;
	claimed = 1;
	if (!io_byte("ASF slave set bit4", b + SMBSLVCTRL, saved_slave | 0x10, 0x10) ||
	    !poll_io(b + SMBSLVCTRL, 0x10, 0x10, READY_POLLS))
		goto out;
	if ((inb(b + SMBHSTSTAT) & 0x1f) || (inb(b + SMBSLVSTAT) & 0x3f))
		goto out;
	/* Exact original byte-data register protocol, with no collision retry. */
	outb(0x1f, b + SMBHSTSTAT);
	outb(0x3f, b + SMBSLVSTAT);
	outb(command, b + SMBHSTCMD);
	outb(0x40 | !!read, b + SMBHSTADDR);
	/* Original controller1 stages byte-data mode and reads control before
	 * writing DATA0, then issues START separately. No transaction retry.
	 */
	outb(0x08, b + SMBHSTCTRL);
	uint8_t staged_control = inb(b + SMBHSTCTRL);
	if (!read)
		outb(*value, b + SMBHSTDAT0);
	uint8_t staged_command = inb(b + SMBHSTCMD);
	uint8_t staged_address = inb(b + SMBHSTADDR);
	uint8_t staged_data = inb(b + SMBHSTDAT0);
	printk(BIOS_INFO, "NATIVE109 ASF PRESTART read=%u CMD=%02x expected=%02x "
	       "ADDR=%02x expected=%02x CTRL_before_DATA0=%02x "
	       "DATA0=%02x requested=%02x\n", read, staged_command, command,
	       staged_address, 0x40 | !!read, staged_control, staged_data, *value);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (!native106_dxio_budget_ok())
		goto out;
	started = 1;
	outb(0x48, b + SMBHSTCTRL);
	for (unsigned int i = 0; i < COMPLETE_POLLS; i++) {
		if (!native106_dxio_budget_ok())
			break;
		uint8_t status = inb(b + SMBHSTSTAT);
		if (status == 0xff || (status & SMBHST_STAT_ERROR_BITS))
			break;
		if (!(status & SMBHST_STAT_BUSY) && (status & SMBHST_STAT_INTERRUPT)) {
			if (read)
				*value = inb(b + SMBHSTDAT0);
			ok = 1;
			break;
		}
		if (!native106_dxio_delay(1))
			break;
	}
out:
	/* Cleanup remains reachable after budget expiry. Only this transaction's
	 * START may be killed; no automatic recovery/restart attempt follows.
	 */
	if (started) {
		uint8_t status = inb(b + SMBHSTSTAT);
		native106_dxio_io("ASF completion status", b + SMBHSTSTAT, status, status);
		if (status & SMBHST_STAT_BUSY) {
			outb(SMBHST_CTRL_KILL, b + SMBHSTCTRL);
			if (!poll_io(b + SMBHSTSTAT, SMBHST_STAT_BUSY, 0, READY_POLLS))
				restored = 0;
		}
		outb(0x1f, b + SMBHSTSTAT);
	}
	if (claimed)
		outb(inb(b + SMBSLVCTRL) | 0x20, b + SMBSLVCTRL);
	/* CMD/address/data are host-controller staging registers, not a second
	 * expander write. START is never restored as one by the idle admission.
	 */
	outb(saved_command, b + SMBHSTCMD);
	outb(saved_address, b + SMBHSTADDR);
	outb(saved_data, b + SMBHSTDAT0);
	restored &= io_byte("ASF slave restore", b + SMBSLVCTRL, saved_slave, 0xff);
	restored &= io_byte("ASF ctrl restore", b + SMBHSTCTRL, saved_control, 0xff);
	restored &= mmio_byte("PM302 restore", PM302, saved_pm, 0xff);
	printk(BIOS_INFO, "NATIVE109 ASF END ok=%u restored=%u started=%u value=%02x\n",
	       ok, restored, started, *value);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	return ok && restored && native106_dxio_budget_ok();
}

static int expander(unsigned int group, unsigned int level)
{
	const uint8_t bit = 1U << (group - 8);
	uint8_t direction = 0, output = 0;
	if (!asf_byte(1, 3, &direction))
		return reset_error("Type6 direction read");
	if (direction & bit) {
		direction &= ~bit;
		if (!asf_byte(0, 3, &direction))
			return reset_error("Type6 direction write");
		unsigned int i;
		for (i = 0; i < DIRECTION_POLLS; i++) {
			if (!asf_byte(1, 3, &direction))
				return reset_error("Type6 direction readback");
			if (!(direction & bit))
				break;
			if (!native106_dxio_delay(10))
				return reset_error("Type6 direction deadline");
		}
		if (i == DIRECTION_POLLS)
			return reset_error("Type6 direction did not clear");
	}
	if (!asf_byte(1, 1, &output))
		return reset_error("Type6 output read");
	uint8_t wanted = level ? output | bit : output & ~bit;
	if (!asf_byte(0, 1, &wanted))
		return reset_error("Type6 output write");
	uint8_t after = 0;
	if (!asf_byte(1, 1, &after))
		return reset_error("Type6 output diagnostic read");
	/* Original returns after the acknowledged write without a value check.
	 * Keep this extra bus read diagnostic; its transaction error is fatal.
	 */
	printk(BIOS_INFO, "NATIVE109 Type6 reg1 group=%02x wanted=%02x "
	       "observed=%02x bit_match=%u diagnostic_only=1\n",
	       group, wanted, after, (after & bit) == (wanted & bit));
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	native106_dxio_io("Type6 output reg1 observation", group, output, after);
	return 1;
}

static int gpio_output(unsigned int gpio, unsigned int level)
{
	const uint32_t address = GPIO_BYTE(gpio);
	printk(BIOS_INFO, "NATIVE109 GPIO%u level%u BEGIN\n", gpio, level);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (!native106_dxio_budget_ok() || read8p(MUX(gpio)) != gpio_mux(gpio))
		return reset_error("reset GPIO mux/budget");
	uint8_t before = read8p(address);
	uint8_t wanted = (before | 0x80U) & ~0x40U;
	if (level)
		wanted |= 0x40;
	return mmio_byte("SoC reset GPIO output", address, wanted, 0xc0) ||
		reset_error("reset GPIO readback");
}

static int reset_action(unsigned int group, unsigned int level)
{
	switch (group) {
	case 2:
	case 8:
	case 7:
	case 6:
	case 5: return gpio_output(reset_gpio(group), level);
	case 0x0c:
	case 0x0d:
	case 0x0e: return expander(group, level);
	default: return reset_error("unselected reset group");
	}
}

int native106_dxio_reset_preflight(void)
{
	static const unsigned int groups[] = { 8, 2, 7, 5, 6 };
	const unsigned int detector = detect_gpio();
	if (preflight_state || reset_failed || !native106_dxio_budget_ok())
		return reset_error("reset preflight order/budget");
	preflight_state = 1;
	printk(BIOS_INFO, "NATIVE109 RESET PREFLIGHT: Original %s ABI; "
	       "2bd native default0, group8 extra-pulse policy0; expander identity UNKNOWN\n",
	       CONFIG(B565D4_DESKTOP_NATIVE) ? "desktop2E3C" : "Cezanne588D");
	printk(BIOS_INFO, "NATIVE109 PREFLIGHT MMIO BEGIN detectGPIO%u mux=%08x "
	       "control=%08x GPIO-controls/MUX then PM300=%08x\n",
	       detector, MUX(detector), GPIO_BYTE(detector), PM300);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (CONFIG(B565D4_DESKTOP_NATIVE) && !desktop_gpio_init())
		return 0;
	if (read8p(MUX(detector)) != gpio_mux(detector) ||
	    read32p(GPIO_BYTE(detector) - 2) == 0xffffffffU)
		return reset_error("detect GPIO input route/read");
	detect_level = read8p(GPIO_BYTE(detector)) & 1;
	for (unsigned int i = 0; i < ARRAY_SIZE(groups); i++) {
		if (detect_level && i >= 3)
			break;
		unsigned int gpio = reset_gpio(groups[i]);
		uint8_t mux = read8p(MUX(gpio));
		uint8_t value = read8p(GPIO_BYTE(gpio));
		native106_dxio_io("reset GPIO preflight", GPIO_BYTE(gpio), mux, value);
		if (mux != gpio_mux(gpio) || value == 0xff)
			return reset_error("selected reset GPIO route/absent");
	}
	if (!detect_level) {
		uint16_t base = read16p(PM300) & 0xff00U;
		if (!base || base > 0xffc0)
			return reset_error("ASF base range");
		asf_base = base + 0x20;
		uint8_t direction = 0, output = 0;
		if (!asf_byte(1, 3, &direction) || !asf_byte(1, 1, &output))
			return reset_error("required Type6 provider preflight");
		printk(BIOS_INFO, "NATIVE109 Type6 slave20 dir3=%02x out1=%02x "
		       "channel1=%04x (historical0b20)\n", direction, output, asf_base);
	}
	printk(BIOS_INFO, "NATIVE109 RESET PREFLIGHT PASS GPIO%u=%u "
	       "extra groups6/5/c/d/e/2=%u\n", detector, detect_level, !detect_level);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	preflight_state = 2;
	return native106_dxio_budget_ok();
}

int native106_dxio_reset(unsigned int group, unsigned int level)
{
	static const unsigned int extra[] = { 6, 5, 0x0c, 0x0d, 0x0e, 2 };
	/* The three selected 144-byte engine records request only these groups.
	 * Prelude-only groups are handled privately, not accepted as new events.
	 */
	if (preflight_state != 2 || reset_failed || level > 1 ||
	    (group != 2 && group != 8 && group != 7) ||
	    !native106_dxio_budget_ok())
		return reset_error("reset call order/level/budget");
	printk(BIOS_INFO, "NATIVE109 RESET group=%02x level=%u once=%u BEGIN\n",
	       group, level, prelude_state);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (!prelude_state) {
		if (read8p(MUX(detect_gpio())) != gpio_mux(detect_gpio()) ||
		    (read8p(GPIO_BYTE(detect_gpio())) & 1) != detect_level)
			return reset_error("detect GPIO changed before once prelude");
		/* Consume before the first write, as original SetBool2bd, without
		 * pretending that 'attempt started' is already reset success.
		 */
		prelude_state = 1;
		int ok = mmio_byte("GPIO26 mux temporary1", MUX(26), 1, 0xff);
		if (ok) {
			printk(BIOS_INFO, "NATIVE109 MMIO BEGIN GPIO26 RMW address=%08x\n",
			       GPIO26_CONTROL);
			uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
			uint32_t before = read32p(GPIO26_CONTROL);
			uint32_t wanted = (before & 0xff8fffffU) | 0x00800000U |
				(level ? 0x00400000U : 0);
			write32p(GPIO26_CONTROL, wanted);
			uint32_t after = read32p(GPIO26_CONTROL);
			native106_dxio_io("GPIO26 prelude control", GPIO26_CONTROL, before, after);
			ok = (after & 0x00f00000U) == (wanted & 0x00f00000U);
		}
		/* Original final selection is PCIE_RST_L mux0, even on failure. */
		ok &= mmio_byte("GPIO26 final mux0", MUX(26), 0, 0xff);
		if (!ok || (read8p(GPIO_BYTE(detect_gpio())) & 1) != detect_level) {
			prelude_state = 3;
			return reset_error("GPIO26 write/readback or detect branch changed");
		}
		if (!detect_level) {
			for (unsigned int i = 0; i < sizeof(extra) / sizeof(extra[0]); i++) {
				if (!reset_action(extra[i], 1)) {
					prelude_state = 3;
					return 0;
				}
			}
		}
		prelude_state = 2;
	}
	if (prelude_state != 2 || !reset_action(group, level))
		return reset_error("reset action failed; no retry");
	/* Original AMD_PBS_SETUP+36 optional group8/group2 pulse is explicitly0
	 * for this source selection. It is not emulated by forcing2bd to1.
	 */
	printk(BIOS_INFO, "NATIVE109 RESET group=%02x level=%u COMPLETE once=%u "
	       "group8 extra-pulse policy0\n", group, level, prelude_state);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	return native106_dxio_budget_ok();
}
