/* SPDX-License-Identifier: GPL-2.0-only */
#include "native84_profile.h"
#include "ramstage_contract.h"
#include <cbfs.h>
#include <cbmem.h>
#include <commonlib/bsd/cbfs_mdata.h>
#include <commonlib/bsd/compression.h>
#include <commonlib/endian.h>
#include <limits.h>
#include <program_loading.h>
#include <console/console.h>
#include <string.h>
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
/* Exact already-SHA-admitted LATE154/Stage77 image, not a generic untrusted
 * rmodule parser. The current verified-medium provider owns this epoch and
 * no caller writes M between this metadata check and the genuine load.
 */

_Static_assert(sizeof(void *) == 4 && sizeof(struct native84_handoff) == 56 &&
	sizeof(struct rmodule_header) == 64 && DYN_CBMEM_ALIGN_SIZE == 4096,
	"Qualified IA32 descriptor/rmodule/genuine allocator alignment");
_Static_assert(CONFIG_EARLYRAM_BSP_STACK_SIZE == 0x4000 &&
	CONFIG(CBMEM_BOUNDED_ARENA) && CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE) &&
	CONFIG(CBMEM_BOUNDED_SKIP_RAM_CHECK) && !CONFIG(VBOOT) &&
	!CONFIG(CBFS_PRELOAD), "Existing bounded, synchronous single-ROM trial");
_Static_assert(((RAM_STAGE_MEMORY + 4096 + 4095) & ~4095U) == RAM_REGION_BYTES &&
	RAM_SERIALIZED_BYTES <= RAM_REGION_BYTES - 4096 + 64 &&
	RAM_PARAMETERS_OFFSET + 56 <= RAM_INITIALIZED_BYTES,
	"Both decoded file and module/initialized parameters fit genuine allocation");

int __real_rmodule_stage_load(struct rmod_stage_load *rsl);
static unsigned int load_attempted;

static int fixed_ramstage_metadata(const char *name)
{
	union cbfs_mdata mdata;
	struct region_device rdev;
	const struct cbfs_file_attr_stageheader *stage;
	const struct cbfs_file_attr_compression *compression;
	void *mapping;
	int valid;

	if (strcmp(name, CONFIG_CBFS_PREFIX "/ramstage") ||
	    _cbfs_boot_lookup(name, true, &mdata, &rdev) != CB_SUCCESS)
		return 0;
	stage = cbfs_find_attr(&mdata, CBFS_FILE_ATTR_TAG_STAGEHEADER, sizeof(*stage));
	compression = cbfs_find_attr(&mdata, CBFS_FILE_ATTR_TAG_COMPRESSION,
		sizeof(*compression));
	if (!stage || !compression || be32toh(mdata.h.type) != CBFS_TYPE_STAGE ||
	    be32toh(mdata.h.len) != RAM_STORED_BYTES ||
	    region_device_sz(&rdev) != RAM_STORED_BYTES ||
	    be64toh(stage->loadaddr) || be32toh(stage->entry_offset) ||
	    be32toh(stage->memlen) != RAM_STAGE_MEMORY ||
	    be32toh(compression->compression) != CBFS_COMPRESS_LZMA ||
	    be32toh(compression->decompressed_size) != RAM_SERIALIZED_BYTES)
		return 0;
	mapping = rdev_mmap_full(&rdev);
	valid = mapping == (void *)(uintptr_t)(NATIVE84_MEDIA_BEGIN + RAM_CBFS_DATA_OFFSET);
	if (mapping)
		rdev_munmap(&rdev, mapping);
	return valid;
}

int native84_rmodule_stage_load(struct rmod_stage_load *rsl)
{
	const struct cbmem_entry *entry;
	const struct native84_handoff *descriptor;
	struct native84_handoff *parameters;
	const volatile uint8_t *stored;
	const uint8_t *saved;
	uintptr_t region, start;

	native84_stage_check();
	if (load_attempted || !rsl || !rsl->prog || !prog_name(rsl->prog) ||
	    prog_type(rsl->prog) != PROG_RAMSTAGE || rsl->cbmem_id != CBMEM_ID_RAMSTAGE ||
	    !cbmem_online() || cbmem_find(CBMEM_ID_RAMSTAGE))
		return -1;
	load_attempted = 1;
	if (!fixed_ramstage_metadata(prog_name(rsl->prog)))
		return -1;
	/* No private allocator/decode/preflight/parse/load call. The qualified
	 * fixed image satisfies the real loader's header, relocation and size
	 * premises; its cbmem_add stays inside the existing admitted arena.
	 * Failure after decoder/loader stores is terminal; no fallback/retry.
	 */
	if (__real_rmodule_stage_load(rsl))
		return -1;
	native84_stage_check();
	entry = cbmem_entry_find(CBMEM_ID_RAMSTAGE);
	if (!entry || cbmem_entry_size(entry) != RAM_REGION_BYTES)
		return -1;
	region = (uintptr_t)cbmem_entry_start(entry);
	if (region < NATIVE84_CBMEM_BEGIN || region >= NATIVE84_PARENT_END ||
	    (region & 4095) || RAM_REGION_BYTES > NATIVE84_PARENT_END - region)
		return -1;
	start = region + 4096;
	if ((uintptr_t)prog_start(rsl->prog) != start ||
	    prog_size(rsl->prog) != RAM_PROGRAM_BYTES ||
	    (uintptr_t)prog_entry(rsl->prog) != start ||
	    (uintptr_t)rsl->params != start + RAM_PARAMETERS_OFFSET ||
	    prog_entry_arg(rsl->prog) != rsl->params)
		return -1;
	/* Real rmodule_load has finished relocations and BSS clearing. Attach
	 * only the existing initialized56B parameter span, then leave genuine
	 * run_ramstage to override the entry argument with cbmem_top.
	 */
	descriptor = native84_descriptor();
	parameters = rsl->params;
	*parameters = *descriptor;
	stored = (const volatile uint8_t *)parameters;
	saved = (const uint8_t *)descriptor;
	for (size_t i = 0; i < sizeof(*parameters); i++) {
		if (stored[i] != saved[i])
			return -1;
	}
	rsl->prog->cbfs_type = CBFS_TYPE_STAGE;
	printk(BIOS_INFO, "NATIVE GENERIC RMODULE LOAD PASS program=%lx params=%lx "
		"bytes=%u allocation=%u; DESCRIPTOR56 ATTACHED\n",
		start, (uintptr_t)parameters, RAM_PROGRAM_BYTES, RAM_REGION_BYTES);
	return 0;
}
#endif
