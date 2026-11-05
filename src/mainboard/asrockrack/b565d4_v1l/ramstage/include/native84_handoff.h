/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NATIVE84_HANDOFF_H
#define NATIVE84_HANDOFF_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

/* Genuine coreboot stdint.h supplies types but not this C99 literal macro. */
#ifndef UINT32_C
#define UINT32_C(value) value##U
#endif
_Static_assert(sizeof(unsigned int) == 4 && sizeof(uint32_t) == 4,
	"Native84 unsigned DWORD literal width");

#define NATIVE84_HANDOFF_MAGIC UINT32_C(0x34384243)
#define NATIVE84_HANDOFF_VERSION UINT32_C(1)
#define NATIVE84_HANDOFF_BYTES UINT32_C(56)
#define NATIVE84_PARENT_BYTES UINT32_C(0x800000)
#define NATIVE84_IA32_HANDLE_BYTES UINT32_C(16)
#define NATIVE84_MAX_PROTECTED 32

/* Initialized module parameters, separate from the stock cbmem_top argument. */
struct native84_handoff {
	uint32_t magic;
	uint32_t version;
	uint32_t bytes;
	uint32_t epoch;
	uint32_t parent_base;
	uint32_t parent_bytes;
	uint32_t early_base;
	uint32_t early_bytes;
	uint32_t cbmem_bottom;
	uint32_t cbmem_top;
	uint32_t media_base;
	uint32_t media_bytes;
	uint32_t handle_bank_base;
	uint32_t handle_bank_bytes;
};

#include "native84_interval.h"

struct native84_validation_request {
	uint32_t expected_epoch;
	/* The eventual actual IA32 build must separately assert sizeof(struct imd). */
	uint32_t emitted_handle_bytes;
	const struct native84_interval *protected;
	size_t protected_count;
};

struct native84_layout {
	uint32_t epoch;
	struct native84_interval parent;
	struct native84_interval early;
	struct native84_interval cbmem;
	struct native84_interval media;
	struct native84_interval handle_bank;
	uint32_t creator_handle;
	uint32_t recovery_handle;
};

enum native84_validation_result {
	NATIVE84_VALID = 0,
	NATIVE84_INVALID_ARGUMENT,
	NATIVE84_INVALID_PROTOCOL,
	NATIVE84_INVALID_EXTENT,
	NATIVE84_INVALID_PARTITION,
	NATIVE84_INVALID_HANDLE_BANK,
	NATIVE84_PROTECTED_OVERLAP,
};

/*
 * Pure arithmetic validation only. The caller supplies readable, stable input
 * objects and a writable, separate output; mapping, current ownership, complete
 * protected extents and all-agent exclusion remain caller prerequisites.
 * On failure, *layout is unchanged. Neither a bank nor an arena is touched.
 */
enum native84_validation_result native84_validate_handoff(
	const struct native84_handoff *handoff,
	const struct native84_validation_request *request,
	struct native84_layout *layout);

#endif
