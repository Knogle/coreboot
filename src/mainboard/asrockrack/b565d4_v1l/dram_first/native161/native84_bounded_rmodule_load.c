/* SPDX-License-Identifier: GPL-2.0-only */
#include "native84_bounded_rmodule_load.h"
#include <imd.h>
#include <limits.h>
#include <stddef.h>
#include <rmodule.h>

#if !defined(__i386__) || !defined(__BYTE_ORDER__) || !defined(__ORDER_LITTLE_ENDIAN__) || \
	__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "This wrapper requires the separately qualified IA32 little-endian ABI"
#endif

_Static_assert(CHAR_BIT == 8 && sizeof(void *) == 4 && sizeof(uintptr_t) == 4 &&
	sizeof(size_t) == 4 && sizeof(int) == 4, "real IA32 ABI");
_Static_assert(sizeof(struct native84_handoff) == 56 && sizeof(struct imd) == 16,
	"descriptor and actual IMD handle ABI");
_Static_assert(sizeof(struct rmodule_header) == 64 && sizeof(struct rmodule) == 20,
	"genuine rmodule ABI");
_Static_assert(sizeof(struct native84_bounded_load_request) == 36 &&
	sizeof(struct native84_bounded_load_output) == 32, "wrapper control ABI");
_Static_assert(sizeof(struct native84_rmodule_request) == 36 &&
	sizeof(struct native84_rmodule_facts) == 144, "unchanged preflight ABI");
_Static_assert(_Alignof(struct native84_bounded_load_request) == 4 &&
	_Alignof(struct native84_bounded_load_output) == 4, "control alignment");
_Static_assert(NATIVE84_MAX_PROTECTED == NATIVE84_RMODULE_MAX_PROTECTED,
	"both complete protected-list capacities agree");

struct load_workspace {
	struct native84_bounded_load_request request;
	struct native84_handoff descriptor;
	struct native84_interval controls[5];
	struct native84_interval guard_protected[NATIVE84_MAX_PROTECTED];
	struct native84_rmodule_interval load_protected[NATIVE84_RMODULE_MAX_PROTECTED];
	struct native84_validation_request guard_request;
	struct native84_layout layout;
	struct native84_rmodule_request preflight_request;
	struct native84_rmodule_facts facts;
	struct rmodule module;
	struct native84_bounded_load_output result;
};

static int span(uint32_t begin, uint32_t bytes, struct native84_interval *out)
{
	if (!begin || !bytes || bytes > UINT32_MAX - begin)
		return 0;
	out->begin = begin;
	out->end = begin + bytes;
	return 1;
}

static int overlaps(struct native84_interval a, struct native84_interval b)
{
	return a.begin < b.end && b.begin < a.end;
}

static int contains(struct native84_interval outer, struct native84_interval inner)
{
	return outer.begin <= inner.begin && inner.end <= outer.end;
}

static int covered(const struct native84_interval *list, uint32_t count,
	struct native84_interval object)
{
	uint32_t i;

	for (i = 0; i < count; i++) {
		if (contains(list[i], object))
			return 1;
	}
	return 0;
}

static uint32_t le32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
		(uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int header_equal(const uint8_t *source, const uint8_t *saved)
{
	const volatile uint8_t *live = source;
	uint32_t i;

	for (i = 0; i < NATIVE84_RMODULE_HEADER_BYTES; i++) {
		if (live[i] != saved[i])
			return 0;
	}
	return 1;
}

static int module_state(const struct rmodule *module,
	const struct native84_rmodule_facts *facts, int loaded)
{
	/* Verify pointers before any genuine getter dereferences the header. */
	if ((uintptr_t)module->header != facts->source.begin ||
	    (uintptr_t)module->payload != facts->source.begin + 64 ||
	    (uint32_t)module->payload_size != facts->payload.end - facts->payload.begin ||
	    (uintptr_t)module->relocations != facts->relocations.begin ||
	    (uintptr_t)module->location != (loaded ? facts->program.begin : 0))
		return 0;
	if ((uint32_t)rmodule_memory_size(module) !=
	    facts->program.end - facts->program.begin ||
	    (uint32_t)rmodule_entry_offset(module) != facts->entry - facts->program.begin ||
	    rmodule_load_alignment(module) != 4096)
		return 0;
	if (loaded) {
		if ((uintptr_t)rmodule_entry(module) != facts->entry ||
		    (uintptr_t)rmodule_parameters(module) != facts->parameters.begin)
			return 0;
	} else if (rmodule_entry(module) || rmodule_parameters(module)) {
		return 0;
	}
	return 1;
}

enum native84_bounded_load_status native84_bounded_rmodule_load(
	const struct native84_bounded_load_request *request,
	struct native84_bounded_load_output *output)
{
	struct load_workspace w;
	struct native84_interval source, site, parameters;
	struct native84_handoff *attached;
	const volatile uint8_t *stored;
	const uint8_t *saved;
	uint32_t i, j, table_offset;

	/* Stable complete frames and valid typed backing are pre-prologue premises. */
	if (!request || !output || ((uintptr_t)request & 3) || ((uintptr_t)output & 3) ||
	    !span((uintptr_t)request, sizeof(*request), &w.controls[0]) ||
	    !span((uintptr_t)output, sizeof(*output), &w.controls[1]))
		return NATIVE84_LOAD_BAD_ARGUMENT;
	w.request = *request;
	if (!w.request.descriptor || ((uintptr_t)w.request.descriptor & 3) ||
	    !span((uintptr_t)w.request.descriptor, sizeof(w.descriptor), &w.controls[2]) ||
	    !w.request.protected || ((uintptr_t)w.request.protected & 3) ||
	    !w.request.protected_count || w.request.protected_count > NATIVE84_MAX_PROTECTED ||
	    !span((uintptr_t)w.request.protected,
		w.request.protected_count * sizeof(*w.request.protected), &w.controls[3]) ||
	    !span((uintptr_t)&w, sizeof(w), &w.controls[4]) ||
	    w.request.source_bytes < 64 || w.request.source_bytes > INT32_MAX ||
	    !span((uintptr_t)w.request.source, w.request.source_bytes, &source) ||
	    !w.request.allocation.begin ||
	    w.request.allocation.begin >= w.request.allocation.end)
		return NATIVE84_LOAD_BAD_ARGUMENT;
	for (i = 0; i < 5; i++) {
		if (overlaps(w.controls[i], source) ||
		    overlaps(w.controls[i], w.request.allocation))
			return NATIVE84_LOAD_BAD_ALIAS;
		for (j = 0; j < i; j++) {
			if (overlaps(w.controls[i], w.controls[j]))
				return NATIVE84_LOAD_BAD_ALIAS;
		}
	}
	w.descriptor = *w.request.descriptor;
	for (i = 0; i < w.request.protected_count; i++) {
		struct native84_interval p = w.request.protected[i];

		if (p.begin >= p.end)
			return NATIVE84_LOAD_BAD_ARGUMENT;
		if (overlaps(w.request.allocation, p))
			return NATIVE84_LOAD_BAD_ALLOCATION;
		w.guard_protected[i] = p;
		w.load_protected[i].begin = p.begin;
		w.load_protected[i].end = p.end;
	}
	for (i = 0; i < 5; i++) {
		if (!covered(w.guard_protected, w.request.protected_count, w.controls[i]))
			return NATIVE84_LOAD_BAD_ALIAS;
	}
	w.guard_request.expected_epoch = w.request.expected_epoch;
	w.guard_request.emitted_handle_bytes = sizeof(struct imd);
	w.guard_request.protected = w.guard_protected;
	w.guard_request.protected_count = w.request.protected_count;
	if (native84_validate_handoff(&w.descriptor, &w.guard_request, &w.layout) !=
	    NATIVE84_VALID)
		return NATIVE84_LOAD_BAD_HANDOFF;
	if (!contains(w.layout.cbmem, w.request.allocation) ||
	    overlaps(w.layout.handle_bank, w.request.allocation) ||
	    overlaps(w.layout.handle_bank, source))
		return NATIVE84_LOAD_BAD_ALLOCATION;
	for (i = 0; i < 5; i++) {
		if (overlaps(w.layout.handle_bank, w.controls[i]))
			return NATIVE84_LOAD_BAD_ALIAS;
	}
	w.preflight_request.source = w.request.source;
	w.preflight_request.source_bytes = w.request.source_bytes;
	w.preflight_request.destination = w.request.destination;
	w.preflight_request.allocation.begin = w.request.allocation.begin;
	w.preflight_request.allocation.end = w.request.allocation.end;
	w.preflight_request.descriptor = &w.descriptor;
	w.preflight_request.descriptor_bytes = sizeof(w.descriptor);
	w.preflight_request.protected = w.load_protected;
	w.preflight_request.protected_count = w.request.protected_count;
	if (native84_rmodule_preflight(&w.preflight_request, &w.facts) !=
	    NATIVE84_RMODULE_VALID)
		return NATIVE84_LOAD_PREFLIGHT_REJECTED;
	parameters.begin = w.facts.parameters.begin;
	parameters.end = w.facts.parameters.end;
	table_offset = w.facts.relocations.begin - w.facts.source.begin;
	for (i = 0; i < w.facts.relocation_count; i++) {
		uint32_t offset = le32(w.request.source + table_offset + 4 * i) -
			w.facts.link_start;

		if (!span(w.facts.program.begin + offset, 4, &site) ||
		    overlaps(site, parameters))
			return NATIVE84_LOAD_PARAMETER_RELOCATION;
	}
	/* Fresh preflight and both genuine calls share one caller-qualified epoch. */
	if (!header_equal(w.request.source, w.facts.header))
		return NATIVE84_LOAD_HEADER_CHANGED;
	if (rmodule_parse((void *)w.request.source, &w.module))
		return NATIVE84_LOAD_PARSE_REJECTED;
	if (!module_state(&w.module, &w.facts, 0))
		return NATIVE84_LOAD_PARSED_STATE_REJECTED;
	if (!header_equal(w.request.source, w.facts.header))
		return NATIVE84_LOAD_HEADER_CHANGED;
	/* Point of no rollback: genuine copy, relocations, BSS and loaded hooks. */
	if (rmodule_load((void *)(uintptr_t)w.facts.program.begin, &w.module))
		return NATIVE84_LOAD_ATTEMPT_FAILED;
	if (!header_equal(w.request.source, w.facts.header) ||
	    !module_state(&w.module, &w.facts, 1))
		return NATIVE84_LOAD_POSTLOAD_STATE_FAILED;
	/* Never inspect an in-place payload or cleared relocation table here. */
	attached = (struct native84_handoff *)(uintptr_t)w.facts.parameters.begin;
	*attached = w.descriptor;
	stored = (const volatile uint8_t *)attached;
	saved = (const uint8_t *)&w.descriptor;
	for (i = 0; i < sizeof(w.descriptor); i++) {
		if (stored[i] != saved[i])
			return NATIVE84_LOAD_DESCRIPTOR_COPY_FAILED;
	}
	w.result.epoch = w.layout.epoch;
	w.result.entry = w.facts.entry;
	w.result.parameters = w.facts.parameters.begin;
	w.result.cbmem_top = w.descriptor.cbmem_top;
	w.result.allocation = w.request.allocation;
	w.result.program.begin = w.facts.program.begin;
	w.result.program.end = w.facts.program.end;
	*output = w.result;
	return NATIVE84_LOAD_VALID;
}
