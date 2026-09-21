/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_ICH10_HDA_H
#define MAINBOARD_MSI_X58_PRO_E_ICH10_HDA_H

struct device;
struct device_operations;

/* Explicit operation table used by every board ICH10 HDA node. */
extern struct device_operations x58_ich10_hda_ops;
void x58_ich10_hda_init(struct device *dev);

#endif /* MAINBOARD_MSI_X58_PRO_E_ICH10_HDA_H */
