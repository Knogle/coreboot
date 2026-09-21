/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef SOUTHBRIDGE_INTEL_I82801JX_THERMAL_INIT_H
#define SOUTHBRIDGE_INTEL_I82801JX_THERMAL_INIT_H

struct device;

/* Initialize the thermal controller through an assigned MMIO BAR. The caller
 * must have completed resource allocation and enabled memory decoding. */
void i82801jx_thermal_init_assigned(struct device *dev);

#endif /* SOUTHBRIDGE_INTEL_I82801JX_THERMAL_INIT_H */
