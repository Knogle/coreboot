/* SPDX-License-Identifier: GPL-2.0-only */

#include <limits.h>
#include <string.h>
#include <amdblocks/matisse_dxio_configured.h>

struct plan_writer {
	struct matisse_dxio_configured_step *output;
	size_t count;
};

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

static void emit(struct plan_writer *writer, unsigned int kind, uint32_t address,
		 uint32_t and_mask, uint32_t value, uint8_t encoding)
{
	if (writer->output) {
		struct matisse_dxio_configured_step *step = &writer->output[writer->count];

		step->kind = kind;
		step->address = address;
		step->and_mask = and_mask;
		step->value = value;
		step->encoding = encoding;
	}
	writer->count++;
}

static void selector(struct plan_writer *writer, unsigned int block,
		     unsigned int port, unsigned int wrapper, unsigned int number)
{
	uint32_t address = 0x090467acU + ((block & 255U) << 12);
	uint32_t index = (uint32_t)(port * 0x61U + number) - 0x20000U;
	uint8_t encoding = (uint8_t)((port << 4) | wrapper);

	emit(writer, MATISSE_DXIO_CONFIGURED_WRITE, address, 0, index, encoding);
	emit(writer, MATISSE_DXIO_CONFIGURED_WRITE, address + 4, 0, 0, encoding);
}

static void build_plan(const uint8_t *wrappers, size_t wrapper_count,
		const struct matisse_dxio_result_slot *slots, size_t slot_count,
		const struct matisse_dxio_port *ports,
		const struct matisse_dxio_result *result,
		const struct matisse_dxio_configured_policy *policy,
		struct plan_writer *writer)
{
	for (size_t w = 0; w < wrapper_count; w++) {
		unsigned int wrapper = wrappers[w];
		unsigned int block = policy->root_index + wrapper * 4U;
		uint32_t wrapper_offset = (uint32_t)block << 20;
		uint8_t wrapper_encoding = (uint8_t)wrapper;

		for (size_t s = 0; s < slot_count; s++) {
			uint8_t encoding = slots[s].encoding;
			unsigned int port = encoding >> 4;
			uint32_t offset = wrapper_offset + ((uint32_t)port << 12);

			if ((encoding & 15U) != wrapper)
				continue;
			if (policy->records)
				emit(writer, MATISSE_DXIO_CONFIGURED_RECORD,
				     0x11100388U + offset, 0, 0, encoding);
			for (size_t b = 0; b < result->count; b++) {
				const struct matisse_dxio_engine_result *binding =
					&result->engines[b];
				const struct matisse_dxio_port_policy *control;

				if (binding->encoding != encoding)
					continue;
				control = &ports[binding->policy_index].policy;
				if (control->extended & 2U)
					emit(writer, MATISSE_DXIO_CONFIGURED_RMW,
					     0x11140304U + offset, 0xffffff3fU,
					     0xc0, encoding);
				emit(writer, MATISSE_DXIO_CONFIGURED_RMW,
				     0x11140080U + offset, 0xffff7fffU, 0, encoding);
				if (policy->token_231 && (control->equalization & 8U)) {
					selector(writer, block, port, wrapper, 0x12f);
					selector(writer, block, port, wrapper, 0x130);
				}
			}
		}
		emit(writer, MATISSE_DXIO_CONFIGURED_RMW, 0x1118018cU + wrapper_offset,
		     wrapper < 2 ? 0xfffffff0U : 0xf7fffff4U,
		     wrapper < 2 ? wrapper + 2U : 0x14000004U, wrapper_encoding);
		if (policy->token_194) {
			emit(writer, MATISSE_DXIO_CONFIGURED_RMW,
			     0x111804d4U + wrapper_offset, 0xfffffffbU, 4, wrapper_encoding);
			emit(writer, MATISSE_DXIO_CONFIGURED_RMW,
			     0x111804d8U + wrapper_offset, 0xfff0007fU,
			     0x64b80, wrapper_encoding);
			emit(writer, MATISSE_DXIO_CONFIGURED_RMW,
			     0x111804dcU + wrapper_offset, 0xff07ffffU,
			     0x780000, wrapper_encoding);
		}
		if (policy->token_24c)
			emit(writer, MATISSE_DXIO_CONFIGURED_RMW,
			     0x11180080U + wrapper_offset, 0xdfffffffU,
			     0x20000000, wrapper_encoding);
		if (policy->token_2fb)
			emit(writer, MATISSE_DXIO_CONFIGURED_RMW,
			     0x11180080U + wrapper_offset, 0x7fffffffU,
			     0x80000000, wrapper_encoding);
	}
}

size_t matisse_plan_dxio_configured(const uint8_t *wrappers, size_t wrapper_count,
		const struct matisse_dxio_result_slot *slots, size_t slot_count,
		const struct matisse_dxio_port *ports, size_t port_count,
		const struct matisse_dxio_result *result,
		const struct matisse_dxio_configured_policy *policy,
		struct matisse_dxio_configured_step *output, size_t capacity)
{
	struct plan_writer writer = { 0 };
	size_t slots_bytes, ports_bytes, output_bytes;
	uint32_t present_wrappers = 0, present_slots = 0, allocated_slots = 0;

	if (wrapper_count > 16 || slot_count > MATISSE_NBIF_MAP_SLOTS ||
	    port_count > UINT16_MAX || port_count > SIZE_MAX / sizeof(*ports) ||
	    !valid_range(wrappers, wrapper_count) ||
	    !valid_range(slots, slot_count * sizeof(*slots)) ||
	    !valid_range(ports, port_count * sizeof(*ports)) ||
	    !result || !valid_range(result, sizeof(*result)) ||
	    !policy || !valid_range(policy, sizeof(*policy)) ||
	    result->count > MATISSE_NBIF_MAP_SLOTS)
		return 0;
	slots_bytes = slot_count * sizeof(*slots);
	ports_bytes = port_count * sizeof(*ports);
	for (size_t w = 0; w < wrapper_count; w++) {
		if (wrappers[w] > 15 || (present_wrappers & (1U << wrappers[w])))
			return 0;
		present_wrappers |= 1U << wrappers[w];
	}
	for (size_t s = 0; s < slot_count; s++) {
		unsigned int slot = map_slot(slots[s].encoding);

		if ((slots[s].encoding >> 4) > 7 || slot >= MATISSE_NBIF_MAP_SLOTS ||
		    !(present_wrappers & (1U << (slots[s].encoding & 15U))) ||
		    (present_slots & (1U << slot)))
			return 0;
		present_slots |= 1U << slot;
	}
	for (size_t b = 0; b < result->count; b++) {
		const struct matisse_dxio_engine_result *binding = &result->engines[b];
		unsigned int slot = map_slot(binding->encoding);

		if ((binding->encoding >> 4) > 7 ||
		    slot >= MATISSE_NBIF_MAP_SLOTS || binding->slot != slot ||
		    !(present_slots & (1U << slot)) || (allocated_slots & (1U << slot)) ||
		    binding->policy_index >= port_count ||
		    ports[binding->policy_index].policy.type != 1)
			return 0;
		allocated_slots |= 1U << slot;
	}
	build_plan(wrappers, wrapper_count, slots, slot_count, ports, result, policy, &writer);
	if (!writer.count || !output || capacity < writer.count ||
	    writer.count > SIZE_MAX / sizeof(*output))
		return 0;
	output_bytes = writer.count * sizeof(*output);
	if (!valid_range(output, output_bytes) ||
	    overlap(output, output_bytes, wrappers, wrapper_count) ||
	    overlap(output, output_bytes, slots, slots_bytes) ||
	    overlap(output, output_bytes, ports, ports_bytes) ||
	    overlap(output, output_bytes, result, sizeof(*result)) ||
	    overlap(output, output_bytes, policy, sizeof(*policy)))
		return 0;
	memset(output, 0, output_bytes);
	writer.output = output;
	writer.count = 0;
	build_plan(wrappers, wrapper_count, slots, slot_count, ports, result, policy, &writer);
	return writer.count;
}
