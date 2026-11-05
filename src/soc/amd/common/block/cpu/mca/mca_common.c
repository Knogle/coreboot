/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/mca.h>
#include <cpu/x86/msr.h>
#include <console/console.h>
#include <console/streams.h>
#include <cpu/cpu.h>
#include <smp/node.h>
#include <types.h>
#include "mca_common_defs.h"

/* Diagnostic output only: use the already-established GS CPU context, not another MSR read. */
#define NATIVE174_BSP_TRACE(fmt, ...) do { \
	if ((CONFIG(B565D4_RETAINED_PEI_LATE_ENTRY) || CONFIG(SOC_AMD_CEZANNE_NATIVE)) && cpu_index() == 0) { \
		printk(BIOS_INFO, "NATIVE174 CPU_TRACE " fmt "\n", ##__VA_ARGS__); \
		console_tx_flush(); \
	} \
} while (0)

static void mca_check_all_banks(void)
{
	struct mca_bank_status mci;
	unsigned int num_banks;

	NATIVE174_BSP_TRACE("MCA_COUNT BEFORE");
	num_banks = mca_get_bank_count();
	NATIVE174_BSP_TRACE("MCA_COUNT AFTER count=%u", num_banks);

	/*
	 * Only the BSP has access to non CPU (Cache, UMC, ..) MCA banks.
	 * APs can only access their own CPU MCA banks.
	 */
	if (boot_cpu() && !mca_has_expected_bank_count())
		printk(BIOS_WARNING, "CPU has an unexpected number of MCA banks %d!\n", mca_get_bank_count());

	NATIVE174_BSP_TRACE("MCA_BANKS_CHECK BEGIN count=%u", num_banks);
	if (mca_skip_check())
		return;

	for (unsigned int i = 0 ; i < num_banks ; i++) {
		if (!mca_is_valid_bank(i))
			continue;
		mci.bank = i;
		/* The MCA status register can be used in both the MCA and MCAX case */
		NATIVE174_BSP_TRACE("MCA_STATUS BEFORE bank=%u msr=%08x",
			i, IA32_MC_STATUS(i));
		mci.sts = rdmsr(IA32_MC_STATUS(i));
		NATIVE174_BSP_TRACE("MCA_STATUS AFTER bank=%u value=%08x:%08x",
			i, mci.sts.hi, mci.sts.lo);
		if (mci.sts.hi || mci.sts.lo) {
			bool skip;

			NATIVE174_BSP_TRACE("MCA_SKIP_ERROR BEFORE bank=%u", i);
			skip = mca_skip_error(i);
			NATIVE174_BSP_TRACE("MCA_SKIP_ERROR AFTER bank=%u skip=%d", i, skip);
			if (skip)
				continue;
			NATIVE174_BSP_TRACE("MCA_PRINT_ERROR BEFORE bank=%u", i);
			mca_print_error(i);
			NATIVE174_BSP_TRACE("MCA_PRINT_ERROR AFTER bank=%u", i);

			if (CONFIG(ACPI_BERT) && mca_valid(mci.sts))
				build_bert_mca_error(&mci);
		}
	}
}

/* Exact mca_clear_status() count/read and zero-store loop, with BSP-only output.
 * Keep the shared x86 MSR header and every AP's original helper untouched.
 */
static void native174_bsp_mca_clear_status(void)
{
	unsigned int num_banks;
	const msr_t msr = {.lo = 0, .hi = 0};

	NATIVE174_BSP_TRACE("MCA_CLEAR_COUNT BEFORE");
	num_banks = mca_get_bank_count();
	NATIVE174_BSP_TRACE("MCA_CLEAR_COUNT AFTER count=%u", num_banks);
	for (unsigned int i = 0; i < num_banks; i++) {
		NATIVE174_BSP_TRACE("MCA_CLEAR BEFORE bank=%u msr=%08x", i, IA32_MC_STATUS(i));
		wrmsr(IA32_MC_STATUS(i), msr);
		NATIVE174_BSP_TRACE("MCA_CLEAR AFTER bank=%u", i);
	}
}

void check_mca(void)
{
	NATIVE174_BSP_TRACE("MCA_CHECK_ALL BEFORE");
	mca_check_all_banks();
	NATIVE174_BSP_TRACE("MCA_CHECK_ALL AFTER");
	/* mca_clear_status uses the MCA registers which can be used in both the MCA and MCAX
	  case */
	if ((CONFIG(B565D4_RETAINED_PEI_LATE_ENTRY) || CONFIG(SOC_AMD_CEZANNE_NATIVE)) && cpu_index() == 0)
		native174_bsp_mca_clear_status();
	else
		mca_clear_status();
}
