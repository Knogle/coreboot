/* SPDX-License-Identifier: GPL-2.0-only */

#include <arch/io.h>
#include <arch/romstage.h>
#include <cbmem.h>
#include <cpu/x86/mtrr.h>
#include <stdint.h>

#include "b06v6_handoff.h"

#define POST_B00_UNIMPLEMENTED_MEMORY_PATH	0xee
#define POST_B06V6_POSTCAR_FRAME_READY		0x0d

#define B06VM_LOW_RAM0_BASE	0x00000000u
#define B06VM_LOW_RAM0_SIZE	0x80000000u
#define B06VM_LOW_RAM1_BASE	0x80000000u
#define B06VM_LOW_RAM1_SIZE	0x40000000u
#define B06VM_VGA_HOLE_BASE	0x000a0000u
#define B06VM_VGA_HOLE_SIZE	0x00020000u

#if !CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF
static void stop_unimplemented_memory_path(void)
{
	outb(POST_B00_UNIMPLEMENTED_MEMORY_PATH, CONFIG_POST_IO_PORT);
	asm volatile (
		"cli\n\t"
		"1: hlt\n\t"
		"jmp 1b"
	);
	__builtin_unreachable();
}
#endif

void fill_postcar_frame(struct postcar_frame *pcf)
{
#if CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF
#if CONFIG_X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE
	/*
	 * B06VM is the deliberately broad first-boot experiment.  Its exact
	 * one-DIMM gate reports a 3-GiB low-memory range and a 1-GiB remap above
	 * 4 GiB.  At the operator's explicit request this image assumes the
	 * complete map is usable, beyond B06VL's bounded smoke-test evidence.
	 * Cache the complete low range as WB so a real payload and boot loader do
	 * not execute out of UC memory.  The legacy VGA aperture is overlaid UC;
	 * the writable C0000-FFFFF PAM shadow stays WB for SeaBIOS and option-ROM
	 * relocation.  The 32-bit postcar API cannot describe the remapped range
	 * above 4 GiB, which therefore remains UC.
	 */
	postcar_frame_add_mtrr(pcf, B06VM_LOW_RAM0_BASE,
				B06VM_LOW_RAM0_SIZE, MTRR_TYPE_WRBACK);
	postcar_frame_add_mtrr(pcf, B06VM_LOW_RAM1_BASE,
				B06VM_LOW_RAM1_SIZE, MTRR_TYPE_WRBACK);
	postcar_frame_add_mtrr(pcf, B06VM_VGA_HOLE_BASE,
				B06VM_VGA_HOLE_SIZE, MTRR_TYPE_UNCACHEABLE);
#else
	/*
	 * MINIT has proved only the narrow window used by this experiment.  One
	 * power-of-two MTRR covers CBMEM, the post-CAR stack and loaded stages;
	 * the separate upload window deliberately remains UC in the first image.
	 */
	postcar_frame_add_mtrr(pcf, X58_B06V6_CBMEM_BASE,
				X58_B06V6_CBMEM_SIZE, MTRR_TYPE_WRBACK);
#endif
	outb(POST_B06V6_POSTCAR_FRAME_READY, CONFIG_POST_IO_PORT);
#else
	(void)pcf;
	stop_unimplemented_memory_path();
#endif
}

uintptr_t cbmem_top_chipset(void)
{
#if CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF
	return X58_B06V6_CBMEM_TOP;
#else
	stop_unimplemented_memory_path();
	return 0;
#endif
}
