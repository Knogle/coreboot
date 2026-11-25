/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/smu.h>
#include <amdblocks/vermeer_smu_cppc.h>
#include <arch/cpu.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <soc/smu.h>

#define QUALIFIED_SMU_VERSION 0x00384c00U

static bool attempted;
static bool prepared;
static struct vermeer_smu_cppc_info service_info;

bool vermeer_smu_cppc_prepare(uint32_t smu_version,
			      struct vermeer_smu_cppc_info *info)
{
	struct smu_payload payload = { .msg = {0} };
	struct vermeer_smu_cppc_info result;
	uint32_t before;

	if (!info)
		return false;
	if (attempted) {
		if (prepared)
			*info = service_info;
		return prepared;
	}
	attempted = true;
	if (cpuid_eax(1) != 0x00a20f12 || smu_version != QUALIFIED_SMU_VERSION)
		return false;
	if (send_smu_message(SMC_MSG_GET_CPPC_INFO, &payload) != CB_SUCCESS) {
		printk(BIOS_WARNING, "Vermeer CPPC: initial information request failed\n");
		return false;
	}
	before = payload.msg[0];
	if (!before) {
		/* SMU384c feature table entry 22 initializes the CPPC service.
		 * Request it once and never retry an ambiguous result.
		 */
		payload = (struct smu_payload) { .msg = {BIT(22)} };
		if (send_smu_message(SMC_MSG_ENABLE_SMU_FEATURES, &payload) != CB_SUCCESS) {
			printk(BIOS_WARNING, "Vermeer CPPC: service feature request failed\n");
			return false;
		}
		payload = (struct smu_payload) { .msg = {0} };
		if (send_smu_message(SMC_MSG_GET_CPPC_INFO, &payload) != CB_SUCCESS) {
			printk(BIOS_WARNING, "Vermeer CPPC: information request failed\n");
			return false;
		}
	}
	result.information = payload.msg[0];
	printk(BIOS_NOTICE, "Vermeer CPPC: information %08x -> %08x\n",
	       before, result.information);
	if (!result.information || !(result.information & 3))
		return false;
	payload = (struct smu_payload) { .msg = {0} };
	if (send_smu_message(SMC_MSG_GET_CPPC_NOMINAL_FREQUENCY, &payload) != CB_SUCCESS) {
		printk(BIOS_WARNING, "Vermeer CPPC: nominal frequency request failed\n");
		return false;
	}
	result.nominal_mhz = payload.msg[0];
	payload = (struct smu_payload) { .msg = {0} };
	if (send_smu_message(SMC_MSG_GET_CPPC_LOWEST_FREQUENCY, &payload) != CB_SUCCESS) {
		printk(BIOS_WARNING, "Vermeer CPPC: lowest frequency request failed\n");
		return false;
	}
	result.lowest_mhz = payload.msg[0];
	printk(BIOS_NOTICE, "Vermeer CPPC: nominal=%uMHz lowest=%uMHz; "
	       "OS interface not published\n", result.nominal_mhz, result.lowest_mhz);
	service_info = result;
	*info = service_info;
	prepared = true;
	return true;
}
