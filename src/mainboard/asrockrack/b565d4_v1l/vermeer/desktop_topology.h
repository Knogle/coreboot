/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AM4_DESKTOP_TOPOLOGY_H
#define AM4_DESKTOP_TOPOLOGY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <b565d4_cpu_ids.h>

#define AM4_DESKTOP_TOPOLOGY_BYTES 0x1c8
#define AM4_DESKTOP_MAX_CCDS 2
#define AM4_DESKTOP_PHYSICAL_CCD_SLOTS 8
#define AM4_DESKTOP_MAX_THREADS 32

struct am4_desktop_thread {
	uint8_t ccd, ccx, core, thread;
	uint8_t physical_ccd, physical_ccx, physical_core;
	uint8_t launch_bit;
};

struct am4_desktop_topology {
	unsigned int ccd_count;
	unsigned int thread_count;
	unsigned int threads_per_core;
	uint8_t logical_ccd[AM4_DESKTOP_MAX_CCDS];
	uint8_t physical_ccd[AM4_DESKTOP_MAX_CCDS];
	uint16_t launch_mask[AM4_DESKTOP_MAX_CCDS];
	struct am4_desktop_thread threads[AM4_DESKTOP_MAX_THREADS];
};

/* Matisse and Vermeer APOB 3/3/0 decoding. This pure decoder
 * distinguishes two four-core complexes from one eight-core complex per CCD.
 * It does not establish the enclosing APOB version, memory ownership, AP reset
 * backing, an SMU operation or physical target support. Masks include the BSP;
 * callers must exclude already released threads before any future register I/O.
 * On failure the output is unchanged.
 */
bool am4_desktop_decode_topology(enum b565d4_cpu_family family,
				 const uint8_t *record, size_t bytes,
				 unsigned int logical_threads,
				 struct am4_desktop_topology *topology);

#endif /* AM4_DESKTOP_TOPOLOGY_H */
