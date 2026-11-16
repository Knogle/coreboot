/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef B565D4_DIMM_SPD_H
#define B565D4_DIMM_SPD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define B565D4_DIMM_COUNT 4
#define B565D4_SPD_BYTES 512

struct b565d4_dimm_spd {
	uint8_t channel, slot;
	uint8_t spd[B565D4_SPD_BYTES];
};

struct b565d4_spd_cache {
	uint8_t count;
	struct b565d4_dimm_spd dimm[B565D4_DIMM_COUNT];
};

/* Read the bounded, immutable Vermeer APOB copy, not the live SPD bus.
 * The supplier caches a serial/part-number copy in reserved SPD bytes80..103.
 * Normalize only that documented local-copy extent and require both original
 * SPD CRCs. The output must be separately owned; failure leaves it unchanged.
 * This inventory does not publish RAM, train DIMMs or establish enabled ECC.
 */
bool b565d4_decode_dimm_spd(const uint8_t *apob, size_t bytes,
	struct b565d4_spd_cache *result);

#endif
