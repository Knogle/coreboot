/* SPDX-License-Identifier: GPL-2.0-only */

#include <bootstate.h>
#include <arch/io.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <device/pci.h>
#include <device/pci_def.h>
#include <device/pci_ops.h>
#include <halt.h>
#include <pc80/i8259.h>
#include <southbridge/intel/common/acpi_pirq_gen.h>
#include <southbridge/intel/i82801jx/i82801jx.h>

#include "full_ich10_acpi.h"
#include "full_pci.h"

#define X58_FULL_ICH10_PIRQ_IRQ 11
#define X58_FULL_ICH10_IRQ_MAX_NODES 512

_Static_assert(CONFIG(NO_SMM),
	"full ICH10 compatibility requires NO_SMM and the disjoint uncore root");

void x58_full_ich10_require_native_acpi(void)
{
	_Static_assert(CONFIG(HAVE_ACPI_TABLES),
		"full ICH10 requires the board's native ACPI tables");
}

/* The standard LPC owner establishes PIRQ A-H = IRQ11. Normal PCI resource
 * programming clears PCI_INTERRUPT_LINE, and an early LPC pass is therefore
 * insufficient. Reconcile only that config byte after all device operations.
 * This reuses i82801jx/lpc.c's existing PCI_INTERRUPT_LINE policy: no PIRQ,
 * ELCR, IOAPIC, pin, command/BME or device-vendor register is written here.
 * The board's native _PRT supplies APIC-mode GSI routes independently.
 */
static void x58_full_ich10_final_irq(void *unused)
{
	const struct device *lpc = pcidev_on_root(0x1f, 0);
	const uint8_t pirq_regs[] = {
		PIRQA_ROUT, PIRQB_ROUT, PIRQC_ROUT, PIRQD_ROUT,
		PIRQE_ROUT, PIRQF_ROUT, PIRQG_ROUT, PIRQH_ROUT,
	};

	(void)unused;
	x58_full_ich10_require_native_acpi();
	if (!lpc || !x58_full_ich10_pci_admitted(lpc) ||
	    pci_read_config32(lpc, PCI_VENDOR_ID) != 0x3a168086)
		die("[ICH10] final IRQ LPC identity/graph invalid\n");
	for (size_t i = 0; i < ARRAY_SIZE(pirq_regs); i++)
		if (pci_read_config8(lpc, pirq_regs[i]) != X58_FULL_ICH10_PIRQ_IRQ)
			die("[ICH10] final IRQ PIRQ policy changed; no line writes\n");
	/* Standard LPC remains the sole PIC/ELCR programmer. OCW1 and
	 * ELCR reads have no acknowledge or clear side effects. Mask state
	 * is telemetry, not a promise about the payload's later policy. */
	const unsigned int mask = inb(MASTER_PIC_OCW1) | (inb(SLAVE_PIC_OCW1) << 8);
	const unsigned int elcr = inb(ELCR1) | (inb(ELCR2) << 8);
	printk(BIOS_DEBUG, "[ICH10] IRQ_OWNER=STANDARD_LPC PIRQ_A_H=0b PIC_MASK=%04x "
	       "ELCR=%04x IRQ11_LEVEL=%u IRQ11_MASKED=%u READ_ONLY=1 DELIVERY_TESTED=0\n",
	       mask, elcr, !!(elcr & (1U << 11)), !!(mask & (1U << 11)));

	unsigned int routed = 0, changed = 0;
	/* Validate the complete admitted graph before the first config write.
	 * The second pass repeats all checks rather than trusting stale records.
	 */
	for (unsigned int apply = 0; apply < 2; apply++) {
		unsigned int nodes = 0;
		for (struct device *dev = all_devices; dev; dev = dev->next) {
			if (++nodes > X58_FULL_ICH10_IRQ_MAX_NODES)
				die("[ICH10] final IRQ device-list bound exceeded\n");
			if (!dev->enabled || dev->path.type != DEVICE_PATH_PCI)
				continue;
			/* CPU uncore is a separate, non-INTx root. Never touch it. */
			if (dev->upstream && !dev->upstream->segment_group &&
			    dev->upstream->secondary == 0xff)
				continue;
			if (!x58_full_ich10_pci_admitted(dev))
				die("[ICH10] final IRQ unadmitted/stale PCI node\n");
			const uint8_t pin = pci_read_config8(dev, PCI_INTERRUPT_PIN);
			if (pin > PCI_INT_D)
				die("[ICH10] final IRQ invalid PCI interrupt pin\n");
			if (pin && dev->upstream->secondary) {
				struct x58_full_intx_route route;
				if (!x58_full_pci_intx_route(dev, pin, &route))
					die("[ICH10] final IRQ missing native PRT route\n");
				if (apply)
					printk(BIOS_DEBUG, "[ICH10] INTX_ROUTE BUS=%02x DEVFN=%02x PIN=%u "
					       "ROOT=%02x.%u PRT_SLOT=%u PRT_PIN=%u APIC_GSI=%u LEGACY_LINE=11 "
					       "NATIVE_PRT=1 DELIVERY_TESTED=0\n",
					       dev->upstream->secondary, dev->path.pci.devfn, pin,
					       PCI_SLOT(route.root_devfn), PCI_FUNC(route.root_devfn),
					       route.slot, route.pin - 1, route.gsi);
			}
			if (!apply || pin == 0)
				continue;
			routed++;
			if (pci_read_config8(dev, PCI_INTERRUPT_LINE) != X58_FULL_ICH10_PIRQ_IRQ) {
				pci_write_config8(dev, PCI_INTERRUPT_LINE, X58_FULL_ICH10_PIRQ_IRQ);
				changed++;
			}
			if (pci_read_config8(dev, PCI_INTERRUPT_LINE) != X58_FULL_ICH10_PIRQ_IRQ)
				die("[ICH10] final IRQ line readback failed; no retry\n");
		}
	}
	printk(BIOS_NOTICE, "[ICH10] FINAL_INT_LINE IRQ=11 FUNCTIONS=%u CHANGED=%u "
	       "AFTER_RESOURCES=1 NATIVE_PRT=1 INTX_DELIVERY_TESTED=0\n",
	       routed, changed);
}

BOOT_STATE_INIT_ENTRY(BS_POST_DEVICE, BS_ON_ENTRY, x58_full_ich10_final_irq, NULL);
