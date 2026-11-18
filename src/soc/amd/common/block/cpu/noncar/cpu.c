/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/amd_pci_mmconf.h>
#include <amdblocks/cpu.h>
#include <amdblocks/mca.h>
#include <arch/cpuid.h>
#include <console/console.h>
#include <console/streams.h>
#include <cpu/cpu.h>
#include <cpu/x86/msr.h>
#include <cpu/amd/cpuid.h>
#include <cpu/amd/microcode.h>
#include <cpu/amd/msr.h>
#include <cpu/amd/mtrr.h>
#include <halt.h>
#include <lib.h>
#include <smbios.h>
#include <soc/iomap.h>
#include <soc/msr.h>
#include <types.h>

#if CONFIG(SOC_AMD_CEZANNE_NATIVE)
#include <soc/cpu.h>
#endif

/* Diagnostic output only: use the already-established GS CPU context, not another MSR read. */
#define NATIVE174_BSP_TRACE(fmt, ...) do { \
	if (CONFIG(SOC_AMD_CEZANNE_NATIVE) && cpu_index() == 0) { \
		printk(BIOS_INFO, "NATIVE174 CPU_TRACE " fmt "\n", ##__VA_ARGS__); \
		console_tx_flush(); \
	} \
} while (0)

#define UNSUPPORTED 0

#define UNINITIALIZED ((uint64_t)-1)
#define CPUID_FEATURES_7_HLE_RTM_MASK		(BIT(4) | BIT(11))

struct core_info {
	/* Core max boost frequency */
	uint32_t max_frequency;

	/* L3 Cache block unique ID & size (in bytes) */
	uint16_t l3_cache_uid;
	size_t l3_cache_size;
};

static struct core_info core_info_list[CONFIG_MAX_CPUS];

static union pstate_msr get_pstate0_msr(void)
{
	static union pstate_msr pstate_reg = { .raw = UNINITIALIZED };

	struct cpuid_result res;

	/* Check if we have already determined P-state support */
	if (pstate_reg.raw == UNINITIALIZED) {
		if (cpu_cpuid_extended_level() < CPUID_EXT_PM) {
			pstate_reg.pstate_en = UNSUPPORTED;
			return pstate_reg;
		}

		res = cpuid(CPUID_EXT_PM);

		if (!(res.edx & BIT(7))) { /* Hardware P-state control */
			pstate_reg.pstate_en = UNSUPPORTED;
			return pstate_reg;
		}

		pstate_reg.raw = rdmsr(PSTATE_MSR(get_pstate_0_reg())).raw;
	}

	return pstate_reg;
}

unsigned int smbios_cpu_get_current_speed_mhz(void)
{
	union pstate_msr pstate_reg = get_pstate0_msr();

	if (!pstate_reg.pstate_en)
		return 0;

	return get_pstate_core_freq(pstate_reg);
}

unsigned int smbios_cpu_get_voltage(void)
{
	union pstate_msr pstate_reg = get_pstate0_msr();

	if (!pstate_reg.pstate_en)
		return 0;

	return get_pstate_core_uvolts(pstate_reg) / 100000; /* uV to (10 * V) */
}

uint32_t get_pstate_0_reg(void)
{
	return 0;
}

uint32_t get_pstate_latency(void)
{
	return 0;
}

unsigned int smbios_processor_family(struct cpuid_result res)
{
	return 0x6b; /* Zen */
}

unsigned int smbios_processor_external_clock(void)
{
	return 100; /* 100 MHz */
}

void set_cstate_io_addr(void)
{
	msr_t cst_addr;

	cst_addr.hi = 0;
	cst_addr.lo = ACPI_CSTATE_CONTROL;
	wrmsr(MSR_CSTATE_ADDRESS, cst_addr);
}

/* Number of most significant physical address bits reserved for secure memory encryption */
unsigned int get_reserved_phys_addr_bits(void)
{
	if (!(rdmsr(SYSCFG_MSR).raw & SYSCFG_MSR_SMEE))
		return 0;

	return (cpuid_ebx(CPUID_EBX_MEM_ENCRYPT) & CPUID_EBX_MEM_ENCRYPT_ADDR_BITS_MASK) >>
			CPUID_EBX_MEM_ENCRYPT_ADDR_BITS_SHIFT;
}

unsigned int smbios_cpu_get_max_speed_mhz(void)
{
	uint32_t max_freq = 0;

	for (int i = 0; i < get_cpu_count(); i++) {
		struct core_info *core_info = &core_info_list[i];

		if (max_freq < core_info->max_frequency)
			max_freq = core_info->max_frequency;
	}
	return max_freq;
}

/*
 * On some SoCs like AMD Glinda, multiple unique L3 cache blocks exist,
 * each with a distinct UID and possibly different sizes. The default
 * method assumes a single shared L3 cache across all cores, which is
 * inaccurate. This function aggregates sizes of all uniquely identified
 * L3 cache blocks to compute the correct total L3 size.
 */
bool soc_fill_cpu_cache_info(uint8_t level, struct cpu_cache_info *info)
{
	if (level != CACHE_L3)
		return x86_get_cpu_cache_info(level, info);

	if (!info)
		return false;

	uint32_t total_cache = 0;
	bool seen_cache_ids[CONFIG_MAX_CPUS] = {false};

	x86_get_cpu_cache_info(level, info);

	/*
	 * To calculate the total L3 cache size, iterate over core_info_list and add up the sizes
	 * of the L3 cache blocks with unique cache ID.
	 */
	for (int i = 0; i < get_cpu_count(); i++) {
		struct core_info *cache = &core_info_list[i];

		printk(BIOS_SPEW, "CPU %d: Cache Level: %d, Cache Size: %zu bytes\n",
		       i, cache->l3_cache_uid, cache->l3_cache_size);

		if (!seen_cache_ids[cache->l3_cache_uid]) {
			total_cache += cache->l3_cache_size;
			seen_cache_ids[cache->l3_cache_uid] = true;
		}
	}

	info->num_cores_shared = get_cpu_count();
	info->size = total_cache;

	printk(BIOS_SPEW, "Total cache at level: %d is:%zu\n", level, info->size);
	return true;
}

static uint32_t get_max_boost_frequency(void)
{
	msr_t msr = rdmsr(MSR_CPPC_CAPABILITY_1);
	uint16_t nominal_perf = (msr.lo >> SHIFT_CPPC_CAPABILITY_1_NOMINAL_PERF) & 0xff;
	uint16_t max_perf = (msr.lo >> SHIFT_CPPC_CAPABILITY_1_HIGHEST_PERF) & 0xff;

	return (smbios_cpu_get_current_speed_mhz() * max_perf)/nominal_perf;
}

static void ap_stash_core_info(void)
{
	unsigned int cpuid_cpu_id = cpuid_ebx(CPUID_EBX_CORE_ID) & 0xff;

	const uint8_t level = CACHE_L3;
	struct cpu_cache_info info;
	x86_get_cpu_cache_info(level, &info);

	struct core_info *core_info = &core_info_list[cpu_index()];
	core_info->l3_cache_uid = cpuid_cpu_id >> log2(info.num_cores_shared);
	core_info->l3_cache_size = info.size;

	core_info->max_frequency = get_max_boost_frequency();
}

static void clear_unsupported_hle_rtm(void)
{
	msr_t msr = rdmsr(CPU_ID_FEATURES_7_MSR);
	msr.lo &= ~CPUID_FEATURES_7_HLE_RTM_MASK;
	wrmsr(CPU_ID_FEATURES_7_MSR, msr);
}

void amd_cpu_init(struct device *dev)
{
	if (CONFIG(SOC_AMD_COMMON_BLOCK_MCA_COMMON)) {
		NATIVE174_BSP_TRACE("CHECK_MCA BEFORE");
		check_mca();
		NATIVE174_BSP_TRACE("CHECK_MCA AFTER");
	}

#if CONFIG(SOC_AMD_CEZANNE_NATIVE)
	/* Keep C1/HLT and the PM map; establish MMCONF and the selected AMD patch. */
	msr_t before;
	msr_t after;
	msr_t mmconf_before;
	msr_t mmconf_after;
	uint32_t cpuid_signature;
	const uint32_t target_patch_id = amd_microcode_patch_id();
	uint32_t expected_patch_id;
	bool skip_update;

	_Static_assert(CONFIG_ECAM_MMCONF_BASE_ADDRESS == 0xf0000000 &&
		CONFIG_ECAM_MMCONF_BUS_NUMBER == 128,
		"Native board MMCONF trial uses the actual selected MCFG geometry");
	NATIVE174_BSP_TRACE("CPUID1 BEFORE");
	cpuid_signature = cpuid_eax(1);
	NATIVE174_BSP_TRACE("CPUID1 AFTER eax=%08x", cpuid_signature);
#if CONFIG(SOC_AMD_CEZANNE_NATIVE_CPU_IDENTITY_PROVIDER)
	if (!mainboard_amd_native_cpu_identity_valid(cpuid_signature))
#else
	if (!cpuid_match(cpuid_signature, CEZANNE_A0_CPUID, CPUID_ALL_STEPPINGS_MASK))
#endif
		die("NATIVE149 microcode CPU identity mismatch");
	NATIVE174_BSP_TRACE("SIGN_ID_BEFORE BEFORE");
	before = rdmsr(IA32_BIOS_SIGN_ID);
	NATIVE174_BSP_TRACE("SIGN_ID_BEFORE AFTER value=%08x:%08x", before.hi, before.lo);
	/* Keep equal-revision application for per-thread microcode state. */
	skip_update = !target_patch_id || before.lo > target_patch_id;
	expected_patch_id = skip_update ? before.lo : target_patch_id;
	if (skip_update) {
		printk(BIOS_INFO, "NATIVE175 CPU%lu microcode retain current=%08x:%08x "
		       "target=%08x\n", cpu_index(), before.hi, before.lo, target_patch_id);
	} else {
		NATIVE174_BSP_TRACE("APPLY_MICROCODE BEFORE");
		amd_apply_microcode_patch();
		NATIVE174_BSP_TRACE("APPLY_MICROCODE AFTER");
	}
	NATIVE174_BSP_TRACE("SIGN_ID_AFTER BEFORE");
	after = rdmsr(IA32_BIOS_SIGN_ID);
	NATIVE174_BSP_TRACE("SIGN_ID_AFTER AFTER value=%08x:%08x", after.hi, after.lo);
	printk(BIOS_INFO, "NATIVE175 CPU%lu microcode before=%08x:%08x after=%08x:%08x "
	       "expected_lo=%08x skip_update=%u; C1 only\n",
	       cpu_index(), before.hi, before.lo, after.hi, after.lo,
	       expected_patch_id, skip_update);
	if (after.lo != expected_patch_id || (skip_update && after.hi != before.hi))
		die("NATIVE175 microcode patch readback mismatch");

	/* Establish the board's MMCONF geometry on every CPU, including cores
	 * whose reset state differs from the BSP. Validate the completed update.
	 */
	mmconf_before = rdmsr(MMIO_CONF_BASE);
	enable_pci_mmconf();
	mmconf_after = rdmsr(MMIO_CONF_BASE);
	printk(BIOS_INFO, "NATIVE CPU%lu MMCONF before=%08x:%08x after=%08x:%08x "
		"expected=00000000:f000001d\n", cpu_index(),
		mmconf_before.hi, mmconf_before.lo, mmconf_after.hi, mmconf_after.lo);
	if (mmconf_after.hi || mmconf_after.lo != 0xf000001dU)
		die("Native MMCONF per-CPU readback mismatch");

	/* Read-only witness for the final SIPI MTRR operating-mode handoff. */
	msr_t operating_syscfg = rdmsr(SYSCFG_MSR);
	printk(BIOS_INFO, "NATIVE252 CPU%lu SYSCFG operating=%08x:%08x "
	       "MtrrFixDramModEn=%u\n", cpu_index(), operating_syscfg.hi,
	       operating_syscfg.lo,
	       !!(operating_syscfg.lo & SYSCFG_MSR_MtrrFixDramModEn));
	if (operating_syscfg.lo & SYSCFG_MSR_MtrrFixDramModEn)
		die("NATIVE252 fixed-MTRR initialization visibility left enabled");
#else
	set_cstate_io_addr();

	if (CONFIG(SOC_AMD_COMMON_BLOCK_UCODE))
		amd_apply_microcode_patch();
#endif

	if (CONFIG(SOC_AMD_COMMON_BLOCK_CLEAR_HLE_RTM))
		clear_unsupported_hle_rtm();

	if (CONFIG(SOC_FILL_CPU_CACHE_INFO))
		ap_stash_core_info();
}
