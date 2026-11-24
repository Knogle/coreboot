/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_USB_POLICY_H
#define AMD_MATISSE_USB_POLICY_H

#include <types.h>

#define MATISSE_USB_PHY_PORTS 6
#define MATISSE_USB_PHY_PARAMETERS 9
#define MATISSE_USB_MAX_REQUESTS 40

/* Board-selected policy, not an in-memory vendor FCH structure. No pointers
 * into retained firmware or NVRAM are followed. The optional PHY parameters
 * use the qualified Matisse OEM type-7 order; their bit packing is preserved.
 */
struct matisse_usb_policy {
	uint32_t removable_mask;
	uint32_t disable_usb2;
	uint32_t disable_usb3;
	uint32_t overcurrent_map;
	uint8_t overcurrent_map_ext;
	uint8_t force_gen1;
	bool enabled;
	bool skip_port_disable_update;
	bool ras;
	bool phy_override;
	uint8_t phy[MATISSE_USB_PHY_PORTS][MATISSE_USB_PHY_PARAMETERS];
	uint8_t oem_disable;
};

struct matisse_usb_request {
	uint32_t message;
	uint32_t args[6];
};

/* Pure calculation for the desktop backend. Returns the complete
 * request count or zero without output stores for null/insufficient space.
 * Input is copied before output, so overlapping backing is supported. The
 * caller owns initialization order, mailbox serialization and execution.
 */
size_t matisse_build_usb_requests(const struct matisse_usb_policy *policy,
				  struct matisse_usb_request *requests, size_t capacity);

#endif /* AMD_MATISSE_USB_POLICY_H */
