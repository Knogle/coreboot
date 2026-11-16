/* SPDX-License-Identifier: GPL-2.0-only */

#include "usb_policy.h"

#define USB_CONFIG 0x13U
#define USB_BOOT 0x0aU
#define USB_ATTRIBUTES 0x01000001U
#define USB_PHY_ATTRIBUTES 0x01000003U

static void append(struct matisse_usb_request *requests, size_t *count,
		   uint32_t target, uint32_t mask, uint32_t value, uint32_t attributes)
{
	requests[(*count)++] = (struct matisse_usb_request) {
		.message = USB_CONFIG,
		.args = { target, mask, value, attributes, 0, 0 },
	};
}

static uint32_t phy_value(const uint8_t *p)
{
	uint32_t value = ((uint32_t)p[6] << 4) + (p[5] & 3U);

	value = value * 2 + (p[4] & 1U);
	value = value * 8 + (p[3] & 3U);
	value = value * 16 + (p[2] & 15U);
	value = value * 16 + (p[1] & 7U);
	return (value << 12) + (p[0] & 7U);
}

size_t matisse_build_usb_requests(const struct matisse_usb_policy *policy,
				  struct matisse_usb_request *requests, size_t capacity)
{
	struct matisse_usb_policy p;
	size_t count = 0, required;
	uint32_t value;

	if (!policy || !requests)
		return 0;
	p = *policy;
	required = 13 + (p.enabled ? 14U : 0U) + (p.phy_override ? 13U : 0U) -
		(p.skip_port_disable_update ? 2U : 0U);
	if (capacity < required)
		return 0;

	if (p.enabled) {
		append(requests, &count, 0x16d80118, 0x100, 0x100, USB_ATTRIBUTES);
		append(requests, &count, 0x16d80020, 8, 8, USB_ATTRIBUTES);
		append(requests, &count, 0x16d60008, 4, 4, USB_ATTRIBUTES);
		append(requests, &count, 0x16d2003c, 0x210, 0x3c0, 0x01000002);
		for (unsigned int i = 0; i < 4; i++)
			append(requests, &count, 0x16c0d000 + i * 0x80, 0xff00, 0x2100,
			       USB_PHY_ATTRIBUTES);
		append(requests, &count, 0x16c0c12c, 0x40000, 0, USB_PHY_ATTRIBUTES);
		for (unsigned int i = 0; i < 4; i++)
			append(requests, &count, 0x16c0d020 + i * 0x80, 0x0e000000,
			       0x0e000000, USB_PHY_ATTRIBUTES);
		append(requests, &count, 0x16c0c60c, 0x10000, 0x10000, USB_PHY_ATTRIBUTES);
	}

	if (!p.skip_port_disable_update) {
		/* The original keeps port zero enabled and disables absent USB2 4/5. */
		value = ((p.disable_usb3 & 0xeU) << 16) | (p.disable_usb2 & 0x3eU) | 0x30;
		append(requests, &count, 0x16d80128, 0x000f003f, value, USB_ATTRIBUTES);
		append(requests, &count, 0x16d80130, 15, 4, USB_ATTRIBUTES);
	}
	append(requests, &count, 0x16d80120, 0xffffffff, p.overcurrent_map, USB_ATTRIBUTES);
	append(requests, &count, 0x16d80124, 255, p.overcurrent_map_ext, USB_ATTRIBUTES);
	append(requests, &count, 0x16d80114, 0x000f003f,
	       p.removable_mask & 0x000f003fU, USB_ATTRIBUTES);
	append(requests, &count, 0x16d80130, 0x100, p.ras ? 0x100U : 0U, USB_ATTRIBUTES);

	if (p.phy_override) {
		for (unsigned int i = 0; i < MATISSE_USB_PHY_PORTS; i++) {
			uint32_t target = 0x16d1c000 + i * 0x400;

			append(requests, &count, target, 0xf3bf7007, phy_value(p.phy[i]),
			       USB_ATTRIBUTES);
			append(requests, &count, target + 4, 15,
			       (p.phy[i][7] & 3U) + ((p.phy[i][8] & 3U) << 2), USB_ATTRIBUTES);
		}
		value = p.oem_disable & 0x3fU;
		value |= (value & 15U) << 16;
		append(requests, &count, 0x16d80114, 0xffffffff, value, USB_ATTRIBUTES);
	}

	value = p.force_gen1 & 15U;
	append(requests, &count, 0x16d80130, 0x000f0000, value << 16, USB_ATTRIBUTES);
	for (unsigned int i = 0; i < 4; i++)
		append(requests, &count, 0x16c0d024 + i * 0x80, 0x400,
		       (value & (1U << i)) ? 0x400U : 0U, USB_PHY_ATTRIBUTES);
	append(requests, &count, 0, 0, 0, 0xc0000001);
	requests[count++] = (struct matisse_usb_request) {
		.message = USB_BOOT,
		.args = { 0, 0, 0, 0, 0, 0 },
	};
	return count;
}
