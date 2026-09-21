/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef SOUTHBRIDGE_INTEL_I82801JX_USB_INIT_H
#define SOUTHBRIDGE_INTEL_I82801JX_USB_INIT_H

struct device;

/*
 * The standard EHCI device-init callback: enable PCI bus mastering, preserving
 * every other command bit. The caller owns identity, resources and readiness.
 * This is not i82801jx_ehci_init(), which programs the two controllers' earlier
 * BIOS-required EHCIIR2 fields. No reset, legacy handoff, port access, polling
 * or rollback is performed here; those remain with the caller/payload policy.
 */
void i82801jx_usb_ehci_init(struct device *dev);

/* Standard protected subsystem-ID write; caller admits the requested IDs and
 * original access-control byte. Restores that byte exactly, not just bit 0.
 */
void i82801jx_usb_ehci_set_subsystem(struct device *dev, unsigned int vendor,
	unsigned int device);

/* ICH10 UHCI has no ICH7 config-CA erratum policy. Resource/decode admission
 * belongs to the caller, controller reset and enumeration to the payload.
 */
void i82801jx_usb_uhci_init(struct device *dev);

/* ICH10 16.1.12/13 require separate 16-bit R/WO subsystem writes, unlike the
 * generic PCI dword setter. Caller admits a fresh unconsumed pair of fields.
 */
void i82801jx_usb_uhci_set_subsystem(struct device *dev, unsigned int vendor,
	unsigned int device);

#endif
