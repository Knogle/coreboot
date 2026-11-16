/* SPDX-License-Identifier: GPL-2.0-only */

#include "platform.h"
#include <amdblocks/acpimmio.h>
#include <amdblocks/aoac.h>
#include <console/console.h>
#include <delay.h>
#include <device/pci_def.h>
#include <device/pci_ops.h>

/* The board has no SD socket. On the desktop backends, AOAC device 25 controls the
 * otherwise exposed 1022:7906 SD controller at 00:14.6. This is distinct
 * from the eMMC device at AOAC index 28.
 */
void b565d4_matisse_disable_sdhc(void)
{
	const pci_devfn_t sd = PCI_DEV(0, 0x14, 6);
	const unsigned int aoac_sd = 25;
	uint32_t id = pci_s_read_config32(sd, PCI_VENDOR_ID);
	uint8_t control;

	if (id != 0x79061022)
		return;
	control = aoac_read8(AOAC_DEV_D3_CTL(aoac_sd));
	if (control == UINT8_MAX)
		return;
	if (control & FCH_AOAC_PWR_ON_DEV)
		power_off_aoac_device(aoac_sd);
	for (unsigned int n = 0; n < 1000; n++) {
		id = pci_s_read_config32(sd, PCI_VENDOR_ID);
		if (id == UINT32_MAX)
			break;
		udelay(100);
	}
	printk(id == UINT32_MAX ? BIOS_INFO : BIOS_WARNING,
	       B565D4_DESKTOP_NAME " unused SDHC: AOAC25 %02x->%02x id=%08x\n", control,
	       aoac_read8(AOAC_DEV_D3_CTL(aoac_sd)), id);
}
