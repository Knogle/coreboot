/* SPDX-License-Identifier: GPL-2.0-only */
#include "native84_profile.h"
#include <boot_device.h>
#include <commonlib/region.h>

/* Root15 copies the separately admitted logical medium into the fresh parent.
 * Physical SPI aliases, RAW member offset and writer are separate future APIs.
 */
static const struct mem_region_device native84_ram_media =
	MEM_REGION_DEV_RO_INIT((void *)(uintptr_t)NATIVE84_MEDIA_BEGIN,
		NATIVE84_MEDIA_BYTES);

const struct region_device *__wrap_boot_device_ro(void);
const struct region_device *__wrap_boot_device_ro(void)
{
	native84_stage_check();
	return &native84_ram_media.rdev;
}

void __wrap_boot_device_init(void);
void __wrap_boot_device_init(void)
{
	/* No SPI controller initialization for this immutable admitted RAM device. */
	native84_stage_check();
}
