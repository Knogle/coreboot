/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_B06WD_INPUT_H
#define MAINBOARD_MSI_X58_PRO_E_B06WD_INPUT_H

#include <acpi/acpi.h>

void b06wd_prepare_input(void);
void b06wd_enable_fadt_8042(acpi_fadt_t *fadt);

#endif
