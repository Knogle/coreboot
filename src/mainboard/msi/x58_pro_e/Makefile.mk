## SPDX-License-Identifier: GPL-2.0-only

# The board has one build graph. Historical Kconfig carriers are deliberately
# absent; source flattening freezes the selected platform policy.
bootblock-y += bootblock.c

romstage-y += romstage.c
romstage-y += fail_closed_memmap.c
romstage-y += vendor_init.c
romstage-y += vendor_init_trampoline.S
romstage-y += rommon_script.c
romstage-y += raminit_handoff.c

ramstage-y += mainboard.c
ramstage-y += full_pci.c
ramstage-y += full_ich10_acpi.c
ramstage-y += hda_verb.c
ramstage-y += ich10_hda.c
ramstage-y += ich10_sata_handoff.c
ramstage-y += hda_recovery.c
ramstage-y += board_health.c
ramstage-y += fintek_hwm.c
ramstage-y += ecam.c
ramstage-y += platform_acpi.c
ramstage-y += ebda.c
ramstage-y += acpi_resources.c
ramstage-y += legacy_io.c
ramstage-y += raminit_handoff.c
ramstage-y += ram_loader.c
ramstage-y += ramstage_rommon.c
ramstage-y += payload.c
ramstage-y += usb_admission.c
ramstage-y += legacy_input.c
ramstage-y += usb_power.c
ramstage-y += irqroute.c
ramstage-y += pci.c

# These four hash-pinned private ranges are one board-specific XIP ABI.  MINIT
# is not a standalone Intel MRC/FSP interface.  The documented release helper
# writes the inputs below the ignored site-local tree; coreboot never downloads
# them.
x58_private_input_args :=
x58_private_input_files :=
x58_private_input_verifier := $(top)/util/x58/scripts/prepare_x58_vendor_init.py

ifeq ($(CONFIG_MSI_X58_PRO_E_VENDOR_INIT_BLOBS),y)
x58_vendor_init_dir := $(call strip_quotes,$(CONFIG_MSI_X58_PRO_E_VENDOR_INIT_DIR))
x58_private_input_args += --verify-dir "$(x58_vendor_init_dir)"
x58_private_input_files += \
	$(x58_vendor_init_dir)/csi-wrapper.bin \
	$(x58_vendor_init_dir)/csi-helper.bin \
	$(x58_vendor_init_dir)/minit.bin \
	$(x58_vendor_init_dir)/csi.bin

cbfs-files-y += x58/csi-wrapper.bin
x58/csi-wrapper.bin-file := $(x58_vendor_init_dir)/csi-wrapper.bin
x58/csi-wrapper.bin-type := raw
x58/csi-wrapper.bin-compression := none
x58/csi-wrapper.bin-position := 0xfffc04e2
x58/csi-wrapper.bin-required := prepared MSI X58 Pro-E CSI wrapper

cbfs-files-y += x58/csi-helper.bin
x58/csi-helper.bin-file := $(x58_vendor_init_dir)/csi-helper.bin
x58/csi-helper.bin-type := raw
x58/csi-helper.bin-compression := none
x58/csi-helper.bin-position := 0xfffc1554
x58/csi-helper.bin-required := prepared MSI X58 Pro-E CSI helper

cbfs-files-y += x58/minit.bin
x58/minit.bin-file := $(x58_vendor_init_dir)/minit.bin
x58/minit.bin-type := raw
x58/minit.bin-compression := none
x58/minit.bin-position := 0xfffc1dc0
x58/minit.bin-required := prepared MSI X58 Pro-E MINIT module

cbfs-files-y += x58/csi.bin
x58/csi.bin-file := $(x58_vendor_init_dir)/csi.bin
x58/csi.bin-type := raw
x58/csi.bin-compression := none
x58/csi.bin-position := 0xfffe6de0
x58/csi.bin-required := prepared MSI X58 Pro-E CSI module
endif

ifneq ($(strip $(x58_private_input_args)),)
PHONY += x58-pro-e-verify-private-inputs
x58-pro-e-verify-private-inputs: $(x58_private_input_files) \
		$(x58_private_input_verifier)
	@printf "    VERIFY     MSI X58 Pro-E private inputs\n"
	@python3 $(x58_private_input_verifier) $(x58_private_input_args)

$(obj)/coreboot.pre: x58-pro-e-verify-private-inputs
endif

# Preserve the selected SeaBIOS knobs as fixed platform policy rather than
# selectable bring-up stages.
$(call add_intermediate, x58_seabios_usb_attach_settle, $(CBFSTOOL))
	@printf "    SeaBIOS    Allow 1000 ms for USB signal attachment\n"
	$(if $(CONFIG_UPDATE_IMAGE),-$(CBFSTOOL) $< remove -n etc/usb-time-sigatt 2>/dev/null)
	$(CBFSTOOL) $< add-int -i 1000 -n etc/usb-time-sigatt

$(call add_intermediate, x58_seabios_physical_vbios, $(CBFSTOOL))
	@printf "    SeaBIOS    Validate the admitted physical VGA ROM checksum\n"
	$(if $(CONFIG_UPDATE_IMAGE),-$(CBFSTOOL) $< remove -n etc/optionroms-checksum 2>/dev/null)
	$(CBFSTOOL) $< add-int -i 1 -n etc/optionroms-checksum
	@printf "    SeaBIOS    Restrict physical PCI option ROM mapping to VGA\n"
	$(if $(CONFIG_UPDATE_IMAGE),-$(CBFSTOOL) $< remove -n etc/pci-optionrom-exec 2>/dev/null)
	$(CBFSTOOL) $< add-int -i 1 -n etc/pci-optionrom-exec
