## SPDX-License-Identifier: GPL-2.0-only
smm-$(CONFIG_SOC_AMD_COMMON_BLOCK_SMU) += smu.c
bootblock-$(CONFIG_SOC_AMD_COMMON_BLOCK_SMU) += smu.c
romstage-$(CONFIG_SOC_AMD_COMMON_BLOCK_SMU) += smu.c
ramstage-$(CONFIG_SOC_AMD_COMMON_BLOCK_SMU) += smu.c
ramstage-$(CONFIG_SOC_AMD_COMMON_BLOCK_VERMEER_SMU_TELEMETRY) += vermeer_smu_telemetry.c

smm-$(CONFIG_SOC_AMD_COMMON_BLOCK_SMU_SX_ENTRY) += smu_sx_entry.c
