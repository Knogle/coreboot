/* SPDX-License-Identifier: GPL-2.0-only */

#include "usb_service.h"

enum cb_err matisse_usb_config_update(uint32_t target, uint32_t mask,
				      uint32_t value, uint32_t attributes)
{
	struct smu_payload payload = {
		.msg = { target, mask, value, attributes, 0, 0 },
	};

	return send_smu_message(SMC_MSG_USB_CONFIG_UPDATE, &payload);
}

enum cb_err matisse_usb_boot(void)
{
	struct smu_payload payload = { 0 };

	return send_smu_message(SMC_MSG_USB_BOOT, &payload);
}
