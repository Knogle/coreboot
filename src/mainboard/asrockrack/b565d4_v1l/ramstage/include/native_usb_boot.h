/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NATIVE_USB_BOOT_H
#define NATIVE_USB_BOOT_H

/* Early-native entry2 only, once before DXIO. Failure halts; never retries. */
void native_usb_boot_prepare(void);

#endif /* NATIVE_USB_BOOT_H */
