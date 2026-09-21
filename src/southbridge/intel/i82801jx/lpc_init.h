/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SOUTHBRIDGE_INTEL_I82801JX_LPC_INIT_H
#define SOUTHBRIDGE_INTEL_I82801JX_LPC_INIT_H

#include <stdbool.h>

struct device;

/* Explicit caller owns identity/decode admission, sequencing and readback.
 * The callback must establish the caller's ACPI-mode policy. It must not issue
 * APMC when no SMM handler exists. This API installs no device-model hooks.
 */
void i82801jx_lpc_init_sequence(struct device *dev, bool preserve_cmos,
	void (*set_acpi_mode)(struct device *dev));

/* Describe normal LPC PCI, subtractive, IOAPIC and enabled generic-decode
 * resources. This writes no hardware registers. */
void i82801jx_lpc_read_resources(struct device *dev);

#endif
