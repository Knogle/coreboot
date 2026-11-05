/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_NBIO_BASE_PLAN_H
#define AMD_MATISSE_NBIO_BASE_PLAN_H

#include "dxio_configured.h"

#define MATISSE_NBIO_BASE_MAX_STEPS 311

enum matisse_nbio_base_program {
	MATISSE_NBIO_BASE_AM4_DEFAULTS,
	MATISSE_NBIO_BASE_COMMON_DEFAULTS,
	MATISSE_NBIO_BASE_INITIAL_AM4_DEFAULTS,
};

/* Desktop consumed register fields. These programs
 * correspond to the original Matisse AM4 package selection, single NBIO
 * instance3/bus0 and compiled PCD defaults. They contain no vendor bytecode,
 * retained pointers or firmware calls. The combined program preserves both
 * tables' original order, including direct writes and repeated accesses.
 *
 * Caller admits the actual initial phase, board policy and exclusive SMN
 * ownership. It may execute the resulting native steps through the existing
 * bounded matisse_run_dxio_configured() SMN engine with persistent state and
 * matching admission hooks. This is not the full original BaseInit: adjacent
 * BAR/resource allocation, other callbacks and later OEM/NVRAM policy remain
 * separate. A plan does not establish physical I/O or link training.
 *
 * Invalid program, null, insufficient capacity or wrapping/aliased output
 * returns zero before output stores. Capacity counts native steps.
 */
size_t matisse_build_nbio_base_plan(enum matisse_nbio_base_program program,
		struct matisse_dxio_configured_step *output, size_t capacity);

#endif /* AMD_MATISSE_NBIO_BASE_PLAN_H */
