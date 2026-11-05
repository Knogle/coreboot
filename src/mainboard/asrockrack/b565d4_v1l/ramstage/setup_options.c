/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/acpimmio.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <cpu/x86/msr.h>
#include <option.h>
#include <pc80/mc146818rtc.h>
#include <soc/southbridge.h>
#include "setup_options.h"

static const struct {
	const char *name;
	uint8_t byte;
	uint8_t fallback;
	uint8_t maximum;
} options[] = {
#define OPTION_ENTRY(id, name, byte, def, max) { name, byte, def, max },
	B565D4_SETUP_OPTIONS(OPTION_ENTRY)
#undef OPTION_ENTRY
};

static unsigned int values[B565D4_OPT_COUNT];
static bool ready;

static bool setup_enabled(void)
{
	return CONFIG(B565D4_BOARD_SETUP) || CONFIG(B565D4_DESKTOP_SETUP);
}

bool b565d4_setup_ready(void)
{
	return ready;
}

unsigned int b565d4_setup_get(enum b565d4_setup_option option)
{
	if ((unsigned int)option >= B565D4_OPT_COUNT)
		return 0;
	return ready ? values[option] : options[option].fallback;
}

bool b565d4_setup_core_c6_enabled(void)
{
#if CONFIG(B565D4_DESKTOP_SETUP)
	return ready && b565d4_setup_get(B565D4_OPT_CORE_C6);
#else
	return true;
#endif
}

static void write_defaults(unsigned int bifurcation)
{
	/* No date/control/century/AMD-register writes and no RTC stop needed. */
	for (unsigned int byte = B565D4_SETUP_BEGIN; byte <= B565D4_SETUP_END; byte++)
		cmos_write(0, byte);
	for (unsigned int i = 0; i < ARRAY_SIZE(options); i++)
		cmos_write(options[i].fallback, options[i].byte);
	cmos_write(bifurcation, options[B565D4_OPT_BIFURCATION].byte);
	cmos_set_checksum(B565D4_SETUP_BEGIN, B565D4_SETUP_END, B565D4_SETUP_CHECKSUM);
}

void b565d4_setup_init(void)
{
	if (!setup_enabled() || ready)
		return;

	/* The verified CBFS medium is available here. The DRAM-first bootblock
	 * cannot load cmos.default, so apply the standard static-table policy
	 * before capturing any of the board's runtime settings.
	 */
	if (CONFIG(STATIC_OPTION_TABLE))
		sanitize_cmos();

	bool checksum_ok = !cmos_error() &&
		cmos_checksum_valid(B565D4_SETUP_BEGIN, B565D4_SETUP_END,
				    B565D4_SETUP_CHECKSUM);
	/* Schema 1 reserved both wake bytes as zero. Populate the newly backed
	 * options without replacing the operator's other valid settings.
	 */
	if (checksum_ok && cmos_read(97) == 1 && !cmos_read(121)) {
		cmos_write(options[B565D4_OPT_WOL].fallback, options[B565D4_OPT_WOL].byte);
		cmos_write(options[B565D4_OPT_RTC_WAKE].fallback,
			   options[B565D4_OPT_RTC_WAKE].byte);
		cmos_write(2, 97);
		cmos_set_checksum(B565D4_SETUP_BEGIN, B565D4_SETUP_END,
				  B565D4_SETUP_CHECKSUM);
	}
	/* Schema 2 kept the payload policy fields reserved. Preserve every
	 * existing board choice and initialize only the newly backed fields.
	 * EDK2 adopts an existing standard EFI Timeout on first use.
	 */
	if (checksum_ok && cmos_read(97) == 2 && !cmos_read(121)) {
		static const enum b565d4_setup_option new_options[] = {
			B565D4_OPT_SERIAL, B565D4_OPT_PXE, B565D4_OPT_SAS_ROM,
			B565D4_OPT_BOOT_POLICY, B565D4_OPT_BOOT_TIMEOUT,
		};

		for (unsigned int i = 0; i < ARRAY_SIZE(new_options); i++) {
			unsigned int option = new_options[i];

			cmos_write(options[option].fallback, options[option].byte);
		}
		cmos_write(3, 97);
		cmos_set_checksum(B565D4_SETUP_BEGIN, B565D4_SETUP_END,
				  B565D4_SETUP_CHECKSUM);
	}
	if (checksum_ok && cmos_read(97) == 3 && !cmos_read(121)) {
		cmos_write(options[B565D4_OPT_ARI].fallback, options[B565D4_OPT_ARI].byte);
		cmos_write(4, 97);
		cmos_set_checksum(B565D4_SETUP_BEGIN, B565D4_SETUP_END,
				  B565D4_SETUP_CHECKSUM);
	}
	if (checksum_ok && cmos_read(97) == 4 && !cmos_read(121)) {
		cmos_write(options[B565D4_OPT_PSTATE_LIMIT].fallback,
			   options[B565D4_OPT_PSTATE_LIMIT].byte);
		cmos_write(5, 97);
		cmos_set_checksum(B565D4_SETUP_BEGIN, B565D4_SETUP_END,
				  B565D4_SETUP_CHECKSUM);
	}
#if CONFIG(B565D4_DESKTOP_SETUP)
	/* Schema 5 reserved byte 120. Initialize only the new idle policy,
	 * preserving every existing desktop setting and AMD's upper bytes.
	 */
	if (checksum_ok && cmos_read(97) == 5 && !cmos_read(121)) {
		cmos_write(options[B565D4_OPT_CORE_C6].fallback,
			   options[B565D4_OPT_CORE_C6].byte);
		cmos_write(B565D4_SETUP_SCHEMA, 97);
		cmos_set_checksum(B565D4_SETUP_BEGIN, B565D4_SETUP_END,
				  B565D4_SETUP_CHECKSUM);
	}
#endif
	bool valid = checksum_ok && cmos_read(97) == B565D4_SETUP_SCHEMA;

	if (!valid || cmos_read(options[B565D4_OPT_DEFAULTS].byte) == 1) {
		unsigned int bifurcation = 0;

		/* Migrate only the original, independently checksummed lane option.
		 * A requested defaults reset deliberately restores the build default.
		 */
		if (!valid && !cmos_error() && cmos_checksum_valid(96, 96, 97) &&
		    cmos_read(96) <= options[B565D4_OPT_BIFURCATION].maximum)
			bifurcation = cmos_read(96);
		write_defaults(bifurcation);
		printk(BIOS_NOTICE, "Board setup: initialized schema=%u lanes=%u\n",
		       B565D4_SETUP_SCHEMA, bifurcation);
	}
	for (unsigned int i = 0; i < ARRAY_SIZE(options); i++) {
		unsigned int value = get_uint_option(options[i].name, options[i].fallback);

		if (value > options[i].maximum ||
		    (CONFIG(B565D4_DESKTOP_NATIVE) && i == B565D4_OPT_BIFURCATION &&
		     value == 2)) {
			printk(BIOS_WARNING, "Board setup: invalid %s=%u, using %u\n",
			       options[i].name, value, options[i].fallback);
			value = options[i].fallback;
			set_uint_option(options[i].name, value);
		}
		values[i] = value;
		printk(BIOS_NOTICE, "Board setup: %s=%u\n", options[i].name, value);
	}
	ready = true;
}

void b565d4_setup_power_failure(void)
{
	if (!setup_enabled())
		return;
	/* Same PM_RTC_SHADOW_REG encoding as AMD's common pmlib.c. The lower
	 * nibble is the write command, reflected by hardware in the upper nibble.
	 */
	static const uint8_t policy[] = { 0, 1, 3 };
	unsigned int option = b565d4_setup_get(B565D4_OPT_POWER_FAILURE);

	pm_write8(0x5b, BIT(2) | policy[option]);
	printk(BIOS_NOTICE, "Board setup: AC recovery=%u PM5b=%02x\n",
	       option, pm_read8(0x5b));
}

void b565d4_setup_wake(void)
{
	if (!setup_enabled())
		return;
	uint32_t before = pm_read32(PM_ACPI_CONF);
	uint32_t wanted = before & ~(PM_ACPI_RTC_WAKE_EN | PM_ACPI_BLOCK_PCIE_PME);

	/* Keep S0 RTC alarms on the fixed ACPI event path. _PTS applies the
	 * configured wake permission only for S5; _WAK/_TTS restore S0.
	 */
	if (!b565d4_setup_get(B565D4_OPT_WOL))
		wanted |= PM_ACPI_BLOCK_PCIE_PME;
	/* Retain the board's existing PME/GEVENT routing. This permits wake;
	 * the OS must still arm a NIC's PME or the RTC fixed event and alarm.
	 * No stale status acknowledgement or speculative GPE routing change.
	 */
	if (wanted != before)
		pm_write32(PM_ACPI_CONF, wanted);
	uint32_t after = pm_read32(PM_ACPI_CONF);

	printk(after == wanted ? BIOS_NOTICE : BIOS_ERR,
	       "Board setup: S5 RTC permission=%u PCIe=%u S0 PM74=%08x -> %08x wanted=%08x\n",
	       b565d4_setup_get(B565D4_OPT_RTC_WAKE), b565d4_setup_get(B565D4_OPT_WOL),
	       before, after, wanted);
}

void b565d4_setup_cpu(void)
{
	if (!setup_enabled())
		return;
	/* AMD64 APM vol.2, VM_CR: C001_0114, LOCK bit3, SVMDIS bit4.
	 * SVMDIS alone is cleared by INIT on APs. Lock a disabled policy until
	 * processor reset so the OS's INIT/SIPI startup preserves it. A cold
	 * boot can select another policy. Leave an enabled policy unlocked.
	 * CPUID still advertises the architectural capability with SVMDIS set.
	 */
	msr_t value = rdmsr(0xc0010114);
	bool disable = !b565d4_setup_get(B565D4_OPT_SVM);
	uint32_t wanted = disable ? value.lo | BIT(3) | BIT(4) : value.lo & ~BIT(4);

	if (value.lo != wanted) {
		if ((value.lo & BIT(3)) || (rdmsr(0xc0000080).lo & BIT(12))) {
			printk(BIOS_ERR, "Board setup: cannot apply SVM policy (locked or active)\n");
			return;
		}
		value.lo = wanted;
		wrmsr(0xc0010114, value);
	}
	value = rdmsr(0xc0010114);
	printk(BIOS_NOTICE, "Board setup: CPU SVM requested=%u VM_CR=%08x:%08x\n",
	       !disable, value.hi, value.lo);
}
