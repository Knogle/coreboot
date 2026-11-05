/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef B565D4_MEMLAYOUT_H
#define B565D4_MEMLAYOUT_H

/* Keep the later workspace in low DRAM, following the AMD DRAM-first ports.
 * A large low UMA reservation must not impose a minimum installed capacity.
 * The original PSP reset image/APOB placements remain unchanged. Romstage
 * validates this entire workspace against the actual APOB before writing it.
 * SeaBIOS config_seabios must use BASE + 0x150000 as CBFS_LOCATION.
 */
#define B565D4_NATIVE_DRAM_BASE 0x02000000
#define B565D4_NATIVE_DRAM_BYTES 0x00800000
#define B565D4_NATIVE_DRAM_END (B565D4_NATIVE_DRAM_BASE + B565D4_NATIVE_DRAM_BYTES)

#endif
