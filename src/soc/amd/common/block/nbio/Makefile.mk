## SPDX-License-Identifier: GPL-2.0-only

ifeq ($(CONFIG_SOC_AMD_COMMON_BLOCK_MATISSE_NBIO),y)

ramstage-y += matisse_dxio_engines.c
ramstage-y += matisse_dxio_lanes.c
ramstage-y += matisse_nbif_map.c
ramstage-y += matisse_nbif_port.c
ramstage-y += matisse_nbio_memory.c

endif
