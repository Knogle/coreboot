/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef B565D4_DESKTOP_PCI_IRQ_H
#define B565D4_DESKTOP_PCI_IRQ_H

#include <amdblocks/amd_pci_util.h>
#include <types.h>

#define B565D4_DESKTOP_IRQ_ROUTES 22
#define B565D4_DESKTOP_NBIF_REMAPS 18

bool b565d4_desktop_decode_irq_routes(const uint32_t *words, const uint32_t *remap,
	struct pci_routing_info *routes, size_t capacity);
void b565d4_desktop_pci_irq_ssdt(const struct device *domain);

#endif
