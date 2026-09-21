/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_X58_USB_POWER_USB_AUTO_H
#define MAINBOARD_MSI_X58_PRO_E_X58_USB_POWER_USB_AUTO_H

#include <device/pci_type.h>
#include <stdbool.h>
/* Read-only single-controller admission, also usable before GPIO57 release. */
bool x58_usb_power_standard_usb_check(pci_devfn_t dev, bool initialized);

#define X58_USB_POWER_STAGE_ID "X58 USB power"

#endif
