/* SPDX-License-Identifier: GPL-2.0-only */

#include <console/console.h>
#include <cpu/cpu.h>
#include <cpu/intel/microcode.h>
#include <cpu/x86/cr.h>
#include <cpu/x86/lapic.h>
#include <cpu/x86/mp.h>
#include <cpu/x86/msr.h>
#include <cpu/x86/mtrr.h>
#include <device/device.h>
#include <string.h>
#include <types.h>

#include "cpu_init.h"
#include "platform_policy.h"
#include "mca_diagnostic.h"

/* Neutral names: no public bit-field semantics or lock behavior are claimed
 * for MSR 0x2e2. The one exact BSP write below is a separately authorized
 * path derived from an observed vendor package-scope write of BIT(1).
 * It is not a generic Westmere initialization recipe or a demonstrated fix.
 */
#define MAX_VARIABLE_MTRRS 16
#define REQUIRED_CPUID_EDX (BIT(5) | BIT(9) | BIT(12) | BIT(14) | BIT(16))

static const unsigned int fixed_mtrrs[] = {
	MTRR_FIX_64K_00000, MTRR_FIX_16K_80000, MTRR_FIX_16K_A0000,
	MTRR_FIX_4K_C0000, MTRR_FIX_4K_C8000, MTRR_FIX_4K_D0000,
	MTRR_FIX_4K_D8000, MTRR_FIX_4K_E0000, MTRR_FIX_4K_E8000,
	MTRR_FIX_4K_F0000, MTRR_FIX_4K_F8000,
};

static struct {
	msr_t cap;
	msr_t def_type;
	msr_t pat;
	msr_t fixed[NUM_FIXED_MTRRS];
	msr_t variable[2 * MAX_VARIABLE_MTRRS];
	unsigned int variable_count;
} bsp_cache;

static struct model_206cx_topology bsp_topology;
static const void *microcode_patch;
static unsigned int microcode_revision;
static unsigned int requested_cpus;
static unsigned int initialized_cpus;
static unsigned int verified_apic_ids[CONFIG_MAX_CPUS];
static bool verified[CONFIG_MAX_CPUS];
static bool attempted;

static bool same_msr(msr_t a, msr_t b)
{
	return a.lo == b.lo && a.hi == b.hi;
}

static void require_cpu_identity(void)
{
	const struct cpuid_result vendor = cpuid(0);
	const struct cpuid_result features = cpuid(1);

	/* CPUID vendor string is EBX:EDX:ECX, "GenuineIntel". */
	if (vendor.ebx != 0x756e6547 || vendor.edx != 0x49656e69 ||
	    vendor.ecx != 0x6c65746e || vendor.eax < 0x0b ||
	    features.eax != MODEL_206CX_SIGNATURE ||
	    (features.edx & REQUIRED_CPUID_EDX) != REQUIRED_CPUID_EDX)
		die("[CPU] Unsupported CPU/features; no platform write\n");
}

static struct model_206cx_topology read_topology(void)
{
	const struct cpuid_result smt = cpuid_ext(0x0b, 0);
	const struct cpuid_result core = cpuid_ext(0x0b, 1);
	const struct cpuid_result end = cpuid_ext(0x0b, 2);
	struct model_206cx_topology topology;

	if (!model_206cx_decode_topology(smt.eax, smt.ebx, smt.ecx, smt.edx,
		core.eax, core.ebx, core.ecx, core.edx, end.ebx, end.ecx, &topology))
		die("[CPU] Unsupported CPUID.0b topology; no AP-ID guesses\n");
	if (smt.edx != lapicid())
		die("[CPU] CPUID/LAPIC identity mismatch\n");
	return topology;
}

/* Read and retain error evidence. Do not clear status, enable MCA banks,
 * change exception policy, or label stale/pending records harmless.
 */
void __weak model_206cx_mca_fault(void)
{
	/* The caller still halts if no board-specific console is available. */
}

static void require_no_pending_mca(const char *phase)
{
	const bool clean = !strcmp(phase, "CPU") ?
		model_206cx_mca_report_compact(phase) : model_206cx_mca_report(phase);

	if (clean)
		return;
	printk(BIOS_EMERG, "[CPU] Pending MCA evidence retained; stopping, no retry\n");
	/* Only the BSP's PRE check can enter a console: no APs have been sent
	 * INIT/SIPI and no package policy write has been attempted at this point.
	 * A later failure must not contend with APs for UART or shared state.
	 */
	if (!strcmp(phase, "PRE"))
		model_206cx_mca_fault();
	die("[CPU] MCA failure; cold recovery required\n");
}

static void snapshot_bsp_cache(void)
{
	bsp_cache.cap = rdmsr(MTRR_CAP_MSR);
	bsp_cache.variable_count = bsp_cache.cap.lo & MTRR_CAP_VCNT;
	bsp_cache.def_type = rdmsr(MTRR_DEF_TYPE_MSR);
	bsp_cache.pat = rdmsr(IA32_PAT);
	if (!(bsp_cache.cap.lo & MTRR_CAP_FIX) || !bsp_cache.variable_count ||
	    bsp_cache.variable_count > MAX_VARIABLE_MTRRS ||
	    !(bsp_cache.def_type.lo & MTRR_DEF_TYPE_EN) ||
	    (read_cr0() & (CR0_CD | CR0_NW)))
		die("[CPU] BSP cache policy not ready for AP startup\n");
	for (unsigned int i = 0; i < ARRAY_SIZE(fixed_mtrrs); i++)
		bsp_cache.fixed[i] = rdmsr(fixed_mtrrs[i]);
	for (unsigned int i = 0; i < 2 * bsp_cache.variable_count; i++)
		bsp_cache.variable[i] = rdmsr(MTRR_PHYS_BASE(0) + i);
	printk(BIOS_NOTICE,
	       "[CPU] BSP cache snapshot: var=%u DEF=%08x:%08x PAT=%08x:%08x\n",
	       bsp_cache.variable_count, bsp_cache.def_type.hi, bsp_cache.def_type.lo,
	       bsp_cache.pat.hi, bsp_cache.pat.lo);
}

static void verify_cache(void)
{
	if (!same_msr(rdmsr(MTRR_CAP_MSR), bsp_cache.cap) ||
	    !same_msr(rdmsr(MTRR_DEF_TYPE_MSR), bsp_cache.def_type) ||
	    !same_msr(rdmsr(IA32_PAT), bsp_cache.pat) ||
	    (read_cr0() & (CR0_CD | CR0_NW)))
		die("[CPU] Cache capability/DEF/PAT mismatch\n");
	for (unsigned int i = 0; i < ARRAY_SIZE(fixed_mtrrs); i++)
		if (!same_msr(rdmsr(fixed_mtrrs[i]), bsp_cache.fixed[i]))
			die("[CPU] Fixed MTRR mismatch\n");
	for (unsigned int i = 0; i < 2 * bsp_cache.variable_count; i++)
		if (!same_msr(rdmsr(MTRR_PHYS_BASE(0) + i), bsp_cache.variable[i]))
			die("[CPU] Variable MTRR mismatch\n");
}

static void package_write_once(bool requested)
{
	const msr_t value = { .lo = MODEL_206CX_MSR_2E2_VALUE, .hi = 0 };
	static bool decided;

	if (decided)
		die("[CPU] MSR 2e2 decision already consumed; no retry\n");
	decided = true;
	if (!requested) {
		printk(BIOS_WARNING, "[CPU] MSR 2e2 SKIPPED runtime request OFF; no access\n");
		return;
	}

	printk(BIOS_WARNING,
	       "[CPU] MSR 2e2 BEFORE_WRMSR VALUE=00000000:00000002 UNQUALIFIED\n");
	/* Exactly one attempt, on the BSP before any AP startup; not an RMW.
	 * The inspected vendor sequence only writes. Neither readability nor
	 * equality-readback semantics are established: do not add RDMSR here.
	 * A fault requires external cold recovery. There is no retry, rollback
	 * write, or resume path. RETURNED does not verify value or side effects.
	 */
	wrmsr(MODEL_206CX_MSR_2E2, value);
	printk(BIOS_WARNING, "[CPU] MSR 2e2 WRMSR_RETURNED VALUE_UNVERIFIED\n");
}

static void model_206cx_cpu_device_init(struct device *cpu)
{
	const unsigned int index = cpu_info()->index;
	struct model_206cx_topology topology;
	unsigned int revision;

	require_cpu_identity();
	topology = read_topology();
	if (index >= requested_cpus || verified[index] ||
	    topology.threads != bsp_topology.threads ||
	    topology.smt_shift != bsp_topology.smt_shift ||
	    topology.package_shift != bsp_topology.package_shift ||
	    topology.package_id != bsp_topology.package_id ||
	    cpu->path.apic.apic_id != lapicid())
		die("[CPU] Unexpected CPU slot/package/topology\n");
	require_no_pending_mca("CPU");
	verify_cache();
	revision = get_current_microcode_rev();
	/* Generic SIPI serializes updates for HT siblings. It does not replace
	 * an AP's existing nonzero revision. Reject a mismatch instead of
	 * inventing an unsafe parallel update/downgrade or retry policy here.
	 */
	if (revision != microcode_revision)
		die("[CPU] APIC=%02x MCU=%08x expected=%08x; no retry\n",
		    lapicid(), revision, microcode_revision);
	verified_apic_ids[index] = lapicid();
	verified[index] = true;
	printk(BIOS_NOTICE,
	       "[CPU] slot=%u APIC=%02x package=%u core=%u thread=%u MCU=%08x cache-match\n",
	       index, lapicid(), cpu->path.apic.package_id, cpu->path.apic.core_id,
	       cpu->path.apic.thread_id, revision);
}

static struct device_operations model_206cx_cpu_ops = {
	.init = model_206cx_cpu_device_init,
};

static const struct cpu_device_id model_206cx_cpu_table[] = {
	{ X86_VENDOR_INTEL, MODEL_206CX_SIGNATURE, CPUID_EXACT_MATCH_MASK },
	CPU_TABLE_END
};

static const struct cpu_driver model_206cx_cpu_driver __cpu_driver = {
	.ops = &model_206cx_cpu_ops,
	.id_table = model_206cx_cpu_table,
};

static int get_cpu_count(void)
{
	return requested_cpus;
}

static void get_microcode_info(const void **microcode, size_t *size, int *parallel)
{
	*microcode = microcode_patch;
	*size = get_microcode_size(microcode_patch);
	*parallel = 0;
}

static const struct mp_ops model_206cx_mp_ops = {
	.get_cpu_count = get_cpu_count,
	.get_microcode_info = get_microcode_info,
	/* No SMM callbacks: NO_SMM remains the platform contract. */
};

void model_206cx_init(struct bus *cpu_bus, bool smp_requested,
				 bool msr2e2_requested)
{
	msr_t apic_base;
	unsigned int old_revision;

	if (!CONFIG(CPU_INTEL_MODEL_206CX_INIT) || !CONFIG(NO_SMM) ||
	    CONFIG(HAVE_SMI_HANDLER) || CONFIG(PARALLEL_MP_AP_WORK) ||
	    !CONFIG(XAPIC_ONLY) || !CONFIG(X86_MP_INIT_INHERIT_PAT) ||
	    !CONFIG(X86_MP_INIT_REQUIRE_ALL_CPUS) || attempted || !cpu_bus || cpu_bus->children)
		die("[CPU] Invalid/repeated initialization context\n");
	attempted = true;
	require_cpu_identity();
	apic_base = rdmsr(LAPIC_BASE_MSR);
	if (apic_base.hi || apic_base.lo != (LAPIC_DEFAULT_BASE | LAPIC_BASE_MSR_ENABLE |
					    LAPIC_BASE_MSR_BOOTSTRAP_PROCESSOR))
		die("[CPU] Requires BSP, default xAPIC base, xAPIC enabled\n");
	bsp_topology = read_topology();
	if (smp_requested && (!CONFIG(CPU_INTEL_MODEL_206CX_SMP) || !CONFIG(SMP)))
		die("[CPU] SMP requested in a BSP-only build\n");
	requested_cpus = smp_requested ? bsp_topology.threads : 1;
	if (requested_cpus > CONFIG_MAX_CPUS)
		die("[CPU] CPUID CPU count exceeds allocated slots\n");
	printk(BIOS_WARNING,
	       "[CPU] CPUID=206c2 package=%u detected=%u selected=%u mode=%s NO_SMM\n",
	       bsp_topology.package_id, bsp_topology.threads, requested_cpus,
	       smp_requested ? "SMP" : "BSP-only (no AP IPIs)");
	require_no_pending_mca("PRE");
	microcode_patch = intel_microcode_find();
	if (!microcode_patch)
		die("[CPU] No matching CBFS microcode; no platform write\n");
	microcode_revision = get_microcode_rev(microcode_patch);
	old_revision = get_current_microcode_rev();
	printk(BIOS_NOTICE, "[CPU] BSP MCU PRE=%08x CBFS=%08x\n",
	       old_revision, microcode_revision);
	if (!microcode_revision || old_revision > microcode_revision)
		die("[CPU] Invalid microcode/downgrade refused\n");
	if (old_revision != microcode_revision)
		intel_microcode_load_unlocked(microcode_patch);
	if (get_current_microcode_rev() != microcode_revision)
		die("[CPU] BSP microcode readback mismatch; no retry\n");
	printk(BIOS_NOTICE, "[CPU] BSP MCU POST=%08x\n", microcode_revision);
	snapshot_bsp_cache();
	package_write_once(msr2e2_requested);
	require_no_pending_mca("POST-2E2");
	verify_cache();
	/* Generic MP uses the reserved 0x30000 SIPI area. Its BSP waits are
	 * bounded (50 ms/AP startup, >=1 s/flight record). On any failure,
	 * halt; never continue into ACPI/payload with uncertain AP state.
	 */
	if (mp_init_with_smm(cpu_bus, &model_206cx_mp_ops) != CB_SUCCESS)
		die("[CPU] Bounded MP initialization failed; cold recovery required\n");
	for (unsigned int i = 0; i < requested_cpus; i++) {
		if (!verified[i])
			die("[CPU] CPU driver did not verify every requested CPU\n");
		for (unsigned int j = 0; j < i; j++)
			if (verified_apic_ids[i] == verified_apic_ids[j])
				die("[CPU] Duplicate observed APIC ID\n");
	}
	initialized_cpus = requested_cpus;
	printk(BIOS_NOTICE,
	       "[CPU] initialized=%u; MTRR/PAT/MCU matched; APs handed to generic park\n",
	       initialized_cpus);
}

unsigned int model_206cx_cpu_count(void)
{
	return initialized_cpus;
}

unsigned int model_206cx_apic_id(unsigned int index)
{
	if (index >= initialized_cpus)
		die("[CPU] ACPI requested an unverified CPU\n");
	return verified_apic_ids[index];
}
