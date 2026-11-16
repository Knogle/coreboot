/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _IMD_H_
#define _IMD_H_

#include <stdint.h>
#include <stddef.h>

/*
 * imd is an in-memory database/directory/datastore (whatever d word you
 * desire). It grows downwards in memory from provided upper limit and
 * root size. Each entry has a size alignment which is also provided by
 * the caller.
 *
 *             +----------------------+ <- upper_limit
 *    |   +----|   root pointer       |
 *    |   |    +----------------------+
 *    |   |    |                      |--------+
 *    |   +--->|   root block         |-----+  |
 *    |        +----------------------+-----|--|--- root_size
 *    |        |                      |     |  |
 *    |        |                      |     |  |
 *    |        |   alloc N            |<----+  |
 *    |        +----------------------+        |
 *    |        |                      |        |
 *    |        |                      |        |
 *   \|/       |   alloc N + 1        |<-------+
 *    v        +----------------------+
 *
 * The root_size in imd_create_empty() encompasses the root pointer
 * and root block. The root_size value, therefore, dictates the number
 * of allocations maintained by the imd.
 */

/*
 * NOTE: This API has the following calling conventions: all functions
 * returning int supply 0 on success or < 0 on error.
 */

struct imd_entry;
struct imd;

static const size_t LIMIT_ALIGN = 4096;

/*
 * Initialize handle to use for working with an imd. Upper limit is the
 * exclusive address (aligned down to LIMIT_ALIGN) to start allocating down
 * from. This function needs to be called at least once before any other imd
 * related functions can be used.
 */
void imd_handle_init(struct imd *imd, void *upper_limit);

/*
 * Initialize a handle with a shallow recovery. This function doesn't
 * verify every entry, but it does set up the root pointer. Because of
 * this behavior it's not very safe. However, the current CBMEM constraints
 * demand having these semantics.
 */
void imd_handle_init_partial_recovery(struct imd *imd);

/*
 * Create an empty imd with a specified root_size and each entry is aligned to
 * the provided entry_align. As noted above the root size encompasses the
 * root pointer and root block leading to the number of imd entries being a
 * function of the root_size parameter. Please note, that one entry is allocated
 * for covering root region, thus caller should consider this calculating
 * root_size.
 */
int imd_create_empty(struct imd *imd, size_t root_size, size_t entry_align);

/*
 * Create an empty imd with both large and small allocations. The small
 * allocations come from a fixed imd stored internally within the large
 * imd. The region allocated for tracking the smaller allocations is dependent
 * on the small root_size and the large entry alignment by calculating the
 * number of entries within the small imd and multiplying that by the small
 * entry alignment.
 */
int imd_create_tiered_empty(struct imd *imd,
				size_t lg_root_size, size_t lg_entry_align,
				size_t sm_root_size, size_t sm_entry_align);

/*
 * Recover a previously created imd.
 */
int imd_recover(struct imd *imd);

/*
 * Opt-in two-tier APIs for a caller-qualified stable [lower_limit, upper) arena.
 * upper is the exact limit already established in the initialized handle.
 * The nonzero span must be at most INT32_MAX; root/entry parameters retain
 * the legacy power-of-two and LIMIT_ALIGN size restrictions. The lower bound
 * need not be page-aligned. Each root address must meet its entry alignment.
 * Both interval endpoints, including exclusive upper, must stay in the same
 * intptr_t signed-address half while legacy relative-pointer APIs are used.
 *
 * Creation validates the complete two-tier footprint before any arena store,
 * and fixes each tier's max_offset before exposing it for allocation.
 * Recovery checks every pointer/header/table/entry before following it; a
 * complete, uniquely backed small tier is required. No shallow recovery or
 * unbounded max_offset is accepted. Both tiers publish together on success.
 *
 * Failure returns -1 and clears both handle roots and the small limit while
 * retaining the original large limit. Creation does not roll back arena bytes
 * if an unexpected internal allocation failure follows root initialization.
 * The caller must refuse/fail closed, not fall back to empty/OEM recovery.
 *
 * These APIs do not establish mapping, permissions, exclusive ownership or
 * retention. The valid writable caller-owned handle must be disjoint from
 * the arena. There must be no concurrent metadata writer during recovery or
 * later use. The caller must preserve these limits and the internal root and
 * small-backing entries; legacy reinitialization, partial recovery or a wider
 * imd_limit_size() is outside this contract. Existing callers do not
 * automatically use these bounds.
 */
int imd_create_tiered_empty_bounded(struct imd *imd,
				size_t lg_root_size, size_t lg_entry_align,
				size_t sm_root_size, size_t sm_entry_align,
				void *lower_limit);
int imd_recover_bounded(struct imd *imd, void *lower_limit);

/* Limit imd to provided max_size. */
int imd_limit_size(struct imd *imd, size_t max_size);

/* Lock down imd from further modifications. */
int imd_lockdown(struct imd *imd);

/* Fill in base address and size of region used by imd. */
int imd_region_used(struct imd *imd, void **base, size_t *size);

/* Add an entry to the imd. If id already exists NULL is returned. */
const struct imd_entry *imd_entry_add(const struct imd *imd, uint32_t id,
					size_t size);

/* Locate an entry within the imd. NULL is returned when not found. */
const struct imd_entry *imd_entry_find(const struct imd *imd, uint32_t id);

/* Find an existing entry or add a new one. */
const struct imd_entry *imd_entry_find_or_add(const struct imd *imd,
						uint32_t id, size_t size);

/* Returns size of entry. */
size_t imd_entry_size(const struct imd_entry *entry);

/* Returns pointer to region described by entry or NULL on failure. */
void *imd_entry_at(const struct imd *imd, const struct imd_entry *entry);

/* Returns id for the imd entry. */
uint32_t imd_entry_id(const struct imd_entry *entry);

/* Attempt to remove entry from imd. */
int imd_entry_remove(const struct imd *imd, const struct imd_entry *entry);

/* Print the entry information provided by lookup with the specified size. */
struct imd_lookup {
	uint32_t id;
	const char *name;
};

int imd_print_entries(const struct imd *imd, const struct imd_lookup *lookup,
			size_t size);

struct imd_cursor;
/* Initialize an imd_cursor object to walk the IMD entries. */
int imd_cursor_init(const struct imd *imd, struct imd_cursor *cursor);

/* Retrieve the next imd entry the cursor is referencing. Returns NULL when
 * no more entries exist. */
const struct imd_entry *imd_cursor_next(struct imd_cursor *cursor);

/*
 * The struct imd is a handle for working with an in-memory directory.
 *
 * NOTE: Do not directly touch any fields within this structure. An imd pointer
 * is meant to be opaque, but the fields are exposed for stack allocation.
 */
struct imdr {
	uintptr_t limit;
	void *r;
};
struct imd {
	struct imdr lg;
	struct imdr sm;
};

struct imd_cursor {
	size_t current_imdr;
	size_t current_entry;
	const struct imdr *imdr[2];
};

#endif /* _IMD_H_ */
