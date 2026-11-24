## SPDX-License-Identifier: GPL-2.0-only

ifeq ($(CONFIG_SOC_AMD_COMMON_BLOCK_MATISSE_NBIO),y)

ramstage-y += matisse_dxio_engines.c
ramstage-y += matisse_dxio_lanes.c
ramstage-y += matisse_nbif_map.c
ramstage-y += matisse_nbif_port.c
ramstage-y += matisse_nbio_memory.c

endif

ifeq ($(CONFIG_SOC_AMD_COMMON_BLOCK_MATISSE_DXIO),y)

ramstage-y += matisse_dxio_ancillary.c
ramstage-y += matisse_dxio_configured.c
ramstage-y += matisse_dxio_configured_run.c
ramstage-y += matisse_dxio_dma.c
ramstage-y += matisse_dxio_early_link.c
ramstage-y += matisse_dxio_events.c
ramstage-y += matisse_dxio_frame.c
ramstage-y += matisse_dxio_init.c
ramstage-y += matisse_dxio_init_plan.c
ramstage-y += matisse_dxio_link_controls.c
ramstage-y += matisse_dxio_mapped.c
ramstage-y += matisse_dxio_poll.c
ramstage-y += matisse_dxio_reconfigure.c
ramstage-y += matisse_dxio_result.c
ramstage-y += matisse_dxio_service.c

endif
