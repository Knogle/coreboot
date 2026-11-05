/* SPDX-License-Identifier: GPL-2.0-only */
#include <arch/exception.h>
#include <bootblock_common.h>
#include <console/console.h>
#include <console/uart.h>
#include <delay.h>
#include <halt.h>
#include <program_loading.h>
#include <stdint.h>
#include "normal_uart2.h"

uint32_t b565d4_normal_reset_eax;

void bootblock_mainboard_early_init(void)
{
	b565d4_normal_uart2_early_init();
	/* Warm the stock TSC cache while console logging is still inactive. */
	init_timer();
	if (1)
		return;
	console_init();
	exception_init();
	printk(BIOS_INFO, "B565D4 NORMAL_UART2 board_early_return=1 console_return=1 ResetEAX=%08x\n",
		b565d4_normal_reset_eax);
	uart_tx_flush(1);
	if (1) {
		/* Normal loader, but intentionally before later FCH initialization. */
		run_romstage();
	}
	/* The normal SoC early path has run; later SoC/romstage/FSP has not. */
	halt();
}

void bootblock_mainboard_init(void)
{
	printk(BIOS_INFO,
		"B565D4 NORMAL_BOOTBLOCK_FCH soc_init_return=1 before_stock_romstage=1 ResetEAX=%08x\n",
		b565d4_normal_reset_eax);
	uart_tx_flush(1);
	/* Return to the stock bootblock completion and romstage loader. */
}
