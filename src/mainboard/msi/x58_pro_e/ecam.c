/* SPDX-License-Identifier: GPL-2.0-only */
/* Read-only prerequisite for the optional late standard PCI access backend. */
#include <arch/pci_io_cfg.h>
#include <bootstate.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <device/pci_ops.h>
#include <stddef.h>
#include <stdint.h>

#define X58_ECAM_BASE 0xe0000000u
#define X58_SAD PCI_DEV(0xff, 0, 1)
#define X58_SAD_ID 0x2d818086u
#define X58_PCIEXBAR_LO 0x50
#define X58_PCIEXBAR_HI 0x54

_Static_assert(1 && CONFIG(NO_SMM) &&
	CONFIG(ECAM_MMCONF_RAMSTAGE_ONLY) && PCI_CFG_USE_ECAM,
	"Late ECAM requires the ramstage-only, no-SMM profile");
_Static_assert(X58_ECAM_BASE == CONFIG_ECAM_MMCONF_BASE_ADDRESS &&
	256 == CONFIG_ECAM_MMCONF_BUS_NUMBER && 0x10000000 == CONFIG_ECAM_MMCONF_LENGTH,
	"Late ECAM requires the existing complete 256-bus decode");

static void x58_ecam_preflight(void *unused)
{
	static bool checked;
	static const struct {
		pci_devfn_t dev;
		uint32_t id;
	} roots[] = {
		{ PCI_DEV(0, 0, 0), 0x34058086 },
		{ PCI_DEV(0, 0x1c, 4), 0x3a488086 },
		{ PCI_DEV(0, 0x1f, 0), 0x3a168086 },
		{ X58_SAD, X58_SAD_ID },
	};
	(void)unused;
	if (checked)
		die("[X58-ECAM] repeated admission\n");
	/* Validate through CF8 before dereferencing any ECAM address. These are
	 * the existing X58_PCI SAD/decode values, not a new PCIEXBAR program. */
	if (pci_io_read_config32(X58_SAD, 0) != X58_SAD_ID ||
	    pci_io_read_config32(X58_SAD, X58_PCIEXBAR_LO) != (X58_ECAM_BASE | 1) ||
	    pci_io_read_config32(X58_SAD, X58_PCIEXBAR_HI) != 0)
		die("[X58-ECAM] missing exact inherited PCIEXBAR; no ECAM probe\n");
	for (size_t i = 0; i < ARRAY_SIZE(roots); ++i) {
		if (pci_io_read_config32(roots[i].dev, 0) != roots[i].id ||
		    pci_s_read_config32(roots[i].dev, 0) != roots[i].id)
			die("[X58-ECAM] fixed root identity/CF8 comparison failed\n");
	}
	checked = true;
	printk(BIOS_NOTICE, "[X58-ECAM] READY BASE=e0000000 BUSES=256 "
		"PRE_RAM=CF8 RAMSTAGE=ECAM ROOTS=4 FULL_ICH10=%u\n",
		1);
}

/* ON_ENTRY is before dev_initialize_chips(), which is where the full ICH10
 * owner would eventually access extended offsets 0x300 and 0x324. */
BOOT_STATE_INIT_ENTRY(BS_DEV_INIT_CHIPS, BS_ON_ENTRY, x58_ecam_preflight, NULL);
