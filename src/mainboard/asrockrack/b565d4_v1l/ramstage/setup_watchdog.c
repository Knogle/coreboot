/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/acpimmio.h>
#include <amdblocks/iomap.h>
#include <bootstate.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <device/mmio.h>
#include <device/pci_def.h>
#include <device/pci_ops.h>
#include "setup_options.h"

/* FCH register layout shared with Linux sp5100_tco's efch_mmio backend. */
#define WDT_DECODE_ENABLE BIT(7)
#define WDT_PM_CONFIG 0x03
#define WDT_SECOND_RESOLUTION 3
#define WDT_DISABLE_MASK (BIT(2) | BIT(3))
#define WDT_START BIT(0)
#define WDT_FIRED BIT(1)
#define WDT_POWER_OFF BIT(2)
#define WDT_TRIGGER BIT(7)

static bool guard_active;

static void watchdog_stop(void)
{
	uint32_t control = read32p(FCH_WATCHDOG_BASE);

	/* FIRED is W1C: preserve the previous reset indication on a stop. */
	write32p(FCH_WATCHDOG_BASE, control & ~(WDT_START | WDT_FIRED | WDT_TRIGGER));
}

void b565d4_setup_watchdog(void)
{
	pci_devfn_t smbus = PCI_DEV(0, 0x14, 0);
	unsigned int policy = b565d4_setup_get(B565D4_OPT_WATCHDOG);
	uint8_t decode;
	uint32_t control;

	if (pci_s_read_config32(smbus, PCI_VENDOR_ID) != 0x790b1022 ||
	    pci_s_read_config8(smbus, PCI_REVISION_ID) < 0x51) {
		printk(BIOS_WARNING, "Board watchdog: unsupported FCH register layout\n");
		return;
	}
	decode = pm_read8(PM_DECODE_EN);
	pm_write8(PM_DECODE_EN, decode | WDT_DECODE_ENABLE);
	if (!(pm_read8(PM_DECODE_EN) & WDT_DECODE_ENABLE)) {
		printk(BIOS_ERR, "Board watchdog: MMIO decode unavailable\n");
		return;
	}
	control = read32p(FCH_WATCHDOG_BASE);
	if (control == UINT32_MAX) {
		printk(BIOS_ERR, "Board watchdog: inaccessible control register\n");
		return;
	}
	printk(BIOS_NOTICE, "Board watchdog: policy=%u prior control=%08x fired=%u\n",
	       policy, control, !!(control & WDT_FIRED));
	watchdog_stop();
	if (!policy) {
		pm_write8(PM_DECODE_EN, decode & ~WDT_DECODE_ENABLE);
		return;
	}
	pm_write8(WDT_PM_CONFIG,
		  (pm_read8(WDT_PM_CONFIG) & ~WDT_DISABLE_MASK & ~3) |
		  WDT_SECOND_RESOLUTION);
	if (policy != 2)
		return; /* Stopped, decoded, ready for the ordinary OS driver. */

	write32p(FCH_WATCHDOG_BASE + 4, 60);
	control = read32p(FCH_WATCHDOG_BASE) & ~(WDT_POWER_OFF | WDT_TRIGGER);
	/* Clear a recorded old expiry only after reporting it above. */
	write32p(FCH_WATCHDOG_BASE, control | WDT_FIRED | WDT_START);
	write32p(FCH_WATCHDOG_BASE, (control & ~WDT_FIRED) | WDT_START | WDT_TRIGGER);
	guard_active = !!(read32p(FCH_WATCHDOG_BASE) & WDT_START);
	printk(guard_active ? BIOS_NOTICE : BIOS_ERR,
	       "Board watchdog: coreboot 60-second guard active=%u\n", guard_active);
}

static void watchdog_ping(void *unused)
{
	if (guard_active) {
		uint32_t control = read32p(FCH_WATCHDOG_BASE);

		write32p(FCH_WATCHDOG_BASE, (control & ~WDT_FIRED) | WDT_TRIGGER);
	}
}

static void watchdog_handoff(void *unused)
{
	if (!guard_active)
		return;
	watchdog_stop();
	guard_active = false;
	printk(BIOS_NOTICE, "Board watchdog: guard stopped before payload; OS owns timer\n");
}

BOOT_STATE_INIT_ENTRY(BS_DEV_RESOURCES, BS_ON_ENTRY, watchdog_ping, NULL);
BOOT_STATE_INIT_ENTRY(BS_DEV_INIT, BS_ON_ENTRY, watchdog_ping, NULL);
BOOT_STATE_INIT_ENTRY(BS_WRITE_TABLES, BS_ON_ENTRY, watchdog_ping, NULL);
BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_LOAD, BS_ON_ENTRY, watchdog_ping, NULL);
BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_BOOT, BS_ON_ENTRY, watchdog_handoff, NULL);
BOOT_STATE_INIT_ENTRY(BS_OS_RESUME, BS_ON_ENTRY, watchdog_handoff, NULL);
