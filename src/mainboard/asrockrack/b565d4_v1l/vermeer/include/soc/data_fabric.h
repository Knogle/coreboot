/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_DATA_FABRIC_H
#define AMD_MATISSE_DATA_FABRIC_H

#include <amdblocks/data_fabric_defs.h>
#include <types.h>

/* AM4 Matisse window encoding and 32-bit indirect accesses, HOST qualified.
 * Optional window attributes and the hardware MMIO count remain unqualified.
 * Selected only by the experimental desktop backend.
 */
#define IOMS0_FABRIC_ID			4

#define DF_PCI_CFG_MAP0			DF_REG_ID(0, 0xa0)
#define DF_PCI_CFG_MAP_COUNT		8
#define DF_PCI_CFG_MAP(reg)		(DF_PCI_CFG_MAP0 + (reg) * sizeof(uint32_t))

union df_pci_cfg_map {
	struct {
		uint32_t re		:  1; /* [ 0.. 0] */
		uint32_t we		:  1; /* [ 1.. 1] */
		uint32_t		:  2; /* [ 2.. 3] */
		uint32_t dst_fabric_id	: 10; /* [ 4..13] */
		uint32_t		:  2; /* [14..15] */
		uint32_t bus_num_base	:  8; /* [16..23] */
		uint32_t bus_num_limit	:  8; /* [24..31] */
	};
	uint32_t raw;
};

#define DF_IO_BASE0			DF_REG_ID(0, 0xc0)
#define DF_IO_LIMIT0			DF_REG_ID(0, 0xc4)
#define DF_IO_REG_COUNT			8
#define DF_IO_REG_OFFSET(instance)	((instance) * 2 * sizeof(uint32_t))
#define DF_IO_BASE(reg)			(DF_IO_BASE0 + DF_IO_REG_OFFSET(reg))
#define DF_IO_LIMIT(reg)			(DF_IO_LIMIT0 + DF_IO_REG_OFFSET(reg))
#define DF_IO_ADDR_SHIFT			12

union df_io_base {
	struct {
		uint32_t re		:  1; /* [ 0.. 0] */
		uint32_t we		:  1; /* [ 1.. 1] */
		uint32_t		: 10; /* [ 2..11] */
		uint32_t io_base		: 13; /* [12..24] */
		uint32_t		:  7; /* [25..31] */
	};
	uint32_t raw;
};

union df_io_limit {
	struct {
		uint32_t dst_fabric_id	: 10; /* [ 0.. 9] */
		uint32_t		:  2; /* [10..11] */
		uint32_t io_limit	: 13; /* [12..24] */
		uint32_t		:  7; /* [25..31] */
	};
	uint32_t raw;
};

#define DF_MMIO_BASE0			DF_REG_ID(0, 0x200)
#define DF_MMIO_LIMIT0			DF_REG_ID(0, 0x204)
#define DF_MMIO_CTRL0			DF_REG_ID(0, 0x208)
#define DF_MMIO_SHIFT			16
#define DF_MMIO_REG_SET_SIZE		4

/* A resource backend must also qualify the window count and optional attributes. */
union df_mmio_control {
	struct {
		uint32_t re		:  1; /* [ 0.. 0] */
		uint32_t we		:  1; /* [ 1.. 1] */
		uint32_t		:  2; /* [ 2.. 3] */
		uint32_t dst_fabric_id	: 10; /* [ 4..13] */
		uint32_t		: 18; /* [14..31] */
	};
	uint32_t raw;
};

#define DF_FICAA_BIOS			DF_REG_ID(4, 0x5c)
#define DF_FICAD_LO			DF_REG_ID(4, 0x98)

union df_ficaa {
	struct {
		uint32_t cfg_inst_acc_en	:  1; /* [ 0.. 0] */
		uint32_t		:  1; /* [ 1.. 1] */
		uint32_t reg_num		:  9; /* [ 2..10] */
		uint32_t func_num	:  3; /* [11..13] */
		uint32_t		:  2; /* [14..15], 32-bit accesses */
		uint32_t inst_id		:  8; /* [16..23] */
		uint32_t		:  8; /* [24..31] */
	};
	uint32_t raw;
};

#endif /* AMD_MATISSE_DATA_FABRIC_H */
