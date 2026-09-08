/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * USBADMIT1 is deliberately observational.  Structural inconsistencies stop
 * the boot, but over-current and connect state are telemetry, never a reason
 * to mutate PORTSC, GPIO, over-current routing, or controller state here.
 */

#include <arch/io.h>
#include <arch/pci_io_cfg.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <device/mmio.h>
#include <device/pci_def.h>
#include <device/pci_type.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <southbridge/intel/i82801jx/i82801jx.h>

#include "b06wc_usb_admit.h"

#define B06WC_STAGE_ID		"B06WC-USBADMIT1"
#define POST_B06WC_BEGIN	0x83u
#define POST_B06WC_READY	0x84u
#define POST_B06WC_FAIL		0x85u

#define B06WC_LPC_DEV		PCI_DEV(0, 0x1f, 0)
#define B06WC_LPC_ID		0x3a168086u
#define B06WC_PMBASE_REG	0x40u
#define B06WC_PMBASE_ENABLED	(DEFAULT_PMBASE | 1u)
#define B06WC_ACPI_CNTL_ENABLED	0x80u
#define B06WC_GPIOBASE_ENABLED	(DEFAULT_GPIOBASE | 1u)
#define B06WC_GPIO_CNTL_ENABLED	0x10u

#define B06WC_PCI_IO_BASE	0x1000u
#define B06WC_PCI_IO_TOP	0x10000u
#define B06WC_PCI_MMIO_BASE	0xc0000000u
#define B06WC_PCI_MMIO_TOP	0xe0000000u
#define B06WC_EHCI_BAR_SIZE	0x400u
#define B06WC_UHCI_BAR_SIZE	0x20u

#define B06WC_RCBA_PPO		0x3524u
#define B06WC_PMBASE_UPRWC	0x3cu
#define B06WC_USB_CG_DISABLE	BIT(20)
#define B06WC_USB_PPO_MASK	0x0fffu
#define B06WC_USB_MAP_MODE	BIT(0)
#define B06WC_USB_GPIO1_OC_MASK	0xe0000000u
#define B06WC_USB_GPIO2_OC_MASK	0x0800ff00u

#define B06WC_EHCI_CLASS	0x0c0320u
#define B06WC_UHCI_CLASS	0x0c0300u
#define B06WC_EHCI_PMCSR	0x54u
#define B06WC_EHCI_CAPLEN	0x20u
#define B06WC_EHCI_HCIVER	0x0100u
#define B06WC_EHCI_HCSPARAMS	0x00103206u
#define B06WC_EHCI_USBCMD	0x00u
#define B06WC_EHCI_USBSTS	0x04u
#define B06WC_EHCI_USBINTR	0x08u
#define B06WC_EHCI_CONFIGFLAG	0x40u
#define B06WC_EHCI_PORTSC	0x44u
#define B06WC_EHCI_PORT_COUNT	6u
#define B06WC_EHCI_PORT_CCS	BIT(0)
#define B06WC_EHCI_PORT_PE	BIT(2)
#define B06WC_EHCI_PORT_OCA	BIT(4)
#define B06WC_EHCI_PORT_OCC	BIT(5)

#define B06WC_UHCI_USBCMD	0x00u
#define B06WC_UHCI_USBSTS	0x02u
#define B06WC_UHCI_USBINTR	0x04u
#define B06WC_UHCI_PORTSC1	0x10u
#define B06WC_UHCI_PORTSC2	0x12u
#define B06WC_UHCI_LEGKEY	0xc0u
#define B06WC_UHCI_CFG_C8	0xc8u
#define B06WC_UHCI_CFG_CA	0xcau
#define B06WC_UHCI_PORT_COUNT	2u
#define B06WC_UHCI_PORT_CCS	BIT(0)
#define B06WC_UHCI_PORT_PE	BIT(2)
#define B06WC_UHCI_PORT_OCA	BIT(10)
#define B06WC_UHCI_PORT_OCC	BIT(11)

#define B06WC_USB_FD_DISABLE_MASK \
	(FD_EHCI1D | FD_EHCI2D | FD_U1D | FD_U2D | FD_U3D | FD_U4D | \
	 FD_U5D | FD_U6D)

struct b06wc_controller {
	pci_devfn_t dev;
	uint32_t id;
	const char *name;
};

struct b06wc_port_summary {
	uint16_t ccs;
	uint16_t pe;
	uint16_t oca;
	uint16_t occ;
};

static const struct b06wc_controller b06wc_ehci[] = {
	{ PCI_DEV(0, 0x1a, 7), 0x3a3c8086u, "EHCI2" },
	{ PCI_DEV(0, 0x1d, 7), 0x3a3a8086u, "EHCI1" },
};

static const struct b06wc_controller b06wc_uhci[] = {
	{ PCI_DEV(0, 0x1a, 0), 0x3a378086u, "UHCI4" },
	{ PCI_DEV(0, 0x1a, 1), 0x3a388086u, "UHCI5" },
	{ PCI_DEV(0, 0x1a, 2), 0x3a398086u, "UHCI6" },
	{ PCI_DEV(0, 0x1d, 0), 0x3a348086u, "UHCI1" },
	{ PCI_DEV(0, 0x1d, 1), 0x3a358086u, "UHCI2" },
	{ PCI_DEV(0, 0x1d, 2), 0x3a368086u, "UHCI3" },
};

static __noreturn void b06wc_fail(const char *reason, const char *name)
{
	die_with_post_code(POST_B06WC_FAIL,
		"[USB-ADMIT] %s FAIL reason=%s controller=%s\n",
		B06WC_STAGE_ID, reason, name);
}

static void b06wc_gate_function(const struct b06wc_controller *controller,
	uint32_t expected_class)
{
	const uint32_t id = pci_io_read_config32(controller->dev, PCI_VENDOR_ID);
	const uint32_t class_rev = pci_io_read_config32(controller->dev, PCI_CLASS_REVISION);
	const uint8_t header = pci_io_read_config8(controller->dev, PCI_HEADER_TYPE);

	printk(BIOS_NOTICE,
	       "[USB-ADMIT] FUNC %s ID=%08x CLASS=%06x HDR=%02x\n",
	       controller->name, id, class_rev >> 8, header);
	if (id != controller->id)
		b06wc_fail("identity", controller->name);
	if ((class_rev >> 8) != expected_class)
		b06wc_fail("class", controller->name);
	if ((header & 0x7fu) != PCI_HEADER_TYPE_NORMAL)
		b06wc_fail("header", controller->name);
}

static void b06wc_census_ehci(const struct b06wc_controller *controller,
	unsigned int controller_index, struct b06wc_port_summary *summary)
{
	uint16_t command, pmcsr;
	uint32_t bar, fc, base;
	uintptr_t op;

	b06wc_gate_function(controller, B06WC_EHCI_CLASS);
	command = pci_io_read_config16(controller->dev, PCI_COMMAND);
	bar = pci_io_read_config32(controller->dev, PCI_BASE_ADDRESS_0);
	pmcsr = pci_io_read_config16(controller->dev, B06WC_EHCI_PMCSR);
	fc = pci_io_read_config32(controller->dev, I82801JX_EHCI_FCREG);
	base = bar & ~PCI_BASE_ADDRESS_MEM_ATTR_MASK;
	if ((command & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER)) !=
	    PCI_COMMAND_MEMORY)
		b06wc_fail("EHCI-command", controller->name);
	if ((bar & PCI_BASE_ADDRESS_SPACE_IO) || base < B06WC_PCI_MMIO_BASE ||
	    base > B06WC_PCI_MMIO_TOP - B06WC_EHCI_BAR_SIZE)
		b06wc_fail("EHCI-BAR", controller->name);
	if (pmcsr & 3u)
		b06wc_fail("EHCI-power-state", controller->name);
	if ((fc & I82801JX_EHCI_FCREG_REQUIRED_MASK) !=
	    I82801JX_EHCI_FCREG_REQUIRED_VALUE)
		b06wc_fail("EHCIIR2", controller->name);
	if (read8p(base) != B06WC_EHCI_CAPLEN ||
	    read16p(base + 2u) != B06WC_EHCI_HCIVER ||
	    read32p(base + 4u) != B06WC_EHCI_HCSPARAMS)
		b06wc_fail("EHCI-capability", controller->name);

	op = base + B06WC_EHCI_CAPLEN;
	printk(BIOS_NOTICE,
	       "[USB-ADMIT] %s CMD=%04x BAR=%08x PMCSR=%04x EHCIIR2=%08x "
	       "USBCMD=%08x USBSTS=%08x USBINTR=%08x CONFIGFLAG=%08x\n",
	       controller->name, command, bar, pmcsr, fc,
	       read32p(op + B06WC_EHCI_USBCMD), read32p(op + B06WC_EHCI_USBSTS),
	       read32p(op + B06WC_EHCI_USBINTR),
	       read32p(op + B06WC_EHCI_CONFIGFLAG));
	for (unsigned int port = 0; port < B06WC_EHCI_PORT_COUNT; port++) {
		const uint32_t portsc = read32p(op + B06WC_EHCI_PORTSC + 4u * port);
		const unsigned int bit = controller_index * B06WC_EHCI_PORT_COUNT + port;

		if (portsc & B06WC_EHCI_PORT_CCS)
			summary->ccs |= BIT(bit);
		if (portsc & B06WC_EHCI_PORT_PE)
			summary->pe |= BIT(bit);
		if (portsc & B06WC_EHCI_PORT_OCA)
			summary->oca |= BIT(bit);
		if (portsc & B06WC_EHCI_PORT_OCC)
			summary->occ |= BIT(bit);
		printk(BIOS_NOTICE, "[USB-ADMIT] %s PORTSC%u=%08x\n",
		       controller->name, port + 1u, portsc);
	}
}

static void b06wc_census_uhci(const struct b06wc_controller *controller,
	unsigned int controller_index, struct b06wc_port_summary *summary)
{
	uint16_t command;
	uint32_t bar, base;

	b06wc_gate_function(controller, B06WC_UHCI_CLASS);
	command = pci_io_read_config16(controller->dev, PCI_COMMAND);
	bar = pci_io_read_config32(controller->dev, PCI_BASE_ADDRESS_4);
	base = bar & ~PCI_BASE_ADDRESS_IO_ATTR_MASK;
	if ((command & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER)) !=
	    PCI_COMMAND_IO)
		b06wc_fail("UHCI-command", controller->name);
	if (!(bar & PCI_BASE_ADDRESS_SPACE_IO) || base < B06WC_PCI_IO_BASE ||
	    base > B06WC_PCI_IO_TOP - B06WC_UHCI_BAR_SIZE)
		b06wc_fail("UHCI-BAR", controller->name);

	printk(BIOS_NOTICE,
	       "[USB-ADMIT] %s CMD=%04x BAR4=%08x LEGKEY=%04x CFGC8=%04x "
	       "CFGCA=%04x USBCMD=%04x USBSTS=%04x USBINTR=%04x\n",
	       controller->name, command, bar,
	       pci_io_read_config16(controller->dev, B06WC_UHCI_LEGKEY),
	       pci_io_read_config16(controller->dev, B06WC_UHCI_CFG_C8),
	       pci_io_read_config16(controller->dev, B06WC_UHCI_CFG_CA),
	       inw(base + B06WC_UHCI_USBCMD), inw(base + B06WC_UHCI_USBSTS),
	       inw(base + B06WC_UHCI_USBINTR));
	for (unsigned int port = 0; port < B06WC_UHCI_PORT_COUNT; port++) {
		const uint16_t portsc = inw(base + B06WC_UHCI_PORTSC1 + 2u * port);
		const unsigned int bit = controller_index * B06WC_UHCI_PORT_COUNT + port;

		if (portsc & B06WC_UHCI_PORT_CCS)
			summary->ccs |= BIT(bit);
		if (portsc & B06WC_UHCI_PORT_PE)
			summary->pe |= BIT(bit);
		if (portsc & B06WC_UHCI_PORT_OCA)
			summary->oca |= BIT(bit);
		if (portsc & B06WC_UHCI_PORT_OCC)
			summary->occ |= BIT(bit);
		printk(BIOS_NOTICE, "[USB-ADMIT] %s PORTSC%u=%04x\n",
		       controller->name, port + 1u, portsc);
	}
}

void b06wc_usb_admit(void)
{
	struct b06wc_port_summary ehci = { 0 };
	struct b06wc_port_summary uhci = { 0 };
	const uint32_t lpc_id = pci_io_read_config32(B06WC_LPC_DEV, PCI_VENDOR_ID);
	const uint32_t rcba = pci_io_read_config32(B06WC_LPC_DEV, RCBA);
	const uint32_t pmbase = pci_io_read_config32(B06WC_LPC_DEV, B06WC_PMBASE_REG);
	const uint8_t acpi_cntl = pci_io_read_config8(B06WC_LPC_DEV, ACPI_CNTL);
	const uint32_t gpiobase = pci_io_read_config32(B06WC_LPC_DEV, GPIOBASE);
	const uint8_t gpio_cntl = pci_io_read_config8(B06WC_LPC_DEV, D31F0_GPIO_CNTL);
	uint32_t fd, cg, map, gpio1, gpio2;
	uint16_t ppo;

	post_code(POST_B06WC_BEGIN);
	printk(BIOS_NOTICE, "[USB-ADMIT] %s BEGIN READ_ONLY=1\n", B06WC_STAGE_ID);
	if (lpc_id != B06WC_LPC_ID)
		b06wc_fail("LPC-identity", "LPC");
	if (rcba != (CONFIG_FIXED_RCBA_MMIO_BASE | 1u))
		b06wc_fail("RCBA-decode", "LPC");
	if (pmbase != B06WC_PMBASE_ENABLED || acpi_cntl != B06WC_ACPI_CNTL_ENABLED)
		b06wc_fail("PM-decode", "LPC");
	if (gpiobase != B06WC_GPIOBASE_ENABLED || gpio_cntl != B06WC_GPIO_CNTL_ENABLED)
		b06wc_fail("GPIO-decode", "LPC");

	/* RCBA/GPIO/PM reads are admitted only after all three exact decodes. */
	fd = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_FD);
	cg = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_CG);
	ppo = read16p(CONFIG_FIXED_RCBA_MMIO_BASE + B06WC_RCBA_PPO);
	map = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_MAP);
	gpio1 = inl(DEFAULT_GPIOBASE + GP_IO_USE_SEL);
	gpio2 = inl(DEFAULT_GPIOBASE + GP_IO_USE_SEL2);
	printk(BIOS_NOTICE,
	       "[USB-ADMIT] GLOBAL LPC=%08x RCBA=%08x PM=%08x/%02x GPIO=%08x/%02x "
	       "FD=%08x CG=%08x PPO=%04x MAP=%08x UPRWC=%04x GPIO_USE=%08x/%08x\n",
	       lpc_id, rcba, pmbase, acpi_cntl, gpiobase, gpio_cntl, fd, cg, ppo,
	       map, inw(DEFAULT_PMBASE + B06WC_PMBASE_UPRWC), gpio1, gpio2);
	if (fd & B06WC_USB_FD_DISABLE_MASK)
		b06wc_fail("USB-function-disabled", "RCBA");
	if (cg & B06WC_USB_CG_DISABLE)
		b06wc_fail("USB-clock-disabled", "RCBA");
	if (ppo & B06WC_USB_PPO_MASK)
		b06wc_fail("USB-port-routing", "RCBA");
	if (map & B06WC_USB_MAP_MODE)
		b06wc_fail("UHCI6-remap", "RCBA");
	if ((gpio1 & B06WC_USB_GPIO1_OC_MASK) ||
	    (gpio2 & B06WC_USB_GPIO2_OC_MASK))
		b06wc_fail("OC-pins-not-native", "GPIO");

	for (size_t i = 0; i < ARRAY_SIZE(b06wc_ehci); i++)
		b06wc_census_ehci(&b06wc_ehci[i], i, &ehci);
	for (size_t i = 0; i < ARRAY_SIZE(b06wc_uhci); i++)
		b06wc_census_uhci(&b06wc_uhci[i], i, &uhci);

	/* PORTSC OCA/OCC/CCS/PE are classified, never structural gates. */
	printk(BIOS_NOTICE,
	       "[USB-ADMIT] ELECTRICAL EHCI CCS=%03x PE=%03x OCA=%03x OCC=%03x "
	       "UHCI CCS=%03x PE=%03x OCA=%03x OCC=%03x CLASS=%s\n",
	       ehci.ccs, ehci.pe, ehci.oca, ehci.occ,
	       uhci.ccs, uhci.pe, uhci.oca, uhci.occ,
	       (ehci.ccs || uhci.ccs) && (ehci.oca || uhci.oca) ? "CONNECT_OC" :
	       (ehci.ccs || uhci.ccs) ? "CONNECT" :
	       ((ehci.oca || uhci.oca) ? "OC_NO_CONNECT" : "IDLE_NO_CONNECT"));
	post_code(POST_B06WC_READY);
	printk(BIOS_NOTICE,
	       "[USB-ADMIT] %s READY STRUCTURAL=PASS ELECTRICAL=CLASSIFIED MUTATIONS=0\n",
	       B06WC_STAGE_ID);
}
