/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_B06V6_HANDOFF_H
#define MAINBOARD_MSI_X58_PRO_E_B06V6_HANDOFF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "spd_profile.h"

/* Board-local CBMEM record.  The numeric value spells "X58H". */
#define X58_B06V6_CBMEM_ID		0x58353848u
#define X58_B06V6_HANDOFF_MAGIC		0x5836484fu /* "X6HO" */
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
/* Distinguish raw identity from the serial-neutral compatibility digest. */
#define X58_B06V6_HANDOFF_VERSION	10u
#else
#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
#define X58_B06V6_HANDOFF_VERSION	9u
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
#define X58_B06V6_HANDOFF_VERSION	8u
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
#define X58_B06V6_HANDOFF_VERSION	7u
#elif CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
#define X58_B06V6_HANDOFF_VERSION	6u
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
#define X58_B06V6_HANDOFF_VERSION	5u
#else
#if CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
#define X58_B06V6_HANDOFF_VERSION	4u
#else
#define X58_B06V6_HANDOFF_VERSION	3u
#endif
#endif
#elif CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
#define X58_B06V6_HANDOFF_VERSION	2u
#else
#define X58_B06V6_HANDOFF_VERSION	1u
#endif
#endif

/* Conservative, hardware-smoked regions used by the first handoff build. */
#define X58_B06V6_CBMEM_BASE		0x01000000u
#define X58_B06V6_CBMEM_TOP		0x01800000u
#define X58_B06V6_CBMEM_SIZE		(X58_B06V6_CBMEM_TOP - \
						 X58_B06V6_CBMEM_BASE)
#define X58_B06V6_OBJECT_BASE		0x02000000u
#define X58_B06V6_OBJECT_SIZE		0x00800000u
#define X58_B06V6_OBJECT_MAX_SIZE	0x00400000u
#define X58_B06VF_LOWMEM_BASE		0x00000000u
#define X58_B06VF_LOWMEM_TOP		0x000a0000u
#define X58_B06VF_LOWMEM_SIZE		(X58_B06VF_LOWMEM_TOP - \
						 X58_B06VF_LOWMEM_BASE)

/* Exact B06V5 observations accepted by this deliberately narrow build. */
#define X58_B06V6_EXPECTED_CPUID		0x000206c2u
#define X58_B06V6_EXPECTED_UCODE		0x0000001fu
#define X58_B06V6_EXPECTED_SPD_FNV	0xfb66b530u
#define X58_B06V6_EXPECTED_CSI_FNV	0xa3736e20u
#define X58_B06V6_EXPECTED_POLICY_FNV	0x3c0f3a0bu
#define X58_B06V6_EXPECTED_WORKSPACE_FNV	0x6d87f4e4u
#define X58_B06V7_EXPECTED_CSI_CANONICAL_FNV	0x8403ac98u
#define X58_B06V7_EXPECTED_WORKSPACE_CANONICAL_FNV	0x7a34f363u
#define X58_B06VE_EXPECTED_CSI_FNV	0x03e3d24eu
#define X58_B06VE_EXPECTED_CSI_CANONICAL_FNV	0x908dabb6u
#define X58_B06VE_EXPECTED_WORKSPACE_FNV	0x94299f43u
/* Exact raw-hash/pattern pairs admitted only by B06VF. */
#define X58_B06VF_EXPECTED_WORKSPACE_RAW_A_FNV	0x94299f43u
#define X58_B06VF_EXPECTED_WORKSPACE_RAW_B_FNV	0xb6346533u
#define X58_B06VF_EXPECTED_WORKSPACE_CANONICAL_FNV	0xa6f9c2e6u
/* Two additional exact PRIMARY returns admitted only by B06VH. */
#define X58_B06VH_EXPECTED_WORKSPACE_RAW_P_FNV	0xeb15c076u
#define X58_B06VH_EXPECTED_WORKSPACE_CANONICAL_P_FNV	0xc313e060u
#define X58_B06VH_EXPECTED_WORKSPACE_RAW_N_FNV	0x5fb636d9u
#define X58_B06VH_EXPECTED_WORKSPACE_CANONICAL_N_FNV	0xb3fafec0u
/* Exact B06VH-HW-G3-04 OBSERVATION return promoted only by B06VI. */
#define X58_B06VI_EXPECTED_CSI_DYNAMIC_O	0x0cu
#define X58_B06VI_EXPECTED_CSI_RAW_O_FNV	0x8b38506au
#define X58_B06VI_EXPECTED_WORKSPACE_RAW_O_FNV	0x50f67315u
#define X58_B06VI_EXPECTED_WORKSPACE_CANONICAL_O_FNV	0x92c70df4u
/* Exact B06VI-HW-G3-02 controlled-G3 return promoted only by B06VJ. */
#define X58_B06VJ_EXPECTED_WORKSPACE_RAW_Q_FNV	0x69b4c386u
#define X58_B06VJ_EXPECTED_WORKSPACE_CANONICAL_Q_FNV	0x0b161f01u
#define X58_B06V6_EXPECTED_MC_MAPPER	0x00024489u
#define X58_B06V6_EXPECTED_MC_COMMON_F8	0x00001545u
#define X58_B06V6_EXPECTED_CH2_DOD	0x000002acu
#define X58_B06V6_EXPECTED_CH2_RANKS	0x00000003u
#define X58_B06V6_EXPECTED_CH2_STATUS	0x00000140u
#define X58_B06V6_EXPECTED_QPI_STATUS	0x030f0f03u
#define X58_B06VE_EXPECTED_QPI_STATUS	0x070f0f03u
#define X58_B06VE_EXPECTED_POST_MINIT_IOH_STAGE_9C	0xbf000000u

static inline bool x58_b06v6_spd_digest_is_exact(uint32_t raw_digest,
					      uint32_t profile_digest)
{
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
	(void)raw_digest;
	return profile_digest == X58_SPD_PROFILE_FNV;
#else
	(void)profile_digest;
	return raw_digest == X58_B06V6_EXPECTED_SPD_FNV;
#endif
}

enum x58_b06v6_handoff_flags {
	X58_B06V6_HANDOFF_EXACT_MINIT = 1u << 0,
	X58_B06V6_HANDOFF_MTRR_GATED = 1u << 1,
	X58_B06V6_HANDOFF_CBMEM_SMOKED = 1u << 2,
	X58_B06V6_HANDOFF_OBJECT_SMOKED = 1u << 3,
	X58_B06V6_HANDOFF_CBMEM_READY = 1u << 4,
	X58_B06VE_HANDOFF_HIGH_QPI_POST_MINIT = 1u << 5,
	X58_B06VF_HANDOFF_LOWMEM_SMOKED = 1u << 6,
	/* Set only by B06VL after its hard post-MINIT producer gate passes. */
	X58_B06VL_HANDOFF_BROAD_POST_MINIT = 1u << 7,
};

#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
/*
 * These are wire values, not an inferred silicon classification.  Giving
 * each admitted PRIMARY workspace form its own ID prevents a consumer from
 * rebuilding acceptance from independent value lists.
 */
enum x58_b06vi_profile_id {
	X58_B06VI_PROFILE_INVALID = 0,
	X58_B06VI_PROFILE_PRIMARY_A = 1,
	X58_B06VI_PROFILE_PRIMARY_B = 2,
	X58_B06VI_PROFILE_PRIMARY_P = 3,
	X58_B06VI_PROFILE_PRIMARY_N = 4,
	X58_B06VI_PROFILE_O = 5,
#if CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
	/* Q is the second exact O-family workspace observation, never a wildcard. */
	X58_B06VJ_PROFILE_Q = 6,
#endif
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	/* B06VL raw/canonical workspace digests are integrity-covered telemetry. */
	X58_B06VL_PROFILE_BROAD = 7,
#endif
};

#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
static inline bool x58_b06vl_handoff_cpu_a0_candidate(uint32_t value)
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
#endif
#endif

/*
 * Vendor-free result copied out only after the automatic romstage path has
 * passed every build-specific exact-result gate.  B06VE deliberately leaves
 * its persistent reset-loop guard armed until post-memory validation finishes.
 */
struct x58_b06v6_raminit_result {
	uint32_t cpuid;
	uint32_t microcode_revision;
	/* Unmodified full-256-byte identity hash, never a normalized surrogate. */
	uint32_t spd_fnv;
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
	uint32_t spd_profile_fnv;
#endif
	uint32_t csi_state_fnv;
	uint32_t policy_fnv;
	uint32_t workspace_fnv;
#if CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
	uint32_t csi_state_canonical_fnv;
	uint32_t workspace_canonical_fnv;
#endif
	uint32_t minit_eax;
	uint32_t mc_mapper;
	uint32_t mc_common_f8;
	uint32_t ch2_dod;
	uint32_t ch2_ranks;
	uint32_t ch2_status;
	uint32_t qpi_status;
#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
	uint32_t post_minit_ioh_stage_9c;
#endif
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	uint32_t profile_id;
	uint32_t saved_pre_a0;
	uint32_t post_minit_cpu_a0;
	uint32_t post_minit_cpu_9c;
#endif
};

/* Persisted in CBMEM; all reserved words must remain zero. */
struct x58_b06v6_handoff {
	uint32_t magic;
	uint32_t version;
	uint32_t structure_size;
	uint32_t flags;
	struct x58_b06v6_raminit_result raminit;
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
void x58_b06v6_handoff_record_success(
	const struct x58_b06v6_raminit_result *result);

#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
/* Romstage implementation; called only after all DRAM and CBMEM readbacks. */
bool x58_b06ve_finalize_persistent_guard(void);
#endif
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
/* Exact O-family profiles reach post-memory with MINIT's return tuple present. */
bool x58_b06vi_rearm_o_guard_after_postmem(void);
#endif

uint32_t x58_b06v6_handoff_digest(const struct x58_b06v6_handoff *handoff);
bool x58_b06v6_raminit_result_is_exact(
	const struct x58_b06v6_raminit_result *result);
bool x58_b06v6_handoff_is_valid(const struct x58_b06v6_handoff *handoff);

#if CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
/* Shared producer/consumer truth table; raw and canonical digests stay paired. */
static inline bool x58_b06vh_workspace_digest_pair_exact(
	uint32_t raw_digest, uint32_t canonical_digest)
{
	return (raw_digest == X58_B06VF_EXPECTED_WORKSPACE_RAW_A_FNV &&
		canonical_digest == X58_B06VF_EXPECTED_WORKSPACE_CANONICAL_FNV) ||
		(raw_digest == X58_B06VF_EXPECTED_WORKSPACE_RAW_B_FNV &&
		 canonical_digest == X58_B06VF_EXPECTED_WORKSPACE_CANONICAL_FNV) ||
#if CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
		(raw_digest == X58_B06VH_EXPECTED_WORKSPACE_RAW_P_FNV &&
		 canonical_digest == X58_B06VH_EXPECTED_WORKSPACE_CANONICAL_P_FNV) ||
		(raw_digest == X58_B06VH_EXPECTED_WORKSPACE_RAW_N_FNV &&
		 canonical_digest == X58_B06VH_EXPECTED_WORKSPACE_CANONICAL_N_FNV);
#else
		false;
#endif
}
#endif

#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
/*
 * The sole B06VI admission truth table, shared verbatim by the romstage
 * producer and ramstage consumer.  Every row couples the profile ID, saved
 * and post-MINIT CPU endpoint, raw/canonical CSI pair, and raw/canonical
 * workspace pair.  B06VK's Q row deliberately treats only its raw workspace
 * digest as telemetry; the exact canonical digest and byte markers remain
 * coupled at the producer, and every other row retains its exact raw digest.
 * B06VL is a separate version-9 contract: it couples only the seven-value CPU/CSI
 * hard class here and treats both workspace digests as telemetry.  Its
 * completion, ABI, canary, I801, platform, and memory-test gates live in the
 * producer and are represented by a mandatory v9 evidence flag.
 * Do not replace this with independent allowlists.
 */
static inline enum x58_b06vi_profile_id x58_b06vi_profile_for_tuple(
	uint32_t saved_pre_a0, uint32_t post_minit_cpu_a0,
	uint32_t post_minit_cpu_9c, uint8_t csi_dynamic_2a6,
	uint32_t csi_raw_digest,
	uint32_t csi_canonical_digest, uint32_t workspace_raw_digest,
	uint32_t workspace_canonical_digest)
{
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	if (x58_b06vl_handoff_cpu_a0_candidate(saved_pre_a0) &&
	    post_minit_cpu_a0 == saved_pre_a0 &&
	    (post_minit_cpu_9c == 0x00a00502u ||
	     post_minit_cpu_9c == 0x00b00502u) &&
	    csi_canonical_digest == X58_B06VE_EXPECTED_CSI_CANONICAL_FNV &&
	    ((csi_dynamic_2a6 == 0x08u &&
	      csi_raw_digest == X58_B06VE_EXPECTED_CSI_FNV) ||
	     (csi_dynamic_2a6 == X58_B06VI_EXPECTED_CSI_DYNAMIC_O &&
	      csi_raw_digest == X58_B06VI_EXPECTED_CSI_RAW_O_FNV))) {
		(void)workspace_raw_digest;
		(void)workspace_canonical_digest;
		return X58_B06VL_PROFILE_BROAD;
	}
#endif

	if (saved_pre_a0 == 0x00017000u &&
	    post_minit_cpu_a0 == 0x00017000u &&
	    post_minit_cpu_9c == 0x00b00502u &&
	    csi_dynamic_2a6 == 0x08u &&
	    csi_raw_digest == X58_B06VE_EXPECTED_CSI_FNV &&
	    csi_canonical_digest == X58_B06VE_EXPECTED_CSI_CANONICAL_FNV &&
	    workspace_raw_digest == X58_B06VF_EXPECTED_WORKSPACE_RAW_A_FNV &&
	    workspace_canonical_digest ==
		X58_B06VF_EXPECTED_WORKSPACE_CANONICAL_FNV)
		return X58_B06VI_PROFILE_PRIMARY_A;

	if (saved_pre_a0 == 0x00017000u &&
	    post_minit_cpu_a0 == 0x00017000u &&
	    post_minit_cpu_9c == 0x00b00502u &&
	    csi_dynamic_2a6 == 0x08u &&
	    csi_raw_digest == X58_B06VE_EXPECTED_CSI_FNV &&
	    csi_canonical_digest == X58_B06VE_EXPECTED_CSI_CANONICAL_FNV &&
	    workspace_raw_digest == X58_B06VF_EXPECTED_WORKSPACE_RAW_B_FNV &&
	    workspace_canonical_digest ==
		X58_B06VF_EXPECTED_WORKSPACE_CANONICAL_FNV)
		return X58_B06VI_PROFILE_PRIMARY_B;

	if (saved_pre_a0 == 0x00017000u &&
	    post_minit_cpu_a0 == 0x00017000u &&
	    post_minit_cpu_9c == 0x00b00502u &&
	    csi_dynamic_2a6 == 0x08u &&
	    csi_raw_digest == X58_B06VE_EXPECTED_CSI_FNV &&
	    csi_canonical_digest == X58_B06VE_EXPECTED_CSI_CANONICAL_FNV &&
	    workspace_raw_digest == X58_B06VH_EXPECTED_WORKSPACE_RAW_P_FNV &&
	    workspace_canonical_digest ==
		X58_B06VH_EXPECTED_WORKSPACE_CANONICAL_P_FNV)
		return X58_B06VI_PROFILE_PRIMARY_P;

	if (saved_pre_a0 == 0x00017000u &&
	    post_minit_cpu_a0 == 0x00017000u &&
	    post_minit_cpu_9c == 0x00b00502u &&
	    csi_dynamic_2a6 == 0x08u &&
	    csi_raw_digest == X58_B06VE_EXPECTED_CSI_FNV &&
	    csi_canonical_digest == X58_B06VE_EXPECTED_CSI_CANONICAL_FNV &&
	    workspace_raw_digest == X58_B06VH_EXPECTED_WORKSPACE_RAW_N_FNV &&
	    workspace_canonical_digest ==
		X58_B06VH_EXPECTED_WORKSPACE_CANONICAL_N_FNV)
		return X58_B06VI_PROFILE_PRIMARY_N;

	if (saved_pre_a0 == 0x00017c00u &&
	    post_minit_cpu_a0 == 0x00017c00u &&
	    post_minit_cpu_9c == 0x00a00502u &&
	    csi_dynamic_2a6 == X58_B06VI_EXPECTED_CSI_DYNAMIC_O &&
	    csi_raw_digest == X58_B06VI_EXPECTED_CSI_RAW_O_FNV &&
	    csi_canonical_digest == X58_B06VE_EXPECTED_CSI_CANONICAL_FNV &&
	    workspace_raw_digest == X58_B06VI_EXPECTED_WORKSPACE_RAW_O_FNV &&
	    workspace_canonical_digest ==
		X58_B06VI_EXPECTED_WORKSPACE_CANONICAL_O_FNV)
		return X58_B06VI_PROFILE_O;

#if CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
	if (saved_pre_a0 == 0x00017c00u &&
	    post_minit_cpu_a0 == 0x00017c00u &&
	    post_minit_cpu_9c == 0x00a00502u &&
	    csi_dynamic_2a6 == X58_B06VI_EXPECTED_CSI_DYNAMIC_O &&
	    csi_raw_digest == X58_B06VI_EXPECTED_CSI_RAW_O_FNV &&
	    csi_canonical_digest == X58_B06VE_EXPECTED_CSI_CANONICAL_FNV &&
#if !CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
	    workspace_raw_digest == X58_B06VJ_EXPECTED_WORKSPACE_RAW_Q_FNV &&
#endif
	    workspace_canonical_digest ==
		X58_B06VJ_EXPECTED_WORKSPACE_CANONICAL_Q_FNV)
		return X58_B06VJ_PROFILE_Q;
#endif

	return X58_B06VI_PROFILE_INVALID;
}

static inline bool x58_b06vi_profile_requires_deferred_rearm(
	uint32_t profile_id)
{
	return profile_id == X58_B06VI_PROFILE_O
#if CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
		|| profile_id == X58_B06VJ_PROFILE_Q
#endif
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
		|| profile_id == X58_B06VL_PROFILE_BROAD
#endif
		;
}

static inline bool x58_b06vi_profile_tuple_exact(
	uint32_t profile_id, uint32_t saved_pre_a0,
	uint32_t post_minit_cpu_a0, uint32_t post_minit_cpu_9c,
	uint32_t csi_raw_digest, uint32_t csi_canonical_digest,
	uint32_t workspace_raw_digest, uint32_t workspace_canonical_digest)
{
	uint8_t csi_dynamic_2a6;
	enum x58_b06vi_profile_id selected;

	if (profile_id == X58_B06VI_PROFILE_O
#if CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
	    || profile_id == X58_B06VJ_PROFILE_Q
#endif
	   )
		csi_dynamic_2a6 = X58_B06VI_EXPECTED_CSI_DYNAMIC_O;
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	else if (profile_id == X58_B06VL_PROFILE_BROAD &&
		 csi_raw_digest == X58_B06VE_EXPECTED_CSI_FNV)
		csi_dynamic_2a6 = 0x08;
	else if (profile_id == X58_B06VL_PROFILE_BROAD &&
		 csi_raw_digest == X58_B06VI_EXPECTED_CSI_RAW_O_FNV)
		csi_dynamic_2a6 = X58_B06VI_EXPECTED_CSI_DYNAMIC_O;
#endif
	else if (profile_id == X58_B06VI_PROFILE_PRIMARY_A ||
		 profile_id == X58_B06VI_PROFILE_PRIMARY_B ||
		 profile_id == X58_B06VI_PROFILE_PRIMARY_P ||
		 profile_id == X58_B06VI_PROFILE_PRIMARY_N)
		csi_dynamic_2a6 = 0x08;
	else
		return false;

	selected = x58_b06vi_profile_for_tuple(
		saved_pre_a0, post_minit_cpu_a0, post_minit_cpu_9c,
		csi_dynamic_2a6,
		csi_raw_digest, csi_canonical_digest, workspace_raw_digest,
		workspace_canonical_digest);

	return selected != X58_B06VI_PROFILE_INVALID && profile_id == selected;
}

static inline bool x58_b06vi_profile_is_primary(uint32_t profile_id)
{
	return profile_id == X58_B06VI_PROFILE_PRIMARY_A ||
		profile_id == X58_B06VI_PROFILE_PRIMARY_B ||
		profile_id == X58_B06VI_PROFILE_PRIMARY_P ||
		profile_id == X58_B06VI_PROFILE_PRIMARY_N;
}
#endif

#endif /* MAINBOARD_MSI_X58_PRO_E_B06V6_HANDOFF_H */
