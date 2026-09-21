/* SPDX-License-Identifier: GPL-2.0-only */

#include <device/pci_ops.h>

#include "i82801jx.h"

/* Intel ICH10 Family Datasheet, sections 14.1.16 and 14.1.30. */
void i82801jx_sata_select_ahci(const pci_devfn_t sata)
{
	uint16_t map = pci_io_read_config16(sata, I82801JX_SATA_MAP);

	map &= ~I82801JX_SATA_MAP_AHCI_D31F2_MASK;
	map |= I82801JX_SATA_MAP_AHCI_D31F2_VALUE;
	pci_io_write_config16(sata, I82801JX_SATA_MAP, map);

	/* BAR5 changes from an I/O BAR to ABAR; hardware does not clear its BA. */
	pci_io_write_config32(sata, I82801JX_SATA_ABAR, 0);
}
