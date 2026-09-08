## SPDX-License-Identifier: GPL-2.0-only

bootblock-y += bootblock.c
romstage-y += romstage.c
romstage-y += fail_closed_memmap.c
romstage-$(CONFIG_X58_PRO_E_B06V0_VENDOR_INIT) += vendor_init.c
romstage-$(CONFIG_X58_PRO_E_B06V0_VENDOR_INIT) += vendor_init_trampoline.S
romstage-$(CONFIG_X58_PRO_E_B06V1_REG_SCRIPT) += rommon_script.c
romstage-$(CONFIG_X58_PRO_E_ROMMON_IRQPROBE_PIT) += rommon_irqprobe.c
romstage-$(CONFIG_X58_PRO_E_ROMMON_IRQPROBE_PIT) += rommon_irqprobe_entry.S
romstage-$(CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF) += b06v6_handoff.c
ramstage-y += mainboard.c
ramstage-$(CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF) += b06v6_handoff.c
ramstage-$(CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF) += ram_loader.c
ramstage-$(CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF) += ramstage_rommon.c
ramstage-$(CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY) += b06vf_payload.c
ramstage-$(CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM) += b06wc_usb_admit.c
ramstage-$(CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT) += b06wd_input.c
ramstage-$(CONFIG_X58_PRO_E_B06WF_USB_LAB) += b06wf_usb_lab.c
ramstage-$(CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS) += b06wg_usb_auto.c
ramstage-$(CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS) += b06wg_usb_auto.c
ramstage-$(CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI) += b06wi_irq_route.c
ifeq ($(CONFIG_X58_PRO_E_B06VN_IOU0_PCIE_PHYS_VBIOS),y)
ramstage-y += b06vn_pci.c
else
ramstage-$(CONFIG_X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE) += b06vm_pci.c
endif

ifneq ($(filter y,$(CONFIG_X58_PRO_E_B06VQ_USB_TRACE1) $(CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE)),)
$(call add_intermediate, b06vq_usbtrace1_attach_settle, $(CBFSTOOL))
	@printf "    SeaBIOS    Allow 1000 ms for USB signal attachment\n"
	$(if $(CONFIG_UPDATE_IMAGE),-$(CBFSTOOL) $< remove -n etc/usb-time-sigatt 2>/dev/null)
	$(CBFSTOOL) $< add-int -i 1000 -n etc/usb-time-sigatt
endif

# The exact GT630 used for B06VM declares a 56,320-byte legacy image in both
# its ROM header and PCIR structure, but those bytes sum to 0xff instead of
# zero. SeaBIOS otherwise rejects the image before its VGA entry point runs.
#
# RunPCIroms=1 still maps and executes that physical VGA ROM and always admits
# the matching CBFS RTL8168 iPXE ROM. It prevents SeaBIOS from sizing the ROM
# BAR of every other visible normal-header X58 function outside B06VM's exact
# write allowlist. Scope both controls to the default-off fixed-card build.
ifeq ($(CONFIG_X58_PRO_E_B06VN_IOU0_PCIE_PHYS_VBIOS),y)
$(call add_intermediate, b06vn_seabios_physical_vbios, $(CBFSTOOL))
	@printf "    SeaBIOS    Validate the B06VN physical AMD VGA ROM checksum\n"
	$(if $(CONFIG_UPDATE_IMAGE),-$(CBFSTOOL) $< remove -n etc/optionroms-checksum 2>/dev/null)
	$(CBFSTOOL) $< add-int -i 1 -n etc/optionroms-checksum
	@printf "    SeaBIOS    Restrict physical PCI option ROM mapping to VGA\n"
	$(if $(CONFIG_UPDATE_IMAGE),-$(CBFSTOOL) $< remove -n etc/pci-optionrom-exec 2>/dev/null)
	$(CBFSTOOL) $< add-int -i 1 -n etc/pci-optionrom-exec
else ifeq ($(CONFIG_X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE),y)
$(call add_intermediate, b06vm_seabios_optionrom_checksum, $(CBFSTOOL))
	@printf "    SeaBIOS    Allow the measured bad-checksum B06VM GT630 ROM\n"
	$(if $(CONFIG_UPDATE_IMAGE),-$(CBFSTOOL) $< remove -n etc/optionroms-checksum 2>/dev/null)
	$(CBFSTOOL) $< add-int -i 0 -n etc/optionroms-checksum
	@printf "    SeaBIOS    Restrict physical PCI option ROM mapping to VGA\n"
	$(if $(CONFIG_UPDATE_IMAGE),-$(CBFSTOOL) $< remove -n etc/pci-optionrom-exec 2>/dev/null)
	$(CBFSTOOL) $< add-int -i 1 -n etc/pci-optionrom-exec
endif
