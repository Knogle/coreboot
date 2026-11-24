/* SPDX-License-Identifier: GPL-2.0-only */

#include <commonlib/endian.h>
#include <limits.h>
#include <string.h>
#include <amdblocks/matisse_dxio_engines.h>

static void build_engine(const struct matisse_dxio_port *port,
			const struct matisse_dxio_lane_assignment *lanes,
			uint8_t pspp_policy, bool force_reset_id, uint8_t *output)
{
	const struct matisse_dxio_port_policy *p = &port->policy;
	uint32_t control = 0, extended = 0;
	uint8_t reset_id = force_reset_id ? 1 : p->reset_id;

	write_le32(output, p->type | ((uint32_t)(p->engine_flags & 1) << 8) |
		   ((uint32_t)lanes->first << 16) | ((uint32_t)lanes->last << 24));
	write_le32(output + 4, 1 | ((uint32_t)reset_id << 8) |
		   ((uint32_t)(p->port_flags & 1) << 16));
	write_le32(output + 8, (uint32_t)port->ancillary_offset << 16);

	/* Bits shared by the original PCIe and SATA engine encodings. */
	extended |= (p->extended >> 1) & 8;
	extended |= (uint32_t)(p->extended & 0x20) << 4;
	if (p->type == 3) {
		write_le32(output + 8, read_le32(output + 8) |
			   ((p->miscellaneous >> 4) & 7));
		write_le32(output + 20, extended);
		return;
	}

	control |= (p->miscellaneous >> 1) & 2;
	if ((control & 2) || pspp_policy == 2)
		control |= 0x10;
	else
		control |= ((uint32_t)p->routing[1] << 1) & 0x70;
	if (p->link[0] & 0xf)
		extended |= 4 | (p->link[0] & 3);
	if (p->extended & 1)
		control = (control & ~0x40u) | 0x30;
	if (p->clock & 8)
		extended |= 0x100 | ((uint32_t)(p->clock & 0x30) << 2);
	write_le32(output + 12, (p->routing[0] & 0xf8) | (p->routing[1] & 7));
	control |= 0x80000 | ((uint32_t)(p->miscellaneous & 0x80) << 13);
	control |= p->routing[0] & 1;
	control |= (uint32_t)lanes->reversed << 3;
	control |= (uint32_t)(p->link[1] & 0x70) << 4;
	control |= (uint32_t)(p->equalization & 4) << 29;
	control |= (uint32_t)(p->miscellaneous & 1) << 2;
	extended |= (uint32_t)(p->auxiliary & 0x1f) << 10;
	if ((p->power & 0xe0) == 0x20)
		write_le32(output + 4, read_le32(output + 4) | 0x20000);
	extended |= (uint32_t)(p->extended & 0xc) << 2;
	control |= (uint32_t)(p->clock & 1) << 21;
	write_le32(output + 16, control);
	write_le32(output + 20, extended);
}

size_t matisse_build_dxio_platform(const struct matisse_dxio_port *ports,
				 size_t count, uint8_t pspp_policy,
				 bool force_reset_id, uint8_t *output,
				 size_t capacity)
{
	struct matisse_dxio_lane_assignment lanes;
	size_t bytes, input_bytes;
	uintptr_t input_address = (uintptr_t)ports;
	uintptr_t output_address = (uintptr_t)output;

	if (!ports || !output || !count || count > UINT16_MAX)
		return 0;
	bytes = MATISSE_DXIO_PLATFORM_HEADER_BYTES + count * MATISSE_DXIO_ENGINE_BYTES +
		MATISSE_DXIO_PLATFORM_TAIL_BYTES;
	input_bytes = count * sizeof(*ports);
	if (capacity < bytes || input_address > UINTPTR_MAX - input_bytes ||
	    output_address > UINTPTR_MAX - bytes ||
	    (input_address < output_address + bytes &&
	     output_address < input_address + input_bytes))
		return 0;

	for (size_t i = 0; i < count; i++) {
		if ((ports[i].policy.type != 1 && ports[i].policy.type != 3) ||
		    !matisse_map_dxio_lanes(&ports[i].lanes, ports[i].first,
					   ports[i].last, &lanes))
			return 0;
	}

	memset(output, 0, bytes);
	write_le16(output + 12, (uint16_t)count);
	for (size_t i = 0; i < count; i++) {
		matisse_map_dxio_lanes(&ports[i].lanes, ports[i].first,
				       ports[i].last, &lanes);
		build_engine(&ports[i], &lanes, pspp_policy, force_reset_id,
			     output + MATISSE_DXIO_PLATFORM_HEADER_BYTES +
			     i * MATISSE_DXIO_ENGINE_BYTES);
	}
	return bytes;
}
