/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/smn.h>
#include <amdblocks/smu.h>
#include <arch/cpu.h>
#include <console/console.h>
#include <cpu/x86/msr.h>
#include <string.h>
#include <amdblocks/vermeer_cpu_name.h>

#define CPU_NAME_BYTES 48
#define CPU_NAME_MSR 0xc0010030
#define QUALIFIED_SMU_VERSION 0x00384c00

static char cpu_name[CPU_NAME_BYTES];
static bool cpu_name_ready;
static bool cpu_name_attempted;

void vermeer_cpu_name_apply(void)
{
	if (!cpu_name_ready || cpuid_eax(1) != 0x00a20f12)
		return;

	/* Original Zen3 CCX PEI copies the same six name MSRs on each thread. */
	for (unsigned int i = 0; i < CPU_NAME_BYTES / sizeof(msr_t); i++) {
		msr_t value;

		memcpy(&value, cpu_name + i * sizeof(value), sizeof(value));
		if (rdmsr(CPU_NAME_MSR + i).raw != value.raw)
			wrmsr(CPU_NAME_MSR + i, value);
		if (rdmsr(CPU_NAME_MSR + i).raw != value.raw) {
			printk(BIOS_WARNING, "Vermeer CPU name: APIC%u readback failed\n",
			       cpuid(1).ebx >> 24);
			return;
		}
	}
}

void vermeer_cpu_name_prepare(void)
{
	struct smu_payload payload = { .msg = {0} };
	char name[CPU_NAME_BYTES];
	bool ended = false, nonblank = false;

	/* Called once by the admitted BSP before genuine MP startup. APs only
	 * consume the completed cache; they never contend for the SMU mailbox.
	 */
	if (cpu_name_attempted)
		return;
	cpu_name_attempted = true;
	if (cpuid_eax(1) != 0x00a20f12 || smn_read32(SMN_SMU_MESG_RESP) != 1)
		return;
	if (send_smu_message(SMC_MSG_GET_SMU_VERSION, &payload) != CB_SUCCESS ||
	    payload.msg[0] != QUALIFIED_SMU_VERSION)
		return;

	/* Original SmuV11PeiVMR PPI +30h uses BIOS0d with word indices0..11.
	 * The exact SMU384c handler bounds the index and reads four name bytes.
	 * CCX PEI requests47 bytes, retaining a zero final byte in the48-byte MSR
	 * image. Query all words first so a failure cannot publish a partial name.
	 */
	for (unsigned int i = 0; i < CPU_NAME_BYTES / sizeof(uint32_t); i++) {
		payload = (struct smu_payload) { .msg = {i} };
		if (send_smu_message(SMC_MSG_GET_CPU_NAME, &payload) != CB_SUCCESS) {
			printk(BIOS_WARNING, "Vermeer CPU name: SMU query failed\n");
			return;
		}
		memcpy(name + i * sizeof(uint32_t), &payload.msg[0], sizeof(uint32_t));
	}
	name[CPU_NAME_BYTES - 1] = '\0';
	if (!name[0])
		return;
	for (unsigned int i = 0; i < CPU_NAME_BYTES - 1; i++) {
		if (!name[i]) {
			ended = true;
			continue;
		}
		if (name[i] < ' ' || name[i] > '~' || (ended && name[i] != ' ')) {
			printk(BIOS_WARNING, "Vermeer CPU name: invalid SMU text\n");
			return;
		}
		nonblank |= name[i] != ' ';
	}
	if (!nonblank)
		return;
	memcpy(cpu_name, name, sizeof(cpu_name));
	cpu_name_ready = true;
	vermeer_cpu_name_apply();
	printk(BIOS_NOTICE, "Vermeer CPU name: %s\n", cpu_name);
}
