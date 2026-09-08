/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_B06WF_USB_LAB_H
#define MAINBOARD_MSI_X58_PRO_E_B06WF_USB_LAB_H

#include <stdbool.h>

/*
 * B06WF is a deliberately late, operator-driven USB electrical experiment.
 * The normal B06WC admission path remains read-only; these entry points are
 * reachable only from the B06WF ramstage monitor before ACPI/table creation.
 */
void b06wf_usb_lab_begin(void);
void b06wf_usb_lab_help(void);
void b06wf_usb_lab_lock(void);
bool b06wf_usb_lab_command(int argc, char **argv);
bool b06wf_usb_lab_can_continue(void);

#endif
