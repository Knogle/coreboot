/* SPDX-License-Identifier: GPL-2.0-only */

#include <arch/io.h>
#include <arch/romstage.h>
#include <cbmem.h>
#include <console/console.h>
#include <cpu/cpu.h>
#include <cpu/x86/mtrr.h>
#include <stdint.h>

#include "raminit_handoff.h"

#define POST_B00_UNIMPLEMENTED_MEMORY_PATH	0xee
#define POST_X58_RAMINIT_POSTCAR_FRAME_READY		0x0d

#define X58_MEMORY_MAP_LOW_RAM0_BASE	0x00000000u
#define X58_MEMORY_MAP_LOW_RAM0_SIZE	0x80000000u
#define X58_MEMORY_MAP_LOW_RAM1_BASE	0x80000000u
#define X58_MEMORY_MAP_LOW_RAM1_SIZE	0x40000000u
#define X58_MEMORY_MAP_VGA_HOLE_BASE	0x000a0000u
#define X58_MEMORY_MAP_VGA_HOLE_SIZE	0x00020000u


#define X58_PLATFORM_HIGH_RAM_BASE	0x100000000ULL
#define X58_PLATFORM_HIGH_RAM_SIZE	0x040000000ULL
#define X58_PLATFORM_PHYS_BITS	40u
#define X58_PLATFORM_MTRR_CAP		0x00000d0au
#define X58_PLATFORM_LOW_MTRRS		3u

_Static_assert((X58_PLATFORM_HIGH_RAM_SIZE & (X58_PLATFORM_HIGH_RAM_SIZE - 1)) == 0,
	"high RAM must fit one power-of-two MTRR");
_Static_assert((X58_PLATFORM_HIGH_RAM_BASE & (X58_PLATFORM_HIGH_RAM_SIZE - 1)) == 0,
	"high RAM must be aligned to its MTRR size");

static void x58_unqualified_postcar_high_ram(struct postcar_frame *pcf)
{
	struct var_mtrr_context *ctx = pcf->mtrr;
	const msr_t cap = rdmsr(MTRR_CAP_MSR);
	const msr_t def = rdmsr(MTRR_DEF_TYPE_MSR);
	const uint64_t address_mask = (1ULL << X58_PLATFORM_PHYS_BITS) - 1;
	const uint64_t mask = (~(X58_PLATFORM_HIGH_RAM_SIZE - 1) & address_mask) |
		MTRR_PHYS_MASK_VALID;
	const uint64_t base = X58_PLATFORM_HIGH_RAM_BASE | MTRR_TYPE_WRBACK;

	/* Reserve the next entry for the common flash-ROM WP mapping, too. */
	if (ctx == NULL || ctx->used_var_mtrrs != X58_PLATFORM_LOW_MTRRS ||
	    ctx->max_var_mtrrs < X58_PLATFORM_LOW_MTRRS + 2 ||
	    ctx->max_var_mtrrs != (cap.lo & MTRR_CAP_VCNT) ||
	    pcf->skip_common_mtrr || cpu_phys_address_size() != X58_PLATFORM_PHYS_BITS ||
	    cap.hi != 0 || cap.lo != X58_PLATFORM_MTRR_CAP ||
	    def.hi != 0 || def.lo != MTRR_DEF_TYPE_EN)
		die("[MTRR] unqualified high-RAM postcar prerequisites mismatch\n");

	/*
	 * The preceding exact X58_MEMORY_MAP result gate admits only 0..3 GiB plus this
	 * 4..5 GiB remap.  Its RAM stability remains unqualified.
	 * The uintptr_t postcar API truncates a 4-GiB base on this 32-bit target,
	 * but its var_mtrr_context already carries complete 64-bit MSR pairs.
	 * Use the same mask/base encoding as cpu/x86/mtrr/mtrr.c:prep_var_mtrr.
	 * Only prepare that context here: no live MTRR write and no RAM access
	 * above 4 GiB.  Standard exit_car/earlymtrr commits it after CAR teardown,
	 * while MTRRs remain disabled, before enabling the complete solution.
	 * The 3..4 GiB MMIO hole stays default UC (existing flash-ROM WP excepted).
	 */
	ctx->mtrr[ctx->used_var_mtrrs].base = (msr_t) {
		.lo = (uint32_t)base, .hi = base >> 32,
	};
	ctx->mtrr[ctx->used_var_mtrrs].mask = (msr_t) {
		.lo = (uint32_t)mask, .hi = mask >> 32,
	};
	ctx->used_var_mtrrs++;
	printk(BIOS_NOTICE, "[MTRR] unqualified postcar plan: 4..5 GiB WB; RAM stability unproven\n");
}


void fill_postcar_frame(struct postcar_frame *pcf)
{
	/*
	 * X58_MEMORY_MAP is the deliberately broad first-boot path.  Its exact
	 * one-DIMM gate reports a 3-GiB low-memory range and a 1-GiB remap above
	 * 4 GiB.  At the operator's explicit request this image assumes the
	 * complete map is usable, beyond X58_MEMORY_PROFILE's bounded smoke-test evidence.
	 * Cache the complete low range as WB so a real payload and boot loader do
	 * not execute out of UC memory.  The legacy VGA aperture is overlaid UC;
	 * the writable C0000-FFFFF PAM shadow stays WB for SeaBIOS and option-ROM
	 * relocation.  The legacy 32-bit postcar API leaves remapped high RAM UC;
	 * only the separately gated board platform appends its WB entry.
	 */
	postcar_frame_add_mtrr(pcf, X58_MEMORY_MAP_LOW_RAM0_BASE,
				X58_MEMORY_MAP_LOW_RAM0_SIZE, MTRR_TYPE_WRBACK);
	postcar_frame_add_mtrr(pcf, X58_MEMORY_MAP_LOW_RAM1_BASE,
				X58_MEMORY_MAP_LOW_RAM1_SIZE, MTRR_TYPE_WRBACK);
	postcar_frame_add_mtrr(pcf, X58_MEMORY_MAP_VGA_HOLE_BASE,
				X58_MEMORY_MAP_VGA_HOLE_SIZE, MTRR_TYPE_UNCACHEABLE);
	x58_unqualified_postcar_high_ram(pcf);
	outb(POST_X58_RAMINIT_POSTCAR_FRAME_READY, CONFIG_POST_IO_PORT);
}

uintptr_t cbmem_top_chipset(void)
{
	return X58_RAMINIT_CBMEM_TOP;
}
