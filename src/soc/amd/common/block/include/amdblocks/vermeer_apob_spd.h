/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_BLOCK_VERMEER_APOB_SPD_H
#define AMD_BLOCK_VERMEER_APOB_SPD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VERMEER_APOB_MAX_DIMMS 4
#define VERMEER_APOB_SPD_BYTES 512

struct vermeer_apob_dimm_spd {
	uint8_t channel;
	uint8_t slot;
	uint8_t spd[VERMEER_APOB_SPD_BYTES];
};

struct vermeer_apob_spd_cache {
	uint8_t count;
	struct vermeer_apob_dimm_spd dimm[VERMEER_APOB_MAX_DIMMS];
};

/* Decode raw cached SPD rows from a bounded, immutable Vermeer APOB copy.
 * The output must be separately owned; failure leaves it unchanged. This
 * function does not normalize SPD bytes, validate SPD CRCs, publish memory,
 * train DIMMs, or establish enabled ECC.
 */
bool vermeer_apob_decode_spd(const uint8_t *apob, size_t bytes,
			    struct vermeer_apob_spd_cache *result);

#endif
