/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/smn.h>
#include <amdblocks/smu.h>
#include <arch/cpu.h>
#include <bootstate.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <soc/smu.h>
#include "cppc_pcc.h"
#include "cpu_power.h"
#include "cstates.h"
#include "powerplay.h"
#include "telemetry.h"

/* Original B565 SmuV11DxeVMR feature definitions. These differ from Cezanne:
 * data calculation, PPT, TDC, thermal, FIT, EDC and CCLK DPM. The board requests
 * a conservative temperature ceiling. The original compiled board PowerPlay
 * defaults are imported before these features; no manual voltage, frequency
 * or PPT/TDC/EDC override is requested. CPPC service initialization below is
 * separate from publishing an OS-facing CPPC interface.
 */
#define CPU_POWER_FEATURES (BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4) | BIT(5) | BIT(8))
#define QUALIFIED_SMU_VERSION 0x00384c00

static bool cpu_power_ready;
static bool cpu_power_attempted;

static void prepare_cppc_service(void)
{
	struct smu_payload payload = { .msg = {0} };
	uint32_t before, after, nominal;

	if (send_smu_message(SMC_MSG_GET_CPPC_INFO, &payload) != CB_SUCCESS) {
		printk(BIOS_WARNING, "Vermeer CPPC: initial information request failed\n");
		return;
	}
	before = payload.msg[0];
	if (!before) {
		/* Original SMU384c feature table B208h entry22 initializes the
		 * CPPC service through callback288B8h. Enable this bit once; do not
		 * retry an ambiguous result or replace the existing feature subset.
		 */
		payload = (struct smu_payload) { .msg = {BIT(22)} };
		if (send_smu_message(SMC_MSG_ENABLE_SMU_FEATURES, &payload) != CB_SUCCESS) {
			printk(BIOS_WARNING, "Vermeer CPPC: service feature request failed\n");
			return;
		}
		payload = (struct smu_payload) { .msg = {0} };
		if (send_smu_message(SMC_MSG_GET_CPPC_INFO, &payload) != CB_SUCCESS) {
			printk(BIOS_WARNING, "Vermeer CPPC: information request failed\n");
			return;
		}
	}
	after = payload.msg[0];
	printk(BIOS_NOTICE, "Vermeer CPPC: information %08x -> %08x\n", before, after);
	if (!after || !(after & 3))
		return;

	payload = (struct smu_payload) { .msg = {0} };
	if (send_smu_message(SMC_MSG_GET_CPPC_NOMINAL_FREQUENCY, &payload) != CB_SUCCESS) {
		printk(BIOS_WARNING, "Vermeer CPPC: nominal frequency request failed\n");
		return;
	}
	nominal = payload.msg[0];
	payload = (struct smu_payload) { .msg = {0} };
	if (send_smu_message(SMC_MSG_GET_CPPC_LOWEST_FREQUENCY, &payload) != CB_SUCCESS) {
		printk(BIOS_WARNING, "Vermeer CPPC: lowest frequency request failed\n");
		return;
	}
	printk(BIOS_NOTICE, "Vermeer CPPC: nominal=%uMHz lowest=%uMHz; "
	       "OS interface not published\n", nominal, payload.msg[0]);
	b565d4_vermeer_cppc_pcc_prepare(after, nominal, payload.msg[0]);
	/* GetInfo returns a static capability word after service initialization.
	 * The optional PCC probe checks its private allocation and one finite
	 * read separately. Keep ordinary hardware P-states until CPU indexing,
	 * the ACPI contract and actual OS operation are independently proven.
	 */
}

bool b565d4_vermeer_cpu_power_ready(void)
{
	return cpu_power_ready;
}

static void setup_cpu_power(void *unused)
{
	struct smu_payload payload = { .msg = {0} };
	bool powerplay_ready;

	if (cpu_power_attempted)
		return;
	cpu_power_attempted = true;
	if (cpuid_eax(1) != 0x00a20f12 || smn_read32(SMN_SMU_MESG_RESP) != 1) {
		printk(BIOS_WARNING, "Vermeer CPU power: CPU/mailbox not qualified\n");
		return;
	}
	if (send_smu_message(SMC_MSG_GET_SMU_VERSION, &payload) != CB_SUCCESS ||
	    payload.msg[0] != QUALIFIED_SMU_VERSION) {
		printk(BIOS_WARNING, "Vermeer CPU power: SMU version not qualified\n");
		return;
	}
	powerplay_ready = b565d4_vermeer_powerplay_prepare();
	if (CONFIG(B565D4_VERMEER_SMU_TELEMETRY) && powerplay_ready)
		b565d4_vermeer_telemetry_prepare(QUALIFIED_SMU_VERSION);
	payload = (struct smu_payload) { .msg = {CPU_POWER_FEATURES} };
	/* One request only, including ambiguous failure. A successful response is
	 * an acknowledgement, not an enabled-feature bitmap readback. Actual
	 * hardware P-state and boost behavior require OS qualification.
	 */
	if (send_smu_message(SMC_MSG_ENABLE_SMU_FEATURES, &payload) != CB_SUCCESS) {
		printk(BIOS_WARNING, "Vermeer CPU power: feature request failed\n");
		return;
	}
	printk(BIOS_NOTICE, "Vermeer CPU power: SMU=%08x features=%08x accepted\n",
	       QUALIFIED_SMU_VERSION, (unsigned int)CPU_POWER_FEATURES);
	/* The exact SMU384c BIOS3e handler takes integer degrees C and clamps
	 * against its existing bound. Feature activation initializes the
	 * thermal controller, so request its ceiling afterward. Never retry an
	 * ambiguous request or add a diagnostic boot stop. A failed request skips
	 * P-state advertisement; acknowledgement is not a limit readback.
	 */
	payload = (struct smu_payload) { .msg = {CONFIG_B565D4_CPU_TCTL_LIMIT} };
	if (send_smu_message(SMC_MSG_SET_TCTL_LIMIT, &payload) != CB_SUCCESS) {
		printk(BIOS_WARNING, "Vermeer CPU power: thermal ceiling request failed\n");
		return;
	}
	printk(BIOS_NOTICE, "Vermeer CPU power: Tctl ceiling request=%uC accepted\n",
	       CONFIG_B565D4_CPU_TCTL_LIMIT);
	cpu_power_ready = true;
	if (powerplay_ready) {
		prepare_cppc_service();
		if (CONFIG(B565D4_VERMEER_CORE_C6))
			b565d4_vermeer_cc6_enable();
	} else
		printk(BIOS_WARNING, "Vermeer CPPC: CPU topology table unavailable; skipped\n");
}

/* Complete MP and PCI initialization before enabling CPU clock policy, then
 * generate the common AMD hardware P-state tables. No x86 vendor entry call.
 */
BOOT_STATE_INIT_ENTRY(BS_WRITE_TABLES, BS_ON_ENTRY, setup_cpu_power, NULL);
