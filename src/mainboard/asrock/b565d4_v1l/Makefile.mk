# SPDX-License-Identifier: GPL-2.0-only

bootblock-y += bootblock.c

ifeq ($(CONFIG_B565D4_NORMAL_UART2),y)
bootblock-y += normal_uart2.c
romstage-y += normal_uart2.c
ramstage-y += normal_uart2.c
LDFLAGS_bootblock += --wrap=uart_init
LDFLAGS_romstage += --wrap=uart_init
LDFLAGS_ramstage += --wrap=uart_init
endif
ifeq ($(CONFIG_B565D4_C10_PROFILE),y)
bootblock-y += c10/cpu_guard.S
else
bootblock-y += cpu_guard.S
endif
LDFLAGS_bootblock += --wrap=bootblock_pre_c_entry

ifeq ($(CONFIG_B565D4_C10_PROFILE),y)
bootblock-$(CONFIG_B565D4_EARLY_RAW_CONSOLE) += c10/early_probe.c
else
bootblock-$(CONFIG_B565D4_EARLY_RAW_CONSOLE) += early_probe.c
endif
ifeq ($(CONFIG_B565D4_EARLY_RAW_CONSOLE),y)
LDFLAGS_bootblock += --wrap=bootblock_c_entry
endif

ifeq ($(CONFIG_B565D4_C10_PROFILE),y)
bootblock-$(CONFIG_B565D4_PRE_MSR_RAW_CONSOLE) += c10/pre_c_raw.S
ifeq ($(CONFIG_B565D4_PRE_MSR_RAW_CONSOLE),y)
LDFLAGS_bootblock += -T $(src)/mainboard/$(MAINBOARDDIR)/c10/pre_c_raw.ld
endif
bootblock-$(CONFIG_B565D4_EMBEDDED_DIAGNOSTIC_ROMSTAGE) += c10/diagnostic_transition.c
else
bootblock-$(CONFIG_B565D4_PRE_MSR_RAW_CONSOLE) += pre_c_raw.S
ifeq ($(CONFIG_B565D4_PRE_MSR_RAW_CONSOLE),y)
LDFLAGS_bootblock += -T $(src)/mainboard/$(MAINBOARDDIR)/pre_c_raw.ld
endif
endif

ifeq ($(CONFIG_B565D4_VENDOR_STAGE_V1_PROFILE),y)
vendor-stage-v1-dir := $(obj)/vendor-stage-v1
.PHONY: vendor-coreboot-stage-v1
vendor-coreboot-stage-v1: $(vendor-stage-v1-dir)/profile-manifest.json

$(vendor-stage-v1-dir)/profile-manifest.json: $(obj)/cbfs/fallback/bootblock_fixed_data.elf \
		$(wildcard porting/asrock_am4/vendor_stage_v1/diagnostic_romstage/*) \
		$(wildcard porting/asrock_am4/vendor_stage_v1/*)
	python3 porting/asrock_am4/vendor_stage_v1/build_profile.py \
		--config-dir $(obj) --output-dir $(vendor-stage-v1-dir) \
		--cross-prefix $(XGCCPATH)i386-elf-
endif

ifeq ($(CONFIG_B565D4_C10_PROFILE),y)
ifeq ($(CONFIG_B565D4_EMBEDDED_DIAGNOSTIC_ROMSTAGE),y)
c10-profile-dir := $(obj)/c10-profile
C10_BASE_ROM ?= blobs-local/asrock-am4/bringup-20261011/read-pair-ef4019/read-1.rom
.PHONY: coreboot-c10
coreboot-c10: $(c10-profile-dir)/profile-manifest.json

$(c10-profile-dir)/profile-manifest.json: $(obj)/cbfs/fallback/bootblock_fixed_data.elf \
		$(wildcard porting/asrock_am4/c10/*) \
		$(wildcard porting/asrock_am4/c10/diagnostic_romstage/*) \
		$(wildcard porting/asrock_am4/c10/scripts/*)
	python3 porting/asrock_am4/c10/build_profile.py \
		--config-dir $(obj) --output-dir $(c10-profile-dir) --base $(C10_BASE_ROM) \
		--cross-prefix $(XGCCPATH)i386-elf-
endif
endif
