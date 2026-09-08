/* SPDX-License-Identifier: GPL-2.0-only */

#include <arch/cpuid.h>
#include <arch/io.h>
#include <arch/romstage.h>
#include <cbmem.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <console/uart.h>
#include <cpu/x86/msr.h>
#include <cpu/x86/mtrr.h>
#include <device/mmio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "b06v6_handoff.h"

#define POST_B06V6_POSTMEM_ENTRY	0x09
#define POST_B06V6_MTRR_GATED	0x0a
#define POST_B06V6_WINDOWS_SMOKED	0x0b
#define POST_B06V6_CBMEM_READY	0x0c

#define POST_B06V6_NO_RESULT	0x14
#define POST_B06V6_RESULT_MISMATCH	0x15
#define POST_B06V6_MTRR_MISMATCH	0x16
#define POST_B06V6_WINDOWS_SMOKE_FAILED	0x17
#define POST_B06V6_CBMEM_FAILED	0x18
#define POST_B06V6_HANDOFF_FAILED	0x19

#define B06V6_EXPECTED_MTRR_CAP	0x00000d0au
#define B06V6_EXPECTED_MTRR_DEF_TYPE	MTRR_DEF_TYPE_EN
#define B06V6_EXPECTED_PHYS_BITS	40u
#define B06V6_EXPECTED_MTRR_HIGH_MASK	0x000000ffu

#define B06V6_CAR_MTRR_BASE_LO	0xfff80006u
#define B06V6_CAR_MTRR_MASK_LO	0xffff0800u
#define B06V6_ROM_MTRR_BASE_LO	0xfffc0005u
#define B06V6_ROM_MTRR_MASK_LO	0xfffc0800u

#define B06V6_COMMON_REQUIRED_FLAGS \
	(X58_B06V6_HANDOFF_EXACT_MINIT | X58_B06V6_HANDOFF_MTRR_GATED | \
	 X58_B06V6_HANDOFF_CBMEM_SMOKED | \
	 X58_B06V6_HANDOFF_OBJECT_SMOKED | X58_B06V6_HANDOFF_CBMEM_READY)
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
#define B06VL_REQUIRED_FLAG X58_B06VL_HANDOFF_BROAD_POST_MINIT
#else
#define B06VL_REQUIRED_FLAG 0
#endif
#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
#if CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
#define B06V6_REQUIRED_FLAGS \
	(B06V6_COMMON_REQUIRED_FLAGS | X58_B06VE_HANDOFF_HIGH_QPI_POST_MINIT | \
	 X58_B06VF_HANDOFF_LOWMEM_SMOKED | B06VL_REQUIRED_FLAG)
#else
#define B06V6_REQUIRED_FLAGS \
	(B06V6_COMMON_REQUIRED_FLAGS | X58_B06VE_HANDOFF_HIGH_QPI_POST_MINIT)
#endif
#else
#define B06V6_REQUIRED_FLAGS B06V6_COMMON_REQUIRED_FLAGS
#endif

#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
_Static_assert(sizeof(struct x58_b06v6_handoff) == 164,
	"Serial-independent v10 handoff wire size changed");
#else
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	_Static_assert(sizeof(struct x58_b06v6_handoff) == 160,
		"B06VL handoff wire size changed");
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
	_Static_assert(sizeof(struct x58_b06v6_handoff) == 160,
		"B06VK handoff wire size changed");
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
	_Static_assert(sizeof(struct x58_b06v6_handoff) == 160,
		"B06VJ handoff wire size changed");
#elif CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	_Static_assert(sizeof(struct x58_b06v6_handoff) == 160,
		"B06VI handoff wire size changed");
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
	_Static_assert(sizeof(struct x58_b06v6_handoff) == 144,
		"B06VH handoff wire size changed");
#elif CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
	_Static_assert(sizeof(struct x58_b06v6_handoff) == 144,
		"B06VF handoff wire size changed");
#elif CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
_Static_assert(sizeof(struct x58_b06v6_handoff) == 144,
	"B06VE handoff wire size changed");
#elif CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
_Static_assert(sizeof(struct x58_b06v6_handoff) == 140,
	"B06V7 handoff wire size changed");
#else
_Static_assert(sizeof(struct x58_b06v6_handoff) == 132,
	"B06V6 handoff wire size changed");
#endif
#endif

static uint32_t fnv1a32(const void *data, size_t size)
{
	const uint8_t *bytes = data;
	uint32_t digest = 2166136261u;

	while (size--) {
		digest ^= *bytes++;
		digest *= 16777619u;
	}

	return digest;
}

uint32_t x58_b06v6_handoff_digest(const struct x58_b06v6_handoff *handoff)
{
	if (handoff == NULL)
		return 0;

	/* digest is last and is deliberately excluded from its own coverage. */
	return fnv1a32(handoff, offsetof(struct x58_b06v6_handoff, digest));
}

bool x58_b06v6_raminit_result_is_exact(
	const struct x58_b06v6_raminit_result *result)
{
	return result != NULL &&
		result->cpuid == X58_B06V6_EXPECTED_CPUID &&
		result->microcode_revision == X58_B06V6_EXPECTED_UCODE &&
		x58_b06v6_spd_digest_is_exact(result->spd_fnv,
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
			result->spd_profile_fnv) &&
#else
			0) &&
#endif
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
		result->profile_id == X58_B06VL_PROFILE_BROAD &&
#endif
		x58_b06vi_profile_tuple_exact(result->profile_id,
			result->saved_pre_a0, result->post_minit_cpu_a0,
			result->post_minit_cpu_9c, result->csi_state_fnv,
			result->csi_state_canonical_fnv, result->workspace_fnv,
			result->workspace_canonical_fnv) &&
#elif CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
		result->csi_state_fnv == X58_B06VE_EXPECTED_CSI_FNV &&
		result->csi_state_canonical_fnv ==
			X58_B06VE_EXPECTED_CSI_CANONICAL_FNV &&
#elif CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
		result->csi_state_canonical_fnv ==
			X58_B06V7_EXPECTED_CSI_CANONICAL_FNV &&
#else
		result->csi_state_fnv == X58_B06V6_EXPECTED_CSI_FNV &&
#endif
		result->policy_fnv == X58_B06V6_EXPECTED_POLICY_FNV &&
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
		/* The shared profile truth table owns the applicable digest contract. */
#elif CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
#if CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
		x58_b06vh_workspace_digest_pair_exact(result->workspace_fnv,
			result->workspace_canonical_fnv) &&
#else
		(result->workspace_fnv ==
			X58_B06VF_EXPECTED_WORKSPACE_RAW_A_FNV ||
		 result->workspace_fnv ==
			X58_B06VF_EXPECTED_WORKSPACE_RAW_B_FNV) &&
		result->workspace_canonical_fnv ==
			X58_B06VF_EXPECTED_WORKSPACE_CANONICAL_FNV &&
#endif
#elif CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
		result->workspace_fnv == X58_B06VE_EXPECTED_WORKSPACE_FNV &&
		result->workspace_canonical_fnv == 0 &&
#elif CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
		result->workspace_canonical_fnv ==
			X58_B06V7_EXPECTED_WORKSPACE_CANONICAL_FNV &&
#else
		result->workspace_fnv == X58_B06V6_EXPECTED_WORKSPACE_FNV &&
#endif
		result->minit_eax == 0 &&
		result->mc_mapper == X58_B06V6_EXPECTED_MC_MAPPER &&
		result->mc_common_f8 == X58_B06V6_EXPECTED_MC_COMMON_F8 &&
		result->ch2_dod == X58_B06V6_EXPECTED_CH2_DOD &&
		result->ch2_ranks == X58_B06V6_EXPECTED_CH2_RANKS &&
		result->ch2_status == X58_B06V6_EXPECTED_CH2_STATUS &&
#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
		result->qpi_status == X58_B06VE_EXPECTED_QPI_STATUS &&
		result->post_minit_ioh_stage_9c ==
			X58_B06VE_EXPECTED_POST_MINIT_IOH_STAGE_9C;
#else
		result->qpi_status == X58_B06V6_EXPECTED_QPI_STATUS;
#endif
}

bool x58_b06v6_handoff_is_valid(const struct x58_b06v6_handoff *handoff)
{
	if (handoff == NULL || handoff->magic != X58_B06V6_HANDOFF_MAGIC ||
	    handoff->version != X58_B06V6_HANDOFF_VERSION ||
	    handoff->structure_size != sizeof(*handoff) ||
	    handoff->flags != B06V6_REQUIRED_FLAGS ||
	    !x58_b06v6_raminit_result_is_exact(&handoff->raminit) ||
	    handoff->mtrr_cap != B06V6_EXPECTED_MTRR_CAP ||
	    handoff->mtrr_def_type != B06V6_EXPECTED_MTRR_DEF_TYPE ||
	    handoff->cbmem_base != X58_B06V6_CBMEM_BASE ||
	    handoff->cbmem_top != X58_B06V6_CBMEM_TOP ||
	    handoff->object_base != X58_B06V6_OBJECT_BASE ||
	    handoff->object_size != X58_B06V6_OBJECT_SIZE ||
	    handoff->object_max_size != X58_B06V6_OBJECT_MAX_SIZE)
		return false;

	for (size_t i = 0; i < ARRAY_SIZE(handoff->reserved); i++) {
		if (handoff->reserved[i] != 0)
			return false;
	}

	return handoff->digest == x58_b06v6_handoff_digest(handoff);
}

#if ENV_ROMSTAGE_OR_BEFORE

static struct x58_b06v6_raminit_result recorded_result;
static bool result_recorded;

void x58_b06v6_handoff_record_success(
	const struct x58_b06v6_raminit_result *result)
{
	result_recorded = false;
	memset(&recorded_result, 0, sizeof(recorded_result));
	if (!x58_b06v6_raminit_result_is_exact(result))
		return;
	memcpy(&recorded_result, result, sizeof(recorded_result));
	result_recorded = true;
}

static void __noreturn b06v6_postmem_stop(uint8_t post, const char *reason)
{
	printk(BIOS_EMERG, "[RAMINIT] POSTMEM FAIL %s POST=%02x\n", reason,
	       post);
	uart_tx_flush(get_uart_for_console());
	outb(post, CONFIG_POST_IO_PORT);
	asm volatile ("cli" ::: "memory");
	for (;;)
		asm volatile ("hlt");
}

static bool b06v6_mtrr_state_is_exact(msr_t *cap_out, msr_t *def_out)
{
	const struct cpuid_result address_bits = cpuid(0x80000008);
	const msr_t cap = rdmsr(MTRR_CAP_MSR);
	const msr_t def_type = rdmsr(MTRR_DEF_TYPE_MSR);
	const msr_t base0 = rdmsr(MTRR_PHYS_BASE(0));
	const msr_t mask0 = rdmsr(MTRR_PHYS_MASK(0));
	const msr_t base1 = rdmsr(MTRR_PHYS_BASE(1));
	const msr_t mask1 = rdmsr(MTRR_PHYS_MASK(1));
	bool exact;

	*cap_out = cap;
	*def_out = def_type;
	exact = (address_bits.eax & 0xff) == B06V6_EXPECTED_PHYS_BITS &&
		cap.hi == 0 && cap.lo == B06V6_EXPECTED_MTRR_CAP &&
		def_type.hi == 0 && def_type.lo == B06V6_EXPECTED_MTRR_DEF_TYPE &&
		base0.hi == 0 && base0.lo == B06V6_CAR_MTRR_BASE_LO &&
		mask0.hi == B06V6_EXPECTED_MTRR_HIGH_MASK &&
		mask0.lo == B06V6_CAR_MTRR_MASK_LO &&
		base1.hi == 0 && base1.lo == B06V6_ROM_MTRR_BASE_LO &&
		mask1.hi == B06V6_EXPECTED_MTRR_HIGH_MASK &&
		mask1.lo == B06V6_ROM_MTRR_MASK_LO;

	for (unsigned int index = 2; index < (cap.lo & MTRR_CAP_VCNT); index++) {
		if (rdmsr(MTRR_PHYS_MASK(index)).lo & MTRR_PHYS_MASK_VALID)
			exact = false;
	}

	if (!exact) {
		printk(BIOS_ERR,
		       "[RAMINIT] MTRR mismatch PHYS=%02x CAP=%08x:%08x DEF=%08x:%08x\n",
		       address_bits.eax & 0xff, cap.hi, cap.lo, def_type.hi,
		       def_type.lo);
		printk(BIOS_ERR,
		       "[RAMINIT] MTRR0 BASE=%08x:%08x MASK=%08x:%08x MTRR1 BASE=%08x:%08x MASK=%08x:%08x\n",
		       base0.hi, base0.lo, mask0.hi, mask0.lo, base1.hi,
		       base1.lo, mask1.hi, mask1.lo);
	}

	return exact;
}

struct smoke_word {
	uintptr_t address;
	uint32_t original;
	uint32_t pattern;
};

static void b06v6_memory_barrier(void)
{
	/* The fixed E5645 target has SSE2; this also orders UC stores. */
	asm volatile ("mfence" ::: "memory");
}

static bool b06v6_transactional_smoke(const uintptr_t *addresses,
				      size_t count, uint32_t seed)
{
	struct smoke_word words[32];
	bool patterns_ok = true;
	bool restore_ok = true;

	if (count > ARRAY_SIZE(words))
		return false;

	/* Read every original before modifying anything, including alias cases. */
	for (size_t i = 0; i < count; i++) {
		words[i].address = addresses[i];
		words[i].original = read32p(addresses[i]);
		words[i].pattern = seed ^ (uint32_t)addresses[i] ^
			(uint32_t)(0x01010101u * (i + 1));
	}

	for (size_t i = 0; i < count; i++)
		write32p(words[i].address, words[i].pattern);
	b06v6_memory_barrier();

	for (size_t i = 0; i < count; i++) {
		const uint32_t observed = read32p(words[i].address);

		if (observed != words[i].pattern) {
			patterns_ok = false;
			printk(BIOS_ERR,
			       "[RAMINIT] smoke mismatch @%08lx expected=%08x observed=%08x\n",
			       words[i].address, words[i].pattern, observed);
		}
	}

	/* Restoration is attempted even when the pattern phase failed. */
	for (size_t i = count; i > 0; i--)
		write32p(words[i - 1].address, words[i - 1].original);
	b06v6_memory_barrier();

	for (size_t i = 0; i < count; i++) {
		const uint32_t observed = read32p(words[i].address);

		if (observed != words[i].original) {
			restore_ok = false;
			printk(BIOS_ERR,
			       "[RAMINIT] restore mismatch @%08lx expected=%08x observed=%08x\n",
			       words[i].address, words[i].original, observed);
		}
	}

	return patterns_ok && restore_ok;
}

#if CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS || \
	CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS
#define B06WG_CLEAR_READBACK_COUNT 9u

static bool b06wg_clear_window_once_sparse(uintptr_t base, size_t size)
{
	const uintptr_t end = base + size;
	const size_t quarter = (size / 4) & ~(size_t)3;
	const size_t readback_offsets[B06WG_CLEAR_READBACK_COUNT] = {
		0, 4, 0xffc, quarter, 2 * quarter, 3 * quarter,
		size - 0x1000, size - 8, size - 4,
	};

	if ((base & 3) || (size & 3) || size < 0x1000 || end <= base)
		return false;

	printk(BIOS_NOTICE,
	       "[RAMINIT] B06WG single-clear begin %08lx..%08lx; sparse-readback=%u\n",
	       base, end - 1, B06WG_CLEAR_READBACK_COUNT);
	for (uintptr_t address = base; address < end; address += 4)
		write32p(address, 0);
	b06v6_memory_barrier();

	for (size_t i = 0; i < ARRAY_SIZE(readback_offsets); i++) {
		const uintptr_t address = base + readback_offsets[i];
		const uint32_t observed = read32p(address);

		if (observed != 0) {
			printk(BIOS_ERR,
			       "[RAMINIT] B06WG sparse clear readback @%08lx observed=%08x\n",
			       address, observed);
			return false;
		}
	}

	printk(BIOS_NOTICE,
	       "[RAMINIT] B06WG single-clear sparse-readback PASS %08lx..%08lx\n",
	       base, end - 1);
	return true;
}
#else
static uint32_t b06v6_window_pattern(uintptr_t address, uint32_t seed)
{
	/* The odd multiplier is bijective over 32-bit values. */
	return seed ^ (uint32_t)address * 0x9e3779b1u;
}

static bool b06v6_destructive_window_test(uintptr_t base, size_t size,
	uint32_t seed)
{
	const uintptr_t end = base + size;
	bool passed = true;

	if ((base & 3) || (size & 3) || size == 0 || end <= base)
		return false;
	printk(BIOS_NOTICE,
	       "[RAMINIT] UC full-window test begin %08lx..%08lx\n",
	       base, end - 1);
	for (uintptr_t address = base; address < end; address += 4)
		write32p(address, b06v6_window_pattern(address, seed));
	b06v6_memory_barrier();
	for (uintptr_t address = base; address < end; address += 4) {
		const uint32_t expected = b06v6_window_pattern(address, seed);
		const uint32_t observed = read32p(address);

		if (observed != expected) {
			printk(BIOS_ERR,
			       "[RAMINIT] full-window phase A @%08lx expected=%08x observed=%08x\n",
			       address, expected, observed);
			passed = false;
			break;
		}
	}
	if (passed) {
		for (uintptr_t address = base; address < end; address += 4)
			write32p(address,
				~b06v6_window_pattern(address, seed));
		b06v6_memory_barrier();
		for (uintptr_t address = base; address < end; address += 4) {
			const uint32_t expected =
				~b06v6_window_pattern(address, seed);
			const uint32_t observed = read32p(address);

			if (observed != expected) {
				printk(BIOS_ERR,
				       "[RAMINIT] full-window phase B @%08lx expected=%08x observed=%08x\n",
				       address, expected, observed);
				passed = false;
				break;
			}
		}
	}

	/* Both regions are reserved scratch at this boundary; leave them clean. */
	for (uintptr_t address = base; address < end; address += 4)
		write32p(address, 0);
	b06v6_memory_barrier();
	for (uintptr_t address = base; address < end; address += 4) {
		const uint32_t observed = read32p(address);

		if (observed != 0) {
			printk(BIOS_ERR,
			       "[RAMINIT] full-window clear @%08lx observed=%08x\n",
			       address, observed);
			passed = false;
			break;
		}
	}
	printk(passed ? BIOS_NOTICE : BIOS_ERR,
	       "[RAMINIT] UC full-window test %s %08lx..%08lx\n",
	       passed ? "PASS" : "FAIL", base, end - 1);
	return passed;
}
#endif

void platform_romstage_post_mem(void)
{
	/*
	 * Keep both windows in one transaction.  Besides testing each selected
	 * address, this catches representative aliases between the CBMEM and
	 * object windows while all distinct patterns are resident simultaneously.
	 */
	static const uintptr_t window_smoke[] = {
		0x01000000, 0x01000004, 0x01000ffc, 0x01001000,
		0x013ffffc, 0x017ff000, 0x017ffffc,
		0x02000000, 0x02000004, 0x02000ffc, 0x02001000,
		0x023ffffc, 0x027ff000, 0x027ffffc,
	};
	const struct cbmem_entry *handoff_entry;
	struct x58_b06v6_handoff *handoff;
	msr_t mtrr_cap;
	msr_t mtrr_def_type;

	outb(POST_B06V6_POSTMEM_ENTRY, CONFIG_POST_IO_PORT);
	printk(BIOS_NOTICE,
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
	       "[RAMINIT] B06VL broad hard-gated v10 handoff entry; serial-independent SPD; DRAM remains UC\n");
#else
	       "[RAMINIT] B06VL broad hard-gated v9 handoff entry; DRAM remains UC\n");
#endif
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
	       "[RAMINIT] B06VK coupled PRIMARY/O/Q v8 handoff entry; DRAM remains UC\n");
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
	       "[RAMINIT] B06VJ coupled PRIMARY/O/Q v7 handoff entry; DRAM remains UC\n");
#elif CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	       "[RAMINIT] B06VI coupled PRIMARY/O v6 handoff entry; DRAM remains UC\n");
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
	       "[RAMINIT] B06VH exact PRIMARY workspace handoff entry; DRAM remains UC\n");
#elif CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
	       "[RAMINIT] B06VF canonical High-QPI payload handoff entry; DRAM remains UC\n");
#elif CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
	       "[RAMINIT] B06VE exact High-QPI post-MINIT handoff entry; DRAM remains UC\n");
#elif CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
	       "[RAMINIT] B06V7 robust Slow-QPI post-memory handoff entry; DRAM remains UC\n");
#else
	       "[RAMINIT] B06V6 post-memory exact handoff entry; DRAM remains UC\n");
#endif
	if (!result_recorded)
		b06v6_postmem_stop(POST_B06V6_NO_RESULT, "NO_EXACT_RESULT");
	if (!x58_b06v6_raminit_result_is_exact(&recorded_result))
		b06v6_postmem_stop(POST_B06V6_RESULT_MISMATCH,
				     "RESULT_CHANGED");
	if (!b06v6_mtrr_state_is_exact(&mtrr_cap, &mtrr_def_type))
		b06v6_postmem_stop(POST_B06V6_MTRR_MISMATCH,
				     "MTRR_NOT_EXACT_UC");
	outb(POST_B06V6_MTRR_GATED, CONFIG_POST_IO_PORT);

#if CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS || \
	CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS
	printk(BIOS_WARNING,
	       "[RAMINIT] EXPERIMENTAL B06WG FAST_POSTMEM: RAM/QPI ASSUMED_STABLE; exact result/MTRR gates retained, exhaustive tests omitted\n");
	if (!b06wg_clear_window_once_sparse(X58_B06VF_LOWMEM_BASE,
					    X58_B06VF_LOWMEM_SIZE))
		b06v6_postmem_stop(POST_B06V6_WINDOWS_SMOKE_FAILED,
				     "B06WG_LOWMEM_CLEAR_SPARSE_READBACK");
#else
#if CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
	/*
	 * This is the complete conventional-RAM window required by the first
	 * payload experiment.  The exact MTRR gate above proves that accesses are
	 * UC, and the test clears the complete range before CBMEM is created.
	 */
	if (!b06v6_destructive_window_test(X58_B06VF_LOWMEM_BASE,
					  X58_B06VF_LOWMEM_SIZE, 0x10b06f01u))
		b06v6_postmem_stop(POST_B06V6_WINDOWS_SMOKE_FAILED,
				     "B06VF_FULL_LOWMEM_TEST");
	printk(BIOS_NOTICE,
	       "[RAMINIT] B06VF UC lowmem full test PASS %08x..%08x; range cleared\n",
	       X58_B06VF_LOWMEM_BASE, X58_B06VF_LOWMEM_TOP - 1);
#endif
#endif

	if (!b06v6_transactional_smoke(window_smoke,
				       ARRAY_SIZE(window_smoke), 0x16cb20db))
		b06v6_postmem_stop(POST_B06V6_WINDOWS_SMOKE_FAILED,
				     "CBMEM_OBJECT_ALIAS_SMOKE");
#if CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS || \
	CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS
	if (!b06wg_clear_window_once_sparse(X58_B06V6_CBMEM_BASE,
					    X58_B06V6_CBMEM_SIZE) ||
	    !b06wg_clear_window_once_sparse(X58_B06V6_OBJECT_BASE,
					    X58_B06V6_OBJECT_SIZE))
		b06v6_postmem_stop(POST_B06V6_WINDOWS_SMOKE_FAILED,
				     "B06WG_HIGH_WINDOWS_CLEAR_SPARSE_READBACK");
	outb(POST_B06V6_WINDOWS_SMOKED, CONFIG_POST_IO_PORT);
	printk(BIOS_WARNING,
	       "[RAMINIT] B06WG FAST_POSTMEM PASS: sparse alias transaction + one clear pass/window + bounded readback; no phase-A/invert/full-read pass\n");
#else
	if (!b06v6_destructive_window_test(X58_B06V6_CBMEM_BASE,
					  X58_B06V6_CBMEM_SIZE, 0xc06b6c01) ||
	    !b06v6_destructive_window_test(X58_B06V6_OBJECT_BASE,
					  X58_B06V6_OBJECT_SIZE, 0x0b1ec701))
		b06v6_postmem_stop(POST_B06V6_WINDOWS_SMOKE_FAILED,
				     "FULL_WINDOW_TEST");
	outb(POST_B06V6_WINDOWS_SMOKED, CONFIG_POST_IO_PORT);
	printk(BIOS_NOTICE,
	       "[RAMINIT] UC alias+full test PASS CBMEM=%08x..%08x OBJECT=%08x..%08x; windows cleared\n",
	       X58_B06V6_CBMEM_BASE, X58_B06V6_CBMEM_TOP - 1,
	       X58_B06V6_OBJECT_BASE,
	       X58_B06V6_OBJECT_BASE + X58_B06V6_OBJECT_SIZE - 1);
#endif

	cbmem_initialize_empty_id_size(X58_B06V6_CBMEM_ID, sizeof(*handoff));
	handoff_entry = cbmem_entry_find(X58_B06V6_CBMEM_ID);
	if (!cbmem_online() || handoff_entry == NULL ||
	    cbmem_entry_size(handoff_entry) < sizeof(*handoff))
		b06v6_postmem_stop(POST_B06V6_CBMEM_FAILED,
				     "CBMEM_OFFLINE_OR_SIZE");
	handoff = cbmem_entry_start(handoff_entry);
	if (handoff == NULL || (uintptr_t)handoff < X58_B06V6_CBMEM_BASE ||
	    (uintptr_t)handoff > X58_B06V6_CBMEM_TOP - sizeof(*handoff))
		b06v6_postmem_stop(POST_B06V6_CBMEM_FAILED,
				     "CBMEM_ENTRY_RANGE");

	memset(handoff, 0, sizeof(*handoff));
	handoff->magic = X58_B06V6_HANDOFF_MAGIC;
	handoff->version = X58_B06V6_HANDOFF_VERSION;
	handoff->structure_size = sizeof(*handoff);
	handoff->flags = B06V6_REQUIRED_FLAGS;
	memcpy(&handoff->raminit, &recorded_result, sizeof(recorded_result));
	handoff->mtrr_cap = mtrr_cap.lo;
	handoff->mtrr_def_type = mtrr_def_type.lo;
	handoff->cbmem_base = X58_B06V6_CBMEM_BASE;
	handoff->cbmem_top = X58_B06V6_CBMEM_TOP;
	handoff->object_base = X58_B06V6_OBJECT_BASE;
	handoff->object_size = X58_B06V6_OBJECT_SIZE;
	handoff->object_max_size = X58_B06V6_OBJECT_MAX_SIZE;
	handoff->digest = x58_b06v6_handoff_digest(handoff);
	b06v6_memory_barrier();
	if (!x58_b06v6_handoff_is_valid(handoff))
		b06v6_postmem_stop(POST_B06V6_HANDOFF_FAILED,
				     "CBMEM_HANDOFF_READBACK");
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	/*
	 * PRIMARY arrives with B06VH's original pre-postmem rearm unchanged.  The
	 * exact O-family profiles deliberately retain MINIT's 08:00:5d:01:a3
	 * return tuple until every
	 * exact-result, MTRR, lowmem, alias, full-window, CBMEM and handoff
	 * readback above has passed.  Only then may it recreate the finalizer
	 * marker.
	 */
	if (x58_b06vi_profile_requires_deferred_rearm(
		    recorded_result.profile_id)) {
		if (!x58_b06vi_rearm_o_guard_after_postmem())
			b06v6_postmem_stop(POST_B06V6_HANDOFF_FAILED,
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
					     "B06VL_BROAD_POSTMEM_REARM");
		printk(BIOS_NOTICE,
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
		       "[RAMINIT] B06VL broad I801 guard rearmed only after full v10 postmem readback\n");
#else
		       "[RAMINIT] B06VL broad I801 guard rearmed only after full v9 postmem readback\n");
#endif
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
					     "B06VK_O_FAMILY_POSTMEM_REARM");
		printk(BIOS_NOTICE,
		       "[RAMINIT] B06VK O-family I801 guard rearmed only after full v8 postmem readback\n");
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
					     "B06VJ_O_FAMILY_POSTMEM_REARM");
		printk(BIOS_NOTICE,
		       "[RAMINIT] B06VJ O-family I801 guard rearmed only after full v7 postmem readback\n");
#else
					     "B06VI_O_POSTMEM_REARM");
		printk(BIOS_NOTICE,
		       "[RAMINIT] B06VI O I801 guard rearmed only after full v6 postmem readback\n");
#endif
	}
#endif
#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
	if (!x58_b06ve_finalize_persistent_guard())
		b06v6_postmem_stop(POST_B06V6_HANDOFF_FAILED,
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
				     "B06VL_GUARD_FINALIZE");
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
				     "B06VK_GUARD_FINALIZE");
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
				     "B06VJ_GUARD_FINALIZE");
#elif CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
				     "B06VI_GUARD_FINALIZE");
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
				     "B06VH_GUARD_FINALIZE");
#elif CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
				     "B06VF_GUARD_FINALIZE");
#else
				     "B06VE_GUARD_FINALIZE");
#endif
	printk(BIOS_NOTICE,
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
	       "[RAMINIT] B06VL persistent guard finalized after v10 CBMEM readback\n");
#else
	       "[RAMINIT] B06VL persistent guard finalized after v9 CBMEM readback\n");
#endif
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
	       "[RAMINIT] B06VK persistent guard finalized after v8 CBMEM readback\n");
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
	       "[RAMINIT] B06VJ persistent guard finalized after v7 CBMEM readback\n");
#elif CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	       "[RAMINIT] B06VI persistent guard finalized after v6 CBMEM readback\n");
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
	       "[RAMINIT] B06VH persistent guard finalized after v5 CBMEM readback\n");
#elif CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
	       "[RAMINIT] B06VF persistent guard finalized after v4 CBMEM readback\n");
#else
	       "[RAMINIT] B06VE persistent guard finalized after CBMEM readback\n");
#endif
#endif

	outb(POST_B06V6_CBMEM_READY, CONFIG_POST_IO_PORT);
	printk(BIOS_NOTICE,
	       "[RAMINIT] CBMEM READY top=%08x handoff=%p FNV=%08x; entering postcar\n",
	       X58_B06V6_CBMEM_TOP, handoff, handoff->digest);
	uart_tx_flush(get_uart_for_console());
}

#endif /* ENV_ROMSTAGE_OR_BEFORE */
