/* SPDX-License-Identifier: GPL-2.0-only */
/* Retained board DSDT; Native96 FCH SDHC INTA-only routing trial.
 * The genuine domain SSDT supplies PCI0 _CRS/_SEG/_BBN. Its generic _PRT
 * remains absent because get_pci_routing_table stays NULL. The one direct-GSI
 * root entry below is separate from the qualified GNB child tables.
 * Source17 retained PnP ops emit no generic SuperIO AML, so COM2 is static.
 */
/* Reuse only the genuine fixed-PCI template, not the cold SoC AML. */
#include <soc/amd/common/acpi/pci_template.asl>
#include <soc/amd/common/acpi/pci_root.asl>

DefinitionBlock ("", "DSDT", 2, "COREBT", "B565LATE", 1)
{
	Name (\_S5, Package (0x04) { 0x05, Zero, Zero, Zero })

#if CONFIG(B565D4_BOARD_SETUP) || CONFIG(B565D4_DESKTOP_SETUP)
	External (RWA5, IntObj)
	/* Only PM74[29]: S0 fixed-event delivery and S5 wake are distinct paths. */
	OperationRegion (RWPM, SystemMemory, 0xFED80377, One)
	Field (RWPM, ByteAcc, NoLock, Preserve)
	{
		, 5,
		RTWE, 1
	}

	Method (_PTS, 1, Serialized)
	{
		Store (Zero, RTWE)
		If (LEqual (Arg0, 0x05))
		{
			Store (RWA5, RTWE)
		}
	}

	Method (_WAK, 1, Serialized)
	{
		Store (Zero, RTWE)
		Return (Package (0x02) { Zero, Zero })
	}

	Method (_TTS, 1, Serialized)
	{
		If (LEqual (Arg0, Zero))
		{
			Store (Zero, RTWE)
		}
	}
#endif

	Scope (\_SB)
	{
		ROOT_BRIDGE(PCI0)
		Scope (PCI0)
		{
			Name (_UID, Zero)

			/* IOAPIC-only: vendor root device14h INTA is FCH GSI16.
			 * Source27 programs only SD selector97; no other pin/GNB
			 * route or PIC link-device package is advertised in this trial.
			 */
			Name (_PRT, Package (One)
			{
				Package (0x04) { 0x0014FFFF, Zero, Zero, 0x10 }
			})

			/* Actual root ports: SSDT updates STAT and supplies port methods. */
			ACPI_PCI_DEV(GP09, 1, 1)
			ACPI_PCI_DEV(GP0A, 1, 2)
			ACPI_PCI_DEV(GP0B, 1, 3)
#if CONFIG(B565D4_DESKTOP_NATIVE)
			ACPI_PCI_DEV(GP0C, 1, 4)
			ACPI_PCI_DEV(GP0D, 1, 5)
			ACPI_PCI_DEV(GP0E, 1, 6)
			ACPI_PCI_DEV(GP0F, 1, 7)
			ACPI_PCI_DEV(GP19, 3, 1)
			ACPI_PCI_DEV(GP1A, 3, 2)
			ACPI_PCI_DEV(GP1B, 3, 3)
			ACPI_PCI_DEV(GP1C, 3, 4)
			ACPI_PCI_DEV(GP1D, 3, 5)
			ACPI_PCI_DEV(GP1E, 3, 6)
			ACPI_PCI_DEV(GP1F, 3, 7)
			ACPI_PCI_DEV(GP21, 4, 1)
			ACPI_PCI_DEV(GP39, 7, 1)
#endif
			ACPI_PCI_DEV(GP11, 2, 1)
			ACPI_PCI_DEV(GP12, 2, 2)
			ACPI_PCI_DEV(GP13, 2, 3)
			ACPI_PCI_DEV(GP14, 2, 4)
			ACPI_PCI_DEV(GP15, 2, 5)
			ACPI_PCI_DEV(GP16, 2, 6)
			ACPI_PCI_DEV(GP17, 2, 7)
			ACPI_PCI_DEV(GP41, 8, 1)
			ACPI_PCI_DEV(GP42, 8, 2)
			ACPI_PCI_DEV(GP43, 8, 3)

#if !CONFIG(B565D4_DESKTOP_INTX)
			/* Exact Original GPP0/AR10 and GPP3/AR16, child dev0/allfn.
			 * These do not describe the root ports' own INTA routes.
			 */
			Scope (GP09)
			{
				Name (_PRT, Package (0x04)
				{
					Package (0x04) { 0x0000FFFF, Zero, Zero, 0x18 },
					Package (0x04) { 0x0000FFFF, One, Zero, 0x19 },
					Package (0x04) { 0x0000FFFF, 0x02, Zero, 0x1A },
					Package (0x04) { 0x0000FFFF, 0x03, Zero, 0x1B }
				})
			}
			Scope (GP11)
			{
				Name (_PRT, Package (0x04)
				{
					Package (0x04) { 0x0000FFFF, Zero, Zero, 0x24 },
					Package (0x04) { 0x0000FFFF, One, Zero, 0x25 },
					Package (0x04) { 0x0000FFFF, 0x02, Zero, 0x26 },
					Package (0x04) { 0x0000FFFF, 0x03, Zero, 0x27 }
				})
			}

			/* Exact Original GPP4/AR18: ADR00020002, child dev0/allfn.
			 * ROOT12 uses group4/ABCD at14300050; root-own INTA is separate.
			 */
			Scope (GP12)
			{
				Name (_PRT, Package (0x04)
				{
					Package (0x04) { 0x0000FFFF, Zero, Zero, 0x28 },
					Package (0x04) { 0x0000FFFF, One, Zero, 0x29 },
					Package (0x04) { 0x0000FFFF, 0x02, Zero, 0x2A },
					Package (0x04) { 0x0000FFFF, 0x03, Zero, 0x2B }
				})
			}

			/* GP14 retains reset group0/ABCD at SMN14300058. The observed
			 * NVMe port uses this group; do not claim a root-own INTA route.
			 */
			Scope (GP14)
			{
				Name (_PRT, Package (0x04)
				{
					Package (0x04) { 0x0000FFFF, Zero, Zero, 0x18 },
					Package (0x04) { 0x0000FFFF, One, Zero, 0x19 },
					Package (0x04) { 0x0000FFFF, 0x02, Zero, 0x1A },
					Package (0x04) { 0x0000FFFF, 0x03, Zero, 0x1B }
				})
			}

			/* Same-board GP17: group3/CDAB at14300068, GNB GSIbase24.
			 * The new fixed controller resource is initialized before tables.
			 * This child table does not claim root8's own INTA routing.
			 */
			Scope (GP41)
			{
				Name (_PRT, Package (0x04)
				{
					Package (0x04) { 0x0000FFFF, Zero, Zero, 0x26 },
					Package (0x04) { 0x0000FFFF, One, Zero, 0x27 },
					Package (0x04) { 0x0000FFFF, 0x02, Zero, 0x24 },
					Package (0x04) { 0x0000FFFF, 0x03, Zero, 0x25 }
				})
			}

			/* Exact Original GP18/AR31: group2/CDAB; child dev0/allfn. */
			Scope (GP42)
			{
				Name (_PRT, Package (0x04)
				{
					Package (0x04) { 0x0000FFFF, Zero, Zero, 0x22 },
					Package (0x04) { 0x0000FFFF, One, Zero, 0x23 },
					Package (0x04) { 0x0000FFFF, 0x02, Zero, 0x20 },
					Package (0x04) { 0x0000FFFF, 0x03, Zero, 0x21 }
				})
			}

#endif
			Device (LPCB)
			{
				Name (_ADR, 0x00140003)

				Device (RTC0)
				{
					Name (_HID, EisaId ("PNP0B00"))
					Name (_CRS, ResourceTemplate ()
					{
						IRQ (Edge, ActiveHigh, Shared) { 8 }
						IO (Decode16, 0x0070, 0x0070, 1, 2)
					})
				}

				Device (TMR)
				{
					Name (_HID, EisaId ("PNP0100"))
					Name (_CRS, ResourceTemplate ()
					{
						IRQ (Edge, ActiveHigh, Shared) { 0 }
						IO (Decode16, 0x0040, 0x0040, 0, 4)
					})
				}

				Device (HPET)
				{
					Name (_HID, EisaId ("PNP0103"))
					OperationRegion (HPMM, SystemMemory, 0xFED00010, 4)
					Field (HPMM, DWordAcc, NoLock, Preserve)
					{
						, 1,
						LGEN, 1
					}

					Name (CRS0, ResourceTemplate ()
					{
						Memory32Fixed (ReadWrite, 0xFED00000, 0x400)
					})
					Name (CRS1, ResourceTemplate ()
					{
						Memory32Fixed (ReadWrite, 0xFED00000, 0x400)
						IRQ (Edge, ActiveHigh, Shared) { 0 }
						IRQ (Edge, ActiveHigh, Shared) { 8 }
					})

					Method (_CRS, 0, NotSerialized)
					{
						/* Firmware leaves IRQ0/8 to PIT/RTC. If the OS
						 * enables replacement, report the actual HPET mode.
						 * Shared does not permit independent edge sources.
						 */
						If (LGEN)
						{
							Return (CRS1)
						}
						Return (CRS0)
					}
				}

				Device (COM2)
				{
					Name (_HID, EisaId ("PNP0501"))
					Name (_UID, 2)
					Name (_CRS, ResourceTemplate ()
					{
						IO (Decode16, 0x02F8, 0x02F8, 1, 8)
						IRQ (Edge, ActiveHigh, Exclusive) { 3 }
					})
				}
			}
		}
	}
}
