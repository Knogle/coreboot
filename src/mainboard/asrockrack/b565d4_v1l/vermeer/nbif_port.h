/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_NBIF_PORT_H
#define AMD_MATISSE_NBIF_PORT_H

#include "nbif_map.h"

struct matisse_nbif_port_assignment {
	uint8_t slot;
	uint8_t devfn;
	uint8_t pool_index;
};

/* Pure preparation decoder for original port assignment helper09c7be80.
 * port_encoding is the original descriptor+1d (wrapper low nibble, local port
 * high nibble); devfn and fallback are explicit original+13/+1c inputs. A zero
 * devfn uses the original eight-bit rotate-left-three fallback. assigned is
 * the current fresh per-frame partial map, not the completed hardware map.
 * Caller qualifies that the encoded wrapper/port actually exists in topology.
 *
 * Returns a proposed assignment without changing inputs. False leaves output
 * unchanged for an unrepresentable slot, unqualified/colliding devfn, invalid
 * pointer/wrap or overlap. It does not copy packed vendor descriptors, write
 * the map or initialize PCIe hardware. Actual placement in the fresh map is
 * performed by the caller after validating the complete returned frame.
 */
bool matisse_decode_nbif_port(uint8_t port_encoding, uint8_t devfn, uint8_t fallback,
			      const uint8_t assigned[MATISSE_NBIF_MAP_SLOTS],
			      struct matisse_nbif_port_assignment *result);

#endif /* AMD_MATISSE_NBIF_PORT_H */
