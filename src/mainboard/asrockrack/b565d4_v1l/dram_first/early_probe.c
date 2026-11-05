/* SPDX-License-Identifier: GPL-2.0-only */
/* Early raw-console diagnostic: wrap bootblock_c_entry and remain before FCH. */

#include <arch/io.h>
#include <bootblock_common.h>
#include <cpu/cpu.h>
#include <stdint.h>

#define LPC_CFG_ADDRESS 0x8000a300 /* 00:14.3, PCI configuration mechanism 1 */
#define AST_PORT 0x4e
#define UART_BASE 0x3f8
#define IOMUX_BASE 0xfed80d00

asmlinkage void __wrap_bootblock_c_entry(uint64_t base_timestamp);


static uint32_t lpc_read_cfg(uint8_t reg)
{
	outl(LPC_CFG_ADDRESS | (reg & 0xfc), 0xcf8);
	return inl(0xcfc);
}

static void lpc_write_cfg(uint8_t reg, uint32_t value)
{
	outl(LPC_CFG_ADDRESS | (reg & 0xfc), 0xcf8);
	outl(value, 0xcfc);
}

static uint8_t lpc_read_cfg8(uint8_t reg)
{
	outl(LPC_CFG_ADDRESS | (reg & 0xfc), 0xcf8);
	return inb(0xcfc + (reg & 3));
}

static void lpc_write_cfg8(uint8_t reg, uint8_t value)
{
	outl(LPC_CFG_ADDRESS | (reg & 0xfc), 0xcf8);
	outb(value, 0xcfc + (reg & 3));
}

static uint8_t pm_io_read(uint8_t reg)
{
	outb(reg, 0xcd6);
	return inb(0xcd7);
}

static void pm_io_set(uint8_t reg, uint8_t bits)
{
	uint8_t value = pm_io_read(reg);

	outb(value | bits, 0xcd7);
}

static uint8_t iomux_read(uint8_t gpio)
{
	return *(volatile uint8_t *)(uintptr_t)(IOMUX_BASE + gpio);
}

static void setup_lpc_clocks(void)
{
	/* Vendor LPC clocks: D14F3xD1 bits 5/6, both PCD defaults are one. */
	lpc_write_cfg8(0xd1, lpc_read_cfg8(0xd1) | 0x60);
	/* Vendor mux0 when eMMC/SD is off: clock0, CLKRUN, clock1, SERIRQ. */
	*(volatile uint8_t *)(uintptr_t)(IOMUX_BASE + 74) = 0;
	*(volatile uint8_t *)(uintptr_t)(IOMUX_BASE + 88) = 0;
	*(volatile uint8_t *)(uintptr_t)(IOMUX_BASE + 75) = 0;
	*(volatile uint8_t *)(uintptr_t)(IOMUX_BASE + 87) = 0;
}

static uint8_t sio_read(uint16_t port, uint8_t reg)
{
	outb(reg, port);
	return inb(port + 1);
}

static void sio_write(uint16_t port, uint8_t reg, uint8_t value)
{
	outb(reg, port);
	outb(value, port + 1);
}

static uint16_t disable_known_nuvoton_uarts(uint16_t *uart_state)
{
	uint16_t id;

	*uart_state = UINT16_MAX;
	outb(0x87, 0x2e);
	outb(0x87, 0x2e);
	id = ((uint16_t)sio_read(0x2e, 0x20) << 8) | sio_read(0x2e, 0x21);
	if ((id & 0xfff0) == 0xd420 || (id & 0xfff0) == 0xc800) {
		*uart_state = 0;
		for (uint8_t ldn = 2; ldn <= 3; ldn++) {
			sio_write(0x2e, 0x07, ldn);
			sio_write(0x2e, 0x30, sio_read(0x2e, 0x30) & ~1);
			*uart_state |= (uint16_t)sio_read(0x2e, 0x30) << (8 * (ldn - 2));
		}
	}
	outb(0xaa, 0x2e);
	return id;
}

static void ast_enter(void)
{
	outb(0xa5, AST_PORT);
	outb(0xa5, AST_PORT);
}

/* Read-only SCU revision is used to qualify the AHB response. */
static uint32_t ast_read_ahb(uint32_t address)
{
	uint32_t value = 0;

	ast_enter();
	sio_write(AST_PORT, 0x07, 0x0d);
	sio_write(AST_PORT, 0x30, 1);
	for (unsigned int i = 0; i < 4; i++)
		sio_write(AST_PORT, 0xf0 + i, address >> (24 - 8 * i));
	sio_write(AST_PORT, 0xf8, (sio_read(AST_PORT, 0xf8) & 0xfc) | 2);
	(void)sio_read(AST_PORT, 0xfe);
	for (unsigned int i = 0; i < 4; i++)
		value = (value << 8) | sio_read(AST_PORT, 0xf4 + i);
	sio_write(AST_PORT, 0x30, 0);
	outb(0xaa, AST_PORT);
	return value;
}

static void ast_write_ahb(uint32_t address, uint32_t value)
{
	ast_enter();
	sio_write(AST_PORT, 0x07, 0x0d);
	sio_write(AST_PORT, 0x30, 1);
	for (unsigned int i = 0; i < 4; i++) {
		sio_write(AST_PORT, 0xf0 + i, address >> (24 - 8 * i));
		sio_write(AST_PORT, 0xf4 + i, value >> (24 - 8 * i));
	}
	sio_write(AST_PORT, 0xf8, (sio_read(AST_PORT, 0xf8) & 0xfc) | 2);
	sio_write(AST_PORT, 0xfe, 0xcf);
	sio_write(AST_PORT, 0x30, 0);
	outb(0xaa, AST_PORT);
}

static unsigned int ast_enable_uart(uint8_t *uart_f0)
{
	unsigned int enabled, base;

	ast_enter();
	sio_write(AST_PORT, 0x07, 2);
	sio_write(AST_PORT, 0x30, 0);
	sio_write(AST_PORT, 0x60, UART_BASE >> 8);
	sio_write(AST_PORT, 0x61, UART_BASE & 0xff);
	/* 24 MHz / 13 permits the usual divisor 1 at 115200 baud. */
	sio_write(AST_PORT, 0xf0, sio_read(AST_PORT, 0xf0) & ~2);
	sio_write(AST_PORT, 0x30, 1);
	enabled = sio_read(AST_PORT, 0x30);
	base = ((unsigned int)sio_read(AST_PORT, 0x60) << 8) |
		sio_read(AST_PORT, 0x61);
	*uart_f0 = sio_read(AST_PORT, 0xf0);
	outb(0xaa, AST_PORT);
	return (enabled & 1) && base == UART_BASE;
}

static void uart_init_raw(void)
{
	outb(0, UART_BASE + 1); /* no UART interrupts */
	outb(0x80, UART_BASE + 3); /* DLAB */
	/* ASPEED starts the baud counter when DLL is written: load DLH first. */
	outb(0, UART_BASE + 1);
	outb(1, UART_BASE);
	outb(3, UART_BASE + 3); /* 8N1 */
	outb(7, UART_BASE + 2); /* FIFO enable/reset */
	outb(3, UART_BASE + 4); /* DTR/RTS */
}

static void uart_byte_raw(uint8_t byte)
{
	/* Never hang forever on an unimplemented or contended UART. */
	for (unsigned int i = 0; i < 100000; i++) {
		uint8_t status = inb(UART_BASE + 5);
		if (status != 0xff && (status & 0x20)) {
			outb(byte, UART_BASE);
			return;
		}
	}
	outb(0xe8, 0x80);
}

static void uart_text_raw(const char *text)
{
	while (*text)
		uart_byte_raw(*text++);
}

static void uart_hex_raw(uint32_t value)
{
	static const char digits[] = "0123456789abcdef";

	for (int shift = 28; shift >= 0; shift -= 4)
		uart_byte_raw(digits[(value >> shift) & 15]);
}

static void uart_field_raw(const char *name, uint32_t value)
{
	uart_text_raw(name);
	uart_hex_raw(value);
}

static void report_lpc(void)
{
	uart_field_raw("\r\nLPC44=", lpc_read_cfg(0x44));
	uart_field_raw(" LPC48=", lpc_read_cfg(0x48));
	uart_field_raw(" LPCD0=", lpc_read_cfg(0xd0));
	uart_field_raw(" D1=", lpc_read_cfg8(0xd1));
	uart_field_raw(" PM00=", pm_io_read(0x00));
	uart_field_raw(" PM04=", pm_io_read(0x04));
	uart_field_raw(" PMEC=", pm_io_read(0xec));
	uart_field_raw("\r\nIOMUX74=", iomux_read(74));
	uart_field_raw(" 75=", iomux_read(75));
	uart_field_raw(" 88=", iomux_read(88));
	uart_field_raw(" 87=", iomux_read(87));
	uart_field_raw(" 104=", iomux_read(104));
	uart_field_raw(" 105=", iomux_read(105));
	uart_field_raw(" 106=", iomux_read(106));
	uart_field_raw(" 107=", iomux_read(107));
	uart_field_raw(" 109=", iomux_read(109));
	uart_field_raw(" 32=", iomux_read(32));
	uart_field_raw(" 21=", iomux_read(21));
}

static void report_ast(void)
{
	uart_field_raw("\r\nSCU84=", ast_read_ahb(0x1e6e2084));
	uart_field_raw(" SCU90=", ast_read_ahb(0x1e6e2090));
	uart_field_raw(" SCU94=", ast_read_ahb(0x1e6e2094));
	uart_field_raw(" HICRA=", ast_read_ahb(0x1e78909c));
	uart_field_raw("\r\nSCU2C=", ast_read_ahb(0x1e6e202c));
	uart_field_raw(" SCU4C=", ast_read_ahb(0x1e6e204c));
	uart_field_raw(" SRUART1=", ast_read_ahb(0x1e789160));
}

/* This deliberate loop needs no timer initialization or TSC calibration. */
static __attribute__((noinline)) void retry_delay(void)
{
	for (unsigned int i = 0; i < 10000000; i++)
		__asm__ __volatile__("pause");
}

asmlinkage void __wrap_bootblock_c_entry(uint64_t base_timestamp)
{
	uint16_t nct_id, nct_uart_state;
	uint32_t revision, scu84, scu90, signature;
	uint8_t uart_f0;

	(void)base_timestamp;
	outb(0xc5, 0x80);
	signature = cpuid_eax(1);
	/* Enable only legacy I/O and LPC through already available PM index/data. */
	pm_io_set(0x00, 1);
	pm_io_set(0xec, 1);
	/* Same decode-enable bit as enable_acpimmio_decode_pm04(), before IOMUX. */
	pm_io_set(0x04, 1 << 1);
	if ((lpc_read_cfg(0) & 0xffff) != 0x1022) {
		outb(0xe4, 0x80);
		for (;;)
			__asm__ __volatile__("cli; hlt");
	}
	/* Preserve ROM/SPI and existing routing while opening both SIO index pairs. */
	lpc_write_cfg(0x48, lpc_read_cfg(0x48) | 3 | (1 << 21));
	lpc_write_cfg(0x44, lpc_read_cfg(0x44) | (1 << 6));
	setup_lpc_clocks();
	/* Do not let the internal SoC UART range claim the external COM1 address. */
	*(volatile uint16_t *)(uintptr_t)0xfedc0020 = 0;
	nct_id = disable_known_nuvoton_uarts(&nct_uart_state);

	for (;;) {
		if (!ast_enable_uart(&uart_f0)) {
			outb(0xe7, 0x80);
			retry_delay();
			continue;
		}
		revision = ast_read_ahb(0x1e6e207c);
		if (revision != 0 && revision != UINT32_MAX) {
			/* Board-vendor AST2500PeiInit masks, already audited for C4. */
			ast_write_ahb(0x1e6e2000, 0x1688a8a8);
			scu84 = ast_read_ahb(0x1e6e2084);
			scu90 = ast_read_ahb(0x1e6e2090);
			ast_write_ahb(0x1e6e2084, (scu84 & 0xff00ffff) | 0x00ff0000);
			ast_write_ahb(0x1e6e2090, scu90 & 0xffffffcf);
			ast_write_ahb(0x1e6e2000, 0);
		}
		uart_init_raw();
		uart_text_raw("\r\nB565D4-V1L coreboot early raw probe C5 CPUID=");
		uart_hex_raw(signature);
		uart_text_raw(" NCT=");
		uart_hex_raw(nct_id);
		uart_field_raw(" NCT_UARTS=", nct_uart_state);
		uart_text_raw(" AST_REV=");
		uart_hex_raw(revision);
		uart_field_raw(" F0=", uart_f0);
		report_lpc();
		report_ast();
		uart_text_raw("\r\nBefore ECAM, early_cache, S3_EIP and AOAC; repeating.\r\n");
		outb(0xc6, 0x80);
		retry_delay();
	}
}
