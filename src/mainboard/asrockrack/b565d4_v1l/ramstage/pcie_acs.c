/* SPDX-License-Identifier: GPL-2.0-only */

#include "pcie_acs.h"
#include <amdblocks/smn.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <types.h>

/* Same-board BaseRnPei TE [3fa4, 4187): PCD 1d3 == 1. These twenty-one
 * records have identical clear and set masks, so their RMW is an OR.
 * PCIe GPP ACS features are requested separately in the DXIO descriptors.
 */
void b565d4_pcie_acs_prepare(void)
{
	static const struct {
		uint32_t address, mask;
	} straps[] = {
		{ 0x10131000, 0x00000004 },
		{ 0x10131200, 0x00000004 },
		{ 0x10131400, 0x00000004 },
		{ 0x10134008, 0x00020000 },
		{ 0x10134208, 0x00020000 },
		{ 0x10134408, 0x00020000 },
		{ 0x10134608, 0x00020000 },
		{ 0x10134808, 0x00020000 },
		{ 0x10134a08, 0x00020000 },
		{ 0x10134c08, 0x00020000 },
		{ 0x10134e08, 0x00020000 },
		{ 0x10135008, 0x00020000 },
		{ 0x10135208, 0x00020000 },
		{ 0x10135408, 0x00020000 },
		{ 0x10135608, 0x00020000 },
		{ 0x10136008, 0x00020000 },
		{ 0x10136208, 0x00020000 },
		{ 0x10136408, 0x00020000 },
		{ 0x10131014, 0x2f800000 },
		{ 0x10131214, 0x2f800000 },
		{ 0x10131414, 0x2f800000 },
	};
	unsigned int accepted = 0;

	for (size_t i = 0; i < ARRAY_SIZE(straps); i++) {
		uint32_t before = smn_read32(straps[i].address);
		uint32_t after;

		smn_write32(straps[i].address, before | straps[i].mask);
		after = smn_read32(straps[i].address);
		accepted += (after & straps[i].mask) == straps[i].mask;
		printk(BIOS_INFO, "NATIVE306 ACS NBIF %08x %08x -> %08x mask=%08x\n",
		       straps[i].address, before, after, straps[i].mask);
	}
	printk(BIOS_NOTICE, "NATIVE306 ACS NBIF straps accepted=%u/%zu\n",
	       accepted, ARRAY_SIZE(straps));
}
