/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef B565D4_CPU_TOPOLOGY_H
#define B565D4_CPU_TOPOLOGY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define B565D4_CEZANNE_MAX_CORES 8
#define B565D4_CEZANNE_TOPOLOGY_BYTES 0x1c8

struct b565d4_cpu_topology {
	uint32_t launch_mask;
	uint8_t physical_core[B565D4_CEZANNE_MAX_CORES];
	unsigned int logical_threads;
	unsigned int threads_per_core;
};

bool b565d4_decode_cezanne_topology(const uint8_t *record, size_t bytes,
				  unsigned int logical_threads,
				  struct b565d4_cpu_topology *topology);

#endif /* B565D4_CPU_TOPOLOGY_H */
