/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_BLOCK_VERMEER_PCC_H
#define AMD_BLOCK_VERMEER_PCC_H

#include <acpi/acpigen.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VERMEER_PCC_BYTES 0x10000U
#define VERMEER_PCC_CPU_STRIDE 0x100U
#define VERMEER_PCC_CPU_DATA_BYTES 0x58U
#define VERMEER_PCC_MAX_CPUS 16U

struct vermeer_pcc_subspace {
	uint8_t type, length, reserved[6];
	uint64_t base, bytes;
	acpi_addr_t doorbell;
	uint64_t preserve, write;
	uint32_t latency_us, maximum_rate_per_minute;
	uint16_t minimum_turnaround_us;
} __packed;

struct vermeer_pcc_plan {
	uint64_t owner_begin, owner_end;
	uint64_t shared_begin, shared_end;
	uint64_t uc_base, uc_mask;
	unsigned int cpu_count;
};

/* Pure calculations only. The caller must first obtain the owner interval
 * through the admitted CBMEM/resource allocation path. An arbitrary reserved
 * PSP/ABL/SMM range is not an allocation. No allocation or MTRR write occurs.
 * All helpers leave their output unchanged on failure.
 */
bool vermeer_pcc_plan(uint64_t owner_begin, uint64_t owner_end,
		    unsigned int physical_bits, unsigned int cpu_count,
		    struct vermeer_pcc_plan *out);
bool vermeer_pcc_subspace(uint64_t base, struct vermeer_pcc_subspace *out);
bool vermeer_cppc_config(uint32_t information, unsigned int slot,
		       unsigned int lowest_mhz, unsigned int nominal_mhz,
		       struct cppc_config *config);

/* This validates a captured completed read reply, not its freshness. Actual
 * SMU registration, all-CPU UC attributes, ownership barriers, a finite PCC
 * transaction and its observed completion remain the caller's responsibility.
 */
bool vermeer_pcc_reply(const uint8_t *snapshot, size_t bytes, unsigned int cpu_count);

#endif /* AMD_BLOCK_VERMEER_PCC_H */
