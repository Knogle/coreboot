/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef B565D4_MATISSE_CPU_H
#define B565D4_MATISSE_CPU_H

#include <stdint.h>
#include <types.h>

enum {
	MATISSE_THREAD_MSR_END = 4,
	MATISSE_CORE_MSR_END = 24,
	MATISSE_MSR_COUNT = 26,
};

struct matisse_vendor_msr {
	uint32_t index;
	uint64_t data;
	uint64_t mask;
};

extern const struct matisse_vendor_msr matisse_vendor_registers[MATISSE_MSR_COUNT];
void b565d4_matisse_init_msrs(void);
enum cb_err b565d4_desktop_finalize_mp(void);

#endif
