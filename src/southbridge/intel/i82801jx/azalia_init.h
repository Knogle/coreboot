/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef SOUTHBRIDGE_INTEL_I82801JX_AZALIA_INIT_H
#define SOUTHBRIDGE_INTEL_I82801JX_AZALIA_INIT_H

struct device;

/*
 * Standard ICH10 Azalia controller setup.  An explicit board-owned device
 * operation may call this after normal PCI resource assignment.  This helper
 * deliberately preserves the existing board-selected policy in azalia.c;
 * moving that policy out of the generic driver is a separate transition.
 */
void i82801jx_azalia_init(struct device *dev);

#endif /* SOUTHBRIDGE_INTEL_I82801JX_AZALIA_INIT_H */
