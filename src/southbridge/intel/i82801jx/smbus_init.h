/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef SOUTHBRIDGE_INTEL_I82801JX_SMBUS_INIT_H
#define SOUTHBRIDGE_INTEL_I82801JX_SMBUS_INIT_H

struct device;

/* Enable only the documented SMBus clock-gating bits. The caller owns PCI
 * resource admission and SMBus child discovery. */
void i82801jx_smbus_init(struct device *dev);

#endif /* SOUTHBRIDGE_INTEL_I82801JX_SMBUS_INIT_H */
