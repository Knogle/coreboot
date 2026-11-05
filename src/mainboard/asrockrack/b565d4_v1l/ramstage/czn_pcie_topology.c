/* SPDX-License-Identifier: GPL-2.0-only */
#include "czn_pcie_topology.h"
#include <commonlib/helpers.h>

#define GFX_CONTROL 0x00180001U
#define GPP_CONTROL 0x00180009U
#define HUB_CONTROL 0x00180013U

static const struct dxio_port x16[] = {
	{ 8, 23, 8, 0, 0, 0, 0x09, 0, 0, { 3, 3, 7, 7 }, GFX_CONTROL },
	{ 0, 3, 7, 1, 1, 4, 0x12, 0, 0, { 3, 3, 7, 7 }, GPP_CONTROL },
	{ 4, 7, 2, 1, 2, 5, 0x11, 0, 0, { 3, 3, 7, 7 }, HUB_CONTROL },
};

/* AM4 physical GFX[7:0], GFX[15:12], GFX[11:8], respectively. SMU readback
 * places logical8..15 in the GPP block, not in the three-port GFX block.
 * Keep the existing NBIF map: GFX ports0..2, GPP ports0..6, with GPP/HUB
 * assigned DevFn12/11 as on the qualified five-engine cold boot.
 */
static const struct dxio_port x8x4x4[] = {
	{ 16, 23, 8, 0, 1, 1, 0x0a, 0, 0, { 3, 3, 7, 7 }, GFX_CONTROL },
	{ 8, 11, 8, 1, 2, 5, 0x13, 0, 0, { 3, 3, 7, 7 }, GFX_CONTROL },
	{ 12, 15, 8, 1, 3, 6, 0x14, 0, 0, { 3, 3, 7, 7 }, GFX_CONTROL },
	{ 0, 3, 7, 1, 0, 3, 0x12, 0, 0, { 3, 3, 7, 7 }, GPP_CONTROL },
	{ 4, 7, 2, 1, 1, 4, 0x11, 0, 0, { 3, 3, 7, 7 }, HUB_CONTROL },
};

/* Six-engine allocation readback: SMU block18/Port19 is 0:2, 0:3,
 * 1:1, 1:2, 0:0, 0:1. Source wrapper numbering is the inverse of block18.
 */
static const struct dxio_port four_x4[] = {
	{ 8, 11, 8, 1, 2, 5, 0x13, 0, 0, { 3, 3, 7, 7 }, GFX_CONTROL },
	{ 12, 15, 8, 1, 3, 6, 0x14, 0, 0, { 3, 3, 7, 7 }, GFX_CONTROL },
	{ 16, 19, 8, 0, 1, 1, 0x0a, 0, 0, { 3, 3, 7, 7 }, GFX_CONTROL },
	{ 20, 23, 8, 0, 2, 2, 0x0b, 0, 0, { 3, 3, 7, 7 }, GFX_CONTROL },
	{ 0, 3, 7, 1, 0, 3, 0x12, 0, 0, { 3, 3, 7, 7 }, GPP_CONTROL },
	{ 4, 7, 2, 1, 1, 4, 0x11, 0, 0, { 3, 3, 7, 7 }, HUB_CONTROL },
};

static const struct dxio_port four_x4_probe[] = {
	{ 8, 11, 8, 0, 0, 0, 0x09, 0, 0, { 3, 3, 7, 7 }, GFX_CONTROL },
	{ 12, 15, 8, 0, 1, 1, 0x0a, 0, 0, { 3, 3, 7, 7 }, GFX_CONTROL },
	{ 16, 19, 8, 0, 2, 2, 0x0b, 0, 0, { 3, 3, 7, 7 }, GFX_CONTROL },
	/* Carry an existing spare root BDF. Mapping remains deliberately
	 * unselected: the allocation probe stops before any host map write.
	 */
	{ 20, 23, 8, 0, 3, 0xff, 0x13, 0, 0, { 3, 3, 7, 7 }, GFX_CONTROL },
	{ 0, 3, 7, 1, 1, 4, 0x12, 0, 0, { 3, 3, 7, 7 }, GPP_CONTROL },
	{ 4, 7, 2, 1, 2, 5, 0x11, 0, 0, { 3, 3, 7, 7 }, HUB_CONTROL },
};

size_t czn_pcie_ports(enum czn_pcie_topology topology, struct dxio_port *ports,
	size_t capacity)
{
	const struct dxio_port *source;
	size_t count;

	switch (topology) {
	case CZN_PCIE_X16:
		source = x16;
		count = ARRAY_SIZE(x16);
		break;
	case CZN_PCIE_X8X4X4:
		source = x8x4x4;
		count = ARRAY_SIZE(x8x4x4);
		break;
	case CZN_PCIE_FOUR_X4_PROBE:
		source = four_x4_probe;
		count = ARRAY_SIZE(four_x4_probe);
		break;
	case CZN_PCIE_FOUR_X4:
		source = four_x4;
		count = ARRAY_SIZE(four_x4);
		break;
	default:
		return 0;
	}
	if (!ports || capacity < count)
		return 0;
	for (size_t i = 0; i < count; i++)
		ports[i] = source[i];
	return count;
}

static void put32(uint8_t *p, uint32_t value)
{
	for (unsigned int i = 0; i < 4; i++)
		p[i] = value >> (8 * i);
}

size_t czn_pcie_engine(uint8_t *engine, size_t capacity,
	const struct dxio_port *ports, size_t count)
{
	/* Vendor builder allocates a 16-byte header, records and an eight-byte tail. */
	size_t bytes = 24 + count * 40;

	if (!engine || !ports || !count || count > CZN_MAX_PORTS || capacity < bytes)
		return 0;
	for (size_t i = 0; i < count; i++) {
		if (ports[i].first > ports[i].last || ports[i].last > 23 ||
		    !((ports[i].devfn >= 0x09 && ports[i].devfn <= 0x0b) ||
		      (ports[i].devfn >= 0x11 && ports[i].devfn <= 0x17)))
			return 0;
		for (size_t j = 0; j < i; j++)
			if ((ports[i].first <= ports[j].last && ports[j].first <= ports[i].last) ||
			    ports[i].devfn == ports[j].devfn)
				return 0;
	}
	for (size_t i = 0; i < bytes; i++)
		engine[i] = 0;
	put32(engine + 12, count);
	for (size_t i = 0; i < count; i++) {
		uint8_t *entry = engine + 16 + 40 * i;

		entry[0] = 1;
		entry[2] = ports[i].first;
		entry[3] = ports[i].last;
		entry[4] = 1;
		entry[5] = ports[i].group;
		put32(entry + 12, ports[i].devfn);
		put32(entry + 16, ports[i].control);
		put32(entry + 20, 7);
	}
	return bytes;
}

static uint32_t get32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
		(uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

bool czn_pcie_x16_ancillary_matches(const uint8_t *data, size_t bytes,
	const uint8_t *original, size_t original_bytes, bool acs)
{
	if (!data || !original || bytes != 352 || original_bytes != 352)
		return false;
	/* Seven documented ACS strap values in each wrapper. All other bits
	 * and every other DWORD must remain identical to the vendor template.
	 */
	for (size_t offset = 0; offset < bytes; offset += 4) {
		uint32_t expected = get32(original + offset);

		if (acs && ((offset >= 44 && offset <= 68) ||
			    (offset >= 232 && offset <= 256)))
			expected = (expected & 0x1fff) | 0x2000;
		if (get32(data + offset) != expected)
			return false;
	}
	return true;
}

size_t czn_pcie_ancillary(uint8_t *ancillary, size_t capacity,
	const struct dxio_port *ports, size_t count,
	const uint8_t *original, size_t original_bytes, bool acs)
{
	unsigned int per_wrapper[2] = { 0, 0 };
	size_t bytes = 244 + 36 * count, pos = 8;

	if (!ancillary || !ports || !original || !count || count > CZN_MAX_PORTS ||
	    capacity < bytes || original_bytes != 352)
		return 0;
	/* Exact retained Cezanne wrapper and port template boundaries. */
	if (get32(original) != 0x00570006 || get32(original + 4) != 0x00560107 ||
	    get32(original + 8) != 0x0001002f || get32(original + 196) != 0x00000026)
		return 0;
	for (size_t i = 0; i < count; i++) {
		if (ports[i].wrapper > 1 || ports[i].port > 7)
			return 0;
		for (size_t j = 0; j < i; j++)
			if (ports[i].wrapper == ports[j].wrapper && ports[i].port == ports[j].port)
				return 0;
		per_wrapper[ports[i].wrapper]++;
	}
	for (size_t i = 0; i < bytes; i++)
		ancillary[i] = 0;
	put32(ancillary, ((bytes / 4 - 1) << 16) | 6);
	put32(ancillary + 4, ((bytes / 4 - 2) << 16) | 0x0107);
	/* Preserve vendor wrapper order and electrical policy. ACS follows the
	 * same-board builder at 09c664bc..09c66509: seven wrapper values become
	 * one while their parameter IDs remain unchanged (PCD 1d3 == 1).
	 */
	for (int wrapper = 1; wrapper >= 0; wrapper--) {
		size_t template = wrapper ? 12 : 200;

		put32(ancillary + pos, (wrapper << 16) | ((116 + 36 * per_wrapper[wrapper]) / 4));
		pos += 4;
		for (size_t k = 0; k < 112; k += 4) {
			uint32_t value = get32(original + template + k);

			if (acs && k >= 0x20 && k <= 0x38)
				value = (value & 0x1fff) | 0x2000;
			put32(ancillary + pos, value);
			pos += 4;
		}
		for (size_t i = 0; i < count; i++) {
			if (ports[i].wrapper != wrapper)
				continue;
			for (size_t k = 0; k < 36; k += 4)
				put32(ancillary + pos + k,
					(get32(original + 312 + k) & 0xfffff807U) | (ports[i].port << 3));
			pos += 36;
		}
	}
	return bytes;
}
