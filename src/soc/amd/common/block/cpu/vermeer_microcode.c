/* SPDX-License-Identifier: GPL-2.0-only */

#include <arch/cpu.h>
#include <console/console.h>
#include <console/streams.h>
#include <cpu/amd/microcode.h>
#include <cpu/x86/msr.h>
#include <stdint.h>
#include <amdblocks/vermeer_microcode.h>

#define VERMEER_UCODE_ADDRESS_MSR 0xc001100c

void vermeer_microcode_address_prepare(void)
{
	const void *patch;
	msr_t address, before, cleared, after;

	/* Qualified Vermeer B2 DXE loaders write the complete patch-buffer
	 * pointer here immediately before PATCH_LOADER. This is an observed
	 * firmware handoff, not an architectural definition of this MSR.
	 */
	if (cpuid_eax(1) != 0x00a20f12 || amd_microcode_patch_id() != 0x0a201213)
		return;

	/* The BSP maps this shared CBFS buffer before genuine MP initialization;
	 * each ordinary CPU init consumes that existing mapping.
	 */
	patch = amd_microcode_find();
	if (!patch || ((uintptr_t)patch & 15)) {
		printk(BIOS_WARNING, "Vermeer microcode: unavailable aligned patch buffer\n");
		return;
	}
	address.raw = (uintptr_t)patch;
	before = rdmsr(VERMEER_UCODE_ADDRESS_MSR);
	printk(BIOS_INFO, "Vermeer microcode: address handoff before=%08x:%08x "
	       "patch=%08x:%08x\n", before.hi, before.lo, address.hi, address.lo);
	console_tx_flush();
	/* The original DXE loader clears this MSR before selecting the patch. */
	wrmsr(VERMEER_UCODE_ADDRESS_MSR, (msr_t) { .raw = 0 });
	cleared = rdmsr(VERMEER_UCODE_ADDRESS_MSR);
	printk(cleared.raw ? BIOS_WARNING : BIOS_INFO,
	       "Vermeer microcode: initial clear readback=%08x:%08x match=%u\n",
	       cleared.hi, cleared.lo, cleared.raw == 0);
	console_tx_flush();
	wrmsr(VERMEER_UCODE_ADDRESS_MSR, address);
	after = rdmsr(VERMEER_UCODE_ADDRESS_MSR);
	printk(after.raw == address.raw ? BIOS_INFO : BIOS_WARNING,
	       "Vermeer microcode: address handoff readback=%08x:%08x match=%u\n",
	       after.hi, after.lo, after.raw == address.raw);
	console_tx_flush();
}
