/* SPDX-License-Identifier: GPL-2.0-only */
#include <acpi/acpigen.h>
#include <arch/io.h>
#include <bootstate.h>
#include <console/console.h>
#include <device/device.h>
#include <device/pci_ops.h>
#include <device/pci_def.h>
#include <halt.h>
#include <southbridge/intel/common/lpc_def.h>
#include "acpi_registers.h"
#include "fintek_hwm.h"

#define HWM_BASE 0x290
#define HWM_SIZE 8
#define HWM_RESOURCE 0x5800
#define HWM_DECODE LPC_IO(HWM_BASE, HWM_SIZE)

static bool hwm_ready;
static bool hwm_attempted;

static u8 sio_read(u8 index)
{
	outb(index, 0x4e);
	return inb(0x4f);
}

static void sio_select(u8 ldn)
{
	outb(7, 0x4e);
	outb(ldn, 0x4f);
}

/* After enumeration, before allocation. Early UART, CMOS, fan policy and the
 * vendor-assisted initialization are untouched. ICH10 319973-003 13.1.24;
 * F71882 V0.28P 8.6.1. Physical Linux trials verified this exact aperture.
 */
static void fintek_hwm_prepare(void *unused)
{
	struct device *lpc = pcidev_on_root(0x1f, 0);
	(void)unused;
	if (hwm_attempted)
		die("[FINTEK-HWM] repeated prepare\n");
	hwm_attempted = true;
	if (!lpc || !lpc->enabled || pci_read_config32(lpc, PCI_VENDOR_ID) != 0x3a168086 ||
	    !(pci_read_config16(lpc, LPC_EN) & CNF2_LPC_EN))
		goto skip;
	for (unsigned int offset = LPC_GEN1_DEC; offset <= LPC_GEN4_DEC; offset += 4)
		if (pci_read_config32(lpc, offset))
			goto skip;
	outb(0x87, 0x4e);
	outb(0x87, 0x4e);
	const u16 id = (sio_read(0x20) << 8) | sio_read(0x21);
	const u16 vendor = (sio_read(0x23) << 8) | sio_read(0x24);
	const u8 old_ldn = sio_read(7);
	if (id != 0x0541 || vendor != 0x1934 || old_ldn > 0x0a) {
		outb(0xaa, 0x4e);
		goto skip;
	}
	sio_select(4);
	const u8 enable = sio_read(0x30);
	const u16 base = (sio_read(0x60) << 8) | sio_read(0x61);
	sio_select(old_ldn);
	const u8 restored = sio_read(7);
	outb(0xaa, 0x4e);
	if (restored != old_ldn || enable != 1 || base != HWM_BASE + 5)
		goto skip;
	/* Do not enable any additional legacy LPC functions or touch HWM data.
	 * The standard LPC subtractive window already contains this low I/O range;
	 * explicitly reserve the consumer without making it allocatable PCI I/O.
	 */
	pci_write_config32(lpc, LPC_GEN2_DEC, HWM_DECODE);
	if (pci_read_config32(lpc, LPC_GEN2_DEC) != HWM_DECODE) {
		pci_write_config32(lpc, LPC_GEN2_DEC, 0);
		if (pci_read_config32(lpc, LPC_GEN2_DEC))
			die("[FINTEK-HWM] decode rollback failed; state uncertain\n");
		goto skip;
	}
	fixed_io_range_flags(lpc, HWM_RESOURCE, HWM_BASE, HWM_SIZE,
		IORESOURCE_RESERVE | IORESOURCE_STORED);
	hwm_ready = true;
	printk(BIOS_NOTICE, "[FINTEK-HWM] READY IO=0290-0297 GEN2=00040291 FAN_POLICY=UNCHANGED\n");
	return;
skip:
	printk(BIOS_WARNING, "[FINTEK-HWM] unavailable; boot continues without HWM publication\n");
}

void x58_fintek_hwm_ssdt(void)
{
	if (!hwm_ready)
		return;
	struct device *lpc = pcidev_on_root(0x1f, 0);
	if (!lpc)
		die("[FINTEK-HWM] LPC disappeared before ACPI publication\n");
	const struct resource *res = probe_resource(lpc, HWM_RESOURCE);
	if (!res || res->base != HWM_BASE || res->size != HWM_SIZE ||
	    pci_read_config32(lpc, LPC_GEN2_DEC) != HWM_DECODE)
		die("[FINTEK-HWM] resource/decode changed before ACPI publication\n");
	/* Reservation only; never add runtime control methods or redefine IOHB.
	 * No OperationRegion: native Linux f71882fg owns runtime index/data access.
	 */
	acpigen_write_scope("\\_SB");
	acpigen_write_device("FHWM");
	acpigen_write_name("_HID");
	acpigen_emit_eisaid("PNP0C02");
	acpigen_write_name_integer("_UID", X58_ACPI_UID_FHWM);
	acpigen_write_name_integer("_STA", 0x0f);
	acpigen_write_name("_CRS");
	acpigen_write_resourcetemplate_header();
	acpigen_write_io16(HWM_BASE, HWM_BASE, 1, HWM_SIZE, 1);
	acpigen_write_resourcetemplate_footer();
	acpigen_pop_len();
	acpigen_pop_len();
}

BOOT_STATE_INIT_ENTRY(BS_DEV_RESOURCES, BS_ON_ENTRY, fintek_hwm_prepare, NULL);
