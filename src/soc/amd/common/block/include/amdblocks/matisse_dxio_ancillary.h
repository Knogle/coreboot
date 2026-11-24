/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_DXIO_ANCILLARY_H
#define AMD_MATISSE_DXIO_ANCILLARY_H

#include <types.h>

struct matisse_dxio_parameter {
	uint8_t key;
	uint8_t value;
};

/* Pure per-port ancillary serialization for the desktop backend.
 * Each parameter is an explicit qualified original key/value pair. No vendor
 * pointers or sentinel-terminated lists are followed. extended_format selects
 * the original PCD e3 format and absolute lane numbering; the other format uses
 * lane indices relative to the port. Endpoints may descend. Used lane numbers
 * must fit their six-bit wire field; the complete record must fit its one-byte
 * dword count. The caller keeps input stable and disjoint from output.
 *
 * Returns the full byte count, or zero without output stores for empty input
 * or invalid parameters/bounds/backing. An empty list needs no ancillary data.
 * Key 1 includes the original builder's extra zero tail; its separate vendor
 * size helper underestimates this extent. The full native size includes it.
 * The caller owns board policy, outer ancillary framing, offsets into that
 * buffer, DMA placement, SMU sequencing and physical link training.
 */
size_t matisse_build_dxio_port_ancillary(
	const struct matisse_dxio_parameter *parameters, size_t count,
	uint8_t first, uint8_t last, bool extended_format,
	uint8_t *output, size_t capacity);

#endif /* AMD_MATISSE_DXIO_ANCILLARY_H */
