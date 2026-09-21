/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef CPU_INTEL_MODEL_206CX_MCA_DIAGNOSTIC_H
#define CPU_INTEL_MODEL_206CX_MCA_DIAGNOSTIC_H

#include <stdbool.h>

/* Read-only, bounded, BSP/AP-local report; no clearing or exception-policy
 * changes. False means unsupported, nonzero global status, a valid bank, or
 * a changing snapshot. True is only admission under those checks, not proof
 * of initialized MCA, fault-free hardware, or an atomic snapshot.
 */
bool model_206cx_mca_report(const char *phase);

/* Ramstage only: identical read/admission policy, one compact success line.
 * Failure prints the saved full report without sampling the hardware again.
 * Uses an AP-local snapshot; the verbose CAR reporter above is unchanged.
 */
bool model_206cx_mca_report_compact(const char *phase);

#endif
