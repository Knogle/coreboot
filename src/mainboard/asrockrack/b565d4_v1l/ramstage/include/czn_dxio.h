/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NATIVE106_CZN_DXIO_H
#define NATIVE106_CZN_DXIO_H
#include <stdint.h>

/* One Root-selected Original-AM4 trial, before generic PCI enumeration.
 * No retry/PEI services; all published storage stays reserved until cold reset.
 */
void native106_dxio_train(void);
int native106_dxio_reset_preflight(void);
int native106_dxio_reset(unsigned int group, unsigned int level);
int native106_dxio_budget_ok(void);
int native106_dxio_delay(unsigned int usec);
void native106_dxio_io(const char *label, uint32_t address,
	uint32_t before, uint32_t after);
#endif
