/* SPDX-License-Identifier: GPL-2.0-only */

#include <acpi/acpigen.h>
#include "acpi_resources.h"

void x58_platform_resource_ssdt(void)
{
	/*
	 * The native DSDT begins the PCI root resource buffer with a Word Address
	 * Space / bus-number descriptor (ACPI 6.6, 6.4.3.5.3). These module-level
	 * stores run at SSDT load, before OSPM evaluates the root resources.
	 * Offsets 10 and 14 are the 16-bit maximum and length in that first
	 * descriptor, not AML offsets.
	 */
	acpigen_write_scope("\\_SB.PCI0");
	acpigen_write_create_buffer_word_field("_CRS", 10, "XBMX");
	acpigen_write_create_buffer_word_field("_CRS", 14, "XBLN");
	acpigen_write_store_int_to_namestr(X58_PLATFORM_PCI_BUS_END, "XBMX");
	acpigen_write_store_int_to_namestr(X58_PLATFORM_PCI_BUS_END + 1, "XBLN");
	acpigen_pop_len();

	/* A disjoint root for the already-observed CPU uncore functions. No PCI
	 * BAR apertures, interrupt routing, power methods or hardware writes.
	 * MCFG still describes the full physical ECAM decode, buses 00..ff.
	 */
	acpigen_write_scope("\\_SB");
	acpigen_write_device("UCFF");
	acpigen_write_name("_HID");
	acpigen_emit_eisaid("PNP0A03");
	acpigen_write_name_integer("_UID", 1);
	acpigen_write_name_integer("_SEG", 0);
	acpigen_write_name_integer("_BBN", X58_PLATFORM_UNCORE_BUS);
	acpigen_write_name("_CRS");
	acpigen_write_resourcetemplate_header();
	acpigen_emit_byte(0x88); /* Word Address Space descriptor */
	acpigen_emit_word(13);
	acpigen_emit_byte(2); /* Bus number range */
	acpigen_emit_byte(0x0c); /* Producer, positive decode, min/max fixed */
	acpigen_emit_byte(0);
	acpigen_emit_word(0); /* Granularity */
	acpigen_emit_word(X58_PLATFORM_UNCORE_BUS);
	acpigen_emit_word(X58_PLATFORM_UNCORE_BUS);
	acpigen_emit_word(0); /* Translation */
	acpigen_emit_word(1);
	acpigen_write_resourcetemplate_footer();
	acpigen_pop_len();
	acpigen_pop_len();
}
