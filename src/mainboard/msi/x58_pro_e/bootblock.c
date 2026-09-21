/* SPDX-License-Identifier: GPL-2.0-only */

#include <arch/io.h>
#include <arch/pci_io_cfg.h>
#include <bootblock_common.h>
#include <commonlib/bsd/compiler.h>
#include <commonlib/helpers.h>
#include <console/uart.h>
#include <device/pci_def.h>
#include <device/pci_type.h>
#include <device/pnp.h>
#include <device/pnp_ops.h>
#include <device/pnp_type.h>
#include <drivers/uart/uart8250reg.h>
#include <stdbool.h>
#include <southbridge/intel/common/lpc_def.h>
#include <superio/fintek/common/fintek.h>

#define POST_B00_C_ENVIRONMENT	0xc0
#define POST_B01_ICH10R_BEGIN	0xc1
#define POST_X58_BOOTBLOCK_LPC_BEGIN	0xc2
#define POST_B01_ICH10R_FOUND	0xd0
#define POST_X58_BOOTBLOCK_FINTEK_BEGIN	0xd1
#define POST_X58_BOOTBLOCK_FINTEK_FOUND	0xd2
#define POST_X58_BOOTBLOCK_UART_ENABLED	0xd3
#define POST_X58_BOOTBLOCK_UART_SENT	0xd4
#define POST_X58_EARLY_UART_ROMSTAGE_HANDOFF 0xc3
#define POST_B00_STOP		0xdf
#define POST_B01_NO_PCI_DEVICE	0xe1
#define POST_B01_DEVICE_ERROR	0xe2
#define POST_X58_BOOTBLOCK_FINTEK_ID_ERROR	0xe3
#define POST_X58_BOOTBLOCK_FINTEK_VID_ERROR 0xe4
#define POST_X58_BOOTBLOCK_LPC_ERROR	0xe5
#define POST_X58_BOOTBLOCK_UART_LDN_ERROR	0xe6
#define POST_X58_BOOTBLOCK_UART_REG_ERROR	0xe7
#define POST_X58_BOOTBLOCK_UART_TX_ERROR	0xed

#define PCI_VENDOR_ID_INTEL	0x8086
#define PCI_DEVICE_ID_ICH10R	0x3a16
#define PCI_ID_ICH10R_LPC	((PCI_DEVICE_ID_ICH10R << 16) | PCI_VENDOR_ID_INTEL)
#define ICH10R_LPC_DEV		PCI_DEV(0, 0x1f, 0)

#define X58_BOOTBLOCK_LPC_IO_DEC		0x0010
#define X58_BOOTBLOCK_LPC_EN		(CNF2_LPC_EN | COMA_LPC_EN)

#define FINTEK_CONFIG_PORT	0x4e
#define FINTEK_UART_LDN		0x01
#define FINTEK_DEVICE_ID0_REG	0x20
#define FINTEK_DEVICE_ID1_REG	0x21
#define FINTEK_VENDOR_ID0_REG	0x23
#define FINTEK_VENDOR_ID1_REG	0x24
#define FINTEK_DEVICE_ID0	0x05
#define FINTEK_DEVICE_ID1	0x41
#define FINTEK_VENDOR_ID0	0x19
#define FINTEK_VENDOR_ID1	0x34
#define FINTEK_UART_BASE		0x3f8

#define UART_TX_POLL_LIMIT	100000
#define UART_FLUSH_POLL_LIMIT	1000000

struct x58_bootblock_lpc_snapshot {
	uint16_t io_dec;
	uint16_t enables;
	uint32_t generic[4];
};

static void __noreturn stop_with_post(uint8_t code)
{
	outb(code, CONFIG_POST_IO_PORT);
	asm volatile (
		"cli\n\t"
		"1: hlt\n\t"
		"jmp 1b"
	);
	__builtin_unreachable();
}

static void probe_ich10r_lpc(void)
{
	uint32_t id;

	outb(POST_B01_ICH10R_BEGIN, CONFIG_POST_IO_PORT);
	id = pci_io_read_config32(ICH10R_LPC_DEV, PCI_VENDOR_ID);

	if (id == UINT32_MAX)
		stop_with_post(POST_B01_NO_PCI_DEVICE);
	if (id != PCI_ID_ICH10R_LPC)
		stop_with_post(POST_B01_DEVICE_ERROR);

	outb(POST_B01_ICH10R_FOUND, CONFIG_POST_IO_PORT);
}

static void configure_narrow_lpc(struct x58_bootblock_lpc_snapshot *before)
{
	static const uint16_t generic_regs[] = {
		LPC_GEN1_DEC, LPC_GEN2_DEC, LPC_GEN3_DEC, LPC_GEN4_DEC,
	};
	size_t i;

	outb(POST_X58_BOOTBLOCK_LPC_BEGIN, CONFIG_POST_IO_PORT);

	before->io_dec = pci_io_read_config16(ICH10R_LPC_DEV, LPC_IO_DEC);
	before->enables = pci_io_read_config16(ICH10R_LPC_DEV, LPC_EN);
	for (i = 0; i < ARRAY_SIZE(generic_regs); i++)
		before->generic[i] =
			pci_io_read_config32(ICH10R_LPC_DEV, generic_regs[i]);
	for (i = 0; i < ARRAY_SIZE(before->generic); i++) {
		if (before->generic[i] & LPC_LGIR_EN)
			stop_with_post(POST_X58_BOOTBLOCK_LPC_ERROR);
	}

	/*
	 * ICH10R EDS LPC_IO_DEC/LPC_EN: COMA selector 000 is 0x3f8;
	 * enable only COMA and the second Super-I/O configuration range.
	 */
	pci_io_write_config16(ICH10R_LPC_DEV, LPC_IO_DEC, X58_BOOTBLOCK_LPC_IO_DEC);
	if (pci_io_read_config16(ICH10R_LPC_DEV, LPC_IO_DEC) != X58_BOOTBLOCK_LPC_IO_DEC)
		stop_with_post(POST_X58_BOOTBLOCK_LPC_ERROR);

	pci_io_write_config16(ICH10R_LPC_DEV, LPC_EN, X58_BOOTBLOCK_LPC_EN);
	if (pci_io_read_config16(ICH10R_LPC_DEV, LPC_EN) != X58_BOOTBLOCK_LPC_EN)
		stop_with_post(POST_X58_BOOTBLOCK_LPC_ERROR);
}

static void identify_fintek(void)
{
	const pnp_devfn_t uart = PNP_DEV(FINTEK_CONFIG_PORT, FINTEK_UART_LDN);
	uint8_t device_id0;
	uint8_t device_id1;
	uint8_t vendor_id0;
	uint8_t vendor_id1;

	outb(POST_X58_BOOTBLOCK_FINTEK_BEGIN, CONFIG_POST_IO_PORT);
	pnp_enter_conf_state(uart);
	device_id0 = pnp_read_config(uart, FINTEK_DEVICE_ID0_REG);
	device_id1 = pnp_read_config(uart, FINTEK_DEVICE_ID1_REG);
	vendor_id0 = pnp_read_config(uart, FINTEK_VENDOR_ID0_REG);
	vendor_id1 = pnp_read_config(uart, FINTEK_VENDOR_ID1_REG);
	pnp_exit_conf_state(uart);

	if (device_id0 != FINTEK_DEVICE_ID0 ||
	    device_id1 != FINTEK_DEVICE_ID1)
		stop_with_post(POST_X58_BOOTBLOCK_FINTEK_ID_ERROR);
	if (vendor_id0 != FINTEK_VENDOR_ID0 ||
	    vendor_id1 != FINTEK_VENDOR_ID1)
		stop_with_post(POST_X58_BOOTBLOCK_FINTEK_VID_ERROR);

	outb(POST_X58_BOOTBLOCK_FINTEK_FOUND, CONFIG_POST_IO_PORT);
}

static void configure_fintek_uart(void)
{
	const pnp_devfn_t uart = PNP_DEV(FINTEK_CONFIG_PORT, FINTEK_UART_LDN);
	uint8_t enabled;
	uint16_t iobase;

	/* Common Fintek code touches only LDN 1 enable and IO0 registers. */
	fintek_enable_serial(uart, FINTEK_UART_BASE);

	pnp_enter_conf_state(uart);
	pnp_set_logical_device(uart);
	enabled = pnp_read_config(uart, PNP_IDX_EN);
	iobase = pnp_read_iobase(uart, PNP_IDX_IO0);
	pnp_exit_conf_state(uart);

	if (enabled != 1 || iobase != FINTEK_UART_BASE)
		stop_with_post(POST_X58_BOOTBLOCK_UART_LDN_ERROR);

	outb(POST_X58_BOOTBLOCK_UART_ENABLED, CONFIG_POST_IO_PORT);
}

static bool uart_wait_for(uint8_t mask, unsigned int limit);

static void test_uart_registers(void)
{
	uint8_t original = inb(FINTEK_UART_BASE + UART8250_SCR);
	bool passed;

	outb(0x5a, FINTEK_UART_BASE + UART8250_SCR);
	passed = inb(FINTEK_UART_BASE + UART8250_SCR) == 0x5a;
	outb(0xa5, FINTEK_UART_BASE + UART8250_SCR);
	passed &= inb(FINTEK_UART_BASE + UART8250_SCR) == 0xa5;
	outb(original, FINTEK_UART_BASE + UART8250_SCR);

	if (!passed)
		stop_with_post(POST_X58_BOOTBLOCK_UART_REG_ERROR);

	/* Internal loopback proves the UART block without relying on JCOM1. */
	outb(UART8250_FCR_FIFO_EN | UART8250_FCR_CLEAR_RCVR |
	     UART8250_FCR_CLEAR_XMIT, FINTEK_UART_BASE + UART8250_FCR);
	outb(UART8250_MCR_DTR | UART8250_MCR_RTS | UART8250_MCR_LOOP,
	     FINTEK_UART_BASE + UART8250_MCR);
	passed = uart_wait_for(UART8250_LSR_THRE, UART_TX_POLL_LIMIT);
	if (passed) {
		outb(0x5a, FINTEK_UART_BASE + UART8250_TBR);
		passed = uart_wait_for(UART8250_LSR_DR, UART_TX_POLL_LIMIT);
	}
	if (passed)
		passed = inb(FINTEK_UART_BASE + UART8250_RBR) == 0x5a;
	outb(UART8250_MCR_DTR | UART8250_MCR_RTS,
	     FINTEK_UART_BASE + UART8250_MCR);
	outb(UART8250_FCR_FIFO_EN | UART8250_FCR_CLEAR_RCVR |
	     UART8250_FCR_CLEAR_XMIT, FINTEK_UART_BASE + UART8250_FCR);

	if (!passed)
		stop_with_post(POST_X58_BOOTBLOCK_UART_REG_ERROR);
}

static bool uart_wait_for(uint8_t mask, unsigned int limit)
{
	while (limit--) {
		if ((inb(FINTEK_UART_BASE + UART8250_LSR) & mask) == mask)
			return true;
	}
	return false;
}

static bool uart_putc(uint8_t value)
{
	if (!uart_wait_for(UART8250_LSR_THRE, UART_TX_POLL_LIMIT))
		return false;
	outb(value, FINTEK_UART_BASE + UART8250_TBR);
	return true;
}

static bool uart_puts(const char *string)
{
	while (*string) {
		if (!uart_putc(*string++))
			return false;
	}
	return true;
}

static bool uart_put_hex(uint32_t value, unsigned int digits)
{
	static const char hex[] = "0123456789abcdef";

	while (digits--) {
		unsigned int shift = digits * 4;

		if (!uart_putc(hex[(value >> shift) & 0xf]))
			return false;
	}
	return true;
}

static bool send_x58_rommon_bootblock_banner(const struct x58_bootblock_lpc_snapshot *before)
{
	size_t i;

	if (!uart_puts("\r\n[") || !uart_puts(CONFIG_MAINBOARD_PART_NUMBER) ||
	    !uart_puts("]\r\nICH10R=8086:3a16\r\nLPC_PRE IO="))
		return false;
	if (!uart_put_hex(before->io_dec, 4) || !uart_puts(" EN=") ||
	    !uart_put_hex(before->enables, 4))
		return false;

	for (i = 0; i < ARRAY_SIZE(before->generic); i++) {
		if (!uart_puts(" G") || !uart_put_hex(i + 1, 1) ||
		    !uart_putc('=') || !uart_put_hex(before->generic[i], 8))
			return false;
	}

	return uart_puts("\r\nLPC_POST IO=0010 EN=2001\r\n"
			 "FINTEK DID=4105 VID=3419 LDN1=03f8\r\n"
			 "UART=03f8 115200 8N1 CAR\r\n"
			 "POST=c3 CBFS_ROMSTAGE_HANDOFF\r\n") &&
		uart_wait_for(UART8250_LSR_TEMT, UART_FLUSH_POLL_LIMIT);
}

static void bring_up_x58_bootblock_uart(const struct x58_bootblock_lpc_snapshot *before)
{
	configure_fintek_uart();
	uart_init(CONFIG_UART_FOR_CONSOLE);
	test_uart_registers();

	if (!send_x58_rommon_bootblock_banner(before))
		stop_with_post(POST_X58_BOOTBLOCK_UART_TX_ERROR);

	outb(POST_X58_BOOTBLOCK_UART_SENT, CONFIG_POST_IO_PORT);
}

void bootblock_mainboard_early_init(void)
{
	outb(POST_B00_C_ENVIRONMENT, CONFIG_POST_IO_PORT);

	probe_ich10r_lpc();

	struct x58_bootblock_lpc_snapshot before;

	configure_narrow_lpc(&before);
	identify_fintek();
	bring_up_x58_bootblock_uart(&before);

}

void bootblock_mainboard_init(void)
{
	/* The normal bootblock calls run_romstage() immediately after this hook. */
	outb(POST_X58_EARLY_UART_ROMSTAGE_HANDOFF, CONFIG_POST_IO_PORT);
}
