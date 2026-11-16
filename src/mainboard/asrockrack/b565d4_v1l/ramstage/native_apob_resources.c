/* SPDX-License-Identifier: GPL-2.0-only */
#include "native_apob_resources.h"
#include "native84_profile.h"
#include "native99_apob_packet.h"
#include "native99_apob_select.h"
#include <commonlib/helpers.h>
#include <arch/cpu.h>
#include <cbmem.h>
#include <device/device.h>
#include <device/resource.h>
#include <limits.h>
#include <memrange.h>

enum {
	MAX_PACKET_BYTES = NATIVE99_PACKET_HEADER_BYTES + NATIVE99_APOB_MAX_BYTES,
	MAX_PLAN = 64,
};
_Static_assert(sizeof(void *) == 4 &&
	sizeof(struct native99_apob_packet) == NATIVE99_PACKET_HEADER_BYTES &&
	NATIVE99_PACKET_END - NATIVE99_PACKET_BASE >= MAX_PACKET_BYTES &&
	NATIVE84_A == NATIVE99_PARENT_BEGIN &&
	NATIVE84_PARENT_END == NATIVE99_PARENT_END, "Native99 later packet ABI");

static const struct native99_span protected[] = {
	{ 0, 0x00001000 }, /* IVT/BDA and the low LBIO forwarder. */
	{ 0x000a0000, 0x00100000 }, /* Legacy VGA/BIOS window. */
	{ 0x09000000, 0x0b100000 }, /* Actual reset/native images and early state. */
	{ 0x0a200000, 0x0a228000 }, /* Whole original APOB backing. */
	{ 0xbe000000, 0xbf000000 }, /* Conservative retained reservation. */
	{ NATIVE99_PARENT_BEGIN, NATIVE99_PARENT_END },
};
struct planned_range { uint64_t begin, end; unsigned int ram; };
/* Serialized once-only provider workspace, not a large automatic stack frame. */
static struct range_entry range_entries[MAX_PLAN];
static struct planned_range plan[MAX_PLAN];
static unsigned int plan_count;
static const uint8_t *snapshot;
static uint32_t snapshot_physical_bits, snapshot_crc32;
static struct native99_apob_selection snapshot_shape;
static unsigned int capture_attempted, publication_attempted;

static uint32_t snapshot_bytes(void)
{
	return NATIVE99_PACKET_HEADER_BYTES + snapshot_shape.apob_bytes;
}

static uint32_t le32(const volatile uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
		(uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint64_t le64(const uint8_t *p)
{
	return le32(p) | (uint64_t)le32(p + 4) << 32;
}

/* Same CPU-observed private packet bytes, not PSP cache-coherency proof. */
static uint32_t packet_crc32(const uint8_t *p, uint32_t bytes)
{
	uint32_t value = ~0U;

	for (uint32_t i = 0; i < bytes; i++) {
		value ^= p[i];
		for (unsigned int j = 0; j < 8; j++)
			value = (value >> 1) ^ (0xedb88320U & -(value & 1U));
	}
	return ~value;
}

static uint32_t physical_bits(void)
{
	if (cpuid(0x80000000).eax < 0x80000008)
		return 0;
	return cpuid(0x80000008).eax & 0xff;
}

static int same_shape(const struct native99_apob_selection *a,
	const struct native99_apob_selection *b)
{
	return a->apob_bytes == b->apob_bytes && a->records == b->records &&
		a->map_offset == b->map_offset && a->hole_count == b->hole_count &&
		a->top == b->top && a->available_begin == b->available_begin &&
		a->available_end == b->available_end;
}

static enum native_apob_status inspect_packet(const volatile uint8_t *p,
	uint32_t parent, uint32_t bytes, uint32_t bits, uint32_t apob_bytes,
	struct native99_apob_selection *shape)
{
	struct b565d4_smm_regions smm;

	if (apob_bytes < NATIVE99_APOB_MIN_BYTES ||
	    apob_bytes > NATIVE99_APOB_MAX_BYTES ||
	    le32(p) != NATIVE99_PACKET_MAGIC || le32(p + 4) != NATIVE99_PACKET_VERSION ||
	    le32(p + 8) != NATIVE99_PACKET_HEADER_BYTES || le32(p + 12) != NATIVE84_EPOCH ||
	    le32(p + 16) != parent || le32(p + 20) != bytes ||
	    le32(p + 24) != apob_bytes || le32(p + 28))
		return NATIVE_APOB_BAD_PACKET;
	if (!b565d4_decode_smm_regions(le64((const uint8_t *)p + 32),
		le64((const uint8_t *)p + 40), le64((const uint8_t *)p + 48),
		le64((const uint8_t *)p + 56), bits, &smm))
		return NATIVE_APOB_BAD_PACKET;
	/* This is the pure inspector, not EARLY's external-exclusion wrapper.
	 * The immutable packet, own CBMEM copy and result may be inside parent.
	 */
	if (native99_apob_inspect((const uint8_t *)p + NATIVE99_PACKET_HEADER_BYTES,
		apob_bytes, bits, shape))
		return NATIVE_APOB_BAD_MAP;
	return NATIVE_APOB_OK;
}

static int own_entry(const struct cbmem_entry *entry, const void *copy,
	const struct native84_handoff *d, uint32_t packet_bytes)
{
	uint64_t start = (uintptr_t)copy, bytes;

	if (!entry || cbmem_entry_start(entry) != copy)
		return 0;
	bytes = cbmem_entry_size(entry);
	return start >= d->cbmem_bottom && start < d->cbmem_top &&
		bytes >= packet_bytes && bytes <= (uint64_t)d->cbmem_top - start &&
		(start + bytes <= NATIVE99_PACKET_BASE || start >= NATIVE99_PACKET_END);
}

enum native_apob_status native_apob_capture(uint32_t parent, uint32_t bytes)
{
	const volatile uint8_t *live = (void *)(uintptr_t)NATIVE99_PACKET_BASE;
	const struct native84_handoff *d;
	const struct cbmem_entry *entry;
	struct native99_apob_selection before, after;
	enum native_apob_status status;
	uint32_t bits, apob_bytes, packet_bytes;
	struct b565d4_smm_state captured_smm, fresh_smm;
	uint8_t *copy;

	if (capture_attempted)
		return NATIVE_APOB_ALREADY_ATTEMPTED;
	capture_attempted = 1;
	d = native84_descriptor();
	if (parent != NATIVE99_PARENT_BEGIN || bytes != NATIVE84_R ||
	    parent != d->parent_base || bytes != d->parent_bytes ||
	    d->cbmem_bottom != NATIVE84_CBMEM_BEGIN ||
	    d->cbmem_top != NATIVE99_PARENT_END || !cbmem_online())
		return NATIVE_APOB_BAD_ARGUMENT;
	bits = physical_bits();
	/* The complete fixed header is inside the owned packet reservation.
	 * Bind its length once; every inspection requires this exact captured value
	 * before following payload bytes, including after the CBMEM copy.
	 */
	apob_bytes = le32(live + 24);
	if (apob_bytes < NATIVE99_APOB_MIN_BYTES ||
	    apob_bytes > NATIVE99_APOB_MAX_BYTES)
		return NATIVE_APOB_BAD_PACKET;
	packet_bytes = NATIVE99_PACKET_HEADER_BYTES + apob_bytes;
	status = inspect_packet(live, parent, bytes, bits, apob_bytes, &before);
	if (status != NATIVE_APOB_OK)
		return status;
	captured_smm = ((const volatile struct native99_apob_packet *)live)->smm;
	if (!b565d4_read_smm_state(&fresh_smm) ||
	    !b565d4_same_smm_state(&captured_smm, &fresh_smm))
		return NATIVE_APOB_UNSTABLE;
	if (cbmem_entry_find(CBMEM_ID_NATIVE_APOB_SNAPSHOT))
		return NATIVE_APOB_ALREADY_ATTEMPTED;
	copy = cbmem_add(CBMEM_ID_NATIVE_APOB_SNAPSHOT, packet_bytes);
	entry = cbmem_entry_find(CBMEM_ID_NATIVE_APOB_SNAPSHOT);
	if (!copy || !own_entry(entry, copy, d, packet_bytes))
		return NATIVE_APOB_ALLOCATION_FAILED;
	for (uint32_t i = 0; i < packet_bytes; i++)
		copy[i] = live[i];
	status = inspect_packet(copy, parent, bytes, bits, apob_bytes, &after);
	if (status != NATIVE_APOB_OK || !same_shape(&before, &after))
		return NATIVE_APOB_UNSTABLE;
	for (uint32_t i = 0; i < packet_bytes; i++)
		if (copy[i] != live[i])
			return NATIVE_APOB_UNSTABLE;
	if (!b565d4_same_smm_state(&((const struct native99_apob_packet *)copy)->smm,
		&captured_smm) || !b565d4_read_smm_state(&fresh_smm) ||
	    !b565d4_same_smm_state(&captured_smm, &fresh_smm))
		return NATIVE_APOB_UNSTABLE;
	snapshot_crc32 = packet_crc32(copy, packet_bytes);
	snapshot_physical_bits = bits;
	snapshot_shape = after;
	snapshot = copy;
	return NATIVE_APOB_OK;
}

static int add_plan(uint64_t begin, uint64_t end, unsigned int ram)
{
	if (begin >= end || ((begin | end) & 4095) || plan_count == MAX_PLAN)
		return -1;
	plan[plan_count++] = (struct planned_range) { begin, end, ram };
	return 0;
}

static int prepare_plan(const struct native99_apob_selection *shape)
{
	const struct native99_apob_packet *packet = (const void *)snapshot;
	const uint8_t *map = snapshot + NATIVE99_PACKET_HEADER_BYTES + shape->map_offset;
	struct b565d4_smm_regions smm;
	struct memranges ranges;
	struct range_entry *entry;
	uint64_t smbase_begin, smbase_end;

	/* Each insertion/removal can add at most two entries. This static pool
	 * covers all 18 supplier holes and every explicit firmware exclusion.
	 * No allocator or supplier record ordering is needed to build the map.
	 */
	_Static_assert(MAX_PLAN >= 1 + 2 * (NATIVE99_APOB_MAX_HOLES +
		ARRAY_SIZE(protected) + 2), "Complete range pool");
	if (!b565d4_decode_smm_regions(packet->smm.smbase, packet->smm.smm_addr,
		packet->smm.smm_mask, packet->smm.hwcr, snapshot_physical_bits, &smm) ||
	    smm.smbase_end > shape->top ||
	    ((smm.flags & B565D4_SMM_TSEG_VALID) && smm.tseg_end > shape->top))
		return -1;
	plan_count = 0;
	memranges_init_empty(&ranges, range_entries, MAX_PLAN);
	memranges_insert(&ranges, 0, shape->top, 1);
	for (uint32_t i = 0; i < shape->hole_count; i++) {
		const uint8_t *hole = map + 64 + i * 24;

		/* UMA/raw0 and all non-PCI supplier classes stay reserved. */
		if (le32(hole + 16) != 1)
			memranges_insert(&ranges, le64(hole), le64(hole + 8), 0);
	}
	for (unsigned int i = 0; i < ARRAY_SIZE(protected); i++) {
		if (protected[i].end > shape->top)
			return -1;
		memranges_insert(&ranges, protected[i].begin,
			protected[i].end - protected[i].begin, 0);
	}
	/* These intervals came from the immutable early snapshot. Round the
	 * SMBASE footprint outwards to pages; TSEG is already 128-KiB aligned.
	 * They may only remove usable RAM, never grant new memory.
	 */
	smbase_begin = smm.smbase_begin & ~4095ULL;
	smbase_end = (smm.smbase_end + 4095) & ~4095ULL;

	if (smbase_end > shape->top)
		return -1;
	memranges_insert(&ranges, smbase_begin, smbase_end - smbase_begin, 0);
	if (smm.flags & B565D4_SMM_TSEG_VALID)
		memranges_insert(&ranges, smm.tseg_begin, smm.tseg_end - smm.tseg_begin, 0);
	/* PCI windows remain absent from RAM resources, including intersections
	 * with conservative firmware exclusions. Their fixed MMIO is separate.
	 */
	for (uint32_t i = 0; i < shape->hole_count; i++) {
		const uint8_t *hole = map + 64 + i * 24;

		if (le32(hole + 16) == 1)
			memranges_create_hole(&ranges, le64(hole), le64(hole + 8));
	}
	memranges_each_entry(entry, &ranges) {
		if (add_plan(range_entry_base(entry), range_entry_end(entry),
			range_entry_tag(entry)))
			return -1;
	}
	return 0;
}

enum native_apob_status native_apob_publish_resources(struct device *domain,
	unsigned long first_index, unsigned long *next_index)
{
	const struct native84_handoff *d;
	struct native99_apob_selection shape;
	enum native_apob_status status;
	unsigned long index = first_index;

	if (!domain || domain->path.type != DEVICE_PATH_DOMAIN ||
	    domain->path.domain.domain_id || !next_index)
		return NATIVE_APOB_BAD_ARGUMENT;
	if (publication_attempted)
		return NATIVE_APOB_ALREADY_ATTEMPTED;
	publication_attempted = 1;
	d = native84_descriptor();
	if (!snapshot || physical_bits() != snapshot_physical_bits ||
	    !own_entry(cbmem_entry_find(CBMEM_ID_NATIVE_APOB_SNAPSHOT), snapshot, d,
		snapshot_bytes()))
		return NATIVE_APOB_NO_SNAPSHOT;
	if (packet_crc32(snapshot, snapshot_bytes()) != snapshot_crc32)
		return NATIVE_APOB_UNSTABLE;
	status = inspect_packet(snapshot, d->parent_base, d->parent_bytes,
		snapshot_physical_bits, snapshot_shape.apob_bytes, &shape);
	if (status != NATIVE_APOB_OK || !same_shape(&shape, &snapshot_shape) ||
	    packet_crc32(snapshot, snapshot_bytes()) != snapshot_crc32)
		return NATIVE_APOB_UNSTABLE;
	if (prepare_plan(&shape) || first_index > UINT_MAX ||
	    plan_count > UINT_MAX - first_index)
		return NATIVE_APOB_BAD_MAP;
	if (packet_crc32(snapshot, snapshot_bytes()) != snapshot_crc32)
		return NATIVE_APOB_UNSTABLE;
	for (unsigned int i = 0; i < plan_count; i++)
		if (probe_resource(domain, (unsigned int)(first_index + i)))
			return NATIVE_APOB_INDEX_COLLISION;
	for (unsigned int i = 0; i < plan_count; i++) {
		const struct resource *r = plan[i].ram ?
			ram_range(domain, index++, plan[i].begin, plan[i].end - plan[i].begin) :
			reserved_ram_range(domain, index++, plan[i].begin,
				plan[i].end - plan[i].begin);

		if (!r)
			return NATIVE_APOB_PUBLICATION_FAILED;
	}
	*next_index = index;
	return NATIVE_APOB_OK;
}


enum native_apob_status native_apob_require_pci_hole(uint64_t begin, uint64_t end)
{
	const struct native84_handoff *d;
	struct native99_apob_selection shape;
	enum native_apob_status status;
	const uint8_t *map;

	if (begin >= end || ((begin | end) & 0xffff))
		return NATIVE_APOB_BAD_ARGUMENT;
	d = native84_descriptor();
	if (!snapshot || physical_bits() != snapshot_physical_bits ||
	    !own_entry(cbmem_entry_find(CBMEM_ID_NATIVE_APOB_SNAPSHOT), snapshot, d,
		snapshot_bytes()))
		return NATIVE_APOB_NO_SNAPSHOT;
	if (packet_crc32(snapshot, snapshot_bytes()) != snapshot_crc32)
		return NATIVE_APOB_UNSTABLE;
	status = inspect_packet(snapshot, d->parent_base, d->parent_bytes,
		snapshot_physical_bits, snapshot_shape.apob_bytes, &shape);
	if (status != NATIVE_APOB_OK || !same_shape(&shape, &snapshot_shape) ||
	    packet_crc32(snapshot, snapshot_bytes()) != snapshot_crc32)
		return NATIVE_APOB_UNSTABLE;
	map = snapshot + NATIVE99_PACKET_HEADER_BYTES + shape.map_offset;
	for (uint32_t i = 0; i < shape.hole_count; i++) {
		const uint8_t *hole = map + 64 + i * 24;
		uint64_t base = le64(hole), bytes = le64(hole + 8);

		/* inspect_packet already checked nonzero, disjoint, bounded holes. */
		if (le32(hole + 16) == 1 && begin >= base && end <= base + bytes)
			return NATIVE_APOB_OK;
	}
	return NATIVE_APOB_BAD_MAP;
}


/* Read-only agreement with the already captured immutable own CBMEM map.
 * Revalidate the original length/shape and architectural physical-width bound;
 * never substitute TOM, a new live APOB or a fixed capacity on failure.
 */
enum native_apob_status native_apob_require_top(uint64_t top)
{
	const struct native84_handoff *d = native84_descriptor();
	struct native99_apob_selection shape;
	enum native_apob_status status;

	if (!snapshot || physical_bits() != snapshot_physical_bits ||
	    !own_entry(cbmem_entry_find(CBMEM_ID_NATIVE_APOB_SNAPSHOT), snapshot, d,
		snapshot_bytes()))
		return NATIVE_APOB_NO_SNAPSHOT;
	if (packet_crc32(snapshot, snapshot_bytes()) != snapshot_crc32)
		return NATIVE_APOB_UNSTABLE;
	status = inspect_packet(snapshot, d->parent_base, d->parent_bytes,
		snapshot_physical_bits, snapshot_shape.apob_bytes, &shape);
	if (status != NATIVE_APOB_OK || !same_shape(&shape, &snapshot_shape) ||
	    packet_crc32(snapshot, snapshot_bytes()) != snapshot_crc32)
		return NATIVE_APOB_UNSTABLE;
	return top == shape.top ? NATIVE_APOB_OK : NATIVE_APOB_BAD_MAP;
}

enum native_apob_status native_apob_get_snapshot(const uint8_t **apob, uint32_t *bytes)
{
	enum native_apob_status status;

	if (!apob || !bytes)
		return NATIVE_APOB_BAD_ARGUMENT;
	status = native_apob_require_top(snapshot_shape.top);
	if (status != NATIVE_APOB_OK)
		return status;
	*apob = snapshot + NATIVE99_PACKET_HEADER_BYTES;
	*bytes = snapshot_shape.apob_bytes;
	return NATIVE_APOB_OK;
}
