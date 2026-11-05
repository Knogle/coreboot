/* SPDX-License-Identifier: GPL-2.0-only */
#include "native84_handoff.h"

_Static_assert(sizeof(struct native84_handoff) == NATIVE84_HANDOFF_BYTES,
	"Native84 handoff ABI");

static int extent(uint32_t begin, uint32_t bytes, struct native84_interval *out)
{
	if (!bytes || bytes > UINT32_MAX - begin)
		return 0;
	out->begin = begin;
	out->end = begin + bytes;
	return 1;
}

static int valid_interval(struct native84_interval interval)
{
	return interval.begin < interval.end;
}

static int contains(struct native84_interval outer, struct native84_interval inner)
{
	return outer.begin <= inner.begin && inner.end <= outer.end;
}

static int overlaps(struct native84_interval a, struct native84_interval b)
{
	return a.begin < b.end && b.begin < a.end;
}

/* Actual input/output object addresses are additional protected objects. */
static int object_avoids_bank(const void *object, size_t bytes,
	struct native84_interval bank)
{
	uintptr_t begin = (uintptr_t)object;

	if (!bytes || bytes > UINTPTR_MAX - begin)
		return 0;
	return begin >= bank.end || bank.begin >= begin + bytes;
}

static int separate_objects(const void *a, size_t a_bytes, const void *b, size_t b_bytes)
{
	uintptr_t a_begin = (uintptr_t)a;
	uintptr_t b_begin = (uintptr_t)b;

	if (!a_bytes || !b_bytes || a_bytes > UINTPTR_MAX - a_begin ||
	    b_bytes > UINTPTR_MAX - b_begin)
		return 0;
	return a_begin >= b_begin + b_bytes || b_begin >= a_begin + a_bytes;
}

enum native84_validation_result native84_validate_handoff(
	const struct native84_handoff *handoff,
	const struct native84_validation_request *request,
	struct native84_layout *layout)
{
	struct native84_layout candidate;
	size_t protected_bytes;
	size_t i;

	if (!handoff || !request || !layout || !request->protected ||
	    !request->protected_count || request->protected_count > NATIVE84_MAX_PROTECTED)
		return NATIVE84_INVALID_ARGUMENT;
	protected_bytes = request->protected_count * sizeof(*request->protected);
	if (!separate_objects(layout, sizeof(*layout), handoff, sizeof(*handoff)) ||
	    !separate_objects(layout, sizeof(*layout), request, sizeof(*request)) ||
	    !separate_objects(layout, sizeof(*layout), request->protected, protected_bytes))
		return NATIVE84_INVALID_ARGUMENT;
	if (handoff->magic != NATIVE84_HANDOFF_MAGIC ||
	    handoff->version != NATIVE84_HANDOFF_VERSION ||
	    handoff->bytes != NATIVE84_HANDOFF_BYTES || !request->expected_epoch ||
	    handoff->epoch != request->expected_epoch)
		return NATIVE84_INVALID_PROTOCOL;
	if (!handoff->parent_base || handoff->parent_bytes != NATIVE84_PARENT_BYTES ||
	    !extent(handoff->parent_base, handoff->parent_bytes, &candidate.parent) ||
	    !extent(handoff->early_base, handoff->early_bytes, &candidate.early) ||
	    !extent(handoff->media_base, handoff->media_bytes, &candidate.media) ||
	    !extent(handoff->handle_bank_base, handoff->handle_bank_bytes,
		&candidate.handle_bank))
		return NATIVE84_INVALID_EXTENT;
	candidate.cbmem.begin = handoff->cbmem_bottom;
	candidate.cbmem.end = handoff->cbmem_top;
	if (!valid_interval(candidate.cbmem) ||
	    (candidate.parent.begin & 4095) || (candidate.parent.end & 4095) ||
	    (candidate.cbmem.begin & 4095) || (candidate.cbmem.end & 4095) ||
	    candidate.cbmem.end - candidate.cbmem.begin < 8192 ||
	    candidate.cbmem.end - candidate.cbmem.begin > INT32_MAX ||
	    (candidate.cbmem.begin > INT32_MAX) != (candidate.cbmem.end > INT32_MAX))
		return NATIVE84_INVALID_EXTENT;
	if (!contains(candidate.parent, candidate.early) ||
	    !contains(candidate.parent, candidate.media) ||
	    !contains(candidate.parent, candidate.cbmem) ||
	    overlaps(candidate.early, candidate.media) ||
	    overlaps(candidate.early, candidate.cbmem) ||
	    overlaps(candidate.media, candidate.cbmem))
		return NATIVE84_INVALID_PARTITION;
	if (request->emitted_handle_bytes != NATIVE84_IA32_HANDLE_BYTES ||
	    (candidate.handle_bank.begin & 3) ||
	    handoff->handle_bank_bytes < 2 * NATIVE84_IA32_HANDLE_BYTES ||
	    handoff->handle_bank_bytes > INT32_MAX ||
	    (candidate.handle_bank.begin > INT32_MAX) !=
		(candidate.handle_bank.end > INT32_MAX) ||
	    !contains(candidate.early, candidate.handle_bank) ||
	    overlaps(candidate.handle_bank, candidate.cbmem) ||
	    overlaps(candidate.handle_bank, candidate.media))
		return NATIVE84_INVALID_HANDLE_BANK;
	if (!object_avoids_bank(handoff, sizeof(*handoff), candidate.handle_bank) ||
	    !object_avoids_bank(request, sizeof(*request), candidate.handle_bank) ||
	    !object_avoids_bank(layout, sizeof(*layout), candidate.handle_bank) ||
	    !object_avoids_bank(request->protected, protected_bytes, candidate.handle_bank))
		return NATIVE84_PROTECTED_OVERLAP;
	for (i = 0; i < request->protected_count; i++) {
		if (!valid_interval(request->protected[i]))
			return NATIVE84_INVALID_EXTENT;
		if (overlaps(candidate.handle_bank, request->protected[i]))
			return NATIVE84_PROTECTED_OVERLAP;
	}
	candidate.epoch = handoff->epoch;
	candidate.creator_handle = candidate.handle_bank.begin;
	candidate.recovery_handle = candidate.handle_bank.begin + NATIVE84_IA32_HANDLE_BYTES;
	*layout = candidate;
	return NATIVE84_VALID;
}
