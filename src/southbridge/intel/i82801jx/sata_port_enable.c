/* SPDX-License-Identifier: GPL-2.0-only */

#include <device/pci_ops.h>

#include "i82801jx.h"

/* Intel ICH10 Family Datasheet, section 14.1.31. */
void i82801jx_sata_enable_all_ports(const pci_devfn_t sata)
{
	uint8_t pcs_low = pci_io_read_config8(sata, I82801JX_SATA_PCS);

	pcs_low &= ~I82801JX_SATA_PCS_PORT_ENABLE_MASK;
	pcs_low |= I82801JX_SATA_PCS_ALL_PORTS_ENABLED;
	pci_io_write_config8(sata, I82801JX_SATA_PCS, pcs_low);
}
