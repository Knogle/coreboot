/* SPDX-License-Identifier: GPL-2.0-only */
#include "native84_profile.h"
#include <smbios.h>

int __real_smbios_write_type4(unsigned long *current, int handle);
int __wrap_smbios_write_type4(unsigned long *current, int handle);

/* Preserve genuine CPUID package counts after successful entry2 MP.
 * Retained entry1 still initializes only the BSP. No SPD or DIMM invention.
 */
int __wrap_smbios_write_type4(unsigned long *current, int handle)
{
	struct smbios_type4 *table = (void *)*current;
	int bytes = __real_smbios_write_type4(current, handle);

	if (native84_profile_policy.entry_path != 2) {
		table->core_enabled = 1;
		table->core_enabled2 = 1;
	}
	return bytes;
}
