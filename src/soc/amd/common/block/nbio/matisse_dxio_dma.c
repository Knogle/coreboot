/* SPDX-License-Identifier: GPL-2.0-only */

#include <cpu/x86/cache.h>
#include <limits.h>
#include <amdblocks/matisse_dxio_dma.h>

static bool valid_range(const void *pointer, size_t bytes)
{
	return pointer && (uintptr_t)pointer <= UINTPTR_MAX - bytes;
}

static bool overlap(uint64_t a, uint64_t a_bytes, uint64_t b, uint64_t b_bytes)
{
	return a < b + b_bytes && b < a + a_bytes;
}

static bool valid_io(const struct matisse_dxio_dma_state *state,
		const struct matisse_dxio_dma_io *io)
{
	return valid_range(state, sizeof(*state)) && valid_range(io, sizeof(*io)) &&
		io->admit && io->may_access &&
		!overlap((uintptr_t)state, sizeof(*state), (uintptr_t)io, sizeof(*io));
}

static bool valid_buffer(void *mapping, uint64_t physical, uint32_t dwords)
{
	uint64_t bytes = (uint64_t)dwords * 4;

	return physical && physical < (1ULL << 32) && !(physical & 3) &&
		bytes <= SIZE_MAX && bytes <= (1ULL << 32) - physical &&
		!((uintptr_t)mapping & 3) && valid_range(mapping, (size_t)bytes);
}

static bool valid_input(const struct matisse_dxio_dma_input *input,
		struct matisse_dxio_dma_state *state, const struct matisse_dxio_dma_io *io)
{
	if (!valid_io(state, io) || !valid_range(input, sizeof(*input)) ||
	    overlap((uintptr_t)state, sizeof(*state), (uintptr_t)input, sizeof(*input)))
		return false;
	const struct matisse_dxio_init_buffers *wire = &input->wire;

	if (wire->platform_dwords < 16 || (wire->platform_dwords - 6) % 10 ||
	    (wire->platform_dwords - 6) / 10 > UINT16_MAX ||
	    !valid_buffer(input->platform, wire->platform, wire->platform_dwords))
		return false;
	if (wire->ancillary_dwords) {
		if (wire->ancillary_dwords < 3 || wire->ancillary_dwords > 256 ||
		    !valid_buffer(input->ancillary, wire->ancillary, wire->ancillary_dwords) ||
		    overlap(wire->platform, (uint64_t)wire->platform_dwords * 4,
			wire->ancillary, (uint64_t)wire->ancillary_dwords * 4) ||
		    overlap((uintptr_t)input->platform, (uint64_t)wire->platform_dwords * 4,
			(uintptr_t)input->ancillary, (uint64_t)wire->ancillary_dwords * 4))
			return false;
	} else if (wire->ancillary || input->ancillary) {
		return false;
	}
	const void *backing[] = { input->platform, input->ancillary };
	const size_t bytes[] = { (size_t)wire->platform_dwords * 4,
		(size_t)wire->ancillary_dwords * 4 };

	for (size_t i = 0; i < 2; i++) {
		if (!bytes[i])
			continue;
		if (overlap((uintptr_t)backing[i], bytes[i], (uintptr_t)state, sizeof(*state)) ||
		    overlap((uintptr_t)backing[i], bytes[i], (uintptr_t)io, sizeof(*io)) ||
		    overlap((uintptr_t)backing[i], bytes[i], (uintptr_t)input, sizeof(*input)))
			return false;
	}
	return true;
}

static bool available(struct matisse_dxio_dma_state *state,
		const struct matisse_dxio_dma_io *io)
{
	if (!io->may_access(&state->backing, io->context)) {
		state->phase = MATISSE_DXIO_DMA_FAILED;
		return false;
	}
	return true;
}

static enum cb_err synchronize(struct matisse_dxio_dma_state *state,
		const struct matisse_dxio_dma_io *io)
{
	state->phase = MATISSE_DXIO_DMA_SYNCHRONIZING;
	if (!available(state, io))
		return CB_ERR;
	state->cache_attempted++;
	wbinvd();
	state->cache_completed++;
	return available(state, io) ? CB_SUCCESS : CB_ERR;
}

enum cb_err matisse_begin_dxio_dma(const struct matisse_dxio_dma_input *input,
		struct matisse_dxio_dma_state *state, const struct matisse_dxio_dma_io *io)
{
	if (!valid_input(input, state, io) || state->phase != MATISSE_DXIO_DMA_NOT_STARTED)
		return CB_ERR;
	state->phase = MATISSE_DXIO_DMA_ADMITTING;
	state->backing = *input;
	if (io->admit(&state->backing, io->context) != CB_SUCCESS) {
		state->phase = MATISSE_DXIO_DMA_FAILED;
		return CB_ERR;
	}
	if (!available(state, io))
		return CB_ERR;
	state->phase = MATISSE_DXIO_DMA_CPU_READY;
	return CB_SUCCESS;
}

enum cb_err matisse_prepare_dxio_dma(struct matisse_dxio_dma_state *state,
		unsigned int use, const struct matisse_dxio_dma_io *io)
{
	if (!valid_io(state, io) || state->phase != MATISSE_DXIO_DMA_CPU_READY ||
	    (use != 3 && use != 6 && use != 14) ||
	    (use == 6 && !state->backing.wire.ancillary_dwords))
		return CB_ERR;
	state->last_use = use;
	state->operation = use == 14 ? MATISSE_DXIO_DMA_PUBLISH_RESULT :
		MATISSE_DXIO_DMA_PUBLISH_LOAD;
	if (synchronize(state, io) != CB_SUCCESS)
		return CB_ERR;
	if (use == 14)
		state->phase = MATISSE_DXIO_DMA_RESULT_PENDING;
	else {
		state->loads++;
		state->phase = MATISSE_DXIO_DMA_CPU_READY;
	}
	return CB_SUCCESS;
}

enum cb_err matisse_consume_dxio_dma(struct matisse_dxio_dma_state *state,
		const struct matisse_dxio_dma_io *io)
{
	if (!valid_io(state, io) || state->phase != MATISSE_DXIO_DMA_RESULT_PENDING)
		return CB_ERR;
	state->operation = MATISSE_DXIO_DMA_CONSUME_RESULT;
	if (synchronize(state, io) != CB_SUCCESS)
		return CB_ERR;
	state->result_reads++;
	state->phase = MATISSE_DXIO_DMA_CPU_READY;
	return CB_SUCCESS;
}
