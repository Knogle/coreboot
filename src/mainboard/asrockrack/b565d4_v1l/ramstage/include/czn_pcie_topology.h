/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef CZN_PCIE_TOPOLOGY_H
#define CZN_PCIE_TOPOLOGY_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

enum czn_pcie_topology {
	CZN_PCIE_X16,
	CZN_PCIE_X8X4X4,
	CZN_PCIE_FOUR_X4_PROBE,
	CZN_PCIE_FOUR_X4,
};

#define CZN_MAX_PORTS 6
/* Keep the trailing eight bytes after all six 40-byte records. */
#define CZN_MAX_ENGINE_BYTES 272
#define CZN_MAX_ANCILLARY_BYTES 460

struct dxio_port {
	uint8_t first, last, group, wrapper, port, map_slot, devfn, allocated, flags27;
	uint8_t eq[4];
	uint32_t control;
};

/* These construct owned descriptor bytes only; they perform no hardware I/O.
 * The allocation-only probe retains an unselected host map slot. Its caller
 * must stop before host-side port programming; the normal four-x4 variant
 * uses the separately observed SMU block and local port assignments.
 */
size_t czn_pcie_ports(enum czn_pcie_topology topology, struct dxio_port *ports,
	size_t capacity);
size_t czn_pcie_engine(uint8_t *engine, size_t capacity,
	const struct dxio_port *ports, size_t count);
size_t czn_pcie_ancillary(uint8_t *ancillary, size_t capacity,
	const struct dxio_port *ports, size_t count,
	const uint8_t *original, size_t original_bytes, bool acs);
bool czn_pcie_x16_ancillary_matches(const uint8_t *data, size_t bytes,
	const uint8_t *original, size_t original_bytes, bool acs);

#endif
