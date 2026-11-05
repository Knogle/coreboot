/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NATIVE84_PROFILE_H
#define NATIVE84_PROFILE_H

#include <commonlib/bsd/compiler.h>
#include <b565d4_memlayout.h>
#include "native84_handoff.h"

/* Genuine coreboot CPPFLAGS_common force-includes kconfig.h before C headers.
 * Preserve mutual exclusion without recursive Kconfig dependency backedges.
 */


/* Keep core feature exclusions outside the selected-option dependency graph. */
#if CONFIG(SOC_AMD_CEZANNE_NATIVE) && \
	(CONFIG(VBOOT) || CONFIG(TPM) || \
	 (CONFIG(USE_OPTION_TABLE) && !CONFIG(B565D4_PCIE_RUNTIME_OPTIONS)) || \
	 CONFIG(CMOS_POST) || CONFIG(HAVE_ACPI_RESUME) || CONFIG(COOP_MULTITASKING))
#error "Native84 retained profile forbids unqualified vboot/TPM/CMOS/resume/task profiles"
#endif

/* Root's SOURCE placement selection. None of these numbers proves a lease. */
#define NATIVE84_A B565D4_NATIVE_DRAM_BASE
#define NATIVE84_R B565D4_NATIVE_DRAM_BYTES
#define NATIVE84_EPOCH UINT32_C(0x00840001)
#define NATIVE84_EARLY_BYTES UINT32_C(0x10000)
#define NATIVE84_BOOTBLOCK_BEGIN (NATIVE84_A + 0x10000)
#define NATIVE84_BOOTBLOCK_END (NATIVE84_A + 0x30000)
#define NATIVE84_ROMSTAGE_BEGIN (NATIVE84_A + 0x30000)
#define NATIVE84_ROMSTAGE_END (NATIVE84_A + 0xd0000)
#define NATIVE84_MEDIA_BEGIN (NATIVE84_A + 0xd0000)
#define NATIVE84_MEDIA_BYTES UINT32_C(0x80000)
#define NATIVE84_CBMEM_BEGIN (NATIVE84_A + 0x150000)
#define NATIVE84_PARENT_END (NATIVE84_A + NATIVE84_R)
#define NATIVE84_BANK_BEGIN (NATIVE84_A + 0x40)
#define NATIVE84_BANK_BYTES UINT32_C(32)
#define NATIVE84_CONTEXT_BEGIN (NATIVE84_A + 0x100)
#define NATIVE84_BRIDGE_BEGIN (NATIVE84_A + 0x2000)
#define NATIVE84_BRIDGE_END (NATIVE84_A + 0x4000)
#define NATIVE84_LATE_STACK_BEGIN (NATIVE84_A + 0x8000)
#define NATIVE84_LATE_STACK_END (NATIVE84_A + 0xc000)
#define NATIVE84_EARLY_STACK_BEGIN (NATIVE84_A + 0xc000)
#define NATIVE84_EARLY_STACK_END (NATIVE84_A + 0x10000)

/* Per-stage initialized SOURCE gate. Root must separately qualify the actual
 * ELF, code/IO/effects/lifetimes and fill these exact words in a fresh build.
 * Entry path2 selects the early native APOB protocol, not a PEI receipt.
 * A matching DWORD is not authority and cannot manufacture ownership.
 */
struct native84_profile_policy {
	uint32_t enabled, epoch, entry_path;
	uint32_t expected_cr0, expected_cr3, expected_cr4;
	uint32_t uart_max_polls, fault_entry;
};
extern const volatile struct native84_profile_policy native84_profile_policy;

void native84_terminal(void) __noreturn;
void native84_stage_admit(const struct native84_handoff *descriptor);
void native84_stage_check(void);
const struct native84_handoff *native84_descriptor(void);
size_t native84_protected(struct native84_interval *list, size_t capacity);
void native84_late_entry(const struct native84_handoff *descriptor)
	__attribute__((regparm(0), noreturn));
void native84_romstage_main(void) __noreturn;
struct rmod_stage_load;
int native84_rmodule_stage_load(struct rmod_stage_load *rsl);
union cbfs_mdata;
int native84_romstage_file_ok(const union cbfs_mdata *metadata, size_t bytes);

#endif
