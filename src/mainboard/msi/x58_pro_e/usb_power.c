/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * X58_USB_POWER promotes only the live-proven X58_LATE_USB GPIO57/H_PWRGD path into
 * an automatic policy.  It runs after device initialization and before ACPI
 * tables, then verifies the retained state again after table and
 * chip-finalization work but before the payload executes.
 *
 * No USB controller register is written here.  SeaBIOS remains responsible
 * for EHCI/UHCI reset, ownership, enumeration and HID setup.  Any unexpected
 * prestate, target readback or live over-current indication stops before the
 * payload, after a best-effort low/input rollback of GPIO57.
 */

#include <arch/io.h>
#include <arch/pci_io_cfg.h>
#include <bootstate.h>
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

#include "usb_power.h"
#include "pci.h"
#include "platform_acpi.h"
/* PLATFORM moves only LPC decode addresses after USB release, before tables.
 * Retain all USB/GPIO state gates, reading the currently admitted BARs. */
#undef DEFAULT_PMBASE
#undef DEFAULT_GPIOBASE
#define DEFAULT_PMBASE x58_platform_pmbase()
#define DEFAULT_GPIOBASE x58_platform_gpiobase()

#define POST_X58_USB_POWER_ENTRY	0x98u
#define POST_X58_USB_POWER_PREFLIGHT	0x99u
#define POST_X58_USB_POWER_GPIO_LOW	0x9au
#define POST_X58_USB_POWER_GPIO_HIGH	0x9bu
#define POST_X58_USB_POWER_RELEASED	0x9cu
#define POST_X58_USB_POWER_PAYLOAD_READY	0x9du
#define POST_X58_USB_POWER_FAIL		0x9eu
#define POST_X58_USB_POWER_ROLLBACK	0x9fu

#define X58_USB_POWER_LPC_DEV		PCI_DEV(0, 0x1f, 0)
#define X58_USB_POWER_LPC_ID		0x3a168086u
#define X58_USB_POWER_PMBASE_REG		0x40u
#define X58_USB_POWER_PMBASE_EXPECTED	(DEFAULT_PMBASE | 1u)
#define X58_USB_POWER_ACPI_CNTL_EXPECTED	0x80u
#define X58_USB_POWER_GPIOBASE_EXPECTED	(DEFAULT_GPIOBASE | 1u)
#define X58_USB_POWER_GPIO_CNTL_EXPECTED	0x10u

#define X58_USB_POWER_RCBA_PPO		0x3524u
#define X58_USB_POWER_PMBASE_UPRWC	0x3cu
#define X58_USB_POWER_FD_EXPECTED	0x02000001u
/* i82801jx/lpc.c:enable_clock_gating applied to the admitted zero baseline.
 * Keep an exact full-register gate; USB static clock-disable bit20 is clear. */
#define X58_USB_POWER_CG_EXPECTED	0xbfcf001fu
#define X58_USB_POWER_PPO_EXPECTED	0x0000u
#define X58_USB_POWER_MAP_EXPECTED	0x00000000u
#define X58_USB_POWER_UPRWC_EXPECTED	0x0000u
#define X58_USB_POWER_GPIO_USE1_EXPECTED 0x197e75ffu
#define X58_USB_POWER_GPIO_USE2_EXPECTED 0x030300ffu
#define X58_USB_POWER_GPIO_DIR2_EXPECTED 0x0f55fff0u
#define X58_USB_POWER_GPIO_LVL2_EXPECTED 0x15ff00d3u
#define X58_USB_POWER_GPIO57		BIT(25)
/* Only already-configured output levels and the explicit target are stable. */
#define X58_USB_POWER_GPIO_LVL2_STABLE_MASK 0x0002000fu
#define X58_USB_POWER_GPIO_BANK2_MSB	3u
#define X58_USB_POWER_GPIO57_BYTE	BIT(1)
#define X58_USB_POWER_GPIO57_DIR_NEIGHBORS ((uint8_t)~X58_USB_POWER_GPIO57_BYTE)
#define X58_USB_POWER_GPIO57_LOW_SETTLE_US 0x00010000u

#define X58_USB_POWER_EHCI_COUNT		2u
#define X58_USB_POWER_EHCI_PORT_COUNT	6u
#define X58_USB_POWER_UHCI_COUNT		6u
#define X58_USB_POWER_UHCI_PORT_COUNT	2u
#define X58_USB_POWER_TOTAL_PORTS	12u

#define X58_USB_POWER_EHCI_CLASS		0x0c0320u
#define X58_USB_POWER_UHCI_CLASS		0x0c0300u
#define X58_USB_POWER_EHCI_BAR_SIZE	0x400u
#define X58_USB_POWER_UHCI_BAR_SIZE	0x20u
#define X58_USB_POWER_PCI_IO_BASE	0x1000u
#define X58_USB_POWER_PCI_IO_TOP		0x10000u
#define X58_USB_POWER_PCI_MMIO_BASE	0xc0000000u
#define X58_USB_POWER_PCI_MMIO_TOP	0xe0000000u

#define X58_USB_POWER_EHCI_PMCSR		0x54u
#define X58_USB_POWER_EHCI_LEGACY	0x68u
#define X58_USB_POWER_EHCI_LEGACY_CTL	0x6cu
#define X58_USB_POWER_EHCI_LEGACY_EXT	0x70u
#define X58_USB_POWER_EHCI_CFG84		0x84u
#define X58_USB_POWER_EHCI_LEGACY_CTL_EXPECTED 0xc0040000u
#define X58_USB_POWER_EHCIIR2_EXPECTED	0x2002170au
#define X58_USB_POWER_EHCI_CAPLEN	0x20u
#define X58_USB_POWER_EHCI_HCIVER	0x0100u
#define X58_USB_POWER_EHCI_HCSPARAMS	0x00103206u
#define X58_USB_POWER_EHCI_USBCMD	0x00u
#define X58_USB_POWER_EHCI_USBSTS	0x04u
#define X58_USB_POWER_EHCI_USBINTR	0x08u
#define X58_USB_POWER_EHCI_CONFIGFLAG	0x40u
#define X58_USB_POWER_EHCI_PORTSC	0x44u
#define X58_USB_POWER_EHCI_USBCMD_EXPECTED 0x00080000u
#define X58_USB_POWER_EHCI_USBSTS_EXPECTED 0x00001004u
/* Measured X58_LEGACY_INPUT late reference only; dynamic PORTSC fields are not gated. */
#define X58_USB_POWER_EHCI_PORT_EXPECTED 0x00003030u
#define X58_USB_POWER_EHCI_PORT_CCS	BIT(0)
#define X58_USB_POWER_EHCI_PORT_PE	BIT(2)
#define X58_USB_POWER_EHCI_PORT_OCA	BIT(4)
#define X58_USB_POWER_EHCI_PORT_OCC	BIT(5)
#define X58_USB_POWER_EHCI_PORT_OWNER	BIT(13)
#define X58_PLATFORM_USB_EHCI_ACTIVE_PORT (BIT(2) | BIT(6) | BIT(7) | BIT(8))

#define X58_USB_POWER_UHCI_LEGKEY	0xc0u
#define X58_USB_POWER_UHCI_CFG_C8	0xc8u
#define X58_USB_POWER_UHCI_CFG_CA	0xcau
#define X58_USB_POWER_UHCI_USBCMD	0x00u
#define X58_USB_POWER_UHCI_USBSTS	0x02u
#define X58_USB_POWER_UHCI_USBINTR	0x04u
#define X58_USB_POWER_UHCI_PORTSC1	0x10u
#define X58_USB_POWER_UHCI_PORT_EXPECTED 0x0c80u
#define X58_USB_POWER_UHCI_PORT_CCS	BIT(0)
#define X58_USB_POWER_UHCI_PORT_PE	BIT(2)
#define X58_USB_POWER_UHCI_PORT_LSDA	BIT(8)
#define X58_USB_POWER_UHCI_PORT_OCA	BIT(10)
#define X58_USB_POWER_UHCI_PORT_OCI	BIT(11)
#define X58_PLATFORM_USB_UHCI_ACTIVE_PORT (BIT(2) | BIT(9) | BIT(12))

struct x58_usb_power_controller_desc {
	pci_devfn_t dev;
	uint32_t id;
	const char *name;
};

struct x58_usb_power_ehci_snapshot {
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
	uint32_t portsc[X58_USB_POWER_EHCI_PORT_COUNT];
};

struct x58_usb_power_uhci_snapshot {
	uint16_t command;
	uint16_t legkey;
	uint16_t cfg_c8;
	uint16_t cfg_ca;
	uint32_t bar;
	uint16_t base;
	uint16_t usbcmd;
	uint16_t usbsts;
	uint16_t usbintr;
	uint16_t portsc[X58_USB_POWER_UHCI_PORT_COUNT];
};

struct x58_usb_power_snapshot {
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
	struct x58_usb_power_ehci_snapshot ehci[X58_USB_POWER_EHCI_COUNT];
	struct x58_usb_power_uhci_snapshot uhci[X58_USB_POWER_UHCI_COUNT];
};

static const struct x58_usb_power_controller_desc x58_usb_power_ehci_desc[] = {
	{ PCI_DEV(0, 0x1a, 7), 0x3a3c8086u, "EHCI2" },
	{ PCI_DEV(0, 0x1d, 7), 0x3a3a8086u, "EHCI1" },
};

static const struct x58_usb_power_controller_desc x58_usb_power_uhci_desc[] = {
	{ PCI_DEV(0, 0x1a, 0), 0x3a378086u, "UHCI4" },
	{ PCI_DEV(0, 0x1a, 1), 0x3a388086u, "UHCI5" },
	{ PCI_DEV(0, 0x1a, 2), 0x3a398086u, "UHCI6" },
	{ PCI_DEV(0, 0x1d, 0), 0x3a348086u, "UHCI1" },
	{ PCI_DEV(0, 0x1d, 1), 0x3a358086u, "UHCI2" },
	{ PCI_DEV(0, 0x1d, 2), 0x3a368086u, "UHCI3" },
};

static struct x58_usb_power_snapshot x58_usb_power_baseline;
static bool x58_usb_power_baseline_valid;
static bool x58_usb_power_released;

_Static_assert(ARRAY_SIZE(x58_usb_power_ehci_desc) == X58_USB_POWER_EHCI_COUNT,
	"X58_USB_POWER must cover both ICH10R EHCI controllers");
_Static_assert(ARRAY_SIZE(x58_usb_power_uhci_desc) == X58_USB_POWER_UHCI_COUNT,
	"X58_USB_POWER must cover all six ICH10R UHCI controllers");
_Static_assert(X58_USB_POWER_EHCI_COUNT * X58_USB_POWER_EHCI_PORT_COUNT == X58_USB_POWER_TOTAL_PORTS,
	"X58_USB_POWER EHCI bitmap must cover twelve ports");
_Static_assert(X58_USB_POWER_UHCI_COUNT * X58_USB_POWER_UHCI_PORT_COUNT == X58_USB_POWER_TOTAL_PORTS,
	"X58_USB_POWER UHCI bitmap must cover twelve ports");
_Static_assert(X58_USB_POWER_GPIO57 == ((uint32_t)X58_USB_POWER_GPIO57_BYTE << 24),
	"X58_USB_POWER GPIO57 byte and dword masks must agree");

static void x58_usb_power_fence(void)
{
	asm volatile ("mfence" ::: "memory");
}

static bool x58_usb_power_gpio2_msb_write_target(uint16_t register_offset,
	uint8_t mask, bool set, uint8_t stable_neighbor_mask)
{
	const uint16_t port = DEFAULT_GPIOBASE + register_offset +
		X58_USB_POWER_GPIO_BANK2_MSB;
	const uint8_t before = inb(port);
	const uint8_t target = set ? before | mask : before & ~mask;
	uint8_t after;

	outb(target, port);
	x58_usb_power_fence();
	after = inb(port);
	printk(BIOS_DEBUG,
	       "[USB-AUTO] GPIO REG=%04x BEFORE=%02x WRITE=%02x AFTER=%02x "
	       "TARGET=%02x NEIGHBOR_DELTA=%02x\n",
	       port, before, target, after, mask, (before ^ after) & ~mask);
	return !!(after & mask) == set &&
		!((after ^ before) & stable_neighbor_mask);
}

static bool x58_usb_power_global_snapshot(struct x58_usb_power_snapshot *snapshot,
	bool persistent)
{
	const uint32_t expected_dir2 = persistent ?
		(X58_USB_POWER_GPIO_DIR2_EXPECTED & ~X58_USB_POWER_GPIO57) :
		X58_USB_POWER_GPIO_DIR2_EXPECTED;
	const uint32_t expected_gpio57 = persistent ? X58_USB_POWER_GPIO57 :
		(X58_USB_POWER_GPIO_LVL2_EXPECTED & X58_USB_POWER_GPIO57);

	snapshot->lpc_id = pci_io_read_config32(X58_USB_POWER_LPC_DEV, PCI_VENDOR_ID);
	snapshot->rcba = pci_io_read_config32(X58_USB_POWER_LPC_DEV, RCBA);
	snapshot->pmbase = pci_io_read_config32(X58_USB_POWER_LPC_DEV, X58_USB_POWER_PMBASE_REG);
	snapshot->acpi_cntl = pci_io_read_config8(X58_USB_POWER_LPC_DEV, ACPI_CNTL);
	snapshot->gpiobase = pci_io_read_config32(X58_USB_POWER_LPC_DEV, GPIOBASE);
	snapshot->gpio_cntl = pci_io_read_config8(X58_USB_POWER_LPC_DEV, D31F0_GPIO_CNTL);
	printk(BIOS_DEBUG,
	       "[USB-AUTO] DECODE LPC=%08x RCBA=%08x PM=%08x/%02x "
	       "GPIO=%08x/%02x\n",
	       snapshot->lpc_id, snapshot->rcba, snapshot->pmbase,
	       snapshot->acpi_cntl, snapshot->gpiobase, snapshot->gpio_cntl);
	if (snapshot->lpc_id != X58_USB_POWER_LPC_ID ||
	    snapshot->rcba != (CONFIG_FIXED_RCBA_MMIO_BASE | 1u) ||
	    snapshot->pmbase != X58_USB_POWER_PMBASE_EXPECTED ||
	    snapshot->acpi_cntl != X58_USB_POWER_ACPI_CNTL_EXPECTED ||
	    snapshot->gpiobase != X58_USB_POWER_GPIOBASE_EXPECTED ||
	    snapshot->gpio_cntl != X58_USB_POWER_GPIO_CNTL_EXPECTED)
		return false;

	/* Decoded RCBA/PM/GPIO ranges are touched only after every base passes. */
	snapshot->fd = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_FD);
	snapshot->cg = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_CG);
	snapshot->ppo = read16p(CONFIG_FIXED_RCBA_MMIO_BASE + X58_USB_POWER_RCBA_PPO);
	snapshot->map = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_MAP);
	snapshot->uprwc = inw(DEFAULT_PMBASE + X58_USB_POWER_PMBASE_UPRWC);
	snapshot->gpio_use1 = inl(DEFAULT_GPIOBASE + GP_IO_USE_SEL);
	snapshot->gpio_use2 = inl(DEFAULT_GPIOBASE + GP_IO_USE_SEL2);
	snapshot->gpio_dir2 = inl(DEFAULT_GPIOBASE + GP_IO_SEL2);
	snapshot->gpio_lvl2 = inl(DEFAULT_GPIOBASE + GP_LVL2);
	printk(BIOS_DEBUG,
	       "[USB-AUTO] GLOBAL FD=%08x CG=%08x PPO=%04x MAP=%08x "
	       "UPRWC=%04x GPIO=%08x/%08x DIR2=%08x LVL2=%08x "
	       "GPIO57=%s/%s EXPECT_PERSIST=%u\n",
	       snapshot->fd, snapshot->cg, snapshot->ppo, snapshot->map,
	       snapshot->uprwc, snapshot->gpio_use1, snapshot->gpio_use2,
	       snapshot->gpio_dir2, snapshot->gpio_lvl2,
	       (snapshot->gpio_dir2 & X58_USB_POWER_GPIO57) ? "input" : "output",
	       (snapshot->gpio_lvl2 & X58_USB_POWER_GPIO57) ? "high" : "low",
	       persistent);

	return snapshot->fd == X58_USB_POWER_FD_EXPECTED &&
		snapshot->cg == X58_USB_POWER_CG_EXPECTED &&
		snapshot->ppo == X58_USB_POWER_PPO_EXPECTED &&
		snapshot->map == X58_USB_POWER_MAP_EXPECTED &&
		snapshot->uprwc == X58_USB_POWER_UPRWC_EXPECTED &&
		snapshot->gpio_use1 == X58_USB_POWER_GPIO_USE1_EXPECTED &&
		snapshot->gpio_use2 == X58_USB_POWER_GPIO_USE2_EXPECTED &&
		snapshot->gpio_dir2 == expected_dir2 &&
		(snapshot->gpio_lvl2 & X58_USB_POWER_GPIO_LVL2_STABLE_MASK) ==
		(X58_USB_POWER_GPIO_LVL2_EXPECTED & X58_USB_POWER_GPIO_LVL2_STABLE_MASK) &&
		(snapshot->gpio_lvl2 & X58_USB_POWER_GPIO57) == expected_gpio57;
}

static bool x58_usb_power_ehci_snapshot(size_t index,
	struct x58_usb_power_ehci_snapshot *snapshot, uint16_t expected_command)
{
	/* The standard chip enable callback sets SERR before PCI allocation.
	 * Keep an exact command check, including that bit, for every caller:
	 * pre-release admission, whole-platform capture and payload recheck.
	 */
	expected_command |= PCI_COMMAND_SERR;
	const struct x58_usb_power_controller_desc *desc = &x58_usb_power_ehci_desc[index];
	const uint32_t id = pci_io_read_config32(desc->dev, PCI_VENDOR_ID);
	const uint32_t class_rev = pci_io_read_config32(desc->dev,
		PCI_CLASS_REVISION);
	const uint8_t header = pci_io_read_config8(desc->dev, PCI_HEADER_TYPE);
	uint32_t base;

	snapshot->command = pci_io_read_config16(desc->dev, PCI_COMMAND);
	snapshot->bar = pci_io_read_config32(desc->dev, PCI_BASE_ADDRESS_0);
	snapshot->pmcsr = pci_io_read_config16(desc->dev, X58_USB_POWER_EHCI_PMCSR);
	snapshot->ehciir2 = pci_io_read_config32(desc->dev, I82801JX_EHCI_FCREG);
	snapshot->legacy = pci_io_read_config32(desc->dev, X58_USB_POWER_EHCI_LEGACY);
	snapshot->legacy_ctl = pci_io_read_config32(desc->dev,
		X58_USB_POWER_EHCI_LEGACY_CTL);
	snapshot->legacy_ext = pci_io_read_config32(desc->dev,
		X58_USB_POWER_EHCI_LEGACY_EXT);
	snapshot->cfg84 = pci_io_read_config32(desc->dev, X58_USB_POWER_EHCI_CFG84);
	printk(BIOS_DEBUG,
	       "[USB-AUTO] %s ID=%08x CLASS=%06x HDR=%02x CMD=%04x "
	       "BAR=%08x PMCSR=%04x IR2=%08x LEG=%08x/%08x/%08x "
	       "CFG84=%08x\n",
	       desc->name, id, class_rev >> 8, header, snapshot->command,
	       snapshot->bar, snapshot->pmcsr, snapshot->ehciir2,
	       snapshot->legacy, snapshot->legacy_ctl, snapshot->legacy_ext,
	       snapshot->cfg84);
	if (id != desc->id || (class_rev >> 8) != X58_USB_POWER_EHCI_CLASS ||
	    (header & 0x7fu) != PCI_HEADER_TYPE_NORMAL ||
	    snapshot->command != expected_command || snapshot->pmcsr != 0 ||
	    (snapshot->bar & PCI_BASE_ADDRESS_SPACE_IO) ||
	    snapshot->ehciir2 != X58_USB_POWER_EHCIIR2_EXPECTED ||
	    snapshot->legacy != 1 ||
	    snapshot->legacy_ctl != X58_USB_POWER_EHCI_LEGACY_CTL_EXPECTED ||
	    snapshot->legacy_ext != 0 || snapshot->cfg84 != 1)
		return false;
	base = snapshot->bar & ~PCI_BASE_ADDRESS_MEM_ATTR_MASK;
	if (base < X58_USB_POWER_PCI_MMIO_BASE ||
	    base > X58_USB_POWER_PCI_MMIO_TOP - X58_USB_POWER_EHCI_BAR_SIZE ||
	    read8p(base) != X58_USB_POWER_EHCI_CAPLEN ||
	    read16p(base + 2u) != X58_USB_POWER_EHCI_HCIVER ||
	    read32p(base + 4u) != X58_USB_POWER_EHCI_HCSPARAMS)
		return false;

	snapshot->op = base + X58_USB_POWER_EHCI_CAPLEN;
	snapshot->usbcmd = read32p(snapshot->op + X58_USB_POWER_EHCI_USBCMD);
	snapshot->usbsts = read32p(snapshot->op + X58_USB_POWER_EHCI_USBSTS);
	snapshot->usbintr = read32p(snapshot->op + X58_USB_POWER_EHCI_USBINTR);
	snapshot->configflag = read32p(snapshot->op + X58_USB_POWER_EHCI_CONFIGFLAG);
	for (size_t port = 0; port < X58_USB_POWER_EHCI_PORT_COUNT; port++)
		snapshot->portsc[port] = read32p(snapshot->op + X58_USB_POWER_EHCI_PORTSC +
			4u * port);
	printk(BIOS_DEBUG,
	       "[USB-AUTO] %s USBCMD=%08x USBSTS=%08x USBINTR=%08x "
	       "CF=%08x PORTS=%08x/%08x/%08x/%08x/%08x/%08x\n",
	       desc->name, snapshot->usbcmd, snapshot->usbsts,
	       snapshot->usbintr, snapshot->configflag,
	       snapshot->portsc[0], snapshot->portsc[1], snapshot->portsc[2],
	       snapshot->portsc[3], snapshot->portsc[4], snapshot->portsc[5]);
	return snapshot->usbcmd == X58_USB_POWER_EHCI_USBCMD_EXPECTED &&
		snapshot->usbsts == X58_USB_POWER_EHCI_USBSTS_EXPECTED &&
		snapshot->usbintr == 0 && snapshot->configflag == 0;
}

static bool x58_usb_power_uhci_snapshot(size_t index,
	struct x58_usb_power_uhci_snapshot *snapshot, uint16_t expected_command)
{
	/* Intel 319973-003, 16.1.3: UHCI Command bit8 is reserved/RO zero.
	 * Unlike EHCI (17.1.3), the standard chip's SERR write cannot latch.
	 */
	const struct x58_usb_power_controller_desc *desc = &x58_usb_power_uhci_desc[index];
	const uint32_t id = pci_io_read_config32(desc->dev, PCI_VENDOR_ID);
	const uint32_t class_rev = pci_io_read_config32(desc->dev,
		PCI_CLASS_REVISION);
	const uint8_t header = pci_io_read_config8(desc->dev, PCI_HEADER_TYPE);
	uint32_t base;

	snapshot->command = pci_io_read_config16(desc->dev, PCI_COMMAND);
	snapshot->bar = pci_io_read_config32(desc->dev, PCI_BASE_ADDRESS_4);
	snapshot->legkey = pci_io_read_config16(desc->dev, X58_USB_POWER_UHCI_LEGKEY);
	snapshot->cfg_c8 = pci_io_read_config16(desc->dev, X58_USB_POWER_UHCI_CFG_C8);
	snapshot->cfg_ca = pci_io_read_config16(desc->dev, X58_USB_POWER_UHCI_CFG_CA);
	printk(BIOS_DEBUG,
	       "[USB-AUTO] %s ID=%08x CLASS=%06x HDR=%02x CMD=%04x "
	       "BAR4=%08x LEGKEY=%04x C8/CA=%04x/%04x\n",
	       desc->name, id, class_rev >> 8, header, snapshot->command,
	       snapshot->bar, snapshot->legkey, snapshot->cfg_c8,
	       snapshot->cfg_ca);
	if (snapshot->command != expected_command) {
		printk(BIOS_WARNING, "[USB-AUTO] %s reject CMD=%04x expected=%04x\n",
		       desc->name, snapshot->command, expected_command);
		return false;
	}
	/* Intel 319973-003, 16.1.23: bits15,11:8 are R/WC event status;
	 * bit12 is RO USB interrupt status. None is an enable/control bit.
	 * Keep USBPIRQEN set, SMI/pass-through controls and reserved bits clear.
	 * Do not acknowledge or manufacture events just to match an old dump.
	 * Engine-idle and interrupt checks below remain mandatory.
	 */
	const uint16_t legkey_status = 0x9f00u;
	const uint16_t legkey_pirq_enable = 0x2000u;
	const bool legkey_ok = (snapshot->legkey & ~legkey_status) == legkey_pirq_enable;
	if (!legkey_ok) {
		printk(BIOS_WARNING,
		       "[USB-AUTO] %s reject LEGKEY=%04x controls=%04x expected=%04x\n",
		       desc->name, snapshot->legkey, snapshot->legkey & ~legkey_status,
		       legkey_pirq_enable);
		return false;
	}
	if (id != desc->id || (class_rev >> 8) != X58_USB_POWER_UHCI_CLASS ||
	    (header & 0x7fu) != PCI_HEADER_TYPE_NORMAL ||
	    !(snapshot->bar & PCI_BASE_ADDRESS_SPACE_IO) ||
	    snapshot->cfg_c8 != 0 || snapshot->cfg_ca != 0) {
		printk(BIOS_WARNING, "[USB-AUTO] %s reject identity/BAR-type/C8/CA\n",
		       desc->name);
		return false;
	}
	base = snapshot->bar & ~PCI_BASE_ADDRESS_IO_ATTR_MASK;
	if (base < X58_USB_POWER_PCI_IO_BASE ||
	    base > X58_USB_POWER_PCI_IO_TOP - X58_USB_POWER_UHCI_BAR_SIZE) {
		printk(BIOS_WARNING, "[USB-AUTO] %s reject BAR4 aperture=%08x\n",
		       desc->name, base);
		return false;
	}

	snapshot->base = base;
	snapshot->usbcmd = inw(base + X58_USB_POWER_UHCI_USBCMD);
	snapshot->usbsts = inw(base + X58_USB_POWER_UHCI_USBSTS);
	snapshot->usbintr = inw(base + X58_USB_POWER_UHCI_USBINTR);
	for (size_t port = 0; port < X58_USB_POWER_UHCI_PORT_COUNT; port++)
		snapshot->portsc[port] = inw(base + X58_USB_POWER_UHCI_PORTSC1 +
			2u * port);
	printk(BIOS_DEBUG,
	       "[USB-AUTO] %s USBCMD=%04x USBSTS=%04x USBINTR=%04x "
	       "PORTS=%04x/%04x\n",
	       desc->name, snapshot->usbcmd, snapshot->usbsts,
	       snapshot->usbintr, snapshot->portsc[0], snapshot->portsc[1]);
	return snapshot->usbcmd == 0 && snapshot->usbsts == 0x0020u &&
		snapshot->usbintr == 0;
}

bool x58_usb_power_standard_usb_check(pci_devfn_t dev, bool initialized)
{
	const uint16_t master = initialized ? PCI_COMMAND_MASTER : 0;
	for (size_t i = 0; i < ARRAY_SIZE(x58_usb_power_ehci_desc); i++) {
		if (dev != x58_usb_power_ehci_desc[i].dev)
			continue;
		struct x58_usb_power_ehci_snapshot snapshot;
		if (!x58_usb_power_ehci_snapshot(i, &snapshot, PCI_COMMAND_MEMORY | master))
			return false;
		/* EHCI PORTSC: PE, Force Port Resume, Suspend, Port Reset.
		 * No enabled/reset/suspended port before DMA admission. Dynamic
		 * CCS/change/OCA bits remain telemetry; GPIO57 still owns power.
		 */
		for (size_t p = 0; p < X58_USB_POWER_EHCI_PORT_COUNT; p++)
			if (snapshot.portsc[p] & X58_PLATFORM_USB_EHCI_ACTIVE_PORT)
				return false;
		return true;
	}
	for (size_t i = 0; i < ARRAY_SIZE(x58_usb_power_uhci_desc); i++) {
		if (dev != x58_usb_power_uhci_desc[i].dev)
			continue;
		struct x58_usb_power_uhci_snapshot snapshot;
		if (!x58_usb_power_uhci_snapshot(i, &snapshot, PCI_COMMAND_IO | master))
			return false;
		/* UHCI PORTSC: PE, Port Reset, Suspend. Resume-detect bit6 is
		 * asynchronous status, unlike EHCI's Force Port Resume control.
		 */
		for (size_t p = 0; p < X58_USB_POWER_UHCI_PORT_COUNT; p++)
			if (snapshot.portsc[p] & X58_PLATFORM_USB_UHCI_ACTIVE_PORT)
				return false;
		return true;
	}
	return false;
}

static void x58_usb_power_log_summary(const char *phase,
	const struct x58_usb_power_snapshot *snapshot)
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

	for (size_t controller = 0; controller < X58_USB_POWER_EHCI_COUNT; controller++) {
		for (size_t port = 0; port < X58_USB_POWER_EHCI_PORT_COUNT; port++) {
			const unsigned int bit = controller * X58_USB_POWER_EHCI_PORT_COUNT + port;
			const uint32_t portsc = snapshot->ehci[controller].portsc[port];

			ehci_ccs |= !!(portsc & X58_USB_POWER_EHCI_PORT_CCS) << bit;
			ehci_pe |= !!(portsc & X58_USB_POWER_EHCI_PORT_PE) << bit;
			ehci_oca |= !!(portsc & X58_USB_POWER_EHCI_PORT_OCA) << bit;
			ehci_occ |= !!(portsc & X58_USB_POWER_EHCI_PORT_OCC) << bit;
			ehci_owner |= !!(portsc & X58_USB_POWER_EHCI_PORT_OWNER) << bit;
		}
	}
	for (size_t controller = 0; controller < X58_USB_POWER_UHCI_COUNT; controller++) {
		for (size_t port = 0; port < X58_USB_POWER_UHCI_PORT_COUNT; port++) {
			const unsigned int bit = controller * X58_USB_POWER_UHCI_PORT_COUNT + port;
			const uint16_t portsc = snapshot->uhci[controller].portsc[port];

			uhci_ccs |= !!(portsc & X58_USB_POWER_UHCI_PORT_CCS) << bit;
			uhci_pe |= !!(portsc & X58_USB_POWER_UHCI_PORT_PE) << bit;
			uhci_lsda |= !!(portsc & X58_USB_POWER_UHCI_PORT_LSDA) << bit;
			uhci_oca |= !!(portsc & X58_USB_POWER_UHCI_PORT_OCA) << bit;
			uhci_oci |= !!(portsc & X58_USB_POWER_UHCI_PORT_OCI) << bit;
		}
	}
	printk(BIOS_DEBUG,
	       "[USB-AUTO] %s EHCI CCS=%03x PE=%03x OCA=%03x OCC=%03x "
	       "OWNER=%03x UHCI CCS=%03x PE=%03x LSDA=%03x OCA=%03x "
	       "OCI=%03x\n",
	       phase, ehci_ccs, ehci_pe, ehci_oca, ehci_occ, ehci_owner,
	       uhci_ccs, uhci_pe, uhci_lsda, uhci_oca, uhci_oci);
}

static bool x58_usb_power_all_oca_active(const struct x58_usb_power_snapshot *snapshot)
{
	for (size_t i = 0; i < X58_USB_POWER_EHCI_COUNT; i++)
		for (size_t port = 0; port < X58_USB_POWER_EHCI_PORT_COUNT; port++)
			if (!(snapshot->ehci[i].portsc[port] &
			      X58_USB_POWER_EHCI_PORT_OCA))
				return false;
	for (size_t i = 0; i < X58_USB_POWER_UHCI_COUNT; i++)
		for (size_t port = 0; port < X58_USB_POWER_UHCI_PORT_COUNT; port++)
			if (!(snapshot->uhci[i].portsc[port] &
			      X58_USB_POWER_UHCI_PORT_OCA))
				return false;
	return true;
}

static bool x58_usb_power_all_oca_clear(const struct x58_usb_power_snapshot *snapshot)
{
	for (size_t i = 0; i < X58_USB_POWER_EHCI_COUNT; i++)
		for (size_t port = 0; port < X58_USB_POWER_EHCI_PORT_COUNT; port++)
			if (snapshot->ehci[i].portsc[port] & X58_USB_POWER_EHCI_PORT_OCA)
				return false;
	for (size_t i = 0; i < X58_USB_POWER_UHCI_COUNT; i++)
		for (size_t port = 0; port < X58_USB_POWER_UHCI_PORT_COUNT; port++)
			if (snapshot->uhci[i].portsc[port] & X58_USB_POWER_UHCI_PORT_OCA)
				return false;
	return true;
}

static bool x58_usb_power_collect(struct x58_usb_power_snapshot *snapshot,
	const char *phase, bool persistent, bool require_oca_active)
{
	bool valid;

	memset(snapshot, 0, sizeof(*snapshot));
	valid = x58_usb_power_global_snapshot(snapshot, persistent);
	if (valid) {
		for (size_t i = 0; i < X58_USB_POWER_EHCI_COUNT; i++)
			valid &= x58_usb_power_ehci_snapshot(i, &snapshot->ehci[i],
				PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
		for (size_t i = 0; i < X58_USB_POWER_UHCI_COUNT; i++)
			valid &= x58_usb_power_uhci_snapshot(i, &snapshot->uhci[i],
				PCI_COMMAND_IO | PCI_COMMAND_MASTER);
	}
	/* OCC/OCI and connect/speed/change fields are asynchronous telemetry. */
	if (valid && require_oca_active)
		valid = x58_usb_power_all_oca_active(snapshot);
	if (valid)
		x58_usb_power_log_summary(phase, snapshot);
	else
		printk(BIOS_ERR,
			       "[USB-AUTO] %s INVALID persistent=%u require_oca_active=%u\n",
			       phase, persistent, require_oca_active);
	return valid;
}

static void x58_usb_power_sample_ports(const char *phase,
	const struct x58_usb_power_snapshot *addresses)
{
	struct x58_usb_power_snapshot sample = *addresses;

	for (size_t i = 0; i < X58_USB_POWER_EHCI_COUNT; i++)
		for (size_t port = 0; port < X58_USB_POWER_EHCI_PORT_COUNT; port++)
			sample.ehci[i].portsc[port] =
				read32p(addresses->ehci[i].op + X58_USB_POWER_EHCI_PORTSC +
					4u * port);
	for (size_t i = 0; i < X58_USB_POWER_UHCI_COUNT; i++)
		for (size_t port = 0; port < X58_USB_POWER_UHCI_PORT_COUNT; port++)
			sample.uhci[i].portsc[port] =
				inw(addresses->uhci[i].base + X58_USB_POWER_UHCI_PORTSC1 +
					2u * port);
	printk(BIOS_DEBUG,
	       "[USB-AUTO] %s GPIO DIR2=%08x LVL2=%08x "
	       "EHCI2=%08x/%08x/%08x/%08x/%08x/%08x "
	       "EHCI1=%08x/%08x/%08x/%08x/%08x/%08x\n",
	       phase, inl(DEFAULT_GPIOBASE + GP_IO_SEL2),
	       inl(DEFAULT_GPIOBASE + GP_LVL2),
	       sample.ehci[0].portsc[0], sample.ehci[0].portsc[1],
	       sample.ehci[0].portsc[2], sample.ehci[0].portsc[3],
	       sample.ehci[0].portsc[4], sample.ehci[0].portsc[5],
	       sample.ehci[1].portsc[0], sample.ehci[1].portsc[1],
	       sample.ehci[1].portsc[2], sample.ehci[1].portsc[3],
	       sample.ehci[1].portsc[4], sample.ehci[1].portsc[5]);
	for (size_t i = 0; i < X58_USB_POWER_UHCI_COUNT; i++)
		printk(BIOS_DEBUG, "[USB-AUTO] %s %s=%04x/%04x\n",
		       phase, x58_usb_power_uhci_desc[i].name,
		       sample.uhci[i].portsc[0], sample.uhci[i].portsc[1]);
	x58_usb_power_log_summary(phase, &sample);
}

static bool x58_usb_power_rollback_gpio57(void)
{
	bool level_exact;
	bool direction_exact;
	const uint32_t lpc_id = pci_io_read_config32(X58_USB_POWER_LPC_DEV, PCI_VENDOR_ID);
	const uint32_t gpiobase = pci_io_read_config32(X58_USB_POWER_LPC_DEV, GPIOBASE);
	const uint8_t gpio_cntl = pci_io_read_config8(X58_USB_POWER_LPC_DEV,
		D31F0_GPIO_CNTL);
	uint32_t direction;
	const uint32_t level_mask = X58_USB_POWER_GPIO_LVL2_STABLE_MASK | X58_USB_POWER_GPIO57;

	/* Never issue a recovery write through a decode that just failed a gate. */
	if (lpc_id != X58_USB_POWER_LPC_ID || gpiobase != X58_USB_POWER_GPIOBASE_EXPECTED ||
	    gpio_cntl != X58_USB_POWER_GPIO_CNTL_EXPECTED)
		return false;
	if (inl(DEFAULT_GPIOBASE + GP_IO_USE_SEL2) != X58_USB_POWER_GPIO_USE2_EXPECTED)
		return false;
	direction = inl(DEFAULT_GPIOBASE + GP_IO_SEL2);
	if ((direction & ~X58_USB_POWER_GPIO57) !=
	    (X58_USB_POWER_GPIO_DIR2_EXPECTED & ~X58_USB_POWER_GPIO57))
		return false;

	/* Return H_PWRGD low before placing the signal back in input mode. */
	level_exact = x58_usb_power_gpio2_msb_write_target(GP_LVL2,
		X58_USB_POWER_GPIO57_BYTE, false, 0);
	direction_exact = x58_usb_power_gpio2_msb_write_target(GP_IO_SEL2,
		X58_USB_POWER_GPIO57_BYTE, true, X58_USB_POWER_GPIO57_DIR_NEIGHBORS);
	if (!level_exact || !direction_exact)
		return false;
	if (inl(DEFAULT_GPIOBASE + GP_IO_SEL2) !=
	    X58_USB_POWER_GPIO_DIR2_EXPECTED)
		return false;
	if ((inl(DEFAULT_GPIOBASE + GP_LVL2) & level_mask) !=
	    (X58_USB_POWER_GPIO_LVL2_EXPECTED & level_mask))
		return false;
	post_code(POST_X58_USB_POWER_ROLLBACK);
	printk(BIOS_NOTICE,
	       "[USB-AUTO] GPIO57 rollback target low/input verified\n");
	return true;
}

static void __noreturn x58_usb_power_fail(const char *reason, bool rollback)
{
	bool rollback_ok = true;

	if (rollback)
		rollback_ok = x58_usb_power_baseline_valid && x58_usb_power_rollback_gpio57();
	/* Keep the externally visible terminal code at FAIL, not ROLLBACK. */
	post_code(POST_X58_USB_POWER_FAIL);
	printk(BIOS_EMERG,
	       "[USB-AUTO] %s FAIL reason=%s rollback_requested=%u "
	       "rollback_ok=%u; payload blocked; cold recovery required\n",
	       X58_USB_POWER_STAGE_ID, reason, rollback, rollback_ok);
	console_tx_flush();
	die("X58_USB_POWER automatic USB release failed\n");
}

static void x58_usb_power_automatic_release(void *unused)
{
	struct x58_usb_power_snapshot released;

	(void)unused;
	x58_pci_standard_usb_verify();
	post_code(POST_X58_USB_POWER_ENTRY);
	printk(BIOS_NOTICE,
	       "\n[USB-AUTO] %s BEGIN BS_WRITE_TABLES/ON_ENTRY; "
	       "SeaBIOS owns HC reset/enumeration/HID\n",
	       X58_USB_POWER_STAGE_ID);
	x58_usb_power_baseline_valid = x58_usb_power_collect(&x58_usb_power_baseline,
		"PRECISE-X58_LEGACY_INPUT-BASELINE", false, true);
	if (!x58_usb_power_baseline_valid)
		x58_usb_power_fail("exact-late-preflight", false);
	post_code(POST_X58_USB_POWER_PREFLIGHT);
	console_tx_flush();

	/* Direction output while the already-admitted GPIO57 latch remains low. */
	if (!x58_usb_power_gpio2_msb_write_target(GP_IO_SEL2,
		X58_USB_POWER_GPIO57_BYTE, false, X58_USB_POWER_GPIO57_DIR_NEIGHBORS))
		x58_usb_power_fail("GPIO57-output-low-readback", true);
	if (inl(DEFAULT_GPIOBASE + GP_IO_SEL2) !=
	    (X58_USB_POWER_GPIO_DIR2_EXPECTED & ~X58_USB_POWER_GPIO57) ||
	    (inl(DEFAULT_GPIOBASE + GP_LVL2) & X58_USB_POWER_GPIO57))
		x58_usb_power_fail("GPIO57-output-low-full-gate", true);
	post_code(POST_X58_USB_POWER_GPIO_LOW);
	x58_usb_power_sample_ports("GPIO57-LOW-T+0MS", &x58_usb_power_baseline);
	udelay(X58_USB_POWER_GPIO57_LOW_SETTLE_US);
	x58_usb_power_sample_ports("GPIO57-LOW-T+65536US", &x58_usb_power_baseline);

	/* The level byte may contain dynamic native inputs; gate the target only. */
	if (!x58_usb_power_gpio2_msb_write_target(GP_LVL2,
		X58_USB_POWER_GPIO57_BYTE, true, 0))
		x58_usb_power_fail("GPIO57-high-readback", true);
	post_code(POST_X58_USB_POWER_GPIO_HIGH);
	x58_usb_power_sample_ports("GPIO57-HIGH-T+0MS", &x58_usb_power_baseline);
	udelay(10000);
	x58_usb_power_sample_ports("GPIO57-HIGH-T+10MS", &x58_usb_power_baseline);
	udelay(90000);
	x58_usb_power_sample_ports("GPIO57-HIGH-T+100MS", &x58_usb_power_baseline);
	udelay(400000);
	x58_usb_power_sample_ports("GPIO57-HIGH-T+500MS", &x58_usb_power_baseline);

	if (!x58_usb_power_collect(&released, "GPIO57-RELEASE-GATE", true, false))
		x58_usb_power_fail("persistent-control-readback", true);
	if (!x58_usb_power_all_oca_clear(&released))
		x58_usb_power_fail("live-OCA-still-active", true);
	x58_usb_power_released = true;
	post_code(POST_X58_USB_POWER_RELEASED);
	printk(BIOS_NOTICE,
	       "[USB-AUTO] GPIO57 output/high and all EHCI/UHCI OCA clear; "
	       "sticky OCC/OCI and dynamic CCS/LSDA are telemetry only\n");
	console_tx_flush();
}

static void x58_usb_power_payload_gate(void *unused)
{
	struct x58_usb_power_snapshot final;

	(void)unused;
	x58_pci_standard_usb_verify();
	if (!x58_usb_power_released)
		x58_usb_power_fail("release-callback-not-complete", false);
	if (!x58_usb_power_collect(&final, "FINAL-PRE-PAYLOAD-GATE", true, false))
		x58_usb_power_fail("final-control-readback", true);
	if (!x58_usb_power_all_oca_clear(&final))
		x58_usb_power_fail("final-live-OCA-active", true);
	post_code(POST_X58_USB_POWER_PAYLOAD_READY);
	printk(BIOS_NOTICE,
	       "[USB-AUTO] %s PAYLOAD ADMIT; GPIO57 remains output/high; "
	       "SeaBIOS EHCI/UHCI/HID path follows\n",
	       X58_USB_POWER_STAGE_ID);
	console_tx_flush();
}

BOOT_STATE_INIT_ENTRY(BS_WRITE_TABLES, BS_ON_ENTRY,
	x58_usb_power_automatic_release, NULL);
BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_BOOT, BS_ON_ENTRY,
	x58_usb_power_payload_gate, NULL);
