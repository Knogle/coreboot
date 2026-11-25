/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/smn.h>
#include <amdblocks/smu.h>
#include <amdblocks/vermeer_smu_power.h>
#include <arch/cpu.h>
#include <console/console.h>
#include <soc/smu.h>

#define QUALIFIED_SMU_VERSION 0x00384c00U

static bool qualification_attempted;
static bool qualified;
static bool enable_attempted;
static bool ready;
static uint32_t version;

bool vermeer_smu_power_qualify(uint32_t *smu_version)
{
	struct smu_payload payload = { .msg = {0} };

	if (!smu_version)
		return false;
	if (qualification_attempted) {
		if (qualified)
			*smu_version = version;
		return qualified;
	}
	qualification_attempted = true;
	if (cpuid_eax(1) != 0x00a20f12 || smn_read32(SMN_SMU_MESG_RESP) != 1) {
		printk(BIOS_WARNING, "Vermeer CPU power: CPU/mailbox not qualified\n");
		return false;
	}
	if (send_smu_message(SMC_MSG_GET_SMU_VERSION, &payload) != CB_SUCCESS ||
	    payload.msg[0] != QUALIFIED_SMU_VERSION) {
		printk(BIOS_WARNING, "Vermeer CPU power: SMU version not qualified\n");
		return false;
	}
	version = payload.msg[0];
	*smu_version = version;
	qualified = true;
	return true;
}

bool vermeer_smu_power_enable(uint32_t features, unsigned int tctl_limit)
{
	struct smu_payload payload;

	if (enable_attempted)
		return ready;
	enable_attempted = true;
	if (!qualified)
		return false;
	/* One request only, including ambiguous failure. A successful response is
	 * an acknowledgement, not an enabled-feature bitmap readback. Actual
	 * hardware P-state and boost behavior require OS qualification.
	 */
	payload = (struct smu_payload) { .msg = {features} };
	if (send_smu_message(SMC_MSG_ENABLE_SMU_FEATURES, &payload) != CB_SUCCESS) {
		printk(BIOS_WARNING, "Vermeer CPU power: feature request failed\n");
		return false;
	}
	printk(BIOS_NOTICE, "Vermeer CPU power: SMU=%08x features=%08x accepted\n",
	       version, features);
	/* Feature activation initializes the thermal controller. The qualified
	 * SMU handler takes integer degrees C and clamps against its existing bound.
	 * Never retry an ambiguous request. A failure leaves power policy unready.
	 */
	payload = (struct smu_payload) { .msg = {tctl_limit} };
	if (send_smu_message(SMC_MSG_SET_TCTL_LIMIT, &payload) != CB_SUCCESS) {
		printk(BIOS_WARNING, "Vermeer CPU power: thermal ceiling request failed\n");
		return false;
	}
	printk(BIOS_NOTICE, "Vermeer CPU power: Tctl ceiling request=%uC accepted\n",
	       tctl_limit);
	ready = true;
	return true;
}

bool vermeer_smu_power_ready(void)
{
	return ready;
}
