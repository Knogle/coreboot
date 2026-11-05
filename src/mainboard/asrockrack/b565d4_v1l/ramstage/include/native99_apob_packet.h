/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NATIVE99_APOB_PACKET_H
#define NATIVE99_APOB_PACKET_H

#include "native_smm_memory.h"
#include <stddef.h>
#include <stdint.h>
#include <b565d4_memlayout.h>

#define NATIVE99_PACKET_BASE (B565D4_NATIVE_DRAM_BASE + 0x10000)
#define NATIVE99_PACKET_END (B565D4_NATIVE_DRAM_BASE + 0x40000)
#define NATIVE99_PACKET_VERSION 2U
#define NATIVE99_PACKET_HEADER_BYTES 64U
#define NATIVE99_PACKET_MAGIC 0x50413939U /* "99AP", not an ownership token. */

/* Private native record, never a PI HOB or supplier PPI. The descriptor56 and
 * current CBMEM/top ABI remain separate and unchanged. The full record stays
 * immutable until the later RAMstage has copied it into its own CBMEM entry.
 * The direct romstage-to-ramstage flow does not execute the historical late
 * bootblock/romstage slots occupied by this 192-KiB packet reservation.
 */
struct native99_apob_packet {
	uint32_t magic, version, header_bytes, epoch;
	uint32_t parent_base, parent_bytes, apob_bytes, reserved;
	struct b565d4_smm_state smm;
	uint8_t apob[];
};

_Static_assert(sizeof(struct native99_apob_packet) == NATIVE99_PACKET_HEADER_BYTES &&
	offsetof(struct native99_apob_packet, smm) == 32 &&
	offsetof(struct native99_apob_packet, apob) == NATIVE99_PACKET_HEADER_BYTES,
	"Private whole-APOB and early SMM-state handoff prefix");

#endif
