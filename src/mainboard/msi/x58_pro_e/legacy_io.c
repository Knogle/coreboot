/* SPDX-License-Identifier: GPL-2.0-only */
/* LEGACY_IO: narrow LPC-IRQ path, not generic southbridge initialization. */
#include <arch/io.h>
#include <arch/pci_io_cfg.h>
#include <console/console.h>
#include <device/pci_type.h>
#include <southbridge/intel/common/lpc_def.h>
#include <southbridge/intel/i82801jx/i82801jx.h>
#include <string.h>
#include "legacy_input.h"
#include "legacy_io.h"

#define X58_PLATFORM_LPC PCI_DEV(0, 0x1f, 0)
#define X58_PLATFORM_LPC_ID 0x3a168086u
#define X58_PLATFORM_SERIRQ_ENABLE (1u << 7)
#define X58_PLATFORM_SERIRQ_CONTINUOUS (1u << 6)
#define X58_PLATFORM_SERIRQ_FRAME_21 ((21u - 17u) << 2)
#define X58_PLATFORM_SERIRQ_READY (X58_PLATFORM_SERIRQ_ENABLE | X58_PLATFORM_SERIRQ_CONTINUOUS | X58_PLATFORM_SERIRQ_FRAME_21)
#define X58_PLATFORM_LPC_DECODE (CNF2_LPC_EN | COMA_LPC_EN | KBC_LPC_EN)
#define X58_PLATFORM_IOST_KEYBOARD (1u << 10)
#define X58_PLATFORM_IOST_MOUSE (1u << 12)

static bool requested = true;
static bool attempted;
static bool serirq_ready;
static bool ps2_ready;

void x58_platform_legacy_status(void)
{
	/* Cached decisions only: safe also at the first/fault ROMMON. */
	printk(BIOS_NOTICE, "[LEGACY_IO] LEGACYIRQ_REQUEST=%s ATTEMPTED=%u "
		"SERIRQ_READY=%u PS2_CONTROLLER=%u IRQ_DELIVERY_UNTESTED=1\n",
		requested ? "ON" : "OFF", attempted, serirq_ready, ps2_ready);
}

bool x58_platform_legacy_command(int argc, char **argv, bool frozen)
{
	if (argc < 1 || strcmp(argv[0], "legacyirq"))
		return false;
	if (argc == 1) {
		x58_platform_legacy_status();
		return true;
	}
	if (argc != 2 || (strcmp(argv[1], "on") && strcmp(argv[1], "off"))) {
		printk(BIOS_ERR, "[LEGACY_IO] legacyirq [on|off]\n");
		return true;
	}
	if (!strcmp(argv[1], "off")) {
		printk(BIOS_ERR, "[PLATFORM] legacyirq off unavailable: standard LPC owns SERIRQ\n");
		return true;
	}
	if (frozen || attempted) {
		printk(BIOS_ERR, "[LEGACY_IO] legacyirq frozen; cold boot to change; no rollback\n");
		return true;
	}
	requested = !strcmp(argv[1], "on");
	x58_platform_legacy_status();
	return true;
}

void x58_platform_legacy_enable_serirq(void)
{
	if (attempted)
		die("[LEGACY_IO] repeated SERIRQ preparation; cold recovery required\n");
	attempted = true;
	if (!requested)
		die("[PLATFORM] standard LPC SERIRQ ownership cannot be disabled\n");
	if (!requested) {
		x58_platform_legacy_status();
		return;
	}
	/* Caller has masked external PIC/IOAPIC inputs (PIC cascade IRQ2 stays
	 * enabled) and installed the existing IRQ routes.
	 * ICH10 lpc.c:i82801jx_enable_serial_irqs / early_core.c:lpc_setup
	 * use d0; the same value is observed on the AMI reference. Preserve the
	 * measured 21-frame/low-bit encoding. No other LPC initialization here.
	 * One attempt only. Polled COM1 remains available if readback fails. */
	if (pci_io_read_config32(X58_PLATFORM_LPC, 0) != X58_PLATFORM_LPC_ID ||
	    inb(0x21) != 0xfb || inb(0xa1) != 0xff)
		die("[LEGACY_IO] SERIRQ identity/PIC-mask precondition failed; no write\n");
	const uint8_t before = pci_io_read_config8(X58_PLATFORM_LPC, D31F0_SERIRQ_CNTL);
	/* Admit the standard initializer's result; this path never programs it. */
	if (before != X58_PLATFORM_SERIRQ_READY)
		die("[PLATFORM] standard LPC SERIRQ=%02x not ready; no write\n", before);
	const uint8_t after = pci_io_read_config8(X58_PLATFORM_LPC, D31F0_SERIRQ_CNTL);
	if (after != X58_PLATFORM_SERIRQ_READY)
		die("[LEGACY_IO] SERIRQ readback=%02x; cold recovery required\n", after);
	serirq_ready = true;
	printk(BIOS_NOTICE, "[PLATFORM] SERIRQ PRE=%02x POST=%02x WRITE_MASK=00 "
		"WRITES=0 OWNER=STANDARD_LPC CONTINUOUS=1 IRQ_DELIVERY_UNTESTED=1\n",
		before, after);
}

void x58_platform_legacy_admit_ps2(void)
{
	if (!attempted || ps2_ready)
		die("[LEGACY_IO] invalid PS2 admission phase\n");
	if (!requested)
		return;
	/* Reuse the immediately preceding X58_LEGACY_INPUT exact Fintek snapshot and
	 * bounded controller setup. Datasheet V0.28P sections 8.1.7/8.7.1:
	 * power-down=00, LDN5 enabled, base0060, IRQ1/12, F0=83 (12MHz,
	 * hardware A20/reset acceleration). No new Fintek/KBC command here.
	 * _STA describes available controller interfaces, not attached devices. */
	if (!serirq_ready || !x58_legacy_input_is_ready() ||
	    pci_io_read_config32(X58_PLATFORM_LPC, 0) != X58_PLATFORM_LPC_ID ||
	    pci_io_read_config8(X58_PLATFORM_LPC, D31F0_SERIRQ_CNTL) != X58_PLATFORM_SERIRQ_READY ||
	    pci_io_read_config16(X58_PLATFORM_LPC, LPC_IO_DEC) != 0x0010 ||
	    pci_io_read_config16(X58_PLATFORM_LPC, LPC_EN) != X58_PLATFORM_LPC_DECODE)
		die("[LEGACY_IO] PS2 controller/decode admission failed\n");
	ps2_ready = true;
	x58_platform_legacy_status();
}

bool x58_platform_legacy_ps2_present(void)
{
	return requested && serirq_ready && ps2_ready;
}

uint16_t x58_platform_legacy_iost(void)
{
	/* Private unchanged MSI AML: IOST at byte1; PS2K bit10, PS2M bit12. */
	return x58_platform_legacy_ps2_present() ? X58_PLATFORM_IOST_KEYBOARD | X58_PLATFORM_IOST_MOUSE : 0;
}
