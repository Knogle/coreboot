/* SPDX-License-Identifier: GPL-2.0-only */

#include <arch/cpuid.h>
#include <arch/cpu.h>
#include <arch/pci_io_cfg.h>
#include <arch/symbols.h>
#include <commonlib/bsd/helpers.h>
#include <cpu/x86/cr.h>
#include <cpu/x86/gdt.h>
#include <cpu/x86/msr.h>
#include <device/pci_type.h>
#include <types.h>

#include "vendor_init.h"
#include "vendor_init_internal.h"

#define X58_VENDOR_RUNTIME_MAGIC	0x58385630u /* "X8V0" */
#define X58_VENDOR_CANARY		0x58ca4a5au
#define X58_VENDOR_STACK_POISON		0xa5
#define X58_VENDOR_GUARD_SIZE		16
#define X58_VENDOR_GUARD_COUNT		5
#define X58_VENDOR_MIN_STACK_SIZE	0x6000

#define X58_VENDOR_CSI_WRAPPER_ENTRY	0xfffc04e2u
#define X58_VENDOR_CSI_WRAPPER_SIZE	0x837u
#if CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
#define X58_VENDOR_CSI_WRAPPER_FNV1A	0x98e2f3deu
#elif CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
#define X58_VENDOR_CSI_WRAPPER_FNV1A	0x986153c5u
#else
#define X58_VENDOR_CSI_WRAPPER_FNV1A	0x65b20e50u
#endif
#define X58_VENDOR_CSI_HELPER_ENTRY	0xfffc1554u
#define X58_VENDOR_CSI_HELPER_SIZE	0x25u
#define X58_VENDOR_CSI_HELPER_FNV1A	0x99aeb979u
#define X58_VENDOR_CSI_CMOS_HELPER	0xfffc1566u
#define X58_VENDOR_CSI_ENTRY		0xfffe7000u
#define X58_VENDOR_CSI_IMAGE_BASE	0xfffe6de0u
#define X58_VENDOR_CSI_IMAGE_SIZE	0x75e0u
#define X58_VENDOR_CSI_IMAGE_FNV1A	0x77990db9u
/* DOS e_lfanew from the hash-pinned raw/XIP image; checked by the extractor. */
#define X58_VENDOR_CSI_PE_OFFSET	0xb0u
#define X58_VENDOR_MINIT_ENTRY		0xfffc2000u
#define X58_VENDOR_MINIT_IMAGE_BASE	0xfffc1dc0u
#define X58_VENDOR_MINIT_IMAGE_SIZE	0x18700u
#define X58_VENDOR_MINIT_IMAGE_FNV1A	0x6465d8f5u

#define X58_VENDOR_CSI_WRAPPER_STATE_FROM_TOP	0x318

#define X58_VENDOR_CPUID_E5645		0x000206c2u
#define X58_VENDOR_MICROCODE_E5645	0x0000001fu
#define IA32_APIC_BASE_MSR		0x1b
#define IA32_BIOS_SIGN_ID_MSR		0x8b
#define IA32_APIC_BASE_BSP		BIT(8)
#define IA32_APIC_BASE_ENABLE		BIT(11)
#define IA32_APIC_BASE_ADDRESS_MASK	0xfffff000u
#define IA32_APIC_DEFAULT_ADDRESS	0xfee00000u

/* Exact, measured single-socket E5645 lab state required before CSI. */
#define X58_VENDOR_SAD_DEV		PCI_DEV(0xff, 0, 1)
#define X58_VENDOR_QPI_PHY_DEV		PCI_DEV(0xff, 2, 1)
#define X58_VENDOR_MEMORY_CLOCK_DEV	PCI_DEV(0xff, 3, 4)
#define X58_VENDOR_SAD_ID		0x2d818086u
#define X58_VENDOR_SAD_PCIEXBAR_LO	0x50
#define X58_VENDOR_SAD_PCIEXBAR_HI	0x54
#define X58_VENDOR_PCIEXBAR_LO		0xe0000001u
#define X58_VENDOR_X58_ID		0x34058086u
#define X58_VENDOR_QPI_SLOW_STATE	0x030f0f03u
#define X58_VENDOR_QPI_HIGH_STATE	0x070f0f03u
#define X58_VENDOR_MEMORY_CLOCK_STATE	0x0a000006u
#define X58_VENDOR_MEMORY_RATIO_STATE	0x00000006u

#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
#define X58_VENDOR_QPI_LINK_DEV		PCI_DEV(0xff, 2, 0)
#define X58_VENDOR_IOH_QPI_LINK_DEV	PCI_DEV(0, 0x10, 0)
#define X58_VENDOR_IOH_SYRE_DEV		PCI_DEV(0, 0x14, 2)
#define X58_VENDOR_IOH_QPI0_ECAM	0xe0068000u
#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
#define X58_VENDOR_IOH_STAGE_DEV		PCI_DEV(0, 0x14, 1)
#define X58_VENDOR_B06VB_CSI_FNV1A	0x8b38506au
#if CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
#define X58_VENDOR_B06VD_CSI_DYNAMIC_OFFSET	0x02a6u
#define X58_VENDOR_B06VD_CSI_RAW_08_FNV1A	0x03e3d24eu
#define X58_VENDOR_B06VD_CSI_RAW_0C_FNV1A	0x8b38506au
#define X58_VENDOR_B06VD_CSI_CANONICAL_FNV1A	0x908dabb6u
_Static_assert(X58_VENDOR_B06VD_CSI_DYNAMIC_OFFSET < X58_VENDOR_CSI_STATE_SIZE,
	"B06VD CSI canonical offset is outside the state buffer");
#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
#define X58_VENDOR_B06VE_POLICY_FNV1A		0x3c0f3a0bu
#define X58_VENDOR_B06VE_WORKSPACE_FNV1A	0x94299f43u
#define X58_VENDOR_B06VE_WORK_B3_FLAGS_OFFSET	0x004fu
#define X58_VENDOR_B06VE_WORK_COMPLETE_OFFSET	0x0e79u
#define X58_VENDOR_B06VE_POLICY_STATUS_OFFSET	0x000au
#define X58_VENDOR_B06VE_MC_COMMON_DEV		PCI_DEV(0xff, 3, 0)
#define X58_VENDOR_B06VE_UNCORE_COMMON_DEV	PCI_DEV(0xff, 3, 4)
#define X58_VENDOR_B06VE_CHANNEL2_DEV		PCI_DEV(0xff, 6, 0)
#define X58_VENDOR_B06VE_CHANNEL2_ADDR_DEV	PCI_DEV(0xff, 6, 1)
#endif
#endif
#endif
#endif

struct x58_vendor_csi_state {
	uint8_t raw[X58_VENDOR_CSI_STATE_SIZE];
};

struct x58_vendor_minit_policy {
	uint8_t raw[X58_VENDOR_MINIT_POLICY_SIZE];
};

struct x58_vendor_minit_workspace {
	uint8_t raw[X58_VENDOR_MINIT_WORKSPACE_SIZE];
};

struct x58_vendor_runtime {
	uint32_t magic;
	struct x58_vendor_csi_state *csi_state;
	struct x58_vendor_minit_policy *minit_policy;
	struct x58_vendor_minit_workspace *minit_workspace;
	uint32_t *guards[X58_VENDOR_GUARD_COUNT];
	uintptr_t scratch_begin;
	uintptr_t scratch_end;
	uintptr_t stack_low;
	uintptr_t stack_top;
	uint32_t minit_policy_digest;
	uint32_t csi_state_digest;
	uint8_t experimental_status_seed;
	bool busy;
	bool csi_wrapper_armed;
	bool csi_call_attempted;
	bool csi_returned;
	bool csi_result_accepted;
	bool minit_policy_confirmed;
	bool minit_armed;
	bool minit_call_attempted;
	bool minit_returned;
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
	bool b06v8_high_qpi_csi_profile;
#endif
#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
	bool b06vb_high_qpi_minit_authorized;
#endif
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	uint32_t b06vg_pre_csi_cpu_a0;
	bool b06vg_pre_csi_cpu_a0_valid;
#endif
	struct x58_vendor_call_result last_call;
};

_Static_assert(sizeof(struct x58_vendor_csi_state) == X58_VENDOR_CSI_STATE_SIZE,
	"CSI state size changed");
_Static_assert(sizeof(struct x58_vendor_minit_policy) == X58_VENDOR_MINIT_POLICY_SIZE,
	"MINIT policy size changed");
_Static_assert(sizeof(struct x58_vendor_minit_workspace) ==
	X58_VENDOR_MINIT_WORKSPACE_SIZE, "MINIT workspace size changed");
_Static_assert(sizeof(uintptr_t) == sizeof(uint32_t),
	"B06V0 vendor trampoline requires 32-bit romstage");

check_member(x58_vendor_call_state, entry, VENDOR_CALL_ENTRY);
check_member(x58_vendor_call_state, arg0, VENDOR_CALL_ARG0);
check_member(x58_vendor_call_state, arg1, VENDOR_CALL_ARG1);
check_member(x58_vendor_call_state, arg2, VENDOR_CALL_ARG2);
check_member(x58_vendor_call_state, argc, VENDOR_CALL_ARGC);
check_member(x58_vendor_call_state, stack_top, VENDOR_CALL_STACK_TOP);
check_member(x58_vendor_call_state, caller_esp, VENDOR_CALL_CALLER_ESP);
check_member(x58_vendor_call_state, caller_eflags, VENDOR_CALL_CALLER_EFLAGS);
check_member(x58_vendor_call_state, vendor_esp, VENDOR_CALL_VENDOR_ESP);
check_member(x58_vendor_call_state, eax, VENDOR_CALL_EAX);
check_member(x58_vendor_call_state, ebx, VENDOR_CALL_EBX);
check_member(x58_vendor_call_state, ecx, VENDOR_CALL_ECX);
check_member(x58_vendor_call_state, edx, VENDOR_CALL_EDX);
check_member(x58_vendor_call_state, eflags, VENDOR_CALL_EFLAGS);
check_member(x58_vendor_call_state, edi, VENDOR_CALL_EDI);
_Static_assert(sizeof(struct x58_vendor_call_state) == VENDOR_CALL_STATE_SIZE,
	"vendor trampoline call-state size changed");

struct x58_vendor_call_state x58_vendor_call_state;
static struct x58_vendor_runtime runtime;

static uintptr_t align_up(uintptr_t value, uintptr_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

static uintptr_t align_down(uintptr_t value, uintptr_t alignment)
{
	return value & ~(alignment - 1);
}

static void bytes_set(void *destination, uint8_t value, size_t size)
{
	uint8_t *out = destination;

	for (size_t i = 0; i < size; i++)
		out[i] = value;
}

static void bytes_copy(void *destination, const void *source, size_t size)
{
	uint8_t *out = destination;
	const uint8_t *in = source;

	for (size_t i = 0; i < size; i++)
		out[i] = in[i];
}

static uint32_t bytes_digest(const void *buffer, size_t size)
{
	const uint8_t *bytes = buffer;
	uint32_t digest = 2166136261u;

	for (size_t i = 0; i < size; i++) {
		digest ^= bytes[i];
		digest *= 16777619u;
	}

	return digest;
}

#if CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
static uint32_t b06vd_csi_canonical_digest(const uint8_t *state)
{
	uint32_t digest = 2166136261u;

	/* Canonicalize only the digest stream; never modify the raw CSI state. */
	for (size_t offset = 0; offset < X58_VENDOR_CSI_STATE_SIZE; offset++) {
		const uint8_t value = offset == X58_VENDOR_B06VD_CSI_DYNAMIC_OFFSET ?
			0 : state[offset];

		digest ^= value;
		digest *= 16777619u;
	}

	return digest;
}

static bool b06vd_csi_digest_pair_exact(const uint8_t *state,
	uint32_t raw_digest, uint32_t canonical_digest)
{
	if (state == NULL ||
	    canonical_digest != X58_VENDOR_B06VD_CSI_CANONICAL_FNV1A)
		return false;

	return (state[X58_VENDOR_B06VD_CSI_DYNAMIC_OFFSET] == 0x08 &&
			raw_digest == X58_VENDOR_B06VD_CSI_RAW_08_FNV1A) ||
		(state[X58_VENDOR_B06VD_CSI_DYNAMIC_OFFSET] == 0x0c &&
			raw_digest == X58_VENDOR_B06VD_CSI_RAW_0C_FNV1A);
}
#endif

static bool signature_digest_valid(uintptr_t address, size_t size,
	uint32_t expected_fnv1a)
{
	const volatile uint8_t *actual = (const volatile uint8_t *)address;
	uint32_t digest = 2166136261u;

	for (size_t i = 0; i < size; i++) {
		digest ^= actual[i];
		digest *= 16777619u;
	}

	return digest == expected_fnv1a;
}

static uint16_t fixed_read16(uintptr_t address)
{
	const volatile uint8_t *p = (const volatile uint8_t *)address;

	return p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t fixed_read32(uintptr_t address)
{
	const volatile uint8_t *p = (const volatile uint8_t *)address;

	return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
		((uint32_t)p[3] << 24);
}

#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
static bool b06v8_high_qpi_platform_exact(void)
{
	const uint32_t pciexbar_low = pci_io_read_config32(X58_VENDOR_SAD_DEV,
		X58_VENDOR_SAD_PCIEXBAR_LO);
	const uint32_t pciexbar_high = pci_io_read_config32(X58_VENDOR_SAD_DEV,
		X58_VENDOR_SAD_PCIEXBAR_HI);
	uint32_t cpu_link_50;

	/* Never dereference the IOH ECAM window before its decode is exact. */
	if (pci_io_read_config32(X58_VENDOR_SAD_DEV, 0) != X58_VENDOR_SAD_ID ||
	    pciexbar_low != X58_VENDOR_PCIEXBAR_LO || pciexbar_high != 0)
		return false;
	if (fixed_read32(0xe0000000u) != X58_VENDOR_X58_ID ||
	    fixed_read32(0xe0000008u) != 0x06000013u)
		return false;
	cpu_link_50 = pci_io_read_config32(X58_VENDOR_QPI_LINK_DEV, 0x50);

	return pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x50) ==
			0x160c0112u &&
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x54) ==
			0x00000012u &&
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x6c) ==
			0x0040a0a0u &&
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x80) ==
			X58_VENDOR_QPI_HIGH_STATE &&
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x94) ==
			0x00010202u &&
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x9c) ==
			0x00000502u &&
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
		x58_vendor_b06vg_high_qpi_cpu_a0_exact(
			pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0xa0)) &&
#else
#if CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
		x58_vendor_b06vd_high_qpi_cpu_a0_exact(
			pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0xa0)) &&
#else
#if CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT
		x58_vendor_b06vc_high_qpi_cpu_a0_exact(
			pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0xa0)) &&
#else
#if CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET
		x58_vendor_high_qpi_cpu_a0_exact(
			pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0xa0)) &&
#else
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0xa0) ==
			0x00017600u &&
#endif
#endif
#endif
#endif
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0xa4) ==
			0x00322808u &&
		(cpu_link_50 == 0x86000000u ||
		 cpu_link_50 == 0x96000000u) &&
		pci_io_read_config32(X58_VENDOR_QPI_LINK_DEV, 0x58) ==
			0x00064555u &&
		fixed_read32(X58_VENDOR_IOH_QPI0_ECAM + 0x82c) ==
			0x004060a0u &&
		fixed_read32(X58_VENDOR_IOH_QPI0_ECAM + 0x840) ==
			0x070f0f03u &&
		fixed_read32(X58_VENDOR_IOH_QPI0_ECAM + 0x854) ==
			0x00010102u &&
		fixed_read32(X58_VENDOR_IOH_QPI0_ECAM + 0x85c) ==
			0x00000002u &&
		fixed_read32(X58_VENDOR_IOH_QPI0_ECAM + 0x864) ==
			0x00322808u &&
		pci_io_read_config32(X58_VENDOR_IOH_QPI_LINK_DEV, 0xc8) ==
			0x0606fc00u &&
		pci_io_read_config32(X58_VENDOR_IOH_SYRE_DEV, 0xcc) ==
			0x00000600u;
}

#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
static bool b06vb_high_qpi_post_csi_platform_exact(void)
{
	const uint8_t *state;
#if CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
	uint32_t raw_digest;
	uint32_t canonical_digest;
#endif
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	uint32_t post_cpu_9c;
	uint32_t post_cpu_a0;
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	bool broad_profile;
#else
	bool primary_profile;
	bool observation_profile;
#endif
#endif

	if (runtime.csi_state == NULL || !runtime.csi_returned)
		return false;
	state = runtime.csi_state->raw;

	/* Exact B06VA pass-three observation; field meanings remain unknown. */
#if CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
	raw_digest = bytes_digest(state, X58_VENDOR_CSI_STATE_SIZE);
	canonical_digest = b06vd_csi_canonical_digest(state);

	if (runtime.last_call.eax != 0 || runtime.last_call.ebx != 0 ||
	    runtime.last_call.ecx != 0x11 ||
	    runtime.last_call.edx != runtime.last_call.edi ||
	    runtime.csi_state_digest != raw_digest ||
	    !b06vd_csi_digest_pair_exact(state, raw_digest, canonical_digest))
		return false;
#else
	if (runtime.last_call.eax != 0 || runtime.last_call.ebx != 0 ||
	    runtime.last_call.ecx != 0x11 ||
	    runtime.last_call.edx != runtime.last_call.edi ||
	    runtime.csi_state_digest != X58_VENDOR_B06VB_CSI_FNV1A ||
	    bytes_digest(state, X58_VENDOR_CSI_STATE_SIZE) !=
		X58_VENDOR_B06VB_CSI_FNV1A)
		return false;
#endif
#if CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
	if (state[0x06] != 0x01 || state[0x07] != 0x01 ||
	    (state[0x2a6] != 0x08 && state[0x2a6] != 0x0c) ||
	    state[0x2ef] != 0x00 ||
	    state[0x301] != 0x00 ||
	    state[0x1c] != 0x00 || state[0x1d] != 0x01 ||
	    state[0x70] != 0x00 || state[0x71] != 0x01 ||
	    state[0xcd] != 0x00 || state[0xce] != 0x01 ||
	    state[0x121] != 0x00 || state[0x122] != 0x01)
		return false;
#else
	if (state[0x06] != 0x01 || state[0x07] != 0x01 ||
	    state[0x2a6] != 0x0c || state[0x2ef] != 0x00 ||
	    state[0x301] != 0x00 ||
	    state[0x1c] != 0x00 || state[0x1d] != 0x01 ||
	    state[0x70] != 0x00 || state[0x71] != 0x01 ||
	    state[0xcd] != 0x00 || state[0xce] != 0x01 ||
	    state[0x121] != 0x00 || state[0x122] != 0x01)
		return false;
#endif
	if (state[0x030] != 0x00 || state[0x031] != 0x00 ||
	    state[0x1db] != 0x01 || state[0x206] != 0x12 ||
	    state[0x275] != 0x01 || state[0x285] != 0x0e ||
	    state[0x2ed] != 0x01 || state[0x2ef] != 0x00 ||
	    state[0x2f8] != 0x11 ||
	    state[0x2f9] != 0x00 || state[0x2fc] != 0x00 ||
	    state[0x302] != 0x01)
		return false;

#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	post_cpu_9c = pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x9c);
	post_cpu_a0 = pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0xa0);
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	/*
	 * B06VL accepts the bounded CPU/CSI hard class, not a workspace profile.
	 * CSI byte/raw pairing and the complete marker tuple were checked above.
	 */
	broad_profile = runtime.b06vg_pre_csi_cpu_a0_valid &&
		x58_vendor_b06vl_high_qpi_cpu_a0_candidate(
			runtime.b06vg_pre_csi_cpu_a0) &&
		post_cpu_a0 == runtime.b06vg_pre_csi_cpu_a0 &&
		(post_cpu_9c == 0x00a00502u || post_cpu_9c == 0x00b00502u) &&
		((state[X58_VENDOR_B06VD_CSI_DYNAMIC_OFFSET] == 0x08 &&
		  raw_digest == X58_VENDOR_B06VD_CSI_RAW_08_FNV1A) ||
		 (state[X58_VENDOR_B06VD_CSI_DYNAMIC_OFFSET] == 0x0c &&
		  raw_digest == X58_VENDOR_B06VD_CSI_RAW_0C_FNV1A));
	if (!broad_profile)
		return false;
#else
	primary_profile = runtime.b06vg_pre_csi_cpu_a0_valid &&
		runtime.b06vg_pre_csi_cpu_a0 == 0x00017000u &&
		post_cpu_a0 == runtime.b06vg_pre_csi_cpu_a0 &&
		post_cpu_9c == 0x00b00502u &&
		state[X58_VENDOR_B06VD_CSI_DYNAMIC_OFFSET] == 0x08 &&
		raw_digest == X58_VENDOR_B06VD_CSI_RAW_08_FNV1A;
	observation_profile = runtime.b06vg_pre_csi_cpu_a0_valid &&
		x58_vendor_b06vg_high_qpi_cpu_a0_exact(
			runtime.b06vg_pre_csi_cpu_a0) &&
		runtime.b06vg_pre_csi_cpu_a0 != 0x00017000u &&
		post_cpu_a0 == runtime.b06vg_pre_csi_cpu_a0 &&
		post_cpu_9c == 0x00a00502u &&
		((state[X58_VENDOR_B06VD_CSI_DYNAMIC_OFFSET] == 0x08 &&
		  raw_digest == X58_VENDOR_B06VD_CSI_RAW_08_FNV1A) ||
		 (state[X58_VENDOR_B06VD_CSI_DYNAMIC_OFFSET] == 0x0c &&
		  raw_digest == X58_VENDOR_B06VD_CSI_RAW_0C_FNV1A));
	if (!primary_profile && !observation_profile)
		return false;
#endif
#endif

	/*
	 * Full final endpoint plus the stable IOH stage observation.  CPU-link
	 * offsets 0x80 and 0xd0 are reported by romstage but deliberately omitted
	 * here: one contains a layout-dependent CSI-local pointer residue, while
	 * the other has differed between vendor observations.
	 */
	return pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x50) ==
			0x160c0112u &&
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x54) ==
			0x00000012u &&
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x6c) ==
			0x0040a0a8u &&
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x80) ==
			X58_VENDOR_QPI_HIGH_STATE &&
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x94) ==
			0x00010202u &&
#if !CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x9c) ==
			0x00b00502u &&
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0xa0) ==
			0x00017000u &&
#endif
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0xa4) ==
			0x00322808u &&
		pci_io_read_config32(X58_VENDOR_QPI_LINK_DEV, 0x50) ==
			0x86000000u &&
		pci_io_read_config32(X58_VENDOR_QPI_LINK_DEV, 0x58) ==
			0x00064555u &&
		pci_io_read_config32(X58_VENDOR_MEMORY_CLOCK_DEV, 0x50) ==
			X58_VENDOR_MEMORY_CLOCK_STATE &&
		pci_io_read_config32(X58_VENDOR_MEMORY_CLOCK_DEV, 0x54) ==
			X58_VENDOR_MEMORY_RATIO_STATE &&
		fixed_read32(X58_VENDOR_IOH_QPI0_ECAM + 0x82c) ==
			0x004060a0u &&
		fixed_read32(X58_VENDOR_IOH_QPI0_ECAM + 0x840) ==
			0x070f0f03u &&
		fixed_read32(X58_VENDOR_IOH_QPI0_ECAM + 0x854) ==
			0x00010102u &&
		fixed_read32(X58_VENDOR_IOH_QPI0_ECAM + 0x85c) ==
			0x00000002u &&
		fixed_read32(X58_VENDOR_IOH_QPI0_ECAM + 0x864) ==
			0x00322808u &&
		pci_io_read_config32(X58_VENDOR_IOH_QPI_LINK_DEV, 0xc8) ==
			0x0616fc00u &&
		pci_io_read_config32(X58_VENDOR_IOH_STAGE_DEV, 0x7c) == 0 &&
		pci_io_read_config32(X58_VENDOR_IOH_STAGE_DEV, 0x80) == 0 &&
		pci_io_read_config32(X58_VENDOR_IOH_STAGE_DEV, 0x9c) ==
			0xea000000u &&
		pci_io_read_config32(X58_VENDOR_IOH_SYRE_DEV, 0xcc) ==
			0x00000600u;
}

static bool b06vb_high_qpi_minit_is_authorized(void)
{
	return runtime.b06v8_high_qpi_csi_profile &&
		runtime.b06vb_high_qpi_minit_authorized;
}
#endif
#endif

static bool pe32_header_valid(uintptr_t image_base, uint32_t expected_lfanew,
	uint32_t expected_entry_rva, uint32_t expected_image_base,
	uint32_t expected_image_size, uint32_t expected_header_size)
{
	const uintptr_t pe = image_base + expected_lfanew;
	const uintptr_t optional = pe + 24;

	return fixed_read16(image_base) == 0x5a4d &&
		fixed_read32(image_base + 0x3c) == expected_lfanew &&
		fixed_read32(pe) == 0x00004550 &&
		fixed_read16(pe + 4) == 0x014c &&
		fixed_read16(optional) == 0x010b &&
		fixed_read32(optional + 16) == expected_entry_rva &&
		fixed_read32(optional + 28) == expected_image_base &&
		fixed_read32(optional + 32) == 0x20 &&
		fixed_read32(optional + 36) == 0x20 &&
		fixed_read32(optional + 56) == expected_image_size &&
		fixed_read32(optional + 60) == expected_header_size;
}

static bool csi_wrapper_signature_valid(void)
{
	/*
	 * Public source retains offset/length/digest metadata, not vendor
	 * instruction bytes. FNV-1a detects accidental corruption; this is not
	 * authentication. Whole-wrapper/helper checks remain mandatory. The
	 * local composition tool separately pins the input with SHA-256.
	 */
	return bytes_digest((const void *)X58_VENDOR_CSI_WRAPPER_ENTRY,
			X58_VENDOR_CSI_WRAPPER_SIZE) ==
			X58_VENDOR_CSI_WRAPPER_FNV1A &&
		bytes_digest((const void *)X58_VENDOR_CSI_HELPER_ENTRY,
			X58_VENDOR_CSI_HELPER_SIZE) ==
			X58_VENDOR_CSI_HELPER_FNV1A &&
		signature_digest_valid(X58_VENDOR_CSI_WRAPPER_ENTRY, 16, 0x0a838011u) &&
		signature_digest_valid(0xfffc0cf6u, 16, 0x171c45b6u) &&
		signature_digest_valid(X58_VENDOR_CSI_HELPER_ENTRY, 16, 0xc7f60366u) &&
		signature_digest_valid(X58_VENDOR_CSI_CMOS_HELPER, 19, 0xbc9d90a8u)
#if CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
		&& signature_digest_valid(0xfffc0510u, 10, 0x9321081du)
		&& signature_digest_valid(0xfffc0574u, 9, 0xcc90302eu)
		&& signature_digest_valid(0xfffc0667u, 9, 0x06f2f37eu)
		&& signature_digest_valid(0xfffc0838u, 15, 0x9b2cef9au)
		&& signature_digest_valid(0xfffc0a05u, 15, 0xc2398590u)
		&& signature_digest_valid(0xfffc0bbeu, 16, 0x18b7b7e9u)
#elif CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
		&& signature_digest_valid(0xfffc0510u, 10, 0xa246696cu)
		&& signature_digest_valid(0xfffc0bbeu, 16, 0x51beeb35u)
#endif
		;
}

static bool csi_signature_valid(void)
{
	/* This is the loaded PE image entry, not raw file offset 0x220. */
	return bytes_digest((const void *)X58_VENDOR_CSI_IMAGE_BASE,
			X58_VENDOR_CSI_IMAGE_SIZE) == X58_VENDOR_CSI_IMAGE_FNV1A &&
		pe32_header_valid(X58_VENDOR_CSI_IMAGE_BASE,
			X58_VENDOR_CSI_PE_OFFSET, 0x220,
			X58_VENDOR_CSI_IMAGE_BASE, X58_VENDOR_CSI_IMAGE_SIZE, 0x220) &&
		signature_digest_valid(X58_VENDOR_CSI_ENTRY, 16, 0x2511cb2bu);
}

static bool minit_signature_valid(void)
{
	return bytes_digest((const void *)X58_VENDOR_MINIT_IMAGE_BASE,
			X58_VENDOR_MINIT_IMAGE_SIZE) == X58_VENDOR_MINIT_IMAGE_FNV1A &&
		pe32_header_valid(X58_VENDOR_MINIT_IMAGE_BASE, 0xc0, 0x240,
			X58_VENDOR_MINIT_IMAGE_BASE, X58_VENDOR_MINIT_IMAGE_SIZE, 0x240) &&
		signature_digest_valid(X58_VENDOR_MINIT_ENTRY, 16, 0x314badb0u);
}

static void write_guard(uint32_t *guard)
{
	for (size_t i = 0; i < X58_VENDOR_GUARD_SIZE / sizeof(*guard); i++)
		guard[i] = X58_VENDOR_CANARY;
}

static bool guard_valid(const uint32_t *guard)
{
	for (size_t i = 0; i < X58_VENDOR_GUARD_SIZE / sizeof(*guard); i++) {
		if (guard[i] != X58_VENDOR_CANARY)
			return false;
	}

	return true;
}

static void reserve_guard(uintptr_t *cursor, size_t index)
{
	/* Do not put unverified alignment padding between an object and guard. */
	*cursor = align_up(*cursor, sizeof(uint32_t));
	runtime.guards[index] = (uint32_t *)*cursor;
	*cursor += X58_VENDOR_GUARD_SIZE;
}

static bool runtime_layout_valid(void)
{
	uintptr_t cursor = align_up((uintptr_t)_car_unallocated_start,
		X58_VENDOR_GUARD_SIZE);
	const uintptr_t car_end = align_down((uintptr_t)_car_region_end,
		X58_VENDOR_GUARD_SIZE);
	uintptr_t guards[X58_VENDOR_GUARD_COUNT];
	uintptr_t csi_state;
	uintptr_t minit_policy;
	uintptr_t minit_workspace;
	uintptr_t stack_low;
	uintptr_t stack_top;

	if (cursor < (uintptr_t)_car_region_start || car_end <= cursor ||
	    car_end > (uintptr_t)_car_region_end ||
	    car_end - cursor < X58_VENDOR_GUARD_SIZE)
		return false;

	guards[0] = align_up(cursor, sizeof(uint32_t));
	cursor = guards[0] + X58_VENDOR_GUARD_SIZE;
	csi_state = cursor;
	cursor += sizeof(struct x58_vendor_csi_state);
	guards[1] = align_up(cursor, sizeof(uint32_t));
	cursor = guards[1] + X58_VENDOR_GUARD_SIZE;
	minit_policy = cursor;
	cursor += sizeof(struct x58_vendor_minit_policy);
	guards[2] = align_up(cursor, sizeof(uint32_t));
	cursor = guards[2] + X58_VENDOR_GUARD_SIZE;
	minit_workspace = cursor;
	cursor += sizeof(struct x58_vendor_minit_workspace);
	guards[3] = align_up(cursor, sizeof(uint32_t));
	cursor = guards[3] + X58_VENDOR_GUARD_SIZE;
	stack_low = align_up(cursor, 16);
	stack_top = car_end - X58_VENDOR_GUARD_SIZE;
	guards[4] = stack_top;

	if (stack_top <= stack_low ||
	    stack_top - stack_low < X58_VENDOR_MIN_STACK_SIZE)
		return false;

	return runtime.scratch_begin ==
		align_up((uintptr_t)_car_unallocated_start, X58_VENDOR_GUARD_SIZE) &&
		runtime.scratch_end == car_end &&
		(uintptr_t)runtime.csi_state == csi_state &&
		(uintptr_t)runtime.minit_policy == minit_policy &&
		(uintptr_t)runtime.minit_workspace == minit_workspace &&
		runtime.stack_low == stack_low && runtime.stack_top == stack_top &&
		(uintptr_t)runtime.guards[0] == guards[0] &&
		(uintptr_t)runtime.guards[1] == guards[1] &&
		(uintptr_t)runtime.guards[2] == guards[2] &&
		(uintptr_t)runtime.guards[3] == guards[3] &&
		(uintptr_t)runtime.guards[4] == guards[4];
}

static bool running_from_car_stack(void)
{
	uintptr_t esp;

	asm volatile("movl %%esp, %0" : "=r" (esp));
	return esp >= (uintptr_t)_car_stack && esp < (uintptr_t)_ecar_stack;
}

static uint32_t read_eflags(void)
{
	uint32_t flags;

	asm volatile("pushfl; popl %0" : "=rm" (flags));
	return flags;
}

static void read_segment_selectors(uint16_t *code, uint16_t *data,
	uint16_t *extra, uint16_t *stack)
{
	asm volatile("mov %%cs, %0" : "=rm" (*code));
	asm volatile("mov %%ds, %0" : "=rm" (*data));
	asm volatile("mov %%es, %0" : "=rm" (*extra));
	asm volatile("mov %%ss, %0" : "=rm" (*stack));
}

static enum x58_vendor_status cpu_mode_gate(void)
{
	const uint32_t cr0 = read_cr0();
	const uint32_t cr4 = read_cr4();
	const uint32_t eflags = read_eflags();
	uint16_t code;
	uint16_t data;
	uint16_t extra;
	uint16_t stack;

	read_segment_selectors(&code, &data, &extra, &stack);
	if (!(cr0 & CR0_PE) || (cr0 & (CR0_EM | CR0_TS | CR0_PG)) != 0 ||
	    !(cr4 & CR4_OSFXSR) || !(cpuid(1).edx & BIT(26)))
		return X58_VENDOR_ERR_CPU_MODE;
	if ((eflags & (X86_EFLAGS_TF | X86_EFLAGS_IF | X86_EFLAGS_DF |
		      X86_EFLAGS_NT | X86_EFLAGS_VM)) != 0 ||
	    code != GDT_CODE_SEG || data != GDT_DATA_SEG ||
	    extra != GDT_DATA_SEG || stack != GDT_DATA_SEG)
		return X58_VENDOR_ERR_CPU_MODE;

	return X58_VENDOR_OK;
}

static uint32_t current_microcode_revision(void)
{
	const msr_t zero = { 0 };

	/* Intel SDM sequence: clear BIOS_SIGN_ID, serialize with CPUID, read high. */
	wrmsr(IA32_BIOS_SIGN_ID_MSR, zero);
	(void)cpuid(1);
	return rdmsr(IA32_BIOS_SIGN_ID_MSR).hi;
}

static enum x58_vendor_status hardware_gate(void)
{
	const struct cpuid_result vendor = cpuid(0);
	enum x58_vendor_status status;
	msr_t apic_base;

	if (!running_from_car_stack())
		return X58_VENDOR_ERR_WRONG_STAGE;
	if (vendor.eax < 1 || vendor.ebx != 0x756e6547 ||
	    vendor.edx != 0x49656e69 || vendor.ecx != 0x6c65746e)
		return X58_VENDOR_ERR_CPU_VENDOR;
	if (cpuid_eax(1) != X58_VENDOR_CPUID_E5645)
		return X58_VENDOR_ERR_CPU_SIGNATURE;
	if (current_microcode_revision() != X58_VENDOR_MICROCODE_E5645)
		return X58_VENDOR_ERR_MICROCODE_REVISION;

	apic_base = rdmsr(IA32_APIC_BASE_MSR);
	if (!(apic_base.lo & IA32_APIC_BASE_BSP))
		return X58_VENDOR_ERR_NOT_BSP;
	if (apic_base.hi != 0 || !(apic_base.lo & IA32_APIC_BASE_ENABLE) ||
	    (apic_base.lo & IA32_APIC_BASE_ADDRESS_MASK) !=
		IA32_APIC_DEFAULT_ADDRESS)
		return X58_VENDOR_ERR_CPU_MODE;
	status = cpu_mode_gate();
	if (status != X58_VENDOR_OK)
		return status;

	return X58_VENDOR_OK;
}

#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
static bool b06ve_high_qpi_post_minit_platform_exact(void);
#endif

static enum x58_vendor_status csi_platform_gate(void)
{
	const uint32_t sad_id = pci_io_read_config32(X58_VENDOR_SAD_DEV, 0);
	const uint32_t pciexbar_low = pci_io_read_config32(X58_VENDOR_SAD_DEV,
		X58_VENDOR_SAD_PCIEXBAR_LO);
	const uint32_t pciexbar_high = pci_io_read_config32(X58_VENDOR_SAD_DEV,
		X58_VENDOR_SAD_PCIEXBAR_HI);
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
	const uint32_t qpi_state = pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV,
		0x80);
#endif
	uint32_t x58_class_revision;

	if (sad_id != X58_VENDOR_SAD_ID ||
	    pciexbar_low != X58_VENDOR_PCIEXBAR_LO || pciexbar_high != 0)
		return X58_VENDOR_ERR_PCIEXBAR_STATE;

	/* Read MMCONFIG only after the base, enable bit, and high half match. */
	if (fixed_read32(0xe0000000u) != X58_VENDOR_X58_ID)
		return X58_VENDOR_ERR_PLATFORM_STATE;
	x58_class_revision = fixed_read32(0xe0000008u);
	if ((x58_class_revision & 0xffffff00u) != 0x06000000u ||
	    (x58_class_revision & 0xff) < 0x10)
		return X58_VENDOR_ERR_PLATFORM_STATE;

#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
	if (runtime.b06v8_high_qpi_csi_profile) {
#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
		if (runtime.minit_returned) {
			if (!b06ve_high_qpi_post_minit_platform_exact())
				return X58_VENDOR_ERR_PLATFORM_STATE;
		} else
#endif
#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
		if (runtime.csi_returned) {
			if (!b06vb_high_qpi_post_csi_platform_exact())
				return X58_VENDOR_ERR_PLATFORM_STATE;
		} else if (!b06v8_high_qpi_platform_exact()) {
			return X58_VENDOR_ERR_PLATFORM_STATE;
		}
#else
		if (!b06v8_high_qpi_platform_exact())
			return X58_VENDOR_ERR_PLATFORM_STATE;
#endif
	} else if (qpi_state != X58_VENDOR_QPI_SLOW_STATE) {
		return X58_VENDOR_ERR_PLATFORM_STATE;
	}
#else
	if (pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x80) !=
		X58_VENDOR_QPI_SLOW_STATE)
		return X58_VENDOR_ERR_PLATFORM_STATE;
#endif
	if (pci_io_read_config32(X58_VENDOR_MEMORY_CLOCK_DEV, 0x50) !=
			X58_VENDOR_MEMORY_CLOCK_STATE ||
	    pci_io_read_config32(X58_VENDOR_MEMORY_CLOCK_DEV, 0x54) !=
			X58_VENDOR_MEMORY_RATIO_STATE)
		return X58_VENDOR_ERR_PLATFORM_STATE;

	return X58_VENDOR_OK;
}

static enum x58_vendor_status pre_pciexbar_platform_gate(void)
{
	const uint32_t sad_id = pci_io_read_config32(X58_VENDOR_SAD_DEV, 0);
	const uint32_t pciexbar_low = pci_io_read_config32(X58_VENDOR_SAD_DEV,
		X58_VENDOR_SAD_PCIEXBAR_LO);
	const uint32_t pciexbar_high = pci_io_read_config32(X58_VENDOR_SAD_DEV,
		X58_VENDOR_SAD_PCIEXBAR_HI);
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
	const uint32_t qpi_state = pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV,
		0x80);
#endif

	if (sad_id != X58_VENDOR_SAD_ID || pciexbar_high != 0 ||
	    (pciexbar_low != 0 && pciexbar_low != X58_VENDOR_PCIEXBAR_LO))
		return X58_VENDOR_ERR_PCIEXBAR_STATE;
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
	if (runtime.b06v8_high_qpi_csi_profile) {
#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
		if (runtime.minit_returned) {
			if (pciexbar_low != X58_VENDOR_PCIEXBAR_LO ||
			    !b06ve_high_qpi_post_minit_platform_exact())
				return X58_VENDOR_ERR_PLATFORM_STATE;
		} else
#endif
#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
		if (runtime.csi_returned) {
			if (pciexbar_low != X58_VENDOR_PCIEXBAR_LO ||
			    !b06vb_high_qpi_post_csi_platform_exact())
				return X58_VENDOR_ERR_PLATFORM_STATE;
		} else if (pciexbar_low != X58_VENDOR_PCIEXBAR_LO ||
			   !b06v8_high_qpi_platform_exact()) {
			return X58_VENDOR_ERR_PLATFORM_STATE;
		}
#else
		if (pciexbar_low != X58_VENDOR_PCIEXBAR_LO ||
		    !b06v8_high_qpi_platform_exact())
			return X58_VENDOR_ERR_PLATFORM_STATE;
#endif
	} else if (qpi_state != X58_VENDOR_QPI_SLOW_STATE) {
		return X58_VENDOR_ERR_PLATFORM_STATE;
	}
#else
	if (pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x80) !=
		X58_VENDOR_QPI_SLOW_STATE)
		return X58_VENDOR_ERR_PLATFORM_STATE;
#endif
	if (pci_io_read_config32(X58_VENDOR_MEMORY_CLOCK_DEV, 0x50) !=
			X58_VENDOR_MEMORY_CLOCK_STATE ||
	    pci_io_read_config32(X58_VENDOR_MEMORY_CLOCK_DEV, 0x54) !=
			X58_VENDOR_MEMORY_RATIO_STATE)
		return X58_VENDOR_ERR_PLATFORM_STATE;

	return X58_VENDOR_OK;
}

static enum x58_vendor_status common_preflight(void)
{
	enum x58_vendor_status status;

	if (runtime.magic != X58_VENDOR_RUNTIME_MAGIC)
		return X58_VENDOR_ERR_NOT_PREPARED;
	if (!runtime_layout_valid())
		return X58_VENDOR_ERR_CAR_LAYOUT;
	if (runtime.busy)
		return X58_VENDOR_ERR_BUSY;

	status = hardware_gate();
	if (status != X58_VENDOR_OK)
		return status;

	return x58_vendor_runtime_check_canaries();
}

static void reset_vendor_stack(void)
{
	bytes_set((void *)runtime.stack_low, X58_VENDOR_STACK_POISON,
		runtime.stack_top - runtime.stack_low);
	for (size_t i = 0; i < X58_VENDOR_GUARD_COUNT; i++)
		write_guard(runtime.guards[i]);
}

static size_t vendor_stack_high_water(void)
{
	const uint8_t *stack = (const uint8_t *)runtime.stack_low;
	const size_t size = runtime.stack_top - runtime.stack_low;

	for (size_t i = 0; i < size; i++) {
		if (stack[i] != X58_VENDOR_STACK_POISON)
			return size - i;
	}

	return 0;
}

static void copy_call_result(struct x58_vendor_call_result *result)
{
	runtime.last_call.eax = x58_vendor_call_state.eax;
	runtime.last_call.ebx = x58_vendor_call_state.ebx;
	runtime.last_call.ecx = x58_vendor_call_state.ecx;
	runtime.last_call.edx = x58_vendor_call_state.edx;
	runtime.last_call.edi = x58_vendor_call_state.edi;
	runtime.last_call.eflags = x58_vendor_call_state.eflags;
	runtime.last_call.vendor_esp_after_return = x58_vendor_call_state.vendor_esp;

	if (result != NULL)
		*result = runtime.last_call;
}

static void vendor_call(uintptr_t entry, uintptr_t arg0, uintptr_t arg1,
	uintptr_t arg2, unsigned int argc, struct x58_vendor_call_result *result)
{
	x58_vendor_call_state.entry = entry;
	x58_vendor_call_state.arg0 = arg0;
	x58_vendor_call_state.arg1 = arg1;
	x58_vendor_call_state.arg2 = arg2;
	x58_vendor_call_state.argc = argc;
	x58_vendor_call_state.stack_top = runtime.stack_top;

	runtime.busy = true;
	x58_vendor_call_trampoline();
	runtime.busy = false;
	copy_call_result(result);
}

static bool vendor_stack_balanced(unsigned int argc)
{
	return runtime.last_call.vendor_esp_after_return ==
		runtime.stack_top - argc * sizeof(uint32_t);
}

enum x58_vendor_status x58_vendor_runtime_prepare(void)
{
	enum x58_vendor_status status;
	uintptr_t cursor = align_up((uintptr_t)_car_unallocated_start,
		X58_VENDOR_GUARD_SIZE);
	const uintptr_t car_end = align_down((uintptr_t)_car_region_end,
		X58_VENDOR_GUARD_SIZE);
	uintptr_t stack_high_guard;

	if (runtime.magic == X58_VENDOR_RUNTIME_MAGIC)
		return X58_VENDOR_ERR_ORDER;

	bytes_set(&runtime, 0, sizeof(runtime));
	bytes_set(&x58_vendor_call_state, 0, sizeof(x58_vendor_call_state));
	status = hardware_gate();
	if (status != X58_VENDOR_OK)
		return status;

	if (cursor < (uintptr_t)_car_region_start || car_end <= cursor ||
	    car_end > (uintptr_t)_car_region_end)
		return X58_VENDOR_ERR_CAR_LAYOUT;

	runtime.scratch_begin = cursor;
	reserve_guard(&cursor, 0);

	runtime.csi_state = (struct x58_vendor_csi_state *)cursor;
	cursor += sizeof(*runtime.csi_state);
	reserve_guard(&cursor, 1);

	runtime.minit_policy = (struct x58_vendor_minit_policy *)cursor;
	cursor += sizeof(*runtime.minit_policy);
	reserve_guard(&cursor, 2);

	runtime.minit_workspace = (struct x58_vendor_minit_workspace *)cursor;
	cursor += sizeof(*runtime.minit_workspace);
	reserve_guard(&cursor, 3);

	runtime.stack_low = align_up(cursor, 16);
	stack_high_guard = car_end - X58_VENDOR_GUARD_SIZE;
	if (stack_high_guard <= runtime.stack_low ||
	    stack_high_guard - runtime.stack_low < X58_VENDOR_MIN_STACK_SIZE)
		return X58_VENDOR_ERR_CAR_TOO_SMALL;

	runtime.stack_top = stack_high_guard;
	runtime.guards[4] = (uint32_t *)stack_high_guard;
	runtime.scratch_end = car_end;

	bytes_set(runtime.csi_state, 0, sizeof(*runtime.csi_state));
	bytes_set(runtime.minit_policy, 0, sizeof(*runtime.minit_policy));
	bytes_set(runtime.minit_workspace, 0, sizeof(*runtime.minit_workspace));
	reset_vendor_stack();

	/* Direct CSI intentionally remains a zero-only, unvalidated state. */
	runtime.magic = X58_VENDOR_RUNTIME_MAGIC;
	return X58_VENDOR_OK;
}

enum x58_vendor_status x58_vendor_runtime_check_canaries(void)
{
	if (runtime.magic != X58_VENDOR_RUNTIME_MAGIC)
		return X58_VENDOR_ERR_NOT_PREPARED;
	if (!runtime_layout_valid())
		return X58_VENDOR_ERR_CAR_LAYOUT;

	for (size_t i = 0; i < X58_VENDOR_GUARD_COUNT; i++) {
		if (!guard_valid(runtime.guards[i]))
			return X58_VENDOR_ERR_CANARY;
	}

	return X58_VENDOR_OK;
}

enum x58_vendor_status x58_vendor_runtime_probe(
	struct x58_vendor_runtime_info *info)
{
	enum x58_vendor_status status;
	const bool wrapper_valid = csi_wrapper_signature_valid();
	const bool csi_valid = csi_signature_valid();
	const bool minit_valid = minit_signature_valid();
	msr_t apic_base = { 0 };

	if (info == NULL)
		return X58_VENDOR_ERR_ARGUMENT;

	bytes_set(info, 0, sizeof(*info));
	if (runtime.magic == X58_VENDOR_RUNTIME_MAGIC && runtime_layout_valid()) {
		info->prepared = true;
		info->car_scratch_begin = runtime.scratch_begin;
		info->car_scratch_end = runtime.scratch_end;
		info->vendor_stack_low = runtime.stack_low;
		info->vendor_stack_top = runtime.stack_top;
		info->vendor_stack_size = runtime.stack_top - runtime.stack_low;
		info->vendor_stack_high_water = vendor_stack_high_water();
		info->canaries_valid =
			x58_vendor_runtime_check_canaries() == X58_VENDOR_OK;
		info->csi_state_digest = runtime.csi_state_digest;
		info->minit_policy_digest = runtime.minit_policy_digest;
		if (runtime.minit_policy_confirmed)
			info->minit_policy_current_digest = bytes_digest(
				runtime.minit_policy, sizeof(*runtime.minit_policy));
		if (runtime.minit_returned)
			info->minit_workspace_digest = bytes_digest(
				runtime.minit_workspace, sizeof(*runtime.minit_workspace));
		info->csi_wrapper_armed = runtime.csi_wrapper_armed;
		info->csi_call_attempted = runtime.csi_call_attempted;
		info->csi_returned = runtime.csi_returned;
		info->csi_result_accepted = runtime.csi_result_accepted;
		info->minit_policy_confirmed = runtime.minit_policy_confirmed;
		info->minit_armed = runtime.minit_armed;
		info->minit_call_attempted = runtime.minit_call_attempted;
		info->minit_returned = runtime.minit_returned;
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
		info->b06v8_high_qpi_csi_profile =
			runtime.b06v8_high_qpi_csi_profile;
#endif
#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
		info->b06vb_high_qpi_minit_authorized =
			runtime.b06vb_high_qpi_minit_authorized;
#endif
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
		info->b06vg_pre_csi_cpu_a0 = runtime.b06vg_pre_csi_cpu_a0;
		info->b06vg_pre_csi_cpu_a0_valid =
			runtime.b06vg_pre_csi_cpu_a0_valid;
#endif
		info->experimental_status_seed = runtime.experimental_status_seed;
		info->last_call = runtime.last_call;
	}

	info->wrapper_signature_valid = wrapper_valid;
	info->csi_signature_valid = csi_valid;
	info->minit_signature_valid = minit_valid;
	info->cpuid_1_eax = cpuid_eax(1);
	info->cr0 = read_cr0();
	info->cr4 = read_cr4();
	info->eflags_before_call = read_eflags();
	read_segment_selectors(&info->cs_selector, &info->ds_selector,
		&info->es_selector, &info->ss_selector);
	if (info->cpuid_1_eax == X58_VENDOR_CPUID_E5645) {
		info->microcode_revision = current_microcode_revision();
		apic_base = rdmsr(IA32_APIC_BASE_MSR);
		info->apic_base_low = apic_base.lo;
		info->apic_base_high = apic_base.hi;
		info->uncore_sad_id = pci_io_read_config32(X58_VENDOR_SAD_DEV, 0);
		info->pciexbar_low = pci_io_read_config32(X58_VENDOR_SAD_DEV,
			X58_VENDOR_SAD_PCIEXBAR_LO);
		info->pciexbar_high = pci_io_read_config32(X58_VENDOR_SAD_DEV,
			X58_VENDOR_SAD_PCIEXBAR_HI);
		info->qpi_phy_observed_80 = pci_io_read_config32(
			X58_VENDOR_QPI_PHY_DEV, 0x80);
		info->memory_clock_observed_50 = pci_io_read_config32(
			X58_VENDOR_MEMORY_CLOCK_DEV, 0x50);
		info->memory_clock_observed_54 = pci_io_read_config32(
			X58_VENDOR_MEMORY_CLOCK_DEV, 0x54);
		if (info->uncore_sad_id == X58_VENDOR_SAD_ID &&
		    info->pciexbar_low == X58_VENDOR_PCIEXBAR_LO &&
		    info->pciexbar_high == 0) {
			info->x58_hostbridge_id = fixed_read32(0xe0000000u);
			info->x58_hostbridge_class_revision =
				fixed_read32(0xe0000008u);
		}
	}

	status = common_preflight();
	if (status != X58_VENDOR_OK)
		return status;
	if (!wrapper_valid)
		return X58_VENDOR_ERR_WRAPPER_SIGNATURE;
	if (!csi_valid)
		return X58_VENDOR_ERR_CSI_SIGNATURE;
	if (!minit_valid)
		return X58_VENDOR_ERR_MINIT_SIGNATURE;
	status = csi_platform_gate();
	if (status != X58_VENDOR_OK)
		return status;

	return X58_VENDOR_OK;
}

#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
static bool b06ve_high_qpi_post_minit_platform_exact(void)
{
	const uint8_t *state;
	const uint8_t *policy;
	const uint8_t *workspace;
	uint32_t canonical_digest;

	if (runtime.csi_state == NULL || runtime.minit_policy == NULL ||
	    runtime.minit_workspace == NULL)
		return false;
	state = runtime.csi_state->raw;
	policy = runtime.minit_policy->raw;
	workspace = runtime.minit_workspace->raw;
	canonical_digest = b06vd_csi_canonical_digest(state);

	/* Only Run2's byte-08/raw pair is eligible for a B06VE handoff. */
	if (!runtime.b06v8_high_qpi_csi_profile ||
	    !runtime.b06vb_high_qpi_minit_authorized ||
	    runtime.csi_wrapper_armed || !runtime.csi_call_attempted ||
	    !runtime.csi_returned || !runtime.csi_result_accepted ||
	    !runtime.minit_policy_confirmed || runtime.minit_armed ||
	    !runtime.minit_call_attempted || !runtime.minit_returned ||
	    runtime.experimental_status_seed != 0 ||
	    state[X58_VENDOR_B06VD_CSI_DYNAMIC_OFFSET] != 0x08 ||
	    runtime.csi_state_digest != X58_VENDOR_B06VD_CSI_RAW_08_FNV1A ||
	    bytes_digest(state, X58_VENDOR_CSI_STATE_SIZE) !=
		X58_VENDOR_B06VD_CSI_RAW_08_FNV1A ||
	    canonical_digest != X58_VENDOR_B06VD_CSI_CANONICAL_FNV1A ||
	    runtime.minit_policy_digest != X58_VENDOR_B06VE_POLICY_FNV1A ||
	    bytes_digest(policy, X58_VENDOR_MINIT_POLICY_SIZE) !=
		X58_VENDOR_B06VE_POLICY_FNV1A ||
	    bytes_digest(workspace, X58_VENDOR_MINIT_WORKSPACE_SIZE) !=
		X58_VENDOR_B06VE_WORKSPACE_FNV1A ||
	    workspace[1] != 0 || workspace[2] != 0 ||
	    workspace[X58_VENDOR_B06VE_WORK_B3_FLAGS_OFFSET] != 0x02 ||
	    workspace[X58_VENDOR_B06VE_WORK_COMPLETE_OFFSET] != 0x01 ||
	    policy[X58_VENDOR_B06VE_POLICY_STATUS_OFFSET] != 0 ||
	    runtime.last_call.eax != 0 || !vendor_stack_balanced(2))
		return false;

	/* Never dereference IOH ECAM until PCIEXBAR and host identity are exact. */
	if (pci_io_read_config32(X58_VENDOR_SAD_DEV, 0) != X58_VENDOR_SAD_ID ||
	    pci_io_read_config32(X58_VENDOR_SAD_DEV,
		X58_VENDOR_SAD_PCIEXBAR_LO) != X58_VENDOR_PCIEXBAR_LO ||
	    pci_io_read_config32(X58_VENDOR_SAD_DEV,
		X58_VENDOR_SAD_PCIEXBAR_HI) != 0 ||
	    fixed_read32(0xe0000000u) != X58_VENDOR_X58_ID ||
	    fixed_read32(0xe0000008u) != 0x06000013u)
		return false;

	/* Exact post-MINIT Run2 endpoint; CPU ff:02.0 +80/+d0 stay telemetry. */
	return pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x50) ==
			0x160c0112u &&
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x54) ==
			0x00000012u &&
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x6c) ==
			0x0040a0a8u &&
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x80) ==
			X58_VENDOR_QPI_HIGH_STATE &&
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x94) ==
			0x00010202u &&
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0x9c) ==
			0x00b00502u &&
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0xa0) ==
			0x00017000u &&
		pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0xa4) ==
			0x00322808u &&
		pci_io_read_config32(X58_VENDOR_QPI_LINK_DEV, 0x50) ==
			0x86000000u &&
		pci_io_read_config32(X58_VENDOR_QPI_LINK_DEV, 0x58) ==
			0x00064555u &&
		fixed_read32(X58_VENDOR_IOH_QPI0_ECAM + 0x82c) ==
			0x004060a0u &&
		fixed_read32(X58_VENDOR_IOH_QPI0_ECAM + 0x840) ==
			0x070f0f03u &&
		fixed_read32(X58_VENDOR_IOH_QPI0_ECAM + 0x854) ==
			0x00010102u &&
		fixed_read32(X58_VENDOR_IOH_QPI0_ECAM + 0x85c) ==
			0x00000002u &&
		fixed_read32(X58_VENDOR_IOH_QPI0_ECAM + 0x864) ==
			0x00322808u &&
		pci_io_read_config32(X58_VENDOR_IOH_QPI_LINK_DEV, 0xc8) ==
			0x0616fc00u &&
		pci_io_read_config32(X58_VENDOR_IOH_STAGE_DEV, 0x7c) == 0 &&
		pci_io_read_config32(X58_VENDOR_IOH_STAGE_DEV, 0x80) == 0 &&
		pci_io_read_config32(X58_VENDOR_IOH_STAGE_DEV, 0x9c) ==
			0xbf000000u &&
		pci_io_read_config32(X58_VENDOR_IOH_SYRE_DEV, 0xcc) ==
			0x00000600u &&
		pci_io_read_config32(X58_VENDOR_MEMORY_CLOCK_DEV, 0x50) ==
			X58_VENDOR_MEMORY_CLOCK_STATE &&
		pci_io_read_config32(X58_VENDOR_MEMORY_CLOCK_DEV, 0x54) ==
			X58_VENDOR_MEMORY_RATIO_STATE &&
		pci_io_read_config32(X58_VENDOR_B06VE_MC_COMMON_DEV, 0x60) ==
			0x00024489u &&
		pci_io_read_config32(X58_VENDOR_B06VE_UNCORE_COMMON_DEV, 0xf8) ==
			0x00001545u &&
		pci_io_read_config32(X58_VENDOR_B06VE_CHANNEL2_ADDR_DEV, 0x48) ==
			0x000002acu &&
		pci_io_read_config32(X58_VENDOR_B06VE_CHANNEL2_DEV, 0x7c) ==
			0x00000003u &&
		pci_io_read_config32(X58_VENDOR_B06VE_CHANNEL2_DEV, 0x5c) ==
			0x00000140u;
}

enum x58_vendor_status x58_vendor_b06ve_post_minit_probe(
	struct x58_vendor_runtime_info *info)
{
	enum x58_vendor_status status;

	status = x58_vendor_runtime_probe(info);
	if (status != X58_VENDOR_OK)
		return status;
	if (info->minit_policy_digest != X58_VENDOR_B06VE_POLICY_FNV1A ||
	    info->minit_policy_current_digest != X58_VENDOR_B06VE_POLICY_FNV1A)
		return X58_VENDOR_ERR_POLICY_CHANGED;
	status = common_preflight();
	if (status != X58_VENDOR_OK)
		return status;
	if (!csi_wrapper_signature_valid())
		return X58_VENDOR_ERR_WRAPPER_SIGNATURE;
	if (!csi_signature_valid())
		return X58_VENDOR_ERR_CSI_SIGNATURE;
	if (!minit_signature_valid())
		return X58_VENDOR_ERR_MINIT_SIGNATURE;
	if (!b06ve_high_qpi_post_minit_platform_exact())
		return X58_VENDOR_ERR_PLATFORM_STATE;

	return X58_VENDOR_OK;
}
#endif

enum x58_vendor_status x58_vendor_preflight_pciexbar(void)
{
	enum x58_vendor_status status = common_preflight();

	if (status != X58_VENDOR_OK)
		return status;
	if (!csi_wrapper_signature_valid())
		return X58_VENDOR_ERR_WRAPPER_SIGNATURE;
	if (!csi_signature_valid())
		return X58_VENDOR_ERR_CSI_SIGNATURE;
	if (!minit_signature_valid())
		return X58_VENDOR_ERR_MINIT_SIGNATURE;

	return pre_pciexbar_platform_gate();
}

#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
enum x58_vendor_status x58_vendor_b06v8_select_high_qpi_csi(
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	uint32_t saved_pre_a0,
#endif
	uint32_t confirmation)
{
	enum x58_vendor_status status;
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	uint32_t pre_a0_first;
	uint32_t pre_a0_second;
#endif

	if (confirmation != X58_VENDOR_EXPERIMENT_CONFIRMATION)
		return X58_VENDOR_ERR_CONFIRMATION;
	status = common_preflight();
	if (status != X58_VENDOR_OK)
		return status;
	if (runtime.csi_call_attempted || runtime.minit_call_attempted)
		return X58_VENDOR_ERR_ORDER;
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	pre_a0_first = pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0xa0);
	pre_a0_second = pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0xa0);
	if (!x58_vendor_b06vg_high_qpi_cpu_a0_exact(saved_pre_a0) ||
	    pre_a0_first != pre_a0_second || pre_a0_first != saved_pre_a0)
		return X58_VENDOR_ERR_PLATFORM_STATE;
#endif
	if (!b06v8_high_qpi_platform_exact())
		return X58_VENDOR_ERR_PLATFORM_STATE;
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	if (pci_io_read_config32(X58_VENDOR_QPI_PHY_DEV, 0xa0) != saved_pre_a0)
		return X58_VENDOR_ERR_PLATFORM_STATE;
	runtime.b06vg_pre_csi_cpu_a0 = saved_pre_a0;
	runtime.b06vg_pre_csi_cpu_a0_valid = true;
#endif

	runtime.b06v8_high_qpi_csi_profile = true;
#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
	runtime.b06vb_high_qpi_minit_authorized = false;
#endif
	return X58_VENDOR_OK;
}
#endif

#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
enum x58_vendor_status x58_vendor_b06vb_authorize_high_qpi_minit(
	uint32_t confirmation)
{
	enum x58_vendor_status status;

	if (confirmation != X58_VENDOR_EXPERIMENT_CONFIRMATION)
		return X58_VENDOR_ERR_CONFIRMATION;
	status = common_preflight();
	if (status != X58_VENDOR_OK)
		return status;
	if (!runtime.b06v8_high_qpi_csi_profile ||
	    runtime.b06vb_high_qpi_minit_authorized ||
	    !runtime.csi_returned || !runtime.csi_result_accepted ||
	    runtime.minit_call_attempted)
		return X58_VENDOR_ERR_ORDER;
	if (!b06vb_high_qpi_post_csi_platform_exact())
		return X58_VENDOR_ERR_PLATFORM_STATE;

	/* Authorization is for one observation only, not a CSI success claim. */
	runtime.b06vb_high_qpi_minit_authorized = true;
	return X58_VENDOR_OK;
}
#endif

enum x58_vendor_status x58_vendor_arm_csi_wrapper(
	uint8_t experimental_status_seed, uint32_t confirmation)
{
	enum x58_vendor_status status = common_preflight();

	if (confirmation != X58_VENDOR_EXPERIMENT_CONFIRMATION)
		return X58_VENDOR_ERR_CONFIRMATION;
	if (status != X58_VENDOR_OK)
		return status;
	if (!csi_wrapper_signature_valid())
		return X58_VENDOR_ERR_WRAPPER_SIGNATURE;
	if (!csi_signature_valid())
		return X58_VENDOR_ERR_CSI_SIGNATURE;
	status = csi_platform_gate();
	if (status != X58_VENDOR_OK)
		return status;
	if (runtime.csi_call_attempted)
		return X58_VENDOR_ERR_ORDER;

	runtime.experimental_status_seed = experimental_status_seed;
	runtime.csi_wrapper_armed = true;
	return X58_VENDOR_OK;
}

enum x58_vendor_status x58_vendor_call_csi_wrapper(
	struct x58_vendor_call_result *result)
{
	enum x58_vendor_status status;
	uintptr_t transient_state;

	if (runtime.magic != X58_VENDOR_RUNTIME_MAGIC)
		return X58_VENDOR_ERR_NOT_PREPARED;
	if (!runtime.csi_wrapper_armed)
		return X58_VENDOR_ERR_NOT_ARMED;

	/* Consume the one-shot arm even when a repeated preflight rejects it. */
	runtime.csi_wrapper_armed = false;
	status = common_preflight();
	if (status != X58_VENDOR_OK)
		return status;
	if (!csi_wrapper_signature_valid())
		return X58_VENDOR_ERR_WRAPPER_SIGNATURE;
	if (!csi_signature_valid())
		return X58_VENDOR_ERR_CSI_SIGNATURE;
	status = csi_platform_gate();
	if (status != X58_VENDOR_OK)
		return status;
	if (runtime.csi_call_attempted)
		return X58_VENDOR_ERR_ORDER;
	transient_state = runtime.stack_top -
		X58_VENDOR_CSI_WRAPPER_STATE_FROM_TOP;

	/* Latch immediately before entry; even an invalid return may not be retried. */
	runtime.csi_call_attempted = true;
	reset_vendor_stack();
	/* The wrapper returns with port 0x70 set to 0x8e (NMI disabled). */
	vendor_call(X58_VENDOR_CSI_WRAPPER_ENTRY, 0, 0,
		runtime.experimental_status_seed, 3, result);
	status = x58_vendor_runtime_check_canaries();
	if (status != X58_VENDOR_OK)
		return status;
	if (!vendor_stack_balanced(3))
		return X58_VENDOR_ERR_STACK_IMBALANCE;
	/* The wrapper pops its two state arguments into EDX and EDI. */
	if (runtime.last_call.edx != transient_state ||
	    runtime.last_call.edi != transient_state)
		return X58_VENDOR_ERR_CSI_STATE_POINTER;

	/* The wrapper's local object survives on the dedicated stack after ret. */
	bytes_copy(runtime.csi_state, (const void *)transient_state,
		sizeof(*runtime.csi_state));
	runtime.csi_state_digest = bytes_digest(runtime.csi_state,
		sizeof(*runtime.csi_state));
	runtime.csi_returned = true;
	runtime.csi_result_accepted = false;
#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
	runtime.b06vb_high_qpi_minit_authorized = false;
#endif

	return X58_VENDOR_OK;
}

enum x58_vendor_status x58_vendor_accept_csi_result(uint32_t expected_eax,
	uint32_t expected_ebx, uint32_t expected_ecx,
	uint32_t expected_state_digest, uint32_t confirmation)
{
	enum x58_vendor_status status;
	uint32_t current_state_digest;

	if (confirmation != X58_VENDOR_EXPERIMENT_CONFIRMATION)
		return X58_VENDOR_ERR_CONFIRMATION;
	status = common_preflight();
	if (status != X58_VENDOR_OK)
		return status;
	if (!runtime.csi_returned || runtime.csi_state_digest == 0)
		return X58_VENDOR_ERR_ORDER;
	current_state_digest = bytes_digest(runtime.csi_state,
		sizeof(*runtime.csi_state));
	if (runtime.last_call.eax != expected_eax ||
	    runtime.last_call.ebx != expected_ebx ||
	    runtime.last_call.ecx != expected_ecx ||
	    current_state_digest != runtime.csi_state_digest ||
	    current_state_digest != expected_state_digest)
		return X58_VENDOR_ERR_CSI_RESULT_MISMATCH;

	runtime.csi_result_accepted = true;
	return X58_VENDOR_OK;
}

enum x58_vendor_status x58_vendor_call_csi_direct(
	struct x58_vendor_call_result *result)
{
	(void)result;
	/*
	 * The reconstructed ABI is:
	 *   CSI_INITDLL(state_0x304, state_0x304)
	 * Do not remove this gate until a direct-state policy is independently
	 * reconstructed and validated.  The AMI-wrapper path above is preferred.
	 */
	return X58_VENDOR_ERR_POLICY_INCOMPLETE;
}

enum x58_vendor_status x58_vendor_install_confirmed_minit_policy(
	const void *policy, size_t size, uint32_t expected_digest,
	uint32_t confirmation)
{
	enum x58_vendor_status status;
	uint32_t digest;

	if (policy == NULL || size != sizeof(*runtime.minit_policy))
		return X58_VENDOR_ERR_ARGUMENT;
	if (confirmation != X58_VENDOR_EXPERIMENT_CONFIRMATION)
		return X58_VENDOR_ERR_CONFIRMATION;
	status = common_preflight();
	if (status != X58_VENDOR_OK)
		return status;
	if (runtime.minit_call_attempted)
		return X58_VENDOR_ERR_ORDER;
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
	if (runtime.b06v8_high_qpi_csi_profile
#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
	    && !b06vb_high_qpi_minit_is_authorized()
#endif
	   )
		return X58_VENDOR_ERR_PLATFORM_STATE;
#endif
	if (!runtime.csi_result_accepted)
		return X58_VENDOR_ERR_ORDER;
	status = csi_platform_gate();
	if (status != X58_VENDOR_OK)
		return status;
	digest = bytes_digest(policy, size);
	if (digest != expected_digest)
		return X58_VENDOR_ERR_POLICY_CHANGED;

	bytes_copy(runtime.minit_policy, policy, sizeof(*runtime.minit_policy));
	runtime.minit_policy_digest = digest;
	runtime.minit_policy_confirmed = true;
	runtime.minit_armed = false;
	return X58_VENDOR_OK;
}

enum x58_vendor_status x58_vendor_invalidate_minit_policy(void)
{
	enum x58_vendor_status status = common_preflight();

	if (status != X58_VENDOR_OK)
		return status;
	if (runtime.minit_call_attempted)
		return X58_VENDOR_ERR_ORDER;

	bytes_set(runtime.minit_policy, 0, sizeof(*runtime.minit_policy));
	bytes_set(runtime.minit_workspace, 0, sizeof(*runtime.minit_workspace));
	runtime.minit_policy_digest = 0;
	runtime.minit_policy_confirmed = false;
	runtime.minit_armed = false;
	return X58_VENDOR_OK;
}

enum x58_vendor_status x58_vendor_arm_minit(uint32_t confirmation)
{
	enum x58_vendor_status status;

	if (confirmation != X58_VENDOR_EXPERIMENT_CONFIRMATION)
		return X58_VENDOR_ERR_CONFIRMATION;
	status = common_preflight();
	if (status != X58_VENDOR_OK)
		return status;
	if (!minit_signature_valid())
		return X58_VENDOR_ERR_MINIT_SIGNATURE;
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
	if (runtime.b06v8_high_qpi_csi_profile
#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
	    && !b06vb_high_qpi_minit_is_authorized()
#endif
	   )
		return X58_VENDOR_ERR_PLATFORM_STATE;
#endif
	if (!runtime.csi_result_accepted)
		return X58_VENDOR_ERR_ORDER;
	status = csi_platform_gate();
	if (status != X58_VENDOR_OK)
		return status;
	if (!runtime.minit_policy_confirmed)
		return X58_VENDOR_ERR_POLICY_INCOMPLETE;
	if (bytes_digest(runtime.minit_policy, sizeof(*runtime.minit_policy)) !=
	    runtime.minit_policy_digest)
		return X58_VENDOR_ERR_POLICY_CHANGED;
	if (runtime.minit_call_attempted)
		return X58_VENDOR_ERR_ORDER;

	runtime.minit_armed = true;
	return X58_VENDOR_OK;
}

enum x58_vendor_status x58_vendor_call_minit(
	struct x58_vendor_call_result *result)
{
	enum x58_vendor_status status;

	if (runtime.magic != X58_VENDOR_RUNTIME_MAGIC)
		return X58_VENDOR_ERR_NOT_PREPARED;
	if (!runtime.minit_armed)
		return X58_VENDOR_ERR_NOT_ARMED;

	runtime.minit_armed = false;
	status = common_preflight();
	if (status != X58_VENDOR_OK)
		return status;
	if (!minit_signature_valid())
		return X58_VENDOR_ERR_MINIT_SIGNATURE;
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
	if (runtime.b06v8_high_qpi_csi_profile
#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
	    && !b06vb_high_qpi_minit_is_authorized()
#endif
	   )
		return X58_VENDOR_ERR_PLATFORM_STATE;
#endif
	if (!runtime.csi_result_accepted || !runtime.minit_policy_confirmed ||
	    runtime.minit_call_attempted)
		return X58_VENDOR_ERR_ORDER;
	status = csi_platform_gate();
	if (status != X58_VENDOR_OK)
		return status;
	if (bytes_digest(runtime.minit_policy, sizeof(*runtime.minit_policy)) !=
	    runtime.minit_policy_digest)
		return X58_VENDOR_ERR_POLICY_CHANGED;

	/* Latch immediately before entry; even an invalid return may not be retried. */
	runtime.minit_call_attempted = true;
	bytes_set(runtime.minit_workspace, 0, sizeof(*runtime.minit_workspace));
	reset_vendor_stack();
	vendor_call(X58_VENDOR_MINIT_ENTRY, (uintptr_t)runtime.minit_policy,
		(uintptr_t)runtime.minit_workspace, 0, 2, result);
	status = x58_vendor_runtime_check_canaries();
	if (status != X58_VENDOR_OK)
		return status;
	if (!vendor_stack_balanced(2))
		return X58_VENDOR_ERR_STACK_IMBALANCE;
	runtime.minit_returned = true;

	/* Never copy the workspace to 1 MiB or access ordinary DRAM here. */
	return X58_VENDOR_OK;
}

const void *x58_vendor_csi_state_snapshot(void)
{
	if (runtime.magic != X58_VENDOR_RUNTIME_MAGIC || !runtime_layout_valid() ||
	    x58_vendor_runtime_check_canaries() != X58_VENDOR_OK ||
	    !runtime.csi_returned)
		return NULL;
	return runtime.csi_state;
}

const void *x58_vendor_minit_policy(void)
{
	if (runtime.magic != X58_VENDOR_RUNTIME_MAGIC || !runtime_layout_valid() ||
	    x58_vendor_runtime_check_canaries() != X58_VENDOR_OK ||
	    !runtime.minit_policy_confirmed)
		return NULL;
	return runtime.minit_policy;
}

const void *x58_vendor_minit_workspace(void)
{
	if (runtime.magic != X58_VENDOR_RUNTIME_MAGIC || !runtime_layout_valid() ||
	    x58_vendor_runtime_check_canaries() != X58_VENDOR_OK ||
	    !runtime.minit_returned)
		return NULL;
	return runtime.minit_workspace;
}

const char *x58_vendor_status_name(enum x58_vendor_status status)
{
	static const char *const names[] = {
		[X58_VENDOR_OK] = "ok",
		[X58_VENDOR_ERR_ARGUMENT] = "argument",
		[X58_VENDOR_ERR_NOT_PREPARED] = "not-prepared",
		[X58_VENDOR_ERR_WRONG_STAGE] = "not-on-car-stack",
		[X58_VENDOR_ERR_CAR_LAYOUT] = "car-layout",
		[X58_VENDOR_ERR_CAR_TOO_SMALL] = "car-too-small",
		[X58_VENDOR_ERR_CANARY] = "car-canary",
		[X58_VENDOR_ERR_CPU_VENDOR] = "cpu-vendor",
		[X58_VENDOR_ERR_CPU_SIGNATURE] = "cpu-signature",
		[X58_VENDOR_ERR_MICROCODE_REVISION] = "microcode-revision",
		[X58_VENDOR_ERR_NOT_BSP] = "not-bsp",
		[X58_VENDOR_ERR_CPU_MODE] = "cpu-mode",
		[X58_VENDOR_ERR_PCIEXBAR_STATE] = "pciexbar-state",
		[X58_VENDOR_ERR_PLATFORM_STATE] = "platform-state",
		[X58_VENDOR_ERR_WRAPPER_SIGNATURE] = "wrapper-signature",
		[X58_VENDOR_ERR_CSI_SIGNATURE] = "csi-signature",
		[X58_VENDOR_ERR_MINIT_SIGNATURE] = "minit-signature",
		[X58_VENDOR_ERR_POLICY_INCOMPLETE] = "policy-incomplete",
		[X58_VENDOR_ERR_POLICY_CHANGED] = "policy-changed",
		[X58_VENDOR_ERR_CONFIRMATION] = "confirmation",
		[X58_VENDOR_ERR_NOT_ARMED] = "not-armed",
		[X58_VENDOR_ERR_ORDER] = "call-order",
		[X58_VENDOR_ERR_BUSY] = "busy",
		[X58_VENDOR_ERR_CSI_RESULT_MISMATCH] = "csi-result-mismatch",
		[X58_VENDOR_ERR_STACK_IMBALANCE] = "vendor-stack-imbalance",
		[X58_VENDOR_ERR_CSI_STATE_POINTER] = "csi-state-pointer",
	};

	if ((unsigned int)status >= ARRAY_SIZE(names) || names[status] == NULL)
		return "unknown";
	return names[status];
}
