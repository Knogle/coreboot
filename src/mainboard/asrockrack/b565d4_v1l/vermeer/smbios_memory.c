/* SPDX-License-Identifier: GPL-2.0-only */

#include "native_apob_resources.h"
#include <amdblocks/vermeer_apob_spd.h>
#include <amdblocks/smn.h>
#include <bootstate.h>
#include <cbmem.h>
#include <console/console.h>
#include <device/dram/common.h>
#include <device/dram/ddr4.h>
#include <memory_info.h>
#include <smbios.h>
#include <string.h>

#define UMC_BASE 0x50000
#define UMC_CHANNEL_STRIDE 0x100000
#define UMC_SDP_CTRL 0x104
#define UMC_DRAM_CONFIG 0x200
#define UMC_CAP_HI 0xdf4
#define UMC_SDP_INIT (1U << 31)
#define UMC_ECC_ENABLED (1U << 30)

/* The supplier snapshot and these workspaces are serialized on the BSP.
 * Do not put several complete DIMM records on the table writer's stack.
 */
static struct vermeer_apob_spd_cache cache;
static struct dimm_attr_ddr4_st decoded[VERMEER_APOB_MAX_DIMMS];

static bool normalize_spd(uint8_t *spd)
{
	uint8_t normalized[VERMEER_APOB_SPD_BYTES];

	/* This board's supplier cache and independent EEPROM captures agree on
	 * these duplicated manufacturing fields. Do not generalize this repair
	 * to the common APOB format parser or guess other overwritten data.
	 */
	if (spd[2] != 0x0c || memcmp(spd + 80, spd + 325, 4) ||
	    memcmp(spd + 84, spd + 329, 20))
		return false;
	memcpy(normalized, spd, sizeof(normalized));
	memset(normalized + 80, 0, 24);
	if (ddr_crc16(normalized, 126) !=
		((uint16_t)normalized[126] | normalized[127] << 8) ||
	    ddr_crc16(normalized + 128, 126) !=
		((uint16_t)normalized[254] | normalized[255] << 8))
		return false;
	memcpy(spd, normalized, sizeof(normalized));
	return true;
}

static uint16_t configured_speed(unsigned int channel, bool *known, bool *ecc)
{
	const uint32_t base = UMC_BASE + channel * UMC_CHANNEL_STRIDE;
	const uint32_t sdp = smn_read32(base + UMC_SDP_CTRL);
	const uint32_t cap = smn_read32(base + UMC_CAP_HI);
	const uint32_t config = smn_read32(base + UMC_DRAM_CONFIG);
	const unsigned int code = config & 0x7f;

	/* Linux amd64_edac uses SDP init and UMC_CAP_HI.EccEn for enabled ECC.
	 * SPD extension width alone describes capability, not active correction.
	 * Vermeer DDR4 clock encoding follows the original UMC/Memtest decoder.
	 */
	*known = sdp != UINT32_MAX && cap != UINT32_MAX && config != UINT32_MAX &&
		(sdp & UMC_SDP_INIT) && code >= 12 && code <= 72;
	*ecc = *known && (cap & UMC_ECC_ENABLED);
	return *known ? code * 200 / 3 : 0;
}

static uint16_t maximum_speed(const uint8_t *spd)
{
	/* DDR4 MTB125ps and FTB1ps; do not interpret reserved timebase encodings. */
	const int tck_ps = spd[18] * 125 + (int8_t)spd[125];

	if (spd[17] || tck_ps < 625 || tck_ps > 1250)
		return 0;
	return 2000000 / tck_ps;
}

static void prepare_memory_info(void *unused)
{
	const uint8_t *apob;
	uint32_t bytes;
	struct memory_info *info;
	bool known[2] = { false }, ecc[2] = { false };
	uint16_t speed[2] = { 0 };
	unsigned int channels = 0, enabled = 0;
	bool all_known = true;

	if (cbmem_find(CBMEM_ID_MEMINFO))
		return;
	if (native_apob_get_snapshot(&apob, &bytes) != NATIVE_APOB_OK ||
	    !vermeer_apob_decode_spd(apob, bytes, &cache)) {
		printk(BIOS_WARNING, "Vermeer SMBIOS: cached DIMM inventory unavailable\n");
		return;
	}
	/* Validate every DIMM before publishing any inventory. Failure here never
	 * changes the RAM map or adds a boot stop for a working DIMM population.
	 */
	for (unsigned int i = 0; i < cache.count; i++) {
		/* The common decoder explicitly does not calculate 3DS capacity. */
		if (!normalize_spd(cache.dimm[i].spd) || (cache.dimm[i].spd[6] & 0x80) ||
		    spd_decode_ddr4(&decoded[i], cache.dimm[i].spd) != SPD_STATUS_OK ||
		    !decoded[i].size_mb) {
			printk(BIOS_WARNING, "Vermeer SMBIOS: cached DDR4 description unsupported\n");
			return;
		}
		channels |= 1U << cache.dimm[i].channel;
	}
	for (unsigned int channel = 0; channel < 2; channel++) {
		if (!(channels & (1U << channel)))
			continue;
		speed[channel] = configured_speed(channel, &known[channel], &ecc[channel]);
		all_known &= known[channel];
		if (ecc[channel])
			enabled |= 1U << channel;
	}
	info = cbmem_add(CBMEM_ID_MEMINFO, sizeof(*info));
	if (!info) {
		printk(BIOS_WARNING, "Vermeer SMBIOS: memory info allocation failed\n");
		return;
	}
	memset(info, 0, sizeof(*info));
	info->number_of_devices = VERMEER_APOB_MAX_DIMMS;
	info->max_capacity_mib = 128 * 1024;
	info->ecc_type = !all_known || (enabled && enabled != channels) ?
		MEMORY_ARRAY_ECC_UNKNOWN : enabled ? MEMORY_ARRAY_ECC_SINGLE_BIT :
		MEMORY_ARRAY_ECC_NONE;
	/* Describe the two logical channels and both sockets on each channel,
	 * including empty sockets. Do not guess the board's printed A/B labels.
	 */
	for (unsigned int i = 0; i < VERMEER_APOB_MAX_DIMMS; i++) {
		info->dimm[i].channel_num = i / 2;
		info->dimm[i].dimm_num = i % 2;
		info->dimm[i].bank_locator = i / 2;
	}
	for (unsigned int i = 0; i < cache.count; i++) {
		const struct vermeer_apob_dimm_spd *source = &cache.dimm[i];
		const struct dimm_attr_ddr4_st *attr = &decoded[i];
		struct dimm_info *dimm = &info->dimm[source->channel * 2 + source->slot];

		dimm->dimm_size = attr->size_mb;
		dimm->ddr_type = MEMORY_TYPE_DDR4;
		dimm->rank_per_dimm = attr->ranks;
		dimm->channel_num = source->channel;
		dimm->dimm_num = source->slot;
		dimm->mod_type = attr->dimm_type;
		dimm->bus_width = attr->bus_width;
		/* Module supplier, not the separate DRAM-die manufacturer. */
		dimm->mod_id = source->spd[320] | (uint16_t)source->spd[321] << 8;
		memcpy(dimm->serial, attr->serial_number, sizeof(dimm->serial));
		memcpy(dimm->module_part_number, attr->part_number, SPD_DDR4_PART_LEN);
		dimm->vdd_voltage = attr->vdd_voltage;
		dimm->max_speed_mts = maximum_speed(source->spd);
		dimm->configured_speed_mts = speed[source->channel];
		if (!attr->ecc_extension && enabled)
			info->ecc_type = MEMORY_ARRAY_ECC_UNKNOWN;
	}
	info->dimm_cnt = VERMEER_APOB_MAX_DIMMS;
	printk(BIOS_NOTICE, "Vermeer SMBIOS: %u cached DIMMs, ECC type=%u\n",
	       cache.count, info->ecc_type);
}

BOOT_STATE_INIT_ENTRY(BS_WRITE_TABLES, BS_ON_ENTRY, prepare_memory_info, NULL);
