/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef B565D4_DESKTOP_PLATFORM_H
#define B565D4_DESKTOP_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#if CONFIG(B565D4_MATISSE)
#define B565D4_DESKTOP_NAME "Matisse"
#else
#define B565D4_DESKTOP_NAME "Vermeer"
#endif

bool b565d4_desktop_posted_route(uint64_t begin, uint64_t end);
void b565d4_desktop_fabric_prepare(void);
void b565d4_desktop_iohc_prepare(void);
void b565d4_desktop_ccp_hide(void *unused);
void b565d4_matisse_hide_unused_ports(void);
void b565d4_matisse_promontory_early(void *unused);
void b565d4_matisse_disable_sdhc(void);
bool b565d4_desktop_pch_set_speed(void);
struct device;
void b565d4_desktop_cpu_init(struct device *dev);
uint8_t b565d4_desktop_c51c_value(uint32_t signature, uint8_t before);

#endif
