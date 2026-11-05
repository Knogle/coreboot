/* SPDX-License-Identifier: GPL-2.0-only */
#include <memlayout.h>
#include <arch/header.ld>
#include <b565d4_memlayout.h>

#if !CONFIG(SOC_AMD_CEZANNE_NATIVE) || !CONFIG(RESET_VECTOR_IN_RAM)
#error "Private retained IA32 layout only"
#endif

/* Expected placement is a SOURCE selection, checked against a fresh Type0
 * allocation by the separate Source15/bootstrap before the first parent store.
 */
SECTIONS
{
	DRAM_START(0)
	EARLY_RESERVED_DRAM_START = B565D4_NATIVE_DRAM_BASE;
	_early_reserved_dram = EARLY_RESERVED_DRAM_START;
	REGION(native84_descriptor, B565D4_NATIVE_DRAM_BASE, 56, 4)
	REGION(native84_handle_bank, B565D4_NATIVE_DRAM_BASE + 0x40, 32, 4)
	REGION(native84_context, B565D4_NATIVE_DRAM_BASE + 0x100, 4328, 8)
	REGION(native84_bridge, B565D4_NATIVE_DRAM_BASE + 0x2000, 0x2000, 16)
	REGION(native84_late_stack, B565D4_NATIVE_DRAM_BASE + 0x8000, 0x4000, 16)
	EARLYRAM_STACK(B565D4_NATIVE_DRAM_BASE + 0xc000, 0x4000)
	BOOTBLOCK(B565D4_NATIVE_DRAM_BASE + 0x10000, 0x20000)
#if ENV_BOOTBLOCK
	/* Genuine non-CAR rules use separate bootblock data/BSS. Root copies
	 * each actual initialized LOAD; no cold .datacopy/pre_c replay exists.
	 */
	.data ALIGN(64) : {
		_data = .;
		*(.data .data.* .ldata .ldata.* .sdata .sdata.*)
		. = ALIGN(4);
		_edata = .;
		RECORD_SIZE(data)
	} : data_segment
	_data_load = _data;
	.bss . (NOLOAD) : {
		_bss = .;
		*(.bss .bss.* .lbss .lbss.* .sbss .sbss.* COMMON)
		. = ALIGN(4);
		_ebss = .;
		RECORD_SIZE(bss)
	} : data_segment
	_eprogram = .;
	RECORD_SIZE(program)
	ASSERT(_program == B565D4_NATIVE_DRAM_BASE + 0x10000 &&
		_eprogram <= B565D4_NATIVE_DRAM_BASE + 0x30000,
		"Native84 bootblock text/data/BSS exceeds its own 128KiB slot")
	ASSERT(native84_late_entry >= _text && native84_late_entry < _etext,
		"Late cdecl entry must be file-backed bootblock code")
#endif
	ROMSTAGE(B565D4_NATIVE_DRAM_BASE + 0x30000, 0xa0000)
	REGION(native84_ram_media, B565D4_NATIVE_DRAM_BASE + 0xd0000, 0x80000, 4096)
	_eearly_reserved_dram = B565D4_NATIVE_DRAM_BASE + 0x150000;
	RECORD_SIZE(early_reserved_dram)
	/* No early cache or FMAP cache writes into borrowed memory. Lookup uses
	 * the admitted RAM medium; the real FMAP hook copies into bounded CBMEM.
	 */
	_preram_cbmem_console = .;
	_epreram_cbmem_console = .;
	_preram_cbmem_console_size = 0;
	_timestamp = .;
	_etimestamp = .;
	_timestamp_size = 0;
	_cbfs_cache = .;
	_ecbfs_cache = .;
	_preram_cbfs_cache = .;
	_epreram_cbfs_cache = .;
	_postram_cbfs_cache = .;
	_epostram_cbfs_cache = .;
	_cbfs_cache_size = 0;
	_preram_cbfs_cache_size = 0;
	_postram_cbfs_cache_size = 0;
	/* Genuine stock rmodule link coordinates, not a target destination.
	 * These are below the selected high parent: no ascending-region macro
	 * may pretend they follow EARLY/media in a single physical RAM map.
	 */
#if ENV_RAMSTAGE
	. = 0;
	RAMSTAGE(512M, 8M)
#else
	_ramstage = 512M;
	_eramstage = 512M + 8M;
	_ramstage_size = 8M;
#endif
}
