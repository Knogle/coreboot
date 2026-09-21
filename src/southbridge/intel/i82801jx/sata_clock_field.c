/* SPDX-License-Identifier: GPL-2.0-only */

#include <device/pci_ops.h>

#include "i82801jx.h"

/* Intel ICH10 Family Datasheet, section 14.1.32. */
void i82801jx_sata_program_clock_field(const pci_devfn_t sata)
{
	uint32_t sclkcg = pci_io_read_config32(sata, I82801JX_SATA_SCLKCG);

	sclkcg &= ~I82801JX_SATA_SCLKCG_FIELD1_MASK;
	sclkcg |= I82801JX_SATA_SCLKCG_FIELD1_REQUIRED;
	pci_io_write_config32(sata, I82801JX_SATA_SCLKCG, sclkcg);
}
