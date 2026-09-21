## SPDX-License-Identifier: GPL-2.0-only

romstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_EARLY_CORE) += early_core.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_EHCI_INIT) += ehci_init.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_REQUIRED_FIELDS) += required_fields.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_AHCI_MAP) += sata_ahci_map.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA2_DISABLE) += sata2_disable.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_PORT_ENABLE) += sata_port_enable.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_CLOCK_FIELD) += sata_clock_field.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_LPC_INIT) += lpc.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_INIT) += sata.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_USB_INIT) += usb_ehci.c usb_uhci.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_PCIE_INIT) += pcie.c pcie_setup.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_PCI_BRIDGE_INIT) += pci.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SMBUS_INIT) += smbus.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_THERMAL_INIT) += thermal.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_HDA_INIT) += azalia.c
ifeq ($(CONFIG_SOUTHBRIDGE_INTEL_I82801JX),y)

# A board-owned no-SMM policy already owns these pre-DRAM hooks and ACPI
# tables. It uses the normal ramstage drivers with explicit adaptation.
ifneq ($(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_BOARD_OWNED_NO_SMM),y)
bootblock-y += bootblock.c
bootblock-y += early_core.c

romstage-y += early_init.c
ramstage-y += fadt.c
smm-y += smihandler.c
endif

ifneq ($(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_DIRECT_DEVICE_MODEL),y)
ramstage-y += i82801jx.c
endif
ramstage-y += ../common/pciehp.c

endif
