/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * B06WF is an operator-driven experiment, not permanent USB policy.  It runs
 * after device initialization/finalization and before ACPI/table creation.
 * Every reversible command admits one exact B06WD-derived prestate, records
 * complete controller/port state, and restores the changed control registers.
 * PORTSC is never read-modify-written.  The isolated OC-latch acknowledgement
 * is deliberately non-reversible and prevents continuation to the payload.
 */

#include <arch/io.h>
#include <arch/pci_io_cfg.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <console/streams.h>
#include <delay.h>
#include <device/mmio.h>
#include <device/pci_def.h>
#include <device/pci_type.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <southbridge/intel/common/pmutil.h>
#include <southbridge/intel/i82801jx/i82801jx.h>

#include "b06wf_usb_lab.h"

#define B06WF_STAGE_ID		"B06WF-LATE-USB-LAB2"

#define POST_B06WF_LAB_ENTRY	0x90u
#define POST_B06WF_LAB_READY	0x91u
#define POST_B06WF_MUTATION	0x92u
#define POST_B06WF_ROLLBACK	0x93u
#define POST_B06WF_CONTINUE	0x94u
#define POST_B06WF_FAIL		0x95u
#define POST_B06WF_NONREV	0x96u
#define POST_B06WF_PWRGD_READY	0x97u

#define B06WF_LPC_DEV		PCI_DEV(0, 0x1f, 0)
#define B06WF_LPC_ID		0x3a168086u
#define B06WF_PMBASE_REG		0x40u
#define B06WF_PMBASE_EXPECTED	(DEFAULT_PMBASE | 1u)
#define B06WF_ACPI_CNTL_EXPECTED	0x80u
#define B06WF_GPIOBASE_EXPECTED	(DEFAULT_GPIOBASE | 1u)
#define B06WF_GPIO_CNTL_EXPECTED	0x10u

#define B06WF_RCBA_PPO		0x3524u
#define B06WF_PMBASE_UPRWC	0x3cu
#define B06WF_FD_EXPECTED	0x02000001u
#define B06WF_CG_EXPECTED	0x00000000u
#define B06WF_PPO_EXPECTED	0x0000u
#define B06WF_MAP_EXPECTED	0x00000000u
#define B06WF_UPRWC_EXPECTED	0x0000u
#define B06WF_GPIO_USE1_EXPECTED	0x197e75ffu
#define B06WF_GPIO_USE2_EXPECTED	0x030300ffu
#define B06WF_GPIO_DIR2_EXPECTED	0x0f55fff0u
#define B06WF_GPIO_LVL2_EXPECTED	0x15ff00d3u
#define B06WF_GPIO56		BIT(24)
#define B06WF_GPIO57		BIT(25)
/* Only already-configured GPIO outputs are stable; native OC inputs are not. */
#define B06WF_GPIO_LVL2_STABLE_MASK 0x0002000fu
#define B06WF_GPIO_BANK2_MSB	3u
#define B06WF_GPIO56_BYTE	BIT(0)
#define B06WF_GPIO57_BYTE	BIT(1)
/*
 * The live XRS primitive used 0x10000 uncalibrated PAUSE instructions.  This
 * lab deliberately gives the low phase 0x10000 calibrated microseconds; the
 * equal numeric values do not imply equal time units.
 */
#define B06WF_GPIO57_XRS_DELAY_PAUSES 0x00010000u
#define B06WF_GPIO57_LOW_SETTLE_US 0x00010000u

#define B06WF_EHCI_COUNT		2u
#define B06WF_EHCI_PORT_COUNT	6u
#define B06WF_UHCI_COUNT		6u
#define B06WF_UHCI_PORT_COUNT	2u
#define B06WF_TOTAL_PORTS	12u

#define B06WF_EHCI_CLASS		0x0c0320u
#define B06WF_UHCI_CLASS		0x0c0300u
#define B06WF_EHCI_BAR_SIZE	0x400u
#define B06WF_UHCI_BAR_SIZE	0x20u
#define B06WF_PCI_IO_BASE	0x1000u
#define B06WF_PCI_IO_TOP		0x10000u
#define B06WF_PCI_MMIO_BASE	0xc0000000u
#define B06WF_PCI_MMIO_TOP	0xe0000000u

#define B06WF_EHCI_PMCSR		0x54u
#define B06WF_EHCI_LEGACY	0x68u
#define B06WF_EHCI_LEGACY_CTL	0x6cu
#define B06WF_EHCI_LEGACY_EXT	0x70u
#define B06WF_EHCI_CFG84		0x84u
/* Live B06WD late state: SMI enables are clear; upper status bits remain set. */
#define B06WF_EHCI_LEGACY_CTL_EXPECTED 0xc0040000u
#define B06WF_EHCIIR2_EXPECTED	0x2002170au
#define B06WF_EHCI_CAPLEN	0x20u
#define B06WF_EHCI_HCIVER	0x0100u
#define B06WF_EHCI_HCSPARAMS	0x00103206u
#define B06WF_EHCI_USBCMD	0x00u
#define B06WF_EHCI_USBSTS	0x04u
#define B06WF_EHCI_USBINTR	0x08u
#define B06WF_EHCI_CONFIGFLAG	0x40u
#define B06WF_EHCI_PORTSC	0x44u
#define B06WF_EHCI_USBCMD_EXPECTED 0x00080000u
#define B06WF_EHCI_USBSTS_EXPECTED 0x00001004u
#define B06WF_EHCI_PORT_EXPECTED	0x00003030u
#define B06WF_EHCI_PORT_OWNER	BIT(13)
#define B06WF_EHCI_PORT_CCS	BIT(0)
#define B06WF_EHCI_PORT_PE	BIT(2)
#define B06WF_EHCI_PORT_OCA	BIT(4)
#define B06WF_EHCI_PORT_OCC	BIT(5)
/* Exact 3030 prestate: preserve owner/power and write one only to OCC. */
#define B06WF_EHCI_OC_ACK_WRITE	0x00003020u
#define B06WF_EHCI_OC_ACK_EXPECTED 0x00003010u

#define B06WF_UHCI_LEGKEY	0xc0u
#define B06WF_UHCI_CFG_C8	0xc8u
#define B06WF_UHCI_CFG_CA	0xcau
#define B06WF_UHCI_USBCMD	0x00u
#define B06WF_UHCI_USBSTS	0x02u
#define B06WF_UHCI_USBINTR	0x04u
#define B06WF_UHCI_PORTSC1	0x10u
#define B06WF_UHCI_PORT_EXPECTED	0x0c80u
#define B06WF_UHCI_PORT_CCS	BIT(0)
#define B06WF_UHCI_PORT_PE	BIT(2)
#define B06WF_UHCI_PORT_LSDA	BIT(8)
#define B06WF_UHCI_PORT_OCA	BIT(10)
#define B06WF_UHCI_PORT_OCI	BIT(11)
/* Exact 0c80 prestate: write one only to OCI; reserved/RO fields are zero. */
#define B06WF_UHCI_OC_ACK_WRITE	0x0800u
#define B06WF_UHCI_OC_ACK_EXPECTED 0x0480u

struct b06wf_controller_desc {
	pci_devfn_t dev;
	uint32_t id;
	const char *name;
};

struct b06wf_ehci_snapshot {
	uint16_t command;
	uint16_t pmcsr;
	uint32_t bar;
	uint32_t ehciir2;
	uint32_t legacy;
	uint32_t legacy_ctl;
	uint32_t legacy_ext;
	uint32_t cfg84;
	uintptr_t op;
	uint32_t usbcmd;
	uint32_t usbsts;
	uint32_t usbintr;
	uint32_t configflag;
	uint32_t portsc[B06WF_EHCI_PORT_COUNT];
};

struct b06wf_uhci_snapshot {
	uint16_t command;
	uint16_t legkey;
	uint16_t cfg_c8;
	uint16_t cfg_ca;
	uint32_t bar;
	uint16_t base;
	uint16_t usbcmd;
	uint16_t usbsts;
	uint16_t usbintr;
	uint16_t portsc[B06WF_UHCI_PORT_COUNT];
};

struct b06wf_snapshot {
	uint32_t lpc_id;
	uint32_t rcba;
	uint32_t pmbase;
	uint8_t acpi_cntl;
	uint32_t gpiobase;
	uint8_t gpio_cntl;
	uint32_t fd;
	uint32_t cg;
	uint16_t ppo;
	uint32_t map;
	uint16_t uprwc;
	uint32_t gpio_use1;
	uint32_t gpio_use2;
	uint32_t gpio_dir2;
	uint32_t gpio_lvl2;
	struct b06wf_ehci_snapshot ehci[B06WF_EHCI_COUNT];
	struct b06wf_uhci_snapshot uhci[B06WF_UHCI_COUNT];
};

struct b06wf_lab_state {
	struct b06wf_snapshot baseline;
	bool baseline_valid;
	bool gpio_high_armed;
	bool gpio_low_armed;
	bool gpio57_armed;
	bool owner_armed;
	bool oc_ack_armed;
	bool mutation_active;
	bool gpio57_persistent;
	bool fault_latched;
	bool rollback_failed;
	bool nonreversible_dirty;
};

static const struct b06wf_controller_desc b06wf_ehci_desc[] = {
	{ PCI_DEV(0, 0x1a, 7), 0x3a3c8086u, "EHCI2" },
	{ PCI_DEV(0, 0x1d, 7), 0x3a3a8086u, "EHCI1" },
};

static const struct b06wf_controller_desc b06wf_uhci_desc[] = {
	{ PCI_DEV(0, 0x1a, 0), 0x3a378086u, "UHCI4" },
	{ PCI_DEV(0, 0x1a, 1), 0x3a388086u, "UHCI5" },
	{ PCI_DEV(0, 0x1a, 2), 0x3a398086u, "UHCI6" },
	{ PCI_DEV(0, 0x1d, 0), 0x3a348086u, "UHCI1" },
	{ PCI_DEV(0, 0x1d, 1), 0x3a358086u, "UHCI2" },
	{ PCI_DEV(0, 0x1d, 2), 0x3a368086u, "UHCI3" },
};

static struct b06wf_lab_state lab;

_Static_assert(ARRAY_SIZE(b06wf_ehci_desc) == B06WF_EHCI_COUNT,
	"B06WF must cover both ICH10R EHCI controllers");
_Static_assert(ARRAY_SIZE(b06wf_uhci_desc) == B06WF_UHCI_COUNT,
	"B06WF must cover all six ICH10R UHCI controllers");
_Static_assert(B06WF_EHCI_COUNT * B06WF_EHCI_PORT_COUNT == B06WF_TOTAL_PORTS,
	"B06WF EHCI bitmap must cover twelve ports");
_Static_assert(B06WF_UHCI_COUNT * B06WF_UHCI_PORT_COUNT == B06WF_TOTAL_PORTS,
	"B06WF UHCI bitmap must cover twelve ports");
_Static_assert(B06WF_GPIO56 == ((uint32_t)B06WF_GPIO56_BYTE << 24),
	"GPIO56 byte and dword masks must agree");
_Static_assert(B06WF_GPIO57 == ((uint32_t)B06WF_GPIO57_BYTE << 24),
	"GPIO57 byte and dword masks must agree");

static void b06wf_fence(void)
{
	asm volatile ("mfence" ::: "memory");
}

static void b06wf_fail(const char *reason)
{
	lab.fault_latched = true;
	post_code(POST_B06WF_FAIL);
	printk(BIOS_ERR, "[USB-LAB] %s FAIL reason=%s\n", B06WF_STAGE_ID, reason);
}

static bool b06wf_gpio2_msb_write_target(uint16_t register_offset,
	uint8_t mask, bool set)
{
	const uint16_t port = DEFAULT_GPIOBASE + register_offset +
		B06WF_GPIO_BANK2_MSB;
	const uint8_t before = inb(port);
	const uint8_t target = set ? before | mask : before & ~mask;

	outb(target, port);
	b06wf_fence();
	return !!(inb(port) & mask) == set;
}

static bool b06wf_global_snapshot(struct b06wf_snapshot *snapshot)
{
	const uint32_t gpio_dir2_expected = lab.gpio57_persistent ?
		(B06WF_GPIO_DIR2_EXPECTED & ~B06WF_GPIO57) :
		B06WF_GPIO_DIR2_EXPECTED;
	const uint32_t gpio57_level_expected = lab.gpio57_persistent ?
		B06WF_GPIO57 : (B06WF_GPIO_LVL2_EXPECTED & B06WF_GPIO57);

	snapshot->lpc_id = pci_io_read_config32(B06WF_LPC_DEV, PCI_VENDOR_ID);
	snapshot->rcba = pci_io_read_config32(B06WF_LPC_DEV, RCBA);
	snapshot->pmbase = pci_io_read_config32(B06WF_LPC_DEV, B06WF_PMBASE_REG);
	snapshot->acpi_cntl = pci_io_read_config8(B06WF_LPC_DEV, ACPI_CNTL);
	snapshot->gpiobase = pci_io_read_config32(B06WF_LPC_DEV, GPIOBASE);
	snapshot->gpio_cntl = pci_io_read_config8(B06WF_LPC_DEV, D31F0_GPIO_CNTL);

	printk(BIOS_NOTICE,
	       "[USB-LAB] DECODE LPC=%08x RCBA=%08x PM=%08x/%02x GPIO=%08x/%02x\n",
	       snapshot->lpc_id, snapshot->rcba, snapshot->pmbase,
	       snapshot->acpi_cntl, snapshot->gpiobase, snapshot->gpio_cntl);
	if (snapshot->lpc_id != B06WF_LPC_ID ||
	    snapshot->rcba != (CONFIG_FIXED_RCBA_MMIO_BASE | 1u) ||
	    snapshot->pmbase != B06WF_PMBASE_EXPECTED ||
	    snapshot->acpi_cntl != B06WF_ACPI_CNTL_EXPECTED ||
	    snapshot->gpiobase != B06WF_GPIOBASE_EXPECTED ||
	    snapshot->gpio_cntl != B06WF_GPIO_CNTL_EXPECTED)
		return false;

	/* Do not touch decoded RCBA/PM/GPIO ranges until all bases pass exactly. */
	snapshot->fd = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_FD);
	snapshot->cg = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_CG);
	snapshot->ppo = read16p(CONFIG_FIXED_RCBA_MMIO_BASE + B06WF_RCBA_PPO);
	snapshot->map = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_MAP);
	snapshot->uprwc = inw(DEFAULT_PMBASE + B06WF_PMBASE_UPRWC);
	snapshot->gpio_use1 = inl(DEFAULT_GPIOBASE + GP_IO_USE_SEL);
	snapshot->gpio_use2 = inl(DEFAULT_GPIOBASE + GP_IO_USE_SEL2);
	snapshot->gpio_dir2 = inl(DEFAULT_GPIOBASE + GP_IO_SEL2);
	snapshot->gpio_lvl2 = inl(DEFAULT_GPIOBASE + GP_LVL2);
	printk(BIOS_NOTICE,
	       "[USB-LAB] GLOBAL FD=%08x CG=%08x PPO=%04x MAP=%08x UPRWC=%04x "
	       "GPIO USE=%08x/%08x DIR2=%08x LVL2=%08x GPIO56=%s/%s "
	       "GPIO57=%s/%s PERSIST=%u\n",
	       snapshot->fd, snapshot->cg, snapshot->ppo, snapshot->map,
	       snapshot->uprwc, snapshot->gpio_use1, snapshot->gpio_use2,
	       snapshot->gpio_dir2, snapshot->gpio_lvl2,
	       (snapshot->gpio_dir2 & B06WF_GPIO56) ? "input" : "output",
	       (snapshot->gpio_lvl2 & B06WF_GPIO56) ? "high" : "low",
	       (snapshot->gpio_dir2 & B06WF_GPIO57) ? "input" : "output",
	       (snapshot->gpio_lvl2 & B06WF_GPIO57) ? "high" : "low",
	       lab.gpio57_persistent);

	return snapshot->fd == B06WF_FD_EXPECTED &&
		snapshot->cg == B06WF_CG_EXPECTED &&
		snapshot->ppo == B06WF_PPO_EXPECTED &&
		snapshot->map == B06WF_MAP_EXPECTED &&
		snapshot->uprwc == B06WF_UPRWC_EXPECTED &&
		snapshot->gpio_use1 == B06WF_GPIO_USE1_EXPECTED &&
		snapshot->gpio_use2 == B06WF_GPIO_USE2_EXPECTED &&
		snapshot->gpio_dir2 == gpio_dir2_expected &&
		(snapshot->gpio_lvl2 & B06WF_GPIO_LVL2_STABLE_MASK) ==
		(B06WF_GPIO_LVL2_EXPECTED & B06WF_GPIO_LVL2_STABLE_MASK) &&
		(snapshot->gpio_lvl2 & B06WF_GPIO57) == gpio57_level_expected;
}

static bool b06wf_ehci_snapshot(size_t index, struct b06wf_ehci_snapshot *snapshot)
{
	const struct b06wf_controller_desc *desc = &b06wf_ehci_desc[index];
	const uint32_t id = pci_io_read_config32(desc->dev, PCI_VENDOR_ID);
	const uint32_t class_rev = pci_io_read_config32(desc->dev, PCI_CLASS_REVISION);
	const uint8_t header = pci_io_read_config8(desc->dev, PCI_HEADER_TYPE);
	uint32_t base;

	snapshot->command = pci_io_read_config16(desc->dev, PCI_COMMAND);
	snapshot->bar = pci_io_read_config32(desc->dev, PCI_BASE_ADDRESS_0);
	snapshot->pmcsr = pci_io_read_config16(desc->dev, B06WF_EHCI_PMCSR);
	snapshot->ehciir2 = pci_io_read_config32(desc->dev, I82801JX_EHCI_FCREG);
	snapshot->legacy = pci_io_read_config32(desc->dev, B06WF_EHCI_LEGACY);
	snapshot->legacy_ctl = pci_io_read_config32(desc->dev, B06WF_EHCI_LEGACY_CTL);
	snapshot->legacy_ext = pci_io_read_config32(desc->dev, B06WF_EHCI_LEGACY_EXT);
	snapshot->cfg84 = pci_io_read_config32(desc->dev, B06WF_EHCI_CFG84);
	printk(BIOS_NOTICE,
	       "[USB-LAB] %s ID=%08x CLASS=%06x HDR=%02x CMD=%04x BAR=%08x "
	       "PMCSR=%04x IR2=%08x LEG=%08x/%08x/%08x CFG84=%08x\n",
	       desc->name, id, class_rev >> 8, header, snapshot->command,
	       snapshot->bar, snapshot->pmcsr, snapshot->ehciir2,
	       snapshot->legacy, snapshot->legacy_ctl, snapshot->legacy_ext,
	       snapshot->cfg84);
	if (id != desc->id || (class_rev >> 8) != B06WF_EHCI_CLASS ||
	    (header & 0x7fu) != PCI_HEADER_TYPE_NORMAL ||
	    snapshot->command != PCI_COMMAND_MEMORY || snapshot->pmcsr != 0 ||
	    (snapshot->bar & PCI_BASE_ADDRESS_SPACE_IO) ||
	    snapshot->ehciir2 != B06WF_EHCIIR2_EXPECTED ||
	    snapshot->legacy != 0x00000001u ||
	    snapshot->legacy_ctl != B06WF_EHCI_LEGACY_CTL_EXPECTED ||
	    snapshot->legacy_ext != 0 || snapshot->cfg84 != 1)
		return false;
	base = snapshot->bar & ~PCI_BASE_ADDRESS_MEM_ATTR_MASK;
	if (base < B06WF_PCI_MMIO_BASE ||
	    base > B06WF_PCI_MMIO_TOP - B06WF_EHCI_BAR_SIZE ||
	    read8p(base) != B06WF_EHCI_CAPLEN ||
	    read16p(base + 2u) != B06WF_EHCI_HCIVER ||
	    read32p(base + 4u) != B06WF_EHCI_HCSPARAMS)
		return false;

	snapshot->op = base + B06WF_EHCI_CAPLEN;
	snapshot->usbcmd = read32p(snapshot->op + B06WF_EHCI_USBCMD);
	snapshot->usbsts = read32p(snapshot->op + B06WF_EHCI_USBSTS);
	snapshot->usbintr = read32p(snapshot->op + B06WF_EHCI_USBINTR);
	snapshot->configflag = read32p(snapshot->op + B06WF_EHCI_CONFIGFLAG);
	for (size_t port = 0; port < B06WF_EHCI_PORT_COUNT; port++)
		snapshot->portsc[port] = read32p(snapshot->op + B06WF_EHCI_PORTSC +
			4u * port);
	printk(BIOS_NOTICE,
	       "[USB-LAB] %s USBCMD=%08x USBSTS=%08x USBINTR=%08x CF=%08x "
	       "PORTS=%08x/%08x/%08x/%08x/%08x/%08x\n",
	       desc->name, snapshot->usbcmd, snapshot->usbsts,
	       snapshot->usbintr, snapshot->configflag,
	       snapshot->portsc[0], snapshot->portsc[1], snapshot->portsc[2],
	       snapshot->portsc[3], snapshot->portsc[4], snapshot->portsc[5]);

	return snapshot->usbcmd == B06WF_EHCI_USBCMD_EXPECTED &&
		snapshot->usbsts == B06WF_EHCI_USBSTS_EXPECTED &&
		snapshot->usbintr == 0 && snapshot->configflag == 0;
}

static bool b06wf_uhci_snapshot(size_t index, struct b06wf_uhci_snapshot *snapshot)
{
	const struct b06wf_controller_desc *desc = &b06wf_uhci_desc[index];
	const uint32_t id = pci_io_read_config32(desc->dev, PCI_VENDOR_ID);
	const uint32_t class_rev = pci_io_read_config32(desc->dev, PCI_CLASS_REVISION);
	const uint8_t header = pci_io_read_config8(desc->dev, PCI_HEADER_TYPE);
	uint32_t base;

	snapshot->command = pci_io_read_config16(desc->dev, PCI_COMMAND);
	snapshot->bar = pci_io_read_config32(desc->dev, PCI_BASE_ADDRESS_4);
	snapshot->legkey = pci_io_read_config16(desc->dev, B06WF_UHCI_LEGKEY);
	snapshot->cfg_c8 = pci_io_read_config16(desc->dev, B06WF_UHCI_CFG_C8);
	snapshot->cfg_ca = pci_io_read_config16(desc->dev, B06WF_UHCI_CFG_CA);
	printk(BIOS_NOTICE,
	       "[USB-LAB] %s ID=%08x CLASS=%06x HDR=%02x CMD=%04x BAR4=%08x "
	       "LEGKEY=%04x C8/CA=%04x/%04x\n",
	       desc->name, id, class_rev >> 8, header, snapshot->command,
	       snapshot->bar, snapshot->legkey, snapshot->cfg_c8,
	       snapshot->cfg_ca);
	if (id != desc->id || (class_rev >> 8) != B06WF_UHCI_CLASS ||
	    (header & 0x7fu) != PCI_HEADER_TYPE_NORMAL ||
	    snapshot->command != PCI_COMMAND_IO ||
	    !(snapshot->bar & PCI_BASE_ADDRESS_SPACE_IO) ||
	    snapshot->legkey != 0x2f00u || snapshot->cfg_c8 != 0 ||
	    snapshot->cfg_ca != 0)
		return false;
	base = snapshot->bar & ~PCI_BASE_ADDRESS_IO_ATTR_MASK;
	if (base < B06WF_PCI_IO_BASE ||
	    base > B06WF_PCI_IO_TOP - B06WF_UHCI_BAR_SIZE)
		return false;
	snapshot->base = base;
	snapshot->usbcmd = inw(base + B06WF_UHCI_USBCMD);
	snapshot->usbsts = inw(base + B06WF_UHCI_USBSTS);
	snapshot->usbintr = inw(base + B06WF_UHCI_USBINTR);
	for (size_t port = 0; port < B06WF_UHCI_PORT_COUNT; port++)
		snapshot->portsc[port] = inw(base + B06WF_UHCI_PORTSC1 + 2u * port);
	printk(BIOS_NOTICE,
	       "[USB-LAB] %s USBCMD=%04x USBSTS=%04x USBINTR=%04x PORTS=%04x/%04x\n",
	       desc->name, snapshot->usbcmd, snapshot->usbsts,
	       snapshot->usbintr, snapshot->portsc[0], snapshot->portsc[1]);
	return snapshot->usbcmd == 0 && snapshot->usbsts == 0x0020u &&
		snapshot->usbintr == 0;
}

static void b06wf_log_summary(const char *phase, const struct b06wf_snapshot *snapshot)
{
	uint16_t ehci_ccs = 0;
	uint16_t ehci_pe = 0;
	uint16_t ehci_oca = 0;
	uint16_t ehci_occ = 0;
	uint16_t ehci_owner = 0;
	uint16_t uhci_ccs = 0;
	uint16_t uhci_pe = 0;
	uint16_t uhci_lsda = 0;
	uint16_t uhci_oca = 0;
	uint16_t uhci_oci = 0;

	for (size_t controller = 0; controller < B06WF_EHCI_COUNT; controller++) {
		for (size_t port = 0; port < B06WF_EHCI_PORT_COUNT; port++) {
			const unsigned int bit = controller * B06WF_EHCI_PORT_COUNT + port;
			const uint32_t portsc = snapshot->ehci[controller].portsc[port];

			ehci_ccs |= !!(portsc & B06WF_EHCI_PORT_CCS) << bit;
			ehci_pe |= !!(portsc & B06WF_EHCI_PORT_PE) << bit;
			ehci_oca |= !!(portsc & B06WF_EHCI_PORT_OCA) << bit;
			ehci_occ |= !!(portsc & B06WF_EHCI_PORT_OCC) << bit;
			ehci_owner |= !!(portsc & B06WF_EHCI_PORT_OWNER) << bit;
		}
	}
	for (size_t controller = 0; controller < B06WF_UHCI_COUNT; controller++) {
		for (size_t port = 0; port < B06WF_UHCI_PORT_COUNT; port++) {
			const unsigned int bit = controller * B06WF_UHCI_PORT_COUNT + port;
			const uint16_t portsc = snapshot->uhci[controller].portsc[port];

			uhci_ccs |= !!(portsc & B06WF_UHCI_PORT_CCS) << bit;
			uhci_pe |= !!(portsc & B06WF_UHCI_PORT_PE) << bit;
			uhci_lsda |= !!(portsc & B06WF_UHCI_PORT_LSDA) << bit;
			uhci_oca |= !!(portsc & B06WF_UHCI_PORT_OCA) << bit;
			uhci_oci |= !!(portsc & B06WF_UHCI_PORT_OCI) << bit;
		}
	}
	printk(BIOS_NOTICE,
	       "[USB-LAB] %s EHCI CCS=%03x PE=%03x OCA=%03x OCC=%03x OWNER=%03x "
	       "UHCI CCS=%03x PE=%03x LSDA=%03x OCA=%03x OCI=%03x\n",
	       phase, ehci_ccs, ehci_pe, ehci_oca, ehci_occ, ehci_owner,
	       uhci_ccs, uhci_pe, uhci_lsda, uhci_oca, uhci_oci);
}

static bool b06wf_collect(struct b06wf_snapshot *snapshot, const char *phase)
{
	bool valid;

	memset(snapshot, 0, sizeof(*snapshot));
	valid = b06wf_global_snapshot(snapshot);
	if (!valid) {
		b06wf_fail("global-preflight");
		return false;
	}
	for (size_t i = 0; i < B06WF_EHCI_COUNT; i++)
		valid &= b06wf_ehci_snapshot(i, &snapshot->ehci[i]);
	for (size_t i = 0; i < B06WF_UHCI_COUNT; i++)
		valid &= b06wf_uhci_snapshot(i, &snapshot->uhci[i]);
	b06wf_log_summary(phase, snapshot);
	if (!valid)
		b06wf_fail("controller-preflight");
	return valid;
}

static bool b06wf_ports_are_exact_oc_reset(const struct b06wf_snapshot *snapshot)
{
	for (size_t i = 0; i < B06WF_EHCI_COUNT; i++)
		for (size_t port = 0; port < B06WF_EHCI_PORT_COUNT; port++)
			if (snapshot->ehci[i].portsc[port] != B06WF_EHCI_PORT_EXPECTED)
				return false;
	for (size_t i = 0; i < B06WF_UHCI_COUNT; i++)
		for (size_t port = 0; port < B06WF_UHCI_PORT_COUNT; port++)
			if (snapshot->uhci[i].portsc[port] != B06WF_UHCI_PORT_EXPECTED)
				return false;
	return true;
}

static bool b06wf_controls_equal(const struct b06wf_snapshot *left,
	const struct b06wf_snapshot *right)
{
	uint32_t expected_dir2 = right->gpio_dir2;
	uint32_t expected_lvl2 = right->gpio_lvl2;
	const uint32_t level_mask = B06WF_GPIO_LVL2_STABLE_MASK | B06WF_GPIO57;

	if (lab.gpio57_persistent) {
		expected_dir2 &= ~B06WF_GPIO57;
		expected_lvl2 |= B06WF_GPIO57;
	}
	if (left->gpio_use1 != right->gpio_use1 ||
	    left->gpio_use2 != right->gpio_use2 ||
	    left->gpio_dir2 != expected_dir2 ||
	    ((left->gpio_lvl2 ^ expected_lvl2) & level_mask))
		return false;
	for (size_t i = 0; i < B06WF_EHCI_COUNT; i++)
		if (left->ehci[i].configflag != right->ehci[i].configflag)
			return false;
	return true;
}

static void b06wf_sample_ports(const char *phase,
	const struct b06wf_snapshot *addresses)
{
	struct b06wf_snapshot sample = *addresses;

	for (size_t i = 0; i < B06WF_EHCI_COUNT; i++) {
		sample.ehci[i].configflag = read32p(addresses->ehci[i].op +
			B06WF_EHCI_CONFIGFLAG);
		for (size_t port = 0; port < B06WF_EHCI_PORT_COUNT; port++)
			sample.ehci[i].portsc[port] = read32p(addresses->ehci[i].op +
				B06WF_EHCI_PORTSC + 4u * port);
	}
	for (size_t i = 0; i < B06WF_UHCI_COUNT; i++)
		for (size_t port = 0; port < B06WF_UHCI_PORT_COUNT; port++)
			sample.uhci[i].portsc[port] = inw(addresses->uhci[i].base +
				B06WF_UHCI_PORTSC1 + 2u * port);
	printk(BIOS_NOTICE,
	       "[USB-LAB] %s GPIO DIR2=%08x LVL2=%08x CF=%08x/%08x\n",
	       phase, inl(DEFAULT_GPIOBASE + GP_IO_SEL2),
	       inl(DEFAULT_GPIOBASE + GP_LVL2), sample.ehci[0].configflag,
	       sample.ehci[1].configflag);
	b06wf_log_summary(phase, &sample);
}

static bool b06wf_all_usb_oca_clear(const struct b06wf_snapshot *addresses)
{
	for (size_t i = 0; i < B06WF_EHCI_COUNT; i++)
		for (size_t port = 0; port < B06WF_EHCI_PORT_COUNT; port++)
			if (read32p(addresses->ehci[i].op + B06WF_EHCI_PORTSC +
			    4u * port) & B06WF_EHCI_PORT_OCA)
				return false;
	for (size_t i = 0; i < B06WF_UHCI_COUNT; i++)
		for (size_t port = 0; port < B06WF_UHCI_PORT_COUNT; port++)
			if (inw(addresses->uhci[i].base + B06WF_UHCI_PORTSC1 +
			    2u * port) & B06WF_UHCI_PORT_OCA)
				return false;
	return true;
}

static bool b06wf_restore_gpio(const struct b06wf_snapshot *before,
	bool level_was_written)
{
	bool direction_exact;
	bool exact;
	bool level_exact = true;

	/* Restore the high latch before returning GPIO56 to input mode. */
	if (level_was_written)
		level_exact = b06wf_gpio2_msb_write_target(GP_LVL2,
			B06WF_GPIO56_BYTE, !!(before->gpio_lvl2 & B06WF_GPIO56));
	/* Attempt input restoration even when level readback was not exact. */
	direction_exact = b06wf_gpio2_msb_write_target(GP_IO_SEL2,
		B06WF_GPIO56_BYTE, !!(before->gpio_dir2 & B06WF_GPIO56));
	if (!level_exact || !direction_exact)
		goto failed;
	exact = inl(DEFAULT_GPIOBASE + GP_IO_USE_SEL) == before->gpio_use1 &&
		inl(DEFAULT_GPIOBASE + GP_IO_USE_SEL2) == before->gpio_use2 &&
		inl(DEFAULT_GPIOBASE + GP_IO_SEL2) == before->gpio_dir2 &&
		!((inl(DEFAULT_GPIOBASE + GP_LVL2) ^ before->gpio_lvl2) &
		  (B06WF_GPIO_LVL2_STABLE_MASK | B06WF_GPIO56));
	if (exact) {
		lab.mutation_active = false;
		post_code(POST_B06WF_ROLLBACK);
		printk(BIOS_NOTICE,
		       "[USB-LAB] GPIO56 ROLLBACK exact USE=%08x/%08x DIR2=%08x LVL2=%08x\n",
		       before->gpio_use1, before->gpio_use2, before->gpio_dir2,
		       before->gpio_lvl2);
		return true;
	}

failed:
	lab.rollback_failed = true;
	b06wf_fail("GPIO56-rollback");
	return false;
}

static void b06wf_gpio56_test(bool drive_high)
{
	struct b06wf_snapshot before;
	bool level_written = false;

	if (!(drive_high ? lab.gpio_high_armed : lab.gpio_low_armed)) {
		printk(BIOS_ERR, "[USB-LAB] GPIO56-%s locked; use: unlock GPIO56-%s\n",
		       drive_high ? "HIGH" : "LOW", drive_high ? "HIGH" : "LOW");
		return;
	}
	lab.gpio_high_armed = false;
	lab.gpio_low_armed = false;
	if (!lab.baseline_valid || !b06wf_collect(&before, "GPIO56-PRE") ||
	    !b06wf_ports_are_exact_oc_reset(&before) ||
	    !(before.gpio_dir2 & B06WF_GPIO56) ||
	    !(before.gpio_lvl2 & B06WF_GPIO56)) {
		b06wf_fail("GPIO56-exact-prestate");
		return;
	}

	printk(BIOS_WARNING,
	       "[USB-LAB] GPIO56-%s BEGIN writes only bank2-MSB bit0 in "
	       "GP_IO_SEL2%s; auto-rollback required\n",
	       drive_high ? "HIGH" : "LOW",
	       drive_high ? "" : " and GP_LVL2");
	console_tx_flush();
	post_code(POST_B06WF_MUTATION);
	lab.mutation_active = true;
	if (!b06wf_gpio2_msb_write_target(GP_IO_SEL2, B06WF_GPIO56_BYTE, false) ||
	    !(inb(DEFAULT_GPIOBASE + GP_LVL2 + B06WF_GPIO_BANK2_MSB) &
	      B06WF_GPIO56_BYTE)) {
		b06wf_fail("GPIO56-output-high-readback");
		(void)b06wf_restore_gpio(&before, false);
		return;
	}
	if (!drive_high) {
		level_written = true;
		if (!b06wf_gpio2_msb_write_target(GP_LVL2,
			B06WF_GPIO56_BYTE, false)) {
			b06wf_fail("GPIO56-low-readback");
			(void)b06wf_restore_gpio(&before, true);
			return;
		}
	}

	b06wf_sample_ports("GPIO56-T+0MS", &before);
	udelay(10000);
	b06wf_sample_ports("GPIO56-T+10MS", &before);
	udelay(90000);
	b06wf_sample_ports("GPIO56-T+100MS", &before);
	udelay(400000);
	b06wf_sample_ports("GPIO56-T+500MS", &before);
	if (!b06wf_restore_gpio(&before, level_written))
		return;
	b06wf_sample_ports("GPIO56-RESTORED", &before);
}

static bool b06wf_restore_gpio57(const struct b06wf_snapshot *before)
{
	bool direction_exact;
	bool level_exact;

	/* Return the target latch low before putting H_PWRGD back in input mode. */
	level_exact = b06wf_gpio2_msb_write_target(GP_LVL2,
		B06WF_GPIO57_BYTE, !!(before->gpio_lvl2 & B06WF_GPIO57));
	/* Attempt input restoration even when level readback was not exact. */
	direction_exact = b06wf_gpio2_msb_write_target(GP_IO_SEL2,
		B06WF_GPIO57_BYTE, !!(before->gpio_dir2 & B06WF_GPIO57));
	if (!level_exact || !direction_exact) {
		lab.rollback_failed = true;
		b06wf_fail("GPIO57-rollback");
		return false;
	}
	lab.mutation_active = false;
	post_code(POST_B06WF_ROLLBACK);
	printk(BIOS_NOTICE,
	       "[USB-LAB] GPIO57 ROLLBACK target-bit exact DIR=%u LVL=%u; "
	       "unrelated native inputs ignored\n",
	       !!(before->gpio_dir2 & B06WF_GPIO57),
	       !!(before->gpio_lvl2 & B06WF_GPIO57));
	return true;
}

static void b06wf_gpio57_pwrgd_test(void)
{
	struct b06wf_snapshot before;

	if (!lab.gpio57_armed) {
		printk(BIOS_ERR,
		       "[USB-LAB] GPIO57-PWRGD locked; use: unlock GPIO57-PWRGD\n");
		return;
	}
	lab.gpio57_armed = false;
	if (!lab.baseline_valid || lab.gpio57_persistent ||
	    !b06wf_collect(&before, "GPIO57-PWRGD-PRE") ||
	    !b06wf_ports_are_exact_oc_reset(&before) ||
	    !(before.gpio_dir2 & B06WF_GPIO57) ||
	    (before.gpio_lvl2 & B06WF_GPIO57)) {
		b06wf_fail("GPIO57-PWRGD-exact-prestate");
		return;
	}

	printk(BIOS_WARNING,
	       "[USB-LAB] GPIO57-PWRGD BEGIN vendor-correlated order: bank2-MSB "
	       "bit1 output-low -> settle %u calibrated us -> high; live XRS "
	       "delay=%08x uncalibrated PAUSE iterations (different units); "
	       "target remains persistent\n",
	       B06WF_GPIO57_LOW_SETTLE_US,
	       B06WF_GPIO57_XRS_DELAY_PAUSES);
	console_tx_flush();
	post_code(POST_B06WF_MUTATION);
	lab.mutation_active = true;
	if (!b06wf_gpio2_msb_write_target(GP_IO_SEL2,
		B06WF_GPIO57_BYTE, false) ||
	    (inb(DEFAULT_GPIOBASE + GP_LVL2 + B06WF_GPIO_BANK2_MSB) &
	     B06WF_GPIO57_BYTE)) {
		b06wf_fail("GPIO57-output-low-readback");
		(void)b06wf_restore_gpio57(&before);
		return;
	}
	b06wf_sample_ports("GPIO57-LOW-T+0MS", &before);
	udelay(B06WF_GPIO57_LOW_SETTLE_US);
	b06wf_sample_ports("GPIO57-LOW-T+65536US", &before);

	if (!b06wf_gpio2_msb_write_target(GP_LVL2,
		B06WF_GPIO57_BYTE, true)) {
		b06wf_fail("GPIO57-high-readback");
		(void)b06wf_restore_gpio57(&before);
		return;
	}
	b06wf_sample_ports("GPIO57-HIGH-T+0MS", &before);
	udelay(10000);
	b06wf_sample_ports("GPIO57-HIGH-T+10MS", &before);
	udelay(90000);
	b06wf_sample_ports("GPIO57-HIGH-T+100MS", &before);
	udelay(400000);
	b06wf_sample_ports("GPIO57-HIGH-T+500MS", &before);
	if ((inb(DEFAULT_GPIOBASE + GP_IO_SEL2 + B06WF_GPIO_BANK2_MSB) &
	     B06WF_GPIO57_BYTE) ||
	    !(inb(DEFAULT_GPIOBASE + GP_LVL2 + B06WF_GPIO_BANK2_MSB) &
	      B06WF_GPIO57_BYTE)) {
		b06wf_fail("GPIO57-persistent-readback");
		(void)b06wf_restore_gpio57(&before);
		return;
	}
	if (!b06wf_all_usb_oca_clear(&before)) {
		b06wf_fail("GPIO57-OCA-still-active");
		(void)b06wf_restore_gpio57(&before);
		return;
	}

	lab.mutation_active = false;
	lab.gpio57_persistent = true;
	post_code(POST_B06WF_PWRGD_READY);
	printk(BIOS_NOTICE,
	       "[USB-LAB] GPIO57-PWRGD PERSISTENT target-bit exact and all EHCI/"
	       "UHCI OCA bits clear; CCS/LSDA and unrelated/native GP_LVL2 inputs "
	       "were allowed to vary; "
	       "continue is permitted after its full gate\n");
}

static bool b06wf_restore_configflags(const struct b06wf_snapshot *before)
{
	bool exact = true;

	for (size_t remaining = B06WF_EHCI_COUNT; remaining > 0; remaining--) {
		const size_t i = remaining - 1;

		write32p(before->ehci[i].op + B06WF_EHCI_CONFIGFLAG,
			before->ehci[i].configflag);
	}
	b06wf_fence();
	for (size_t i = 0; i < B06WF_EHCI_COUNT; i++)
		exact &= read32p(before->ehci[i].op + B06WF_EHCI_CONFIGFLAG) ==
			before->ehci[i].configflag;
	if (exact) {
		lab.mutation_active = false;
		post_code(POST_B06WF_ROLLBACK);
		printk(BIOS_NOTICE,
		       "[USB-LAB] CONFIGFLAG ROLLBACK exact CF=%08x/%08x\n",
		       before->ehci[0].configflag, before->ehci[1].configflag);
		return true;
	}
	lab.rollback_failed = true;
	b06wf_fail("CONFIGFLAG-rollback");
	return false;
}

static void b06wf_owner_test(void)
{
	struct b06wf_snapshot before;

	if (!lab.owner_armed) {
		printk(BIOS_ERR,
		       "[USB-LAB] ownership test locked; use: unlock USB-OWNER\n");
		return;
	}
	lab.owner_armed = false;
	if (!lab.baseline_valid || !b06wf_collect(&before, "OWNER-PRE") ||
	    !b06wf_ports_are_exact_oc_reset(&before)) {
		b06wf_fail("owner-exact-prestate");
		return;
	}
	printk(BIOS_WARNING,
	       "[USB-LAB] OWNER BEGIN writes only EHCI CONFIGFLAG 0->1->0; no PORTSC write\n");
	console_tx_flush();
	post_code(POST_B06WF_MUTATION);
	lab.mutation_active = true;
	for (size_t i = 0; i < B06WF_EHCI_COUNT; i++) {
		write32p(before.ehci[i].op + B06WF_EHCI_CONFIGFLAG, 1);
		b06wf_fence();
		if (read32p(before.ehci[i].op + B06WF_EHCI_CONFIGFLAG) != 1) {
			b06wf_fail("CONFIGFLAG-one-readback");
			(void)b06wf_restore_configflags(&before);
			return;
		}
		b06wf_sample_ports(i ? "OWNER-EHCI1-T+0MS" :
			"OWNER-EHCI2-T+0MS", &before);
		udelay(20000);
		b06wf_sample_ports(i ? "OWNER-EHCI1-T+20MS" :
			"OWNER-EHCI2-T+20MS", &before);
		write32p(before.ehci[i].op + B06WF_EHCI_CONFIGFLAG,
			before.ehci[i].configflag);
		b06wf_fence();
		if (read32p(before.ehci[i].op + B06WF_EHCI_CONFIGFLAG) !=
		    before.ehci[i].configflag) {
			b06wf_fail("CONFIGFLAG-zero-readback");
			(void)b06wf_restore_configflags(&before);
			return;
		}
	}
	if (!b06wf_restore_configflags(&before))
		return;
	b06wf_sample_ports("OWNER-RESTORED", &before);
}

static void b06wf_oc_ack_test(void)
{
	struct b06wf_snapshot before;
	bool exact = true;

	if (!lab.oc_ack_armed) {
		printk(BIOS_ERR,
		       "[USB-LAB] OC latch ack locked; use: unlock USB-OCACK\n");
		return;
	}
	lab.oc_ack_armed = false;
	if (!lab.baseline_valid || !b06wf_collect(&before, "OCACK-PRE") ||
	    !b06wf_ports_are_exact_oc_reset(&before)) {
		b06wf_fail("OCACK-exact-prestate");
		return;
	}
	printk(BIOS_WARNING,
	       "[USB-LAB] OCACK NONREVERSIBLE: acknowledging OCC/OCI history only; "
	       "OCA is read-only; payload continuation will be blocked\n");
	console_tx_flush();
	post_code(POST_B06WF_NONREV);
	lab.nonreversible_dirty = true;
	for (size_t i = 0; i < B06WF_EHCI_COUNT; i++)
		for (size_t port = 0; port < B06WF_EHCI_PORT_COUNT; port++)
			write32p(before.ehci[i].op + B06WF_EHCI_PORTSC + 4u * port,
				B06WF_EHCI_OC_ACK_WRITE);
	for (size_t i = 0; i < B06WF_UHCI_COUNT; i++)
		for (size_t port = 0; port < B06WF_UHCI_PORT_COUNT; port++)
			outw(B06WF_UHCI_OC_ACK_WRITE,
				before.uhci[i].base + B06WF_UHCI_PORTSC1 + 2u * port);
	b06wf_fence();
	for (size_t i = 0; i < B06WF_EHCI_COUNT; i++)
		for (size_t port = 0; port < B06WF_EHCI_PORT_COUNT; port++)
			exact &= read32p(before.ehci[i].op + B06WF_EHCI_PORTSC +
				4u * port) == B06WF_EHCI_OC_ACK_EXPECTED;
	for (size_t i = 0; i < B06WF_UHCI_COUNT; i++)
		for (size_t port = 0; port < B06WF_UHCI_PORT_COUNT; port++)
			exact &= inw(before.uhci[i].base + B06WF_UHCI_PORTSC1 +
				2u * port) == B06WF_UHCI_OC_ACK_EXPECTED;
	b06wf_sample_ports("OCACK-POST", &before);
	if (!exact)
		b06wf_fail("OCACK-readback");
	else
		printk(BIOS_NOTICE,
		       "[USB-LAB] OCACK exact: sticky OCC/OCI cleared, live OCA retained; reset required\n");
}

void b06wf_usb_lab_begin(void)
{
	memset(&lab, 0, sizeof(lab));
	post_code(POST_B06WF_LAB_ENTRY);
	printk(BIOS_NOTICE,
	       "\n[USB-LAB] %s BEGIN LATE=BS_WRITE_TABLES/ON_ENTRY AUTO_WRITES=0\n",
	       B06WF_STAGE_ID);
	lab.baseline_valid = b06wf_collect(&lab.baseline, "BASELINE");
	if (!lab.baseline_valid) {
		b06wf_fail("baseline-invalid");
		printk(BIOS_ERR,
		       "[USB-LAB] mutations and continue locked; read/reset commands remain available\n");
		return;
	}
	post_code(POST_B06WF_LAB_READY);
	printk(BIOS_NOTICE,
	       "[USB-LAB] READY; no automatic mutation; type help or usb status\n");
}

void b06wf_usb_lab_help(void)
{
	printk(BIOS_INFO,
	       "B06WF USB lab commands:\n"
	       "  usb status\n"
	       "  unlock GPIO56-HIGH | usb gpio56 high\n"
	       "  unlock GPIO56-LOW  | usb gpio56 low\n"
	       "  unlock GPIO57-PWRGD | usb gpio57 pwrgd  (persistent to payload)\n"
	       "  unlock USB-OWNER   | usb owner\n"
	       "  unlock USB-OCACK   | usb ocack   (non-reversible; reset required)\n"
	       "  lock | continue | unlock RESET | reset warm|full\n"
	       "No generic write command exists; PORTSC is written only by usb ocack.\n");
}

void b06wf_usb_lab_lock(void)
{
	lab.gpio_high_armed = false;
	lab.gpio_low_armed = false;
	lab.gpio57_armed = false;
	lab.owner_armed = false;
	lab.oc_ack_armed = false;
}

static void b06wf_arm(bool *arm, const char *name)
{
	b06wf_usb_lab_lock();
	if (!lab.baseline_valid || lab.mutation_active || lab.gpio57_persistent ||
	    lab.fault_latched || lab.rollback_failed || lab.nonreversible_dirty) {
		printk(BIOS_ERR,
		       "[USB-LAB] cannot arm %s in current state; reset required\n",
		       name);
		return;
	}
	*arm = true;
	printk(BIOS_WARNING,
	       "[USB-LAB] %s armed for exactly one matching command\n", name);
}

bool b06wf_usb_lab_command(int argc, char **argv)
{
	if (argc == 2 && !strcmp(argv[0], "unlock")) {
		if (!strcmp(argv[1], "GPIO56-HIGH"))
			b06wf_arm(&lab.gpio_high_armed, "GPIO56-HIGH");
		else if (!strcmp(argv[1], "GPIO56-LOW"))
			b06wf_arm(&lab.gpio_low_armed, "GPIO56-LOW");
		else if (!strcmp(argv[1], "GPIO57-PWRGD"))
			b06wf_arm(&lab.gpio57_armed, "GPIO57-PWRGD");
		else if (!strcmp(argv[1], "USB-OWNER"))
			b06wf_arm(&lab.owner_armed, "USB-OWNER");
		else if (!strcmp(argv[1], "USB-OCACK"))
			b06wf_arm(&lab.oc_ack_armed, "USB-OCACK");
		else
			return false;
		return true;
	}
	if (argc == 0 || strcmp(argv[0], "usb"))
		return false;
	if (argc == 2 && !strcmp(argv[1], "status")) {
		struct b06wf_snapshot snapshot;

		(void)b06wf_collect(&snapshot, "STATUS");
		printk(BIOS_INFO,
		       "[USB-LAB] STATE baseline=%u active=%u fault=%u rollback_failed=%u nonrev=%u "
		       "gpio57_persistent=%u arms=%u/%u/%u/%u/%u\n",
		       lab.baseline_valid, lab.mutation_active, lab.fault_latched,
		       lab.rollback_failed, lab.nonreversible_dirty,
		       lab.gpio57_persistent, lab.gpio_high_armed,
		       lab.gpio_low_armed, lab.gpio57_armed, lab.owner_armed,
		       lab.oc_ack_armed);
		return true;
	}
	if (argc == 3 && !strcmp(argv[1], "gpio56") &&
	    (!strcmp(argv[2], "high") || !strcmp(argv[2], "low"))) {
		b06wf_gpio56_test(!strcmp(argv[2], "high"));
		return true;
	}
	if (argc == 3 && !strcmp(argv[1], "gpio57") &&
	    !strcmp(argv[2], "pwrgd")) {
		b06wf_gpio57_pwrgd_test();
		return true;
	}
	if (argc == 2 && !strcmp(argv[1], "owner")) {
		b06wf_owner_test();
		return true;
	}
	if (argc == 2 && !strcmp(argv[1], "ocack")) {
		b06wf_oc_ack_test();
		return true;
	}
	printk(BIOS_ERR, "[USB-LAB] syntax; use help\n");
	return true;
}

bool b06wf_usb_lab_can_continue(void)
{
	struct b06wf_snapshot current;

	b06wf_usb_lab_lock();
	if (!lab.baseline_valid || lab.mutation_active || lab.fault_latched ||
	    lab.rollback_failed || lab.nonreversible_dirty) {
		b06wf_fail("continue-dirty-or-unqualified");
		return false;
	}
	if (!b06wf_collect(&current, "CONTINUE-GATE") ||
	    !b06wf_controls_equal(&current, &lab.baseline) ||
	    (lab.gpio57_persistent && !b06wf_all_usb_oca_clear(&current))) {
		b06wf_fail("continue-control-mismatch");
		return false;
	}
	post_code(POST_B06WF_CONTINUE);
	printk(BIOS_NOTICE,
	       "[USB-LAB] CONTINUE exact approved controls verified; "
	       "GPIO57_PERSIST=%u; entering ACPI/table path\n",
	       lab.gpio57_persistent);
	console_tx_flush();
	return true;
}
