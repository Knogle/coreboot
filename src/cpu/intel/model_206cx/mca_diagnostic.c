/* SPDX-License-Identifier: GPL-2.0-only */

#include <console/console.h>
#include <cpu/cpu.h>
#include <cpu/x86/cr.h>
#include <cpu/x86/msr.h>

#include "platform_policy.h"
#include "mca_diagnostic.h"

#define MODEL_206CX_MCA_BANKS 9
#define MCA_REQUIRED_CPUID_EDX ((1U << 5) | (1U << 14))
/* Intel SDM, architectural IA32_MCG_CTL; present only with MCG_CTL_P. */
#define MODEL_206CX_MCG_CTL 0x17b

static bool same_status(msr_t first, msr_t second)
{
	return first.lo == second.lo && first.hi == second.hi;
}

bool model_206cx_mca_report(const char *phase)
{
	const struct cpuid_result vendor = cpuid(0);
	struct cpuid_result features;
	msr_t cap, global, apic, global_after;
	bool clean;

	if (vendor.eax < 1 || vendor.ebx != 0x756e6547 ||
	    vendor.edx != 0x49656e69 || vendor.ecx != 0x6c65746e) {
		printk(BIOS_NOTICE, "[MCA-RO] %s unsupported CPUID vendor/leaf; no MSR reads\n",
		       phase);
		return false;
	}
	features = cpuid(1);
	if (features.eax != MODEL_206CX_SIGNATURE ||
	    (features.edx & MCA_REQUIRED_CPUID_EDX) != MCA_REQUIRED_CPUID_EDX) {
		printk(BIOS_NOTICE, "[MCA-RO] %s unsupported CPU/MSR/MCA; no MSR reads\n",
		       phase);
		return false;
	}

	cap = rdmsr(IA32_MCG_CAP);
	printk(BIOS_NOTICE, "[MCA-RO] %s CPUID=%08x initial-APIC=%02x MCG_CAP=%08x:%08x\n",
	       phase, features.eax, features.ebx >> 24, cap.hi, cap.lo);
	if ((cap.lo & MCA_BANKS_MASK) != MODEL_206CX_MCA_BANKS) {
		printk(BIOS_NOTICE, "[MCA-RO] %s unsupported bank count; no bank reads\n", phase);
		return false;
	}
	apic = rdmsr(IA32_APIC_BASE_MSR_INDEX);
	global = rdmsr(IA32_MCG_STATUS);
	clean = !global.hi && !global.lo;
	printk(BIOS_NOTICE,
	       "[MCA-RO] %s CR0=%08lx CR4=%08lx APIC_BASE=%08x:%08x MCG_STATUS=%08x:%08x\n",
	       phase, (unsigned long)read_cr0(), (unsigned long)read_cr4(),
	       apic.hi, apic.lo, global.hi, global.lo);
	/* Intel SDM Vol. 3B, 16.8: after power up/cycling,
	 * MCi_STATUS data is not guaranteed valid until software initializes it.
	 * This reporter does not initialize MCA or assert a record's origin.
	 */
	printk(BIOS_NOTICE,
	       "[MCA-RO] %s preserved; reset data unqualified until MCA initialization; non-atomic sample\n",
	       phase);
	if (cap.lo & MCG_CTL_P) {
		const msr_t ctl = rdmsr(MODEL_206CX_MCG_CTL);

		printk(BIOS_NOTICE, "[MCA-RO] %s MCG_CTL=%08x:%08x\n", phase, ctl.hi, ctl.lo);
	}
	for (unsigned int bank = 0; bank < MODEL_206CX_MCA_BANKS; bank++) {
		const msr_t ctl = rdmsr(IA32_MC_CTL(bank));
		const msr_t status = rdmsr(IA32_MC_STATUS(bank));
		const bool valid = !!(status.hi & MCA_STATUS_HI_VAL);
		msr_t after;

		printk(BIOS_NOTICE,
		       "[MCA-RO] %s MC%u_CTL=%08x:%08x STATUS=%08x:%08x "
		       "VAL=%u OVER=%u UC=%u EN=%u MISCV=%u ADDRV=%u PCC=%u "
		       "MCACOD=%04x MSCOD=%04x\n",
		       phase, bank, ctl.hi, ctl.lo, status.hi, status.lo,
		       (unsigned int)valid,
		       (unsigned int)!!(status.hi & MCA_STATUS_HI_OVERFLOW),
		       (unsigned int)!!(status.hi & MCA_STATUS_HI_UC),
		       (unsigned int)!!(status.hi & MCA_STATUS_HI_EN),
		       (unsigned int)!!(status.hi & MCA_STATUS_HI_MISCV),
		       (unsigned int)!!(status.hi & MCA_STATUS_HI_ADDRV),
		       (unsigned int)!!(status.hi & MCA_STATUS_HI_PCC),
		       status.lo & 0xffff, status.lo >> 16);
		if (valid) {
			clean = false;
			if (status.hi & MCA_STATUS_HI_ADDRV) {
				const msr_t address = rdmsr(IA32_MC_ADDR(bank));

				printk(BIOS_NOTICE, "[MCA-RO] %s MC%u_ADDR=%08x:%08x\n",
				       phase, bank, address.hi, address.lo);
			}
			if (status.hi & MCA_STATUS_HI_MISCV) {
				const msr_t misc = rdmsr(IA32_MC_MISC(bank));

				printk(BIOS_NOTICE, "[MCA-RO] %s MC%u_MISC=%08x:%08x\n",
				       phase, bank, misc.hi, misc.lo);
			}
		}
		after = rdmsr(IA32_MC_STATUS(bank));
		if (!same_status(status, after)) {
			clean = false;
			printk(BIOS_NOTICE,
			       "[MCA-RO] %s MC%u_STATUS_AFTER=%08x:%08x changed; details may be inconsistent\n",
			       phase, bank, after.hi, after.lo);
		}
	}
	global_after = rdmsr(IA32_MCG_STATUS);
	if (!same_status(global, global_after)) {
		clean = false;
		printk(BIOS_NOTICE, "[MCA-RO] %s MCG_STATUS_AFTER=%08x:%08x changed\n",
		       phase, global_after.hi, global_after.lo);
	}
	printk(BIOS_NOTICE, "[MCA-RO] %s admitted=%u; evidence preserved, no initialization\n",
	       phase, (unsigned int)clean);
	return clean;
}

#if ENV_RAMSTAGE
/* Keep this storage out of the verbose CAR call frame. Each AP owns its
 * snapshot on its own stack; no shared mutable diagnostic state is needed.
 */
struct mca_bank_snapshot {
	msr_t ctl;
	msr_t status;
	msr_t address;
	msr_t misc;
	msr_t after;
};

struct mca_snapshot {
	struct cpuid_result features;
	msr_t cap, global, apic, global_ctl, global_after;
	unsigned long cr0, cr4;
	struct mca_bank_snapshot banks[MODEL_206CX_MCA_BANKS];
};

/* Match the verbose call's CR0/CR4 argument expressions. As in that call,
 * their relative C argument evaluation order is unspecified; both are read
 * once between the initial MCG_STATUS and any optional MCG_CTL/bank read.
 */
static void save_control_registers(struct mca_snapshot *snapshot,
	unsigned long cr0, unsigned long cr4)
{
	snapshot->cr0 = cr0;
	snapshot->cr4 = cr4;
}

static void print_snapshot_header(const char *phase, const struct mca_snapshot *snapshot)
{
	printk(BIOS_NOTICE, "[MCA-RO] %s CPUID=%08x initial-APIC=%02x MCG_CAP=%08x:%08x\n",
	       phase, snapshot->features.eax, snapshot->features.ebx >> 24,
	       snapshot->cap.hi, snapshot->cap.lo);
}

/* Failure output uses the initial and changed values already collected.
 * There are deliberately no architectural reads in the rendering path.
 */
static void print_failed_snapshot(const char *phase, const struct mca_snapshot *snapshot)
{
	print_snapshot_header(phase, snapshot);
	printk(BIOS_NOTICE,
	       "[MCA-RO] %s CR0=%08lx CR4=%08lx APIC_BASE=%08x:%08x MCG_STATUS=%08x:%08x\n",
	       phase, snapshot->cr0, snapshot->cr4, snapshot->apic.hi, snapshot->apic.lo,
	       snapshot->global.hi, snapshot->global.lo);
	printk(BIOS_NOTICE,
	       "[MCA-RO] %s preserved; reset data unqualified until MCA initialization; non-atomic sample\n",
	       phase);
	if (snapshot->cap.lo & MCG_CTL_P)
		printk(BIOS_NOTICE, "[MCA-RO] %s MCG_CTL=%08x:%08x\n", phase,
		       snapshot->global_ctl.hi, snapshot->global_ctl.lo);
	for (unsigned int bank = 0; bank < MODEL_206CX_MCA_BANKS; bank++) {
		const struct mca_bank_snapshot *entry = &snapshot->banks[bank];
		const msr_t status = entry->status;
		const bool valid = !!(status.hi & MCA_STATUS_HI_VAL);

		printk(BIOS_NOTICE,
		       "[MCA-RO] %s MC%u_CTL=%08x:%08x STATUS=%08x:%08x "
		       "VAL=%u OVER=%u UC=%u EN=%u MISCV=%u ADDRV=%u PCC=%u "
		       "MCACOD=%04x MSCOD=%04x\n",
		       phase, bank, entry->ctl.hi, entry->ctl.lo, status.hi, status.lo,
		       (unsigned int)valid,
		       (unsigned int)!!(status.hi & MCA_STATUS_HI_OVERFLOW),
		       (unsigned int)!!(status.hi & MCA_STATUS_HI_UC),
		       (unsigned int)!!(status.hi & MCA_STATUS_HI_EN),
		       (unsigned int)!!(status.hi & MCA_STATUS_HI_MISCV),
		       (unsigned int)!!(status.hi & MCA_STATUS_HI_ADDRV),
		       (unsigned int)!!(status.hi & MCA_STATUS_HI_PCC),
		       status.lo & 0xffff, status.lo >> 16);
		if (valid) {
			if (status.hi & MCA_STATUS_HI_ADDRV)
				printk(BIOS_NOTICE, "[MCA-RO] %s MC%u_ADDR=%08x:%08x\n",
				       phase, bank, entry->address.hi, entry->address.lo);
			if (status.hi & MCA_STATUS_HI_MISCV)
				printk(BIOS_NOTICE, "[MCA-RO] %s MC%u_MISC=%08x:%08x\n",
				       phase, bank, entry->misc.hi, entry->misc.lo);
		}
		if (!same_status(status, entry->after))
			printk(BIOS_NOTICE,
			       "[MCA-RO] %s MC%u_STATUS_AFTER=%08x:%08x changed; details may be inconsistent\n",
			       phase, bank, entry->after.hi, entry->after.lo);
	}
	if (!same_status(snapshot->global, snapshot->global_after))
		printk(BIOS_NOTICE, "[MCA-RO] %s MCG_STATUS_AFTER=%08x:%08x changed\n",
		       phase, snapshot->global_after.hi, snapshot->global_after.lo);
	printk(BIOS_NOTICE, "[MCA-RO] %s admitted=0; evidence preserved, no initialization\n",
	       phase);
}

bool model_206cx_mca_report_compact(const char *phase)
{
	const struct cpuid_result vendor = cpuid(0);
	struct mca_snapshot snapshot;
	bool clean;

	if (vendor.eax < 1 || vendor.ebx != 0x756e6547 ||
	    vendor.edx != 0x49656e69 || vendor.ecx != 0x6c65746e) {
		printk(BIOS_NOTICE, "[MCA-RO] %s unsupported CPUID vendor/leaf; no MSR reads\n",
		       phase);
		return false;
	}
	snapshot.features = cpuid(1);
	if (snapshot.features.eax != MODEL_206CX_SIGNATURE ||
	    (snapshot.features.edx & MCA_REQUIRED_CPUID_EDX) != MCA_REQUIRED_CPUID_EDX) {
		printk(BIOS_NOTICE, "[MCA-RO] %s unsupported CPU/MSR/MCA; no MSR reads\n",
		       phase);
		return false;
	}

	snapshot.cap = rdmsr(IA32_MCG_CAP);
	if ((snapshot.cap.lo & MCA_BANKS_MASK) != MODEL_206CX_MCA_BANKS) {
		print_snapshot_header(phase, &snapshot);
		printk(BIOS_NOTICE, "[MCA-RO] %s unsupported bank count; no bank reads\n", phase);
		return false;
	}
	snapshot.apic = rdmsr(IA32_APIC_BASE_MSR_INDEX);
	snapshot.global = rdmsr(IA32_MCG_STATUS);
	clean = !snapshot.global.hi && !snapshot.global.lo;
	save_control_registers(&snapshot, read_cr0(), read_cr4());
	if (snapshot.cap.lo & MCG_CTL_P)
		snapshot.global_ctl = rdmsr(MODEL_206CX_MCG_CTL);
	for (unsigned int bank = 0; bank < MODEL_206CX_MCA_BANKS; bank++) {
		struct mca_bank_snapshot *entry = &snapshot.banks[bank];

		entry->ctl = rdmsr(IA32_MC_CTL(bank));
		entry->status = rdmsr(IA32_MC_STATUS(bank));
		if (entry->status.hi & MCA_STATUS_HI_VAL) {
			clean = false;
			if (entry->status.hi & MCA_STATUS_HI_ADDRV)
				entry->address = rdmsr(IA32_MC_ADDR(bank));
			if (entry->status.hi & MCA_STATUS_HI_MISCV)
				entry->misc = rdmsr(IA32_MC_MISC(bank));
		}
		entry->after = rdmsr(IA32_MC_STATUS(bank));
		if (!same_status(entry->status, entry->after))
			clean = false;
	}
	snapshot.global_after = rdmsr(IA32_MCG_STATUS);
	if (!same_status(snapshot.global, snapshot.global_after))
		clean = false;

	if (!clean)
		print_failed_snapshot(phase, &snapshot);
	else
		printk(BIOS_NOTICE,
		       "[MCA-RO] %s initial-APIC=%02x banks=9 admitted=1; preserved, no initialization, non-atomic\n",
		       phase, snapshot.features.ebx >> 24);
	return clean;
}
#endif
