/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_X58_ACPI_USB_ADMIT_H
#define MAINBOARD_MSI_X58_PRO_E_X58_ACPI_USB_ADMIT_H

/*
 * Read-only admission and electrical-state classification immediately before
 * handing the already enumerated ICH10R USB controllers to the payload.
 */
void x58_acpi_usb_admit(void);

#endif
