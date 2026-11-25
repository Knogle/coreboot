## SPDX-License-Identifier: GPL-2.0-only
smm-$(CONFIG_SOC_AMD_COMMON_BLOCK_SMU) += smu.c
bootblock-$(CONFIG_SOC_AMD_COMMON_BLOCK_SMU) += smu.c
romstage-$(CONFIG_SOC_AMD_COMMON_BLOCK_SMU) += smu.c
ramstage-$(CONFIG_SOC_AMD_COMMON_BLOCK_SMU) += smu.c
ramstage-$(CONFIG_SOC_AMD_COMMON_BLOCK_VERMEER_SMU_CC6) += vermeer_smu_cc6.c
ramstage-$(CONFIG_SOC_AMD_COMMON_BLOCK_VERMEER_SMU_CPPC) += vermeer_smu_cppc.c
ramstage-$(CONFIG_SOC_AMD_COMMON_BLOCK_VERMEER_SMU_PCC) += vermeer_smu_pcc.c
ramstage-$(CONFIG_SOC_AMD_COMMON_BLOCK_VERMEER_SMU_POWERPLAY) += vermeer_smu_powerplay.c
ramstage-$(CONFIG_SOC_AMD_COMMON_BLOCK_VERMEER_SMU_TELEMETRY) += vermeer_smu_telemetry.c

smm-$(CONFIG_SOC_AMD_COMMON_BLOCK_SMU_SX_ENTRY) += smu_sx_entry.c
