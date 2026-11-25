/* SPDX-License-Identifier: GPL-2.0-only */
#include <arch/exception.h>
#include <bootblock_common.h>
#include <console/console.h>
#include <console/uart.h>
#include <delay.h>
#include <halt.h>
#include <stdint.h>
#include "../normal_uart2.h"

uint32_t b565d4_normal_reset_eax;

void bootblock_mainboard_early_init(void)
{
	if (!b565d4_uart2_setup()) {
		post_code(0xe7);
		halt();
	}
	init_timer();
}

void bootblock_mainboard_init(void)
{
	printk(BIOS_INFO, "B565D4 native bootblock complete ResetEAX=%08x\n",
	       b565d4_normal_reset_eax);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}
