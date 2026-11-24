/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_DXIO_FRAME_H
#define AMD_MATISSE_DXIO_FRAME_H

#include <types.h>

struct matisse_dxio_ancillary_block {
	const uint8_t *data;
	size_t bytes;
};

struct matisse_dxio_ancillary_global {
	bool pspp_enabled;
	uint32_t pspp_data;
	/* The original includes this second dword only when it is nonzero. */
	uint32_t pspp_tuning;
	bool extra_present;
	const uint8_t *extra_data;
	size_t extra_bytes;
};

/* Pure calculation for the desktop backend. Each nonempty
 * port block comes from matisse_build_dxio_port_ancillary(), including its
 * complete builder extent. Global fields correspond to original config
 * +22/+26 and the optional +16 data pointer, without following vendor pointers.
 *
 * Returns the complete frame size. Returns zero without output stores when no
 * frame is needed, or for invalid headers, lengths, pointers, backing overlap
 * or insufficient capacity. Inputs stay unchanged. port_offsets has count
 * entries and receives offsets in dwords; empty blocks receive zero. With no
 * frame, callers retain their initially zeroed offsets. The outer one-byte
 * length limits the frame to 256 dwords; a port block has at most 255 dwords.
 * Caller owns current policy, physical buffers, DMA and DXIO sequencing.
 */
size_t matisse_build_dxio_ancillary_frame(
	const struct matisse_dxio_ancillary_global *global,
	const struct matisse_dxio_ancillary_block *ports, size_t count,
	uint8_t *output, size_t capacity, uint16_t *port_offsets);

#endif /* AMD_MATISSE_DXIO_FRAME_H */
