/* SPDX-License-Identifier: GPL-2.0-only */
#include "native84_rmodule_preflight.h"
#include <limits.h>
#include <stddef.h>

#if !defined(__i386__) || !defined(__BYTE_ORDER__) || !defined(__ORDER_LITTLE_ENDIAN__) || \
	__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "This leaf requires the separately qualified IA32 little-endian ABI"
#endif

_Static_assert(CHAR_BIT == 8 && sizeof(void *) == 4 && sizeof(uintptr_t) == 4 &&
	sizeof(size_t) == 4 && sizeof(int) == 4, "IA32 byte/pointer/int ABI");
_Static_assert(sizeof(struct native84_rmodule_request) == 36, "request ABI");
_Static_assert(sizeof(struct native84_rmodule_facts) == 144, "facts ABI");
_Static_assert(_Alignof(struct native84_rmodule_request) == 4 &&
	_Alignof(struct native84_rmodule_facts) == 4, "control alignment");

static int span(uint32_t begin, uint32_t bytes,
	struct native84_rmodule_interval *out)
{
	if (!begin || !bytes || bytes > UINT32_MAX - begin)
		return 0;
	out->begin = begin;
	out->end = begin + bytes;
	return 1;
}

static int overlaps(struct native84_rmodule_interval a,
	struct native84_rmodule_interval b)
{
	return a.begin < b.end && b.begin < a.end;
}

static uint32_t le32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
		(uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

enum native84_rmodule_result native84_rmodule_preflight(
	const struct native84_rmodule_request *request,
	struct native84_rmodule_facts *facts)
{
	struct native84_rmodule_request r;
	struct native84_rmodule_facts f;
	struct native84_rmodule_interval request_span, output_span, descriptor, list;
	uint32_t payload_begin, payload_end, reloc_begin, reloc_end;
	uint32_t link, program_bytes, payload_bytes, entry, params_begin, params_end;
	uint32_t bss_begin, bss_end, previous = 0;
	uint32_t i;

	/* Only caller-qualified control objects may be dereferenced here. */
	if (!request || !facts || ((uintptr_t)request & 3) || ((uintptr_t)facts & 3) ||
	    !span((uintptr_t)request, sizeof(*request), &request_span) ||
	    !span((uintptr_t)facts, sizeof(*facts), &output_span) ||
	    overlaps(request_span, output_span))
		return NATIVE84_RMODULE_BAD_ARGUMENT;
	r = *request;
	if (r.source_bytes < NATIVE84_RMODULE_HEADER_BYTES ||
	    r.source_bytes > INT32_MAX ||
	    !span((uintptr_t)r.source, r.source_bytes, &f.source))
		return NATIVE84_RMODULE_BAD_SOURCE;
	if (r.descriptor_bytes != NATIVE84_RMODULE_DESCRIPTOR_BYTES ||
	    ((uintptr_t)r.descriptor & 3) ||
	    !span((uintptr_t)r.descriptor, r.descriptor_bytes, &descriptor) ||
	    !r.protected || ((uintptr_t)r.protected & 3) ||
	    !r.protected_count || r.protected_count > NATIVE84_RMODULE_MAX_PROTECTED ||
	    !span((uintptr_t)r.protected, r.protected_count * sizeof(*r.protected), &list))
		return NATIVE84_RMODULE_BAD_ARGUMENT;
	if (overlaps(output_span, f.source) || overlaps(output_span, descriptor) ||
	    overlaps(output_span, list))
		return NATIVE84_RMODULE_BAD_ALIAS;

	/* All header fields are read from this bounded byte snapshot. */
	for (i = 0; i < NATIVE84_RMODULE_HEADER_BYTES; i++)
		f.header[i] = r.source[i];
	if (f.header[0] != 0xfe || f.header[1] != 0xf8 ||
	    f.header[2] != 1 || f.header[3] != 0)
		return NATIVE84_RMODULE_BAD_HEADER;
	for (i = 48; i < NATIVE84_RMODULE_HEADER_BYTES; i++) {
		if (f.header[i])
			return NATIVE84_RMODULE_BAD_HEADER;
	}
	payload_begin = le32(f.header + 4);
	payload_end = le32(f.header + 8);
	reloc_begin = le32(f.header + 12);
	reloc_end = le32(f.header + 16);
	if (payload_begin != NATIVE84_RMODULE_HEADER_BYTES ||
	    payload_end <= payload_begin || payload_end != reloc_begin ||
	    reloc_begin > reloc_end || reloc_end != r.source_bytes ||
	    ((reloc_end - reloc_begin) & 3))
		return NATIVE84_RMODULE_BAD_SERIALIZATION;
	payload_bytes = payload_end - payload_begin;
	if (payload_bytes > INT32_MAX)
		return NATIVE84_RMODULE_BAD_SERIALIZATION;
	f.relocations.begin = f.source.begin + reloc_begin;
	f.relocations.end = f.source.end;
	f.relocation_count = (reloc_end - reloc_begin) / 4;
	/* Stock forms a uintptr_t table pointer even when empty. Sites may unalign. */
	if (f.relocations.begin & 3)
		return NATIVE84_RMODULE_BAD_SERIALIZATION;

	link = le32(f.header + 20);
	program_bytes = le32(f.header + 24);
	if (!program_bytes || program_bytes > INT32_MAX ||
	    payload_bytes > program_bytes || program_bytes > UINT32_MAX - link ||
	    (r.destination & 4095) ||
	    !span(r.destination, program_bytes, &f.program) ||
	    !r.allocation.begin || r.allocation.begin >= r.allocation.end ||
	    r.destination < r.allocation.begin || f.program.end > r.allocation.end)
		return NATIVE84_RMODULE_BAD_PROGRAM;
	f.allocation = r.allocation;
	f.link_start = link;
	f.link_end = link + program_bytes;
	f.payload.begin = r.destination;
	f.payload.end = r.destination + payload_bytes;
	entry = le32(f.header + 28);
	if (entry < link || entry - link >= payload_bytes || entry - link > INT32_MAX)
		return NATIVE84_RMODULE_BAD_ENTRY;
	f.entry = r.destination + (entry - link);

	bss_begin = le32(f.header + 40);
	bss_end = le32(f.header + 44);
	if (bss_begin < link + payload_bytes || bss_begin > bss_end ||
	    bss_end > f.link_end || bss_end - bss_begin > INT32_MAX)
		return NATIVE84_RMODULE_BAD_BSS;
	f.bss.begin = r.destination + (bss_begin - link);
	f.bss.end = r.destination + (bss_end - link);
	params_begin = le32(f.header + 32);
	params_end = le32(f.header + 36);
	if (params_begin < link || params_begin >= params_end ||
	    params_end > link + payload_bytes ||
	    params_end - params_begin != NATIVE84_RMODULE_DESCRIPTOR_BYTES)
		return NATIVE84_RMODULE_BAD_PARAMETERS;
	f.parameters.begin = r.destination + (params_begin - link);
	f.parameters.end = r.destination + (params_end - link);
	if ((f.parameters.begin & 3) || overlaps(f.parameters, f.bss))
		return NATIVE84_RMODULE_BAD_PARAMETERS;

	/* Exclude controls for the entire program reservation, including NOLOAD. */
	if (overlaps(f.program, request_span) || overlaps(f.program, output_span) ||
	    overlaps(f.program, descriptor) || overlaps(f.program, list))
		return NATIVE84_RMODULE_BAD_ALIAS;
	for (i = 0; i < r.protected_count; i++) {
		struct native84_rmodule_interval p = r.protected[i];

		if (p.begin >= p.end)
			return NATIVE84_RMODULE_BAD_ARGUMENT;
		if (overlaps(f.program, p))
			return NATIVE84_RMODULE_PROTECTED_OVERLAP;
	}
	f.in_place = f.source.begin + NATIVE84_RMODULE_HEADER_BYTES == r.destination;
	if (f.in_place) {
		/*
		 * Header stays before all writes; table starts after all fixup sites.
		 * Table/BSS overlap is safe only for stock copy->relocate->clear order.
		 * Both the whole serialized source and full program need this backing.
		 */
		if (f.source.begin < r.allocation.begin || f.source.end > r.allocation.end)
			return NATIVE84_RMODULE_BAD_ALIAS;
	} else if (overlaps(f.source, f.program)) {
		return NATIVE84_RMODULE_BAD_ALIAS;
	}

	for (i = 0; i < f.relocation_count; i++) {
		uint32_t site = le32(r.source + reloc_begin + 4 * i);
		uint32_t offset;

		if (site < link || site - link > payload_bytes ||
		    payload_bytes - (site - link) < 4 ||
		    (i && (site < previous || site - previous < 4)))
			return NATIVE84_RMODULE_BAD_RELOCATION;
		offset = site - link;
		if (offset > f.program.end - r.destination ||
		    4 > f.program.end - r.destination - offset)
			return NATIVE84_RMODULE_BAD_RELOCATION;
		previous = site;
	}
	/*
	 * Values at the admitted sites are not constrained to [link, link_end).
	 * For every encoded uint32_t v, stock IA32 rebasing is
	 * (uint32_t)(v + (uint32_t)(destination - link)), including wraparound.
	 * This structural leaf checks sites only; future emitted-code identity
	 * must justify actual values and the stock pointer-arithmetic semantics.
	 */
	f.relocation_adjustment = (uint32_t)(r.destination - link);
	*facts = f;
	return NATIVE84_RMODULE_VALID;
}
