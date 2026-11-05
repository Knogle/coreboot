## SPDX-License-Identifier: GPL-2.0-only
ifeq ($(CONFIG_SOC_AMD_COMMON_BLOCK_SMM),y)

romstage-y += smm_helper.c
postcar-y += smm_helper.c
ramstage-y += finalize.c
ifneq ($(CONFIG_B565D4_RETAINED_PEI_LATE_ENTRY),y)
ramstage-y += smm_relocate.c
endif
ramstage-y += smm_helper.c
smm-y += smi_apmc.c
smm-y += smi_handler.c
smm-y += smi_powerbutton.c
smm-$(CONFIG_SOC_AMD_COMMON_BLOCK_SMIHANDLER) += smi_sleep.c

endif # CONFIG_SOC_AMD_COMMON_BLOCK_SMM
