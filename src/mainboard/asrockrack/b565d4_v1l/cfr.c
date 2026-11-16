/* SPDX-License-Identifier: GPL-2.0-only */

#include <boot/coreboot_tables.h>
#include <drivers/option/cfr_frontend.h>
#include "board_config.h"

static const struct sm_object pcie_slot_bifurcation = SM_DECLARE_ENUM({
	.opt_name = OPTION_NAME_PCIE_SLOT_BIFURCATION,
	.ui_name = "CPU x16 slot lane configuration",
	.ui_helptext = "Select the PCIe lane allocation for the CPU x16 slot. "
			"Apply changes with a full power cycle.",
	.default_value = B565D4_PCIE_SLOT_DEFAULT,
	.values = (const struct sm_enum_value[]) {
		{ "Build default", B565D4_PCIE_SLOT_DEFAULT },
		{ "x16", B565D4_PCIE_SLOT_X16 },
#if !CONFIG(B565D4_DESKTOP_NATIVE)
		{ "x8/x4/x4", B565D4_PCIE_SLOT_X8X4X4 },
#endif
		{ "x4/x4/x4/x4", B565D4_PCIE_SLOT_FOUR_X4 },
		SM_ENUM_VALUE_END
	},
});

static struct sm_obj_form pcie = {
	.ui_name = "PCI Express",
	.obj_list = (const struct sm_object *[]) {
		&pcie_slot_bifurcation,
		NULL
	},
};

static struct sm_obj_form *sm_root[] = { &pcie, NULL };

void b565d4_setup_cfr(struct lb_cfr *root, struct sm_obj_form *pcie);

void mb_cfr_setup_menu(struct lb_cfr *cfr_root)
{
	if (CONFIG(B565D4_BOARD_SETUP) || CONFIG(B565D4_DESKTOP_SETUP))
		b565d4_setup_cfr(cfr_root, &pcie);
	else
		cfr_write_setup_menu(cfr_root, sm_root);
}
