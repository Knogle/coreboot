/* SPDX-License-Identifier: GPL-2.0-only */

#include <limits.h>
#include <amdblocks/matisse_dxio_init_plan.h>

#define FEATURE_MASK ((1U << 3) | (1U << 4) | (1U << 5) | \
		      (1U << 7) | (1U << 8) | (1U << 9))

static bool valid_range(const void *pointer, size_t bytes)
{
	return pointer && (uintptr_t)pointer <= UINTPTR_MAX - bytes;
}

static bool overlap(uint64_t first, uint64_t first_bytes,
		    uint64_t second, uint64_t second_bytes)
{
	return first < second + second_bytes && second < first + first_bytes;
}

static bool valid_physical_buffer(uint64_t address, uint32_t dwords)
{
	return address && !(address & 3) && address < (1ULL << 32) &&
		(uint64_t)dwords * 4 <= (1ULL << 32) - address;
}

static void append(struct matisse_dxio_init_request **next, uint32_t opcode,
		   uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e)
{
	*(*next)++ = (struct matisse_dxio_init_request) {
		.opcode = opcode,
		.arguments = { a, b, c, d, e },
	};
}

static void property(struct matisse_dxio_init_request **next,
		     uint32_t number, uint32_t value)
{
	append(next, 0x22, number, value, 0, 0, 0);
}

size_t matisse_build_dxio_init_requests(
	const struct matisse_dxio_init_policy *policy,
	const struct matisse_dxio_init_buffers *buffers,
	struct matisse_dxio_init_request *output, size_t capacity)
{
	static const uint8_t optional_features[] = { 3, 4, 5, 7, 8 };
	struct matisse_dxio_init_request *next = output;
	size_t count = 15, bytes;
	bool ancillary;

	if (!valid_range(policy, sizeof(*policy)) ||
	    !valid_range(buffers, sizeof(*buffers)) ||
	    (policy->features & ~FEATURE_MASK) ||
	    buffers->platform_dwords < 16 ||
	    (buffers->platform_dwords - 6) % 10 ||
	    (buffers->platform_dwords - 6) / 10 > UINT16_MAX ||
	    !valid_physical_buffer(buffers->platform, buffers->platform_dwords))
		return 0;
	ancillary = buffers->ancillary_dwords != 0;
	if (ancillary) {
		if (buffers->ancillary_dwords < 3 || buffers->ancillary_dwords > 256 ||
		    !valid_physical_buffer(buffers->ancillary, buffers->ancillary_dwords) ||
		    overlap(buffers->platform, (uint64_t)buffers->platform_dwords * 4,
			    buffers->ancillary, (uint64_t)buffers->ancillary_dwords * 4))
			return 0;
	} else if (buffers->ancillary) {
		return 0;
	}
	count += (size_t)policy->select_enabled + policy->property11 + policy->property12 +
		(policy->property13 != UINT8_MAX) + ancillary + policy->tuning_enabled;
	for (size_t i = 0; i < sizeof(optional_features); i++)
		count += !!(policy->features & (1U << optional_features[i]));
	bytes = count * sizeof(*output);
	if (count > MATISSE_DXIO_MAX_INIT_REQUESTS || capacity < count ||
	    !valid_range(output, bytes) ||
	    overlap((uintptr_t)output, bytes, (uintptr_t)policy, sizeof(*policy)) ||
	    overlap((uintptr_t)output, bytes, (uintptr_t)buffers, sizeof(*buffers)))
		return 0;

	if (policy->select_enabled)
		append(&next, 0x2d, policy->select_value, 0, 0, 0, 0);
	append(&next, 0x13, 0, 0, 0, 0, 0);
	append(&next, 0, 0, 0, 0, 0, 0);
	if (policy->property11)
		property(&next, 0x11, 1);
	property(&next, 0x0e, 1);
	property(&next, 0x0f, 1);
	property(&next, 0x10, 1);
	if (policy->property12)
		property(&next, 0x12, 1);
	property(&next, 0x1f, policy->property1f);
	property(&next, 0x14, policy->property14);
	property(&next, 0x0d, policy->property0d);
	if (policy->property13 != UINT8_MAX)
		property(&next, 0x13, policy->property13);
	append(&next, 0x24, 0, 0, 0, 0, 0);
	if (ancillary)
		append(&next, 0x23, 0, (uint32_t)buffers->ancillary,
		       buffers->ancillary_dwords, 0, 6);
	append(&next, 0x23, 0, 0, 0, 1, 2);
	append(&next, 0x23, 0, 0, 0, 1, 5);
	append(&next, 0x23, 0, (uint32_t)buffers->platform,
	       buffers->platform_dwords, 0, 3);
	if (policy->tuning_enabled)
		append(&next, 0x3a, 1, 2, policy->tuning[0],
		       policy->tuning[1], policy->tuning[2]);
	for (size_t i = 0; i < sizeof(optional_features); i++) {
		if (policy->features & (1U << optional_features[i]))
			append(&next, 0x3a, 1, 0, optional_features[i],
			       optional_features[i] != 7, 0);
	}
	append(&next, 0x3a, 1, 0, 9, !!(policy->features & (1U << 9)), 0);
	property(&next, 0x23, policy->property23);
	append(&next, 0x307, 0, 0, 0, 0, 0);
	return count;
}
