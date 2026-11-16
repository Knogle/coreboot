/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/cpu.h>
#include <amdblocks/smu.h>
#include <arch/io.h>
#include <cbmem.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <cpu/cpu.h>
#include <cpu/x86/cache.h>
#include <cpu/x86/lapic.h>
#include <cpu/x86/mp.h>
#include <cpu/x86/msr.h>
#include <cpu/x86/mtrr.h>
#include <delay.h>
#include <soc/smu.h>
#include <string.h>
#include <timer.h>
#include "cppc_pcc.h"
#include "pcc_contract.h"

#define CPPC_CBMEM_ID 0x43505043
#define PCC_OWNER_BYTES (2 * B565D4_PCC_BYTES)
#define MAX_VARIABLE_MTRRS 16
#define CPPC_INFORMATION_384C 0x006be3bf
#define CPPC_SET_TABLE_MESSAGE 0x16
#define PCC_SIGNATURE 0x50434300
#define PCC_DOORBELL 0xfd010540

struct cpu_cache_state {
	bool seen;
	bool changed;
	bool exact;
	unsigned int apic_id;
	unsigned int count;
	unsigned int physical_bits;
	unsigned long cr0;
	msr_t default_type;
	msr_t base[MAX_VARIABLE_MTRRS];
	msr_t mask[MAX_VARIABLE_MTRRS];
};

static struct cpu_cache_state cpus[B565D4_PCC_MAX_CPUS];
static struct b565d4_pcc_plan plan;
static unsigned int cpu_count;
static unsigned int mtrr_index;
static bool attempted;
static bool verified;
static struct b565d4_cppc_pcc_state exported_state;

bool b565d4_vermeer_cppc_pcc_get_state(struct b565d4_cppc_pcc_state *out)
{
	if (!verified || !out)
		return false;
	*out = exported_state;
	return true;
}


static void collect_cache_state(void *unused)
{
	const unsigned int index = cpu_index();
	struct cpu_cache_state *state;

	if (index >= cpu_count)
		return;
	state = &cpus[index];
	if (state->seen)
		return;
	state->apic_id = lapicid();
	state->count = get_var_mtrr_count();
	state->physical_bits = cpu_phys_address_size();
	state->cr0 = read_cr0();
	state->default_type = rdmsr(MTRR_DEF_TYPE_MSR);
	if (!state->count || state->count > MAX_VARIABLE_MTRRS)
		return;
	for (unsigned int i = 0; i < state->count; i++) {
		state->base[i] = rdmsr(MTRR_PHYS_BASE(i));
		state->mask[i] = rdmsr(MTRR_PHYS_MASK(i));
	}
	state->seen = true;
}

static bool select_common_unused_mtrr(void)
{
	for (unsigned int cpu = 0; cpu < cpu_count; cpu++) {
		const struct cpu_cache_state *state = &cpus[cpu];

		/* RAMstage is running without paging and with caches enabled. No
		 * other CPU has accessed the newly allocated private PCC interval.
		 */
		if (!state->seen || state->count != cpus[0].count ||
		    state->physical_bits != cpus[0].physical_bits ||
		    state->default_type.raw != cpus[0].default_type.raw ||
		    !(state->default_type.lo & MTRR_DEF_TYPE_EN) ||
		    (state->cr0 & (CR0_CD | CR0_NW | CR0_PG)))
			return false;
		for (unsigned int other = 0; other < cpu; other++)
			if (state->apic_id == cpus[other].apic_id)
				return false;
		for (unsigned int i = 0; i < state->count; i++)
			if (state->base[i].raw != cpus[0].base[i].raw ||
			    state->mask[i].raw != cpus[0].mask[i].raw)
				return false;
	}
	for (unsigned int i = 0; i < cpus[0].count; i++) {
		if (!(cpus[0].mask[i].lo & MTRR_PHYS_MASK_VALID)) {
			mtrr_index = i;
			return true;
		}
	}
	return false;
}

static void configure_private_uc(void *unused)
{
	const unsigned int index = cpu_index();
	struct cpu_cache_state *state;

	if (index >= cpu_count || !cpus[index].seen)
		return;
	state = &cpus[index];
	/* Only an unused variable MTRR changes. All existing mappings and the
	 * global default remain intact. Flush private cached initialization before
	 * registering the interval with the SMU. Restore the exact incoming CR0.
	 */
	write_cr0((state->cr0 | CR0_CD) & ~CR0_NW);
	wbinvd();
	state->changed = true;
	set_var_mtrr(mtrr_index, plan.shared_begin, B565D4_PCC_BYTES, MTRR_TYPE_UNCACHEABLE);
	wbinvd();
	write_cr0(state->cr0);
	state->exact = rdmsr(MTRR_PHYS_BASE(mtrr_index)).raw == plan.uc_base &&
		rdmsr(MTRR_PHYS_MASK(mtrr_index)).raw == plan.uc_mask;
}

static void restore_private_uc(void *unused)
{
	const unsigned int index = cpu_index();
	struct cpu_cache_state *state;

	if (index >= cpu_count || !cpus[index].changed)
		return;
	state = &cpus[index];
	write_cr0((state->cr0 | CR0_CD) & ~CR0_NW);
	wbinvd();
	/* Invalidate the newly introduced range before restoring its old bytes. */
	wrmsr(MTRR_PHYS_MASK(mtrr_index), (msr_t) { .raw = 0 });
	wrmsr(MTRR_PHYS_BASE(mtrr_index), state->base[mtrr_index]);
	wrmsr(MTRR_PHYS_MASK(mtrr_index), state->mask[mtrr_index]);
	wbinvd();
	write_cr0(state->cr0);
	state->exact = rdmsr(MTRR_PHYS_BASE(mtrr_index)).raw == state->base[mtrr_index].raw &&
		rdmsr(MTRR_PHYS_MASK(mtrr_index)).raw == state->mask[mtrr_index].raw;
}

static bool private_uc_ready(void)
{
	bool complete = mp_run_on_all_cpus_synchronously(configure_private_uc, NULL) == CB_SUCCESS;

	for (unsigned int i = 0; i < cpu_count; i++)
		complete &= cpus[i].changed && cpus[i].exact;
	if (complete)
		return true;
	/* No SMU registration has occurred. Retain the CBMEM owner even if the
	 * optional cache setup fails; never hand an ambiguous interval to the OS.
	 */
	const enum cb_err restored = mp_run_on_all_cpus_synchronously(restore_private_uc, NULL);
	printk(BIOS_WARNING, "Vermeer PCC: UC setup incomplete; restore dispatch=%d\n", restored);
	for (unsigned int i = 0; i < cpu_count; i++)
		if (cpus[i].changed && !cpus[i].exact)
			printk(BIOS_WARNING, "Vermeer PCC: CPU%u UC restore readback failed\n", i);
	return false;
}

void b565d4_vermeer_cppc_pcc_prepare(uint32_t information,
				 unsigned int nominal_mhz, unsigned int lowest_mhz)
{
	struct smu_payload payload = { .msg = {0} };
	struct b565d4_cppc_package package;
	struct stopwatch timeout;
	const struct cbmem_entry *entry;
	void *owner;
	uint8_t *shared;
	uint8_t snapshot[8 + B565D4_PCC_MAX_CPUS * B565D4_PCC_CPU_STRIDE];
	bool cache_ready;

	if (attempted)
		return;
	attempted = true;
	cpu_count = get_cpu_count();
	if (information != CPPC_INFORMATION_384C || !cpu_count ||
	    cpu_count > B565D4_PCC_MAX_CPUS ||
	    !b565d4_cppc_package(information, 0, lowest_mhz, nominal_mhz, &package)) {
		printk(BIOS_WARNING, "Vermeer PCC: service description unavailable\n");
		return;
	}
	if (mp_run_on_all_cpus_synchronously(collect_cache_state, NULL) != CB_SUCCESS ||
	    !select_common_unused_mtrr()) {
		printk(BIOS_WARNING, "Vermeer PCC: common CPU cache state unavailable\n");
		return;
	}
	/* cbmem_add can return an existing ID without checking its size. A PCC
	 * owner must be fresh and its actual entry must cover the requested span.
	 */
	if (cbmem_entry_find(CPPC_CBMEM_ID)) {
		printk(BIOS_WARNING, "Vermeer PCC: private CBMEM ID already exists\n");
		return;
	}
	owner = cbmem_add(CPPC_CBMEM_ID, PCC_OWNER_BYTES);
	entry = cbmem_entry_find(CPPC_CBMEM_ID);
	if (!owner || !entry || cbmem_entry_start(entry) != owner ||
	    cbmem_entry_size(entry) != PCC_OWNER_BYTES ||
	    !b565d4_pcc_plan((uintptr_t)owner, (uintptr_t)owner + PCC_OWNER_BYTES,
				     cpus[0].physical_bits, cpu_count, &plan)) {
		printk(BIOS_WARNING, "Vermeer PCC: private CBMEM allocation unavailable\n");
		return;
	}
	shared = (void *)(uintptr_t)plan.shared_begin;
	memset(shared, 0, B565D4_PCC_BYTES);
	write32(shared, PCC_SIGNATURE);
	cache_ready = private_uc_ready();
	if (!cache_ready)
		return;
	printk(BIOS_NOTICE, "Vermeer PCC: owner=%p+%x shared=%p+%x UC%u all%u CPUs exact\n",
	       owner, PCC_OWNER_BYTES, shared, B565D4_PCC_BYTES, mtrr_index, cpu_count);
	payload.msg[0] = plan.shared_begin;
	payload.msg[1] = plan.shared_begin >> 32;
	/* Original SMU384c message16 takes the PCC address directly. There is no
	 * generic DRAM-address setter or PowerPlay-table transfer in this path.
	 * Keep this allocation and UC mapping alive even after an ambiguous reply.
	 */
	if (send_smu_message(CPPC_SET_TABLE_MESSAGE, &payload) != CB_SUCCESS) {
		printk(BIOS_WARNING, "Vermeer PCC: table registration failed; no retry\n");
		return;
	}
	printk(BIOS_NOTICE, "Vermeer PCC: registration accepted header=%08x/%08x\n",
	       read32(shared), read32(shared + 4));
	if (read32(shared) != PCC_SIGNATURE || read32(shared + 4) != 0x00010000) {
		printk(BIOS_WARNING, "Vermeer PCC: registration header incomplete\n");
		return;
	}
	/* One standard command0/read request. PCCT preserves the doorbell's upper
	 * dword and replaces its lower dword with1. A lower-dword store performs
	 * that operation without rewriting the preserved upper register half.
	 */
	write16(shared + 4, 0);
	write16(shared + 6, 0);
	asm volatile ("mfence" ::: "memory");
	write32((void *)(uintptr_t)PCC_DOORBELL, 1);
	stopwatch_init_msecs_expire(&timeout, 400);
	while (!(read16(shared + 6) & 1) && !stopwatch_expired(&timeout))
		udelay(10);
	asm volatile ("mfence" ::: "memory");
	memcpy(snapshot, shared, sizeof(snapshot));
	const bool reply = b565d4_pcc_reply(snapshot, sizeof(snapshot), cpu_count);
	if (reply) {
		exported_state = (struct b565d4_cppc_pcc_state) {
			.shared_base = plan.shared_begin,
			.information = information,
			.nominal_mhz = nominal_mhz,
			.lowest_mhz = lowest_mhz,
			.cpu_count = cpu_count,
		};
		verified = true;
	}

	printk(BIOS_NOTICE, "Vermeer PCC: finite read status=%04x allCPUcapabilities=%u; "
	       "OS interface not published\n", read16(shared + 6), reply);
	for (unsigned int slot = 0; slot < cpu_count; slot++) {
		const uint8_t *p = shared + 8 + slot * B565D4_PCC_CPU_STRIDE;

		printk(BIOS_NOTICE, "Vermeer PCC: slot%u highest=%u nominal=%u nonlinear=%u lowest=%u\n",
		       slot, read32(p), read32(p + 4), read32(p + 8), read32(p + 12));
	}
}
