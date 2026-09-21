/* SPDX-License-Identifier: GPL-2.0-only */

#include <arch/io.h>
#include <device/pci_ops.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ids.h>
#include <console/console.h>
#include "chip.h"
#include "i82801jx.h"
#include "pcie_init.h"
#include "board_policy.h"

typedef struct southbridge_intel_i82801jx_config config_t;

static void i82801jx_enable_device(struct device *dev)
{
	/* Enable SERR */
	pci_or_config16(dev, PCI_COMMAND, PCI_COMMAND_SERR);
}

static void i82801jx_early_settings(const config_t *const info)
{
	/* Program FERR# as processor break event indicator. */
	RCBA32(GCS) |= (1 << 6);
	i82801jx_program_required_fields();
	/* RCBA32(RCBA_CIR5) |= (1 << 0); cf. Specification Update */
}

static void i82801jx_pcie_init(const config_t *const info)
{
	struct device *ports[I82801JX_PCIE_PORT_COUNT];

	for (unsigned int i = 0; i < I82801JX_PCIE_PORT_COUNT; i++)
		ports[i] = pcidev_on_root(0x1c, i);
	if (CONFIG(SOUTHBRIDGE_INTEL_I82801JX_BOARD_OWNED_DEVICE_POLICY))
		i82801jx_pcie_setup_retained(ports);
	else
		i82801jx_pcie_setup(ports, info);
}

static int i82801jx_function_disabled(const unsigned int devfn)
{
	struct device *const dev = pcidev_path_on_root(devfn);
	/* An absent node/function is not a board request to disable hardware.
	 * FD is monotonic; already hidden functions retain their existing bits.
	 */
	if (CONFIG(SOUTHBRIDGE_INTEL_I82801JX_BOARD_OWNED_DEVICE_POLICY))
		return dev && !dev->enabled &&
			pci_read_config16(dev, PCI_VENDOR_ID) == PCI_VID_INTEL;

	if (!dev) {
		printk(BIOS_EMERG,
		       "PCI device 00:%x.%x",
		       PCI_SLOT(devfn), PCI_FUNC(devfn));
		die(" is not listed in devicetree.\n");
	}
	return !dev->enabled;
}

static void i82801jx_hide_functions(void)
{
	int i;
	u32 reg32;

	/* FIXME: This works pretty good if the devicetree is consistent. But
	          some functions have to be disabled in right order and/or have
		  other constraints. */

	if (i82801jx_function_disabled(PCI_DEVFN(0x19, 0)))
		RCBA32(RCBA_BUC) |= BUC_LAND;

	reg32 = RCBA32(RCBA_FD);
	const u32 original_fd = reg32;
	struct {
		int devfn;
		u32 mask;
	} functions[] = {
		{ PCI_DEVFN(0x1a, 0), FD_U4D },		/* UHCI #4 */
		{ PCI_DEVFN(0x1a, 1), FD_U5D },		/* UHCI #5 */
		{ PCI_DEVFN(0x1a, 2), FD_U6D },		/* UHCI #6 */
		{ PCI_DEVFN(0x1a, 7), FD_EHCI2D },	/* EHCI #2 */
		{ PCI_DEVFN(0x1b, 0), FD_HDAD },	/* HD Audio */
		{ PCI_DEVFN(0x1c, 0), FD_PE1D },	/* PCIe #1 */
		{ PCI_DEVFN(0x1c, 1), FD_PE2D },	/* PCIe #2 */
		{ PCI_DEVFN(0x1c, 2), FD_PE3D },	/* PCIe #3 */
		{ PCI_DEVFN(0x1c, 3), FD_PE4D },	/* PCIe #4 */
		{ PCI_DEVFN(0x1c, 4), FD_PE5D },	/* PCIe #5 */
		{ PCI_DEVFN(0x1c, 5), FD_PE6D },	/* PCIe #6 */
		{ PCI_DEVFN(0x1d, 0), FD_U1D },		/* UHCI #1 */
		{ PCI_DEVFN(0x1d, 1), FD_U2D },		/* UHCI #2 */
		{ PCI_DEVFN(0x1d, 2), FD_U3D },		/* UHCI #3 */
		{ PCI_DEVFN(0x1d, 7), FD_EHCI1D },	/* EHCI #1 */
		{ PCI_DEVFN(0x1f, 0), FD_LBD },		/* LPC */
		{ PCI_DEVFN(0x1f, 2), FD_SAD1 },	/* SATA #1 */
		{ PCI_DEVFN(0x1f, 3), FD_SD },		/* SMBus */
		{ PCI_DEVFN(0x1f, 5), FD_SAD2 },	/* SATA #2 */
		{ PCI_DEVFN(0x1f, 6), FD_TTD },		/* Thermal Throttle */
	};
	for (i = 0; i < ARRAY_SIZE(functions); ++i) {
		if (i82801jx_function_disabled(functions[i].devfn))
			reg32 |= functions[i].mask;
	}
	if (!CONFIG(SOUTHBRIDGE_INTEL_I82801JX_BOARD_OWNED_DEVICE_POLICY) ||
	    reg32 != original_fd)
		RCBA32(RCBA_FD) = reg32;
	/* The inherited numbering/capabilities are retained. There is no
	 * evidence for changing FDSW, remapping UHCI, or rewriting RPFN here.
	 * Only explicitly configured, present disabled functions above own FD.
	 */
	if (CONFIG(SOUTHBRIDGE_INTEL_I82801JX_BOARD_OWNED_DEVICE_POLICY))
		return;

	RCBA32(RCBA_FD) |= (1 << 0); /* BIOS must write this... */
	RCBA32(RCBA_FDSW) |= (1 << 7); /* Lock function-disable? */

	/* Hide PCIe root port PCI functions. RPFN is partially R/WO. */
	reg32 = RCBA32(RCBA_RPFN);
	for (i = 0; i < 6; ++i) {
		if (i82801jx_function_disabled(PCI_DEVFN(0x1c, i)))
			reg32 |= (1 << ((i * 4) + 3));
	}
	RCBA32(RCBA_RPFN) = reg32;

	/* Lock R/WO UHCI controller #6 remapping. */
	RCBA32(RCBA_MAP) = RCBA32(RCBA_MAP);
}

static void i82801jx_init(void *chip_info)
{
	const config_t *const info = (config_t *)chip_info;

	printk(BIOS_DEBUG, "Initializing i82801jx southbridge...\n");

	if (CONFIG(SOUTHBRIDGE_INTEL_I82801JX_BOARD_OWNED_DEVICE_POLICY))
		mainboard_ich10_pre_init();

	i82801jx_early_settings(info);

	if (CONFIG(SOUTHBRIDGE_INTEL_I82801JX_BOARD_OWNED_DEVICE_POLICY))
		mainboard_ich10_prepare_sata(info);

	/* PCI Express setup. */
	i82801jx_pcie_init(info);

	/* EHCI configuration. */
	i82801jx_ehci_init();

	/* Now hide internal functions. We can't access them after this. */
	i82801jx_hide_functions();

	/* No boot watchdog is installed on a board-owned no-SMM path. Its
	 * admission owns the bounded TCO halt and preserves status/recovery.
	 */
	if (CONFIG(SOUTHBRIDGE_INTEL_I82801JX_BOARD_OWNED_DEVICE_POLICY))
		return;

	/* Reset watchdog timer. */
#if !CONFIG(HAVE_SMI_HANDLER)
	outw(0x0008, DEFAULT_TCOBASE + 0x12); /* Set higher timer value. */
#endif
	outw(0x0000, DEFAULT_TCOBASE + 0x00); /* Update timer. */
}

struct chip_operations southbridge_intel_i82801jx_ops = {
	.name = "Intel ICH10 (82801Jx) Series Southbridge",
	.enable_dev	= i82801jx_enable_device,
	.init		= i82801jx_init,
};
