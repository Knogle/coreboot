/* SPDX-License-Identifier: GPL-2.0-only */

/* Direct-GSI routes correlated across two byte-identical vendor snapshots. */
Name (RPRT, Package ()
{
	/*
	 * The target's IOH functions and root ports report PCI interrupt pins,
	 * and MSI's APIC-mode root package maps each of these devices A-D to
	 * GSI 16-19. D13 stays absent because MSI also omits it in APIC mode.
	 */
	Package () { 0x0000ffff, Zero, Zero, 16 },
	Package () { 0x0000ffff, One,  Zero, 17 },
	Package () { 0x0000ffff, 0x02, Zero, 18 },
	Package () { 0x0000ffff, 0x03, Zero, 19 },
	Package () { 0x0001ffff, Zero, Zero, 16 },
	Package () { 0x0001ffff, One,  Zero, 17 },
	Package () { 0x0001ffff, 0x02, Zero, 18 },
	Package () { 0x0001ffff, 0x03, Zero, 19 },
	Package () { 0x0002ffff, Zero, Zero, 16 },
	Package () { 0x0002ffff, One,  Zero, 17 },
	Package () { 0x0002ffff, 0x02, Zero, 18 },
	Package () { 0x0002ffff, 0x03, Zero, 19 },
	Package () { 0x0004ffff, Zero, Zero, 16 },
	Package () { 0x0004ffff, One,  Zero, 17 },
	Package () { 0x0004ffff, 0x02, Zero, 18 },
	Package () { 0x0004ffff, 0x03, Zero, 19 },
	Package () { 0x0005ffff, Zero, Zero, 16 },
	Package () { 0x0005ffff, One,  Zero, 17 },
	Package () { 0x0005ffff, 0x02, Zero, 18 },
	Package () { 0x0005ffff, 0x03, Zero, 19 },
	Package () { 0x0006ffff, Zero, Zero, 16 },
	Package () { 0x0006ffff, One,  Zero, 17 },
	Package () { 0x0006ffff, 0x02, Zero, 18 },
	Package () { 0x0006ffff, 0x03, Zero, 19 },
	Package () { 0x0007ffff, Zero, Zero, 16 },
	Package () { 0x0007ffff, One,  Zero, 17 },
	Package () { 0x0007ffff, 0x02, Zero, 18 },
	Package () { 0x0007ffff, 0x03, Zero, 19 },
	Package () { 0x0008ffff, Zero, Zero, 16 },
	Package () { 0x0008ffff, One,  Zero, 17 },
	Package () { 0x0008ffff, 0x02, Zero, 18 },
	Package () { 0x0008ffff, 0x03, Zero, 19 },
	Package () { 0x0009ffff, Zero, Zero, 16 },
	Package () { 0x0009ffff, One,  Zero, 17 },
	Package () { 0x0009ffff, 0x02, Zero, 18 },
	Package () { 0x0009ffff, 0x03, Zero, 19 },
	Package () { 0x000affff, Zero, Zero, 16 },
	Package () { 0x000affff, One,  Zero, 17 },
	Package () { 0x000affff, 0x02, Zero, 18 },
	Package () { 0x000affff, 0x03, Zero, 19 },
	Package () { 0x0016ffff, Zero, Zero, 16 },
	Package () { 0x0016ffff, One,  Zero, 17 },
	Package () { 0x0016ffff, 0x02, Zero, 18 },
	Package () { 0x0016ffff, 0x03, Zero, 19 },
	/* X58 IOH 00:03.0 itself; INTA is active, keep the vendor rotation. */
	Package () { 0x0003ffff, Zero, Zero, 16 },
	Package () { 0x0003ffff, One,  Zero, 17 },
	Package () { 0x0003ffff, 0x02, Zero, 18 },
	Package () { 0x0003ffff, 0x03, Zero, 19 },
	Package () { 0x001fffff, Zero, Zero, 18 },
	Package () { 0x001fffff, One,  Zero, 19 },
	Package () { 0x001fffff, 0x02, Zero, 18 },
	Package () { 0x001dffff, Zero, Zero, 23 },
	Package () { 0x001dffff, One,  Zero, 19 },
	Package () { 0x001dffff, 0x02, Zero, 18 },
	Package () { 0x001dffff, 0x03, Zero, 16 },
	Package () { 0x001cffff, Zero, Zero, 17 },
	Package () { 0x001cffff, One,  Zero, 16 },
	Package () { 0x001cffff, 0x02, Zero, 18 },
	Package () { 0x001cffff, 0x03, Zero, 19 },
	Package () { 0x001bffff, Zero, Zero, 22 },
	Package () { 0x001affff, Zero, Zero, 16 },
	Package () { 0x001affff, One,  Zero, 21 },
	Package () { 0x001affff, 0x02, Zero, 18 },
	Package () { 0x001affff, 0x03, Zero, 19 }
})

Method (_PRT, 0, NotSerialized)
{
	Return (RPRT)
}

/* IOH root port at 00:03.0; HD5450 functions are below its secondary bus. */
Device (NPE3)
{
	Name (_ADR, 0x00030000)
	Name (_PRT, Package ()
	{
		Package () { 0x0000ffff, Zero, Zero, 16 },
		Package () { 0x0000ffff, One,  Zero, 17 },
		Package () { 0x0000ffff, 0x02, Zero, 18 },
		Package () { 0x0000ffff, 0x03, Zero, 19 }
	})
}

/* ICH10 root port 00:1c.4; the RTL8168 is below its secondary bus. */
Device (P0P8)
{
	Name (_ADR, 0x001c0004)
	Name (_PRT, Package ()
	{
		Package () { 0x0000ffff, Zero, Zero, 16 },
		Package () { 0x0000ffff, One,  Zero, 17 },
		Package () { 0x0000ffff, 0x02, Zero, 18 },
		Package () { 0x0000ffff, 0x03, Zero, 19 }
	})
}
