/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef B565D4_NORMAL_APOB_GEOMETRY_POLICY_H
#define B565D4_NORMAL_APOB_GEOMETRY_POLICY_H

#include <stdint.h>

/* Root may bind a NEW source successor only after separate exact read grants.
 * These two fixed-width loaded fields remain0 in this SOURCE proposal.
 * OptionalMSR=1 means exactly C001001D/C0010015/C0010111/C0010112/C0010113.
 * FED=1 additionally requires its default-off Kconfig option and exactly one
 * four-byte read at FED80C18. Neither field grants writes or pointer chasing. */
struct cold_geometry_read_policy {
	uint32_t optional_msr_reads_bound;
	uint32_t fed80c18_read_bound;
};
static const volatile struct cold_geometry_read_policy cold_geometry_read_policy
	__attribute__((section(".data.b565_geometry_read_policy"), used)) = { 1, 1 };
_Static_assert(sizeof(cold_geometry_read_policy) == 8, "Fixed read-policy fields");
#endif
