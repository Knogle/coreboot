/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_X58_RAMINIT_HANDOFF_H
#define MAINBOARD_MSI_X58_PRO_E_X58_RAMINIT_HANDOFF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "spd_profile.h"

/* Board-local CBMEM record.  The numeric value spells "X58H". */
#define X58_RAMINIT_CBMEM_ID		0x58353848u
#define X58_RAMINIT_HANDOFF_MAGIC		0x5836484fu /* "X6HO" */
/* Distinguish raw identity from the serial-neutral compatibility digest. */
#define X58_RAMINIT_HANDOFF_VERSION	10u

/* Conservative, hardware-smoked regions used by the first handoff build. */
#define X58_RAMINIT_CBMEM_BASE		0x01000000u
#define X58_RAMINIT_CBMEM_TOP		0x01800000u
#define X58_RAMINIT_CBMEM_SIZE		(X58_RAMINIT_CBMEM_TOP - \
						 X58_RAMINIT_CBMEM_BASE)
#define X58_RAMINIT_OBJECT_BASE		0x02000000u
#define X58_RAMINIT_OBJECT_SIZE		0x00800000u
#define X58_RAMINIT_OBJECT_MAX_SIZE	0x00400000u
#define X58_MEMORY_HANDOFF_LOWMEM_BASE		0x00000000u
#define X58_MEMORY_HANDOFF_LOWMEM_TOP		0x000a0000u
#define X58_MEMORY_HANDOFF_LOWMEM_SIZE		(X58_MEMORY_HANDOFF_LOWMEM_TOP - \
						 X58_MEMORY_HANDOFF_LOWMEM_BASE)

/* Exact X58_VENDOR_POLICY observations accepted by this deliberately narrow build. */
#define X58_RAMINIT_EXPECTED_CPUID		0x000206c2u
#define X58_RAMINIT_EXPECTED_UCODE		0x0000001fu
#define X58_RAMINIT_EXPECTED_SPD_FNV	0xfb66b530u
#define X58_RAMINIT_EXPECTED_CSI_FNV	0xa3736e20u
#define X58_RAMINIT_EXPECTED_POLICY_FNV	0x3c0f3a0bu
#define X58_RAMINIT_EXPECTED_WORKSPACE_FNV	0x6d87f4e4u
#define X58_CSI_STATE_EXPECTED_CSI_CANONICAL_FNV	0x8403ac98u
#define X58_CSI_STATE_EXPECTED_WORKSPACE_CANONICAL_FNV	0x7a34f363u
#define X58_MEMORY_RESULT_EXPECTED_CSI_FNV	0x03e3d24eu
#define X58_MEMORY_RESULT_EXPECTED_CSI_CANONICAL_FNV	0x908dabb6u
#define X58_MEMORY_RESULT_EXPECTED_WORKSPACE_FNV	0x94299f43u
/* Exact raw-hash/pattern pairs admitted only by X58_MEMORY_HANDOFF. */
#define X58_MEMORY_HANDOFF_EXPECTED_WORKSPACE_RAW_9429_FNV	0x94299f43u
#define X58_MEMORY_HANDOFF_EXPECTED_WORKSPACE_RAW_B634_FNV	0xb6346533u
#define X58_MEMORY_HANDOFF_EXPECTED_WORKSPACE_CANONICAL_FNV	0xa6f9c2e6u
/* Two additional exact PRIMARY returns admitted only by X58_WORKSPACE_STATE. */
#define X58_WORKSPACE_STATE_EXPECTED_WORKSPACE_RAW_EB15_FNV	0xeb15c076u
#define X58_WORKSPACE_STATE_EXPECTED_WORKSPACE_CANONICAL_C313_FNV	0xc313e060u
#define X58_WORKSPACE_STATE_EXPECTED_WORKSPACE_RAW_5FB6_FNV	0x5fb636d9u
#define X58_WORKSPACE_STATE_EXPECTED_WORKSPACE_CANONICAL_B3FA_FNV	0xb3fafec0u
/* Exact deferred workspace return admitted only by X58_PROFILE_POLICY. */
#define X58_PROFILE_POLICY_EXPECTED_CSI_DYNAMIC_DEFERRED	0x0cu
#define X58_PROFILE_POLICY_EXPECTED_CSI_RAW_8B38_FNV	0x8b38506au
#define X58_PROFILE_POLICY_EXPECTED_WORKSPACE_RAW_50F6_FNV	0x50f67315u
#define X58_PROFILE_POLICY_EXPECTED_WORKSPACE_CANONICAL_92C7_FNV	0x92c70df4u
/* Exact controlled-G3 return admitted only by X58_DEFERRED_PROFILE. */
#define X58_DEFERRED_PROFILE_EXPECTED_WORKSPACE_RAW_69B4_FNV	0x69b4c386u
#define X58_DEFERRED_PROFILE_EXPECTED_WORKSPACE_CANONICAL_0B16_FNV	0x0b161f01u
#define X58_RAMINIT_EXPECTED_MC_MAPPER	0x00024489u
#define X58_RAMINIT_EXPECTED_MC_COMMON_F8	0x00001545u
#define X58_RAMINIT_EXPECTED_CH2_DOD	0x000002acu
#define X58_RAMINIT_EXPECTED_CH2_RANKS	0x00000003u
#define X58_RAMINIT_EXPECTED_CH2_STATUS	0x00000140u
#define X58_RAMINIT_EXPECTED_QPI_STATUS	0x030f0f03u
#define X58_MEMORY_RESULT_EXPECTED_QPI_STATUS	0x070f0f03u
#define X58_MEMORY_RESULT_EXPECTED_POST_MINIT_IOH_STAGE_9C	0xbf000000u
#define X58_RAMINIT_WARM_REUSE_MARKER	0x57524d31u /* "WRM1" */

static inline bool x58_raminit_spd_digest_is_exact(uint32_t raw_digest,
					      uint32_t profile_digest)
{
	(void)raw_digest;
	return profile_digest == X58_SPD_PROFILE_FNV;
}

enum x58_raminit_handoff_flags {
	X58_RAMINIT_HANDOFF_EXACT_MINIT = 1u << 0,
	X58_RAMINIT_HANDOFF_MTRR_GATED = 1u << 1,
	X58_RAMINIT_HANDOFF_CBMEM_SMOKED = 1u << 2,
	X58_RAMINIT_HANDOFF_OBJECT_SMOKED = 1u << 3,
	X58_RAMINIT_HANDOFF_CBMEM_READY = 1u << 4,
	X58_MEMORY_RESULT_HANDOFF_HIGH_QPI_POST_MINIT = 1u << 5,
	X58_MEMORY_HANDOFF_LOWMEM_SMOKED = 1u << 6,
	/* Set only by X58_MEMORY_PROFILE after its hard post-MINIT producer gate passes. */
	X58_MEMORY_PROFILE_HANDOFF_BROAD_POST_MINIT = 1u << 7,
	/* No CSI/MINIT claim: live trained state was independently revalidated. */
	X58_RAMINIT_HANDOFF_WARM_REUSE = 1u << 8,
};

/*
 * These are wire values, not an inferred silicon classification.  Giving
 * each admitted PRIMARY workspace form its own ID prevents a consumer from
 * rebuilding acceptance from independent value lists.
 */
enum x58_profile_policy_profile_id {
	X58_PROFILE_POLICY_PROFILE_INVALID = 0,
	X58_PROFILE_POLICY_PROFILE_PRIMARY_9429 = 1,
	X58_PROFILE_POLICY_PROFILE_PRIMARY_B634 = 2,
	X58_PROFILE_POLICY_PROFILE_PRIMARY_EB15 = 3,
	X58_PROFILE_POLICY_PROFILE_PRIMARY_5FB6 = 4,
	X58_PROFILE_POLICY_PROFILE_DEFERRED_50F6 = 5,
	/* The second deferred workspace observation is exact, never a wildcard. */
	X58_PROFILE_POLICY_PROFILE_DEFERRED_0B16 = 6,
	/* X58_MEMORY_PROFILE raw/canonical workspace digests are integrity-covered telemetry. */
	X58_MEMORY_PROFILE_BROAD = 7,
	/* A completed boot's live controller state, not a MINIT result profile. */
	X58_MEMORY_PROFILE_WARM_REUSE = 8,
};

static inline bool x58_memory_profile_handoff_cpu_a0_candidate(uint32_t value)
{
	switch (value) {
	case 0x00017000u:
	case 0x00017200u:
	case 0x00017400u:
	case 0x00017600u:
	case 0x00017800u:
	case 0x00017a00u:
	case 0x00017c00u:
		return true;
	default:
		return false;
	}
}

/* Historical profiles retain their exact set.  The normal profile treats A0
 * as telemetry after stability was established by the producer; all coupled
 * CSI, endpoint, ABI and post-MINIT predicates below remain mandatory.
 */
static inline bool x58_memory_profile_handoff_normal_cpu_a0_candidate(uint32_t value)
{
	if (x58_memory_profile_handoff_cpu_a0_candidate(value))
		return true;
	(void)value;
	return true;
}

/*
 * Vendor-free result copied out only after the automatic romstage path has
 * passed every build-specific exact-result gate.  X58_MEMORY_RESULT deliberately leaves
 * its persistent reset-loop guard armed until post-memory validation finishes.
 */
struct x58_raminit_result {
	uint32_t cpuid;
	uint32_t microcode_revision;
	/* Unmodified full-256-byte identity hash, never a normalized surrogate. */
	uint32_t spd_fnv;
	uint32_t spd_profile_fnv;
	uint32_t csi_state_fnv;
	uint32_t policy_fnv;
	uint32_t workspace_fnv;
	uint32_t csi_state_canonical_fnv;
	uint32_t workspace_canonical_fnv;
	uint32_t minit_eax;
	uint32_t mc_mapper;
	uint32_t mc_common_f8;
	uint32_t ch2_dod;
	uint32_t ch2_ranks;
	uint32_t ch2_status;
	uint32_t qpi_status;
	uint32_t post_minit_ioh_stage_9c;
	uint32_t profile_id;
	uint32_t saved_pre_a0;
	uint32_t post_minit_cpu_a0;
	uint32_t post_minit_cpu_9c;
};

/* Persisted in CBMEM; all reserved words must remain zero. */
struct x58_raminit_handoff {
	uint32_t magic;
	uint32_t version;
	uint32_t structure_size;
	uint32_t flags;
	struct x58_raminit_result raminit;
	uint32_t mtrr_cap;
	uint32_t mtrr_def_type;
	uint32_t cbmem_base;
	uint32_t cbmem_top;
	uint32_t object_base;
	uint32_t object_size;
	uint32_t object_max_size;
	uint32_t reserved[8];
	uint32_t digest;
};

/* Romstage-only producer; calling it is the success signal. */
void x58_raminit_handoff_record_success(
	const struct x58_raminit_result *result);

/* Romstage implementation; called only after all DRAM and CBMEM readbacks. */
bool x58_memory_result_finalize_persistent_guard(void);
/* Exact deferred profiles reach post-memory with MINIT's return tuple present. */
bool x58_profile_policy_rearm_deferred_guard_after_postmem(void);

uint32_t x58_raminit_handoff_digest(const struct x58_raminit_handoff *handoff);
bool x58_raminit_result_is_exact(
	const struct x58_raminit_result *result);
bool x58_raminit_handoff_is_valid(const struct x58_raminit_handoff *handoff);

/* Shared producer/consumer truth table; raw and canonical digests stay paired. */
static inline bool x58_workspace_state_workspace_digest_pair_exact(
	uint32_t raw_digest, uint32_t canonical_digest)
{
	return (raw_digest == X58_MEMORY_HANDOFF_EXPECTED_WORKSPACE_RAW_9429_FNV &&
		canonical_digest == X58_MEMORY_HANDOFF_EXPECTED_WORKSPACE_CANONICAL_FNV) ||
		(raw_digest == X58_MEMORY_HANDOFF_EXPECTED_WORKSPACE_RAW_B634_FNV &&
		 canonical_digest == X58_MEMORY_HANDOFF_EXPECTED_WORKSPACE_CANONICAL_FNV) ||
		(raw_digest == X58_WORKSPACE_STATE_EXPECTED_WORKSPACE_RAW_EB15_FNV &&
		 canonical_digest == X58_WORKSPACE_STATE_EXPECTED_WORKSPACE_CANONICAL_C313_FNV) ||
		(raw_digest == X58_WORKSPACE_STATE_EXPECTED_WORKSPACE_RAW_5FB6_FNV &&
		 canonical_digest == X58_WORKSPACE_STATE_EXPECTED_WORKSPACE_CANONICAL_B3FA_FNV);
}

/*
 * The sole X58_PROFILE_POLICY admission truth table, shared verbatim by the romstage
 * producer and ramstage consumer.  Every row couples the profile ID, saved
 * and post-MINIT CPU endpoint, raw/canonical CSI pair, and raw/canonical
 * workspace pair. The deferred row deliberately treats only its raw workspace
 * digest as telemetry; the exact canonical digest and byte markers remain
 * coupled at the producer, and every other row retains its exact raw digest.
 * X58_MEMORY_PROFILE is a separate version-9 contract: it couples only the seven-value CPU/CSI
 * hard class here and treats both workspace digests as telemetry.  Its
 * completion, ABI, canary, I801, platform, and memory-test gates live in the
 * producer and are represented by a mandatory v9 evidence flag.
 * Do not replace this with independent allowlists.
 */
static inline enum x58_profile_policy_profile_id x58_profile_policy_profile_for_tuple(
	uint32_t saved_pre_a0, uint32_t post_minit_cpu_a0,
	uint32_t post_minit_cpu_9c, uint8_t csi_dynamic_2a6,
	uint32_t csi_raw_digest,
	uint32_t csi_canonical_digest, uint32_t workspace_raw_digest,
	uint32_t workspace_canonical_digest)
{
	if (x58_memory_profile_handoff_normal_cpu_a0_candidate(saved_pre_a0) &&
	    post_minit_cpu_a0 == saved_pre_a0 &&
	    (post_minit_cpu_9c == 0x00a00502u ||
	     post_minit_cpu_9c == 0x00b00502u) &&
	    csi_canonical_digest == X58_MEMORY_RESULT_EXPECTED_CSI_CANONICAL_FNV &&
	    ((csi_dynamic_2a6 == 0x08u &&
	      csi_raw_digest == X58_MEMORY_RESULT_EXPECTED_CSI_FNV) ||
	     (csi_dynamic_2a6 == X58_PROFILE_POLICY_EXPECTED_CSI_DYNAMIC_DEFERRED &&
	      csi_raw_digest == X58_PROFILE_POLICY_EXPECTED_CSI_RAW_8B38_FNV))) {
		(void)workspace_raw_digest;
		(void)workspace_canonical_digest;
		return X58_MEMORY_PROFILE_BROAD;
	}

	if (saved_pre_a0 == 0x00017000u &&
	    post_minit_cpu_a0 == 0x00017000u &&
	    post_minit_cpu_9c == 0x00b00502u &&
	    csi_dynamic_2a6 == 0x08u &&
	    csi_raw_digest == X58_MEMORY_RESULT_EXPECTED_CSI_FNV &&
	    csi_canonical_digest == X58_MEMORY_RESULT_EXPECTED_CSI_CANONICAL_FNV &&
	    workspace_raw_digest == X58_MEMORY_HANDOFF_EXPECTED_WORKSPACE_RAW_9429_FNV &&
	    workspace_canonical_digest ==
		X58_MEMORY_HANDOFF_EXPECTED_WORKSPACE_CANONICAL_FNV)
		return X58_PROFILE_POLICY_PROFILE_PRIMARY_9429;

	if (saved_pre_a0 == 0x00017000u &&
	    post_minit_cpu_a0 == 0x00017000u &&
	    post_minit_cpu_9c == 0x00b00502u &&
	    csi_dynamic_2a6 == 0x08u &&
	    csi_raw_digest == X58_MEMORY_RESULT_EXPECTED_CSI_FNV &&
	    csi_canonical_digest == X58_MEMORY_RESULT_EXPECTED_CSI_CANONICAL_FNV &&
	    workspace_raw_digest == X58_MEMORY_HANDOFF_EXPECTED_WORKSPACE_RAW_B634_FNV &&
	    workspace_canonical_digest ==
		X58_MEMORY_HANDOFF_EXPECTED_WORKSPACE_CANONICAL_FNV)
		return X58_PROFILE_POLICY_PROFILE_PRIMARY_B634;

	if (saved_pre_a0 == 0x00017000u &&
	    post_minit_cpu_a0 == 0x00017000u &&
	    post_minit_cpu_9c == 0x00b00502u &&
	    csi_dynamic_2a6 == 0x08u &&
	    csi_raw_digest == X58_MEMORY_RESULT_EXPECTED_CSI_FNV &&
	    csi_canonical_digest == X58_MEMORY_RESULT_EXPECTED_CSI_CANONICAL_FNV &&
	    workspace_raw_digest == X58_WORKSPACE_STATE_EXPECTED_WORKSPACE_RAW_EB15_FNV &&
	    workspace_canonical_digest ==
		X58_WORKSPACE_STATE_EXPECTED_WORKSPACE_CANONICAL_C313_FNV)
		return X58_PROFILE_POLICY_PROFILE_PRIMARY_EB15;

	if (saved_pre_a0 == 0x00017000u &&
	    post_minit_cpu_a0 == 0x00017000u &&
	    post_minit_cpu_9c == 0x00b00502u &&
	    csi_dynamic_2a6 == 0x08u &&
	    csi_raw_digest == X58_MEMORY_RESULT_EXPECTED_CSI_FNV &&
	    csi_canonical_digest == X58_MEMORY_RESULT_EXPECTED_CSI_CANONICAL_FNV &&
	    workspace_raw_digest == X58_WORKSPACE_STATE_EXPECTED_WORKSPACE_RAW_5FB6_FNV &&
	    workspace_canonical_digest ==
		X58_WORKSPACE_STATE_EXPECTED_WORKSPACE_CANONICAL_B3FA_FNV)
		return X58_PROFILE_POLICY_PROFILE_PRIMARY_5FB6;

	if (saved_pre_a0 == 0x00017c00u &&
	    post_minit_cpu_a0 == 0x00017c00u &&
	    post_minit_cpu_9c == 0x00a00502u &&
	    csi_dynamic_2a6 == X58_PROFILE_POLICY_EXPECTED_CSI_DYNAMIC_DEFERRED &&
	    csi_raw_digest == X58_PROFILE_POLICY_EXPECTED_CSI_RAW_8B38_FNV &&
	    csi_canonical_digest == X58_MEMORY_RESULT_EXPECTED_CSI_CANONICAL_FNV &&
	    workspace_raw_digest == X58_PROFILE_POLICY_EXPECTED_WORKSPACE_RAW_50F6_FNV &&
	    workspace_canonical_digest ==
		X58_PROFILE_POLICY_EXPECTED_WORKSPACE_CANONICAL_92C7_FNV)
		return X58_PROFILE_POLICY_PROFILE_DEFERRED_50F6;

	if (saved_pre_a0 == 0x00017c00u &&
	    post_minit_cpu_a0 == 0x00017c00u &&
	    post_minit_cpu_9c == 0x00a00502u &&
	    csi_dynamic_2a6 == X58_PROFILE_POLICY_EXPECTED_CSI_DYNAMIC_DEFERRED &&
	    csi_raw_digest == X58_PROFILE_POLICY_EXPECTED_CSI_RAW_8B38_FNV &&
	    csi_canonical_digest == X58_MEMORY_RESULT_EXPECTED_CSI_CANONICAL_FNV &&
	    workspace_canonical_digest ==
		X58_DEFERRED_PROFILE_EXPECTED_WORKSPACE_CANONICAL_0B16_FNV)
		return X58_PROFILE_POLICY_PROFILE_DEFERRED_0B16;

	return X58_PROFILE_POLICY_PROFILE_INVALID;
}

static inline bool x58_profile_policy_profile_requires_deferred_rearm(
	uint32_t profile_id)
{
	return profile_id == X58_PROFILE_POLICY_PROFILE_DEFERRED_50F6
		|| profile_id == X58_PROFILE_POLICY_PROFILE_DEFERRED_0B16
		|| profile_id == X58_MEMORY_PROFILE_BROAD
		;
}

static inline bool x58_profile_policy_profile_tuple_exact(
	uint32_t profile_id, uint32_t saved_pre_a0,
	uint32_t post_minit_cpu_a0, uint32_t post_minit_cpu_9c,
	uint32_t csi_raw_digest, uint32_t csi_canonical_digest,
	uint32_t workspace_raw_digest, uint32_t workspace_canonical_digest)
{
	uint8_t csi_dynamic_2a6;
	enum x58_profile_policy_profile_id selected;

	if (profile_id == X58_PROFILE_POLICY_PROFILE_DEFERRED_50F6
	    || profile_id == X58_PROFILE_POLICY_PROFILE_DEFERRED_0B16
	   )
		csi_dynamic_2a6 = X58_PROFILE_POLICY_EXPECTED_CSI_DYNAMIC_DEFERRED;
	else if (profile_id == X58_MEMORY_PROFILE_BROAD &&
		 csi_raw_digest == X58_MEMORY_RESULT_EXPECTED_CSI_FNV)
		csi_dynamic_2a6 = 0x08;
	else if (profile_id == X58_MEMORY_PROFILE_BROAD &&
		 csi_raw_digest == X58_PROFILE_POLICY_EXPECTED_CSI_RAW_8B38_FNV)
		csi_dynamic_2a6 = X58_PROFILE_POLICY_EXPECTED_CSI_DYNAMIC_DEFERRED;
	else if (profile_id == X58_PROFILE_POLICY_PROFILE_PRIMARY_9429 ||
		 profile_id == X58_PROFILE_POLICY_PROFILE_PRIMARY_B634 ||
		 profile_id == X58_PROFILE_POLICY_PROFILE_PRIMARY_EB15 ||
		 profile_id == X58_PROFILE_POLICY_PROFILE_PRIMARY_5FB6)
		csi_dynamic_2a6 = 0x08;
	else
		return false;

	selected = x58_profile_policy_profile_for_tuple(
		saved_pre_a0, post_minit_cpu_a0, post_minit_cpu_9c,
		csi_dynamic_2a6,
		csi_raw_digest, csi_canonical_digest, workspace_raw_digest,
		workspace_canonical_digest);

	return selected != X58_PROFILE_POLICY_PROFILE_INVALID && profile_id == selected;
}

static inline bool x58_profile_policy_profile_is_primary(uint32_t profile_id)
{
	return profile_id == X58_PROFILE_POLICY_PROFILE_PRIMARY_9429 ||
		profile_id == X58_PROFILE_POLICY_PROFILE_PRIMARY_B634 ||
		profile_id == X58_PROFILE_POLICY_PROFILE_PRIMARY_EB15 ||
		profile_id == X58_PROFILE_POLICY_PROFILE_PRIMARY_5FB6;
}

#endif /* MAINBOARD_MSI_X58_PRO_E_X58_RAMINIT_HANDOFF_H */
