/* SPDX-License-Identifier: GPL-2.0-only */

/* Late, exact-gated input enablement for the B06WD SeaBIOS experiment. */

#include <acpi/acpi.h>
#include <arch/io.h>
#include <arch/pci_io_cfg.h>
#include <console/console.h>
#include <device/pci_def.h>
#include <device/pci_type.h>
#include <pc80/keyboard.h>
#include <stdbool.h>
#include <stdint.h>
#include <southbridge/intel/common/lpc_def.h>

#include "b06wd_input.h"

#define B06WD_LPC_DEV		PCI_DEV(0, 0x1f, 0)
#define B06WD_LPC_ID		0x3a168086u
#define B06WD_LPC_IO_DEC		0x0010u
#define B06WD_LPC_EN_PRE		(CNF2_LPC_EN | COMA_LPC_EN)
#define B06WD_LPC_EN_KBC		(B06WD_LPC_EN_PRE | KBC_LPC_EN)

#define B06WD_SIO_INDEX		0x4eu
#define B06WD_SIO_DATA		0x4fu
#define B06WD_SIO_LDN_REG	0x07u
#define B06WD_SIO_KBC_LDN	0x05u
#define B06WD_SIO_DID0_REG	0x20u
#define B06WD_SIO_DID1_REG	0x21u
#define B06WD_SIO_VID0_REG	0x23u
#define B06WD_SIO_VID1_REG	0x24u
#define B06WD_SIO_REV_REG	0x25u
#define B06WD_SIO_ENABLE_REG	0x30u
#define B06WD_SIO_IO0_HI_REG	0x60u
#define B06WD_SIO_IO0_LO_REG	0x61u
#define B06WD_SIO_IRQ1_REG	0x70u
#define B06WD_SIO_IRQ2_REG	0x72u
#define B06WD_SIO_KBC_MODE_REG	0xf0u

#define B06WD_KBC_STATUS_PORT	0x64u
#define POST_B06WD_INPUT_BEGIN	0x8au
#define POST_B06WD_DECODE_READY	0x8bu
#define POST_B06WD_INPUT_READY	0x8cu
#define POST_B06WD_INPUT_FAIL	0x8du

struct b06wd_sio_snapshot {
	uint8_t original_ldn;
	uint8_t did0;
	uint8_t did1;
	uint8_t vid0;
	uint8_t vid1;
	uint8_t revision;
	uint8_t enabled;
	uint8_t io0_hi;
	uint8_t io0_lo;
	uint8_t irq1;
	uint8_t irq2;
	uint8_t mode;
};

static bool b06wd_input_ready;

_Static_assert(B06WD_LPC_EN_PRE == 0x2001,
	"B06WD must start from the measured CNF2/COM1-only LPC decode");
_Static_assert(KBC_LPC_EN == 0x0400,
	"B06WD may add only the documented 60/64 KBC decode bit");
_Static_assert(B06WD_LPC_EN_KBC == 0x2401,
	"B06WD KBC decode target changed");

static uint8_t b06wd_sio_read(uint8_t reg)
{
	outb(reg, B06WD_SIO_INDEX);
	return inb(B06WD_SIO_DATA);
}

static void b06wd_sio_write(uint8_t reg, uint8_t value)
{
	outb(reg, B06WD_SIO_INDEX);
	outb(value, B06WD_SIO_DATA);
}

static struct b06wd_sio_snapshot b06wd_read_sio_snapshot(void)
{
	struct b06wd_sio_snapshot snapshot;

	outb(0x87, B06WD_SIO_INDEX);
	outb(0x87, B06WD_SIO_INDEX);
	snapshot.original_ldn = b06wd_sio_read(B06WD_SIO_LDN_REG);
	snapshot.did0 = b06wd_sio_read(B06WD_SIO_DID0_REG);
	snapshot.did1 = b06wd_sio_read(B06WD_SIO_DID1_REG);
	snapshot.vid0 = b06wd_sio_read(B06WD_SIO_VID0_REG);
	snapshot.vid1 = b06wd_sio_read(B06WD_SIO_VID1_REG);
	snapshot.revision = b06wd_sio_read(B06WD_SIO_REV_REG);
	b06wd_sio_write(B06WD_SIO_LDN_REG, B06WD_SIO_KBC_LDN);
	snapshot.enabled = b06wd_sio_read(B06WD_SIO_ENABLE_REG);
	snapshot.io0_hi = b06wd_sio_read(B06WD_SIO_IO0_HI_REG);
	snapshot.io0_lo = b06wd_sio_read(B06WD_SIO_IO0_LO_REG);
	snapshot.irq1 = b06wd_sio_read(B06WD_SIO_IRQ1_REG);
	snapshot.irq2 = b06wd_sio_read(B06WD_SIO_IRQ2_REG);
	snapshot.mode = b06wd_sio_read(B06WD_SIO_KBC_MODE_REG);
	b06wd_sio_write(B06WD_SIO_LDN_REG, snapshot.original_ldn);
	outb(0xaa, B06WD_SIO_INDEX);

	return snapshot;
}

static bool b06wd_sio_is_exact(const struct b06wd_sio_snapshot *snapshot)
{
	return snapshot->did0 == 0x05 && snapshot->did1 == 0x41 &&
		snapshot->vid0 == 0x19 && snapshot->vid1 == 0x34 &&
		snapshot->revision == 0x00 && snapshot->enabled == 0x01 &&
		snapshot->io0_hi == 0x00 && snapshot->io0_lo == 0x60 &&
		snapshot->irq1 == 0x01 && snapshot->irq2 == 0x0c &&
		snapshot->mode == 0x83;
}

void b06wd_prepare_input(void)
{
	const uint32_t lpc_id = pci_io_read_config32(B06WD_LPC_DEV,
		PCI_VENDOR_ID);
	const uint16_t io_dec = pci_io_read_config16(B06WD_LPC_DEV,
		LPC_IO_DEC);
	const uint16_t lpc_en = pci_io_read_config16(B06WD_LPC_DEV, LPC_EN);
	const uint8_t status_before = inb(B06WD_KBC_STATUS_PORT);
	struct b06wd_sio_snapshot sio;
	uint8_t aux_detected;
	uint8_t status_after;

	if (b06wd_input_ready)
		die_with_post_code(POST_B06WD_INPUT_FAIL,
			"[INPUT] B06WD refused a second input preparation\n");
	printk(BIOS_NOTICE,
	       "[INPUT] B06WD LPC_PRE ID=%08x IO_DEC=%04x LPC_EN=%04x KBC_STS=%02x\n",
	       lpc_id, io_dec, lpc_en, status_before);
	if (lpc_id != B06WD_LPC_ID || io_dec != B06WD_LPC_IO_DEC ||
	    lpc_en != B06WD_LPC_EN_PRE || status_before != 0xff)
		die_with_post_code(POST_B06WD_INPUT_FAIL,
			"[INPUT] B06WD rejected exact LPC/KBC prestate\n");

	/* Enter the Super I/O configuration space only after the LPC gate. */
	sio = b06wd_read_sio_snapshot();
	printk(BIOS_NOTICE,
	       "[INPUT] B06WD SIO_PRE FINTEK=%02x%02x:%02x%02x REV=%02x "
	       "LDN5=%02x IO=%02x%02x "
	       "IRQ=%02x/%02x MODE=%02x RESTORE_LDN=%02x\n",
	       sio.did1, sio.did0, sio.vid1, sio.vid0, sio.revision,
	       sio.enabled, sio.io0_hi,
	       sio.io0_lo, sio.irq1, sio.irq2, sio.mode, sio.original_ldn);
	if (!b06wd_sio_is_exact(&sio))
		die_with_post_code(POST_B06WD_INPUT_FAIL,
			"[INPUT] B06WD rejected exact Fintek prestate\n");

	post_code(POST_B06WD_INPUT_BEGIN);
	pci_io_write_config16(B06WD_LPC_DEV, LPC_EN, B06WD_LPC_EN_KBC);
	status_after = inb(B06WD_KBC_STATUS_PORT);
	if (pci_io_read_config16(B06WD_LPC_DEV, LPC_IO_DEC) !=
		B06WD_LPC_IO_DEC ||
	    pci_io_read_config16(B06WD_LPC_DEV, LPC_EN) != B06WD_LPC_EN_KBC ||
	    status_after == 0xff)
		die_with_post_code(POST_B06WD_INPUT_FAIL,
			"[INPUT] B06WD KBC decode/readback failed\n");
	post_code(POST_B06WD_DECODE_READY);
	printk(BIOS_NOTICE,
	       "[INPUT] B06WD KBC_DECODE 2001->2401 PORT64=%02x->%02x WRITE_MASK=0400\n",
	       status_before, status_after);

	/* This routine is bounded and treats an absent physical keyboard as non-fatal. */
	aux_detected = pc_keyboard_init(NO_AUX_DEVICE);
	if (pci_io_read_config16(B06WD_LPC_DEV, LPC_EN) != B06WD_LPC_EN_KBC)
		die_with_post_code(POST_B06WD_INPUT_FAIL,
			"[INPUT] B06WD LPC decode changed during keyboard probe\n");

	b06wd_input_ready = true;
	post_code(POST_B06WD_INPUT_READY);
	printk(BIOS_NOTICE,
	       "[INPUT] B06WD bounded PS2 probe returned AUX=%u; inspect keyboard ACK/BAT log; "
	       "PNP0303/FADT8042 admission READY\n",
	       aux_detected);
}

void b06wd_enable_fadt_8042(acpi_fadt_t *fadt)
{
	if (!b06wd_input_ready)
		die_with_post_code(POST_B06WD_INPUT_FAIL,
			"[INPUT] B06WD refused FADT 8042 before exact input admission\n");
	fadt->iapc_boot_arch |= ACPI_FADT_8042;
}
