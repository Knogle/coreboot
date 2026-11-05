/* SPDX-License-Identifier: GPL-2.0-only */

#include "platform.h"
#include <amdblocks/acpimmio.h>
#include <amdblocks/lpc.h>
#include <amdblocks/pmlib.h>
#include <amdblocks/smbus.h>
#include <amdblocks/spi.h>
#include <console/console.h>
#include <soc/southbridge.h>

/* LPC/AST has already been set up by the board's pre-SoC entry. Desktop
 * Promontory has no Cezanne AOAC/I2C/mobile UART initialization here.
 */
void fch_pre_init(void)
{
	enable_acpimmio_decode_pm04();
	lpc_early_init();
	lpc_enable_port80();
	fch_spi_early_init();
	fch_smbus_init();
	fch_enable_cf9_io();
	fch_enable_legacy_io();
	fch_disable_legacy_dma_io();
}

void fch_early_init(void)
{
	pm_set_power_failure_state();
	printk(BIOS_NOTICE, "B565D4 " B565D4_DESKTOP_NAME " DRAM-first bootblock; desktop FCH path\n");
}

void __wrap_write_resume_eip(void);
void __wrap_write_resume_eip(void)
{
	/* This first desktop backend does not implement S3. Do not program the
	 * write-once Cezanne resume entry into the Vermeer CC6 save context.
	 */
}
