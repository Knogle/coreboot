/* SPDX-License-Identifier: GPL-2.0-only */

#include <assert.h>
#include <boot/coreboot_tables.h>
#include <bootmem.h>
#include <console/console.h>
#include <cbmem.h>
#include <imd.h>
#include <lib.h>
#include <types.h>

/* The program loader passes on cbmem_top and the program entry point
   has to fill in the _cbmem_top_ptr symbol based on the calling arguments. */
uintptr_t _cbmem_top_ptr;

#if !CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
static struct imd imd;
#endif

uintptr_t cbmem_top(void)
{
	if (ENV_CREATES_CBMEM) {
		static uintptr_t top;
		if (top)
			return top;
		top = cbmem_top_chipset();
		return top;
	}
	if (ENV_POSTCAR || ENV_RAMSTAGE)
		return _cbmem_top_ptr;

	dead_code();
}

int cbmem_initialized;

static inline const struct cbmem_entry *imd_to_cbmem(const struct imd_entry *e)
{
	return (const struct cbmem_entry *)e;
}

static inline const struct imd_entry *cbmem_to_imd(const struct cbmem_entry *e)
{
	return (const struct imd_entry *)e;
}

void cbmem_initialize_empty(void)
{
	cbmem_initialize_empty_id_size(0, 0);
}

#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
/* The callback owns the access and stable-epoch premises, not this validator. */
struct cbmem_imd_context {
	struct imd *handle;
	uintptr_t lower;
	uintptr_t upper;
};

_Static_assert(sizeof(struct imd) == 4 * sizeof(uintptr_t),
		"Unexpected IMD handle layout");
#if __SIZEOF_POINTER__ == 4
_Static_assert(sizeof(struct imd) == 16, "Unexpected IA32 IMD handle size");
#endif

static struct cbmem_imd_context cbmem_external_handle_snapshot(void)
{
	struct cbmem_imd_context context;
	struct cbmem_imd_handle_storage storage;
	uintptr_t slot;
	uintptr_t bank_end;

	context.upper = cbmem_top();
	context.lower = cbmem_bottom_chipset();

	/* Validate the complete allocator interval before asking for its handle. */
	if (!context.lower || context.upper <= context.lower ||
	    (uintptr_t)(u32)context.upper != context.upper ||
	    context.lower % DYN_CBMEM_ALIGN_SIZE ||
	    context.upper % DYN_CBMEM_ALIGN_SIZE ||
	    context.upper - context.lower < cbmem_overhead_size() ||
	    context.upper - context.lower > INT32_MAX ||
	    (context.lower > INT32_MAX) != (context.upper > INT32_MAX))
		die("Invalid bounded CBMEM arena\n");

	storage = cbmem_imd_handle_storage_chipset();
	slot = (uintptr_t)storage.handle;
	if (!storage.bank_base || !storage.bank_bytes ||
	    storage.bank_bytes > UINTPTR_MAX - storage.bank_base)
		die("Invalid CBMEM handle bank\n");
	bank_end = storage.bank_base + storage.bank_bytes;

	/* Subtractions are safe only after these ordered nonwrapping endpoints. */
	if ((uintptr_t)(u32)storage.bank_base != storage.bank_base ||
	    (uintptr_t)(u32)bank_end != bank_end ||
	    storage.bank_bytes > INT32_MAX ||
	    (storage.bank_base > INT32_MAX) != (bank_end > INT32_MAX) ||
	    !slot || slot % _Alignof(struct imd) || slot < storage.bank_base ||
	    slot >= bank_end || sizeof(struct imd) > bank_end - slot ||
	    !(bank_end <= context.lower || storage.bank_base >= context.upper))
		die("Invalid external CBMEM handle storage\n");

	/* No handle or allocator memory has been accessed by this validator. */
	context.handle = storage.handle;
	return context;
}

static struct cbmem_imd_context cbmem_external_handle_init(void)
{
	struct cbmem_imd_context context = cbmem_external_handle_snapshot();

#if !CONFIG(CBMEM_BOUNDED_SKIP_RAM_CHECK)
	if (ENV_CREATES_CBMEM && ENV_X86)
		quick_ram_check_or_die(context.upper - sizeof(u32));
#endif

	/* The bank and complete handle span were checked before this first clear. */
	imd_handle_init(context.handle, (void *)context.upper);
	return context;
}

static void cbmem_external_add_initial(const struct cbmem_imd_context *context,
				      u32 id, u64 size)
{
	const struct imd_entry *entry;

	if (!size)
		return;
	if ((u64)(size_t)size != size)
		die("Bounded CBMEM initial allocation failed\n");
	entry = imd_entry_find_or_add(context->handle, id, size);
	if (entry == NULL || imd_entry_at(context->handle, entry) == NULL)
		die("Bounded CBMEM initial allocation failed\n");
}
#endif
#if CONFIG(CBMEM_BOUNDED_ARENA)
#if !CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
/* The callback contract supplies mapping, access, exclusion and lifetime. */
static uintptr_t cbmem_bounded_handle_init(void)
{
	uintptr_t upper = cbmem_top();
	uintptr_t lower = cbmem_bottom_chipset();

	/* This opt-in is deliberately limited to a below-4GiB, 32-bit arena. */
	if (!lower || upper <= lower || (uintptr_t)(u32)upper != upper ||
	    lower % DYN_CBMEM_ALIGN_SIZE || upper % DYN_CBMEM_ALIGN_SIZE ||
	    upper - lower < cbmem_overhead_size() || upper - lower > INT32_MAX ||
	    (lower > INT32_MAX) != (upper > INT32_MAX))
		die("Invalid bounded CBMEM arena\n");

	/* Use the validated snapshot; do not ask the top provider again. */
#if !CONFIG(CBMEM_BOUNDED_SKIP_RAM_CHECK)
	if (ENV_CREATES_CBMEM && ENV_X86)
		quick_ram_check_or_die(upper - sizeof(u32));
#endif

	imd_handle_init(&imd, (void *)upper);
	return lower;
}
#endif
#else
static void cbmem_top_init_once(void)
{
	/* Call one-time hook on expected cbmem init during boot. */
	if (!ENV_CREATES_CBMEM)
		return;

	/* The test is only effective on X86 and when address hits UC memory. */
	if (ENV_X86)
		quick_ram_check_or_die(cbmem_top() - sizeof(u32));
}
#endif

void cbmem_initialize_empty_id_size(u32 id, u64 size)
{
	const int no_recovery = 0;

#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	struct cbmem_imd_context context = cbmem_external_handle_init();
#else
#if CONFIG(CBMEM_BOUNDED_ARENA)
	uintptr_t lower = cbmem_bounded_handle_init();
#else
	cbmem_top_init_once();

	imd_handle_init(&imd, (void *)cbmem_top());
#endif
#endif

	printk(BIOS_DEBUG, "CBMEM:\n");

#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	if (imd_create_tiered_empty_bounded(context.handle, CBMEM_ROOT_MIN_SIZE,
					  CBMEM_LG_ALIGN, CBMEM_SM_ROOT_SIZE, CBMEM_SM_ALIGN,
					  (void *)context.lower))
		die("Bounded CBMEM creation failed\n");
#else
#if CONFIG(CBMEM_BOUNDED_ARENA)
	if (imd_create_tiered_empty_bounded(&imd, CBMEM_ROOT_MIN_SIZE, CBMEM_LG_ALIGN,
					CBMEM_SM_ROOT_SIZE, CBMEM_SM_ALIGN, (void *)lower))
		die("Bounded CBMEM creation failed\n");
#else
	if (imd_create_tiered_empty(&imd, CBMEM_ROOT_MIN_SIZE, CBMEM_LG_ALIGN,
					CBMEM_SM_ROOT_SIZE, CBMEM_SM_ALIGN)) {
		printk(BIOS_DEBUG, "failed.\n");
		return;
	}
#endif
#endif

#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	cbmem_external_add_initial(&context, id, size);
#else
#if CONFIG(CBMEM_BOUNDED_ARENA)
	if (size && cbmem_add(id, size) == NULL)
		die("Bounded CBMEM initial allocation failed\n");
#else
	/* Add the specified range first */
	if (size)
		cbmem_add(id, size);
#endif
#endif

	/* Complete migration to CBMEM. */
	cbmem_run_init_hooks(no_recovery);

	cbmem_initialized = 1;
}

int cbmem_initialize(void)
{
	return cbmem_initialize_id_size(0, 0);
}

int cbmem_initialize_id_size(u32 id, u64 size)
{
	const int recovery = 1;

#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	struct cbmem_imd_context context = cbmem_external_handle_init();
#else
#if CONFIG(CBMEM_BOUNDED_ARENA)
	uintptr_t lower = cbmem_bounded_handle_init();
#else
	cbmem_top_init_once();

	imd_handle_init(&imd, (void *)cbmem_top());
#endif
#endif

#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	if (imd_recover_bounded(context.handle, (void *)context.lower))
		die("Bounded CBMEM recovery failed\n");
#else
#if CONFIG(CBMEM_BOUNDED_ARENA)
	if (imd_recover_bounded(&imd, (void *)lower))
		die("Bounded CBMEM recovery failed\n");
#else
	if (imd_recover(&imd))
		return 1;
#endif
#endif

	/*
	 * Lock the imd in romstage on a recovery. The assumption is that
	 * if the imd area was recovered in romstage then S3 resume path
	 * is being taken.
	 */
#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	if (ENV_CREATES_CBMEM)
		imd_lockdown(context.handle);
#else
	if (ENV_CREATES_CBMEM)
		imd_lockdown(&imd);
#endif

#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	cbmem_external_add_initial(&context, id, size);
#else
#if CONFIG(CBMEM_BOUNDED_ARENA)
	if (size && cbmem_add(id, size) == NULL)
		die("Bounded CBMEM initial allocation failed\n");
#else
	/* Add the specified range first */
	if (size)
		cbmem_add(id, size);
#endif
#endif

	/* Complete migration to CBMEM. */
	cbmem_run_init_hooks(recovery);

	cbmem_initialized = 1;

	/* Recovery successful. */
	return 0;
}

int cbmem_recovery(bool is_wakeup)
{
	int rv = 0;
	if (!is_wakeup)
		cbmem_initialize_empty();
	else
		rv = cbmem_initialize();
	return rv;
}

const struct cbmem_entry *cbmem_entry_add(u32 id, u64 size64)
{
#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	struct cbmem_imd_context context = cbmem_external_handle_snapshot();
#endif
	const struct imd_entry *e;
#if CONFIG(CBMEM_BOUNDED_ARENA)
	if ((u64)(size_t)size64 != size64)
		return NULL;
#endif

#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	e = imd_entry_find_or_add(context.handle, id, size64);
#else
	e = imd_entry_find_or_add(&imd, id, size64);
#endif

	return imd_to_cbmem(e);
}

void *cbmem_add(u32 id, u64 size)
{
#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	struct cbmem_imd_context context = cbmem_external_handle_snapshot();
#endif
	const struct imd_entry *e;
#if CONFIG(CBMEM_BOUNDED_ARENA)
	if ((u64)(size_t)size != size)
		return NULL;
#endif

#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	e = imd_entry_find_or_add(context.handle, id, size);
#else
	e = imd_entry_find_or_add(&imd, id, size);
#endif

	if (e == NULL)
		return NULL;

#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	return imd_entry_at(context.handle, e);
#else
	return imd_entry_at(&imd, e);
#endif
}

/* Retrieve a region provided a given id. */
const struct cbmem_entry *cbmem_entry_find(u32 id)
{
#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	struct cbmem_imd_context context = cbmem_external_handle_snapshot();
#endif
	const struct imd_entry *e;

#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	e = imd_entry_find(context.handle, id);
#else
	e = imd_entry_find(&imd, id);
#endif

	return imd_to_cbmem(e);
}

void *cbmem_find(u32 id)
{
#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	struct cbmem_imd_context context = cbmem_external_handle_snapshot();
#endif
	const struct imd_entry *e;

#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	e = imd_entry_find(context.handle, id);
#else
	e = imd_entry_find(&imd, id);
#endif

	if (e == NULL)
		return NULL;

#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	return imd_entry_at(context.handle, e);
#else
	return imd_entry_at(&imd, e);
#endif
}

/* Remove a reserved region. Returns 0 on success, < 0 on error. Note: A region
 * cannot be removed unless it was the last one added. */
int cbmem_entry_remove(const struct cbmem_entry *entry)
{
#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	struct cbmem_imd_context context = cbmem_external_handle_snapshot();
	return imd_entry_remove(context.handle, cbmem_to_imd(entry));
#else
	return imd_entry_remove(&imd, cbmem_to_imd(entry));
#endif
}

u64 cbmem_entry_size(const struct cbmem_entry *entry)
{
#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	(void)cbmem_external_handle_snapshot();
#endif
	return imd_entry_size(cbmem_to_imd(entry));
}

void *cbmem_entry_start(const struct cbmem_entry *entry)
{
#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	struct cbmem_imd_context context = cbmem_external_handle_snapshot();
	return imd_entry_at(context.handle, cbmem_to_imd(entry));
#else
	return imd_entry_at(&imd, cbmem_to_imd(entry));
#endif
}

void cbmem_add_bootmem(void)
{
	void *baseptr;
	size_t size;

	if (cbmem_get_region(&baseptr, &size)) {
		printk(BIOS_ERR, "CBMEM pointer/size not found!\n");
		return;
	}
	bootmem_add_range((uintptr_t)baseptr, size, BM_MEM_TABLE);
}

int cbmem_get_region(void **baseptr, size_t *size)
{
#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	struct cbmem_imd_context context = cbmem_external_handle_snapshot();
	return imd_region_used(context.handle, baseptr, size);
#else
	return imd_region_used(&imd, baseptr, size);
#endif
}

#if ENV_PAYLOAD_LOADER || (CONFIG(EARLY_CBMEM_LIST) && ENV_HAS_CBMEM)
/*
 * -fdata-sections doesn't work so well on read only strings. They all
 * get put in the same section even though those strings may never be
 * referenced in the final binary.
 */
void cbmem_list(void)
{
#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	struct cbmem_imd_context context = cbmem_external_handle_snapshot();
#endif
	static const struct imd_lookup lookup[] = { CBMEM_ID_TO_NAME_TABLE };

#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	imd_print_entries(context.handle, lookup, ARRAY_SIZE(lookup));
#else
	imd_print_entries(&imd, lookup, ARRAY_SIZE(lookup));
#endif
}
#endif

void cbmem_add_records_to_cbtable(struct lb_header *header)
{
#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	struct cbmem_imd_context context = cbmem_external_handle_snapshot();
#endif
	struct imd_cursor cursor;

#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
	if (imd_cursor_init(context.handle, &cursor))
#else
	if (imd_cursor_init(&imd, &cursor))
#endif
		return;

	while (1) {
		const struct imd_entry *e;
		struct lb_cbmem_entry *lbe;
		uint32_t id;

		e = imd_cursor_next(&cursor);

		if (e == NULL)
			break;

		id = imd_entry_id(e);
		/* Don't add these metadata entries. */
		if (id == CBMEM_ID_IMD_ROOT || id == CBMEM_ID_IMD_SMALL)
			continue;

		lbe = (struct lb_cbmem_entry *)lb_new_record(header);
		lbe->tag = LB_TAG_CBMEM_ENTRY;
		lbe->size = sizeof(*lbe);
#if CONFIG(CBMEM_BOUNDED_EXTERNAL_HANDLE)
		lbe->address = (uintptr_t)imd_entry_at(context.handle, e);
#else
		lbe->address = (uintptr_t)imd_entry_at(&imd, e);
#endif
		lbe->entry_size = imd_entry_size(e);
		lbe->id = id;
	}
}
