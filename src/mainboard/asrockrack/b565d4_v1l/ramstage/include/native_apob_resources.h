/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NATIVE_APOB_RESOURCES_H
#define NATIVE_APOB_RESOURCES_H

#include <stdint.h>

/* New private ID, distinct from both HOB_POINTER and the old HOB snapshot. */
#define CBMEM_ID_NATIVE_APOB_SNAPSHOT 0x4e41504fU

struct device;

enum native_apob_status {
	NATIVE_APOB_OK,
	NATIVE_APOB_BAD_ARGUMENT,
	NATIVE_APOB_ALREADY_ATTEMPTED,
	NATIVE_APOB_BAD_PACKET,
	NATIVE_APOB_BAD_MAP,
	NATIVE_APOB_ALLOCATION_FAILED,
	NATIVE_APOB_UNSTABLE,
	NATIVE_APOB_NO_SNAPSHOT,
	NATIVE_APOB_INDEX_COLLISION,
	NATIVE_APOB_PUBLICATION_FAILED,
};

/* Once after bounded CBMEM recovery. The caller qualifies this stage's code,
 * stack, descriptor, immutable fixed native packet, selected exclusions and
 * serialized entry.
 * Copy the versioned header, early SMM state and whole APOB into a new
 * bounded own CBMEM entry; no PI HOB, PEI allocation receipt, source pointer into the dead early stage, or
 * alternate-map search is used. Any failure is terminal, never a retry.
 */
enum native_apob_status native_apob_capture(uint32_t parent, uint32_t bytes);

/* Once for domain0 before allocation/bootmem cloning. Only positive supplier
 * Available gaps become RAM, minus every hole and the explicit exclusions.
 * Raw1 is omitted from RAM without occupying the entire PCI window. Actual
 * fixed MMIO, including the genuine IOMMU reservation, remains independent.
 */
enum native_apob_status native_apob_publish_resources(struct device *domain,
	unsigned long first_index, unsigned long *next_index);

/* No resource publication or RAM retagging. Require the whole interval inside
 * one raw1 PCI hole in the current strict owned APOB copy. Both endpoints
 * must be 64-KiB aligned; callers with smaller apertures check their cover.
 */
enum native_apob_status native_apob_require_pci_hole(uint64_t begin, uint64_t end);

/* No resource publication or target access. Require equality with the same
 * bounded, current own-CBMEM APOB map and its physical-address-width bound.
 */
enum native_apob_status native_apob_require_top(uint64_t top);

/* Revalidate the existing own-CBMEM packet and return its immutable APOB
 * extent for inventory consumers. No live supplier read or RAM publication.
 * Output pointers remain unchanged on failure.
 */
enum native_apob_status native_apob_get_snapshot(const uint8_t **apob, uint32_t *bytes);

#endif
