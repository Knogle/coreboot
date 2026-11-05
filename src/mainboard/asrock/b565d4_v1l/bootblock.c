/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/lpc.h>
#include <bootblock_common.h>
#include <console/console.h>
#include <cpu/cpu.h>
#include <delay.h>
#include <device/pnp_ops.h>
#include <halt.h>
#include <post.h>
#include <superio/aspeed/ast2400/ast2400.h>
#include <superio/aspeed/common/aspeed.h>
#include <superio/nuvoton/common/nuvoton.h>
#include <superio/nuvoton/nct6796d/nct6796d.h>
#include "normal_uart2.h"

#define NUVOTON_DEV PNP_DEV(0x2e, 0)
#define NUVOTON_UART1 PNP_DEV(0x2e, NCT6796D_SP1)
#define NUVOTON_UART2 PNP_DEV(0x2e, NCT6796D_SP2)
#define ASPEED_UART1 PNP_DEV(0x4e, AST2400_SUART1)
#define ASPEED_UART_CLOCK 0xf0
#define ASPEED_UART_CLOCK_24MHZ (1 << 1)

static bool disable_nuvoton_uarts(unsigned int *id)
{
	unsigned int enabled = 1;

	nuvoton_pnp_enter_conf_state(NUVOTON_DEV);
	*id = pnp_read_config(NUVOTON_DEV, 0x20) << 8;
	*id |= pnp_read_config(NUVOTON_DEV, 0x21);
	/* These two known families share UART A/B logical device numbers. */
	if ((*id & 0xfff0) == 0xd420 || (*id & 0xfff0) == 0xc800) {
		pnp_set_logical_device(NUVOTON_UART1);
		pnp_set_enable(NUVOTON_UART1, 0);
		enabled = pnp_read_config(NUVOTON_UART1, PNP_IDX_EN);
		pnp_set_logical_device(NUVOTON_UART2);
		pnp_set_enable(NUVOTON_UART2, 0);
		enabled |= pnp_read_config(NUVOTON_UART2, PNP_IDX_EN);
	}
	nuvoton_pnp_exit_conf_state(NUVOTON_DEV);
	return !(enabled & 1);
}

static bool configure_aspeed_uart(unsigned int *base, unsigned int *clock)
{
	unsigned int enabled;

	aspeed_enable_serial(ASPEED_UART1, CONFIG_TTYS0_BASE);
	pnp_enter_conf_state(ASPEED_UART1);
	pnp_set_logical_device(ASPEED_UART1);
	/* Select 24 MHz / 13, matching the standard 16550 baud-rate divisor. */
	*clock = pnp_read_config(ASPEED_UART1, ASPEED_UART_CLOCK);
	pnp_write_config(ASPEED_UART1, ASPEED_UART_CLOCK, *clock & ~ASPEED_UART_CLOCK_24MHZ);
	*clock = pnp_read_config(ASPEED_UART1, ASPEED_UART_CLOCK);
	enabled = pnp_read_config(ASPEED_UART1, PNP_IDX_EN);
	*base = pnp_read_iobase(ASPEED_UART1, PNP_IDX_IO0);
	pnp_exit_conf_state(ASPEED_UART1);
	return (enabled & 1) && *base == CONFIG_TTYS0_BASE;
}

static void configure_aspeed_uart_pins(uint32_t *revision, uint32_t *scu84,
				      uint32_t *scu90, uint32_t *scu94, uint32_t *route)
{
	/* Confirm an AHB response before modifying the BMC's pin mux. */
	lpc_read(0x4e, ASPEED_SCU_BASE + SILICON_REV_ID_REG, revision);
	if (*revision == 0 || *revision == UINT32_MAX)
		return;

	/* Exact SCU masks in this board's vendor AST2500PeiInit table. */
	lpc_write(0x4e, ASPEED_SCU_BASE + PRO_KEY_REG, PRO_KEY_PASSWORD);
	lpc_read(0x4e, ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL2_REG, scu84);
	lpc_write(0x4e, ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL2_REG,
		  (*scu84 & 0xff00ffff) | 0x00ff0000);
	lpc_read(0x4e, ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL5_REG, scu90);
	lpc_write(0x4e, ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL5_REG, *scu90 & 0xffffffcf);
	lpc_write(0x4e, ASPEED_SCU_BASE + PRO_KEY_REG, 0);
	lpc_read(0x4e, ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL2_REG, scu84);
	lpc_read(0x4e, ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL5_REG, scu90);
	lpc_read(0x4e, ASPEED_SCU_BASE + MUL_FUNC_PIN_CTL6_REG, scu94);
	lpc_read(0x4e, ASPEED_LPC_BASE + HICRA_REG, route);
}

/* The board manual routes the COM header to AST2500 UART1, not Nuvoton. */
void bootblock_mainboard_early_init(void)
{
	if (CONFIG(B565D4_NORMAL_UART2)) {
		if (!b565d4_uart2_setup()) {
			post_code(0xe7);
			halt();
		}
		return;
	}

	const unsigned int signature = cpuid_eax(1);
	unsigned int nuvoton_id;
	bool nuvoton_disabled;
	unsigned int aspeed_uart_base;
	unsigned int aspeed_uart_clock;
	uint32_t revision = 0, scu84 = 0, scu90 = 0, scu94 = 0;
	uint32_t route = 0;

	if ((signature & 0xfffffff0) != 0x00a50f00) {
		post_code(0xe2);
		halt();
	}

	lpc_enable_sio_decode(LPC_SELECT_SIO_2E2F);
	lpc_enable_sio_decode(LPC_SELECT_SIO_4E4F);
	nuvoton_disabled = disable_nuvoton_uarts(&nuvoton_id);

	lpc_enable_decode(DECODE_ENABLE_SERIAL_PORT0);
	/* A cold-starting BMC may configure its UART long after x86 release. */
	for (;;) {
		if (!configure_aspeed_uart(&aspeed_uart_base, &aspeed_uart_clock)) {
			post_code(0xe7);
			mdelay(500);
			continue;
		}
		configure_aspeed_uart_pins(&revision, &scu84, &scu90, &scu94, &route);
		console_init();
		printk(BIOS_INFO, "B565D4-V1L coreboot console experiment C4\n");
		printk(BIOS_INFO, "CPUID=%08x NCT=%04x AST2500 UART1=%04x 115200 8N1\n",
		       signature, nuvoton_id, aspeed_uart_base);
		printk(BIOS_INFO, "AST F0=%02x REV=%08x SCU84=%08x SCU90=%08x SCU94=%08x\n",
		       aspeed_uart_clock, revision, scu84, scu90, scu94);
		printk(BIOS_INFO, "AST UART route HICRA=%08x\n", route);
		if (!nuvoton_disabled)
			printk(BIOS_WARNING, "Nuvoton UART disable unconfirmed; check I/O collision.\n");
		if (revision == 0 || revision == UINT32_MAX ||
		    (scu84 & 0x00ff0000) != 0x00ff0000 || (scu90 & 0x30) || (scu94 & 3) ||
		    (aspeed_uart_clock & ASPEED_UART_CLOCK_24MHZ))
			printk(BIOS_WARNING, "ASPEED UART clock or pin routing readback mismatch.\n");
		printk(BIOS_INFO, "Vendor PSP/ABL retained; remaining in bootblock before romstage.\n");
		post_code(0xc1);
		mdelay(500);
	}
}
