/* SPDX-License-Identifier: GPL-2.0-only */

#include <commonlib/endian.h>
#include <limits.h>
#include <amdblocks/matisse_dxio_ancillary.h>

static bool decode_parameter(uint8_t key, uint8_t *opcode, bool *per_lane)
{
	*per_lane = false;
	if (key < 0x40) {
		switch (key) {
		case 1:
			*opcode = 13;
			*per_lane = true;
			return true;
		case 2:
			*opcode = 14;
			return true;
		case 3:
			*opcode = 16;
			return true;
		case 4:
			*opcode = 17;
			return true;
		default:
			return false;
		}
	}

	*opcode = key & 0x3f;
	switch (*opcode) {
	case 0:
	case 1:
	case 2:
	case 10:
	case 11:
		*per_lane = true;
		return true;
	case 18:
	case 19:
		*opcode = (uint8_t)(*opcode + 2);
		*per_lane = true;
		return true;
	case 3:
	case 4:
	case 7:
	case 8:
	case 9:
	case 12:
		return true;
	case 13:
		*opcode = 15;
		return true;
	case 5:
	case 6:
		return (key & 0xc0) == 0xc0;
	default:
		return false;
	}
}

size_t matisse_build_dxio_port_ancillary(
	const struct matisse_dxio_parameter *parameters, size_t count,
	uint8_t first, uint8_t last, bool extended_format,
	uint8_t *output, size_t capacity)
{
	unsigned int low = first < last ? first : last;
	unsigned int high = first > last ? first : last;
	unsigned int width = high - low + 1;
	uintptr_t input_address = (uintptr_t)parameters;
	uintptr_t output_address = (uintptr_t)output;
	size_t dwords = 1, bytes, input_bytes;
	uint8_t *output_end;
	uint8_t opcode;
	bool per_lane;

	if (!parameters || !output || !count || count >= UINT8_MAX ||
	    (extended_format ? high > 63 : width > 64))
		return 0;
	input_bytes = count * sizeof(*parameters);
	if (input_address > UINTPTR_MAX - input_bytes)
		return 0;
	for (size_t i = 0; i < count; i++) {
		if (!decode_parameter(parameters[i].key, &opcode, &per_lane))
			return 0;
		dwords += per_lane ? width : 1;
		/* The original builder adds another lane-sized extent for key 1
		 * without advancing the record pointer. Its separate size helper
		 * omits that extent. Reserve the full builder result and retain
		 * the original zero padding rather than underallocating.
		 */
		if (parameters[i].key == 1)
			dwords += width;
		if (dwords > UINT8_MAX)
			return 0;
	}
	bytes = dwords * sizeof(uint32_t);
	if (capacity < bytes || output_address > UINTPTR_MAX - bytes ||
	    (input_address < output_address + bytes &&
	     output_address < input_address + input_bytes))
		return 0;
	output_end = output + bytes;

	write_le32(output, 3 | (extended_format ? 0x100 : 0x200) |
		   ((uint32_t)dwords << 16));
	for (size_t i = 0; i < count; i++) {
		unsigned int begin = extended_format ? low : 0;
		unsigned int end;
		uint32_t selector;

		decode_parameter(parameters[i].key, &opcode, &per_lane);
		end = per_lane ? begin + width : begin + 1;
		selector = parameters[i].key >> 6;
		selector <<= extended_format ? 8 : 7;
		for (unsigned int lane = begin; lane < end; lane++) {
			output += sizeof(uint32_t);
			write_le32(output, opcode | selector | (lane << 10) |
				   ((uint32_t)parameters[i].value << 16));
		}
	}
	/* The original caller clears the complete ancillary allocation. */
	output += sizeof(uint32_t);
	while (output < output_end) {
		write_le32(output, 0);
		output += sizeof(uint32_t);
	}
	return bytes;
}
