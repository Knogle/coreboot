/* SPDX-License-Identifier: GPL-2.0-only */
/* PLATFORM is an operator-selected, cold-boot-only path; not SMM emulation. */
#include <acpi/acpigen.h>
#include <arch/io.h>
#include <arch/pci_io_cfg.h>
#include <bootmem.h>
#include <console/console.h>
#include <cpu/x86/smm.h>
#include <device/pci.h>
#include <device/resource.h>
#include <string.h>
#include <stdio.h>
#include <symbols.h>
#include <commonlib/helpers.h>
#include "platform_acpi.h"
#include "fintek_hwm.h"
#include "hda_recovery.h"
#include "acpi_resources.h"
#include "ich10_gpe_policy.h"
#include "legacy_io.h"
#include "../../../cpu/intel/model_206cx/cpu_init.h"

/* Match the previously selected ROMMON profile without replaying commands.
 * Legacy PLATFORM builds keep their original native/BSP-only startup policy.
 */
static bool smp_requested = true;
/* Boot-local request only: no CMOS/NVS persistence or MSR access here. */
static bool msr2e2_requested = true;
static bool frozen;
static bool ebda_reserved;

bool x58_platform_smp_requested(void) { return smp_requested; }
bool x58_platform_msr2e2_requested(void) { return msr2e2_requested; }
bool x58_platform_ebda_reserved(void) { return ebda_reserved; }
uint16_t x58_platform_pmbase(void) { return 0x500; }
uint16_t x58_platform_gpiobase(void) { return 0x580; }

void x58_platform_status(void)
{
	printk(BIOS_NOTICE, "[PLATFORM] ACPI=native SMP=%u FROZEN=%u PM=%04x GPIO=%04x "
		"CPU_COUNT=%u MSR2E2_REQUEST=%s MSR2E2_ON_VALUE=2 NO_SMM=1\n",
		smp_requested, frozen, x58_platform_pmbase(), x58_platform_gpiobase(),
		model_206cx_cpu_count(), msr2e2_requested ? "ON" : "OFF");
	x58_platform_legacy_status();
	for (unsigned int i = 0; i < model_206cx_cpu_count(); i++)
		printk(BIOS_DEBUG, "[PLATFORM] CPU index=%u APIC=%u ACPI_UID=%u\n",
			i, model_206cx_apic_id(i), i);
}

bool x58_platform_command(int argc, char **argv)
{
	if (x58_hda_command(argc, argv))
		return true;
	if (x58_platform_legacy_command(argc, argv, frozen))
		return true;
	if (argc == 1 && !strcmp(argv[0], "acpi")) {
		const uintptr_t address = get_coreboot_rsdp();
		printk(BIOS_NOTICE, "[PLATFORM] RSDP=%lx tables=%s\n",
			(unsigned long)address, address ? "published" : "not-built");
		if (address) {
			const acpi_rsdp_t *r = (const void *)address;
			const acpi_rsdt_t *t = (const void *)(uintptr_t)r->rsdt_address;
			if (t && t->header.length >= sizeof(acpi_header_t) &&
			    t->header.length <= sizeof(*t))
				for (unsigned int i = 0;
				     i < (t->header.length - sizeof(acpi_header_t)) / 4; i++) {
					const acpi_header_t *h = (const void *)(uintptr_t)t->entry[i];
					printk(BIOS_NOTICE, "[PLATFORM] %.4s @%08x size=%u checksum=%02x\n",
						h->signature, t->entry[i], h->length, h->checksum);
				}
		}
		return true;
	}
	if (argc == 1 && !strcmp(argv[0], "platform")) {
		x58_platform_status();
		return true;
	}
	if (argc != 2 || (strcmp(argv[0], "smp") && strcmp(argv[0], "msr2e2")))
		return false;
	if (frozen) {
		printk(BIOS_ERR, "[PLATFORM] SMP/MSR2E2 frozen; cold boot to change them\n");
		return true;
	}
	bool *request = !strcmp(argv[0], "smp") ? &smp_requested : &msr2e2_requested;
	if (!strcmp(argv[1], "on"))
		*request = true;
	else if (!strcmp(argv[1], "off"))
		*request = false;
	else
		printk(BIOS_ERR, "[PLATFORM] %s on|off\n", argv[0]);
	x58_platform_status();
	return true;
}

void x58_platform_freeze_profile(void)
{
	frozen = true;
	x58_hda_freeze();
	x58_platform_status();
}

void x58_platform_reserve(struct device *dev)
{
	/* Allocation alone is not admission for the pre-EBDA write guard. */
	ebda_reserved = false;
	reserved_ram_range(dev, 0x92, SMM_DEFAULT_BASE, SMM_DEFAULT_SIZE);
	reserved_ram_range(dev, 0x93, X58_PLATFORM_EBDA_BASE, X58_PLATFORM_EBDA_SIZE);
}

void bootmem_platform_add_ranges(void)
{
	/* Generic MP has left its low-memory trampoline for ramstage C code and
	 * stacks before its final flight-plan barrier. Unlike a permanent SMM
	 * handler, this cold-boot scratch has no payload/OS lifetime. Keep the
	 * device resource reserved until here; change both bootmem maps before
	 * the coreboot memory table is emitted. No contents need restoring on
	 * this NO_SMM, no-S3 path (see cpu/x86/backup_default_smm.c).
	 */
	_Static_assert(CONFIG(NO_SMM) && !CONFIG(HAVE_SMI_HANDLER) &&
		!CONFIG(HAVE_ACPI_RESUME) && !CONFIG(PARALLEL_MP_AP_WORK),
		"SIPI reclaim requires the cold-boot-only, no-SMM parked-AP path");
	const unsigned int cpus = model_206cx_cpu_count();
	if (!frozen || !ebda_reserved || !cpus ||
	    cpus > CONFIG_MAX_CPUS || (!smp_requested && cpus != 1))
		die("[SIPI_HANDOFF] SIPI reclaim requires verified CPU/resource completion\n");
	if (bootmem_add_range_from(SMM_DEFAULT_BASE, SMM_DEFAULT_SIZE,
				   BM_MEM_RAM, BM_MEM_RESERVED))
		die("[SIPI_HANDOFF] SIPI scratch is not exclusively reserved; no handoff\n");
	printk(BIOS_DEBUG, "[SIPI_HANDOFF] SIPI_SCRATCH=%08x+%08x RESERVED->RAM "
		"VERIFIED_CPUS=%u NO_SMM=1 NO_S3=1\n",
		SMM_DEFAULT_BASE, SMM_DEFAULT_SIZE, cpus);
}

void x58_platform_verify_reservations(struct device *dev)
{
	const struct { unsigned int index; uint64_t base, size; unsigned long type; } expected[] = {
		{ 0x92, SMM_DEFAULT_BASE, SMM_DEFAULT_SIZE, IORESOURCE_MEM },
		{ 0x93, X58_PLATFORM_EBDA_BASE, X58_PLATFORM_EBDA_SIZE, IORESOURCE_MEM },
	};
	/* A repeated failed audit must not retain an earlier admission. */
	ebda_reserved = false;
	for (size_t i = 0; i < ARRAY_SIZE(expected); i++) {
		const struct resource *r = probe_resource(dev, expected[i].index);
		if (!r || r->base != expected[i].base || r->size != expected[i].size ||
		    (r->flags & IORESOURCE_TYPE_MASK) != expected[i].type ||
		    (r->flags & (IORESOURCE_FIXED | IORESOURCE_RESERVE)) !=
			(IORESOURCE_FIXED | IORESOURCE_RESERVE))
			die("[PLATFORM] SIPI/EBDA resource contract mismatch\n");
	}
	ebda_reserved = true;
}

const char *acpi_mainboard_dsdt_filename(void)
{
	return CONFIG_CBFS_PREFIX "/dsdt.aml";
}

const char *acpi_spcr_namespace(void)
{
	/* The native namespace reserves COM1 I/O but has no UART device. */
	return ".";
}

void x58_platform_fill_ssdt(const struct device *dev)
{
	const unsigned int count = model_206cx_cpu_count();
	(void)dev;
	x58_fintek_hwm_ssdt();
	if (!count || count > 12)
		die("[PLATFORM] ACPI CPU inventory unavailable\n");
	x58_platform_resource_ssdt();
	printk(BIOS_DEBUG, "[PCIE_ENUM] ACPI_ROOTS PCI0=00-fe UCFF=ff "
		"SEG=0 MCFG_NATIVE=1 DSDT_NATIVE=1\n");
	acpigen_write_scope("\\_SB");
	for (unsigned int i = 0; i < count; i++) {
		char name[5];
		snprintf(name, sizeof(name), "CP%02X", i);
		acpigen_write_device(name);
		acpigen_write_name_string("_HID", "ACPI0007");
		acpigen_write_name_integer("_UID", i);
		acpigen_pop_len();
	}
	acpigen_pop_len();
}

unsigned long x58_platform_write_tables(unsigned long current, acpi_rsdp_t *rsdp)
{
	(void)rsdp;
	x58_platform_status();
	return current;
}
