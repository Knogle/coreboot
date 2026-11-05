/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NATIVE99_APOB_SELECT_H
#define NATIVE99_APOB_SELECT_H

#include <stdint.h>
#include <b565d4_memlayout.h>

#define NATIVE99_PARENT_BEGIN B565D4_NATIVE_DRAM_BASE
#define NATIVE99_PARENT_END B565D4_NATIVE_DRAM_END
#define NATIVE99_APOB_MIN_BYTES 0xf8U
/* Conservative original APOB aperture; actual header length remains binding. */
#define NATIVE99_APOB_MAX_BYTES 0x28000U
#define NATIVE99_APOB_MAX_HOLES 18U

struct native99_span {
	uint64_t begin, end;
};

struct native99_apob_selection {
	uint32_t apob_bytes, records, map_offset, hole_count;
	uint64_t top, available_begin, available_end;
};

/* Pure byte/geometry inspection. Source and separate own result MAY be inside
 * the selected parent (e.g. the immutable packet and current CBMEM/stack).
 * Mapping/stability/write permission precede entry. It validates the complete
 * finite APOB and the fixed parent's positive gap; no external ownership list.
 * Failure leaves result unchanged. This is not the EARLY first-store wrapper.
 */
int native99_apob_inspect(const uint8_t *snapshot, uint32_t bytes,
	uint32_t physical_bits, struct native99_apob_selection *result);

/* Unintegrated IA32 proposal. The caller qualifies stable readable snapshot
 * and exclusions, its existing stack, and separate writable own result before
 * entry. This routine reads only those objects and writes result on success.
 * It classifies one fixed arena under the selected AM4 non-V2 supplier map;
 * it does not grant backing, change ownership, create CBMEM, or write the arena.
 * Exclusions are external to the selected parent: reset/APOB/old stack/images,
 * actual source/control objects, and any fresh protected SMM span. Internal
 * partitions of the newly selected parent belong to the later bootstrap.
 */
int native99_apob_select(const uint8_t *snapshot, uint32_t bytes,
	uint32_t physical_bits, const struct native99_span *excluded,
	uint32_t excluded_count, struct native99_apob_selection *result);

#endif
