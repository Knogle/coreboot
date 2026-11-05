/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/acpimmio_map.h>
#include <amdblocks/iomap.h>
#include <amdblocks/memmap.h>
#include <amdblocks/root_complex.h>
#include <device/device.h>
#include <halt.h>
#include <soc/iomap.h>
#include <stdint.h>

#if (CONFIG(B565D4_RETAINED_PEI_LATE_ENTRY) || CONFIG(SOC_AMD_CEZANNE_NATIVE))
#include "native84_profile.h"
#include "native_apob_resources.h"
#include <console/console.h>
#include <console/uart.h>
#endif

/*
 *                     +--------------------------------+
 *                     |                                |
 *                     |                                |
 *                     |                                |
 *                     |                                |
 *                     |                                |
 *                     |                                |
 *                     |                                |
 *   reserved_dram_end +--------------------------------+
 *                     |                                |
 *                     |       verstage (if reqd)       |
 *                     |          (VERSTAGE_SIZE)       |
 *                     +--------------------------------+ VERSTAGE_ADDR
 *                     |                                |
 *                     |            FSP-M               |
 *                     |         (FSP_M_SIZE)           |
 *                     +--------------------------------+ FSP_M_ADDR
 *                     |           romstage             |
 *                     |        (ROMSTAGE_SIZE)         |
 *                     +--------------------------------+ ROMSTAGE_ADDR = BOOTBLOCK_END
 *                     |                                | X86_RESET_VECTOR = BOOTBLOCK_END  - 0x10
 *                     |           bootblock            |
 *                     |     (C_ENV_BOOTBLOCK_SIZE)     |
 *                     +--------------------------------+ BOOTBLOCK_ADDR = BOOTBLOCK_END - C_ENV_BOOTBLOCK_SIZE
 *                     |          Unused hole           |
 *                     |            (30KiB)             |
 *                     +--------------------------------+
 *                     |     FMAP cache (FMAP_SIZE)     |
 *                     +--------------------------------+ PSP_SHAREDMEM_BASE + PSP_SHAREDMEM_SIZE + PRERAM_CBMEM_CONSOLE_SIZE + 0x200
 *                     |  Early Timestamp region (512B) |
 *                     +--------------------------------+ PSP_SHAREDMEM_BASE + PSP_SHAREDMEM_SIZE + PRERAM_CBMEM_CONSOLE_SIZE
 *                     |      Preram CBMEM console      |
 *                     |   (PRERAM_CBMEM_CONSOLE_SIZE)  |
 *                     +--------------------------------+ PSP_SHAREDMEM_BASE + PSP_SHAREDMEM_SIZE
 *                     |   PSP shared (vboot workbuf)   |
 *                     |      (PSP_SHAREDMEM_SIZE)      |
 *                     +--------------------------------+ PSP_SHAREDMEM_BASE
 *                     |          APOB (120KiB)         |
 *                     +--------------------------------+ PSP_APOB_DRAM_ADDRESS
 *                     |        Early BSP stack         |
 *                     |   (EARLYRAM_BSP_STACK_SIZE)    |
 * reserved_dram_start +--------------------------------+ EARLY_RESERVED_DRAM_BASE
 *                     |              DRAM              |
 *                     +--------------------------------+ 0x100000
 *                     |           Option ROM           |
 *                     +--------------------------------+ 0xc0000
 *                     |           Legacy VGA           |
 *                     +--------------------------------+ 0xa0000
 *                     |              DRAM              |
 *                     +--------------------------------+ 0x0
 */
void read_soc_memmap_resources(struct device *dev, unsigned long *idx)
{
#if (CONFIG(B565D4_RETAINED_PEI_LATE_ENTRY) || CONFIG(SOC_AMD_CEZANNE_NATIVE))
	enum native_apob_status status;

	native84_stage_check();
	/* Preserve the genuine fixed IOMMU reservation independently of APOB RAM.
	 * The DF/IOHC prefix and LPC read_resources provide their actual fixed
	 * registers; do not turn the entire PCI hole into fixed occupied MMIO.
	 */
	if (probe_resource(dev, *idx))
		native84_terminal();
	mmio_range(dev, (*idx)++, IOMMU_RESERVED_MMIO_BASE, IOMMU_RESERVED_MMIO_SIZE);
	status = native_apob_publish_resources(dev, *idx, idx);
	printk(BIOS_INFO, "NATIVE99 APOB RESOURCE MAP status=%u next=%lu\n", status, *idx);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (status != NATIVE_APOB_OK)
		native84_terminal();
#elif CONFIG(B565D4_NORMAL_NATIVE_RAMSTAGE_ENTRY)
	/* A bounded CBMEM top is not an all-RAM/TOLUM/TSEG resource provider. */
	halt();
#else
	read_lower_soc_memmap_resources(dev, idx);

	/* Reserve fixed IOMMU MMIO region */
	mmio_range(dev, (*idx)++, IOMMU_RESERVED_MMIO_BASE, IOMMU_RESERVED_MMIO_SIZE);

	read_fsp_resources(dev, idx);
#endif
}
