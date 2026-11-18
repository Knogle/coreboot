/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef SOC_AMD_CEZANNE_NATIVE_MEMMAP_H
#define SOC_AMD_CEZANNE_NATIVE_MEMMAP_H

struct device;

/* The selected native board must qualify and publish its RAM resource map. */
void mainboard_native_memmap_preflight(struct device *dev, unsigned long index);
void mainboard_native_memmap_publish(struct device *dev, unsigned long *idx);

#endif
