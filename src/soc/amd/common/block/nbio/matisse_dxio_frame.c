/* SPDX-License-Identifier: GPL-2.0-only */

#include <commonlib/endian.h>
#include <limits.h>
#include <string.h>
#include <amdblocks/matisse_dxio_frame.h>

#define MAX_FRAME_BYTES (256 * sizeof(uint32_t))
#define MAX_BLOCK_BYTES (255 * sizeof(uint32_t))

static bool valid_range(const void *pointer, size_t bytes)
{
	return pointer && (uintptr_t)pointer <= UINTPTR_MAX - bytes;
}

static bool overlap(const void *first, size_t first_bytes,
		    const void *second, size_t second_bytes)
{
	return first_bytes && second_bytes &&
		(uintptr_t)first < (uintptr_t)second + second_bytes &&
		(uintptr_t)second < (uintptr_t)first + first_bytes;
}

static bool output_overlaps(const void *input, size_t bytes,
			    const uint8_t *output, size_t output_bytes,
			    const uint16_t *offsets, size_t offset_bytes)
{
	return overlap(input, bytes, output, output_bytes) ||
		overlap(input, bytes, offsets, offset_bytes);
}

size_t matisse_build_dxio_ancillary_frame(
	const struct matisse_dxio_ancillary_global *global,
	const struct matisse_dxio_ancillary_block *ports, size_t count,
	uint8_t *output, size_t capacity, uint16_t *port_offsets)
{
	size_t port_bytes, offset_bytes, bytes = 0, cursor;
	bool present;

	if (!valid_range(global, sizeof(*global)) || count > UINT16_MAX)
		return 0;
	port_bytes = count * sizeof(*ports);
	offset_bytes = count * sizeof(*port_offsets);
	if (count && (!valid_range(ports, port_bytes) ||
		      !valid_range(port_offsets, offset_bytes)))
		return 0;
	if (global->extra_present &&
	    (global->extra_bytes > MAX_BLOCK_BYTES - sizeof(uint32_t) ||
	     global->extra_bytes % sizeof(uint32_t) ||
	     (global->extra_bytes &&
	      !valid_range(global->extra_data, global->extra_bytes))))
		return 0;

	present = global->pspp_enabled || global->extra_present;
	if (global->pspp_enabled)
		bytes += global->pspp_tuning ? 12 : 8;
	if (global->extra_present)
		bytes += sizeof(uint32_t) + global->extra_bytes;
	for (size_t i = 0; i < count; i++) {
		const struct matisse_dxio_ancillary_block *block = &ports[i];
		uint32_t header, format;

		if (!block->bytes)
			continue;
		if (block->bytes < sizeof(uint32_t) || block->bytes > MAX_BLOCK_BYTES ||
		    block->bytes % sizeof(uint32_t) ||
		    !valid_range(block->data, block->bytes))
			return 0;
		header = read_le32(block->data);
		format = header & 0xff00;
		if ((format != 0x100 && format != 0x200) ||
		    header != (3 | format | ((uint32_t)(block->bytes / 4) << 16)))
			return 0;
		present = true;
		bytes += block->bytes;
		if (bytes > MAX_FRAME_BYTES - 8)
			return 0;
	}
	if (!present)
		return 0;
	bytes += 8; /* Outer header and the original final zero dword. */
	if (bytes > MAX_FRAME_BYTES || capacity < bytes ||
	    !valid_range(output, bytes) ||
	    overlap(output, bytes, port_offsets, offset_bytes) ||
	    output_overlaps(global, sizeof(*global), output, bytes,
			    port_offsets, offset_bytes) ||
	    output_overlaps(ports, port_bytes, output, bytes,
			    port_offsets, offset_bytes))
		return 0;
	if (global->extra_present &&
	    output_overlaps(global->extra_data, global->extra_bytes, output, bytes,
			    port_offsets, offset_bytes))
		return 0;
	for (size_t i = 0; i < count; i++) {
		if (output_overlaps(ports[i].data, ports[i].bytes, output, bytes,
				    port_offsets, offset_bytes))
			return 0;
	}

	memset(output, 0, bytes);
	write_le32(output, 6 | ((uint32_t)(bytes / 4 - 1) << 16));
	cursor = sizeof(uint32_t);
	if (global->pspp_enabled) {
		write_le32(output + cursor,
			   0x104 | ((global->pspp_tuning ? 3U : 2U) << 16));
		write_le32(output + cursor + 4, global->pspp_data);
		cursor += 8;
		if (global->pspp_tuning) {
			write_le32(output + cursor, global->pspp_tuning);
			cursor += sizeof(uint32_t);
		}
	}
	if (global->extra_present) {
		write_le32(output + cursor,
			   0x105 | ((uint32_t)(global->extra_bytes / 4 + 1) << 16));
		cursor += sizeof(uint32_t);
		if (global->extra_bytes)
			memcpy(output + cursor, global->extra_data, global->extra_bytes);
		cursor += global->extra_bytes;
	}
	for (size_t i = 0; i < count; i++) {
		port_offsets[i] = ports[i].bytes ? (uint16_t)(cursor / 4) : 0;
		if (ports[i].bytes)
			memcpy(output + cursor, ports[i].data, ports[i].bytes);
		cursor += ports[i].bytes;
	}
	return bytes;
}
