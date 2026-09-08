## SPDX-License-Identifier: GPL-2.0-only

romstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_EARLY_CORE) += early_core.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_EHCI_INIT) += ehci_init.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_REQUIRED_FIELDS) += required_fields.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_AHCI_MAP) += sata_ahci_map.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA2_DISABLE) += sata2_disable.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_PORT_ENABLE) += sata_port_enable.c
ramstage-$(CONFIG_SOUTHBRIDGE_INTEL_I82801JX_SATA_CLOCK_FIELD) += sata_clock_field.c

ifeq ($(CONFIG_SOUTHBRIDGE_INTEL_I82801JX),y)

bootblock-y += bootblock.c
bootblock-y += early_core.c

romstage-y += early_init.c

ramstage-y += azalia.c
ramstage-y += fadt.c
ramstage-y += i82801jx.c
ramstage-y += lpc.c
ramstage-y += pci.c
ramstage-y += pcie.c
ramstage-y += sata.c
ramstage-y += smbus.c
ramstage-y += thermal.c
ramstage-y += usb_ehci.c
ramstage-y += ../common/pciehp.c

smm-y += smihandler.c

endif
