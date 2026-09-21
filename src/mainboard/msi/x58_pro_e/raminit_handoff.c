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

#include "raminit_handoff.h"

#define POST_X58_RAMINIT_POSTMEM_ENTRY	0x09
#define POST_X58_RAMINIT_MTRR_GATED	0x0a
#define POST_X58_RAMINIT_WINDOWS_SMOKED	0x0b
#define POST_X58_RAMINIT_CBMEM_READY	0x0c

#define POST_X58_RAMINIT_NO_RESULT	0x14
#define POST_X58_RAMINIT_RESULT_MISMATCH	0x15
#define POST_X58_RAMINIT_MTRR_MISMATCH	0x16
#define POST_X58_RAMINIT_WINDOWS_SMOKE_FAILED	0x17
#define POST_X58_RAMINIT_CBMEM_FAILED	0x18
#define POST_X58_RAMINIT_HANDOFF_FAILED	0x19

#define X58_RAMINIT_EXPECTED_MTRR_CAP	0x00000d0au
#define X58_RAMINIT_EXPECTED_MTRR_DEF_TYPE	MTRR_DEF_TYPE_EN
#define X58_RAMINIT_EXPECTED_PHYS_BITS	40u
#define X58_RAMINIT_EXPECTED_MTRR_HIGH_MASK	0x000000ffu

#define X58_RAMINIT_CAR_MTRR_BASE_LO	0xfff80006u
#define X58_RAMINIT_CAR_MTRR_MASK_LO	0xffff0800u
#define X58_RAMINIT_ROM_MTRR_BASE_LO	0xfffc0005u
#define X58_RAMINIT_ROM_MTRR_MASK_LO	0xfffc0800u

#define X58_RAMINIT_POSTMEM_REQUIRED_FLAGS \
	(X58_RAMINIT_HANDOFF_MTRR_GATED | \
	 X58_RAMINIT_HANDOFF_CBMEM_SMOKED | \
	 X58_RAMINIT_HANDOFF_OBJECT_SMOKED | X58_RAMINIT_HANDOFF_CBMEM_READY)
#define X58_RAMINIT_COMMON_REQUIRED_FLAGS \
	(X58_RAMINIT_POSTMEM_REQUIRED_FLAGS | X58_RAMINIT_HANDOFF_EXACT_MINIT)
#define X58_MEMORY_PROFILE_REQUIRED_FLAG X58_MEMORY_PROFILE_HANDOFF_BROAD_POST_MINIT
#define X58_RAMINIT_REQUIRED_FLAGS \
	(X58_RAMINIT_COMMON_REQUIRED_FLAGS | X58_MEMORY_RESULT_HANDOFF_HIGH_QPI_POST_MINIT | \
	 X58_MEMORY_HANDOFF_LOWMEM_SMOKED | X58_MEMORY_PROFILE_REQUIRED_FLAG)

#define X58_RAMINIT_WARM_REQUIRED_FLAGS \
	(X58_RAMINIT_POSTMEM_REQUIRED_FLAGS | X58_RAMINIT_HANDOFF_WARM_REUSE | \
	 X58_MEMORY_RESULT_HANDOFF_HIGH_QPI_POST_MINIT | \
	 X58_MEMORY_HANDOFF_LOWMEM_SMOKED | X58_MEMORY_PROFILE_HANDOFF_BROAD_POST_MINIT)

_Static_assert(sizeof(struct x58_raminit_handoff) == 164,
	"Serial-independent v10 handoff wire size changed");

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

uint32_t x58_raminit_handoff_digest(const struct x58_raminit_handoff *handoff)
{
	if (handoff == NULL)
		return 0;

	/* digest is last and is deliberately excluded from its own coverage. */
	return fnv1a32(handoff, offsetof(struct x58_raminit_handoff, digest));
}

bool x58_raminit_result_is_exact(
	const struct x58_raminit_result *result)
{
	/*
	 * Match coreboot's fast/warm-boot shape without inventing cached MINIT
	 * output: the producer records only the independently read live endpoint.
	 * Zero vendor digests make it impossible to mistake this for a cold MINIT
	 * result.  Destructive reserved-memory and CBMEM tests still follow.
	 */
	if (result != NULL && result->minit_eax == X58_RAMINIT_WARM_REUSE_MARKER)
		return result->cpuid == X58_RAMINIT_EXPECTED_CPUID &&
			result->microcode_revision == X58_RAMINIT_EXPECTED_UCODE &&
			x58_raminit_spd_digest_is_exact(result->spd_fnv,
				result->spd_profile_fnv) &&
			result->csi_state_fnv == 0 && result->policy_fnv == 0 &&
			result->workspace_fnv == 0 &&
			result->csi_state_canonical_fnv == 0 &&
			result->workspace_canonical_fnv == 0 &&
			result->mc_mapper == X58_RAMINIT_EXPECTED_MC_MAPPER &&
			result->mc_common_f8 == X58_RAMINIT_EXPECTED_MC_COMMON_F8 &&
			result->ch2_dod == X58_RAMINIT_EXPECTED_CH2_DOD &&
			result->ch2_ranks == X58_RAMINIT_EXPECTED_CH2_RANKS &&
			result->ch2_status == X58_RAMINIT_EXPECTED_CH2_STATUS &&
			result->qpi_status == X58_MEMORY_RESULT_EXPECTED_QPI_STATUS &&
			result->post_minit_ioh_stage_9c ==
				X58_MEMORY_RESULT_EXPECTED_POST_MINIT_IOH_STAGE_9C &&
			result->profile_id == X58_MEMORY_PROFILE_WARM_REUSE &&
			x58_memory_profile_handoff_normal_cpu_a0_candidate(
				result->saved_pre_a0) &&
			result->post_minit_cpu_a0 == result->saved_pre_a0 &&
			(result->post_minit_cpu_9c == 0x00a00502u ||
			 result->post_minit_cpu_9c == 0x00b00502u);
	return result != NULL &&
		result->cpuid == X58_RAMINIT_EXPECTED_CPUID &&
		result->microcode_revision == X58_RAMINIT_EXPECTED_UCODE &&
		x58_raminit_spd_digest_is_exact(result->spd_fnv,
			result->spd_profile_fnv) &&
		result->profile_id == X58_MEMORY_PROFILE_BROAD &&
		x58_profile_policy_profile_tuple_exact(result->profile_id,
			result->saved_pre_a0, result->post_minit_cpu_a0,
			result->post_minit_cpu_9c, result->csi_state_fnv,
			result->csi_state_canonical_fnv, result->workspace_fnv,
			result->workspace_canonical_fnv) &&
		result->policy_fnv == X58_RAMINIT_EXPECTED_POLICY_FNV &&
		/* The shared profile truth table owns the applicable digest contract. */
		result->minit_eax == 0 &&
		result->mc_mapper == X58_RAMINIT_EXPECTED_MC_MAPPER &&
		result->mc_common_f8 == X58_RAMINIT_EXPECTED_MC_COMMON_F8 &&
		result->ch2_dod == X58_RAMINIT_EXPECTED_CH2_DOD &&
		result->ch2_ranks == X58_RAMINIT_EXPECTED_CH2_RANKS &&
		result->ch2_status == X58_RAMINIT_EXPECTED_CH2_STATUS &&
		result->qpi_status == X58_MEMORY_RESULT_EXPECTED_QPI_STATUS &&
		result->post_minit_ioh_stage_9c ==
			X58_MEMORY_RESULT_EXPECTED_POST_MINIT_IOH_STAGE_9C;
}

bool x58_raminit_handoff_is_valid(const struct x58_raminit_handoff *handoff)
{
	if (handoff == NULL || handoff->magic != X58_RAMINIT_HANDOFF_MAGIC ||
	    handoff->version != X58_RAMINIT_HANDOFF_VERSION ||
	    handoff->structure_size != sizeof(*handoff) ||
	    !x58_raminit_result_is_exact(&handoff->raminit) ||
	    handoff->mtrr_cap != X58_RAMINIT_EXPECTED_MTRR_CAP ||
	    handoff->mtrr_def_type != X58_RAMINIT_EXPECTED_MTRR_DEF_TYPE ||
	    handoff->cbmem_base != X58_RAMINIT_CBMEM_BASE ||
	    handoff->cbmem_top != X58_RAMINIT_CBMEM_TOP ||
	    handoff->object_base != X58_RAMINIT_OBJECT_BASE ||
	    handoff->object_size != X58_RAMINIT_OBJECT_SIZE ||
	    handoff->object_max_size != X58_RAMINIT_OBJECT_MAX_SIZE)
		return false;

	if (handoff->raminit.minit_eax == X58_RAMINIT_WARM_REUSE_MARKER) {
		if (handoff->flags != X58_RAMINIT_WARM_REQUIRED_FLAGS)
			return false;
	} else {
		if (handoff->flags != X58_RAMINIT_REQUIRED_FLAGS)
			return false;
	}

	for (size_t i = 0; i < ARRAY_SIZE(handoff->reserved); i++) {
		if (handoff->reserved[i] != 0)
			return false;
	}

	return handoff->digest == x58_raminit_handoff_digest(handoff);
}

#if ENV_ROMSTAGE_OR_BEFORE

static struct x58_raminit_result recorded_result;
static bool result_recorded;

void x58_raminit_handoff_record_success(
	const struct x58_raminit_result *result)
{
	result_recorded = false;
	memset(&recorded_result, 0, sizeof(recorded_result));
	if (!x58_raminit_result_is_exact(result))
		return;
	memcpy(&recorded_result, result, sizeof(recorded_result));
	result_recorded = true;
}

static void __noreturn x58_raminit_postmem_stop(uint8_t post, const char *reason)
{
	printk(BIOS_EMERG, "[RAMINIT] POSTMEM FAIL %s POST=%02x\n", reason,
	       post);
	uart_tx_flush(get_uart_for_console());
	outb(post, CONFIG_POST_IO_PORT);
	asm volatile ("cli" ::: "memory");
	for (;;)
		asm volatile ("hlt");
}

static bool x58_raminit_mtrr_state_is_exact(msr_t *cap_out, msr_t *def_out)
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
	exact = (address_bits.eax & 0xff) == X58_RAMINIT_EXPECTED_PHYS_BITS &&
		cap.hi == 0 && cap.lo == X58_RAMINIT_EXPECTED_MTRR_CAP &&
		def_type.hi == 0 && def_type.lo == X58_RAMINIT_EXPECTED_MTRR_DEF_TYPE &&
		base0.hi == 0 && base0.lo == X58_RAMINIT_CAR_MTRR_BASE_LO &&
		mask0.hi == X58_RAMINIT_EXPECTED_MTRR_HIGH_MASK &&
		mask0.lo == X58_RAMINIT_CAR_MTRR_MASK_LO &&
		base1.hi == 0 && base1.lo == X58_RAMINIT_ROM_MTRR_BASE_LO &&
		mask1.hi == X58_RAMINIT_EXPECTED_MTRR_HIGH_MASK &&
		mask1.lo == X58_RAMINIT_ROM_MTRR_MASK_LO;

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

static void x58_raminit_memory_barrier(void)
{
	/* The fixed E5645 target has SSE2; this also orders UC stores. */
	asm volatile ("mfence" ::: "memory");
}

static bool x58_raminit_transactional_smoke(const uintptr_t *addresses,
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
	x58_raminit_memory_barrier();

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
	x58_raminit_memory_barrier();

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

#define X58_USB_POWER_CLEAR_READBACK_COUNT 9u

static bool x58_usb_power_clear_window_once_sparse(uintptr_t base, size_t size)
{
	const uintptr_t end = base + size;
	const size_t quarter = (size / 4) & ~(size_t)3;
	const size_t readback_offsets[X58_USB_POWER_CLEAR_READBACK_COUNT] = {
		0, 4, 0xffc, quarter, 2 * quarter, 3 * quarter,
		size - 0x1000, size - 8, size - 4,
	};

	if ((base & 3) || (size & 3) || size < 0x1000 || end <= base)
		return false;

	printk(BIOS_DEBUG,
	       "[RAMINIT] X58_USB_POWER single-clear begin %08lx..%08lx; sparse-readback=%u\n",
	       base, end - 1, X58_USB_POWER_CLEAR_READBACK_COUNT);
	for (uintptr_t address = base; address < end; address += 4)
		write32p(address, 0);
	x58_raminit_memory_barrier();

	for (size_t i = 0; i < ARRAY_SIZE(readback_offsets); i++) {
		const uintptr_t address = base + readback_offsets[i];
		const uint32_t observed = read32p(address);

		if (observed != 0) {
			printk(BIOS_ERR,
			       "[RAMINIT] X58_USB_POWER sparse clear readback @%08lx observed=%08x\n",
			       address, observed);
			return false;
		}
	}

	printk(BIOS_DEBUG,
	       "[RAMINIT] X58_USB_POWER single-clear sparse-readback PASS %08lx..%08lx\n",
	       base, end - 1);
	return true;
}

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
	struct x58_raminit_handoff *handoff;
	msr_t mtrr_cap;
	msr_t mtrr_def_type;

	outb(POST_X58_RAMINIT_POSTMEM_ENTRY, CONFIG_POST_IO_PORT);
	printk(BIOS_NOTICE,
	       "[RAMINIT] X58_MEMORY_PROFILE broad hard-gated v10 handoff entry; serial-independent SPD; DRAM remains UC\n");
	if (!result_recorded)
		x58_raminit_postmem_stop(POST_X58_RAMINIT_NO_RESULT, "NO_EXACT_RESULT");
	if (!x58_raminit_result_is_exact(&recorded_result))
		x58_raminit_postmem_stop(POST_X58_RAMINIT_RESULT_MISMATCH,
				     "RESULT_CHANGED");
	if (!x58_raminit_mtrr_state_is_exact(&mtrr_cap, &mtrr_def_type))
		x58_raminit_postmem_stop(POST_X58_RAMINIT_MTRR_MISMATCH,
				     "MTRR_NOT_EXACT_UC");
	outb(POST_X58_RAMINIT_MTRR_GATED, CONFIG_POST_IO_PORT);

	printk(BIOS_WARNING,
	       "[RAMINIT] UNQUALIFIED X58_USB_POWER FAST_POSTMEM: RAM/QPI ASSUMED_STABLE; exact result/MTRR gates retained, exhaustive tests omitted\n");
	printk(BIOS_DEBUG, "[PLATFORM] EXTRA_MEMORY_DIAGNOSTICS=SKIPPED STABILITY_NOT_REQUALIFIED=1\n");
	if (!x58_usb_power_clear_window_once_sparse(X58_MEMORY_HANDOFF_LOWMEM_BASE,
					    X58_MEMORY_HANDOFF_LOWMEM_SIZE))
		x58_raminit_postmem_stop(POST_X58_RAMINIT_WINDOWS_SMOKE_FAILED,
				     "X58_USB_POWER_LOWMEM_CLEAR_SPARSE_READBACK");

	if (!x58_raminit_transactional_smoke(window_smoke,
				       ARRAY_SIZE(window_smoke), 0x16cb20db))
		x58_raminit_postmem_stop(POST_X58_RAMINIT_WINDOWS_SMOKE_FAILED,
				     "CBMEM_OBJECT_ALIAS_SMOKE");
	/* CBMEM must start empty so its allocator cannot accept stale metadata.
	 * The historical object arena has no normal-path consumer: the recovery
	 * loader overwrites and verifies every uploaded byte before use.  Keep its
	 * representative words in the transactional alias test above, but avoid
	 * two million unnecessary uncached stores on every normal boot. */
	if (!x58_usb_power_clear_window_once_sparse(X58_RAMINIT_CBMEM_BASE,
					    X58_RAMINIT_CBMEM_SIZE))
		x58_raminit_postmem_stop(POST_X58_RAMINIT_WINDOWS_SMOKE_FAILED,
				     "X58_USB_POWER_CBMEM_CLEAR_SPARSE_READBACK");
	outb(POST_X58_RAMINIT_WINDOWS_SMOKED, CONFIG_POST_IO_PORT);
	printk(BIOS_WARNING,
	       "[RAMINIT] X58_USB_POWER FAST_POSTMEM PASS: sparse CBMEM/object alias transaction + lowmem/CBMEM clear with bounded readback; object arena left for verified overwrite; no phase-A/invert/full-read pass\n");

	cbmem_initialize_empty_id_size(X58_RAMINIT_CBMEM_ID, sizeof(*handoff));
	handoff_entry = cbmem_entry_find(X58_RAMINIT_CBMEM_ID);
	if (!cbmem_online() || handoff_entry == NULL ||
	    cbmem_entry_size(handoff_entry) < sizeof(*handoff))
		x58_raminit_postmem_stop(POST_X58_RAMINIT_CBMEM_FAILED,
				     "CBMEM_OFFLINE_OR_SIZE");
	handoff = cbmem_entry_start(handoff_entry);
	if (handoff == NULL || (uintptr_t)handoff < X58_RAMINIT_CBMEM_BASE ||
	    (uintptr_t)handoff > X58_RAMINIT_CBMEM_TOP - sizeof(*handoff))
		x58_raminit_postmem_stop(POST_X58_RAMINIT_CBMEM_FAILED,
				     "CBMEM_ENTRY_RANGE");

	memset(handoff, 0, sizeof(*handoff));
	handoff->magic = X58_RAMINIT_HANDOFF_MAGIC;
	handoff->version = X58_RAMINIT_HANDOFF_VERSION;
	handoff->structure_size = sizeof(*handoff);
	handoff->flags = X58_RAMINIT_REQUIRED_FLAGS;
	if (recorded_result.minit_eax == X58_RAMINIT_WARM_REUSE_MARKER)
		handoff->flags = X58_RAMINIT_WARM_REQUIRED_FLAGS;
	memcpy(&handoff->raminit, &recorded_result, sizeof(recorded_result));
	handoff->mtrr_cap = mtrr_cap.lo;
	handoff->mtrr_def_type = mtrr_def_type.lo;
	handoff->cbmem_base = X58_RAMINIT_CBMEM_BASE;
	handoff->cbmem_top = X58_RAMINIT_CBMEM_TOP;
	handoff->object_base = X58_RAMINIT_OBJECT_BASE;
	handoff->object_size = X58_RAMINIT_OBJECT_SIZE;
	handoff->object_max_size = X58_RAMINIT_OBJECT_MAX_SIZE;
	handoff->digest = x58_raminit_handoff_digest(handoff);
	x58_raminit_memory_barrier();
	if (!x58_raminit_handoff_is_valid(handoff))
		x58_raminit_postmem_stop(POST_X58_RAMINIT_HANDOFF_FAILED,
				     "CBMEM_HANDOFF_READBACK");
	/*
	 * PRIMARY arrives with X58_WORKSPACE_STATE's original pre-postmem rearm unchanged.  The
	 * exact deferred profiles deliberately retain MINIT's 08:00:5d:01:a3
	 * return tuple until every
	 * exact-result, MTRR, lowmem clear, CBMEM/object alias smoke, CBMEM
	 * clear and handoff readback above has passed.  Only then may it recreate
	 * the finalizer marker.
	 */
	if (x58_profile_policy_profile_requires_deferred_rearm(
		    recorded_result.profile_id)) {
		if (!x58_profile_policy_rearm_deferred_guard_after_postmem())
			x58_raminit_postmem_stop(POST_X58_RAMINIT_HANDOFF_FAILED,
					     "X58_MEMORY_PROFILE_BROAD_POSTMEM_REARM");
		printk(BIOS_DEBUG,
		       "[RAMINIT] X58_MEMORY_PROFILE broad I801 guard rearmed only after full v10 postmem readback\n");
	}
	if (!x58_memory_result_finalize_persistent_guard())
		x58_raminit_postmem_stop(POST_X58_RAMINIT_HANDOFF_FAILED,
				     "X58_MEMORY_PROFILE_GUARD_FINALIZE");
	printk(BIOS_DEBUG,
	       "[RAMINIT] X58_MEMORY_PROFILE persistent guard finalized after v10 CBMEM readback\n");

	outb(POST_X58_RAMINIT_CBMEM_READY, CONFIG_POST_IO_PORT);
	printk(BIOS_NOTICE,
	       "[RAMINIT] CBMEM READY top=%08x handoff=%p FNV=%08x; entering postcar\n",
	       X58_RAMINIT_CBMEM_TOP, handoff, handoff->digest);
	uart_tx_flush(get_uart_for_console());
}

#endif /* ENV_ROMSTAGE_OR_BEFORE */
