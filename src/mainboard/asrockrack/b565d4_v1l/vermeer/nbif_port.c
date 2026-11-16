/* SPDX-License-Identifier: GPL-2.0-only */

#include <limits.h>
#include "nbif_port.h"

bool matisse_decode_nbif_port(uint8_t port_encoding, uint8_t devfn, uint8_t fallback,
			      const uint8_t assigned[MATISSE_NBIF_MAP_SLOTS],
			      struct matisse_nbif_port_assignment *result)
{
	struct matisse_nbif_port_assignment decoded;
	uintptr_t input = (uintptr_t)assigned;
	uintptr_t output = (uintptr_t)result;
	unsigned int slot = ((port_encoding & 15U) << 3) + (port_encoding >> 4);
	int index;

	if (!assigned || !result || input > UINTPTR_MAX - MATISSE_NBIF_MAP_SLOTS ||
	    output > UINTPTR_MAX - sizeof(*result) ||
	    (input < output + sizeof(*result) &&
	     output < input + MATISSE_NBIF_MAP_SLOTS) || slot >= MATISSE_NBIF_MAP_SLOTS)
		return false;
	if (!devfn)
		devfn = (uint8_t)(((unsigned int)fallback << 3) | (fallback >> 5));
	index = matisse_nbif_devfn_index(devfn);
	if (index < 0)
		return false;
	for (size_t i = 0; i < MATISSE_NBIF_MAP_SLOTS; i++)
		if (assigned[i] == devfn)
			return false;
	decoded.slot = (uint8_t)slot;
	decoded.devfn = devfn;
	decoded.pool_index = (uint8_t)index;
	*result = decoded;
	return true;
}
