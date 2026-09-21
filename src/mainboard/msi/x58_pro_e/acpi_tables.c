/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Native, deliberately narrow ACPI description for X58_ACPI.
 *
 * This is a table-validation contract, not an assertion that ACPI runtime
 * services are complete.  It publishes only after the platform path has
 * entered a neutral ACPI mode with SCI_EN set and every event source and both
 * possible IRQ9 destinations still masked.  This file itself never
 * acknowledges PM status or enables an interrupt, GPE, SMI, sleep, wake,
 * reset, USB-legacy, or application-processor path.
 */

#include <acpi/acpi.h>
#include "ich10_gpe_policy.h"
#include <acpi/acpigen.h>
#include <arch/io.h>
#include <arch/hpet.h>
#include <arch/ioapic.h>
#include <arch/pci_io_cfg.h>
#include <arch/smp/mpspec.h>
#include <console/console.h>
#include <cpu/cpu.h>
#include <cpu/x86/lapic.h>
#include <device/mmio.h>
#include <device/pci_type.h>
#include <string.h>

#include "acpi_registers.h"
#include "pci.h"
#include "full_ich10_acpi.h"
#include "platform_acpi.h"
#include "legacy_io.h"
#include "../../../cpu/intel/model_206cx/cpu_init.h"
/* The native AML and hardware use the established ICH10 decode bases. */
#undef X58_ACPI_PMBASE
#undef X58_ACPI_PMBASE_DECODED
#define X58_ACPI_PMBASE x58_platform_pmbase()
#define X58_ACPI_PMBASE_DECODED (x58_platform_pmbase() | 1u)
#include "acpi_tables.h"
#include "irqroute.h"
#include "legacy_input.h"

_Static_assert(CONFIG(NO_ECAM_MMCONF_SUPPORT) ||
	(CONFIG(ECAM_MMCONF_RAMSTAGE_ONLY) &&
	 X58_ACPI_ECAM_BASE == CONFIG_ECAM_MMCONF_BASE_ADDRESS &&
	 X58_ACPI_ECAM_SIZE == CONFIG_ECAM_MMCONF_LENGTH),
	"X58_ACPI needs CF8 or the explicit late ECAM path matching its MCFG");
_Static_assert(CONFIG(ACPI_CUSTOM_MCFG),
	"X58_ACPI needs the table-only custom MCFG path");
_Static_assert(CONFIG(ACPI_CUSTOM_MADT),
	"X58_ACPI publishes only its measured single-BSP MADT topology");
_Static_assert(CONFIG(ACPI_CUSTOM_MADT_REPLACES_ARCH_ENTRIES),
	"ACPI MADT dedup needs board-owned entries to preserve vendor Processor IDs");
_Static_assert(CONFIG(NO_SMM),
	"X58_ACPI ACPI deliberately has no SMM ACPI-enable service");
_Static_assert(CONFIG(XAPIC_ONLY),
	"X58_ACPI MADT deliberately publishes a legacy Local APIC record");
_Static_assert(CONFIG(ACPI_HAVE_PCAT_8259),
	"X58_ACPI retains the legacy PIC/virtual-wire path");
_Static_assert(!CONFIG(HAVE_CF9_RESET),
	"X58_ACPI has not admitted a native ACPI reset register");
_Static_assert(X58_ACPI_ECAM_BASE + X58_ACPI_ECAM_SIZE - 1 ==
	X58_ACPI_ECAM_LIMIT, "X58_ACPI ECAM range mismatch");
_Static_assert(X58_ACPI_IOAPIC_ID == 6,
	"SCI IOAPIC6 requires the explicit vendor-IRQ path");

static __noreturn void x58_acpi_fail(const char *reason)
{
	die_with_post_code(X58_ACPI_POST_FAIL,
		"[ACPI] %s FAIL reason=%s\n", X58_ACPI_STAGE_ID, reason);
}

static void x58_acpi_require_mode_ready(void)
{
	x58_full_ich10_require_native_acpi();
	if (!x58_acpi_mode_ready())
		x58_acpi_fail("ACPI_MODE_NOT_READY");
}

/* arch_fill_fadt() and the MADT use this same fixed ICH10R SCI topology. */

static void x58_acpi_gate_pm(void)
{
	const pci_devfn_t lpc = PCI_DEV(0, 0x1f, 0);
	const uint32_t id = pci_io_read_config32(lpc, 0x00);
	const uint16_t pmbase = pci_io_read_config16(lpc,
		X58_ACPI_LPC_PMBASE_REG);
	const uint8_t acpi_cntl = pci_io_read_config8(lpc,
		X58_ACPI_LPC_ACPI_CNTL_REG);
	const uint16_t pm1_sts = inw(X58_ACPI_PMBASE + X58_ACPI_PM1_STS);
	const uint16_t pm1_en = inw(X58_ACPI_PMBASE + X58_ACPI_PM1_EN);
	const uint32_t pm1_cnt = inl(X58_ACPI_PMBASE + X58_ACPI_PM1_CNT);
	const uint32_t gpe0_en_lo = inl(X58_ACPI_PMBASE + X58_ACPI_GPE0_EN_LO);
	const uint32_t gpe0_en_hi = inl(X58_ACPI_PMBASE + X58_ACPI_GPE0_EN_HI);
	const uint32_t smi_en = inl(X58_ACPI_PMBASE + X58_ACPI_SMI_EN);
	const uint16_t alt_gp_smi_en =
		inw(X58_ACPI_PMBASE + X58_ACPI_ALT_GP_SMI_EN);
	const uint16_t uprwc = inw(X58_ACPI_PMBASE + X58_ACPI_UPRWC);
	const uint32_t gpio_rout = pci_io_read_config32(lpc,
		X58_ACPI_LPC_GPIO_ROUT_REG);

	printk(BIOS_DEBUG,
	       "[ACPI] %s PM ID=%08x PMBASE=%04x ACPI_CNTL=%02x "
	       "PM1_STS=%04x PM1_EN=%04x PM1_CNT=%08x "
	       "GPE0_EN=%08x:%08x SMI_EN=%08x ALT_GP_SMI_EN=%04x "
	       "UPRWC=%04x GPIO_ROUT=%08x\n",
	       X58_ACPI_STAGE_ID, id, pmbase, acpi_cntl, pm1_sts, pm1_en,
	       pm1_cnt, gpe0_en_hi, gpe0_en_lo, smi_en, alt_gp_smi_en,
	       uprwc, gpio_rout);

	x58_acpi_require_mode_ready();
	if (!x58_irq_route_vendor_irq_ready())
		x58_acpi_fail("X58_IRQ_ROUTE_IRQ_ROUTE_NOT_READY");
	if (id != X58_ACPI_LPC_ID)
		x58_acpi_fail("LPC_ID");
	if (pmbase != X58_ACPI_PMBASE_DECODED ||
	    acpi_cntl != X58_ACPI_CNTL_DECODED)
		x58_acpi_fail("PM_DECODE");
	if (pm1_en != 0 || !x58_ich10_gpe0_low_quiet(gpe0_en_lo) || gpe0_en_hi != 0 ||
	    smi_en != 0 || alt_gp_smi_en != 0 || uprwc != 0 || gpio_rout != 0)
		x58_acpi_fail("PM_SOURCE_ENABLED");
	if (pm1_cnt != X58_ACPI_PM1_CNT_TARGET)
		x58_acpi_fail("PM1_CNT_NOT_EXACT_ACPI_MODE");
}

void acpi_fill_fadt(acpi_fadt_t *fadt)
{
	x58_acpi_gate_pm();

	/* Runtime PM registers proven at the decoded 0x500 block. */
	fadt->sci_int = X58_ACPI_SCI_IRQ;
	fadt->pm1a_evt_blk = X58_ACPI_PMBASE + X58_ACPI_PM1_STS;
	fadt->pm1a_cnt_blk = X58_ACPI_PMBASE + X58_ACPI_PM1_CNT;
	fadt->pm_tmr_blk = X58_ACPI_PMBASE + X58_ACPI_PM_TMR;
	fadt->pm1_evt_len = 4;
	fadt->pm1_cnt_len = 2;
	fadt->pm_tmr_len = 4;

	/* X58_IRQ_ROUTE publishes the vendor-observed block, but leaves every GPE masked. */
	fadt->pm2_cnt_blk = 0;
	fadt->gpe0_blk = X58_ACPI_PMBASE + X58_ACPI_GPE0_STS;
	fadt->gpe0_blk_len = X58_ACPI_GPE0_BLK_LEN;
	fadt->gpe1_blk = 0;
	fadt->pm2_cnt_len = 0;
	fadt->gpe1_blk_len = 0;

	fill_fadt_extended_pm_io(fadt);

	/* NO_SMM means the OS must never issue a vendor B2 ACPI command. */
	fadt->smi_cmd = 0;
	fadt->acpi_enable = 0;
	fadt->acpi_disable = 0;
	fadt->s4bios_req = 0;
	fadt->pstate_cnt = 0;

	/* PC80 arch defaults are not evidence that RTC wake is implemented. */
	fadt->day_alrm = 0;
	fadt->mon_alrm = 0;
	fadt->century = 0;

	fadt->iapc_boot_arch = ACPI_FADT_LEGACY_DEVICES;
	/* The native namespace reserves 60/64 without claiming a working i8042. */
	/*
	 * Earlier paths keep fixed button/RTC wake events unadvertised;
	 * X58_ACPI_REPAIR admits only the fixed power button below. X58_ACPI_PLATFORM's PNP0B00
	 * describes the legacy IRQ8 RTC, not ACPI fixed-event RTC wake support.
	 * In the FADT these bits mean "not a fixed-hardware feature"; leaving
	 * them clear would advertise unvalidated fixed PM1 event sources.
	 */
	fadt->flags = ACPI_FADT_WBINVD | ACPI_FADT_C1_SUPPORTED |
		ACPI_FADT_POWER_BUTTON | ACPI_FADT_SLEEP_BUTTON |
		ACPI_FADT_FIXED_RTC;
	/*
	 * MSI live FADT and PWRF enumeration (2026-09-08) describe the ICH10
	 * fixed power button. POWER_BUTTON means a control-method device,
	 * but this namespace has no PNP0C0C. Describe the fixed event instead;
	 * OSPM owns clearing its status, installing a handler and enabling it.
	 * Keep the inherited masked PM1/GPE/SMI hardware state unchanged.
	 */
	fadt->flags &= ~ACPI_FADT_POWER_BUTTON;
	_Static_assert(ADDR_SPACE_GENERAL_FLAG_CONSUMER == 0x01,
		"X58_ACPI_REPAIR requires the corrected CTBL consumer resource encoding");
	printk(BIOS_DEBUG,
	       "[ACPI] X58_ACPI_REPAIR-REPAIR1 FADT_FLAGS=%08x FIXED_PWRBTN=1 "
	       "PM_EVENTS_LEFT_MASKED=1 CTBL_CONSUMER_FLAG=%02x\n",
	       fadt->flags, ADDR_SPACE_GENERAL_FLAG_CONSUMER);
	memset(&fadt->reset_reg, 0, sizeof(fadt->reset_reg));
	fadt->reset_value = 0;

	printk(BIOS_DEBUG,
	       "[ACPI] %s FADT NATIVE_MINIMAL=1 SCI_EN=1 SMI_CMD=00000000 "
	       "PM1_EVT=%04x PM1_CNT=%04x PM_TMR=%04x "
	       "GPE0=%04x/%02x X_GPE0=%08x I8042=%u\n",
	       X58_ACPI_STAGE_ID, fadt->pm1a_evt_blk, fadt->pm1a_cnt_blk,
	       fadt->pm_tmr_blk, fadt->gpe0_blk, fadt->gpe0_blk_len,
	       fadt->x_gpe0_blk.addrl, !!(fadt->iapc_boot_arch & ACPI_FADT_8042));
}

static void x58_acpi_gate_ioapic(void)
{
	const uint32_t selector_before = read32p(X58_ACPI_IOAPIC_BASE);
	uint32_t id, version;

	if (selector_before != X58_ACPI_IOAPIC_REG_ID)
		x58_acpi_fail("IOAPIC_SELECTOR_PRE");

	id = read32p(X58_ACPI_IOAPIC_BASE + X58_ACPI_IOAPIC_WINDOW);
	write32p(X58_ACPI_IOAPIC_BASE, X58_ACPI_IOAPIC_REG_VERSION);
	version = read32p(X58_ACPI_IOAPIC_BASE + X58_ACPI_IOAPIC_WINDOW);
	write32p(X58_ACPI_IOAPIC_BASE, X58_ACPI_IOAPIC_REG_ID);

	if (id != X58_ACPI_IOAPIC_ID_VALUE ||
	    version != X58_ACPI_IOAPIC_VERSION_VALUE ||
	    read32p(X58_ACPI_IOAPIC_BASE) != X58_ACPI_IOAPIC_REG_ID)
		x58_acpi_fail("IOAPIC_IDENTITY");
	printk(BIOS_DEBUG, "[ACPI] SCI-ID IOAPIC_READBACK=%08x VERSION=%08x\n",
		id, version);
}

/* X58_IRQ_ROUTE programs the ID before PCI scan/CPU initialization. Only now is the
 * verified enabled-CPU inventory available. Validate before writing any MADT
 * record; never truncate a CPU ID into a legacy Local APIC record or publish
 * a colliding controller. ID6 comes from the board's vendor snapshot, not a
 * count-derived assumption about the sparse Westmere APIC ID space.
 */
static void x58_acpi_gate_cpu_ids(unsigned int count)
{
	for (unsigned int i = 0; i < count; i++) {
		const unsigned int id = model_206cx_apic_id(i);

		if (id >= 0xff || (!i && id != 0))
			x58_acpi_fail("CPU_APIC_ID_RANGE_OR_BSP");
		if (id == X58_ACPI_IOAPIC_ID) {
			printk(BIOS_ERR, "[ACPI] SCI-ID COLLISION CPU=%u APIC=%u\n", i, id);
			x58_acpi_fail("IOAPIC_CPU_ID_COLLISION");
		}
		for (unsigned int j = 0; j < i; j++)
			if (id == model_206cx_apic_id(j))
				x58_acpi_fail("CPU_APIC_ID_DUPLICATE");
	}
	printk(BIOS_DEBUG, "[ACPI] SCI-ID CPU_ID_AUDIT=PASS CPUS=%u IOAPIC_ID=%u "
		"SCI_DELIVERY_PROVEN=0\n", count, X58_ACPI_IOAPIC_ID);
}

unsigned long acpi_fill_madt(unsigned long current)
{
	acpi_madt_ioapic_t *ioapic;

	x58_acpi_gate_pm();
	if (cpu_get_lapic_addr() != X58_ACPI_LAPIC_BASE || lapicid() != 0)
		x58_acpi_fail("BSP_LAPIC");
	x58_acpi_gate_ioapic();

	const unsigned int count = model_206cx_cpu_count();
	if (!count || count > 12)
		x58_acpi_fail("CPU_COUNT");
	x58_acpi_gate_cpu_ids(count);
	for (unsigned int i = 0; i < count; i++)
		current = acpi_create_madt_one_lapic(current,
			i,
			model_206cx_apic_id(i));

	/* Avoid the generic helper's static GSI cursor and selector side effect. */
	ioapic = (acpi_madt_ioapic_t *)(uintptr_t)current;
	memset(ioapic, 0, sizeof(*ioapic));
	ioapic->type = IO_APIC;
	ioapic->length = sizeof(*ioapic);
	ioapic->ioapic_id = X58_ACPI_IOAPIC_ID;
	ioapic->ioapic_addr = X58_ACPI_IOAPIC_BASE;
	ioapic->gsi_base = X58_ACPI_IOAPIC_GSI_BASE;
	current += sizeof(*ioapic);

	current += acpi_create_madt_irqoverride(
		(acpi_madt_irqoverride_t *)(uintptr_t)current, MP_BUS_ISA,
		0, 2, MP_IRQ_TRIGGER_EDGE | MP_IRQ_POLARITY_HIGH);

	current += acpi_create_madt_irqoverride(
		(acpi_madt_irqoverride_t *)(uintptr_t)current, MP_BUS_ISA,
		X58_ACPI_SCI_IRQ, X58_ACPI_SCI_IRQ,
		MP_IRQ_TRIGGER_LEVEL | MP_IRQ_POLARITY_HIGH);

	printk(BIOS_DEBUG,
	       "[ACPI] %s MADT BSP=APIC0 IOAPIC=%u@%08x/GSI0 "
	       "SCI=IRQ9->GSI9/level/high IRQ0_OVERRIDE=%u\n",
	       X58_ACPI_STAGE_ID, X58_ACPI_IOAPIC_ID,
	       X58_ACPI_IOAPIC_BASE,
	       1);
	return current;
}

static void x58_acpi_gate_pciexbar(void)
{
	const pci_devfn_t sad = PCI_DEV(0xff, 0, 1);
	const uint32_t sad_id = pci_io_read_config32(sad, 0x00);
	const uint32_t low = pci_io_read_config32(sad,
		X58_ACPI_SAD_PCIEXBAR_LO);
	const uint32_t high = pci_io_read_config32(sad,
		X58_ACPI_SAD_PCIEXBAR_HI);
	const uint32_t host_id = read32p(X58_ACPI_ECAM_BASE);

	post_code(X58_ECAM_ACPI_POST_MCFG_GATE_BEGIN);
	printk(BIOS_DEBUG,
	       "[ACPI] X58_ECAM_ACPI SAD_BDF=ff:00.1 SAD=%08x "
	       "PCIEXBAR=%08x:%08x HOST=%08x COREBOOT_ACCESS=CF8_CFC\n",
	       sad_id, high, low, host_id);
	if (sad_id != X58_ACPI_SAD_ID ||
	    low != X58_ACPI_PCIEXBAR_VALUE || high != 0 ||
	    host_id != X58_ACPI_ECAM_HOST_ID)
		x58_acpi_fail("PCIEXBAR");
	post_code(X58_ECAM_ACPI_POST_MCFG_GATE_READY);
}

unsigned long acpi_fill_mcfg(unsigned long current)
{
	acpi_mcfg_mmconfig_t *mmconfig;

	x58_acpi_gate_pm();
	x58_acpi_gate_pciexbar();
	mmconfig = (acpi_mcfg_mmconfig_t *)(uintptr_t)current;
	memset(mmconfig, 0, sizeof(*mmconfig));
	mmconfig->base_address = X58_ACPI_ECAM_BASE;
	mmconfig->pci_segment_group_number = X58_ACPI_PCI_SEGMENT;
	mmconfig->start_bus_number = X58_ACPI_PCI_BUS_START;
	mmconfig->end_bus_number = X58_ACPI_PCI_BUS_END;

	printk(BIOS_DEBUG,
	       "[ACPI] %s MCFG BASE=%016llx SEG=%04x BUS=%02x-%02x\n",
	       X58_ACPI_STAGE_ID, mmconfig->base_address,
	       mmconfig->pci_segment_group_number, mmconfig->start_bus_number,
	       mmconfig->end_bus_number);
	return current + sizeof(*mmconfig);
}

unsigned long x58_acpi_platform_write_acpi_tables(const struct device *dev,
	unsigned long current, acpi_rsdp_t *rsdp)
{
	/*
	 * X58_IRQ_ROUTE-HW-01 measured the already-decoded, stopped HPET at fed00000
	 * with GCAP 0429b17f:8086a301. Use the common coreboot writer, which
	 * reads its identity and creates a checksummed table without enabling
	 * the counter or any interrupt. The 0x80 minimum-tick policy is the
	 * existing i82801jx default, not an X58 training parameter.
	 */
	_Static_assert(HPET_BASE_ADDRESS == X58_ACPI_HPET_BASE,
		"X58_ACPI_PLATFORM HPET table and AML addresses must agree");
	_Static_assert(CONFIG_HPET_MIN_TICKS == 0x80,
		"X58_ACPI_PLATFORM uses the existing ICH10 HPET minimum-tick policy");
	current = acpi_write_hpet(dev, current, rsdp);
	current = x58_platform_write_tables(current, rsdp);
	printk(BIOS_NOTICE,
	       "[ACPI] X58_ACPI_PLATFORM-PLATFORM1 HPET BASE=%08x ID=%08x MIN_TICK=%04x "
	       "CPU_NAMESPACE=CP00/UID0 LPC=PIC/PIT/RTC LOW_IO=1 "
	       "HW_WRITE=0 SMP=0 SMM=0\n",
	       X58_ACPI_HPET_BASE, read32p(X58_ACPI_HPET_BASE),
	       CONFIG_HPET_MIN_TICKS);
	return current;
}
