/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef B565D4_VERMEER_POWERPLAY_CONTRACT_H
#define B565D4_VERMEER_POWERPLAY_CONTRACT_H

#include <stdbool.h>
#include <stdint.h>

#define B565D4_POWERPLAY_BYTES 0x304U
#define B565D4_POWERPLAY_MAX_CPUS 16U
#define B565D4_POWERPLAY_APIC_OFFSET 0x80U
#define B565D4_POWERPLAY_APIC_SLOTS 256U

/* Exact original compiled board defaults and sorted observed APIC IDs. This
 * does not import runtime Setup overrides or the original protocol hook.
 * Failure leaves the output unchanged. No hardware operation occurs here.
 */
bool b565d4_vermeer_powerplay_build(const uint32_t *apic_ids, unsigned int count,
				  uint8_t out[B565D4_POWERPLAY_BYTES]);

#endif
