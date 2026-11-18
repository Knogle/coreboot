/* SPDX-License-Identifier: GPL-2.0-only */

#include <b565d4_cpu.h>
#include <soc/cpu.h>

bool mainboard_amd_native_cpu_identity_valid(uint32_t signature)
{
	return b565d4_is_selected_cpu(signature);
}
