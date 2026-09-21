/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SOUTHBRIDGE_INTEL_I82801JX_SATA_INIT_H
#define SOUTHBRIDGE_INTEL_I82801JX_SATA_INIT_H

#include <types.h>

struct device;
struct southbridge_intel_i82801jx_config;

/*
 * Shared standard SATA sequence; this API does not install device-model hooks.
 * Mode 0 selects AHCI; any nonzero mode selects native IDE, as in the original
 * full driver. The caller owns identity/reset-state admission, exact resources,
 * configuration lifetime, ordering, one-time execution and post-write checks.
 *
 * enable_mode must precede PCI resource discovery: MAP changes device identity
 * and BAR interpretation. The caller separately owns disabling SATA function 5
 * when all ports are routed to function 2. init_sequence requires assigned,
 * valid I/O BARs (including BMBAR before IOSE, ICH10 datasheet 14.1.3) and an
 * ABAR in AHCI mode, and enables IO, memory and bus-master decoding. It
 * programs IDE decode, PCS (including bit15), clocks, AHCI power-management and
 * R/WO fields, and the original indexed tuning sequence. It must not be combined
 * with another SATA initialization owner or used on a running controller.
 *
 * This preserves the original driver's missing-config/no-ABAR behavior; return
 * from this void sequence is not a hardware readiness result. In particular,
 * callers must reject an absent ABAR before entering rather than relying on
 * the standard memory helper's early return.
 */
void i82801jx_sata_enable_mode(struct device *dev,
	const struct southbridge_intel_i82801jx_config *config, u8 sata_mode);
void i82801jx_sata_init_sequence(struct device *dev,
	const struct southbridge_intel_i82801jx_config *config, u8 sata_mode);

#endif
