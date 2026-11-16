/* SPDX-License-Identifier: GPL-2.0-only */

#include "pci_irq.h"
#include <acpi/acpi_device.h>
#include <acpi/acpigen.h>
#include <acpi/acpigen_pci.h>
#include <amdblocks/smn.h>
#include <console/console.h>
#include <device/device.h>
#include <device/pci_def.h>
#include <device/pci_ids.h>
#include <string.h>

#define DESKTOP_IRQ_BASE 0x14600040
#define DESKTOP_REMAP_BASE 0x13e100b8
#define GNB_GSI_BASE 24

bool b565d4_desktop_decode_irq_routes(const uint32_t *words, const uint32_t *remap,
	struct pci_routing_info *routes, size_t capacity)
{
	/* Interrupt registers follow logical bridge numbering, not DXIO's
	 * physical-port remap. In particular, 01.2 uses the second register
	 * (INTA GSI28), even when the first remap entry contains DevFn 0a.
	 */
	static const uint8_t external_devfns[] = {
		0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x11, 0x19,
		0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x21, 0x29, 0x2a,
	};
	/* The last two external route words follow the four internal words. */
	static const uint8_t irq_index[] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
		20, 21, 16, 17, 18, 19,
	};
	static const uint8_t internal_devfns[] = { 0x39, 0x41, 0x42, 0x43 };
	struct pci_routing_info plan[B565D4_DESKTOP_IRQ_ROUTES];
	uint32_t used = 0;

	if (!words || !remap || !routes || capacity < ARRAY_SIZE(plan))
		return false;

	for (size_t i = 0; i < ARRAY_SIZE(plan); i++) {
		const uint32_t word = words[irq_index[i]];
		uint8_t devfn;

		if (word == UINT32_MAX)
			return false;
		if (i < ARRAY_SIZE(external_devfns)) {
			size_t slot;

			if (remap[i] == UINT32_MAX)
				return false;
			devfn = remap[i] & 0xff;
			for (slot = 0; slot < ARRAY_SIZE(external_devfns); slot++)
				if (devfn == external_devfns[slot])
					break;
			if (slot == ARRAY_SIZE(external_devfns) || (used & (1U << slot)))
				return false;
			used |= 1U << slot;
			devfn = external_devfns[i];
		} else {
			devfn = internal_devfns[i - ARRAY_SIZE(external_devfns)];
		}
		plan[i] = (struct pci_routing_info) {
			.devfn = devfn,
			.group = word & 7,
			/* Hardware subtracts the encoded rotation; coreboot adds it. */
			.swizzle = (4 - ((word >> 4) & 3)) & 3,
			.bridge_irq = (word >> 16) & 31,
		};
	}
	memcpy(routes, plan, sizeof(plan));
	return true;
}

void b565d4_desktop_pci_irq_ssdt(const struct device *domain)
{
	uint32_t words[B565D4_DESKTOP_IRQ_ROUTES];
	uint32_t remap[B565D4_DESKTOP_NBIF_REMAPS];
	struct pci_routing_info routes[B565D4_DESKTOP_IRQ_ROUTES];

	if (!domain->downstream)
		return;
	for (size_t i = 0; i < ARRAY_SIZE(words); i++)
		words[i] = smn_read32(DESKTOP_IRQ_BASE + sizeof(uint32_t) * i);
	for (size_t i = 0; i < ARRAY_SIZE(remap); i++)
		remap[i] = smn_read32(DESKTOP_REMAP_BASE + sizeof(uint32_t) * i);
	if (!b565d4_desktop_decode_irq_routes(words, remap, routes, ARRAY_SIZE(routes))) {
		printk(BIOS_ERR, "Desktop PCI INTx: invalid GNB routing/remap snapshot\n");
		return;
	}

	for (const struct device *dev = domain->downstream->children; dev;
	     dev = dev->sibling) {
		const char *path;

		if (!dev->enabled || dev->path.type != DEVICE_PATH_PCI ||
		    dev->class >> 8 != PCI_CLASS_BRIDGE_PCI)
			continue;
		path = acpi_device_path(dev);
		if (!path)
			continue;
		for (size_t i = 0; i < ARRAY_SIZE(routes); i++) {
			if (routes[i].devfn != dev->path.pci.devfn)
				continue;
			/* Only children of this exact bridge are described. Root-port
			 * own interrupts use a separate map and are not inferred here.
			 * An IOAPIC-only table needs no PICM or legacy link objects.
			 */
			acpigen_write_scope(path);
			acpigen_write_name("_PRT");
			acpigen_write_package(4);
			for (unsigned int pin = 0; pin < 4; pin++)
				acpigen_write_PRT_GSI_entry(0, pin,
					GNB_GSI_BASE + pci_calculate_irq(&routes[i], pin));
			acpigen_pop_len();
			acpigen_pop_len();
			printk(BIOS_INFO, "Desktop PCI INTx: %s group=%u swizzle=%u\n",
			       path, routes[i].group, routes[i].swizzle);
			break;
		}
	}
}
