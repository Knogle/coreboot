/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef B565D4_BOARD_CONFIG_H
#define B565D4_BOARD_CONFIG_H

#include "czn_pcie_topology.h"

#if CONFIG(B565D4_PCIE_RUNTIME_OPTIONS) && !CONFIG(USE_OPTION_TABLE)
#error "B565D4 PCIe runtime options require the CMOS option backend"
#endif

#define OPTION_NAME_PCIE_SLOT_BIFURCATION "pcie_slot_bifurcation"

enum b565d4_pcie_slot_option {
	B565D4_PCIE_SLOT_DEFAULT,
	B565D4_PCIE_SLOT_X16,
	B565D4_PCIE_SLOT_X8X4X4,
	B565D4_PCIE_SLOT_FOUR_X4,
};

static inline enum czn_pcie_topology b565d4_pcie_default_topology(void)
{
	if (CONFIG(B565D4_PCIE_FOUR_X4_PROBE))
		return CZN_PCIE_FOUR_X4_PROBE;
	if (CONFIG(B565D4_PCIE_FOUR_X4))
		return CZN_PCIE_FOUR_X4;
	if (CONFIG(B565D4_PCIE_X8X4X4))
		return CZN_PCIE_X8X4X4;
	return CZN_PCIE_X16;
}

enum czn_pcie_topology b565d4_pcie_topology_from_option(unsigned int value,
	enum czn_pcie_topology fallback);
enum czn_pcie_topology b565d4_pcie_topology(void);

#endif
