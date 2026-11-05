/* SPDX-License-Identifier: GPL-2.0-only */

#include "platform.h"
#include "native84_profile.h"
#include "native_apob_resources.h"
#include "b565d4_cpu.h"
#include <amdblocks/smn.h>
#include <console/console.h>
#include <cpu/amd/msr.h>
#include <cpu/x86/msr.h>
#include <device/device.h>
#include <device/pci.h>
#include <halt.h>
#include <static.h>

#define IOHC 0x13e10000U
#define PSP_BASE 0xfd200000U
#define NBIF_BASE 0xfd000000U
#define GNB_APIC_BASE 0xfec01000U
#define CCP_STRAP 0x10834200U
#define FUNCTION_ENABLE (1U << 28)

static void store(uint32_t address, uint32_t value)
{
	smn_write32(address, value);
	if (smn_read32(address) != value)
		die(B565D4_DESKTOP_NAME " IOHC readback mismatch");
}

static void check_mmio_bar(unsigned int offset, uint32_t base, uint32_t flags)
{
	uint32_t lo = smn_read32(IOHC + offset), hi = smn_read32(IOHC + offset + 4);

	if (hi || lo == UINT32_MAX || (base & 0xfffff) ||
	    (lo & ~(0xfff00000U | flags)))
		die(B565D4_DESKTOP_NAME " IOHC BAR outside IA32 policy");
	if ((lo & 0xfff00000U) && (lo & 0xfff00000U) != base)
		die(B565D4_DESKTOP_NAME " IOHC BAR allocation conflict");
	if (native_apob_require_pci_hole(base, (uint64_t)base + 0x100000) != NATIVE_APOB_OK)
		die(B565D4_DESKTOP_NAME " IOHC BAR overlaps supplier memory");
}

static void mmio_bar(unsigned int offset, uint32_t base, uint32_t flags)
{
	/* A valid supplier mapping does not need a temporary decode disable. */
	if (smn_read32(IOHC + offset) == (base | flags))
		return;
	store(IOHC + offset + 4, 0);
	store(IOHC + offset, base);
	if (flags == 3)
		store(IOHC + offset, base | 1);
	store(IOHC + offset, base | flags);
}

void b565d4_desktop_iohc_prepare(void)
{
	msr_t psp = rdmsr(PSP_ADDR_MSR);
	uint32_t psp_base = smn_read32(IOHC + 0x2e0) & 0xfff00000U;
	uint32_t nbif_base = smn_read32(IOHC + 0x2e8) & 0xfff00000U;
	uint32_t apic_lo = smn_read32(IOHC + 0x2f0);
	uint32_t apic_hi = smn_read32(IOHC + 0x2f4);
	uint32_t features = smn_read32(0x14600000);
	const uint64_t apic_cover = GNB_APIC_BASE & ~0xffffULL;

	printk(BIOS_NOTICE, B565D4_DESKTOP_NAME " IOHC PRE: PSP_MSR=%016llx "
	       "PSP=%08x NBIF=%08x IOAPIC=%08x:%08x features=%08x\n",
	       psp.raw, psp_base, nbif_base, apic_hi, apic_lo, features);
	if (!psp_base)
		psp_base = psp.raw ? psp.raw : PSP_BASE;
	if (!nbif_base)
		nbif_base = psp_base == NBIF_BASE ? NBIF_BASE + 0x100000 : NBIF_BASE;
	if (psp_base == nbif_base || (psp.raw && psp.raw != psp_base))
		die(B565D4_DESKTOP_NAME " PSP/NBIF allocation conflict");
	/* Check every supplier aperture before the first data or MSR store. */
	check_mmio_bar(0x2e0, psp_base, 0x101);
	check_mmio_bar(0x2e8, nbif_base, 3);
	/* A disabled reset descriptor at fec00000 does not own an aperture.
	 * BaseSspPei installs the allocated base before enabling its decode.
	 * Keep rejecting an enabled mapping at a conflicting address.
	 */
	if (apic_hi ||
	    (apic_lo && apic_lo != 0xfec00000U && apic_lo != GNB_APIC_BASE &&
	     apic_lo != (GNB_APIC_BASE | 1)) ||
	    features == UINT32_MAX ||
	    native_apob_require_pci_hole(apic_cover, apic_cover + 0x10000) !=
		NATIVE_APOB_OK)
		die(B565D4_DESKTOP_NAME " IOAPIC supplier mapping unavailable");

	/* ASRock AmdSocAm4MtsPei d6be..d733 selects 13e1 for both desktop
	 * families, programs an allocated 1MiB base|101 and initializes PSPADDR
	 * only when zero. These are native fixed allocation choices inside the
	 * admitted PCI hole, not transplanted reference DRAM reservations.
	 */
	mmio_bar(0x2e0, psp_base, 0x101);
	if (!psp.raw) {
		psp.raw = psp_base;
		wrmsr(PSP_ADDR_MSR, psp);
		if (rdmsr(PSP_ADDR_MSR).raw != psp_base)
			die(B565D4_DESKTOP_NAME " PSPADDR readback mismatch");
	}
	/* BaseSspPei e0fa..e11b and d05..f49: a separate 1MiB internal
	 * aperture, high DWORD zero, enable1 then enable3. PCI IOMMU MMIO is
	 * allocated separately by the ordinary amd_iommu_ops driver.
	 */
	mmio_bar(0x2e8, nbif_base, 3);
	/* BaseSspPei d674..d7d5: high/disabled-low/enabled-low, feature OR14,
	 * bus0 plus virtual-bus enable. IOAPIC IDs are assigned from coreboot's
	 * native policy and emitted from hardware; reference IDs13/14 are unused.
	 */
	if (apic_lo != (GNB_APIC_BASE | 1)) {
		store(IOHC + 0x2f4, 0);
		store(IOHC + 0x2f0, GNB_APIC_BASE);
		store(IOHC + 0x2f0, GNB_APIC_BASE | 1);
	}
	store(0x14600000, features | 0x14);
	store(IOHC + 0x44, 0x100);
	printk(BIOS_NOTICE, B565D4_DESKTOP_NAME " IOHC: PSP=%08x NBIF=%08x IOAPIC=%08x\n",
	       psp_base, nbif_base, GNB_APIC_BASE);
}

void b565d4_desktop_ccp_hide(void *unused)
{
	struct device *dev = DEV_PTR(crypto);
	struct device *parent = DEV_PTR(gpp_bridge_a);
	uint32_t before, after;

	native84_stage_check();
	if (!b565d4_is_selected_cpu(cpuid_eax(1)) || !dev || !parent ||
	    dev->upstream != parent->downstream ||
	    pci_read_config32(parent, PCI_VENDOR_ID) != 0x14841022 ||
	    pci_read_config32(dev, PCI_VENDOR_ID) != 0x14861022)
		die(B565D4_DESKTOP_NAME " CCP quarantine identity mismatch");
	before = smn_read32(CCP_STRAP);
	/* Exact desktop instance/function from the ASRock AM4 strap table.
	 * The working 5600X reference independently reads10001486 here, while
	 * Cezanne's10134400 returnsffffffff. Never reuse its strap address.
	 */
	if ((before & 0xffff) != 0x1486 || !(before & FUNCTION_ENABLE))
		die(B565D4_DESKTOP_NAME " CCP strap identity mismatch");
	store(CCP_STRAP, before & ~FUNCTION_ENABLE);
	after = pci_read_config32(dev, PCI_VENDOR_ID);
	if (after != UINT32_MAX || pci_read_config32(parent, PCI_VENDOR_ID) != 0x14841022)
		die(B565D4_DESKTOP_NAME " CCP hiding readback mismatch");
	dev->hidden = true;
	printk(BIOS_NOTICE, B565D4_DESKTOP_NAME " CCP PCI function hidden: %08x -> %08x\n",
	       before, before & ~FUNCTION_ENABLE);
}
