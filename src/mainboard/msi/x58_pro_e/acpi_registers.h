/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_X58_ACPI_H
#define MAINBOARD_MSI_X58_PRO_E_X58_ACPI_H

/*
 * Constants shared by the native C table writers and the board DSDT.  These
 * describe only state already admitted by the X58_TCO/X58_ACPI platform gates.
 */
#define X58_ACPI_STAGE_ID		"X58_ACPI-NATIVE_ACPI"
#define X58_ACPI_POST_FAIL		0x86

#define X58_ACPI_LPC_ID		0x3a168086
#define X58_ACPI_LPC_PMBASE_REG	0x40
#define X58_ACPI_LPC_ACPI_CNTL_REG	0x44
#define X58_ACPI_LPC_GPIO_ROUT_REG	0xb8
#define X58_ACPI_PMBASE_DECODED	0x0501
#define X58_ACPI_CNTL_DECODED	0x80

#define X58_ACPI_PMBASE		0x0500
#define X58_ACPI_PM1_STS		0x00
#define X58_ACPI_PM1_EN		0x02
#define X58_ACPI_PM1_CNT		0x04
#define X58_ACPI_PM_TMR		0x08
#define X58_ACPI_GPE0_STS		0x20
#define X58_ACPI_GPE0_BLK_LEN		0x10
#define X58_ACPI_GPE0_EN_LO		0x28
#define X58_ACPI_GPE0_EN_HI		0x2c
#define X58_ACPI_SMI_EN		0x30
#define X58_ACPI_ALT_GP_SMI_EN	0x38
#define X58_ACPI_UPRWC		0x3c
#define X58_ACPI_SCI_EN		0x0001
#define X58_ACPI_PM1_CNT_TARGET	0x00000001
#define X58_ACPI_SCI_IRQ		9

#define X58_ACPI_SAD_PCIEXBAR_LO	0x50
#define X58_ACPI_SAD_PCIEXBAR_HI	0x54
#define X58_ACPI_SAD_ID		0x2d818086
#define X58_ACPI_PCIEXBAR_VALUE	0xe0000001
#define X58_ACPI_ECAM_BASE		0xe0000000
#define X58_ACPI_ECAM_SIZE		0x10000000
#define X58_ACPI_ECAM_LIMIT		0xefffffff
#define X58_ACPI_ECAM_HOST_ID		0x34058086
#define X58_ACPI_PCI_SEGMENT		0
#define X58_ACPI_PCI_BUS_START	0x00
#define X58_ACPI_PCI_BUS_END		0xff

#define X58_ECAM_ACPI_POST_MCFG_GATE_BEGIN	0x8e
#define X58_ECAM_ACPI_POST_MCFG_GATE_READY	0x8f

#define X58_ACPI_IOAPIC_BASE		0xfec00000
#define X58_ACPI_IOAPIC_SIZE		0x00001000
/* Vendor-observed ID6, checked against every enabled CPU before MADT output.
 * This tests the inherited BSP-only ID1 collision; SCI delivery is unproven.
 */
#define X58_ACPI_IOAPIC_ID_VALUE	0x06000000
#define X58_ACPI_IOAPIC_ID		6
#define X58_ACPI_IOAPIC_VERSION_VALUE	0x00170020
#define X58_ACPI_IOAPIC_REG_ID	0x00
#define X58_ACPI_IOAPIC_REG_VERSION	0x01
#define X58_ACPI_IOAPIC_WINDOW	0x10
#define X58_ACPI_IOAPIC_GSI_BASE	0

#define X58_ACPI_LAPIC_BASE		0xfee00000
#define X58_ACPI_LAPIC_SIZE		0x00001000

#define X58_ACPI_SMBUS_BASE		0x0400
#define X58_ACPI_SMBUS_SIZE		0x0020
#define X58_ACPI_PM_SIZE		0x0080
#define X58_ACPI_GPIO_BASE		0x0580
#define X58_ACPI_GPIO_SIZE		0x0040
#define X58_ACPI_HPET_BASE		0xfed00000
#define X58_ACPI_HPET_SIZE		0x00000400
#define X58_ACPI_RCBA_BASE		0xfed1c000
#define X58_ACPI_RCBA_SIZE		0x00004000
#define X58_ACPI_FLASH_BASE		0xff000000
#define X58_ACPI_FLASH_SIZE		0x01000000

/*
 * ACPI 6.4 section 6.1.12 requires a unique _UID for every device sharing
 * an _HID.  Keep the board's PNP0C02 resource devices stable and distinct;
 * Windows treats a missing or duplicate _UID in this set as ACPI_BIOS_ERROR.
 */
#define X58_ACPI_UID_MRES		0
#define X58_ACPI_UID_PERC		1
#define X58_ACPI_UID_CMOS		2
#define X58_ACPI_UID_FHWM		3

#if X58_ACPI_UID_MRES == X58_ACPI_UID_PERC || \
	X58_ACPI_UID_MRES == X58_ACPI_UID_CMOS || \
	X58_ACPI_UID_MRES == X58_ACPI_UID_FHWM || \
	X58_ACPI_UID_PERC == X58_ACPI_UID_CMOS || \
	X58_ACPI_UID_PERC == X58_ACPI_UID_FHWM || \
	X58_ACPI_UID_CMOS == X58_ACPI_UID_FHWM
#error "MSI X58 Pro-E PNP0C02 _UID values must be unique"
#endif

#define X58_ACPI_PCI_IO_BASE		0x00001000
#define X58_ACPI_PCI_IO_LIMIT		0x0000ffff
#define X58_ACPI_PCI_IO_SIZE		0x0000f000
#define X58_ACPI_PCI_MMIO_BASE	0xc0000000
#define X58_ACPI_PCI_MMIO_LIMIT	0xdfffffff
#define X58_ACPI_PCI_MMIO_SIZE	0x20000000

#endif
