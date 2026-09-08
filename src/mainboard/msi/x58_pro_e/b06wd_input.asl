/* SPDX-License-Identifier: GPL-2.0-only */

/* Exact Fintek LDN5 keyboard resources admitted by B06WD ramstage. */
Device (PS2K)
{
	Name (_HID, EisaId ("PNP0303"))
	Name (_CID, EisaId ("PNP030B"))
	Name (_UID, Zero)
	Name (_STA, 0x0f)
	Name (_CRS, ResourceTemplate ()
	{
		IO (Decode16, 0x0060, 0x0060, 0x01, 0x01)
		IO (Decode16, 0x0064, 0x0064, 0x01, 0x01)
		IRQ (Edge, ActiveHigh, Exclusive) {1}
	})
}
