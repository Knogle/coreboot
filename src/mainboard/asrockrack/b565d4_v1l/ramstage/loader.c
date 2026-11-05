/* SPDX-License-Identifier: GPL-2.0-only */
#include "native84_profile.h"
#include "native84_bounded_rmodule_load.h"
#include <cbfs.h>
#include <cbmem.h>
#include <commonlib/bsd/cbfs_mdata.h>
#include <commonlib/bsd/compression.h>
#include <commonlib/endian.h>
#include <limits.h>
#include <program_loading.h>
#include <rmodule.h>

static int no_compression(const union cbfs_mdata *mdata)
{
	const struct cbfs_file_attr_compression *attr = cbfs_find_attr(mdata,
		CBFS_FILE_ATTR_TAG_COMPRESSION, sizeof(*attr));

	return !attr || be32toh(attr->compression) == CBFS_COMPRESS_NONE;
}

int native84_romstage_file_ok(const union cbfs_mdata *mdata, size_t bytes)
{
	const struct cbfs_file_attr_stageheader *attr;
	uint64_t load;
	uint32_t memory, entry;

	native84_stage_check();
	if (!mdata || !bytes || bytes > INT32_MAX ||
	    be32toh(mdata->h.type) != CBFS_TYPE_STAGE || !no_compression(mdata))
		return 0;
	attr = cbfs_find_attr(mdata, CBFS_FILE_ATTR_TAG_STAGEHEADER, sizeof(*attr));
	if (!attr)
		return 0;
	load = be64toh(attr->loadaddr);
	memory = be32toh(attr->memlen);
	entry = be32toh(attr->entry_offset);
	/* Bound the complete genuine decoder/copy/BSS destination before stores.
	 * Root separately binds exact immutable medium and emitted entry identity.
	 */
	return load >= NATIVE84_ROMSTAGE_BEGIN && load < NATIVE84_ROMSTAGE_END &&
		memory && memory <= NATIVE84_ROMSTAGE_END - load &&
		bytes <= memory && entry < bytes;
}

#if ENV_SEPARATE_ROMSTAGE
struct allocation_state {
	uint32_t id;
	uint32_t region, region_bytes, source, source_bytes;
	unsigned int attempted;
};

static void *ramstage_allocator(void *arg, size_t bytes, const union cbfs_mdata *mdata)
{
	struct allocation_state *state = arg;
	const struct cbfs_file_attr_stageheader *attr;
	const struct cbmem_entry *entry;
	uint32_t memory, need, file_need, region_bytes;
	void *region;

	native84_stage_check();
	if (!state || state->attempted || !mdata || bytes < 64 || bytes > INT32_MAX ||
	    be32toh(mdata->h.type) != CBFS_TYPE_STAGE)
		return NULL;
	state->attempted = 1;
	attr = cbfs_find_attr(mdata, CBFS_FILE_ATTR_TAG_STAGEHEADER, sizeof(*attr));
	if (!attr)
		return NULL;
	memory = be32toh(attr->memlen);
	/* Keep the genuine page-before-program, header immediately before D
	 * layout; cover BOTH the decoded serialized file and complete program.
	 */
	if (!memory || memory > INT32_MAX - 8191 || bytes > INT32_MAX - 8191)
		return NULL;
	need = memory + 4096;
	file_need = bytes + 4096 - sizeof(struct rmodule_header);
	if (need < file_need)
		need = file_need;
	region_bytes = (need + 4095) & ~UINT32_C(4095);
	if (region_bytes > NATIVE84_PARENT_END - NATIVE84_CBMEM_BEGIN ||
	    cbmem_find(state->id))
		return NULL;
	region = cbmem_add(state->id, region_bytes);
	if (!region)
		return NULL;
	state->region = (uintptr_t)region;
	state->region_bytes = region_bytes;
	if (state->region < NATIVE84_CBMEM_BEGIN ||
	    state->region >= NATIVE84_PARENT_END ||
	    region_bytes > NATIVE84_PARENT_END - state->region || (state->region & 4095))
		return NULL;
	entry = cbmem_entry_find(state->id);
	if (!entry || cbmem_entry_start(entry) != region ||
	    cbmem_entry_size(entry) != region_bytes)
		return NULL;
	state->source = state->region + 4096 - sizeof(struct rmodule_header);
	state->source_bytes = bytes;
	return (void *)(uintptr_t)state->source;
}

int native84_rmodule_stage_load(struct rmod_stage_load *rsl)
{
	struct allocation_state allocation = { 0 };
	struct native84_interval protected[NATIVE84_MAX_PROTECTED];
	struct native84_bounded_load_request request;
	struct native84_bounded_load_output output;
	size_t actual = 0;
	void *source;

	native84_stage_check();
	if (!rsl || !rsl->prog || !prog_name(rsl->prog) ||
	    prog_type(rsl->prog) != PROG_RAMSTAGE || rsl->cbmem_id != CBMEM_ID_RAMSTAGE ||
	    !cbmem_online() || prog_locate_hook(rsl->prog))
		return -1;
	allocation.id = rsl->cbmem_id;
	/* Genuine lookup/verification/decoder, strictly reported initialized size.
	 * Any failure after allocator/decoder writes is terminal at the caller;
	 * no second load, free, rollback or unchecked accessor is attempted.
	 */
	source = cbfs_alloc_exact(prog_name(rsl->prog), ramstage_allocator,
		&allocation, &actual);
	if (!source || !allocation.attempted || (uintptr_t)source != allocation.source ||
	    actual != allocation.source_bytes || !actual)
		return -1;
	request.source = source;
	request.source_bytes = actual;
	request.destination = allocation.region + 4096;
	request.allocation = (struct native84_interval) {
		allocation.region, allocation.region + allocation.region_bytes
	};
	request.descriptor = native84_descriptor();
	request.expected_epoch = NATIVE84_EPOCH;
	request.protected = protected;
	request.protected_count = native84_protected(protected, NATIVE84_MAX_PROTECTED);
	/* Real preflight -> rmodule_parse -> rmodule_load -> descriptor attachment.
	 * The complete loader workspace is in the excluded current EARLYRAM stack.
	 */
	if (native84_bounded_rmodule_load(&request, &output) != NATIVE84_LOAD_VALID)
		return -1;
	rsl->prog->cbfs_type = CBFS_TYPE_STAGE;
	prog_set_area(rsl->prog, (void *)(uintptr_t)output.program.begin,
		output.program.end - output.program.begin);
	rsl->params = (void *)(uintptr_t)output.parameters;
	prog_set_entry(rsl->prog, (void *)(uintptr_t)output.entry, rsl->params);
	/* run_ramstage still overrides this with the genuine cbmem_top argument. */
	return 0;
}
#endif
