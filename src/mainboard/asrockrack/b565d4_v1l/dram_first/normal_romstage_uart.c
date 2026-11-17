/* SPDX-License-Identifier: GPL-2.0-only */
#include "normal_cold_apob.h"
#include <arch/cpu.h>
#include <console/console.h>
#include <console/uart.h>
#include <delay.h>
#include <halt.h>
#include <romstage_common.h>

asmlinkage void __real_car_stage_entry(void);
asmlinkage void __wrap_car_stage_entry(void);

asmlinkage void __wrap_car_stage_entry(void)
{
	/* Stage-local .bss is freshly zeroed. Warm its stock timer cache while
	 * its console is inactive, preserving the reviewed normal V4 order. */
	init_timer();
	__real_car_stage_entry();
	halt();
}

#if !CONFIG(SOC_AMD_CEZANNE_NATIVE)
#error "B565 ROMstage requires the native Cezanne entry path"
#endif
void __noreturn romstage_main(void)
{
	/* Genuine assembly_entry/car_stage_entry already returned from console
	 * initialization and its unsupported-S3 check. The default remains terminal before FSP/CBMEM. */
	printk(BIOS_INFO,
		"B565D4 NORMAL_ROMSTAGE stage_entry=1 console_return=1 before_fsp=1\n");
	uart_tx_flush(1);
	b565d4_cold_apob_header_witness();
	halt();
}
