/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NATIVE84_BOUNDED_RMODULE_LOAD_H
#define NATIVE84_BOUNDED_RMODULE_LOAD_H

#include "native84_handoff.h"
#include "native84_rmodule_preflight.h"

struct native84_bounded_load_request {
	const uint8_t *source;
	uint32_t source_bytes;
	uint32_t destination;
	/* An already isolated allocation, wholly inside descriptor CBMEM. */
	struct native84_interval allocation;
	const struct native84_handoff *descriptor;
	uint32_t expected_epoch;
	/* Complete current live controls, images and transitive stack extents. */
	const struct native84_interval *protected;
	uint32_t protected_count;
};

struct native84_bounded_load_output {
	uint32_t epoch;
	uint32_t entry;
	uint32_t parameters;
	/* Preserve the stock c_start argument; the descriptor is not that arg. */
	uint32_t cbmem_top;
	struct native84_interval allocation;
	struct native84_interval program;
};

enum native84_bounded_load_status {
	NATIVE84_LOAD_VALID = 0,
	NATIVE84_LOAD_BAD_ARGUMENT,
	NATIVE84_LOAD_BAD_ALIAS,
	NATIVE84_LOAD_BAD_HANDOFF,
	NATIVE84_LOAD_BAD_ALLOCATION,
	NATIVE84_LOAD_PREFLIGHT_REJECTED,
	NATIVE84_LOAD_PARAMETER_RELOCATION,
	NATIVE84_LOAD_HEADER_CHANGED,
	NATIVE84_LOAD_PARSE_REJECTED,
	NATIVE84_LOAD_PARSED_STATE_REJECTED,
	/* Every status at or above this value requires caller terminal halt. */
	NATIVE84_LOAD_ATTEMPT_FAILED = 0x80,
	NATIVE84_LOAD_POSTLOAD_STATE_FAILED,
	NATIVE84_LOAD_DESCRIPTOR_COPY_FAILED,
};

/*
 * Isolated, real IA32, single-epoch load operation. No entry is called here.
 * BEFORE the C prologue the caller must qualify all mappings/backing, current
 * ownership, flat context, complete emitted frames/code/control exclusions,
 * authenticated source identity, exact initialized reserved parameter-symbol
 * identity (not executable code), serialized bounds, and all stock loader/
 * console/hook effects. Structural header fields cannot establish symbol identity.
 * Input objects/list/descriptor/header and external controls stay stable and
 * disjoint from wrapper/callee local objects throughout this operation. The
 * complete source is immutable until rmodule_load is attempted. In-place load
 * then may change payload/fixups and clear a table in future BSS; its header
 * remains stable. Disjoint-source bytes stay immutable throughout.
 *
 * The protected list must cover the actual request/output/descriptor/list and
 * this wrapper workspace, plus every other live image/control and the entire
 * future-qualified transitive stack, including hooks. Numeric checks cannot
 * establish those conditions or authorize any effect. No callback selectors,
 * allocator, CBFS access, loader substitute, retry or fallback exist here.
 *
 * Output is published only on VALID; otherwise it is untouched under those
 * caller prerequisites. A load-attempt failure can leave destination contents
 * modified: caller MUST terminal halt, with no retry/rollback/free/OEM return.
 * Even VALID grants no entry-call or native handoff permission. One invocation
 * is one fresh preflight/parse/load transaction, not a reusable capability.
 */
enum native84_bounded_load_status native84_bounded_rmodule_load(
	const struct native84_bounded_load_request *request,
	struct native84_bounded_load_output *output);

#endif
