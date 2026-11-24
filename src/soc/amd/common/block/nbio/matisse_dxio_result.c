/* SPDX-License-Identifier: GPL-2.0-only */

#include <limits.h>
#include <string.h>
#include <amdblocks/matisse_dxio_result.h>

static bool valid_range(const void *pointer, size_t bytes)
{
	return (!bytes || pointer) && (uintptr_t)pointer <= UINTPTR_MAX - bytes;
}

static bool overlap(const void *first, size_t first_bytes,
		    const void *second, size_t second_bytes)
{
	return first_bytes && second_bytes &&
		(uintptr_t)first < (uintptr_t)second + second_bytes &&
		(uintptr_t)second < (uintptr_t)first + first_bytes;
}

static unsigned int map_slot(uint8_t encoding)
{
	return ((encoding & 15U) << 3) + (encoding >> 4);
}

static uint32_t read32(const uint8_t *bytes)
{
	return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
		((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

bool matisse_decode_dxio_result(const struct matisse_dxio_result_slot *slots,
			       size_t slot_count,
			       const struct matisse_dxio_port *ports,
			       size_t port_count, const uint8_t *frame,
			       size_t frame_bytes, uint8_t bus,
			       struct matisse_dxio_result *output)
{
	struct matisse_dxio_result decoded;
	size_t slots_bytes, ports_bytes, count;
	uint32_t present = 0, used = 0;

	if (slot_count > MATISSE_NBIF_MAP_SLOTS || port_count > UINT16_MAX ||
	    slot_count > SIZE_MAX / sizeof(*slots) ||
	    port_count > SIZE_MAX / sizeof(*ports))
		return false;
	slots_bytes = slot_count * sizeof(*slots);
	ports_bytes = port_count * sizeof(*ports);
	if (!output || !frame || frame_bytes < MATISSE_DXIO_PLATFORM_HEADER_BYTES ||
	    !valid_range(output, sizeof(*output)) || !valid_range(frame, frame_bytes) ||
	    !valid_range(slots, slots_bytes) || !valid_range(ports, ports_bytes) ||
	    overlap(output, sizeof(*output), frame, frame_bytes) ||
	    overlap(output, sizeof(*output), slots, slots_bytes) ||
	    overlap(output, sizeof(*output), ports, ports_bytes))
		return false;
	count = (size_t)frame[12] | ((size_t)frame[13] << 8);
	if (count > (frame_bytes - MATISSE_DXIO_PLATFORM_HEADER_BYTES) /
		    MATISSE_DXIO_ENGINE_BYTES)
		return false;
	for (size_t i = 0; i < slot_count; i++) {
		struct matisse_dxio_lane_assignment lanes;
		unsigned int slot = map_slot(slots[i].encoding);

		if (slot >= MATISSE_NBIF_MAP_SLOTS || (present & (1U << slot)) ||
		    !matisse_map_dxio_lanes(&slots[i].lanes,
					   slots[i].lanes.physical_first,
					   slots[i].lanes.physical_last, &lanes))
			return false;
		present |= 1U << slot;
	}
	memset(&decoded, 0, sizeof(decoded));
	for (size_t i = 0; i < count; i++) {
		const uint8_t *engine = frame + MATISSE_DXIO_PLATFORM_HEADER_BYTES +
			i * MATISSE_DXIO_ENGINE_BYTES;
		struct matisse_dxio_engine_result *binding;
		size_t policy = port_count, topology = slot_count;
		unsigned int slot;

		if (engine[0] != 1)
			continue;
		for (size_t p = 0; p < port_count; p++) {
			struct matisse_dxio_lane_assignment lanes;

			if (ports[p].policy.type != 1)
				continue;
			if (!matisse_map_dxio_lanes(&ports[p].lanes, ports[p].first,
						   ports[p].last, &lanes))
				return false;
			if (lanes.first != engine[2])
				continue;
			if (policy != port_count)
				return false;
			policy = p;
		}
		if (policy == port_count)
			return false;
		for (size_t s = 0; s < slot_count; s++) {
			if (engine[2] < slots[s].lanes.dxio_first ||
			    engine[2] > slots[s].lanes.dxio_last ||
			    engine[0x19] != (slots[s].encoding >> 4))
				continue;
			if (topology != slot_count)
				return false;
			topology = s;
		}
		if (topology == slot_count || decoded.count == MATISSE_NBIF_MAP_SLOTS)
			return false;
		slot = map_slot(slots[topology].encoding);
		if (used & (1U << slot))
			return false;
		used |= 1U << slot;
		binding = &decoded.engines[decoded.count++];
		binding->engine_index = (uint16_t)i;
		binding->policy_index = (uint16_t)policy;
		binding->slot = (uint8_t)slot;
		binding->encoding = slots[topology].encoding;
		binding->devfn = (ports[policy].policy.routing[0] & 0xf8U) |
			(ports[policy].policy.routing[1] & 7U);
		binding->enabled = (ports[policy].policy.routing[0] & 1U) != 0;
		binding->early = (engine[0x10] & 2U) != 0;
		binding->pool_index = UINT8_MAX;
		if (binding->enabled) {
			struct matisse_nbif_port_assignment assignment;

			if (!matisse_decode_nbif_port(binding->encoding, binding->devfn,
						      slots[topology].fallback,
						      decoded.assigned, &assignment))
				return false;
			binding->devfn = assignment.devfn;
			binding->pool_index = assignment.pool_index;
			binding->bdf = ((uint32_t)bus << 20) |
				((uint32_t)assignment.devfn << 12);
			decoded.assigned[slot] = assignment.devfn;
		}
		if (binding->early) {
			decoded.has_early = true;
			decoded.early_engine = (uint16_t)i;
			decoded.early_bdf = read32(engine + 12);
		}
	}
	*output = decoded;
	return true;
}
