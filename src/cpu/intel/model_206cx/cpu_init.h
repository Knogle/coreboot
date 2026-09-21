/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef CPU_INTEL_MODEL_206CX_H
#define CPU_INTEL_MODEL_206CX_H

#include <stdbool.h>

struct bus;

/* BSP only, once, after the final BSP cache policy and before ACPI tables.
 * Failure halts: an incomplete AP startup is not a safe BSP fallback.
 * smp_requested=false never issues INIT/SIPI to APs.
 * msr2e2_requested=false skips the package command without reading it.
 * Board runtime selections must be frozen before this call.
 */
void model_206cx_init(struct bus *cpu_bus, bool smp_requested,
				 bool msr2e2_requested);

/* Optional BSP pre-CPU diagnostic endpoint, also used around early EBDA setup.
 * Returning still halts the caller. Never invoked after a package write or
 * while APs are starting.
 */
void model_206cx_mca_fault(void);

/* Zero until successful initialization. Index zero is always the BSP.
 * AP indices are startup slots, not fabricated APIC IDs or topology IDs.
 */
unsigned int model_206cx_cpu_count(void);
unsigned int model_206cx_apic_id(unsigned int index);

#endif
