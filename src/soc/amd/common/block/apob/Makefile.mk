## SPDX-License-Identifier: GPL-2.0-only
ifneq ($(CONFIG_SOC_AMD_COMMON_BLOCK_APOB_NV_DISABLE),y)
romstage-$(CONFIG_SOC_AMD_COMMON_BLOCK_APOB) += apob_cache.c
ramstage-$(CONFIG_SOC_AMD_COMMON_BLOCK_APOB) += apob_cache.c

endif # !CONFIG_SOC_AMD_COMMON_BLOCK_APOB_NV_DISABLE

# First-return experiment retains NV_DISABLE and adds only the cold DRAM getter.
romstage-$(CONFIG_B565D4_NORMAL_FSPM_FIRST_RETURN) += apob_dram_only.c
