/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef CPU_INTEL_MODEL_206CX_POLICY_H
#define CPU_INTEL_MODEL_206CX_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#define MODEL_206CX_SIGNATURE 0x000206c2
#define MODEL_206CX_MAX_CPUS 12
#define MODEL_206CX_MSR_2E2 0x2e2
#define MODEL_206CX_MSR_2E2_VALUE 2

struct model_206cx_topology {
	unsigned int threads;
	unsigned int smt_shift;
	unsigned int package_shift;
	unsigned int package_id;
};

/* Intel SDM Vol. 2, CPUID.0BH. EBX is a count, not a list of APIC IDs.
 * Admit only the two-level, single-package Westmere-EP path. A sparse
 * APIC ID space is valid; running APs must identify themselves separately.
 */
static inline bool model_206cx_decode_topology(
	uint32_t smt_eax, uint32_t smt_ebx, uint32_t smt_ecx, uint32_t smt_edx,
	uint32_t core_eax, uint32_t core_ebx, uint32_t core_ecx, uint32_t core_edx,
	uint32_t end_ebx, uint32_t end_ecx, struct model_206cx_topology *topology)
{
	unsigned int smt_count = smt_ebx & 0xffff;
	unsigned int threads = core_ebx & 0xffff;
	unsigned int smt_shift = smt_eax & 0x1f;
	unsigned int package_shift = core_eax & 0x1f;

	if ((smt_ecx & 0xffff) != 0x100 || (core_ecx & 0xffff) != 0x201 ||
	    (end_ecx & 0xff00) || (end_ebx & 0xffff) || smt_edx != core_edx ||
	    smt_edx >= 0xff || (smt_count != 1 && smt_count != 2) ||
	    smt_shift > 1 || smt_count > (1U << smt_shift) ||
	    package_shift < smt_shift || package_shift > 8 || !threads ||
	    threads > MODEL_206CX_MAX_CPUS ||
	    threads < smt_count || threads % smt_count ||
	    threads / smt_count > 6 || threads > (1U << package_shift))
		return false;

	topology->threads = threads;
	topology->smt_shift = smt_shift;
	topology->package_shift = package_shift;
	topology->package_id = smt_edx >> package_shift;
	return true;
}

#endif
