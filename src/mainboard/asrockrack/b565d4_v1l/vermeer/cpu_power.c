/* SPDX-License-Identifier: GPL-2.0-only */

#include <amdblocks/vermeer_pcc.h>
#include <amdblocks/vermeer_smu_cc6.h>
#include <amdblocks/vermeer_smu_cppc.h>
#include <amdblocks/vermeer_smu_power.h>
#include <amdblocks/vermeer_smu_telemetry.h>
#include <bootstate.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include "powerplay.h"
#include "setup_options.h"

/* Original B565 SmuV11DxeVMR feature definitions. These differ from Cezanne:
 * data calculation, PPT, TDC, thermal, FIT, EDC and CCLK DPM. The board requests
 * a conservative temperature ceiling. The original compiled board PowerPlay
 * defaults are imported before these features; no manual voltage, frequency
 * or PPT/TDC/EDC override is requested.
 */
#define CPU_POWER_FEATURES (BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4) | BIT(5) | BIT(8))

static void setup_cpu_power(void *unused)
{
	struct vermeer_smu_cppc_info cppc_info;
	uint32_t smu_version;
	bool powerplay_ready;

	if (!vermeer_smu_power_qualify(&smu_version))
		return;
	powerplay_ready = b565d4_vermeer_powerplay_prepare();
	if (CONFIG(B565D4_VERMEER_SMU_TELEMETRY) && powerplay_ready)
		vermeer_smu_telemetry_prepare(smu_version);
	if (!vermeer_smu_power_enable(CPU_POWER_FEATURES, CONFIG_B565D4_CPU_TCTL_LIMIT))
		return;
	if (powerplay_ready) {
		if (vermeer_smu_cppc_prepare(smu_version, &cppc_info))
			vermeer_pcc_prepare(cppc_info.information,
					    cppc_info.nominal_mhz,
					    cppc_info.lowest_mhz);
		if (CONFIG(B565D4_VERMEER_CORE_C6) && b565d4_setup_core_c6_enabled())
			vermeer_smu_cc6_enable();
	} else
		printk(BIOS_WARNING, "Vermeer CPPC: CPU topology table unavailable; skipped\n");
}

/* Complete MP and PCI initialization before enabling CPU clock policy, then
 * generate the common AMD hardware P-state tables. No x86 vendor entry call.
 */
BOOT_STATE_INIT_ENTRY(BS_WRITE_TABLES, BS_ON_ENTRY, setup_cpu_power, NULL);
