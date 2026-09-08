/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Native, deliberately narrow ACPI description for B06WC.
 *
 * This is a table-validation contract, not an assertion that ACPI runtime
 * services are complete.  It publishes only after the platform path has
 * entered a neutral ACPI mode with SCI_EN set and every event source and both
 * possible IRQ9 destinations still masked.  This file itself never
 * acknowledges PM status or enables an interrupt, GPE, SMI, sleep, wake,
 * reset, USB-legacy, or application-processor path.
 */

#include <acpi/acpi.h>
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

#include "b06wc_acpi.h"
#include "b06vn_pci.h"
#if CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM
#include "b06wj_acpi.h"
#endif
#if CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI
#include "b06wi_irq_route.h"
#endif
#if CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT
#include "b06wd_input.h"
#endif

_Static_assert(CONFIG(NO_ECAM_MMCONF_SUPPORT),
	"B06WC must retain CF8/CFC for coreboot PCI configuration access");
_Static_assert(CONFIG(ACPI_CUSTOM_MCFG),
	"B06WC needs the table-only custom MCFG path");
_Static_assert(CONFIG(ACPI_CUSTOM_MADT),
	"B06WC publishes only its measured single-BSP MADT topology");
_Static_assert(CONFIG(NO_SMM),
	"B06WC ACPI deliberately has no SMM ACPI-enable service");
_Static_assert(!CONFIG(SMP) && CONFIG_MAX_CPUS == 1,
	"B06WC MADT is valid only for the current BSP-only build");
_Static_assert(CONFIG(XAPIC_ONLY),
	"B06WC MADT deliberately publishes a legacy Local APIC record");
_Static_assert(CONFIG(ACPI_HAVE_PCAT_8259),
	"B06WC retains the legacy PIC/virtual-wire path");
_Static_assert(!CONFIG(HAVE_CF9_RESET),
	"B06WC has not admitted a native ACPI reset register");
_Static_assert(B06WC_ACPI_ECAM_BASE + B06WC_ACPI_ECAM_SIZE - 1 ==
	B06WC_ACPI_ECAM_LIMIT, "B06WC ECAM range mismatch");

static __noreturn void b06wc_acpi_fail(const char *reason)
{
	die_with_post_code(B06WC_ACPI_POST_FAIL,
		"[ACPI] %s FAIL reason=%s\n", B06WC_ACPI_STAGE_ID, reason);
}

static void b06wc_acpi_require_mode_ready(void)
{
	if (!b06wc_acpi_mode_ready())
		b06wc_acpi_fail("ACPI_MODE_NOT_READY");
}

/* arch_fill_fadt() and the MADT use this same fixed ICH10R SCI topology. */
void ioapic_get_sci_pin(u8 *gsi, u8 *irq, u8 *flags)
{
	*gsi = B06WC_ACPI_SCI_IRQ;
	*irq = B06WC_ACPI_SCI_IRQ;
	*flags = MP_IRQ_TRIGGER_LEVEL | MP_IRQ_POLARITY_HIGH;
}

static void b06wc_acpi_gate_pm(void)
{
	const pci_devfn_t lpc = PCI_DEV(0, 0x1f, 0);
	const uint32_t id = pci_io_read_config32(lpc, 0x00);
	const uint16_t pmbase = pci_io_read_config16(lpc,
		B06WC_ACPI_LPC_PMBASE_REG);
	const uint8_t acpi_cntl = pci_io_read_config8(lpc,
		B06WC_ACPI_LPC_ACPI_CNTL_REG);
	const uint16_t pm1_sts = inw(B06WC_ACPI_PMBASE + B06WC_ACPI_PM1_STS);
	const uint16_t pm1_en = inw(B06WC_ACPI_PMBASE + B06WC_ACPI_PM1_EN);
	const uint32_t pm1_cnt = inl(B06WC_ACPI_PMBASE + B06WC_ACPI_PM1_CNT);
	const uint32_t gpe0_en_lo = inl(B06WC_ACPI_PMBASE + B06WC_ACPI_GPE0_EN_LO);
	const uint32_t gpe0_en_hi = inl(B06WC_ACPI_PMBASE + B06WC_ACPI_GPE0_EN_HI);
	const uint32_t smi_en = inl(B06WC_ACPI_PMBASE + B06WC_ACPI_SMI_EN);
	const uint16_t alt_gp_smi_en =
		inw(B06WC_ACPI_PMBASE + B06WC_ACPI_ALT_GP_SMI_EN);
	const uint16_t uprwc = inw(B06WC_ACPI_PMBASE + B06WC_ACPI_UPRWC);
	const uint32_t gpio_rout = pci_io_read_config32(lpc,
		B06WC_ACPI_LPC_GPIO_ROUT_REG);

	printk(BIOS_NOTICE,
	       "[ACPI] %s PM ID=%08x PMBASE=%04x ACPI_CNTL=%02x "
	       "PM1_STS=%04x PM1_EN=%04x PM1_CNT=%08x "
	       "GPE0_EN=%08x:%08x SMI_EN=%08x ALT_GP_SMI_EN=%04x "
	       "UPRWC=%04x GPIO_ROUT=%08x\n",
	       B06WC_ACPI_STAGE_ID, id, pmbase, acpi_cntl, pm1_sts, pm1_en,
	       pm1_cnt, gpe0_en_hi, gpe0_en_lo, smi_en, alt_gp_smi_en,
	       uprwc, gpio_rout);

	b06wc_acpi_require_mode_ready();
#if CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI
	if (!b06wi_vendor_irq_ready())
		b06wc_acpi_fail("B06WI_IRQ_ROUTE_NOT_READY");
#endif
	if (id != B06WC_ACPI_LPC_ID)
		b06wc_acpi_fail("LPC_ID");
	if (pmbase != B06WC_ACPI_PMBASE_DECODED ||
	    acpi_cntl != B06WC_ACPI_ACPI_CNTL_DECODED)
		b06wc_acpi_fail("PM_DECODE");
	if (pm1_en != 0 || gpe0_en_lo != 0 || gpe0_en_hi != 0 ||
	    smi_en != 0 || alt_gp_smi_en != 0 || uprwc != 0 || gpio_rout != 0)
		b06wc_acpi_fail("PM_SOURCE_ENABLED");
	if (pm1_cnt != B06WC_ACPI_PM1_CNT_TARGET)
		b06wc_acpi_fail("PM1_CNT_NOT_EXACT_ACPI_MODE");
}

void acpi_fill_fadt(acpi_fadt_t *fadt)
{
	b06wc_acpi_gate_pm();

	/* Runtime PM registers proven at the decoded 0x500 block. */
	fadt->sci_int = B06WC_ACPI_SCI_IRQ;
	fadt->pm1a_evt_blk = B06WC_ACPI_PMBASE + B06WC_ACPI_PM1_STS;
	fadt->pm1a_cnt_blk = B06WC_ACPI_PMBASE + B06WC_ACPI_PM1_CNT;
	fadt->pm_tmr_blk = B06WC_ACPI_PMBASE + B06WC_ACPI_PM_TMR;
	fadt->pm1_evt_len = 4;
	fadt->pm1_cnt_len = 2;
	fadt->pm_tmr_len = 4;

	/* B06WI publishes the vendor-observed block, but leaves every GPE masked. */
	fadt->pm2_cnt_blk = 0;
#if CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI
	fadt->gpe0_blk = B06WC_ACPI_PMBASE + B06WC_ACPI_GPE0_STS;
	fadt->gpe0_blk_len = B06WC_ACPI_GPE0_BLK_LEN;
#else
	fadt->gpe0_blk = 0;
	fadt->gpe0_blk_len = 0;
#endif
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
#if CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT && \
	!CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI
	b06wd_enable_fadt_8042(fadt);
#endif
	/*
	 * Earlier experiments keep fixed button/RTC wake events unadvertised;
	 * B06WK admits only the fixed power button below. B06WJ's PNP0B00
	 * describes the legacy IRQ8 RTC, not ACPI fixed-event RTC wake support.
	 * In the FADT these bits mean "not a fixed-hardware feature"; leaving
	 * them clear would advertise unvalidated fixed PM1 event sources.
	 */
	fadt->flags = ACPI_FADT_WBINVD | ACPI_FADT_C1_SUPPORTED |
		ACPI_FADT_POWER_BUTTON | ACPI_FADT_SLEEP_BUTTON |
		ACPI_FADT_FIXED_RTC;
#if CONFIG_X58_PRO_E_B06WK_ACPI_REPAIR
	/*
	 * MSI live FADT and PWRF enumeration (2026-09-08) describe the ICH10
	 * fixed power button. POWER_BUTTON means a control-method device,
	 * but this namespace has no PNP0C0C. Describe the fixed event instead;
	 * OSPM owns clearing its status, installing a handler and enabling it.
	 * Keep the inherited masked PM1/GPE/SMI hardware state unchanged.
	 */
	fadt->flags &= ~ACPI_FADT_POWER_BUTTON;
	_Static_assert(ADDR_SPACE_GENERAL_FLAG_CONSUMER == 0x01,
		"B06WK requires the corrected CTBL consumer resource encoding");
	printk(BIOS_NOTICE,
	       "[ACPI] B06WK-REPAIR1 FADT_FLAGS=%08x FIXED_PWRBTN=1 "
	       "PM_EVENTS_LEFT_MASKED=1 CTBL_CONSUMER_FLAG=%02x\n",
	       fadt->flags, ADDR_SPACE_GENERAL_FLAG_CONSUMER);
#endif
	memset(&fadt->reset_reg, 0, sizeof(fadt->reset_reg));
	fadt->reset_value = 0;

#if CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI
	printk(BIOS_NOTICE,
	       "[ACPI] %s FADT NATIVE_MINIMAL=1 SCI_EN=1 SMI_CMD=00000000 "
	       "PM1_EVT=%04x PM1_CNT=%04x PM_TMR=%04x "
	       "GPE0=%04x/%02x X_GPE0=%08x I8042=0\n",
	       B06WC_ACPI_STAGE_ID, fadt->pm1a_evt_blk, fadt->pm1a_cnt_blk,
	       fadt->pm_tmr_blk, fadt->gpe0_blk, fadt->gpe0_blk_len,
	       fadt->x_gpe0_blk.addrl);
#else
	printk(BIOS_NOTICE,
	       "[ACPI] %s FADT NATIVE_MINIMAL=1 SCI_EN=1 SMI_CMD=00000000 "
	       "PM1_EVT=%04x PM1_CNT=%04x PM_TMR=%04x\n",
	       B06WC_ACPI_STAGE_ID, fadt->pm1a_evt_blk, fadt->pm1a_cnt_blk,
	       fadt->pm_tmr_blk);
#endif
}

static void b06wc_acpi_gate_ioapic(void)
{
	const uint32_t selector_before = read32p(B06WC_ACPI_IOAPIC_BASE);
	uint32_t id, version;

	if (selector_before != B06WC_ACPI_IOAPIC_REG_ID)
		b06wc_acpi_fail("IOAPIC_SELECTOR_PRE");

	id = read32p(B06WC_ACPI_IOAPIC_BASE + B06WC_ACPI_IOAPIC_WINDOW);
	write32p(B06WC_ACPI_IOAPIC_BASE, B06WC_ACPI_IOAPIC_REG_VERSION);
	version = read32p(B06WC_ACPI_IOAPIC_BASE + B06WC_ACPI_IOAPIC_WINDOW);
	write32p(B06WC_ACPI_IOAPIC_BASE, B06WC_ACPI_IOAPIC_REG_ID);

	if (id != B06WC_ACPI_IOAPIC_ID_VALUE ||
	    version != B06WC_ACPI_IOAPIC_VERSION_VALUE ||
	    read32p(B06WC_ACPI_IOAPIC_BASE) != B06WC_ACPI_IOAPIC_REG_ID)
		b06wc_acpi_fail("IOAPIC_IDENTITY");
}

unsigned long acpi_fill_madt(unsigned long current)
{
	acpi_madt_ioapic_t *ioapic;

	b06wc_acpi_gate_pm();
	if (cpu_get_lapic_addr() != B06WC_ACPI_LAPIC_BASE || lapicid() != 0)
		b06wc_acpi_fail("BSP_LAPIC");
	b06wc_acpi_gate_ioapic();

	current = acpi_create_madt_one_lapic(current, 0, 0);

	/* Avoid the generic helper's static GSI cursor and selector side effect. */
	ioapic = (acpi_madt_ioapic_t *)(uintptr_t)current;
	memset(ioapic, 0, sizeof(*ioapic));
	ioapic->type = IO_APIC;
	ioapic->length = sizeof(*ioapic);
	ioapic->ioapic_id = B06WC_ACPI_IOAPIC_ID;
	ioapic->ioapic_addr = B06WC_ACPI_IOAPIC_BASE;
	ioapic->gsi_base = B06WC_ACPI_IOAPIC_GSI_BASE;
	current += sizeof(*ioapic);

#if CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI
	current += acpi_create_madt_irqoverride(
		(acpi_madt_irqoverride_t *)(uintptr_t)current, MP_BUS_ISA,
		0, 2, MP_IRQ_TRIGGER_EDGE | MP_IRQ_POLARITY_HIGH);
#endif

	current += acpi_create_madt_irqoverride(
		(acpi_madt_irqoverride_t *)(uintptr_t)current, MP_BUS_ISA,
		B06WC_ACPI_SCI_IRQ, B06WC_ACPI_SCI_IRQ,
		MP_IRQ_TRIGGER_LEVEL | MP_IRQ_POLARITY_HIGH);

	printk(BIOS_NOTICE,
	       "[ACPI] %s MADT BSP=APIC0 IOAPIC=%u@%08x/GSI0 "
	       "SCI=IRQ9->GSI9/level/high IRQ0_OVERRIDE=%u\n",
	       B06WC_ACPI_STAGE_ID, B06WC_ACPI_IOAPIC_ID,
	       B06WC_ACPI_IOAPIC_BASE,
	       CONFIG(X58_PRO_E_B06WI_VENDOR_IRQ_ACPI) ? 1 : 0);
	return current;
}

static void b06wc_acpi_gate_pciexbar(void)
{
#if CONFIG_X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX
	const pci_devfn_t sad = PCI_DEV(0xff, 0, 1);
	const uint32_t sad_id = pci_io_read_config32(sad, 0x00);
#else
	const pci_devfn_t sad = PCI_DEV(0, 0, 1);
#endif
	const uint32_t low = pci_io_read_config32(sad,
		B06WC_ACPI_SAD_PCIEXBAR_LO);
	const uint32_t high = pci_io_read_config32(sad,
		B06WC_ACPI_SAD_PCIEXBAR_HI);
	const uint32_t host_id = read32p(B06WC_ACPI_ECAM_BASE);

#if CONFIG_X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX
	post_code(B06WE_ACPI_POST_MCFG_GATE_BEGIN);
	printk(BIOS_NOTICE,
	       "[ACPI] B06WE SAD_BDF=ff:00.1 SAD=%08x "
	       "PCIEXBAR=%08x:%08x HOST=%08x COREBOOT_ACCESS=CF8_CFC\n",
	       sad_id, high, low, host_id);
	if (sad_id != B06WC_ACPI_SAD_ID ||
	    low != B06WC_ACPI_PCIEXBAR_VALUE || high != 0 ||
	    host_id != B06WC_ACPI_ECAM_HOST_ID)
		b06wc_acpi_fail("PCIEXBAR");
	post_code(B06WE_ACPI_POST_MCFG_GATE_READY);
#else
	printk(BIOS_NOTICE,
	       "[ACPI] %s MCFG_GATE PCIEXBAR=%08x:%08x HOST=%08x "
	       "COREBOOT_ACCESS=CF8_CFC\n",
	       B06WC_ACPI_STAGE_ID, high, low, host_id);
	if (low != B06WC_ACPI_PCIEXBAR_VALUE || high != 0 ||
	    host_id != B06WC_ACPI_ECAM_HOST_ID)
		b06wc_acpi_fail("PCIEXBAR");
#endif
}

unsigned long acpi_fill_mcfg(unsigned long current)
{
	acpi_mcfg_mmconfig_t *mmconfig;

	b06wc_acpi_gate_pm();
	b06wc_acpi_gate_pciexbar();
	mmconfig = (acpi_mcfg_mmconfig_t *)(uintptr_t)current;
	memset(mmconfig, 0, sizeof(*mmconfig));
	mmconfig->base_address = B06WC_ACPI_ECAM_BASE;
	mmconfig->pci_segment_group_number = B06WC_ACPI_PCI_SEGMENT;
	mmconfig->start_bus_number = B06WC_ACPI_PCI_BUS_START;
	mmconfig->end_bus_number = B06WC_ACPI_PCI_BUS_END;

	printk(BIOS_NOTICE,
	       "[ACPI] %s MCFG BASE=%016llx SEG=%04x BUS=%02x-%02x\n",
	       B06WC_ACPI_STAGE_ID, mmconfig->base_address,
	       mmconfig->pci_segment_group_number, mmconfig->start_bus_number,
	       mmconfig->end_bus_number);
	return current + sizeof(*mmconfig);
}

#if CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM
unsigned long b06wj_write_acpi_tables(const struct device *dev,
	unsigned long current, acpi_rsdp_t *rsdp)
{
	/*
	 * B06WI-HW-01 measured the already-decoded, stopped HPET at fed00000
	 * with GCAP 0429b17f:8086a301. Use the common coreboot writer, which
	 * reads its identity and creates a checksummed table without enabling
	 * the counter or any interrupt. The 0x80 minimum-tick policy is the
	 * existing i82801jx default, not an X58 training parameter.
	 */
	_Static_assert(HPET_BASE_ADDRESS == B06WC_ACPI_HPET_BASE,
		"B06WJ HPET table and AML addresses must agree");
	_Static_assert(CONFIG_HPET_MIN_TICKS == 0x80,
		"B06WJ uses the existing ICH10 HPET minimum-tick policy");
	current = acpi_write_hpet(dev, current, rsdp);
	printk(BIOS_NOTICE,
	       "[ACPI] B06WJ-PLATFORM1 HPET BASE=%08x ID=%08x MIN_TICK=%04x "
	       "CPU_NAMESPACE=CP00/UID0 LPC=PIC/PIT/RTC LOW_IO=1 "
	       "HW_WRITE=0 SMP=0 SMM=0\n",
	       B06WC_ACPI_HPET_BASE, read32p(B06WC_ACPI_HPET_BASE),
	       CONFIG_HPET_MIN_TICKS);
	return current;
}
#endif
