/* SPDX-License-Identifier: GPL-2.0-only */
#include <amdblocks/b565d4_fspm_firstcall.h>
#include <arch/cpu.h>
#include <console/uart.h>
#include <fsp/util.h>
#include <halt.h>
#include <soc/platform_descriptors.h>
#include <symbols.h>

_Static_assert(sizeof(uintptr_t) == 4 && sizeof(efi_return_status_t) == 4, "IA32 FSP API");
_Static_assert(sizeof(FSPM_UPD) == 1538, "Exact Cezanne1.0.5 UPD");
_Static_assert(CONFIG_FSP_TEMP_RAM_SIZE == 0x80000, "Genuine512KiB temporary BSS");
_Static_assert(CONFIG_FSP_M_ADDR == 0x09e00000 && CONFIG_FSP_M_SIZE == 0xc0000,
	"Separate PSP-buffer FSP image reservation");
_Static_assert(CONFIG(SOC_AMD_COMMON_BLOCK_APOB_NV_DISABLE), "No NV cache");

/* This seam deliberately admits NOTHING. Volatile preserves the genuine
 * future API/temp_ram roots for a later actual ELF/frame audit. Merely flipping
 * this literal is NOT sufficient: the successor must implement full relocated
 * image hash and actual cold APOB read ownership admission after Root review.
 * No physical/supplier/owner qualification follows from this source draft. */
static const volatile uint32_t firstcall_source_unbound = 1;
/* No immutable current read-owner receipt has been supplied. A full linker
 * protection cap is not a readable supplier object cap. Changing only the
 * unbound flag cannot activate this zero-sized receipt. */
static const volatile uint32_t cold_apob_owned_bytes = 0;
struct firstcall_record {
	uint32_t status, hob_pointer, cr0, cr3, cr4, eflags;
};
static volatile struct firstcall_record firstcall_result;

static uint32_t flags(void)
{
	uint32_t value;
	__asm__ volatile("pushfl; popl %0" : "=r" (value));
	return value;
}
static uint32_t cr0(void)
{
	uint32_t value;
	__asm__ volatile("mov %%cr0,%0" : "=r" (value));
	return value;
}
static void text(const char *p)
{
	for (unsigned int n = 0; n < 160 && p[n]; n++)
		uart_tx_byte(1, p[n]);
}
static void hex32(uint32_t value)
{
	static const char digits[] = "0123456789abcdef";
	for (unsigned int n = 8; n; n--)
		uart_tx_byte(1, digits[(value >> ((n - 1) * 4)) & 15]);
}
static void __noreturn refuse(void)
{
	text("B565D4 FSPM_FIRST_REFUSED identity_and_apob_unbound=1\r\n");
	uart_tx_flush(1);
	halt();
}

void b565d4_fspm_firstcall_prefix_admit(void)
{
	/* Run before the genuine romstage/FSP prefix can read ACPI/MMIO, the
	 * APOB header, or the candidate image. This closed source only halts. */
	if (firstcall_source_unbound || !cold_apob_owned_bytes)
		refuse();
}

bool b565d4_fspm_cold_apob_read_admitted(uintptr_t base, size_t bytes)
{
	/* Linker reservation is only a bound, never an owner receipt. */
	uint32_t admitted_bytes = cold_apob_owned_bytes;
	return !firstcall_source_unbound && base == 0x0a200000 && bytes >= 16 &&
		admitted_bytes <= 0x28000 && bytes <= admitted_bytes;
}

/* A link-only zero-count board callback for this closed first-return option.
 * This is not a qualified B565 PCIe/DDI description or proof of FSP zero-port
 * semantics. No provider invocation may be enabled without a new board policy. */
void mainboard_get_dxio_ddi_descriptors(const fsp_dxio_descriptor **dxio,
	size_t *dxio_count, const fsp_ddi_descriptor **ddi, size_t *ddi_count)
{
	*dxio = NULL;
	*dxio_count = 0;
	*ddi = NULL;
	*ddi_count = 0;
}

void b565d4_fspm_firstcall_before(const struct fsp_header *hdr,
	const FSPM_UPD *upd, const void *temporary, size_t bytes)
{
	uintptr_t start = (uintptr_t)temporary, program = (uintptr_t)_program;
	uintptr_t end = (uintptr_t)_eprogram;
	if (firstcall_source_unbound || cpuid_eax(1) != 0x00a50f00 ||
	    !(cr0() & 1) || (cr0() & 0x80000000) || (flags() & 0x600) ||
	    hdr->signature != 0x48505346 || hdr->header_length != 76 ||
	    hdr->spec_version != 0x20 || hdr->header_revision != 3 ||
	    hdr->image_base != 0x09e00000 || hdr->image_size != 0x39000 ||
	    hdr->image_revision != 0x01000500 || hdr->cfg_region_offset != 0x36e9c ||
	    hdr->cfg_region_size != sizeof(FSPM_UPD) || hdr->fsp_memory_init_entry_offset != 0x758 ||
	    upd->FspUpdHeader.Signature != FSPM_UPD_SIGNATURE || upd->FspUpdHeader.Revision != 1 ||
	    upd->FspmArchUpd.Revision != 1 || upd->FspmConfig.UpdTerminator != 0x55aa ||
	    upd->FspmArchUpd.BootMode != FSP_BOOT_WITH_FULL_CONFIGURATION ||
	    upd->FspmArchUpd.NvsBufferPtr != 0x0a200000 ||
	    upd->FspmArchUpd.StackBase != start || upd->FspmArchUpd.StackSize != bytes ||
	    bytes != 0x80000 || (start & 15) || start < program || end < start || bytes > end - start ||
	    upd->FspmConfig.serial_port_base != 0x2f8 || upd->FspmConfig.serial_port_use_mmio ||
	    upd->FspmConfig.serial_port_baudrate != 115200 || upd->FspmConfig.serial_port_refclk != 1843200)
		refuse();
	if (*(void **)fsp_get_hob_list_ptr() != NULL)
		refuse();
	text("B565D4 FSPM_FIRST_CALL entry=09e00758\r\n");
	uart_tx_flush(1);
}

void __noreturn b565d4_fspm_firstcall_return(efi_return_status_t status,
	const void *hob_pointer, uint32_t original_flags)
{
	/* DF has already been restored by the exact ASM gateway. Validate the
	 * ORIGINAL DF/IF and current protected flat-state before any aggregate. */
	uint32_t c0 = cr0();
	if (!(c0 & 1) || (c0 & 0x80000000) || (original_flags & 0x600))
		halt();
	uint32_t c3, c4;
	__asm__ volatile("mov %%cr3,%0; mov %%cr4,%1" : "=r" (c3), "=r" (c4));
	firstcall_result = (struct firstcall_record){status, (uintptr_t)hob_pointer,
		c0, c3, c4, original_flags};
	text("B565D4 FSPM_FIRST_RETURN status="); hex32(status);
	text(" hob_pointer="); hex32((uintptr_t)hob_pointer);
	text(" cr0="); hex32(firstcall_result.cr0);
	text(" cr3="); hex32(c3);
	text(" cr4="); hex32(c4);
	text(" flags="); hex32(firstcall_result.eflags);
	text(" owner_release=0\r\n");
	uart_tx_flush(1);
	/* No HOB pointer dereference, reset handler, CBMEM, ramstage or payload. */
	halt();
}
