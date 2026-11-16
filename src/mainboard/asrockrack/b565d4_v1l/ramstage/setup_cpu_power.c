/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/smn.h>
#include <amdblocks/smu.h>
#include <arch/cpuid.h>
#include <bootstate.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <delay.h>
#include <soc/smu.h>
#include <timer.h>
#include "setup_options.h"

/* The target's SmuV12DxeCZN identifies these as CCLK, data calculation,
 * PPT, TDC, thermal, FIT and EDC. The bounded SMU47 target trial established
 * P2 transitions with this subset. Keep fan, fabric, voltage overrides,
 * CPPC, C-state and power-down features outside this initialization.
 */
#define CPU_POWER_FEATURES (BIT(0) | BIT(2) | BIT(3) | BIT(4) | BIT(5) | BIT(6) | BIT(7))
#define QUALIFIED_SMU_VERSION 0x00404700

static bool cpu_power_ready;

bool b565d4_setup_cpu_power_ready(void)
{
	return cpu_power_ready;
}

static void setup_thermal_limit(void)
{
	struct smu_payload payload = { .msg = {CONFIG_B565D4_CPU_TCTL_LIMIT} };

	/* Exact SMU47 message 0x19 takes an integer temperature in degrees C.
	 * Its acknowledgement is not a limit readback. This request follows
	 * feature activation and never changes PPT, fan or voltage settings.
	 */
	if (send_smu_message(SMC_MSG_SET_TCTL_LIMIT, &payload) != CB_SUCCESS) {
		printk(BIOS_WARNING, "Board CPU power: Tctl ceiling request failed\n");
		return;
	}
	printk(BIOS_NOTICE, "Board CPU power: Tctl ceiling request=%uC accepted\n",
	       CONFIG_B565D4_CPU_TCTL_LIMIT);
}

static void setup_cpu_power(void *unused)
{
	struct smu_payload payload = { .msg = {0} };
	struct stopwatch wait;
	uint32_t enabled, missing;

	/* Other CPU families keep their own initialization; never block boot. */
	if (cpuid(1).eax != 0x00a50f00)
		return;
	if (smn_read32(SMN_SMU_MESG_RESP) != 1) {
		printk(BIOS_WARNING, "Board CPU power: SMU mailbox unavailable\n");
		return;
	}
	if (send_smu_message(SMC_MSG_GET_SMU_VERSION, &payload) != CB_SUCCESS ||
	    payload.msg[0] != QUALIFIED_SMU_VERSION) {
		printk(BIOS_WARNING, "Board CPU power: SMU version not qualified\n");
		return;
	}
	payload = (struct smu_payload) { .msg = {0} };
	if (send_smu_message(SMC_MSG_GET_ENABLED_SMU_FEATURES, &payload) != CB_SUCCESS)
		return;
	enabled = payload.msg[0];
	missing = CPU_POWER_FEATURES & ~enabled;
	if (missing) {
		payload = (struct smu_payload) { .msg = {missing} };
		if (send_smu_message(SMC_MSG_ENABLE_SMU_FEATURES, &payload) != CB_SUCCESS) {
			printk(BIOS_WARNING, "Board CPU power: feature activation failed\n");
			return;
		}
		/* Activation is scheduled by the SMU. Poll only the readback;
		 * never repeat the feature-changing request.
		 */
		stopwatch_init_msecs_expire(&wait, 200);
		do {
			payload = (struct smu_payload) { .msg = {0} };
			if (send_smu_message(SMC_MSG_GET_ENABLED_SMU_FEATURES,
					     &payload) != CB_SUCCESS)
				return;
			enabled = payload.msg[0];
			if ((enabled & CPU_POWER_FEATURES) == CPU_POWER_FEATURES)
				break;
			udelay(1000);
		} while (!stopwatch_expired(&wait));
	}
	cpu_power_ready = (enabled & CPU_POWER_FEATURES) == CPU_POWER_FEATURES;
	printk(cpu_power_ready ? BIOS_NOTICE : BIOS_WARNING,
	       "Board CPU power: SMU=%08x features=%08x required=%08x ready=%u\n",
	       QUALIFIED_SMU_VERSION, enabled, (unsigned int)CPU_POWER_FEATURES,
	       cpu_power_ready);
	if (cpu_power_ready)
		setup_thermal_limit();
}

/* CPU initialization and DXIO finish before this BSP callback. The table
 * generator advertises standard hardware P-states only after activation.
 * All DRAM training and signed PSP/ABL firmware remain unchanged.
 */
BOOT_STATE_INIT_ENTRY(BS_WRITE_TABLES, BS_ON_ENTRY, setup_cpu_power, NULL);
