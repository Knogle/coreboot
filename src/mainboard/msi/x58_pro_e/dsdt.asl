/* SPDX-License-Identifier: GPL-2.0-only */

#include <acpi/acpi.h>
#include "b06wc_acpi.h"

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

	Scope (\_SB)
	{
#if CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT && \
	!CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI
		#include "b06wd_input.asl"
#endif

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
				Return (B06WC_ACPI_ECAM_BASE)
			}

			Name (_CRS, ResourceTemplate ()
			{
				WordBusNumber (ResourceProducer, MinFixed, MaxFixed,
					PosDecode, 0x0000, B06WC_ACPI_PCI_BUS_START,
					B06WC_ACPI_PCI_BUS_END, 0x0000, 0x0100, ,, )

				/* Legacy PCI configuration mechanism 1, a consumer range. */
				IO (Decode16, 0x0cf8, 0x0cf8, 0x01, 0x08, )

#if CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM
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
#else
				DWordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode,
					EntireRange, 0x00000000,
					B06WC_ACPI_PCI_IO_BASE,
					B06WC_ACPI_PCI_IO_LIMIT, 0x00000000,
					B06WC_ACPI_PCI_IO_SIZE, ,,, TypeStatic)
#endif

#if CONFIG_X58_PRO_E_B06WK_ACPI_REPAIR
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
#endif
				DWordMemory (ResourceProducer, PosDecode, MinFixed,
					MaxFixed, NonCacheable, ReadWrite, 0x00000000,
					B06WC_ACPI_PCI_MMIO_BASE,
					B06WC_ACPI_PCI_MMIO_LIMIT, 0x00000000,
					B06WC_ACPI_PCI_MMIO_SIZE, ,,,
					AddressRangeMemory, TypeStatic)
			})

#if CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI
			#include "b06wi_prt.asl"
#endif
		}

		/*
		 * Motherboard-consumed ranges already present in the B06WA/B06WB
		 * exact domain-resource contract.  No device or runtime method is
		 * implied by reserving a decoded range here.
		 */
		Device (MRES)
		{
			Name (_HID, EisaId ("PNP0C02"))
			Name (_UID, Zero)
			Name (_CRS, ResourceTemplate ()
			{
#if CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM
				/* Fintek configuration port and polling COM1: bootblock.c. */
				IO (Decode16, 0x004e, 0x004e, 0x01, 0x02, )
				IO (Decode16, 0x03f8, 0x03f8, 0x01, 0x08, )
				/* B06WD enables KBC decode; no working PS/2 is implied. */
				IO (Decode16, 0x0060, 0x0060, 0x01, 0x01, )
				IO (Decode16, 0x0064, 0x0064, 0x01, 0x01, )
				/* Existing POST and B06WC ELCR ports. */
				IO (Decode16, 0x0080, 0x0080, 0x01, 0x01, )
				IO (Decode16, 0x04d0, 0x04d0, 0x01, 0x02, )
#endif
				IO (Decode16, B06WC_ACPI_SMBUS_BASE,
					B06WC_ACPI_SMBUS_BASE, 0x01,
					B06WC_ACPI_SMBUS_SIZE, )
				IO (Decode16, B06WC_ACPI_PMBASE,
					B06WC_ACPI_PMBASE, 0x01,
					B06WC_ACPI_PM_SIZE, )
				IO (Decode16, B06WC_ACPI_GPIO_BASE,
					B06WC_ACPI_GPIO_BASE, 0x01,
					B06WC_ACPI_GPIO_SIZE, )

				Memory32Fixed (ReadWrite, B06WC_ACPI_ECAM_BASE,
					B06WC_ACPI_ECAM_SIZE, )
				Memory32Fixed (ReadWrite, B06WC_ACPI_IOAPIC_BASE,
					B06WC_ACPI_IOAPIC_SIZE, )
#if !CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM
				/* B06WJ assigns this range to the actual HPET device. */
				Memory32Fixed (ReadWrite, B06WC_ACPI_HPET_BASE,
					B06WC_ACPI_HPET_SIZE, )
#endif
				Memory32Fixed (ReadWrite, B06WC_ACPI_RCBA_BASE,
					B06WC_ACPI_RCBA_SIZE, )
				Memory32Fixed (ReadWrite, B06WC_ACPI_LAPIC_BASE,
					B06WC_ACPI_LAPIC_SIZE, )
				Memory32Fixed (ReadOnly, B06WC_ACPI_FLASH_BASE,
					B06WC_ACPI_FLASH_SIZE, )
			})
		}
	}

#if CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM
	#include "b06wj_platform.asl"
#endif
}
