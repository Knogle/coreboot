/* SPDX-License-Identifier: GPL-2.0-only */

#include <acpi/acpigen.h>
#include <console/console.h>
#include <device/device.h>
#include <device/pci_def.h>
#include "setup_options.h"

/* Match the actual wired onboard I210 path, independently of bus numbers. */
static bool onboard_i210_path(const struct device *dev, const struct device *domain)
{
	const struct device *port, *upstream, *root;

	if (!dev->enabled || dev->path.type != DEVICE_PATH_PCI ||
	    dev->vendor != 0x8086 || dev->device != 0x1533 ||
	    dev->path.pci.devfn != PCI_DEVFN(0, 0) || !dev->upstream)
		return false;
	port = dev->upstream->dev;
	if (!port || !port->enabled || port->path.type != DEVICE_PATH_PCI ||
	    port->vendor != 0x1022 || port->device != 0x43ea ||
	    port->path.pci.devfn != PCI_DEVFN(8, 0) || !port->upstream)
		return false;
	upstream = port->upstream->dev;
	if (!upstream || !upstream->enabled || upstream->path.type != DEVICE_PATH_PCI ||
	    upstream->vendor != 0x1022 || upstream->device != 0x43e9 ||
	    upstream->path.pci.devfn != PCI_DEVFN(0, 2) || !upstream->upstream)
		return false;
	root = upstream->upstream->dev;
	return root && root->enabled && root->path.type == DEVICE_PATH_PCI &&
		root->vendor == 0x1022 && root->path.pci.devfn == PCI_DEVFN(2, 1) &&
		root->upstream && root->upstream->dev == domain;
}

void b565d4_setup_acpi_wake(const struct device *domain)
{
	const struct device *dev;

	if (!b565d4_setup_get(B565D4_OPT_WOL))
		return;
	for (dev = all_devices; dev; dev = dev->next)
		if (onboard_i210_path(dev, domain))
			break;
	if (!dev) {
		printk(BIOS_NOTICE, "Board wake: onboard I210 path absent; no wake AML\n");
		return;
	}

	/* Actual ASRock V22K PT02/PT28 identifies GPE 0x0d for this wired port.
	 * Native S5 qualification is required for the extended sleep-state limit.
	 * The OS still owns PME arming and wake permission; no GPIO/SMI routing
	 * or device power-control register is written by this table generator.
	 */
	acpigen_write_scope("\\_SB.PCI0.GP11");
	acpigen_write_device("PT02");
	acpigen_write_name_dword("_ADR", 0x00000002);
	acpigen_write_device("PT28");
	acpigen_write_name_dword("_ADR", 0x00080000);
	acpigen_write_PRW(0x0d, 5);
	acpigen_write_device("LAN");
	acpigen_write_name_dword("_ADR", 0);
	acpigen_pop_len(); /* LAN */
	acpigen_pop_len(); /* PT28 */
	acpigen_pop_len(); /* PT02 */
	acpigen_pop_len(); /* GP11 */

	acpigen_write_scope("\\_GPE");
	acpigen_write_method("_L0D", 0);
	acpigen_notify("\\_SB.PCI0.GP11.PT02.PT28.LAN", 2);
	acpigen_pop_len(); /* _L0D */
	acpigen_pop_len(); /* _GPE */
	printk(BIOS_NOTICE, "Board wake: onboard I210 PT28 GPE0d through S5 described\n");
}
