/* SPDX-License-Identifier: GPL-2.0-only */

#include <console/console.h>
#include <option.h>
#include "board_config.h"
#include "setup_options.h"

enum czn_pcie_topology b565d4_pcie_topology_from_option(unsigned int value,
	enum czn_pcie_topology fallback)
{
	switch (value) {
	case B565D4_PCIE_SLOT_X16:
		return CZN_PCIE_X16;
	case B565D4_PCIE_SLOT_X8X4X4:
		return CONFIG(B565D4_DESKTOP_NATIVE) ? fallback : CZN_PCIE_X8X4X4;
	case B565D4_PCIE_SLOT_FOUR_X4:
		return CZN_PCIE_FOUR_X4;
	default:
		return fallback;
	}
}

enum czn_pcie_topology b565d4_pcie_topology(void)
{
	const enum czn_pcie_topology fallback = b565d4_pcie_default_topology();
	unsigned int value = B565D4_PCIE_SLOT_DEFAULT;

	if (CONFIG(B565D4_BOARD_SETUP) || CONFIG(B565D4_DESKTOP_SETUP))
		value = b565d4_setup_get(B565D4_OPT_BIFURCATION);
	else if (CONFIG(B565D4_PCIE_RUNTIME_OPTIONS))
		value = get_uint_option(OPTION_NAME_PCIE_SLOT_BIFURCATION,
					B565D4_PCIE_SLOT_DEFAULT);
	if (value > B565D4_PCIE_SLOT_FOUR_X4 ||
	    (CONFIG(B565D4_DESKTOP_NATIVE) && value == B565D4_PCIE_SLOT_X8X4X4))
		printk(BIOS_WARNING, "Unsupported PCIe slot option %u; using build default\n", value);

	const enum czn_pcie_topology topology = b565d4_pcie_topology_from_option(value, fallback);

	printk(BIOS_NOTICE, "PCIe slot configuration: stored=%u topology=%u default=%u\n",
	       value, topology, fallback);
	return topology;
}
