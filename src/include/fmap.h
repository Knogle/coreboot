/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _FMAP_H_
#define _FMAP_H_

#include <commonlib/bsd/fmap_serialized.h>
#include <commonlib/region.h>
#include <fmap_config.h>
#include <stddef.h>
#include <stdint.h>

#if CONFIG(BOARD_ASROCKRACK_B565D4_V1L)
#if FMAP_OFFSET != 0 || FMAP_SIZE != 182 || \
	FMAP_SECTION_B565D4_RAM_START != 0 || FMAP_SECTION_B565D4_RAM_SIZE != 0x80000 || \
	FMAP_SECTION_COREBOOT_START != 0x1000 || FMAP_SECTION_COREBOOT_SIZE != 0x7f000
#error "B565D4 logical memory medium geometry changed"
#endif
#elif CONFIG(SOC_AMD_CEZANNE_NATIVE)
/* This retained-entry profile uses a separately admitted 512KiB RAM medium.
 * CONFIG_ROM_SIZE continues to describe the unchanged physical vendor flash. */
#if FMAP_SECTION_NATIVE84_PARENT_MEDIUM_START != 0 || \
	FMAP_SECTION_NATIVE84_PARENT_MEDIUM_SIZE != 0x80000
	#error "Native84 RAM FMAP must match its exact 512KiB medium"
#endif
#if FMAP_OFFSET != 0 || FMAP_SIZE > 0x1000 || \
	FMAP_SECTION_FMAP_START != 0 || FMAP_SECTION_FMAP_SIZE != 0x1000 || \
	FMAP_SECTION_BIOS_START != 0 || \
	FMAP_SECTION_BIOS_SIZE != FMAP_SECTION_NATIVE84_PARENT_MEDIUM_SIZE || \
	FMAP_SECTION_COREBOOT_START != 0x1000 || \
	FMAP_SECTION_COREBOOT_SIZE != FMAP_SECTION_NATIVE84_PARENT_MEDIUM_SIZE - 0x1000
	#error "Unreviewed Native84 RAM FMAP/COREBOOT layout"
#endif
#else
#if FMAP_SECTION_FLASH_START != 0
	#error "FMAP must always start flash address 0"
#endif

/* This is usually the case. And it can to lead to confusion if one them is updated without the
 * other. There is however no code that has a hard dependency on this check. So if there comes
 * a platform at some point, which has a Flashmap which doesn't cover the whole flash, we can
 * remove the check again. */
#if FMAP_SECTION_FLASH_SIZE != CONFIG_ROM_SIZE
	#error "ROM_SIZE must always be equal to FMAP flash size"
#endif
#endif /* FMAP medium geometry */

/* Return the name of the boot region. Falls back to COREBOOT, if not overridden
 * by any multi-slot mechanism (e.g Intel Top Swap, vboot). */
const char *cbfs_fmap_region_hint(void);

/* Locate the named area in the fmap and fill in a region device representing
 * that area. The region is a sub-region of the readonly boot media. Return
 * 0 on success, < 0 on error. */
int fmap_locate_area_as_rdev(const char *name, struct region_device *area);

/* Just like fmap_locate_area_as_rdev(), however the region device is
 * created from the RW boot device. */
int fmap_locate_area_as_rdev_rw(const char *name, struct region_device *area);

/* Locate the named area in the fmap and fill in a region with the
 * offset and size of that area within the boot media. Return 0 on success,
 * < 0 on error. */
int fmap_locate_area(const char *name, struct region *r);

/* Find fmap area name by offset and size.
 * Return 0 on success, < 0 on error. */
int fmap_find_region_name(const struct region * const ar,
	char name[FMAP_STRLEN]);

/* Read fmap area into provided buffer.
 * Return size read on success, < 0 on error. */
ssize_t fmap_read_area(const char *name, void *buffer, size_t size);

/* Write provided buffer into fmap area.
 * Return size written on success, < 0 on error. */
ssize_t fmap_overwrite_area(const char *name, const void *buffer, size_t size);

/* Get offset of FMAP in flash. */
uint64_t get_fmap_flash_offset(void);

#endif
