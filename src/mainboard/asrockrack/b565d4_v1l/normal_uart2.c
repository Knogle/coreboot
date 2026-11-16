/* SPDX-License-Identifier: GPL-2.0-only */
/* Optional returning normal-board console initializer, not a firmware port. */
#include "b565d4_cpu.h"
#include <arch/cpu.h>
#include <arch/io.h>
#include <console/uart.h>
#include <drivers/uart/uart8250reg.h>
#include <halt.h>
#include <stdint.h>
#include <superio/aspeed/common/aspeed.h>
#include "normal_uart2.h"

#define UART_BASE 0x2f8
#define LPC_CONFIG 0x8000a300U
#define MAX_POLLS 100000U

void __wrap_uart_init(unsigned int idx);

static bool cpu_valid(void)
{
	const struct cpuid_result vendor = cpuid(0);
	return vendor.eax >= 1 && vendor.ebx == 0x68747541 &&
		vendor.edx == 0x69746e65 && vendor.ecx == 0x444d4163 &&
		b565d4_is_selected_cpu(cpuid_eax(1));
}

static uint32_t save_flags(void)
{
	uint32_t flags;
	asm volatile ("pushfl; popl %0; cli" : "=r" (flags) : : "memory");
	return flags;
}

static void restore_flags(uint32_t flags)
{
	asm volatile ("pushl %0; popfl" : : "r" (flags) : "memory", "cc");
}

static uint32_t lpc_cfg_read(uint8_t reg)
{
	outl(LPC_CONFIG | reg, 0xcf8);
	return inl(0xcfc);
}

static void lpc_cfg_write(uint8_t reg, uint32_t value)
{
	outl(LPC_CONFIG | reg, 0xcf8);
	outl(value, 0xcfc);
}

static void sio_enter(uint16_t port, uint8_t key)
{
	outb(key, port);
	outb(key, port);
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

static bool nuvoton_uarts(bool disable)
{
	sio_enter(0x2e, 0x87);
	uint16_t id = (uint16_t)sio_read(0x2e, 0x20) << 8;
	id |= sio_read(0x2e, 0x21);
	if ((id & 0xfff0) != 0xc800 && (id & 0xfff0) != 0xd420) {
		outb(0xaa, 0x2e);
		return false;
	}
	uint8_t selection = sio_read(0x2e, 7);
	bool passed = true;
	for (unsigned int ldn = 2; ldn <= 3; ldn++) {
		sio_write(0x2e, 7, ldn);
		uint8_t active = sio_read(0x2e, 0x30);
		if (disable)
			sio_write(0x2e, 0x30, active & ~1);
		uint8_t readback = sio_read(0x2e, 0x30);
		passed &= !(readback & 1) && (readback & ~1) == (active & ~1);
	}
	sio_write(0x2e, 7, selection);
	outb(0xaa, 0x2e);
	return passed;
}

/* Preserve the iLPC2AHB logical selection and activation byte. No generic
 * helper reset of that LDN is needed for this bounded single transaction.
 */
static uint32_t ast_ahb(uint32_t address, uint32_t value, bool write)
{
	sio_enter(0x4e, 0xa5);
	uint8_t selection = sio_read(0x4e, 7);
	sio_write(0x4e, 7, LDN_ILPC2AHB);
	uint8_t active = sio_read(0x4e, ACT_REG);
	sio_write(0x4e, ACT_REG, active | 1);
	for (unsigned int i = 0; i < 4; i++)
		sio_write(0x4e, LPC2AHB_ADD0_REG + i, address >> (24 - i * 8));
	uint8_t width = sio_read(0x4e, LPC2AHB_LEN_REG);
	sio_write(0x4e, LPC2AHB_LEN_REG, (width & ~3) | LPC2AHB_4_BYTE);
	if (write) {
		for (unsigned int i = 0; i < 4; i++)
			sio_write(0x4e, LPC2AHB_DAT0_REG + i, value >> (24 - i * 8));
		sio_write(0x4e, LPC2AHB_RW_REG, 0xcf);
	} else {
		(void)sio_read(0x4e, LPC2AHB_RW_REG);
		value = 0;
		for (unsigned int i = 0; i < 4; i++)
			value = (value << 8) | sio_read(0x4e, LPC2AHB_DAT0_REG + i);
	}
	sio_write(0x4e, LPC2AHB_LEN_REG, width);
	sio_write(0x4e, ACT_REG, active);
	sio_write(0x4e, 7, selection);
	outb(0xaa, 0x4e);
	return value;
}

static bool ast_uart(bool configure)
{
	sio_enter(0x4e, 0xa5);
	uint8_t selection = sio_read(0x4e, 7);
	sio_write(0x4e, 7, LDN_SUART2);
	uint8_t active = sio_read(0x4e, ACT_REG);
	uint8_t clock = sio_read(0x4e, 0xf0);
	bool passed = active != 0xff && clock != 0xff && !(clock & 2);
	if (configure && passed) {
		sio_write(0x4e, ACT_REG, active & ~1);
		sio_write(0x4e, 0x60, UART_BASE >> 8);
		sio_write(0x4e, 0x61, UART_BASE & 255);
		sio_write(0x4e, 0x70, 3);
		sio_write(0x4e, ACT_REG, active | 1);
	}
	passed &= (sio_read(0x4e, ACT_REG) & 1) &&
		sio_read(0x4e, 0x60) == (UART_BASE >> 8) &&
		sio_read(0x4e, 0x61) == (UART_BASE & 255);
	sio_write(0x4e, 7, selection);
	outb(0xaa, 0x4e);
	return passed;
}

static bool uart_ready(void)
{
	for (unsigned int i = 0; i < MAX_POLLS; i++) {
		uint8_t state = inb(UART_BASE + UART8250_LSR);
		if (state == 0xff)
			return false;
		if ((state & 0x60) == 0x60)
			return true;
	}
	return false;
}

static bool lpc_ready(void)
{
	return lpc_cfg_read(0) == 0x790e1022 && (lpc_cfg_read(0x48) & 3) == 3 &&
		(lpc_cfg_read(0x44) & (1U << 7));
}

bool b565d4_uart2_setup(void)
{
	if (!cpu_valid())
		return false;
	uint32_t flags = save_flags();
	uint32_t cf8 = inl(0xcf8);
	bool passed = false;
	if (lpc_cfg_read(0) != 0x790e1022)
		goto done;
	/* The generic lpc_enable_decode helper overwrites register44. Keep all
	 * existing decodes while opening UART2 and both qualified SIO indices.
	 */
	lpc_cfg_write(0x48, lpc_cfg_read(0x48) | 3);
	lpc_cfg_write(0x44, lpc_cfg_read(0x44) | (1U << 7));
	if (!lpc_ready() || !nuvoton_uarts(true))
		goto done;
	uint32_t revision = ast_ahb(ASPEED_SCU_BASE + SILICON_REV_ID_REG, 0, false);
	if (!revision || revision == UINT32_MAX || !ast_uart(true))
		goto done;
	/* Exact original AST PEIM masks, applied before the first LSR wait.
	 * Do not infer a new IO1/IO2 route or alter F0/global UART clocks.
	 */
	ast_ahb(ASPEED_SCU_BASE + PRO_KEY_REG, PRO_KEY_PASSWORD, true);
	uint32_t pins = ast_ahb(ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL2_REG, 0, false);
	ast_ahb(ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL2_REG,
		(pins & 0xff00ffff) | 0x00ff0000, true);
	pins = ast_ahb(ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL5_REG, 0, false);
	ast_ahb(ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL5_REG, pins & 0xffffffcf, true);
	ast_ahb(ASPEED_SCU_BASE + PRO_KEY_REG, 0, true);
	passed = ast_ahb(ASPEED_SCU_BASE + SILICON_REV_ID_REG, 0, false) == revision &&
		(ast_ahb(ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL2_REG, 0, false) & 0x00ff0000) == 0x00ff0000 &&
		!(ast_ahb(ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL5_REG, 0, false) & 0x30) &&
		uart_ready();
done:
	outl(cf8, 0xcf8);
	restore_flags(flags);
	return passed;
}

void __wrap_uart_init(unsigned int idx)
{
	if (!cpu_valid() || idx != 1 || uart_platform_base(idx) != UART_BASE ||
	    uart_get_baudrate_divisor() != 1 || CONFIG_TTYS0_LCS != 3) {
		outb(0xec, 0x80);
		halt();
	}
	uint32_t flags = save_flags();
	uint32_t cf8 = inl(0xcf8);
	bool qualified = lpc_ready() && nuvoton_uarts(false) && ast_uart(false);
	outl(cf8, 0xcf8);
	if (!qualified || !uart_ready()) {
		restore_flags(flags);
		outb(0xec, 0x80);
		halt();
	}
	/* Restore 8N1/DLAB=0 before IER; DLL starts the AST baud counter. */
	outb(3, UART_BASE + UART8250_LCR);
	outb(0, UART_BASE + UART8250_IER);
	outb(UART8250_FCR_FIFO_EN, UART_BASE + UART8250_FCR);
	outb(UART8250_MCR_DTR | UART8250_MCR_RTS, UART_BASE + UART8250_MCR);
	outb(UART8250_LCR_DLAB | 3, UART_BASE + UART8250_LCR);
	outb(0, UART_BASE + UART8250_DLM);
	outb(1, UART_BASE + UART8250_DLL);
	outb(3, UART_BASE + UART8250_LCR);
	restore_flags(flags);
}

#if ENV_RAMSTAGE
/* Only the post-ACK RAM helper uses ECAM. Default setup/uart_init keep their
 * original configuration provider. native87_check_ecam precedes this path.
 */
_Static_assert(CONFIG_ECAM_MMCONF_BASE_ADDRESS == 0xf0000000U &&
	CONFIG_ECAM_MMCONF_BUS_NUMBER == 128 &&
	CONFIG_ECAM_MMCONF_LENGTH == 0x08000000U, "Retained LPC ECAM mapping");
#define ACK38_LPC_ECAM 0xf00a3000U /* 00:14.3, base + (0xa3 << 12). */

static uint32_t ack38_lpc_read_ecam(uint8_t reg)
{
	if (reg != 0 && reg != 0x44 && reg != 0x48)
		return UINT32_MAX;
	return *(volatile uint32_t *)(uintptr_t)(ACK38_LPC_ECAM + reg);
}

static void ack38_lpc_write_ecam(uint8_t reg, uint32_t value)
{
	if (reg != 0x44 && reg != 0x48)
		return;
	*(volatile uint32_t *)(uintptr_t)(ACK38_LPC_ECAM + reg) = value;
}

static bool ack38_lpc_ready_ecam(void)
{
	return ack38_lpc_read_ecam(0) == 0x790e1022 &&
		(ack38_lpc_read_ecam(0x48) & 3) == 3 &&
		(ack38_lpc_read_ecam(0x44) & (1U << 7));
}

/* One selected post-ACK attempt. Provider and register masks are the existing
 * board initializer's. No logging, terminal, reset or recursive UART call.
 */
bool b565d4_uart2_ack38_requalify(struct b565d4_uart2_requal *report)
{
	if (!report || !cpu_valid() || uart_platform_base(1) != UART_BASE ||
	    uart_get_baudrate_divisor() != 1 || CONFIG_TTYS0_LCS != 3)
		return false;
	uint32_t flags = save_flags();
	bool passed = false;

	report->helper_step = 1;
	report->lpc_id = ack38_lpc_read_ecam(0);
	report->lpc44_before = ack38_lpc_read_ecam(0x44);
	report->lpc48_before = ack38_lpc_read_ecam(0x48);
	if (report->lpc_id != 0x790e1022)
		goto done;
	report->lsr_before = inb(UART_BASE + UART8250_LSR);
	report->lcr_before = inb(UART_BASE + UART8250_LCR);
	/* Same decode-preserving RMWs as b565d4_uart2_setup(). */
	report->helper_step = 2;
	ack38_lpc_write_ecam(0x48, report->lpc48_before | 3);
	ack38_lpc_write_ecam(0x44, report->lpc44_before | (1U << 7));
	if (!ack38_lpc_ready_ecam() || !nuvoton_uarts(true))
		goto done;
	report->ast_revision = ast_ahb(ASPEED_SCU_BASE + SILICON_REV_ID_REG, 0, false);
	if (report->ast_revision != 0x04030103U || !ast_uart(true))
		goto done;
	report->helper_step = 3;
	ast_ahb(ASPEED_SCU_BASE + PRO_KEY_REG, PRO_KEY_PASSWORD, true);
	uint32_t pins = ast_ahb(ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL2_REG, 0, false);
	ast_ahb(ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL2_REG,
		(pins & 0xff00ffff) | 0x00ff0000, true);
	pins = ast_ahb(ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL5_REG, 0, false);
	ast_ahb(ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL5_REG, pins & 0xffffffcf, true);
	ast_ahb(ASPEED_SCU_BASE + PRO_KEY_REG, 0, true);
	report->scu84_after = ast_ahb(ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL2_REG, 0, false);
	report->scu90_after = ast_ahb(ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL5_REG, 0, false);
	if (ast_ahb(ASPEED_SCU_BASE + SILICON_REV_ID_REG, 0, false) != report->ast_revision ||
	    (report->scu84_after & 0x00ff0000) != 0x00ff0000 ||
	    (report->scu90_after & 0x30))
		goto done;
	/* Existing eight-register UART setup, with DLAB readback added before
	 * 2f8 can be written as DLL. Never write TBR when it is not ready.
	 */
	report->helper_step = 4;
	outb(3, UART_BASE + UART8250_LCR);
	if (inb(UART_BASE + UART8250_LCR) != 3)
		goto done;
	outb(0, UART_BASE + UART8250_IER);
	outb(UART8250_FCR_FIFO_EN, UART_BASE + UART8250_FCR);
	outb(UART8250_MCR_DTR | UART8250_MCR_RTS, UART_BASE + UART8250_MCR);
	outb(UART8250_LCR_DLAB | 3, UART_BASE + UART8250_LCR);
	report->dlab_readback = inb(UART_BASE + UART8250_LCR);
	if (report->dlab_readback != (UART8250_LCR_DLAB | 3)) {
		outb(3, UART_BASE + UART8250_LCR);
		goto done;
	}
	outb(0, UART_BASE + UART8250_DLM);
	outb(1, UART_BASE + UART8250_DLL);
	outb(3, UART_BASE + UART8250_LCR);
	passed = uart_ready();
	report->helper_step = 5;
done:
	if (report->lpc_id == 0x790e1022) {
		report->lpc44_after = ack38_lpc_read_ecam(0x44);
		report->lpc48_after = ack38_lpc_read_ecam(0x48);
		report->lsr_after = inb(UART_BASE + UART8250_LSR);
		report->lcr_after = inb(UART_BASE + UART8250_LCR);
	}
	/* A source property of this helper/call tree, not a CF8 readback. */
	report->cf8_untouched = 1;
	restore_flags(flags);
	return passed && report->cf8_untouched && report->lcr_after == 3 &&
		(report->lpc44_after & (1U << 7)) && (report->lpc48_after & 3) == 3;
}
#endif
