/* SPDX-License-Identifier: GPL-2.0-only */
#include "native99_apob_select.h"

#if !defined(__i386__)
#error "Native99 proposal uses genuine IA32 pointers"
#endif

_Static_assert(sizeof(void *) == 4 && sizeof(uintptr_t) == 4 &&
	sizeof(struct native99_apob_selection) == 40,
	"Native99 small own-result ABI");

static uint32_t le32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
		(uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint64_t le64(const uint8_t *p)
{
	return le32(p) | (uint64_t)le32(p + 4) << 32;
}

static int overlap(uint64_t a, uint64_t z, uint64_t b, uint64_t e)
{
	return a < e && b < z;
}

int native99_apob_inspect(const uint8_t *snapshot, uint32_t bytes,
	uint32_t physical_bits, struct native99_apob_selection *result)
{
	struct native99_apob_selection candidate = { 0 };
	uint32_t offset, map = 0, maps = 0;
	uint64_t limit, result_end, snapshot_end;
	uint64_t out = (uintptr_t)result, source = (uintptr_t)snapshot;

	if (!snapshot || !result || bytes < NATIVE99_APOB_MIN_BYTES || bytes > NATIVE99_APOB_MAX_BYTES ||
	    physical_bits < 17 || physical_bits > 52)
		return -1;
	result_end = out + sizeof(*result);
	snapshot_end = source + bytes;
	if (result_end > 0x100000000ULL || snapshot_end > 0x100000000ULL ||
	    (out & 3) || overlap(out, result_end, source, snapshot_end))
		return -1;
	limit = 1ULL << physical_bits;
	/* Captured versions 23 and 24 share this 9/9/0 memory-map layout.
	 * Use the bounded first-record offset from the header; its other
	 * extension fields remain opaque and grant no memory ownership.
	 */
	offset = le32(snapshot + 12);
	if (le32(snapshot) != 0x424f5041U ||
	    (le32(snapshot + 4) != 23 && le32(snapshot + 4) != 24) ||
	    le32(snapshot + 8) != bytes || offset < 16 ||
	    offset > bytes - 48 || (offset & 3))
		return -1;
	/* Bound every record by the captured container's exact length.
	 * Minimum48B progress bounds the walk; no fixed count or alignment
	 * predicate (actual records include sizes134h/14ch). Opaque fields stay
	 * opaque. The unique non-V2 supplier map contract remains unchanged.
	 */
	while (offset < bytes) {
		uint32_t length;

		if (offset > bytes || bytes - offset < 48)
			return -1;
		length = le32(snapshot + offset + 12);
		if (length < 48 || length > bytes - offset)
			return -1;
		if (le32(snapshot + offset) == 9 &&
		    le32(snapshot + offset + 4) == 9 &&
		    le32(snapshot + offset + 8) == 0) {
			if (++maps != 1 || length != 0x1f0)
				return -1;
			map = offset;
		}
		offset += length;
		candidate.records++;
	}
	if (offset != bytes || maps != 1)
		return -1;
	candidate.apob_bytes = bytes;
	candidate.map_offset = map;
	candidate.top = le64(snapshot + map + 48);
	candidate.hole_count = le32(snapshot + map + 56);
	if (!candidate.top || candidate.top > limit || (candidate.top & 4095) ||
	    candidate.top < NATIVE99_PARENT_END ||
	    candidate.hole_count > NATIVE99_APOB_MAX_HOLES)
		return -1;
	candidate.available_end = candidate.top;
	for (uint32_t i = 0; i < candidate.hole_count; i++) {
		const uint8_t *hole = snapshot + map + 64 + i * 24;
		uint64_t base = le64(hole), length = le64(hole + 8);
		uint64_t end;

		if (!length || base > candidate.top || length > candidate.top - base ||
		    ((base | length) & 4095))
			return -1;
		end = base + length;
		/* Every supplier hole stays unavailable, including raw0 UMA.
		 * Record order does not change ownership. Reject overlapping holes
		 * and find the nearest boundaries around the complete workspace.
		 */
		if (overlap(base, end, NATIVE99_PARENT_BEGIN, NATIVE99_PARENT_END))
			return -1;
		for (uint32_t j = 0; j < i; j++) {
			const uint8_t *previous = snapshot + map + 64 + j * 24;
			uint64_t previous_base = le64(previous);

			if (overlap(base, end, previous_base,
				previous_base + le64(previous + 8)))
				return -1;
		}
		if (end <= NATIVE99_PARENT_BEGIN && end > candidate.available_begin)
			candidate.available_begin = end;
		if (base >= NATIVE99_PARENT_END && base < candidate.available_end)
			candidate.available_end = base;
	}
	/* Every record and every active hole passed.
	 * Never silently clip, choose another address, or fall back to TOM.
	 */
	*result = candidate;
	return 0;
}

int native99_apob_select(const uint8_t *snapshot, uint32_t bytes,
	uint32_t physical_bits, const struct native99_span *excluded,
	uint32_t excluded_count, struct native99_apob_selection *result)
{
	uint64_t out = (uintptr_t)result, source = (uintptr_t)snapshot;
	uint64_t list = (uintptr_t)excluded;
	uint64_t result_end, snapshot_end, excluded_end, limit;

	if (!snapshot || !result || !excluded || !excluded_count ||
	    excluded_count > 32 || bytes < NATIVE99_APOB_MIN_BYTES || bytes > NATIVE99_APOB_MAX_BYTES ||
	    physical_bits < 17 || physical_bits > 52)
		return -1;
	result_end = out + sizeof(*result);
	snapshot_end = source + bytes;
	excluded_end = list + excluded_count * sizeof(*excluded);
	if (result_end > 0x100000000ULL || snapshot_end > 0x100000000ULL ||
	    excluded_end > 0x100000000ULL || (out & 3) || (list & 3) ||
	    overlap(out, result_end, source, snapshot_end) ||
	    overlap(out, result_end, list, excluded_end) ||
	    overlap(out, result_end, NATIVE99_PARENT_BEGIN, NATIVE99_PARENT_END) ||
	    overlap(source, snapshot_end, NATIVE99_PARENT_BEGIN, NATIVE99_PARENT_END) ||
	    overlap(list, excluded_end, NATIVE99_PARENT_BEGIN, NATIVE99_PARENT_END))
		return -1;
	limit = 1ULL << physical_bits;
	for (uint32_t i = 0; i < excluded_count; i++) {
		if (excluded[i].begin >= excluded[i].end || excluded[i].end > limit ||
		    overlap(excluded[i].begin, excluded[i].end,
			NATIVE99_PARENT_BEGIN, NATIVE99_PARENT_END))
			return -1;
	}
	return native99_apob_inspect(snapshot, bytes, physical_bits, result);
}
