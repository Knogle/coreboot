/* SPDX-License-Identifier: GPL-2.0-only */

/* Late, exact-gated input enablement for the X58_LEGACY_INPUT SeaBIOS path. */

#include <arch/io.h>
#include <arch/pci_io_cfg.h>
#include <console/console.h>
#include <device/pci_def.h>
#include <device/pci_type.h>
#include <pc80/keyboard.h>
#include <stdbool.h>
#include <stdint.h>
#include <southbridge/intel/common/lpc_def.h>

#include "legacy_input.h"

#define X58_LEGACY_INPUT_LPC_DEV		PCI_DEV(0, 0x1f, 0)
#define X58_LEGACY_INPUT_LPC_ID		0x3a168086u
#define X58_LEGACY_INPUT_LPC_IO_DEC		0x0010u
#define X58_LEGACY_INPUT_LPC_EN_PRE		(CNF2_LPC_EN | COMA_LPC_EN)
#define X58_LEGACY_INPUT_LPC_EN_KBC		(X58_LEGACY_INPUT_LPC_EN_PRE | KBC_LPC_EN)

#define X58_LEGACY_INPUT_SIO_INDEX		0x4eu
#define X58_LEGACY_INPUT_SIO_DATA		0x4fu
#define X58_LEGACY_INPUT_SIO_LDN_REG	0x07u
#define X58_LEGACY_INPUT_SIO_KBC_LDN	0x05u
#define X58_LEGACY_INPUT_SIO_DID0_REG	0x20u
#define X58_LEGACY_INPUT_SIO_DID1_REG	0x21u
#define X58_LEGACY_INPUT_SIO_VID0_REG	0x23u
#define X58_LEGACY_INPUT_SIO_VID1_REG	0x24u
/* F71882 V0.28P section 8.1.7 p56: software power down, NOT revision. */
#define X58_LEGACY_INPUT_SIO_POWER_DOWN_REG	0x25u
#define X58_LEGACY_INPUT_SIO_ENABLE_REG	0x30u
#define X58_LEGACY_INPUT_SIO_IO0_HI_REG	0x60u
#define X58_LEGACY_INPUT_SIO_IO0_LO_REG	0x61u
#define X58_LEGACY_INPUT_SIO_IRQ1_REG	0x70u
#define X58_LEGACY_INPUT_SIO_IRQ2_REG	0x72u
#define X58_LEGACY_INPUT_SIO_KBC_MODE_REG	0xf0u

#define X58_LEGACY_INPUT_KBC_STATUS_PORT	0x64u
#define POST_X58_LEGACY_INPUT_BEGIN	0x8au
#define POST_X58_LEGACY_INPUT_DECODE_READY	0x8bu
#define POST_X58_LEGACY_INPUT_READY	0x8cu
#define POST_X58_LEGACY_INPUT_FAIL	0x8du

struct x58_legacy_input_sio_snapshot {
	uint8_t original_ldn;
	uint8_t did0;
	uint8_t did1;
	uint8_t vid0;
	uint8_t vid1;
	uint8_t power_down;
	uint8_t enabled;
	uint8_t io0_hi;
	uint8_t io0_lo;
	uint8_t irq1;
	uint8_t irq2;
	uint8_t mode;
};

static bool x58_legacy_input_ready;

_Static_assert(X58_LEGACY_INPUT_LPC_EN_PRE == 0x2001,
	"X58_LEGACY_INPUT must start from the measured CNF2/COM1-only LPC decode");
_Static_assert(KBC_LPC_EN == 0x0400,
	"X58_LEGACY_INPUT may add only the documented 60/64 KBC decode bit");
_Static_assert(X58_LEGACY_INPUT_LPC_EN_KBC == 0x2401,
	"X58_LEGACY_INPUT KBC decode target changed");

static uint8_t x58_legacy_input_sio_read(uint8_t reg)
{
	outb(reg, X58_LEGACY_INPUT_SIO_INDEX);
	return inb(X58_LEGACY_INPUT_SIO_DATA);
}

static void x58_legacy_input_sio_write(uint8_t reg, uint8_t value)
{
	outb(reg, X58_LEGACY_INPUT_SIO_INDEX);
	outb(value, X58_LEGACY_INPUT_SIO_DATA);
}

static struct x58_legacy_input_sio_snapshot x58_legacy_input_read_sio_snapshot(void)
{
	struct x58_legacy_input_sio_snapshot snapshot;

	outb(0x87, X58_LEGACY_INPUT_SIO_INDEX);
	outb(0x87, X58_LEGACY_INPUT_SIO_INDEX);
	snapshot.original_ldn = x58_legacy_input_sio_read(X58_LEGACY_INPUT_SIO_LDN_REG);
	snapshot.did0 = x58_legacy_input_sio_read(X58_LEGACY_INPUT_SIO_DID0_REG);
	snapshot.did1 = x58_legacy_input_sio_read(X58_LEGACY_INPUT_SIO_DID1_REG);
	snapshot.vid0 = x58_legacy_input_sio_read(X58_LEGACY_INPUT_SIO_VID0_REG);
	snapshot.vid1 = x58_legacy_input_sio_read(X58_LEGACY_INPUT_SIO_VID1_REG);
	snapshot.power_down = x58_legacy_input_sio_read(X58_LEGACY_INPUT_SIO_POWER_DOWN_REG);
	x58_legacy_input_sio_write(X58_LEGACY_INPUT_SIO_LDN_REG, X58_LEGACY_INPUT_SIO_KBC_LDN);
	snapshot.enabled = x58_legacy_input_sio_read(X58_LEGACY_INPUT_SIO_ENABLE_REG);
	snapshot.io0_hi = x58_legacy_input_sio_read(X58_LEGACY_INPUT_SIO_IO0_HI_REG);
	snapshot.io0_lo = x58_legacy_input_sio_read(X58_LEGACY_INPUT_SIO_IO0_LO_REG);
	snapshot.irq1 = x58_legacy_input_sio_read(X58_LEGACY_INPUT_SIO_IRQ1_REG);
	snapshot.irq2 = x58_legacy_input_sio_read(X58_LEGACY_INPUT_SIO_IRQ2_REG);
	snapshot.mode = x58_legacy_input_sio_read(X58_LEGACY_INPUT_SIO_KBC_MODE_REG);
	x58_legacy_input_sio_write(X58_LEGACY_INPUT_SIO_LDN_REG, snapshot.original_ldn);
	outb(0xaa, X58_LEGACY_INPUT_SIO_INDEX);

	return snapshot;
}

static bool x58_legacy_input_sio_is_exact(const struct x58_legacy_input_sio_snapshot *snapshot)
{
	return snapshot->did0 == 0x05 && snapshot->did1 == 0x41 &&
		snapshot->vid0 == 0x19 && snapshot->vid1 == 0x34 &&
		snapshot->power_down == 0x00 && snapshot->enabled == 0x01 &&
		snapshot->io0_hi == 0x00 && snapshot->io0_lo == 0x60 &&
		snapshot->irq1 == 0x01 && snapshot->irq2 == 0x0c &&
		snapshot->mode == 0x83;
}

void x58_legacy_input_prepare_input(void)
{
	const uint32_t lpc_id = pci_io_read_config32(X58_LEGACY_INPUT_LPC_DEV,
		PCI_VENDOR_ID);
	const uint16_t io_dec = pci_io_read_config16(X58_LEGACY_INPUT_LPC_DEV,
		LPC_IO_DEC);
	const uint16_t lpc_en = pci_io_read_config16(X58_LEGACY_INPUT_LPC_DEV, LPC_EN);
	const uint8_t status_before = inb(X58_LEGACY_INPUT_KBC_STATUS_PORT);
	struct x58_legacy_input_sio_snapshot sio;
	uint8_t aux_detected;
	uint8_t status_after;

	if (x58_legacy_input_ready)
		die_with_post_code(POST_X58_LEGACY_INPUT_FAIL,
			"[INPUT] X58_LEGACY_INPUT refused a second input preparation\n");
	printk(BIOS_DEBUG,
	       "[INPUT] X58_LEGACY_INPUT LPC_PRE ID=%08x IO_DEC=%04x LPC_EN=%04x KBC_STS=%02x\n",
	       lpc_id, io_dec, lpc_en, status_before);
	if (lpc_id != X58_LEGACY_INPUT_LPC_ID || io_dec != X58_LEGACY_INPUT_LPC_IO_DEC ||
	    lpc_en != X58_LEGACY_INPUT_LPC_EN_PRE || status_before != 0xff)
		die_with_post_code(POST_X58_LEGACY_INPUT_FAIL,
			"[INPUT] X58_LEGACY_INPUT rejected exact LPC/KBC prestate\n");

	/* Enter the Super I/O configuration space only after the LPC gate. */
	sio = x58_legacy_input_read_sio_snapshot();
	printk(BIOS_DEBUG,
	       "[INPUT] X58_LEGACY_INPUT SIO_PRE FINTEK=%02x%02x:%02x%02x POWER_DOWN=%02x "
	       "LDN5=%02x IO=%02x%02x "
	       "IRQ=%02x/%02x MODE=%02x RESTORE_LDN=%02x\n",
	       sio.did1, sio.did0, sio.vid1, sio.vid0, sio.power_down,
	       sio.enabled, sio.io0_hi,
	       sio.io0_lo, sio.irq1, sio.irq2, sio.mode, sio.original_ldn);
	if (!x58_legacy_input_sio_is_exact(&sio))
		die_with_post_code(POST_X58_LEGACY_INPUT_FAIL,
			"[INPUT] X58_LEGACY_INPUT rejected exact Fintek prestate\n");

	post_code(POST_X58_LEGACY_INPUT_BEGIN);
	pci_io_write_config16(X58_LEGACY_INPUT_LPC_DEV, LPC_EN, X58_LEGACY_INPUT_LPC_EN_KBC);
	status_after = inb(X58_LEGACY_INPUT_KBC_STATUS_PORT);
	if (pci_io_read_config16(X58_LEGACY_INPUT_LPC_DEV, LPC_IO_DEC) !=
		X58_LEGACY_INPUT_LPC_IO_DEC ||
	    pci_io_read_config16(X58_LEGACY_INPUT_LPC_DEV, LPC_EN) != X58_LEGACY_INPUT_LPC_EN_KBC ||
	    status_after == 0xff)
		die_with_post_code(POST_X58_LEGACY_INPUT_FAIL,
			"[INPUT] X58_LEGACY_INPUT KBC decode/readback failed\n");
	post_code(POST_X58_LEGACY_INPUT_DECODE_READY);
	printk(BIOS_DEBUG,
	       "[INPUT] X58_LEGACY_INPUT KBC_DECODE 2001->2401 PORT64=%02x->%02x WRITE_MASK=0400\n",
	       status_before, status_after);

	/* This routine is bounded and treats an absent physical keyboard as non-fatal. */
	aux_detected = pc_keyboard_init(NO_AUX_DEVICE);
	if (pci_io_read_config16(X58_LEGACY_INPUT_LPC_DEV, LPC_EN) != X58_LEGACY_INPUT_LPC_EN_KBC)
		die_with_post_code(POST_X58_LEGACY_INPUT_FAIL,
			"[INPUT] X58_LEGACY_INPUT LPC decode changed during keyboard probe\n");

	x58_legacy_input_ready = true;
	post_code(POST_X58_LEGACY_INPUT_READY);
	printk(BIOS_NOTICE,
	       "[INPUT] X58_LEGACY_INPUT bounded PS2 probe returned AUX=%u; inspect keyboard ACK/BAT log; "
	       "KBC decode retained, ACPI i8042 not advertised\n",
	       aux_detected);
}

bool x58_legacy_input_is_ready(void)
{
	/* Controller configuration admitted, not proof of a connected keyboard. */
	return x58_legacy_input_ready;
}
