## SPDX-License-Identifier: GPL-2.0-only

board-dir := $(src)/mainboard/$(MAINBOARDDIR)
board-obj := $(obj)/mainboard/$(MAINBOARDDIR)
CPPFLAGS_common += -I$(board-dir)/ramstage/include
ifeq ($(CONFIG_B565D4_DESKTOP_NATIVE),y)
CPPFLAGS_common := -I$(board-dir)/vermeer/include $(CPPFLAGS_common)
ramstage-c-ccopts += -I$(board-dir)/vermeer
bootblock-y += vermeer/early_fch.c
LDFLAGS_bootblock += --wrap=write_resume_eip
endif

bootblock-$(CONFIG_SOC_AMD_CEZANNE_NATIVE_CPU_IDENTITY_PROVIDER) += vermeer/cpu_identity.c
romstage-$(CONFIG_SOC_AMD_CEZANNE_NATIVE_CPU_IDENTITY_PROVIDER) += vermeer/cpu_identity.c
ramstage-$(CONFIG_SOC_AMD_CEZANNE_NATIVE_CPU_IDENTITY_PROVIDER) += vermeer/cpu_identity.c

bootblock-y += dram_first/bootblock.c dram_first/cpu_guard.S
bootblock-y += normal_uart2.c
bootblock-y += dram_first/normal_reset_eax.S dram_first/normal_pre_soc_uart.c
bootblock-$(CONFIG_B565D4_MATISSE_PRE_SOC_IRQ_DIAGNOSTIC) += dram_first/matisse_early_irq.S
bootblock-y += dram_first/normal_ram_cbfs.c
bootblock-$(CONFIG_B565D4_PCIE_RUNTIME_OPTIONS) += dram_first/setup_cmos.c
ifeq ($(CONFIG_B565D4_PCIE_RUNTIME_OPTIONS),y)
LDFLAGS_bootblock += --wrap=sanitize_cmos
endif
ifneq ($(filter y,$(CONFIG_B565D4_BOARD_SETUP) $(CONFIG_B565D4_DESKTOP_SETUP)),)
LDFLAGS_ramstage += --wrap=amd_cpu_init --wrap=acpi_add_ivrs_table
# PCIe's default operations reference pciexp_scan_bridge in its own C file,
# which --wrap cannot intercept. Its call to the common PCI bridge scanner
# is external and also covers dynamically discovered chipset slot bridges.
LDFLAGS_ramstage += --wrap=do_pci_scan_bridge
endif
ifeq ($(CONFIG_B565D4_BOARD_SETUP),y)
LDFLAGS_ramstage += --wrap=allocate_resources
endif
LDFLAGS_bootblock += --wrap=bootblock_pre_c_entry --wrap=bootblock_c_entry
LDFLAGS_bootblock += --wrap=uart_init --wrap=boot_device_ro

romstage-y += normal_uart2.c dram_first/normal_romstage_uart.c
romstage-y += dram_first/normal_cold_apob.c dram_first/normal_native99.c
romstage-y += dram_first/plain_memory.c
romstage-y += ramstage/native_smm_memory.c
romstage-y += ramstage/native99_apob_select.c dram_first/native99_context_enter.S
romstage-y += dram_first/native161/profile.c dram_first/native161/stages.c
romstage-y += dram_first/native161/loader.c native84_handoff.c
# Vboot supplies SHA-256 through its stage library when selected.
ifneq ($(CONFIG_VBOOT_LIB),y)
romstage-srcs += $(VBOOT_SOURCE)/firmware/2lib/2sha256.c
endif
romstage-c-ccopts += -I$(VBOOT_SOURCE)/firmware/2lib/include
romstage-c-ccopts += -I$(board-dir)/dram_first -I$(board-dir)/dram_first/native161
romstage-c-ccopts += -I$(board-obj)
LDFLAGS_romstage += --wrap=uart_init --wrap=car_stage_entry
LDFLAGS_romstage += --wrap=boot_device_ro --wrap=boot_device_init
LDFLAGS_romstage += --wrap=cbmem_top_chipset --wrap=rmodule_stage_load

ramstage-y += normal_uart2.c
ramstage-$(CONFIG_B565D4_DESKTOP_NATIVE) += dram_first/plain_memory.c
ifeq ($(CONFIG_B565D4_DESKTOP_NATIVE),y)
ramstage-y += vermeer/platform.c vermeer/iohc.c vermeer/desktop_topology.c
ramstage-y += vermeer/cpu.c
ramstage-$(CONFIG_B565D4_MATISSE_PCI_ACCESS_DIAGNOSTIC) += vermeer/pci_diagnostics.c
ramstage-y += vermeer/nbio_base_plan.c
ramstage-y += vermeer/b565d4_policy.c vermeer/b565d4_link_policy.c
ramstage-y += vermeer/b565d4_init_policy.c vermeer/b565d4_dxio.c
else
ramstage-y += platform.c
endif
ramstage-y += ramstage/profile.c ramstage/stages.c ramstage/media.c ramstage/uart.c
ramstage-$(CONFIG_PCI_SCAN_PRE_BRIDGE_HOOK) += ramstage/pci_route_close.c
ramstage-$(CONFIG_PCI_SCAN_VENDOR_ID_HOOK) += ramstage/pci_vendor_probe.c
ramstage-y += ramstage/terminal_providers.c
ramstage-$(CONFIG_B565D4_MATISSE_IRQ_DIAGNOSTIC) += ramstage/matisse_irq_probe.c
ramstage-$(CONFIG_B565D4_MATISSE_IRQ_DIAGNOSTIC) += ramstage/matisse_irq_stubs.S
ramstage-$(CONFIG_B565D4_DESKTOP_NATIVE) += vermeer/matisse_cpu.c
ramstage-$(CONFIG_B565D4_VERMEER) += vermeer/vermeer_cpu.c
ramstage-$(CONFIG_B565D4_VERMEER_MICROCODE_ADDRESS) += vermeer/microcode.c
ramstage-$(CONFIG_B565D4_VERMEER) += vermeer/cpu_power.c
ramstage-$(CONFIG_B565D4_VERMEER) += vermeer/cstates.c
ramstage-$(CONFIG_B565D4_VERMEER) += vermeer/cppc_pcc.c
ramstage-$(CONFIG_B565D4_VERMEER) += vermeer/powerplay.c vermeer/powerplay_contract.c
ramstage-$(CONFIG_B565D4_VERMEER_SMU_TELEMETRY) += vermeer/telemetry.c
ramstage-$(CONFIG_B565D4_DESKTOP_INTX) += vermeer/pci_irq.c
ramstage-$(CONFIG_B565D4_DESKTOP_NATIVE) += vermeer/pcie_link.c
ramstage-$(CONFIG_B565D4_DESKTOP_NATIVE) += vermeer/fch.c
ramstage-y += ramstage/native132_core_launch.c ramstage/native132_reset_stub.S
ifneq ($(CONFIG_B565D4_DESKTOP_NATIVE),y)
ramstage-y += ramstage/cpu_topology.c
endif
ramstage-y += ramstage/native_smm_memory.c
ramstage-y += ramstage/native_apob_resources.c ramstage/native99_apob_select.c
ramstage-y += ramstage/retained_devices.c
ifneq ($(CONFIG_B565D4_DESKTOP_NATIVE),y)
ramstage-y += ramstage/native_df_aperture.c
endif
ifeq ($(CONFIG_B565D4_DESKTOP_NATIVE),y)
ramstage-y += vermeer/promontory.c vermeer/fabric.c
else
ramstage-y += ramstage/native_promontory.c
endif
ifneq ($(CONFIG_B565D4_DESKTOP_NATIVE),y)
ramstage-y += ramstage/czn_dxio.c
endif
ramstage-y += ramstage/czn_dxio_reset.c
ramstage-$(CONFIG_SOC_AMD_CEZANNE) += ramstage/czn_pcie_topology.c
ramstage-y += ramstage/pcie_options.c
ramstage-y += ramstage/setup_options.c
ramstage-$(CONFIG_B565D4_DESKTOP_SETUP) += vermeer/setup_devices.c
ramstage-$(CONFIG_B565D4_DESKTOP_SETUP) += ramstage/setup_devices.c
ramstage-$(CONFIG_B565D4_BOARD_SETUP) += ramstage/setup_devices.c
ramstage-$(CONFIG_B565D4_BOARD_SETUP) += ramstage/setup_resources.c
ramstage-$(CONFIG_B565D4_BOARD_SETUP) += ramstage/setup_watchdog.c
ramstage-$(CONFIG_B565D4_BOARD_SETUP) += ramstage/setup_cpu_power.c
ramstage-$(CONFIG_B565D4_BOARD_SETUP) += ramstage/setup_wake_acpi.c
ramstage-$(CONFIG_B565D4_BOARD_SETUP) += cfr_setup.c
ramstage-$(CONFIG_B565D4_DESKTOP_SETUP) += cfr_setup.c
ramstage-$(CONFIG_B565D4_PCIE_ACS) += ramstage/pcie_acs.c
ramstage-$(CONFIG_B565D4_PCIE_RUNTIME_OPTIONS) += cfr.c
ifneq ($(CONFIG_B565D4_DESKTOP_NATIVE),y)
ramstage-y += ramstage/native_usb_boot.c
endif
ramstage-y += ramstage/retained_payload.c
ramstage-y += ramstage/native253_spi_payload.c ramstage/legacy_shadow_fallback.c
ramstage-y += native84_handoff.c ramstage/native84_parameters.c
ramstage-$(CONFIG_GENERATE_SMBIOS_TABLES) += ramstage/smbios_board.c
ifeq ($(CONFIG_B565D4_VERMEER),y)
ramstage-$(CONFIG_GENERATE_SMBIOS_TABLES) += vermeer/smbios_memory.c
endif
ramstage-c-ccopts += -I$(board-obj)
ramstage-c-ccopts += -I$(src)/soc/amd/common/block/psp
LDFLAGS_ramstage += --wrap=uart_init --wrap=uart_tx_byte --wrap=uart_tx_flush
LDFLAGS_ramstage += --wrap=exception_init --wrap=platform_is_resuming
LDFLAGS_ramstage += --wrap=boot_device_ro --wrap=boot_device_init
LDFLAGS_ramstage += --wrap=main --wrap=cbmem_initialize --wrap=generate_cpu_entries
LDFLAGS_ramstage += --wrap=smbios_write_type4

bootblock-c-ccopts += -mno-sse -mno-sse2 -mno-mmx -msoft-float
romstage-c-ccopts += -mno-sse -mno-sse2 -mno-mmx -msoft-float
ramstage-c-ccopts += -mno-sse -mno-sse2 -mno-mmx -msoft-float

# The PSP loads bootblock and romstage from its compressed reset image.
# The ordinary CBFS intermediate is the later read-only memory medium.
add_bootblock = true

cpu_microcode_a500.bin-file := $(call strip_quotes,$(CONFIG_B565D4_MICROCODE))
cpu_microcode_a500.bin-type := microcode
cbfs-files-y += cpu_microcode_a500.bin

ifeq ($(CONFIG_B565D4_DESKTOP_CPU_PREPARATION),y)
b565d4-desktop-microcode := 8700 8710 a200 a210 a212
$(foreach revision,$(b565d4-desktop-microcode),$(eval \
	cpu_microcode_$(revision).bin-file := $(call strip_quotes,$(CONFIG_B565D4_DESKTOP_MICROCODE_DIR))/cpu_microcode_$(revision).bin))
$(foreach revision,$(b565d4-desktop-microcode),$(eval \
	cpu_microcode_$(revision).bin-type := microcode))
cbfs-files-y += $(addprefix cpu_microcode_,$(addsuffix .bin,$(b565d4-desktop-microcode)))
endif

native42/promontory19.bin-file := $(call strip_quotes,$(CONFIG_B565D4_PROMONTORY_FIRMWARE))
native42/promontory19.bin-type := raw
native42/promontory19.bin-position := 0x2cc34
cbfs-files-y += native42/promontory19.bin

# SeaBIOS treats an empty, explicitly named ROM as an override for the card ROM.
pci1000,0087.rom-file := $(board-obj)/empty.rom
pci1000,0087.rom-type := raw
cbfs-files-$(CONFIG_B565D4_SKIP_SAS_OPROM) += pci1000,0087.rom

$(board-obj)/empty.rom:
	@mkdir -p $(dir $@)
	@truncate -s 0 $@

# Payload and romstage belong to separate parts of the retained PSP container.
MAINBOARD_CBFS_EXCLUDE := $(CONFIG_CBFS_PREFIX)/payload $(CONFIG_CBFS_PREFIX)/romstage
postinclude-hooks += $$(eval include $$(board-dir)/image.mk)
