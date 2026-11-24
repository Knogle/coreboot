/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_DXIO_DMA_H
#define AMD_MATISSE_DXIO_DMA_H

#include <amdblocks/matisse_dxio_init_plan.h>

struct matisse_dxio_dma_input {
	struct matisse_dxio_init_buffers wire;
	void *platform;
	void *ancillary;
};

enum matisse_dxio_dma_phase {
	MATISSE_DXIO_DMA_NOT_STARTED,
	MATISSE_DXIO_DMA_ADMITTING,
	MATISSE_DXIO_DMA_CPU_READY,
	MATISSE_DXIO_DMA_SYNCHRONIZING,
	MATISSE_DXIO_DMA_RESULT_PENDING,
	MATISSE_DXIO_DMA_FAILED,
};

enum matisse_dxio_dma_operation {
	MATISSE_DXIO_DMA_NO_OPERATION,
	MATISSE_DXIO_DMA_PUBLISH_LOAD,
	MATISSE_DXIO_DMA_PUBLISH_RESULT,
	MATISSE_DXIO_DMA_CONSUME_RESULT,
};

struct matisse_dxio_dma_state {
	uint32_t phase, operation, last_use;
	uint32_t cache_attempted, cache_completed, loads, result_reads;
	struct matisse_dxio_dma_input backing;
};

struct matisse_dxio_dma_io {
	enum cb_err (*admit)(const struct matisse_dxio_dma_input *backing, void *context);
	bool (*may_access)(const struct matisse_dxio_dma_input *backing, void *context);
	void *context;
};

/* Cache visibility adapter for the experimental desktop DRAM-first path.
 * The caller supplies reserved, unencrypted physical backing, its actual CPU
 * mapping and populated frames. admit must establish that mapping, capacity,
 * absence of physical aliases, exclusion of PSP/SMM/firmware/other owners,
 * DRAM rather than CAR, and ownership of all CPU stores to these buffers.
 * Geometry checks alone do not establish a DMA lease. may_access repeats the
 * ownership and enclosing-budget checks and must be bounded.
 *
 * prepare(3/6) publishes platform/ancillary loads; prepare(14) publishes the
 * platform result target. consume is called only after successful operation14
 * completion, before the CPU reads or changes the returned frame. Both use the
 * common x86 WBINVD primitive, preserving frame bytes and writing back dirty
 * lines before invalidation. Do not substitute INVD or clear the result frame.
 *
 * Snapshot and partial cache-operation state persist after failure. A pending
 * result cannot be republished or implicitly retried. Actual SMU completion,
 * external ownership and physical Matisse DMA/coherency need target evidence.
 */
enum cb_err matisse_begin_dxio_dma(const struct matisse_dxio_dma_input *input,
		struct matisse_dxio_dma_state *state, const struct matisse_dxio_dma_io *io);

enum cb_err matisse_prepare_dxio_dma(struct matisse_dxio_dma_state *state,
		unsigned int use, const struct matisse_dxio_dma_io *io);

enum cb_err matisse_consume_dxio_dma(struct matisse_dxio_dma_state *state,
		const struct matisse_dxio_dma_io *io);

#endif /* AMD_MATISSE_DXIO_DMA_H */
