/* SPDX-License-Identifier: GPL-2.0-only */

#include <console/console.h>
#include <device/pci_ops.h>
#include <device/smbus_host.h>
#include <southbridge/intel/common/gpio.h>
#include <southbridge/intel/common/pmbase.h>
#include <southbridge/intel/common/pmutil.h>
#include "i82801jx.h"

#define TCO_BASE 0x60

void i82801jx_early_init(void)
{
	const pci_devfn_t d31f0 = PCI_DEV(0, 0x1f, 0);

	if (ENV_RAMINIT)
		enable_smbus();

	printk(BIOS_DEBUG, "Setting up static southbridge registers...");
	i82801jx_setup_bars();
	printk(BIOS_DEBUG, " done.\n");

	setup_pch_gpios(&mainboard_gpio_map);

	printk(BIOS_DEBUG, "Disabling Watchdog reboot...");
	RCBA32(GCS) = RCBA32(GCS) | (1 << 5);	/* No reset */
	write_pmbase16(TCO_BASE + 0x8, (1 << 11));	/* halt timer */
	write_pmbase16(TCO_BASE + 0x4, (1 << 3));	/* clear timeout */
	write_pmbase16(TCO_BASE + 0x6, (1 << 1));	/* clear 2nd timeout */
	printk(BIOS_DEBUG, " done.\n");

	/* Enable IOAPIC */
	RCBA8(OIC) = 0x3;
	RCBA8(OIC);

	/* Initialize power management initialization
	   register early as it affects reboot behavior. */
	/* Bit 20 activates global reset of host and ME on cf9 writes of 0x6
	   and 0xe (required if ME is disabled but present), bit 31 locks it.
	   The other bits are 'must write'. */
	u32 pmir = pci_read_config32(d31f0, D31F0_PMIR);

	pmir |= PMIR_CF9LOCK | PMIR_FIELD_2 | PMIR_CF9GR | PMIR_FIELD_0;
	pci_write_config32(d31f0, D31F0_PMIR, pmir);

	/* TODO: If RTC power failed, reset RTC state machine
		(set, then reset RTC 0x0b bit7) */

	/* TODO: Check power state bits in GEN_PMCON_2 (D31F0 0xa2)
		before they get cleared. */
}
