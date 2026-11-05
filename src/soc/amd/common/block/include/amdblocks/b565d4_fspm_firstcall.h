/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AMD_BLOCK_B565D4_FSPM_FIRSTCALL_H
#define AMD_BLOCK_B565D4_FSPM_FIRSTCALL_H
#include <types.h>
#include <efi/efi_datatype.h>
#include <fsp/api.h>
#include <fsp/info_header.h>
#include <fsp/util.h>

/* Fixed source-owned gate, not a pointer or permission supplied by a caller.
 * The proposed implementation refuses all reads until a new reviewed actual
 * cold APOB/loaded-FSP identity implementation replaces the closed seam. */
void b565d4_fspm_firstcall_prefix_admit(void);
bool b565d4_fspm_cold_apob_read_admitted(uintptr_t base, size_t bytes);
void b565d4_fspm_firstcall_before(const struct fsp_header *hdr,
	const FSPM_UPD *upd, const void *temporary, size_t bytes);
efi_return_status_t __attribute__((cdecl, regparm(0), visibility("hidden")))
	b565d4_fspm_call_boundary(fsp_memory_init_fn function, void *upd,
		void **hob_slot, uint32_t *original_flags);
void __noreturn b565d4_fspm_firstcall_return(efi_return_status_t status,
	const void *hob_pointer, uint32_t original_flags);
#endif
