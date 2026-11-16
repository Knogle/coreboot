/* SPDX-License-Identifier: GPL-2.0-only */

#include "pcc_contract.h"
#include <string.h>

_Static_assert(sizeof(struct b565d4_pcc_gas) == 12, "ACPI GAS size");
_Static_assert(sizeof(struct b565d4_pcc_subspace) == 62, "PCCT type0 size");
_Static_assert(offsetof(struct b565d4_pcc_subspace, base) == 8, "PCCT base");
_Static_assert(offsetof(struct b565d4_pcc_subspace, doorbell) == 24, "PCCT GAS");
_Static_assert(offsetof(struct b565d4_pcc_subspace, preserve) == 36, "PCCT preserve");

bool b565d4_pcc_plan(uint64_t begin, uint64_t end, unsigned int bits,
		    unsigned int cpus, struct b565d4_pcc_plan *out)
{
	struct b565d4_pcc_plan result = {0};
	uint64_t base;

	if (!out || bits < 32 || bits > 52 || !cpus || cpus > B565D4_PCC_MAX_CPUS ||
	    begin >= end || end > (1ULL << 32) || begin > UINT64_MAX - 0xffff)
		return false;
	base = (begin + 0xffff) & ~0xffffULL;
	if (base < 0x100000 || base > end || B565D4_PCC_BYTES > end - base)
		return false;
	result.owner_begin = begin;
	result.owner_end = end;
	result.shared_begin = base;
	result.shared_end = base + B565D4_PCC_BYTES;
	result.uc_base = base;
	result.uc_mask = (((1ULL << bits) - 1) & ~0xffffULL) | 0x800;
	result.cpu_count = cpus;
	*out = result;
	return true;
}

bool b565d4_pcc_subspace(uint64_t base, struct b565d4_pcc_subspace *out)
{
	/* Original B565 SMU384c GetInfo descriptor, independently matched to the
	 * factory5600X PCCT. These values describe its service, not guessed timing.
	 */
	const struct b565d4_pcc_subspace result = {
		.type = 0, .length = 62, .base = base, .bytes = B565D4_PCC_BYTES,
		.doorbell = { .space = 0, .width = 64, .offset = 0,
			.access = 4, .address = 0xfd010540 },
		.preserve = 0xffffffff00000000ULL, .write = 1,
		.latency_us = 4000, .maximum_rate_per_minute = 60000,
	};

	if (!out || base < 0x100000 || (base & 0xffff) ||
	    base > (1ULL << 32) - B565D4_PCC_BYTES)
		return false;
	*out = result;
	return true;
}

bool b565d4_cppc_package(uint32_t information, unsigned int slot,
			unsigned int lowest, unsigned int nominal,
			struct b565d4_cppc_package *out)
{
	static const uint8_t offsets[19] = {
		0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 56,
		64, 68, 72, 76, 80, 84,
	};
	static const unsigned int counts[] = {0, 15, 19, 21};
	struct b565d4_cppc_package result = {0};
	const unsigned int revision = information & 3;
	const uint32_t bitmap = information >> 2;
	unsigned int count;

	if (!out || !revision || slot >= B565D4_PCC_MAX_CPUS)
		return false;
	count = counts[revision];
	if (bitmap & ~((1U << count) - 1))
		return false;
	if (revision == 3 && (((bitmap & (1U << 19)) && !lowest) ||
	    ((bitmap & (1U << 20)) && !nominal) ||
	    ((bitmap & (3U << 19)) == (3U << 19) && lowest > nominal)))
		return false;
	result.revision = revision;
	result.field_count = count;
	for (unsigned int i = 0; i < count; i++) {
		struct b565d4_cppc_field *field = &result.fields[i];

		if (!(bitmap & (1U << i)))
			continue;
		if (i >= 19) {
			field->integer = true;
			field->value = i == 19 ? lowest : nominal;
			continue;
		}
		field->reg.space = 0x0a;
		field->reg.width = i >= 10 && i <= 12 ? 64 : 32;
		field->reg.address = (uint64_t)slot * B565D4_PCC_CPU_STRIDE + offsets[i];
		/* PCC GAS access is the subspace index, zero here. The original
		 * descriptor uses architectural MPERF/APERF when GetInfo bit16 is set.
		 */
		if ((information & (1U << 16)) && (i == 11 || i == 12)) {
			field->reg.space = 0x7f;
			field->reg.access = 4;
			field->reg.address = i == 11 ? 0xe7 : 0xe8;
		}
	}
	*out = result;
	return true;
}

static uint32_t le32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
		(uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

bool b565d4_pcc_reply(const uint8_t *snapshot, size_t bytes, unsigned int cpus)
{
	uint16_t status;

	if (!snapshot || !cpus || cpus > B565D4_PCC_MAX_CPUS ||
	    bytes < 8 + (cpus - 1) * B565D4_PCC_CPU_STRIDE + B565D4_PCC_CPU_DATA_BYTES)
		return false;
	status = (uint16_t)snapshot[6] | (uint16_t)snapshot[7] << 8;
	if (le32(snapshot) != 0x50434300 || snapshot[4] || snapshot[5] || status != 1)
		return false;
	for (unsigned int cpu = 0; cpu < cpus; cpu++) {
		const uint8_t *p = snapshot + 8 + cpu * B565D4_PCC_CPU_STRIDE;
		const uint32_t highest = le32(p), nominal = le32(p + 4);
		const uint32_t nonlinear = le32(p + 8), lowest = le32(p + 12);

		if (!lowest || lowest > nonlinear || nonlinear > nominal || nominal > highest)
			return false;
	}
	return true;
}
