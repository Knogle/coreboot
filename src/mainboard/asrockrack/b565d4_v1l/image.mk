## SPDX-License-Identifier: GPL-2.0-only

# Evaluated after the ordinary source and CBFS file classes have been collected.
add_bootblock = true
cbfs-files := $(foreach entry,$(cbfs-files),$(if $(filter $(MAINBOARD_CBFS_EXCLUDE) apu/amdfw_a apu/amdfw_b,$(call extract_nth,2,$(entry))),,$(entry)))
check-ramstage-overlap-files := $(filter-out $(MAINBOARD_CBFS_EXCLUDE),$(check-ramstage-overlap-files))
board-image-tool := $(board-dir)/image.py
board-payload := $(call strip_quotes,$(CONFIG_PAYLOAD_FILE))
board-payload-layout := $(if $(filter y,$(CONFIG_B565D4_LARGE_PAYLOAD)),--large-payload)
board-cpu-contract := $(board-obj)/cpu_firmware.json
board-firmware-template := $(call strip_quotes,$(CONFIG_B565D4_FIRMWARE_TEMPLATE))
board-desktop-microcode := $(if $(filter y,$(CONFIG_B565D4_DESKTOP_CPU_PREPARATION)),--desktop-microcode --desktop-microcode-dir $(CONFIG_B565D4_DESKTOP_MICROCODE_DIR))
board-vermeer-boot-microcode := $(if $(filter y,$(CONFIG_B565D4_VERMEER_BOOT_MICROCODE)),--vermeer-boot-microcode)
board-vermeer-abl-file := $(if $(filter y,$(CONFIG_B565D4_VERMEER_ABL_F20)),$(call strip_quotes,$(CONFIG_B565D4_VERMEER_ABL_FILE)))
board-vermeer-abl := $(if $(board-vermeer-abl-file),--vermeer-abl $(CONFIG_B565D4_VERMEER_ABL_FILE))
board-cmos-options := $(if $(filter y,$(CONFIG_B565D4_PCIE_RUNTIME_OPTIONS)),--cmos-options)
board-native-backend := $(if $(filter y,$(CONFIG_B565D4_MATISSE)),--native-backend matisse,$(if $(filter y,$(CONFIG_B565D4_VERMEER)),--native-backend vermeer))

$(board-cpu-contract): $(board-dir)/firmware_contract.py $(board-firmware-template) $(obj)/config.h
	@mkdir -p $(dir $@)
	python3 $(board-dir)/firmware_contract.py --image $(CONFIG_B565D4_FIRMWARE_TEMPLATE) \
		--output $@

$(board-obj)/bridge.o: $(board-dir)/ramstage/native84_bridge.S $(obj)/config.h
	@mkdir -p $(dir $@)
	$(CC_romstage) $(CPPFLAGS_romstage) $(CFLAGS_romstage) -c $< -o $@

$(board-obj)/bridge.ld: $(board-dir)/ramstage/bridge.ld $(board-dir)/ramstage/include/b565d4_memlayout.h
	@mkdir -p $(dir $@)
	$(CC_romstage) -E -P -undef -x c -I$(board-dir)/ramstage/include $< -o $@

$(board-obj)/bridge.elf: $(board-obj)/bridge.o $(board-obj)/bridge.ld
	$(LD_romstage) -T $(board-obj)/bridge.ld -o $@ $<

$(board-obj)/bridge.bin: $(board-obj)/bridge.elf
	$(OBJCOPY_romstage) -O binary --only-section=.text $< $@

$(board-obj)/bridge_bytes.h: $(board-obj)/bridge.bin $(board-image-tool)
	python3 $(board-image-tool) bridge --input $< --header $@

$(call src-to-obj,romstage,$(board-dir)/dram_first/normal_native99.c): $(board-obj)/bridge_bytes.h

$(board-obj)/payload_contract.h: $(board-payload) $(CBFSTOOL) $(board-image-tool) $(obj)/config.h
	@mkdir -p $(dir $@)
	python3 $(board-image-tool) payload --cbfstool $(CBFSTOOL) --input $< \
		--output $(board-obj)/payload.self --header $@ $(board-payload-layout)

$(call src-to-obj,ramstage,$(board-dir)/ramstage/retained_payload.c): $(board-obj)/payload_contract.h
$(call src-to-obj,ramstage,$(board-dir)/ramstage/native253_spi_payload.c): $(board-obj)/payload_contract.h

ifeq ($(CONFIG_PAYLOAD_SEABIOS),y)
$(call add_intermediate,b565d4-usb-attach-delay,$(CBFSTOOL))
	$(CBFSTOOL) $< add-int -i 2000 -n etc/usb-time-sigatt
endif

# Save the complete later medium before the final container replaces coreboot.pre.
board-medium-dependencies := $(INTERMEDIATE)
$(board-obj)/medium.bin: $(obj)/coreboot.pre $(board-medium-dependencies) $(board-image-tool)
	python3 $(board-image-tool) medium --input $< --output $@

$(board-obj)/ramstage_contract.h: $(board-obj)/medium.bin $(board-image-tool)
	python3 $(board-image-tool) memory --input $< --header $@ --prefix $(CONFIG_CBFS_PREFIX)

$(call src-to-obj,romstage,$(board-dir)/dram_first/normal_native99.c): $(board-obj)/ramstage_contract.h
$(call src-to-obj,romstage,$(board-dir)/dram_first/native161/loader.c): $(board-obj)/ramstage_contract.h

$(call add_intermediate,b565d4-firmware-container,$(objcbfs)/bootblock_fixed_data.elf $(objcbfs)/romstage.elf $(board-obj)/medium.bin $(board-obj)/payload_contract.h $(board-image-tool) $(board-cpu-contract) $(board-vermeer-abl-file))
	python3 $(board-image-tool) pack --template $(CONFIG_B565D4_FIRMWARE_TEMPLATE) \
		--bootblock $(objcbfs)/bootblock_fixed_data.elf --romstage $(objcbfs)/romstage.elf \
		--medium $(board-obj)/medium.bin --payload $(board-obj)/payload.self \
		--fmap $(obj)/fmap.fmap --cbfstool $(CBFSTOOL) --prefix $(CONFIG_CBFS_PREFIX) \
		--output $(obj)/coreboot.pre --report $(board-obj)/image.json $(board-payload-layout) \
		--cpu-contract $(board-cpu-contract) $(board-desktop-microcode) $(board-vermeer-boot-microcode) $(board-vermeer-abl) $(board-cmos-options) $(board-native-backend)
