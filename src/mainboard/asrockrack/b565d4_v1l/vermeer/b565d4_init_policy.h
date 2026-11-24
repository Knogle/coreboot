/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef B565D4_MATISSE_INIT_POLICY_H
#define B565D4_MATISSE_INIT_POLICY_H

#include <amdblocks/matisse_dxio_init_plan.h>
#include <amdblocks/matisse_usb_policy.h>

/* Original compiled PCD/CBS settings expressed as native consumed fields.
 * Outside active Makefiles. The native caller explicitly admits an initial
 * single-root phase; vendor CE/19F phase flags are not imported or replayed.
 * select_enabled/value match the zeroed original CPM complex fields.
 *
 * These constants are a starting policy, not actual later OEM/NVRAM values or
 * evidence that USB/DXIO ran on Matisse. Caller applies deliberate board
 * overrides and owns phase, buffers, transport and physical qualification.
 */
extern const struct matisse_dxio_init_policy b565d4_matisse_dxio_init_policy;
extern const struct matisse_usb_policy b565d4_matisse_usb_policy;

#endif /* B565D4_MATISSE_INIT_POLICY_H */
