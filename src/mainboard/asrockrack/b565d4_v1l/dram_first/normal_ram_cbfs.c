/* SPDX-License-Identifier: GPL-2.0-only */
#include <boot_device.h>
#include <commonlib/region.h>

/* Logical read-only media. These bytes are supplied by the separately
 * qualified original PSP 0x62 decompression, never by an SPI transaction. */
static const struct mem_region_device normal_ram_media =
	MEM_REGION_DEV_RO_INIT((void *)(uintptr_t)0x09c10000,
		0x80000);

const struct region_device *__wrap_boot_device_ro(void);
const struct region_device *__wrap_boot_device_ro(void)
{
	return &normal_ram_media.rdev;
}
