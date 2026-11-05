/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NATIVE84_INTERVAL_H
#define NATIVE84_INTERVAL_H

#ifdef __ASSEMBLER__
#error "The numerical interval is a C interface, not assembler data"
#endif
#include <stddef.h>
#include <stdint.h>

/* Numerical half-open endpoints only; no mapping or ownership authority. */
struct native84_interval {
	uint32_t begin;
	uint32_t end;
};
_Static_assert(sizeof(struct native84_interval) == 8 &&
	_Alignof(struct native84_interval) == 4 &&
	offsetof(struct native84_interval, begin) == 0 &&
	offsetof(struct native84_interval, end) == 4, "Shared interval8 ABI");

#endif
