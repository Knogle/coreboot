/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MATISSE_USB_SERVICE_H
#define MATISSE_USB_SERVICE_H

#include <amdblocks/smu.h>

/* Experimental desktop backend component.
 *
 * These are scalar original Matisse SMU requests, not host SMN stores or
 * Cezanne USB messages. The caller must admit the complete board USB policy,
 * establish the appropriate boot phase and exclusively own SMN/SMU access.
 * Each call submits at most once. A transport error does not undo prior
 * firmware effects and must not trigger an automatic retry.
 */
enum cb_err matisse_usb_config_update(uint32_t target, uint32_t mask,
				      uint32_t value, uint32_t attributes);
enum cb_err matisse_usb_boot(void);

#endif /* MATISSE_USB_SERVICE_H */
