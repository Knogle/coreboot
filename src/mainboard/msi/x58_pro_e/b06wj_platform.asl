/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * B06WJ static OS-facing descriptions. The hardware is initialized by the
 * inherited B06WI path; none of these objects programs a register.
 *
 * CPU identity: acpi_tables.c publishes exactly MADT UID 0 / Local APIC 0.
 * ACPI 6.4, section 8.4 requires the matching ACPI0007 namespace device:
 * https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/08_Processor_Configuration_and_Control/declaring-processors.html
 *
 * LPC/PIC/PIT/RTC resources: MSI vendor-live DSDT captured 2026-09-07,
 * PCI0.SBRG.{PIC,TMR,RTC0}; independently documented by the open
 * src/southbridge/intel/i82801jx/acpi/lpc.asl definitions. Only their fixed
 * canonical resources are used here, without vendor methods or globals.
 * See research/msi/vendor-live-acpi-irq-gpio-2026-09-07.md.
 */

Scope (\_SB)
{
	Device (CP00)
	{
		Name (_HID, "ACPI0007")
		Name (_UID, Zero)
	}

	Device (HPET)
	{
		Name (_HID, EisaId ("PNP0103"))
		Name (_UID, Zero)
		/*
		 * B06WA HPTC=0x80, GCAP=0429b17f:8086a301, size 0x400,
		 * observed again in B06WH-HW-02. As in the coreboot QEMU
		 * acpi/hpet.asl, describe this fixed system timer under _SB.
		 * It is outside our PCI allocation window; no expansion of
		 * that window or PCI BAR is needed. The OS owns timer enabling
		 * and routing; no IRQ is reserved here.
		 */
		Name (_CRS, ResourceTemplate ()
		{
			Memory32Fixed (ReadWrite, B06WC_ACPI_HPET_BASE,
				B06WC_ACPI_HPET_SIZE, )
		})
	}
}

Scope (\_SB.PCI0)
{
	Device (LPCB)
	{
		Name (_ADR, 0x001f0000)

		Device (PIC)
		{
			Name (_HID, EisaId ("PNP0000"))
			Name (_CRS, ResourceTemplate ()
			{
				IO (Decode16, 0x0020, 0x0020, 0x01, 0x02, )
				IO (Decode16, 0x00a0, 0x00a0, 0x01, 0x02, )
				IRQNoFlags () { 2 }
			})
		}

		Device (TIMR)
		{
			Name (_HID, EisaId ("PNP0100"))
			Name (_CRS, ResourceTemplate ()
			{
				IO (Decode16, 0x0040, 0x0040, 0x01, 0x04, )
				/* ISA IRQ0; the B06WI MADT maps it to GSI2. */
				IRQNoFlags () { 0 }
			})
		}

		Device (RTC0)
		{
			Name (_HID, EisaId ("PNP0B00"))
			Name (_CRS, ResourceTemplate ()
			{
				IO (Decode16, 0x0070, 0x0070, 0x01, 0x02, )
				IRQNoFlags () { 8 }
			})
		}
	}
}
