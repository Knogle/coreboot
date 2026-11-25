/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/cpu.h>
#include <amdblocks/smu.h>
#include <amdblocks/vermeer_smu_cc6.h>
#include <arch/cpu.h>
#include <console/console.h>
#include <cpu/amd/msr.h>
#include <cpu/cpu.h>
#include <cpu/x86/msr.h>
#include <soc/smu.h>

#define CORE_C6_CONTROL_MSR 0xc0010296
#define CORE_C6_CONTROL_BASE 0x080808ULL
#define CORE_C6_ENABLE_MASK 0x404040ULL

/* Each normal MP callback owns one slot. The ordinary MP join completes
 * before the BSP checks all slots at BS_WRITE_TABLES.
 */
static uint8_t cpu_prepared[CONFIG_MAX_CPUS];
static bool attempted;
static bool ready;

void vermeer_smu_cc6_cpu_prepare(uint16_t cstate_io_base)
{
	const unsigned long index = cpu_index();
	const struct cpuid_result topology = cpuid(0x8000001e);
	const unsigned int threads = ((topology.ebx >> 8) & 0xff) + 1;
	const unsigned int apic = cpuid(1).ebx >> 24;
	msr_t before, requested, after;

	if (cpuid_eax(1) != 0x00a20f12 || index >= CONFIG_MAX_CPUS)
		return;
	if (cpu_prepared[index]) {
		cpu_prepared[index] = 2;
		return;
	}
	cpu_prepared[index] = 2;
	if (threads > 2)
		return;
	if (apic % threads == 0) {
		/* Original Zen3 DXE4260h writes 080808h plus 404040h when
		 * its C6 policy is enabled. Only those enable bits change here.
		 */
		before = rdmsr(CORE_C6_CONTROL_MSR);
		if ((before.raw & ~CORE_C6_ENABLE_MASK) != CORE_C6_CONTROL_BASE)
			return;
		requested.raw = before.raw | CORE_C6_ENABLE_MASK;
		if (requested.raw != before.raw)
			wrmsr(CORE_C6_CONTROL_MSR, requested);
		after = rdmsr(CORE_C6_CONTROL_MSR);
		if (after.raw != requested.raw)
			return;
	}
	before = rdmsr(MSR_CSTATE_ADDRESS);
	requested.raw = (before.raw & ~0xffffULL) | cstate_io_base;
	if (requested.raw != before.raw)
		wrmsr(MSR_CSTATE_ADDRESS, requested);
	after = rdmsr(MSR_CSTATE_ADDRESS);
	if (after.raw != requested.raw)
		return;
	cpu_prepared[index] = 1;
}

void vermeer_smu_cc6_enable(void)
{
	struct smu_payload payload = { .msg = {1U << 17} };
	const unsigned int count = get_cpu_count();

	if (attempted)
		return;
	attempted = true;
	if (!count || count > CONFIG_MAX_CPUS)
		return;
	for (unsigned int i = 0; i < count; i++) {
		if (cpu_prepared[i] != 1) {
			printk(BIOS_WARNING, "Vermeer CC6: CPU preparation incomplete; retain C1\n");
			return;
		}
	}
	/* The qualified SMU384c caller has imported PowerPlay. Original feature
	 * bit17 dispatches CC6 initialization through CCX RPC2dh. A mailbox
	 * acknowledgement permits a separate OS trial, not a residency claim.
	 */
	if (send_smu_message(SMC_MSG_ENABLE_SMU_FEATURES, &payload) != CB_SUCCESS) {
		printk(BIOS_WARNING, "Vermeer CC6: feature request failed; retain C1\n");
		return;
	}
	ready = true;
	printk(BIOS_NOTICE, "Vermeer CC6: all %u CPUs prepared, feature accepted\n", count);
}

bool vermeer_smu_cc6_ready(void)
{
	return ready;
}
