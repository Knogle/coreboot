/* SPDX-License-Identifier: GPL-2.0-only */

#include <acpi/acpi.h>
#include "acpi_registers.h"

DefinitionBlock (
	"dsdt.aml",
	"DSDT",
	ACPI_DSDT_REV_2,
	OEM_ID,
	ACPI_TABLE_CREATOR,
	0x20260906
)
{
	#include <acpi/dsdt_top.asl>

	/* dsdt_top.asl owns PERC; complete its board-wide PNP0C02 identity set. */
	Scope (\_SB.PERC)
	{
		Name (_UID, X58_ACPI_UID_PERC)
	}

	Scope (\_SB)
	{

		Device (PCI0)
		{
			Name (_HID, EisaId ("PNP0A08"))
			Name (_CID, EisaId ("PNP0A03"))
			Name (_UID, Zero)
			Name (_SEG, Zero)
			Name (_BBN, Zero)

			/* The OS may use ECAM; coreboot itself intentionally uses CF8/CFC. */
			Method (_CBA, 0, NotSerialized)
			{
				Return (X58_ACPI_ECAM_BASE)
			}

			Name (_CRS, ResourceTemplate ()
			{
				WordBusNumber (ResourceProducer, MinFixed, MaxFixed,
					PosDecode, 0x0000, X58_ACPI_PCI_BUS_START,
					X58_ACPI_PCI_BUS_END, 0x0000, 0x0100, ,, )

				/* Legacy PCI configuration mechanism 1, a consumer range. */
				IO (Decode16, 0x0cf8, 0x0cf8, 0x01, 0x08, )

				/*
				 * Vendor-live PCI0.CRS (2026-09-07) includes the
				 * low-I/O LPC children and excludes CF8-CFF above.
				 * This describes decode; PCI BAR allocation still
				 * starts at the existing coreboot 0x1000 boundary.
				 */
				WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode,
					EntireRange, 0x0000, 0x0000, 0x0cf7,
					0x0000, 0x0cf8, ,,, TypeStatic, DenseTranslation)
				WordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode,
					EntireRange, 0x0000, 0x0d00, 0xffff,
					0x0000, 0xf300, ,,, TypeStatic, DenseTranslation)

				/*
				 * Legacy VGA framebuffer and option-ROM/shadow decode. These
				 * producer windows match the target's physical VGA path and
				 * the independently generated WJ RAM-lab candidate.
				 */
				DWordMemory (ResourceProducer, PosDecode, MinFixed,
					MaxFixed, NonCacheable, ReadWrite, 0x00000000,
					0x000a0000, 0x000bffff, 0x00000000,
					0x00020000, ,,, AddressRangeMemory, TypeStatic)
				DWordMemory (ResourceProducer, PosDecode, MinFixed,
					MaxFixed, NonCacheable, ReadWrite, 0x00000000,
					0x000c0000, 0x000dffff, 0x00000000,
					0x00020000, ,,, AddressRangeMemory, TypeStatic)
				DWordMemory (ResourceProducer, PosDecode, MinFixed,
					MaxFixed, NonCacheable, ReadWrite, 0x00000000,
					X58_ACPI_PCI_MMIO_BASE,
					X58_ACPI_PCI_MMIO_LIMIT, 0x00000000,
					X58_ACPI_PCI_MMIO_SIZE, ,,,
					AddressRangeMemory, TypeStatic)
			})

			#include "pci_irq.asl"
		}

		/*
		 * Motherboard-consumed ranges already present in the X58_HPET/X58_TCO
		 * exact domain-resource contract.  No device or runtime method is
		 * implied by reserving a decoded range here.
		 */
		Device (MRES)
		{
			Name (_HID, EisaId ("PNP0C02"))
			Name (_UID, X58_ACPI_UID_MRES)
			Name (_CRS, ResourceTemplate ()
			{
				/* Fintek configuration port and polling COM1: bootblock.c. */
				IO (Decode16, 0x004e, 0x004e, 0x01, 0x02, )
				IO (Decode16, 0x03f8, 0x03f8, 0x01, 0x08, )
				/* X58_LEGACY_INPUT enables KBC decode; no working PS/2 is implied. */
				IO (Decode16, 0x0060, 0x0060, 0x01, 0x01, )
				IO (Decode16, 0x0064, 0x0064, 0x01, 0x01, )
				/* Existing POST and X58_ACPI ELCR ports. */
				IO (Decode16, 0x0080, 0x0080, 0x01, 0x01, )
				IO (Decode16, 0x04d0, 0x04d0, 0x01, 0x02, )
				IO (Decode16, X58_ACPI_SMBUS_BASE,
					X58_ACPI_SMBUS_BASE, 0x01,
					X58_ACPI_SMBUS_SIZE, )
				IO (Decode16, X58_ACPI_PMBASE,
					X58_ACPI_PMBASE, 0x01,
					X58_ACPI_PM_SIZE, )
				IO (Decode16, X58_ACPI_GPIO_BASE,
					X58_ACPI_GPIO_BASE, 0x01,
					X58_ACPI_GPIO_SIZE, )

				Memory32Fixed (ReadWrite, X58_ACPI_ECAM_BASE,
					X58_ACPI_ECAM_SIZE, )
				Memory32Fixed (ReadWrite, X58_ACPI_IOAPIC_BASE,
					X58_ACPI_IOAPIC_SIZE, )
				Memory32Fixed (ReadWrite, X58_ACPI_RCBA_BASE,
					X58_ACPI_RCBA_SIZE, )
				Memory32Fixed (ReadWrite, X58_ACPI_LAPIC_BASE,
					X58_ACPI_LAPIC_SIZE, )
				Memory32Fixed (ReadOnly, X58_ACPI_FLASH_BASE,
					X58_ACPI_FLASH_SIZE, )
			})
		}
	}

	#include "platform.asl"
}
