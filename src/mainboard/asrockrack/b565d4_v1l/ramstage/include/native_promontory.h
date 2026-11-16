/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NATIVE42_PROMONTORY_H
#define NATIVE42_PROMONTORY_H

/* Future private BSP DEV_INIT-exit trial, after generic resource assignment.
 * This is the Original FIFO kernel only: no PEIM/PCD call, Gen1 switch,
 * port-policy/PHY setup, temporary enumeration, or whole firmware readback.
 * The exact Original blob must first be selected as raw CBFS file
 * native42/promontory19.bin. The present source fragment installs no CBFS file.
 * Existing root/endpoint resources remain owned and retained until cold reset.
 */
void native42_promontory_init(void);

#endif
