/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef X58_FULL_ICH10_ACPI_H
#define X58_FULL_ICH10_ACPI_H

/* Fixed board-root wiring is described by the board's native pci_irq.asl.
 * ICH10 descendants inherit those routes with standard PCI bridge swizzling.
 * Call before the full-chipset path changes hardware state.
 */
void x58_full_ich10_require_native_acpi(void);

#endif
