/* SPDX-License-Identifier: GPL-2.0-only */

#include <arch/ebda.h>
#include <commonlib/endian.h>
#include <console/console.h>
#include <cpu/intel/model_206cx/cpu_init.h>
#include <cpu/intel/model_206cx/mca_diagnostic.h>
#include <halt.h>
#include <stdint.h>
#include "platform_acpi.h"

/* This is the temporary coreboot EBDA, not a promise about SeaBIOS's layout.
 * Bootmem conservatively reserves the containing page, 0x9f000--0x9ffff.
 * No shadow/PAM or silicon policy is changed by this observation hook.
 */
_Static_assert(CONFIG_DEFAULT_EBDA_LOWMEM == X58_PLATFORM_EBDA_BASE,
	       "PLATFORM conventional RAM must end at the temporary EBDA");
_Static_assert((CONFIG_DEFAULT_EBDA_SEGMENT << 4) == X58_PLATFORM_EBDA_BASE,
	       "PLATFORM EBDA segment/resource mismatch");
_Static_assert(CONFIG_DEFAULT_EBDA_SIZE == X58_PLATFORM_EBDA_SIZE,
	       "PLATFORM EBDA size/resource mismatch");
_Static_assert(X58_PLATFORM_EBDA_BASE + X58_PLATFORM_EBDA_SIZE == 0xa0000,
	       "PLATFORM EBDA must stay below the VGA aperture");

static unsigned int ebda_phase;

static void ebda_fault(const char *reason)
{
	printk(BIOS_EMERG, "[EBDA] STOP: %s; evidence retained, no CPU init\n", reason);
	model_206cx_mca_fault();
	/* Even a replacement/absent fault console cannot accidentally continue. */
	die("[EBDA] diagnostic endpoint returned\n");
}

void mainboard_ebda_init(bool before)
{
	if (!x58_platform_ebda_reserved() || ebda_phase != (before ? 0u : 1u)) {
		ebda_fault("reservation/order mismatch");
		return;
	}
	printk(BIOS_NOTICE, "[EBDA] %s BASE=%08x SIZE=%04x LOWMEM_KB=%u "
		"LEGACY_SHADOW_ACCESS=0\n", before ? "PRE" : "POST",
		X58_PLATFORM_EBDA_BASE, X58_PLATFORM_EBDA_SIZE, CONFIG_DEFAULT_EBDA_LOWMEM >> 10);
	if (!model_206cx_mca_report(before ? "PRE-EBDA" : "POST-EBDA")) {
		ebda_fault(before ? "MCA before initialization" : "MCA after initialization");
		return;
	}
	if (!before) {
		const uint8_t *const ebda = (const void *)(uintptr_t)X58_PLATFORM_EBDA_BASE;
		const uint16_t segment = read_le16((const void *)0x40e);
		const uint16_t lowmem = read_le16((const void *)0x413);
		const uint16_t size_kb = read_le16(ebda);
		if (segment != CONFIG_DEFAULT_EBDA_SEGMENT ||
		    lowmem != (CONFIG_DEFAULT_EBDA_LOWMEM >> 10) ||
		    size_kb != (X58_PLATFORM_EBDA_SIZE >> 10)) {
			ebda_fault("BDA/EBDA header readback mismatch");
			return;
		}
		for (unsigned int i = 2; i < X58_PLATFORM_EBDA_SIZE; ++i) {
			if (ebda[i]) {
				ebda_fault("EBDA clear readback mismatch");
				return;
			}
		}
		printk(BIOS_NOTICE, "[EBDA] READBACK_OK SEG=%04x LOWMEM_KB=%u SIZE_KB=%u\n",
			segment, lowmem, size_kb);
	}
	++ebda_phase;
}
