/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_VENDOR_INIT_H
#define MAINBOARD_MSI_X58_PRO_E_VENDOR_INIT_H

#include <types.h>

/*
 * This interface is research scaffolding for locally supplied MSI code.  It
 * remains behind USE_BLOBS and the board's explicit private-input option.  No
 * proprietary bytes are part of this source file.
 */
#define X58_VENDOR_CSI_STATE_SIZE	0x304
#define X58_VENDOR_MINIT_POLICY_SIZE	0x0e0
#define X58_VENDOR_MINIT_WORKSPACE_SIZE	0x2bcc

/* Deliberately verbose token required in addition to a ROMMON one-shot lock. */
#define X58_VENDOR_WRITE_CONFIRMATION	0x56454e44u /* "VEND" */

/*
 * Seven literal pass-three CSI candidates.  The field meaning remains
 * unknown.  Stability and equality before/after CSI are separate mandatory
 * gates; this helper must not be used as a memory-success claim.
 */
static inline bool x58_vendor_memory_profile_high_qpi_cpu_a0_candidate(uint32_t value)
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

/*
 * Six literal pre-pass-three observations in X58_CSI_PROFILE--X58_PROFILE_CLASS, extended by the
 * separately observed 0x00017200 only in X58_MEMORY_PROFILE.  Their field meaning is
 * unknown; equality against the applicable finite set is the complete policy.
 */
static inline bool x58_vendor_csi_profile_high_qpi_cpu_a0_exact(uint32_t value)
{
	return x58_vendor_memory_profile_high_qpi_cpu_a0_candidate(value);
}

/*
 * A0 is opaque telemetry, not a documented readiness bit.  The normal image
 * therefore admits any value only after two identical reads; the callers
 * still require the independently checked QPI link state, reset phase,
 * wrapper ABI and every post-MINIT integrity predicate.  Historical images
 * retain their exact-value policy.
 */
static inline bool x58_vendor_normal_stable_pre_minit_a0(uint32_t value)
{
	(void)value;
	return true;
}

/* No mask/range or post-CSI promotion: one manual CSI-only path. */
static inline bool x58_vendor_pre_a0_16e00_candidate(uint32_t value)
{
	(void)value;
	return false;
}


/*
 * Neutral read-only observation: five exact pre-pass-three values have been
 * measured while every other High-QPI endpoint predicate matched.  Do not
 * turn this finite set into a mask or range; the field semantics are unknown.
 */
static inline bool x58_vendor_csi_canonical_high_qpi_cpu_a0_exact(uint32_t value)
{
	return value == 0x00017000u || value == 0x00017600u ||
		value == 0x00017800u || value == 0x00017a00u ||
		value == 0x00017c00u;
}

/*
 * Neutral read-only observation: four exact pre-pass-three values have been
 * measured while every other High-QPI endpoint predicate matched.  Do not
 * turn this finite set into a mask or range; the field semantics are unknown.
 */
static inline bool x58_vendor_high_qpi_high_qpi_cpu_a0_exact(uint32_t value)
{
	return value == 0x00017000u || value == 0x00017600u ||
		value == 0x00017800u || value == 0x00017a00u;
}

/*
 * Neutral read-only observation: the field semantics at CPU ff:02.1 +0xa0
 * remain unknown.  X58_RTC_POLICY preserves its original single-value contract;
 * X58_VENDOR_TRACE adds only the second exact value measured both after its controlled
 * SYRE transition and on the vendor-booted ratio-6 High-QPI reference.
 */
static inline bool x58_vendor_high_qpi_cpu_a0_exact(uint32_t value)
{
	return value == 0x00017000u || value == 0x00017600u;
}

enum x58_vendor_status {
	X58_VENDOR_OK = 0,
	X58_VENDOR_ERR_ARGUMENT,
	X58_VENDOR_ERR_NOT_PREPARED,
	X58_VENDOR_ERR_WRONG_STAGE,
	X58_VENDOR_ERR_CAR_LAYOUT,
	X58_VENDOR_ERR_CAR_TOO_SMALL,
	X58_VENDOR_ERR_CANARY,
	X58_VENDOR_ERR_CPU_VENDOR,
	X58_VENDOR_ERR_CPU_SIGNATURE,
	X58_VENDOR_ERR_MICROCODE_REVISION,
	X58_VENDOR_ERR_NOT_BSP,
	X58_VENDOR_ERR_CPU_MODE,
	X58_VENDOR_ERR_PCIEXBAR_STATE,
	X58_VENDOR_ERR_PLATFORM_STATE,
	X58_VENDOR_ERR_WRAPPER_SIGNATURE,
	X58_VENDOR_ERR_CSI_SIGNATURE,
	X58_VENDOR_ERR_MINIT_SIGNATURE,
	X58_VENDOR_ERR_POLICY_INCOMPLETE,
	X58_VENDOR_ERR_POLICY_CHANGED,
	X58_VENDOR_ERR_CONFIRMATION,
	X58_VENDOR_ERR_NOT_ARMED,
	X58_VENDOR_ERR_ORDER,
	X58_VENDOR_ERR_BUSY,
	X58_VENDOR_ERR_CSI_RESULT_MISMATCH,
	X58_VENDOR_ERR_STACK_IMBALANCE,
	X58_VENDOR_ERR_CSI_STATE_POINTER,
};

struct x58_vendor_call_result {
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t edi;
	uint32_t eflags;
	uintptr_t vendor_esp_after_return;
};

struct x58_vendor_runtime_info {
	uintptr_t car_scratch_begin;
	uintptr_t car_scratch_end;
	uintptr_t vendor_stack_low;
	uintptr_t vendor_stack_top;
	size_t vendor_stack_size;
	size_t vendor_stack_high_water;
	uint32_t cpuid_1_eax;
	uint32_t microcode_revision;
	uint32_t apic_base_low;
	uint32_t apic_base_high;
	uint32_t cr0;
	uint32_t cr4;
	uint32_t eflags_before_call;
	uint16_t cs_selector;
	uint16_t ds_selector;
	uint16_t es_selector;
	uint16_t ss_selector;
	uint32_t uncore_sad_id;
	uint32_t pciexbar_low;
	uint32_t pciexbar_high;
	uint32_t x58_hostbridge_id;
	uint32_t x58_hostbridge_class_revision;
	uint32_t qpi_phy_observed_80;
	uint32_t memory_clock_observed_50;
	uint32_t memory_clock_observed_54;
	uint32_t csi_state_digest;
	uint32_t minit_policy_digest;
	uint32_t minit_policy_current_digest;
	uint32_t minit_workspace_digest;
	bool prepared;
	bool canaries_valid;
	bool wrapper_signature_valid;
	bool csi_signature_valid;
	bool minit_signature_valid;
	bool csi_wrapper_armed;
	bool csi_call_attempted;
	bool csi_returned;
	bool csi_result_accepted;
	bool minit_policy_confirmed;
	bool minit_armed;
	bool minit_call_attempted;
	bool minit_returned;
	bool x58_init_phase_high_qpi_csi_profile;
	bool x58_minit_high_qpi_minit_authorized;
	uint32_t x58_csi_profile_pre_csi_cpu_a0;
	bool x58_csi_profile_pre_csi_cpu_a0_valid;
	uint8_t vendor_status_seed;
	struct x58_vendor_call_result last_call;
};

/* Set up guarded objects and a dedicated stack in unused CAR. */
enum x58_vendor_status x58_vendor_runtime_prepare(void);

/* Read-only hardware/blob/CAR assessment.  This never calls vendor code. */
enum x58_vendor_status x58_vendor_runtime_probe(
	struct x58_vendor_runtime_info *info);
/* X58_MEMORY_RESULT-only exact assessment after the one permitted High-QPI MINIT return. */
enum x58_vendor_status x58_vendor_memory_result_post_minit_probe(
	struct x58_vendor_runtime_info *info);

enum x58_vendor_status x58_vendor_runtime_check_canaries(void);
const char *x58_vendor_status_name(enum x58_vendor_status status);

/* Full runtime/blob/platform preflight before ROMMON may program PCIEXBAR. */
enum x58_vendor_status x58_vendor_preflight_pciexbar(void);

/*
 * Select the exact hardware-observed High-QPI tuple for one X58_INIT_PHASE CSI probe.
 * This does not arm or call CSI and never authorizes MINIT.
 */
enum x58_vendor_status x58_vendor_init_phase_select_high_qpi_csi(
	uint32_t saved_pre_a0,
	uint32_t confirmation);

/*
 * Promote only the exact X58_VENDOR_TRACE pass-three observation to authorization for
 * one MINIT call.  This is not a semantic CSI-success declaration.  The
 * helper rechecks the complete state digest, bounded state bytes, final QPI
 * endpoint and neutral stage fingerprints before relaxing the otherwise
 * unconditional High-QPI MINIT exclusion.
 */
enum x58_vendor_status x58_vendor_minit_authorize_high_qpi_minit(
	uint32_t confirmation);

/*
 * Preferred CSI path: the original AMI wrapper at 0xfffc04e2 constructs the
 * complete state and calls CSI at 0xfffe7000.  The status seed remains an
 * controlled input; even zero must be explicitly supplied and armed.  CSI
 * contains a HLT/self-loop path which cannot be timed out by code on this CPU;
 * a no-return result requires an external cold reset or flash recovery.
 * The wrapper also leaves the CMOS-index NMI-disable bit set (port 0x70 =
 * 0x8e).  This runtime deliberately neither restores nor changes that state.
 */
enum x58_vendor_status x58_vendor_arm_csi_wrapper(
	uint8_t vendor_status_seed, uint32_t confirmation);
enum x58_vendor_status x58_vendor_call_csi_wrapper(
	struct x58_vendor_call_result *result);

/*
 * Accepting a returned CSI result is separate and exact.  The caller must
 * echo the observed register tuple and complete-state digest; this verifies
 * operator intent but does not assign a success meaning to either value.
 */
enum x58_vendor_status x58_vendor_accept_csi_result(uint32_t expected_eax,
	uint32_t expected_ebx, uint32_t expected_ecx,
	uint32_t expected_state_digest, uint32_t confirmation);

/*
 * Direct CSI is retained only to document the reconstructed two-argument ABI.
 * Its zero-only state initializer is incomplete, so this function currently
 * always fails closed before the entry point can execute.
 */
enum x58_vendor_status x58_vendor_call_csi_direct(
	struct x58_vendor_call_result *result);

/*
 * Install all 0xe0 policy bytes only after a caller has independently
 * validated them.  Installation does not call MINIT and does not imply that
 * the policy is correct for any other CPU, DIMM, or boot state.
 */
enum x58_vendor_status x58_vendor_install_confirmed_minit_policy(
	const void *policy, size_t size, uint32_t expected_digest,
	uint32_t confirmation);
/* Clear any installed/armed policy before changing the ROMMON-side copy. */
enum x58_vendor_status x58_vendor_invalidate_minit_policy(void);
enum x58_vendor_status x58_vendor_arm_minit(uint32_t confirmation);
enum x58_vendor_status x58_vendor_call_minit(
	struct x58_vendor_call_result *result);

const void *x58_vendor_csi_state_snapshot(void);
const void *x58_vendor_minit_policy(void);
const void *x58_vendor_minit_workspace(void);

#endif /* MAINBOARD_MSI_X58_PRO_E_VENDOR_INIT_H */
