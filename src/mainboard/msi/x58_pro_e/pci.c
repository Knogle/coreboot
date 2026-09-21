/* SPDX-License-Identifier: GPL-2.0-only */

#include "manual_recovery.h"
#include <acpi/acpi.h>
#include "board_health.h"
#include "platform_acpi.h"
#include "legacy_io.h"
#include "../../../cpu/intel/model_206cx/cpu_init.h"

/*
 * X58_PCI is intentionally not a generic X58 PCI implementation.  It admits
 * only the exact standard-header functions observed on X58_MEMORY_PROFILE-HW-02, validates
 * every identity before BAR sizing, and allocates only from two fixed holes.
 */

#include <arch/io.h>
#include <arch/pci_io_cfg.h>
#include <arch/cpu.h>
#include <bootstate.h>
#include <cbmem.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <cpu/x86/lapic.h>
#include <smp/node.h>
#include <delay.h>
#include <device/device.h>
#include <device/mmio.h>
#include <device/pci.h>
#include <device/pci_def.h>
#include <device/pci_ids.h>
#include <device/pci_type.h>
#include <device/resource.h>
#include <pc80/i8259.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <southbridge/intel/common/pmutil.h>
#include <southbridge/intel/i82801jx/i82801jx.h>
#include "ich10_gpe_policy.h"

#include "raminit_handoff.h"
#include "pci.h"
_Static_assert(CONFIG(UDELAY_TSC) &&
	CONFIG(CPU_INTEL_COMMON_TIMEBASE) && CONFIG(TSC_MONOTONIC_TIMER) &&
	CONFIG(HAVE_MONOTONIC_TIMER) && !CONFIG(NO_MONOTONIC_TIMER),
	"ICH10_TIMER requires the standard TSC monotonic timer");
#include "full_pci.h"
#include "full_ich10_acpi.h"
#include <southbridge/intel/i82801jx/board_policy.h>
#include <southbridge/intel/common/lpc_def.h>
_Static_assert(CONFIG(BOOTMEDIA_LOCK_NONE) &&
	CONFIG(NO_SMM) && !CONFIG(HAVE_ACPI_RESUME) && !CONFIG(PXE),
	"ICH10 requires a no-SMM vendor-ACPI recovery or normal profile");
#include <southbridge/intel/i82801jx/pcie_init.h>
#include <southbridge/intel/i82801jx/chip.h>
#include <southbridge/intel/i82801jx/chip.h>
#include <southbridge/intel/i82801jx/lpc_init.h>
#include <southbridge/intel/i82801jx/sata_init.h>
#include <device/pci_ehci.h>
#include <southbridge/intel/i82801jx/usb_init.h>
#include "usb_power.h"
#include "usb_admission.h"
#include "legacy_input.h"
#include "irqroute.h"
#include "acpi_tables.h"

#define X58_PCI_SAD_DEV		PCI_DEV(0xff, 0, 1)
#define X58_PCI_SAD_ID		0x2d818086u
#define X58_PCI_SAD_PCIEXBAR_LO	0x50
#define X58_PCI_SAD_PCIEXBAR_HI	0x54
#define X58_PCI_SAD_RULE_FIRST	0x80
#define X58_PCI_SAD_RULE_COUNT	8
#define X58_PCIEXBAR_LO	0xe0000001u

#define X58_PCI_HOST_DEV	PCI_DEV(0, 0, 0)
#define X58_PCI_HOST_ID	0x34058086u
#define X58_PCI_HOST_CLASSREV	0x06000013u

#define X58_PCI_IOH_HM_DEV	PCI_DEV(0, 0x14, 0)
#define X58_PCI_IOH_HM_ID		0x342e8086u
#define X58_PCI_IOH_TOLM		0xd0
#define X58_PCI_IOH_TOHM_LO	0xd4
#define X58_PCI_IOH_TOHM_HI	0xd8
#define X58_PCI_TOLM_VALUE	0xbc000000u
#define X58_PCI_TOHM_LO_VALUE	0x3c000000u
#define X58_PCI_TOHM_HI_VALUE	0x00000001u

#define X58_PCI_LOW_RAM_BASE	0x00100000ULL
#define X58_PCI_LOW_RAM_TOP	0xc0000000ULL
#define X58_PCI_HIGH_RAM_BASE	0x100000000ULL
#define X58_PCI_HIGH_RAM_TOP	0x140000000ULL
#define X58_PCI_IO_BASE	0x1000u
#define X58_PCI_IO_TOP	0x10000u
#define X58_PCI_MMIO_BASE	0xc0000000ULL
#define X58_PCI_MMIO_TOP	0xe0000000ULL
#define X58_PCI_ECAM_BASE		0xe0000000ULL
#define X58_PCI_ECAM_TOP		0xf0000000ULL

/* Full PCIEXBAR address construction; extended offsets cannot use legacy CF8. */
#define X58_PCI_ECAM_ADDR(bus, dev, fn, reg) \
	(X58_PCI_ECAM_BASE + ((uintptr_t)(bus) << 20) + \
	 ((uintptr_t)(dev) << 15) + ((uintptr_t)(fn) << 12) + (reg))
#define X58_PCI_ECAM_DEV0(dev, reg) X58_PCI_ECAM_ADDR(0, dev, 0, reg)
#define X58_PCI_IOU2_X4_DEV	0x01
#define X58_PCI_IOU0_X16_DEV	0x03
#define X58_PCI_AUX_X16_DEV	0x07
#define X58_PCI_IOU2_X4_ID		0x34088086u
#define X58_PCI_IOU0_X16_ID	0x340a8086u
#define X58_PCI_AUX_X16_ID		0x340e8086u
#define X58_PCIE_LNKCAP	0x09c
#define X58_PCIE_LNKCTLSTA	0x0a0
#define X58_PCIE_LNKSTA	0x0a2
#define X58_PCIE_AA		0x0aa
#define X58_PCIE_C0		0x0c0
#define X58_PCIE_PRTX_BIF_CTRL 0x190
#define X58_PCI_IOU0_X16_START	0x0000000cu
#define X58_PCI_LNKSTA_SPEED_MASK	0x000f
#define X58_PCI_LNKSTA_WIDTH_MASK	0x03f0
#define X58_PCI_LNKSTA_WIDTH_SHIFT 4
#define X58_PCI_LNKSTA_DLL_ACTIVE	BIT(13)
#define X58_PCI_LINK_POLL_US	100
#define X58_PCI_LINK_POLL_COUNT	10000
#define X58_PCI_AMD_VENDOR_ID	0x1002u

/*
 * Intel X58 Datasheet sections 17.6.5.12 and 17.6.5.21/.22/.31/.32.
 * IOHBUSNO.Valid=1 restricts the IOH-internal device/function numbers to the
 * bus in bits 7:0.  The reference board programs bus 0 as exact value 0x0100.
 * The local/global bus-range registers remain read-only telemetry in X58_PCIE_TOPOLOGY.
 */
#define X58_PCIE_TOPOLOGY_IOHBUSNO		0x10a
#define X58_PCIE_TOPOLOGY_IOHBUSNO_BUS0_VALID 0x0100u
#define X58_PCIE_TOPOLOGY_LCFGBUS_BASE	0x11c
#define X58_PCIE_TOPOLOGY_LCFGBUS_LIMIT	0x11d
#define X58_PCIE_TOPOLOGY_GCFGBUS_BASE	0x134
#define X58_PCIE_TOPOLOGY_GCFGBUS_LIMIT	0x135
#define X58_PCIE_TOPOLOGY_ALIAS_DEV		0x0d
#define X58_PCIE_TOPOLOGY_PEG_ENDPOINT_DEV	0x00
#define X58_PCIE_TOPOLOGY_ENDPOINT_SAMPLE_COUNT 4

#define X58_PCI_ROOT_FUNCTIONS	16
#define X58_PCI_DOWNSTREAM_FUNCTIONS 5
#define X58_PCI_BRIDGE_FUNCTIONS 7
#define X58_PCI_EXPECTED_FUNCTIONS	(X58_PCI_ROOT_FUNCTIONS + \
					 X58_PCI_DOWNSTREAM_FUNCTIONS)
#define X58_PCI_MAX_LEAF_RESOURCES	264

#define POST_X58_PCI_PREFLIGHT	0x2b
#define POST_X58_PCI_ROOTS_SAFE	0x2c
#define POST_X58_PCI_SCAN_OK	0x2d
#define POST_X58_PCI_RESOURCES	0x2e
#define POST_X58_PCI_ALLOC_OK	0x2f
#define POST_X58_PCI_IOU0_START	0x30
#define POST_X58_PCI_IOU0_POLLED	0x31
#define POST_X58_PCI_GPU_PRESENT	0x32
#define POST_X58_PCI_ENABLE_OK	0x34
#define POST_X58_PCI_GPU_FALLBACK	0x35
#define POST_X58_PCI_HANDOFF_FAIL	0x36
#define POST_X58_PCI_PLATFORM_FAIL 0x37
#define POST_X58_PCI_TOPOLOGY_FAIL 0x38
#define POST_X58_PCI_IDENTITY_FAIL 0x39
#define POST_X58_PCI_COMMAND_FAIL	0x3a
#define POST_X58_PCI_BUS_FAIL	0x3b
#define POST_X58_PCI_RESOURCE_FAIL 0x3c
#define POST_X58_PCI_OVERLAP_FAIL	0x3d
#define POST_X58_PCI_ENABLE_FAIL	0x3e
#define POST_X58_PCIE_TOPOLOGY_IOHBUSNO_OK	0x33
#define POST_X58_PCIE_TOPOLOGY_IOHBUSNO_FAIL 0x3f
#define POST_X58_INTERRUPT_MODE_LAPIC_BEGIN	0x40
#define POST_X58_INTERRUPT_MODE_LAPIC_OK	0x42
#define POST_X58_INTERRUPT_MODE_LAPIC_FAIL	0x43
#define POST_X58_USB_EHCI_BEGIN	0x44
#define POST_X58_USB_EHCI_OK	0x46
#define POST_X58_USB_EHCI_FAIL	0x47
#define POST_X58_SATA_MAP_AHCI_BEGIN	0x48
#define POST_X58_SATA_MAP_AHCI_OK	0x4a
#define POST_X58_SATA_MAP_AHCI_FAIL	0x4b
#define POST_X58_SATA_RESOURCE_PORTS_BEGIN	0x4c
#define POST_X58_SATA_RESOURCE_PORTS_OK	0x4e
#define POST_X58_SATA_RESOURCE_PORTS_FAIL	0x4f
#define POST_X58_SATA_CLOCK_BEGIN	0x50
#define POST_X58_SATA_CLOCK_OK	0x52
#define POST_X58_SATA_CLOCK_FAIL	0x53
#define POST_X58_USB_ICH10_BASE_BEGIN	0x54
#define POST_X58_USB_ICH10_BASE_OK	0x56
#define POST_X58_USB_ICH10_BASE_FAIL	0x57
#define POST_X58_SATA_CONFIG_RESET_TOPOLOGY_OK 0x58
#define POST_X58_SATA_CONFIG_MAP_BEGIN	0x59
#define POST_X58_SATA_CONFIG_MAP_OK	0x5a
#define POST_X58_SATA_CONFIG_FD_SAD2_BEGIN 0x5b
#define POST_X58_SATA_CONFIG_FD_SAD2_OK	0x5c
#define POST_X58_SATA_CONFIG_ROUTE_FAIL	0x5d
#define POST_X58_SATA_HANDOFF_PCS_BEGIN	0x5e
#define POST_X58_SATA_HANDOFF_PCS_OK	0x5f
#define POST_X58_SATA_HANDOFF_SCLK_BEGIN	0x60
#define POST_X58_SATA_HANDOFF_SCLK_OK	0x61
#define POST_X58_SATA_HANDOFF_READY		0x62
#define POST_X58_SATA_HANDOFF_FAIL		0x63
#define POST_X58_AHCI_MEM_BEGIN	0x68
#define POST_X58_AHCI_MEM_OK	0x69
#define POST_X58_AHCI_AE_BEGIN	0x6a
#define POST_X58_AHCI_AE_OK		0x6b
#define POST_X58_AHCI_PI_BEGIN	0x6c
#define POST_X58_AHCI_PI_OK		0x6d
#define POST_X58_AHCI_READY		0x6e
#define POST_X58_AHCI_FAIL		0x6f
#define POST_X58_IOAPIC_DECODE_BEGIN	0x70
#define POST_X58_IOAPIC_DECODE_OK	0x71
#define POST_X58_IOAPIC_CENSUS_OK	0x72
#define POST_X58_IOAPIC_READY		0x73
#define POST_X58_IOAPIC_FAIL		0x74
#define POST_X58_HPET_BEGIN	0x75
#define POST_X58_HPET_DECODED	0x76
#define POST_X58_HPET_READY	0x77
#define POST_X58_HPET_FAIL	0x78
#define POST_X58_TCO_BEGIN	0x79
#define POST_X58_TCO_READY	0x7a
#define POST_X58_TCO_FAIL	0x7b
#define POST_X58_ACPI_PIC_BEGIN	0x7c
#define POST_X58_ACPI_PIC_READY	0x7d
#define POST_X58_ACPI_PIC_FAIL	0x7e
#define POST_X58_ACPI_BEGIN	0x7f
#define POST_X58_ACPI_STATUS	0x80
#define POST_X58_ACPI_READY	0x81
#define POST_X58_ACPI_FAIL	0x82
#define POST_X58_LEGACY_INPUT_POLICY_BEGIN	0x87
#define POST_X58_LEGACY_INPUT_POLICY_READY	0x88
#define POST_X58_LEGACY_INPUT_POLICY_FAIL	0x89

#define X58_USB_EHCI2_DEV		PCI_DEV(0, 0x1a, 7)
#define X58_USB_EHCI1_DEV		PCI_DEV(0, 0x1d, 7)
#define X58_USB_EHCI_BAR_SIZE	0x400u
#define X58_USB_EHCI_CAPLEN_MIN	0x10u
#define X58_USB_EHCI_CAPLEN_MAX	0x80u
#define X58_USB_EHCI_PORT_MAX	6u
#define X58_USB_EHCI_HCIVERSION	0x0100u
#define X58_USB_EHCI_CAP_HCCPARAMS 0x08u
#define X58_USB_EHCI_OP_USBCMD	0x00u
#define X58_USB_EHCI_OP_USBSTS	0x04u
#define X58_USB_EHCI_OP_USBINTR	0x08u
#define X58_USB_EHCI_OP_CONFIGFLAG 0x40u
#define X58_USB_EHCI_OP_PORTSC	0x44u
#define X58_USB_EHCI_LEGACY_CAP	0x68u
#define X58_USB_EHCI_LEGACY_CTLSTS 0x6cu
#define X58_USB_EHCI_LEGACY_EXT	0x70u
#define X58_USB_EHCI_CFG61		0x61u
#define X58_USB_EHCI_CFG84		0x84u
#define X58_USB_UHCI_BAR_SIZE	0x20u
#define X58_USB_UHCI_USBCMD	0x00u
#define X58_USB_UHCI_USBSTS	0x02u
#define X58_USB_UHCI_USBINTR	0x04u
#define X58_USB_UHCI_PORTSC1	0x10u
#define X58_USB_UHCI_PORTSC2	0x12u
#define X58_USB_UHCI_LEGKEY	0xc0u
#define X58_USB_UHCI_CFG_C8	0xc8u
#define X58_USB_UHCI_CFG_CA	0xcau
#define X58_USB_RCBA_PPO		0x3524u
#define X58_USB_PMBASE_UPRWC	0x3cu
#define X58_USB_FD_DISABLE_MASK 0x0000bf80u
#define X58_USB_CG_DISABLE	BIT(20)
#define X58_USB_PPO_MASK	0x0fffu
#define X58_USB_MAP_MODE	BIT(0)
#define X58_USB_GPIO1_OC_MASK	0xe0000000u
#define X58_USB_GPIO2_OC_MASK	0x0800ff00u
#define X58_USB_EHCI_SMI_MASK	0xe03fu
#define X58_USB_EHCI_EXT_SMI_MASK	0x3fffu

#define X58_USB_ICH10_LPC_DEV		PCI_DEV(0, 0x1f, 0)
#define X58_USB_ICH10_LPC_ID		0x3a168086u
#define X58_USB_ICH10_RCBA_ENABLED	(CONFIG_FIXED_RCBA_MMIO_BASE | 1)
#define X58_USB_ICH10_FDSW_UNLOCKED	0x00u
#define X58_USB_ICH10_CIR8_PRE		0x00000000u
#define X58_USB_ICH10_FD_PRE		0x00000000u
#define X58_USB_ICH10_CIR9_PRE		0x00000020u
#define X58_USB_ICH10_CIR7_PRE		0xb2b477ccu
#define X58_USB_ICH10_CIR13_PRE		0xb2b477ccu
#define X58_USB_ICH10_CIR10_PRE		0x0008c008u
#define X58_USB_ICH10_CIR8_TARGET	0x00000002u
#define X58_USB_ICH10_FD_TARGET		0x00000001u
#define X58_USB_ICH10_CIR9_TARGET	0x08000020u
#define X58_USB_ICH10_CIR7_TARGET	0xb2b577ccu
#define X58_USB_ICH10_CIR13_TARGET	0xb2b577ccu
#define X58_USB_ICH10_CIR10_TARGET	0x000bc008u


#define X58_SATA_MAP_SATA1_DEV		PCI_DEV(0, 0x1f, 2)
#define X58_SATA_MAP_SATA2_DEV		PCI_DEV(0, 0x1f, 5)
#define X58_SATA_MAP_SATA1_IDE_ID	0x3a208086u
#define X58_SATA_MAP_SATA2_IDE_ID	0x3a268086u
#define X58_SATA_MAP_SATA1_AHCI_ID	0x3a228086u
#define X58_SATA_MAP_IDE_CLASS		0x0101u
#define X58_SATA_MAP_AHCI_CLASS_PI	0x010601u

#define X58_SATA_CONFIG_SATA1_IDE_CLASSREV	0x01018a00u
#define X58_SATA_CONFIG_SATA2_IDE_CLASSREV	0x01018500u
#define X58_SATA_CONFIG_SATA1_AHCI_CLASSREV	0x01060100u
#define X58_SATA_CONFIG_RESET_COMMAND		0x0000u
#define X58_SATA_CONFIG_RESET_HEADER		0x00u
#define X58_SATA_CONFIG_RESET_BAR5		0x00000001u
#define X58_SATA_CONFIG_RESET_MAP			0x0000u
#define X58_SATA_CONFIG_RESET_PCS			0x0000u
#define X58_SATA_CONFIG_RESET_SCLKCG		0x00000000u
#define X58_SATA_CONFIG_FD_BASELINE		0x00000001u
#define X58_SATA_CONFIG_FD_SATA2_DISABLED		0x02000001u

#define X58_SATA_RESOURCE_ABAR_SIZE		0x800u
#define X58_SATA_RESOURCE_ABAR_GRANULARITY	11
#define X58_SATA_RESOURCE_PCS_SAMPLE_COUNT	5

#define X58_AHCI_CAP		0x00u
#define X58_AHCI_GHC		0x04u
#define X58_AHCI_PI		0x0cu
#define X58_AHCI_VS		0x10u
#define X58_AHCI_CAP_TARGET	0xff22ffc5u
#define X58_AHCI_GHC_AE	BIT(31)
#define X58_AHCI_PI_TARGET	0x0000003fu
#define X58_AHCI_VS_TARGET	0x00010200u

#define X58_IOAPIC_LPC_DEV		PCI_DEV(0, 0x1f, 0)
#define X58_IOAPIC_LPC_ID		0x3a168086u
#define X58_IOAPIC_RCBA_ENABLED	(CONFIG_FIXED_RCBA_MMIO_BASE | 1)
#define X58_IOAPIC_OIC_DISABLED	0x00u
#define X58_IOAPIC_OIC_ENABLED	0x03u
#define X58_IOAPIC_BASE	0xfec00000u
#define X58_IOAPIC_IOREGSEL		0x00u
#define X58_IOAPIC_IOWIN		0x10u
#define X58_IOAPIC_ID_REG	0x00u
#define X58_IOAPIC_VERSION_REG 0x01u
#define X58_IOAPIC_ID_TARGET	0x00000000u
#define X58_IOAPIC_VERSION_TARGET 0x00170020u
#define X58_IOAPIC_VERSION	0x20u
#define X58_IOAPIC_MAX_REDIR	0x17u
#define X58_IOAPIC_REDIR_COUNT 24u
#define X58_IOAPIC_REDIR_BASE	0x10u
#define X58_IOAPIC_MASK_BIT	BIT(16)
#define X58_IOAPIC_LOW_TARGET	0x00010000u
#define X58_IOAPIC_HIGH_TARGET 0x00000000u

/*
 * ICH10 Datasheet HPTC: bits 1:0 select one of four HPET apertures and bit 7
 * enables decode.  The local X58_INTERRUPT_MODE transaction proved selector 0 plus enable
 * at FED00000; no other HPTC contents are admitted here.
 */
#define X58_HPET_HPTC_ADDRESS_SELECT_MASK GENMASK(1, 0)
#define X58_HPET_HPTC_DECODE_ENABLE BIT(7)
#define X58_HPET_HPTC_CONTROL_MASK \
	(X58_HPET_HPTC_ADDRESS_SELECT_MASK | X58_HPET_HPTC_DECODE_ENABLE)
#define X58_HPET_HPTC_DISABLED	0x00000000u
#define X58_HPET_HPTC_FED00000	X58_HPET_HPTC_DECODE_ENABLE

/* IA-PC HPET 1.0a register offsets and the exact locally observed identity. */
#define X58_HPET_BASE		0xfed00000ULL
#define X58_HPET_SIZE		0x00000400ULL
#define X58_HPET_CAP_ID_LOW	0x000u
#define X58_HPET_CAP_ID_HIGH	0x004u
#define X58_HPET_CAP_ID_LOW_TARGET 0x8086a301u
#define X58_HPET_CAP_ID_HIGH_TARGET 0x0429b17fu
#define X58_HPET_GENERAL_CONFIG_LOW 0x010u
#define X58_HPET_GENERAL_CONFIG_HIGH 0x014u
#define X58_HPET_GENERAL_INTERRUPT_STATUS_LOW 0x020u
#define X58_HPET_GENERAL_INTERRUPT_STATUS_HIGH 0x024u
#define X58_HPET_MAIN_COUNTER_LOW 0x0f0u
#define X58_HPET_MAIN_COUNTER_HIGH 0x0f4u
#define X58_HPET_TIMER0_CONFIG_LOW 0x100u
#define X58_HPET_TIMER0_CONFIG_HIGH 0x104u
#define X58_HPET_TIMER_INTERRUPT_ENABLE BIT(2)
#define X58_HPET_COUNTER_STABILITY_DELAY_US 4096u

#define X58_HPET_RESOURCE_INDEX 15u
#define X58_HPET_DOMAIN_RESOURCE_COUNT 16u

/*
 * ICH10 Datasheet sections 10.1.75 and 13.9.  These neutral, local names
 * deliberately describe only the fixed decode and TCO fields used by X58_TCO;
 * the broad historical watchdog helper has additional forbidden side effects.
 */
#define X58_TCO_PMBASE_REG		0x40u
#define X58_TCO_ACPI_CNTL_REG	0x44u
#define X58_TCO_PMBASE_ENABLED	0x00000501u
#define X58_TCO_ACPI_DECODE_ENABLED 0x80u
#define X58_TCO_BASE		0x0560u
#define X58_TCO_RLD		0x00u
#define X58_TCO1_STS		0x04u
#define X58_TCO2_STS		0x06u
#define X58_TCO1_CNT		0x08u
#define X58_TCO2_CNT		0x0au
#define X58_TCO_TMR		0x12u
#define X58_TCO1_CNT_HLT	BIT(11)
#define X58_TCO1_CNT_LOCK	BIT(12)
#define X58_TCO1_CNT_PRE	0x0000u
#define X58_TCO1_CNT_TARGET	0x0800u

/*
 * The complete legacy-PIC tuples admitted by X58_ACPI were both observed on the
 * target: power-on reset and the result of coreboot's standard i8259 setup
 * followed by making only SCI/IRQ9 level triggered.  X58_ACPI always replays the
 * standard initialization so the vector bases are known rather than inferred
 * from the readable masks.
 */
#define X58_ACPI_PIC_MASTER_MASK_RESET	0x00u
#define X58_ACPI_PIC_SLAVE_MASK_RESET	0x00u
#define X58_ACPI_ELCR1_RESET		0x00u
#define X58_ACPI_ELCR2_RESET		0x00u
#define X58_ACPI_PIC_MASTER_MASK_TARGET	0xfbu
#define X58_ACPI_PIC_SLAVE_MASK_TARGET	0xffu
#define X58_ACPI_ELCR1_TARGET		0x00u
#define X58_ACPI_ELCR2_TARGET		BIT(IRQ_9 - 8)

/*
 * X58_INTERRUPT_MODE-HW-01 measured this completely quiescent PM/SCI prestate.  X58_ACPI
 * admits either the measured SCI_EN-clear state or its own exact retained
 * target, acknowledges only the unsafe pending power-button-override status,
 * and sets only SCI_EN.  No event source is enabled here.
 */
#define X58_ACPI_PM1_CNT_PRE		0x0000u
#define X58_ACPI_PM1_CNT_TARGET		SCI_EN
#define X58_ACPI_PM1_CNT_DWORD_PRE		0x00000000u
#define X58_ACPI_PM1_CNT_DWORD_TARGET	0x00000001u
#define X58_ACPI_UPRWC			0x3cu
#define X58_ACPI_IOAPIC_SCI_ENTRY		9u
#define X58_ACPI_IOAPIC_SCI_LOW_REG \
	(X58_IOAPIC_REDIR_BASE + 2u * X58_ACPI_IOAPIC_SCI_ENTRY)
#define X58_ACPI_IOAPIC_SCI_HIGH_REG	(X58_ACPI_IOAPIC_SCI_LOW_REG + 1u)

#define X58_PLATFORM_RESOURCE_SMBUS_RESOURCE_INDEX 8u
#define X58_PLATFORM_RESOURCE_PM_RESOURCE_INDEX	9u
#define X58_PLATFORM_RESOURCE_GPIO_RESOURCE_INDEX 10u
#define X58_PLATFORM_RESOURCE_IOAPIC_RESOURCE_INDEX 11u
#define X58_PLATFORM_RESOURCE_RCBA_RESOURCE_INDEX 12u
#define X58_PLATFORM_RESOURCE_LAPIC_RESOURCE_INDEX 13u
#define X58_PLATFORM_RESOURCE_ROM_RESOURCE_INDEX	14u
#define X58_PLATFORM_RESOURCE_DOMAIN_RESOURCE_COUNT 15u
#define X58_PLATFORM_RESOURCE_FIRST_FIXED_RESOURCE_INDEX X58_PLATFORM_RESOURCE_SMBUS_RESOURCE_INDEX

#define X58_PLATFORM_RESOURCE_SMBUS_IO_BASE	0x0400u
#define X58_PLATFORM_RESOURCE_SMBUS_IO_SIZE	0x0020u
#define X58_PLATFORM_RESOURCE_PM_IO_BASE		0x0500u
#define X58_PLATFORM_RESOURCE_PM_IO_SIZE		0x0080u
#define X58_PLATFORM_RESOURCE_GPIO_IO_BASE	0x0580u
#define X58_PLATFORM_RESOURCE_GPIO_IO_SIZE	0x0040u
#define X58_PLATFORM_RESOURCE_IOAPIC_BASE	0xfec00000ULL
#define X58_PLATFORM_RESOURCE_IOAPIC_SIZE	0x00001000ULL
#define X58_PLATFORM_RESOURCE_RCBA_BASE		0xfed1c000ULL
#define X58_PLATFORM_RESOURCE_RCBA_SIZE		0x00004000ULL
#define X58_PLATFORM_RESOURCE_LAPIC_BASE	0xfee00000ULL
#define X58_PLATFORM_RESOURCE_LAPIC_SIZE	0x00001000ULL
#define X58_PLATFORM_RESOURCE_ROM_BASE		0xff000000ULL
#define X58_PLATFORM_RESOURCE_ROM_SIZE		0x01000000ULL
#define X58_PLATFORM_RESOURCE_ROM_TOP		0x100000000ULL

#define X58_PLATFORM_RESOURCE_RAM_FLAGS \
	(IORESOURCE_ASSIGNED | IORESOURCE_FIXED | IORESOURCE_MEM | \
	 IORESOURCE_CACHEABLE | IORESOURCE_STORED)
#define X58_PLATFORM_RESOURCE_RESERVED_RAM_FLAGS \
	(X58_PLATFORM_RESOURCE_RAM_FLAGS | IORESOURCE_RESERVE)
#define X58_PLATFORM_RESOURCE_MMIO_FLAGS \
	(IORESOURCE_ASSIGNED | IORESOURCE_FIXED | IORESOURCE_MEM | \
	 IORESOURCE_RESERVE | IORESOURCE_STORED)
#define X58_PLATFORM_RESOURCE_FIXED_IO_FLAGS \
	(IORESOURCE_ASSIGNED | IORESOURCE_FIXED | IORESOURCE_IO | \
	 IORESOURCE_RESERVE)
#define X58_PLATFORM_RESOURCE_IO_WINDOW_FLAGS \
	(IORESOURCE_ASSIGNED | IORESOURCE_IO | IORESOURCE_BRIDGE)
#define X58_PLATFORM_RESOURCE_MEM_WINDOW_FLAGS \
	(IORESOURCE_ASSIGNED | IORESOURCE_MEM | IORESOURCE_BRIDGE)
#define X58_PLATFORM_RESOURCE_FULL_DOMAIN_RESOURCE_COUNT X58_HPET_DOMAIN_RESOURCE_COUNT

_Static_assert(CONFIG(MINIMAL_PCI_SCANNING),
	"X58_PCI must never scan PCI functions absent from its mandatory devicetree");
_Static_assert(CONFIG(PCI_ALLOW_BUS_MASTER) &&
	CONFIG(PCI_SET_BUS_MASTER_PCI_BRIDGES),
	"X58_PCI requires standard BME on selected forwarding PCI bridges");
_Static_assert(!CONFIG(PCI_ALLOW_BUS_MASTER_ANY_DEVICE),
	"X58_PCI must not grant blanket endpoint bus mastering");
_Static_assert(X58_RAMINIT_OBJECT_BASE + X58_RAMINIT_OBJECT_SIZE <=
	X58_PCI_MMIO_BASE, "X58_PCI PCI MMIO overlaps a X58_MEMORY_PROFILE-smoked RAM window");
_Static_assert(X58_PCI_LOW_RAM_TOP == X58_PCI_MMIO_BASE,
	"X58_PCI PCI MMIO must begin exactly at the gated low-RAM top");
_Static_assert(X58_PCI_MMIO_TOP == X58_PCI_ECAM_BASE,
	"X58_PCI PCI MMIO must end before PCIEXBAR ECAM");
_Static_assert(X58_PCI_ECAM_TOP <= X58_PCI_HIGH_RAM_BASE,
	"X58_PCI PCIEXBAR ECAM must end before remapped RAM");
_Static_assert(X58_PCI_ECAM_DEV0(X58_PCI_IOU2_X4_DEV,
	X58_PCIE_PRTX_BIF_CTRL) == 0xe0008190ULL,
	"X58_PCI IOU2 telemetry address changed");
_Static_assert(X58_PCI_ECAM_DEV0(X58_PCI_IOU0_X16_DEV,
	X58_PCIE_PRTX_BIF_CTRL) == 0xe0018190ULL,
	"X58_PCI IOU0 start address changed");
_Static_assert(X58_PCI_ECAM_DEV0(X58_PCI_AUX_X16_DEV,
	X58_PCIE_PRTX_BIF_CTRL) == 0xe0038190ULL,
	"X58_PCI auxiliary x16-port telemetry address changed");
_Static_assert(X58_PCI_ECAM_DEV0(0x14, X58_PCIE_TOPOLOGY_IOHBUSNO) == 0xe00a010aULL,
	"X58_PCIE_TOPOLOGY IOHBUSNO address changed");
_Static_assert(X58_PCI_ECAM_ADDR(1, X58_PCIE_TOPOLOGY_ALIAS_DEV, 0, 0) == 0xe0168000ULL,
	"X58_PCIE_TOPOLOGY bus-1 alias-probe address changed");
_Static_assert(X58_PCI_ECAM_ADDR(2, X58_PCIE_TOPOLOGY_ALIAS_DEV, 0, 0) == 0xe0268000ULL,
	"X58_PCIE_TOPOLOGY bus-2 alias-probe address changed");
_Static_assert(X58_PCI_ECAM_ADDR(1, X58_PCIE_TOPOLOGY_PEG_ENDPOINT_DEV, 0, 0) ==
	0xe0100000ULL, "X58_PCIE_TOPOLOGY bus-1 endpoint address changed");
_Static_assert(X58_PCI_LINK_POLL_US * X58_PCI_LINK_POLL_COUNT == 1000000,
	"X58_PCI link poll must remain bounded to one second");
_Static_assert(X58_PLATFORM_RESOURCE_SMBUS_IO_BASE + X58_PLATFORM_RESOURCE_SMBUS_IO_SIZE <=
	X58_PLATFORM_RESOURCE_PM_IO_BASE, "X58_PLATFORM_RESOURCE SMBus and PM resources overlap");
_Static_assert(X58_PLATFORM_RESOURCE_PM_IO_BASE + X58_PLATFORM_RESOURCE_PM_IO_SIZE <=
	X58_PLATFORM_RESOURCE_GPIO_IO_BASE, "X58_PLATFORM_RESOURCE PM and GPIO resources overlap");
_Static_assert(X58_PLATFORM_RESOURCE_GPIO_IO_BASE + X58_PLATFORM_RESOURCE_GPIO_IO_SIZE <=
	X58_PCI_IO_BASE,
	"X58_PLATFORM_RESOURCE fixed I/O resources must remain below the PCI I/O aperture");
_Static_assert(X58_PCI_ECAM_TOP <= X58_PLATFORM_RESOURCE_IOAPIC_BASE,
	"X58_PLATFORM_RESOURCE IOAPIC resource overlaps ECAM");
_Static_assert(X58_PLATFORM_RESOURCE_IOAPIC_BASE + X58_PLATFORM_RESOURCE_IOAPIC_SIZE <= X58_PLATFORM_RESOURCE_RCBA_BASE,
	"X58_PLATFORM_RESOURCE IOAPIC and RCBA resources overlap");
_Static_assert(X58_PLATFORM_RESOURCE_RCBA_BASE + X58_PLATFORM_RESOURCE_RCBA_SIZE <= X58_PLATFORM_RESOURCE_LAPIC_BASE,
	"X58_PLATFORM_RESOURCE RCBA and LAPIC resources overlap");
_Static_assert(X58_PLATFORM_RESOURCE_LAPIC_BASE + X58_PLATFORM_RESOURCE_LAPIC_SIZE <= X58_PLATFORM_RESOURCE_ROM_BASE,
	"X58_PLATFORM_RESOURCE LAPIC and ROM resources overlap");
_Static_assert(X58_PLATFORM_RESOURCE_ROM_BASE + X58_PLATFORM_RESOURCE_ROM_SIZE == X58_PLATFORM_RESOURCE_ROM_TOP &&
	X58_PLATFORM_RESOURCE_ROM_TOP == X58_PCI_HIGH_RAM_BASE,
	"X58_PLATFORM_RESOURCE ROM resource must end exactly where remapped RAM begins");
_Static_assert(X58_PLATFORM_RESOURCE_ROM_RESOURCE_INDEX + 1 ==
	X58_PLATFORM_RESOURCE_DOMAIN_RESOURCE_COUNT,
	"X58_PLATFORM_RESOURCE fixed resource indices must end at 14");
_Static_assert((X58_HPET_HPTC_FED00000 & X58_HPET_HPTC_CONTROL_MASK) ==
	X58_HPET_HPTC_DECODE_ENABLE,
	"X58_HPET must select FED00000 and only enable HPET decode");
_Static_assert(X58_PLATFORM_RESOURCE_IOAPIC_BASE + X58_PLATFORM_RESOURCE_IOAPIC_SIZE <= X58_HPET_BASE,
	"X58_HPET HPET resource overlaps the ICH10R IOAPIC");
_Static_assert(X58_HPET_BASE + X58_HPET_SIZE <= X58_PLATFORM_RESOURCE_RCBA_BASE,
	"X58_HPET HPET resource overlaps RCBA");
_Static_assert(X58_HPET_RESOURCE_INDEX == X58_PLATFORM_RESOURCE_DOMAIN_RESOURCE_COUNT &&
	X58_HPET_RESOURCE_INDEX + 1 == X58_HPET_DOMAIN_RESOURCE_COUNT,
	"X58_HPET must append only resource index 15");
_Static_assert(X58_TCO_BASE == DEFAULT_TCOBASE,
	"X58_TCO fixed TCO base must match the ICH10 PM decode");
_Static_assert(X58_TCO_PMBASE_ENABLED == (DEFAULT_PMBASE | 1),
	"X58_TCO exact PMBASE gate changed");
_Static_assert(X58_TCO1_CNT_TARGET == X58_TCO1_CNT_HLT &&
	!(X58_TCO1_CNT_TARGET & X58_TCO1_CNT_LOCK),
	"X58_TCO target must set only TCO_TMR_HLT and leave TCO_LOCK clear");
_Static_assert(!CONFIG(NO_PCAT_8259),
	"X58_ACPI requires the legacy 8259 virtual-wire source");
_Static_assert(CONFIG(SEABIOS_HARDWARE_IRQ),
	"X58_ACPI must retain SeaBIOS hardware interrupts");
_Static_assert(X58_ACPI_ELCR2_TARGET == BIT(1),
	"X58_ACPI must make only SCI/IRQ9 level triggered in ELCR2");
_Static_assert(I82801JX_SATA_MAP_AHCI_D31F2_MASK == 0x00e0,
	"X58_SATA_MAP must select only SATA MAP bits 7:5");
_Static_assert(I82801JX_SATA_MAP_AHCI_D31F2_VALUE == 0x0060,
	"X58_SATA_MAP must select AHCI with all ports on D31:F2");
_Static_assert(FD_SAD2 == 0x02000000,
	"X58_SATA_CONFIG must select only FD_SAD2 bit 25");
_Static_assert((X58_USB_ICH10_FD_TARGET | FD_SAD2) ==
	X58_SATA_CONFIG_FD_SATA2_DISABLED,
	"X58_SATA_CONFIG final FD must compose ICH10_REQUIRED_FIELDS and SAD2 exactly");

struct x58_pci_expected_pci {
	unsigned int root_devfn;
	unsigned int devfn;
	uint32_t id;
	uint16_t class_code;
	int16_t revision;
	uint8_t header_type;
	bool downstream;
	bool bridge;
	bool runtime_optional;
	const char *name;
};

/*
 * Root IDs/classes come directly from X58_MEMORY_PROFILE-HW-02.  The downstream NIC ID
 * and revision come from the live board inventory.  The operator identifies
 * the replacement card as an AMD Radeon HD 5450, but its exact PCI device ID
 * has not yet been observed.  X58_PCI therefore gates that one endpoint by AMD
 * vendor, VGA class and header shape, and prints the discovered device ID.
 */
static const struct x58_pci_expected_pci x58_pci_expected[] = {
	{ PCI_DEVFN(0x03, 0), PCI_DEVFN(0x03, 0), 0x340a8086u,
	  PCI_CLASS_BRIDGE_PCI, -1, PCI_HEADER_TYPE_BRIDGE, false, true, false,
	  "X58 PEG3" },
	{ PCI_DEVFN(0x03, 0), PCI_DEVFN(0x00, 0), X58_PCI_AMD_VENDOR_ID,
	  PCI_CLASS_DISPLAY_VGA, -1, PCI_HEADER_TYPE_NORMAL, true, false, true,
	  "AMD VGA (operator-stated HD 5450)" },
	{ PCI_DEVFN(0x1a, 0), PCI_DEVFN(0x1a, 0), 0x3a378086u,
	  PCI_CLASS_SERIAL_USB, -1, PCI_HEADER_TYPE_NORMAL, false, false, false,
	  "ICH10R UHCI4" },
	{ PCI_DEVFN(0x1a, 1), PCI_DEVFN(0x1a, 1), 0x3a388086u,
	  PCI_CLASS_SERIAL_USB, -1, PCI_HEADER_TYPE_NORMAL, false, false, false,
	  "ICH10R UHCI5" },
	{ PCI_DEVFN(0x1a, 2), PCI_DEVFN(0x1a, 2), 0x3a398086u,
	  PCI_CLASS_SERIAL_USB, -1, PCI_HEADER_TYPE_NORMAL, false, false, false,
	  "ICH10R UHCI6" },
	{ PCI_DEVFN(0x1a, 7), PCI_DEVFN(0x1a, 7), 0x3a3c8086u,
	  PCI_CLASS_SERIAL_USB, -1, PCI_HEADER_TYPE_NORMAL, false, false, false,
	  "ICH10R EHCI2" },
	/* ECAM1 Linux inventory: fixed onboard identities, not arbitrary plugins. */
	{ PCI_DEVFN(0x1c, 0), PCI_DEVFN(0x1c, 0), 0x3a408086u,
	  PCI_CLASS_BRIDGE_PCI, -1, PCI_HEADER_TYPE_BRIDGE, false, true, false,
	  "ICH10R RP1" },
	{ PCI_DEVFN(0x1c, 1), PCI_DEVFN(0x1c, 1), 0x3a428086u,
	  PCI_CLASS_BRIDGE_PCI, -1, PCI_HEADER_TYPE_BRIDGE, false, true, false,
	  "ICH10R RP2" },
	{ PCI_DEVFN(0x1c, 1), PCI_DEVFN(0, 0), 0x2363197bu,
	  PCI_CLASS_STORAGE_SATA, 0x03, PCI_HEADER_TYPE_NORMAL, true, false, false,
	  "JMB363 SATA" },
	{ PCI_DEVFN(0x1c, 1), PCI_DEVFN(0, 1), 0x2363197bu,
	  PCI_CLASS_STORAGE_IDE, 0x03, PCI_HEADER_TYPE_NORMAL, true, false, false,
	  "JMB363 IDE" },
	{ PCI_DEVFN(0x1c, 2), PCI_DEVFN(0x1c, 2), 0x3a448086u,
	  PCI_CLASS_BRIDGE_PCI, -1, PCI_HEADER_TYPE_BRIDGE, false, true, false,
	  "ICH10R RP3" },
	{ PCI_DEVFN(0x1c, 3), PCI_DEVFN(0x1c, 3), 0x3a468086u,
	  PCI_CLASS_BRIDGE_PCI, -1, PCI_HEADER_TYPE_BRIDGE, false, true, false,
	  "ICH10R RP4" },
	{ PCI_DEVFN(0x1c, 5), PCI_DEVFN(0x1c, 5), 0x3a4a8086u,
	  PCI_CLASS_BRIDGE_PCI, -1, PCI_HEADER_TYPE_BRIDGE, false, true, false,
	  "ICH10R RP6" },
	{ PCI_DEVFN(0x1c, 5), PCI_DEVFN(0, 0), 0x2380197bu,
	  PCI_CLASS_SERIAL_FIREWIRE, 0x10, PCI_HEADER_TYPE_NORMAL, true, false, false,
	  "JMicron FireWire" },
	{ PCI_DEVFN(0x1c, 4), PCI_DEVFN(0x1c, 4), 0x3a488086u,
	  PCI_CLASS_BRIDGE_PCI, -1, PCI_HEADER_TYPE_BRIDGE, false, true, false,
	  "ICH10R RP5" },
	{ PCI_DEVFN(0x1c, 4), PCI_DEVFN(0x00, 0), 0x816810ecu,
	  PCI_CLASS_NETWORK_ETHERNET, 0x02, PCI_HEADER_TYPE_NORMAL, true, false, false,
	  "RTL8168" },
	{ PCI_DEVFN(0x1d, 0), PCI_DEVFN(0x1d, 0), 0x3a348086u,
	  PCI_CLASS_SERIAL_USB, -1, PCI_HEADER_TYPE_NORMAL, false, false, false,
	  "ICH10R UHCI1" },
	{ PCI_DEVFN(0x1d, 1), PCI_DEVFN(0x1d, 1), 0x3a358086u,
	  PCI_CLASS_SERIAL_USB, -1, PCI_HEADER_TYPE_NORMAL, false, false, false,
	  "ICH10R UHCI2" },
	{ PCI_DEVFN(0x1d, 2), PCI_DEVFN(0x1d, 2), 0x3a368086u,
	  PCI_CLASS_SERIAL_USB, -1, PCI_HEADER_TYPE_NORMAL, false, false, false,
	  "ICH10R UHCI3" },
	{ PCI_DEVFN(0x1d, 7), PCI_DEVFN(0x1d, 7), 0x3a3a8086u,
	  PCI_CLASS_SERIAL_USB, -1, PCI_HEADER_TYPE_NORMAL, false, false, false,
	  "ICH10R EHCI1" },
	{ PCI_DEVFN(0x1f, 2), PCI_DEVFN(0x1f, 2), X58_SATA_MAP_SATA1_AHCI_ID,
	  PCI_CLASS_STORAGE_SATA, -1, PCI_HEADER_TYPE_NORMAL, false, false, false,
	  "ICH10R SATA AHCI" },
};

static const uint32_t x58_pci_sad_rules[X58_PCI_SAD_RULE_COUNT] = {
	0x00000bc3u, 0x00000fc0u, 0x000013c3u, 0x000013c0u,
	0x000013c0u, 0x000013c0u, 0x000013c0u, 0x000013c0u,
};

struct x58_pci_leaf_resource {
	const struct device *dev;
	const struct resource *resource;
};

struct x58_platform_resource_domain_resource_contract {
	unsigned long index;
	uint64_t base;
	uint64_t top;
	unsigned long flags;
};

static const struct x58_platform_resource_domain_resource_contract
x58_platform_resource_domain_resource_contracts[] = {
	{ 0, 0x00000000ULL, 0x000a0000ULL, X58_PLATFORM_RESOURCE_RAM_FLAGS },
	{ 1, 0x000a0000ULL, 0x000c0000ULL, X58_PLATFORM_RESOURCE_MMIO_FLAGS },
	{ 2, 0x000c0000ULL, 0x00100000ULL, X58_PLATFORM_RESOURCE_RESERVED_RAM_FLAGS },
	{ 3, X58_PCI_LOW_RAM_BASE, X58_PCI_LOW_RAM_TOP, X58_PLATFORM_RESOURCE_RAM_FLAGS },
	{ 4, X58_PCI_HIGH_RAM_BASE, X58_PCI_HIGH_RAM_TOP, X58_PLATFORM_RESOURCE_RAM_FLAGS },
	{ 5, X58_PCI_IO_BASE, X58_PCI_IO_TOP,
	  X58_PLATFORM_RESOURCE_IO_WINDOW_FLAGS },
	{ 6, X58_PCI_MMIO_BASE, X58_PCI_MMIO_TOP,
	  X58_PLATFORM_RESOURCE_MEM_WINDOW_FLAGS },
	{ 7, X58_PCI_ECAM_BASE, X58_PCI_ECAM_TOP, X58_PLATFORM_RESOURCE_MMIO_FLAGS },
	{ X58_PLATFORM_RESOURCE_SMBUS_RESOURCE_INDEX, X58_PLATFORM_RESOURCE_SMBUS_IO_BASE,
	  X58_PLATFORM_RESOURCE_SMBUS_IO_BASE + X58_PLATFORM_RESOURCE_SMBUS_IO_SIZE,
	  X58_PLATFORM_RESOURCE_FIXED_IO_FLAGS },
	{ X58_PLATFORM_RESOURCE_PM_RESOURCE_INDEX, X58_PLATFORM_RESOURCE_PM_IO_BASE,
	  X58_PLATFORM_RESOURCE_PM_IO_BASE + X58_PLATFORM_RESOURCE_PM_IO_SIZE, X58_PLATFORM_RESOURCE_FIXED_IO_FLAGS },
	{ X58_PLATFORM_RESOURCE_GPIO_RESOURCE_INDEX, X58_PLATFORM_RESOURCE_GPIO_IO_BASE,
	  X58_PLATFORM_RESOURCE_GPIO_IO_BASE + X58_PLATFORM_RESOURCE_GPIO_IO_SIZE, X58_PLATFORM_RESOURCE_FIXED_IO_FLAGS },
	{ X58_PLATFORM_RESOURCE_IOAPIC_RESOURCE_INDEX, X58_PLATFORM_RESOURCE_IOAPIC_BASE,
	  X58_PLATFORM_RESOURCE_IOAPIC_BASE + X58_PLATFORM_RESOURCE_IOAPIC_SIZE, X58_PLATFORM_RESOURCE_MMIO_FLAGS },
	{ X58_PLATFORM_RESOURCE_RCBA_RESOURCE_INDEX, X58_PLATFORM_RESOURCE_RCBA_BASE,
	  X58_PLATFORM_RESOURCE_RCBA_BASE + X58_PLATFORM_RESOURCE_RCBA_SIZE, X58_PLATFORM_RESOURCE_MMIO_FLAGS },
	{ X58_PLATFORM_RESOURCE_LAPIC_RESOURCE_INDEX, X58_PLATFORM_RESOURCE_LAPIC_BASE,
	  X58_PLATFORM_RESOURCE_LAPIC_BASE + X58_PLATFORM_RESOURCE_LAPIC_SIZE, X58_PLATFORM_RESOURCE_MMIO_FLAGS },
	{ X58_PLATFORM_RESOURCE_ROM_RESOURCE_INDEX, X58_PLATFORM_RESOURCE_ROM_BASE, X58_PLATFORM_RESOURCE_ROM_TOP,
	  X58_PLATFORM_RESOURCE_MMIO_FLAGS },
};

_Static_assert(ARRAY_SIZE(x58_platform_resource_domain_resource_contracts) ==
	X58_PLATFORM_RESOURCE_DOMAIN_RESOURCE_COUNT,
	"X58_PLATFORM_RESOURCE must describe domain resources 0 through 14 exactly");

static const struct x58_platform_resource_domain_resource_contract
x58_hpet_resource_contract = {
	X58_HPET_RESOURCE_INDEX,
	X58_HPET_BASE,
	X58_HPET_BASE + X58_HPET_SIZE,
	X58_PLATFORM_RESOURCE_MMIO_FLAGS,
};

static struct device *x58_pci_domain;
static bool x58_pci_iou0_start_attempted;
static bool x58_pci_gpu_absence_reported;
static bool x58_pcie_topology_iohbusno_attempted;
static bool x58_interrupt_mode_lapic_setup_attempted;
static bool x58_usb_ehci_init_attempted;
static bool x58_usb_telemetry_emitted;
static bool x58_usb_ich10_baseline_attempted;
static bool x58_sata_config_ahci_route_attempted;
static bool x58_sata_handoff_pcs_attempted;
static bool x58_sata_handoff_sclk_attempted;
static bool x58_legacy_input_sata_policy_ready;
static bool x58_ahci_mmio_attempted;
static bool x58_ahci_final_verified;
static bool x58_ioapic_attempted;
static bool x58_hpet_attempted;
static bool x58_hpet_ready;
static bool x58_tco_attempted;
static bool x58_tco_ready;
static bool x58_acpi_pic_attempted;
static bool x58_acpi_pic_ready;
static bool x58_acpi_mode_attempted;
static bool x58_acpi_mode_is_ready;
static bool x58_pci_standard_lpc_attempted;
static bool x58_pci_standard_lpc_ready;

_Static_assert(I82801JX_CIR8_FIELD_1_0_MASK == 0x00000003u &&
	I82801JX_CIR8_FIELD_1_0_REQUIRED == 0x00000002u,
	"X58_USB-ICH10_REQUIRED_FIELDS CIR8 field contract changed");
_Static_assert(I82801JX_FD_REQUIRED_BIT_0 == 0x00000001u,
	"X58_USB-ICH10_REQUIRED_FIELDS FD field contract changed");
_Static_assert(I82801JX_CIR9_FIELD_27_26_MASK == 0x0c000000u &&
	I82801JX_CIR9_FIELD_27_26_REQUIRED == 0x08000000u,
	"X58_USB-ICH10_REQUIRED_FIELDS CIR9 field contract changed");
_Static_assert(I82801JX_CIR7_FIELD_19_16_MASK == 0x000f0000u &&
	I82801JX_CIR7_FIELD_19_16_REQUIRED == 0x00050000u &&
	I82801JX_CIR13_FIELD_19_16_MASK == 0x000f0000u &&
	I82801JX_CIR13_FIELD_19_16_REQUIRED == 0x00050000u,
	"X58_USB-ICH10_REQUIRED_FIELDS CIR7/CIR13 field contract changed");
_Static_assert(I82801JX_CIR10_REQUIRED_BITS_17_16 == 0x00030000u,
	"X58_USB-ICH10_REQUIRED_FIELDS CIR10 field contract changed");

_Static_assert(ARRAY_SIZE(x58_pci_expected) == X58_PCI_EXPECTED_FUNCTIONS,
	"X58_PCI expected-function count changed");

static const struct x58_raminit_handoff *x58_pci_verified_handoff(void)
{
	const struct cbmem_entry *entry;
	const struct x58_raminit_handoff *handoff;
	void *cbmem_base;
	size_t cbmem_size;
	uintptr_t cbmem_start;
	uintptr_t cbmem_end;

	if (!cbmem_online())
		die_with_post_code(POST_X58_PCI_HANDOFF_FAIL,
			"[RAMSTAGE] X58_PCI CBMEM is offline\n");
	entry = cbmem_entry_find(X58_RAMINIT_CBMEM_ID);
	if (entry == NULL || cbmem_entry_size(entry) < sizeof(*handoff) ||
	    cbmem_get_region(&cbmem_base, &cbmem_size))
		die_with_post_code(POST_X58_PCI_HANDOFF_FAIL,
			"[RAMSTAGE] X58_PCI handoff entry/CBMEM region invalid\n");

	cbmem_start = (uintptr_t)cbmem_base;
	if (cbmem_start > UINTPTR_MAX - cbmem_size)
		die_with_post_code(POST_X58_PCI_HANDOFF_FAIL,
			"[RAMSTAGE] X58_PCI CBMEM region wraps\n");
	cbmem_end = cbmem_start + cbmem_size;
	if (cbmem_start < X58_RAMINIT_CBMEM_BASE ||
	    cbmem_end != X58_RAMINIT_CBMEM_TOP || cbmem_end <= cbmem_start)
		die_with_post_code(POST_X58_PCI_HANDOFF_FAIL,
			"[RAMSTAGE] X58_PCI CBMEM escaped the X58_MEMORY_PROFILE-smoked window\n");

	handoff = cbmem_entry_start(entry);
	if ((uintptr_t)handoff < cbmem_start ||
	    (uintptr_t)handoff > cbmem_end - sizeof(*handoff) ||
	    !x58_raminit_handoff_is_valid(handoff) ||
	    !(handoff->flags & X58_MEMORY_HANDOFF_LOWMEM_SMOKED) ||
	    !(handoff->flags & X58_MEMORY_PROFILE_HANDOFF_BROAD_POST_MINIT))
		die_with_post_code(POST_X58_PCI_HANDOFF_FAIL,
			"[RAMSTAGE] X58_PCI serial-independent v10 handoff gate failed\n");

	return handoff;
}

static void x58_pci_require_platform_state(const char *phase)
{
	uint32_t value;

	(void)x58_pci_verified_handoff();
	if (pci_io_read_config32(X58_PCI_SAD_DEV, PCI_VENDOR_ID) != X58_PCI_SAD_ID ||
	    pci_io_read_config32(X58_PCI_SAD_DEV, X58_PCI_SAD_PCIEXBAR_LO) !=
		X58_PCIEXBAR_LO ||
	    pci_io_read_config32(X58_PCI_SAD_DEV, X58_PCI_SAD_PCIEXBAR_HI) != 0 ||
	    pci_io_read_config32(X58_PCI_HOST_DEV, PCI_VENDOR_ID) !=
		X58_PCI_HOST_ID ||
	    pci_io_read_config32(X58_PCI_HOST_DEV, PCI_CLASS_REVISION) !=
		X58_PCI_HOST_CLASSREV)
		die_with_post_code(POST_X58_PCI_PLATFORM_FAIL,
			"[RAMSTAGE] X58_PCI %s host/PCIEXBAR gate failed\n", phase);

	for (size_t i = 0; i < ARRAY_SIZE(x58_pci_sad_rules); i++) {
		value = pci_io_read_config32(X58_PCI_SAD_DEV,
			X58_PCI_SAD_RULE_FIRST + i * sizeof(uint32_t));
		if (value != x58_pci_sad_rules[i])
			die_with_post_code(POST_X58_PCI_PLATFORM_FAIL,
				"[RAMSTAGE] X58_PCI %s SAD%zu=%08x expected=%08x\n",
				phase, i, value, x58_pci_sad_rules[i]);
	}

	if (pci_io_read_config32(X58_PCI_IOH_HM_DEV, PCI_VENDOR_ID) !=
		X58_PCI_IOH_HM_ID ||
	    pci_io_read_config32(X58_PCI_IOH_HM_DEV, X58_PCI_IOH_TOLM) !=
		X58_PCI_TOLM_VALUE ||
	    pci_io_read_config32(X58_PCI_IOH_HM_DEV, X58_PCI_IOH_TOHM_LO) !=
		X58_PCI_TOHM_LO_VALUE ||
	    pci_io_read_config32(X58_PCI_IOH_HM_DEV, X58_PCI_IOH_TOHM_HI) !=
		X58_PCI_TOHM_HI_VALUE)
		die_with_post_code(POST_X58_PCI_PLATFORM_FAIL,
			"[RAMSTAGE] X58_PCI %s exact TOLM/TOHM gate failed\n", phase);
}

static uint32_t x58_pci_ecam_read32(unsigned int dev, unsigned int reg)
{
	return read32p(X58_PCI_ECAM_DEV0(dev, reg));
}

static uint16_t x58_pci_ecam_read16(unsigned int dev, unsigned int reg)
{
	return read16p(X58_PCI_ECAM_DEV0(dev, reg));
}

static uint32_t x58_pcie_topology_ecam_read_id(unsigned int bus, unsigned int dev,
	unsigned int fn)
{
	return read32p(X58_PCI_ECAM_ADDR(bus, dev, fn, PCI_VENDOR_ID));
}

static void x58_pcie_topology_log_ioh_routing(const char *phase)
{
	printk(BIOS_DEBUG,
	       "[IOHCFG] %s IOHBUSNO=%04x LCFGBUS=%02x-%02x "
	       "GCFGBUS=%02x-%02x ALIAS01:0d.0=%08x ALIAS02:0d.0=%08x\n",
	       phase, read16p(X58_PCI_ECAM_DEV0(0x14, X58_PCIE_TOPOLOGY_IOHBUSNO)),
	       read8p(X58_PCI_ECAM_DEV0(0x14, X58_PCIE_TOPOLOGY_LCFGBUS_BASE)),
	       read8p(X58_PCI_ECAM_DEV0(0x14, X58_PCIE_TOPOLOGY_LCFGBUS_LIMIT)),
	       read8p(X58_PCI_ECAM_DEV0(0x14, X58_PCIE_TOPOLOGY_GCFGBUS_BASE)),
	       read8p(X58_PCI_ECAM_DEV0(0x14, X58_PCIE_TOPOLOGY_GCFGBUS_LIMIT)),
	       x58_pcie_topology_ecam_read_id(1, X58_PCIE_TOPOLOGY_ALIAS_DEV, 0),
	       x58_pcie_topology_ecam_read_id(2, X58_PCIE_TOPOLOGY_ALIAS_DEV, 0));
}

/*
 * This is X58_PCIE_TOPOLOGY's sole new write hypothesis.  Accept only the documented
 * reset value or the exact vendor-observed target, never mask unknown bits,
 * and require the complete 16-bit readback before PCI routing continues.
 */
static void x58_pcie_topology_program_ioh_bus_number_once(void)
{
	const uintptr_t address = X58_PCI_ECAM_DEV0(0x14, X58_PCIE_TOPOLOGY_IOHBUSNO);
	const uint16_t before = read16p(address);
	uint16_t after;
	bool wrote = false;

	if (x58_pcie_topology_iohbusno_attempted)
		die_with_post_code(POST_X58_PCIE_TOPOLOGY_IOHBUSNO_FAIL,
			"[IOHCFG] X58_PCIE_TOPOLOGY refused a second IOHBUSNO attempt\n");
	x58_pcie_topology_iohbusno_attempted = true;
	x58_pcie_topology_log_ioh_routing("PRE");

	if (before != 0x0000 && before != X58_PCIE_TOPOLOGY_IOHBUSNO_BUS0_VALID)
		die_with_post_code(POST_X58_PCIE_TOPOLOGY_IOHBUSNO_FAIL,
			"[IOHCFG] X58_PCIE_TOPOLOGY IOHBUSNO PRE=%04x is outside {0000,0100}\n",
			before);
	if (before == 0x0000) {
		write16p(address, X58_PCIE_TOPOLOGY_IOHBUSNO_BUS0_VALID);
		wrote = true;
	}

	after = read16p(address);
	x58_pcie_topology_log_ioh_routing("POST");
	if (after != X58_PCIE_TOPOLOGY_IOHBUSNO_BUS0_VALID)
		die_with_post_code(POST_X58_PCIE_TOPOLOGY_IOHBUSNO_FAIL,
			"[IOHCFG] X58_PCIE_TOPOLOGY IOHBUSNO readback=%04x expected=0100\n",
			after);
	post_code(POST_X58_PCIE_TOPOLOGY_IOHBUSNO_OK);
	printk(BIOS_NOTICE,
	       "[IOHCFG] X58_PCIE_TOPOLOGY IOHBUSNO exact gate PASS (write=%u)\n",
	       (unsigned int)wrote);
}

struct x58_usb_controller {
	pci_devfn_t dev;
	const char *name;
};

static const struct x58_usb_controller x58_usb_ehci_controllers[] = {
	{ X58_USB_EHCI2_DEV, "EHCI2 00:1a.7" },
	{ X58_USB_EHCI1_DEV, "EHCI1 00:1d.7" },
};

static const struct x58_usb_controller x58_usb_uhci_controllers[] = {
	{ PCI_DEV(0, 0x1a, 0), "UHCI4 00:1a.0" },
	{ PCI_DEV(0, 0x1a, 1), "UHCI5 00:1a.1" },
	{ PCI_DEV(0, 0x1a, 2), "UHCI6 00:1a.2" },
	{ PCI_DEV(0, 0x1d, 0), "UHCI1 00:1d.0" },
	{ PCI_DEV(0, 0x1d, 1), "UHCI2 00:1d.1" },
	{ PCI_DEV(0, 0x1d, 2), "UHCI3 00:1d.2" },
};

static uint32_t x58_usb_ehci_target(uint32_t before)
{
	return (before & ~I82801JX_EHCI_FCREG_REQUIRED_MASK) |
		I82801JX_EHCI_FCREG_REQUIRED_VALUE;
}

static void x58_usb_program_ehci_once(void)
{
	uint32_t before[ARRAY_SIZE(x58_usb_ehci_controllers)];
	uint32_t after[ARRAY_SIZE(x58_usb_ehci_controllers)];

	if (x58_usb_ehci_init_attempted)
		die_with_post_code(POST_X58_USB_EHCI_FAIL,
			"[USB] X58_USB refused a second EHCI initialization attempt\n");
	x58_usb_ehci_init_attempted = true;

	for (size_t i = 0; i < ARRAY_SIZE(x58_usb_ehci_controllers); i++) {
		before[i] = pci_io_read_config32(x58_usb_ehci_controllers[i].dev,
			I82801JX_EHCI_FCREG);
		printk(BIOS_DEBUG,
		       "[USB] %s EHCIIR2 PRE=%08x TARGET=%08x\n",
		       x58_usb_ehci_controllers[i].name, before[i],
		       x58_usb_ehci_target(before[i]));
	}

	post_code(POST_X58_USB_EHCI_BEGIN);
	i82801jx_ehci_init();

	for (size_t i = 0; i < ARRAY_SIZE(x58_usb_ehci_controllers); i++) {
		after[i] = pci_io_read_config32(x58_usb_ehci_controllers[i].dev,
			I82801JX_EHCI_FCREG);
		printk(BIOS_DEBUG, "[USB] %s EHCIIR2 POST=%08x\n",
		       x58_usb_ehci_controllers[i].name, after[i]);
		if (after[i] != x58_usb_ehci_target(before[i]))
			die_with_post_code(POST_X58_USB_EHCI_FAIL,
				"[USB] %s EHCIIR2 readback=%08x expected=%08x\n",
				x58_usb_ehci_controllers[i].name, after[i],
				x58_usb_ehci_target(before[i]));
	}

	post_code(POST_X58_USB_EHCI_OK);
	printk(BIOS_NOTICE,
	       "[USB] X58_USB ICH10 EHCI BIOS-required fields exact gate PASS\n");
}

static bool x58_usb_ehci_bar_valid(uint32_t bar)
{
	const uint32_t base = bar & ~PCI_BASE_ADDRESS_MEM_ATTR_MASK;

	return !(bar & PCI_BASE_ADDRESS_SPACE_IO) &&
		base >= X58_PCI_MMIO_BASE &&
		base <= X58_PCI_MMIO_TOP - X58_USB_EHCI_BAR_SIZE;
}

static void x58_usb_log_ehci_runtime(
	const struct x58_usb_controller *controller)
{
	const uint16_t command = pci_io_read_config16(controller->dev,
		PCI_COMMAND);
	const uint32_t bar = pci_io_read_config32(controller->dev,
		PCI_BASE_ADDRESS_0);
	const uint16_t pmcsr = pci_io_read_config16(controller->dev, 0x54);
	const uint32_t fc = pci_io_read_config32(controller->dev,
		I82801JX_EHCI_FCREG);
	uintptr_t base;
	uintptr_t op;
	uint32_t hcsparams;
	uint8_t caplength;
	uint16_t hciversion;
	unsigned int ports;

	if (!x58_usb_ehci_bar_valid(bar)) {
		printk(BIOS_ERR,
		       "[USB] %s CMD=%04x BAR=%08x PMCSR=%04x EHCIIR2=%08x INVALID_BAR\n",
		       controller->name, command, bar, pmcsr, fc);
		return;
	}
	if (!(command & PCI_COMMAND_MEMORY) || (pmcsr & 3)) {
		printk(BIOS_ERR,
		       "[USB] %s CMD=%04x PMCSR=%04x MMIO decode/D0 gate failed\n",
		       controller->name, command, pmcsr);
		return;
	}

	base = bar & ~PCI_BASE_ADDRESS_MEM_ATTR_MASK;
	caplength = read8p(base);
	hciversion = read16p(base + 2);
	hcsparams = read32p(base + 4);
	ports = hcsparams & 0xf;
	printk(BIOS_DEBUG,
	       "[USB] %s CMD=%04x BAR=%08x PMCSR=%04x EHCIIR2=%08x LEGACY=%08x/%08x/%08x CAPLEN=%02x HCIVER=%04x HCSPARAMS=%08x PORTS=%u\n",
	       controller->name, command, bar, pmcsr, fc,
	       pci_io_read_config32(controller->dev, X58_USB_EHCI_LEGACY_CAP),
	       pci_io_read_config32(controller->dev, X58_USB_EHCI_LEGACY_CTLSTS),
	       pci_io_read_config32(controller->dev, X58_USB_EHCI_LEGACY_EXT),
	       caplength, hciversion, hcsparams, ports);
	printk(BIOS_DEBUG,
	       "[USB-GATE] %s HCCPARAMS=%08x CFG61=%02x LEG68=%08x "
	       "SMI6C=%04x EXT70=%04x CFG84=%08x\n",
	       controller->name,
	       read32p(base + X58_USB_EHCI_CAP_HCCPARAMS),
	       pci_io_read_config8(controller->dev, X58_USB_EHCI_CFG61),
	       pci_io_read_config32(controller->dev, X58_USB_EHCI_LEGACY_CAP),
	       pci_io_read_config16(controller->dev,
			X58_USB_EHCI_LEGACY_CTLSTS) & X58_USB_EHCI_SMI_MASK,
	       pci_io_read_config16(controller->dev,
			X58_USB_EHCI_LEGACY_EXT) & X58_USB_EHCI_EXT_SMI_MASK,
	       pci_io_read_config32(controller->dev, X58_USB_EHCI_CFG84));

	if (caplength < X58_USB_EHCI_CAPLEN_MIN ||
	    caplength > X58_USB_EHCI_CAPLEN_MAX ||
	    hciversion != X58_USB_EHCI_HCIVERSION) {
		printk(BIOS_ERR, "[USB] %s invalid EHCI capability header\n",
		       controller->name);
		return;
	}

	op = base + caplength;
	printk(BIOS_DEBUG,
	       "[USB] %s PRE-SEABIOS USBCMD=%08x USBSTS=%08x USBINTR=%08x CONFIGFLAG=%08x\n",
	       controller->name,
	       read32p(op + X58_USB_EHCI_OP_USBCMD),
	       read32p(op + X58_USB_EHCI_OP_USBSTS),
	       read32p(op + X58_USB_EHCI_OP_USBINTR),
	       read32p(op + X58_USB_EHCI_OP_CONFIGFLAG));
	if (ports > X58_USB_EHCI_PORT_MAX)
		ports = X58_USB_EHCI_PORT_MAX;
	for (unsigned int port = 0; port < ports; port++)
		printk(BIOS_DEBUG, "[USB] %s PORTSC%u=%08x\n",
		       controller->name, port + 1,
		       read32p(op + X58_USB_EHCI_OP_PORTSC + port * sizeof(uint32_t)));
}

static bool x58_usb_uhci_bar_valid(uint32_t bar)
{
	const uint32_t base = bar & ~PCI_BASE_ADDRESS_IO_ATTR_MASK;

	return (bar & PCI_BASE_ADDRESS_SPACE_IO) &&
		base >= X58_PCI_IO_BASE &&
		base <= X58_PCI_IO_TOP - X58_USB_UHCI_BAR_SIZE;
}

static void x58_usb_log_uhci_runtime(
	const struct x58_usb_controller *controller)
{
	const uint16_t command = pci_io_read_config16(controller->dev,
		PCI_COMMAND);
	const uint32_t bar = pci_io_read_config32(controller->dev,
		PCI_BASE_ADDRESS_4);
	uint16_t base;

	if (!x58_usb_uhci_bar_valid(bar)) {
		printk(BIOS_ERR, "[USB] %s CMD=%04x BAR4=%08x INVALID_BAR\n",
		       controller->name, command, bar);
		return;
	}
	if (!(command & PCI_COMMAND_IO)) {
		printk(BIOS_ERR, "[USB] %s CMD=%04x I/O decode gate failed\n",
		       controller->name, command);
		return;
	}

	base = bar & ~PCI_BASE_ADDRESS_IO_ATTR_MASK;
	printk(BIOS_DEBUG,
	       "[USB] %s CMD=%04x BAR4=%08x LEGKEY=%04x PRE-SEABIOS USBCMD=%04x USBSTS=%04x USBINTR=%04x PORTSC1=%04x PORTSC2=%04x\n",
	       controller->name, command, bar,
	       pci_io_read_config16(controller->dev, X58_USB_UHCI_LEGKEY),
	       inw(base + X58_USB_UHCI_USBCMD),
	       inw(base + X58_USB_UHCI_USBSTS),
	       inw(base + X58_USB_UHCI_USBINTR),
	       inw(base + X58_USB_UHCI_PORTSC1),
	       inw(base + X58_USB_UHCI_PORTSC2));
	printk(BIOS_DEBUG,
	       "[USB-GATE] %s CFGC8=%04x CFGCA=%04x\n",
	       controller->name,
	       pci_io_read_config16(controller->dev, X58_USB_UHCI_CFG_C8),
	       pci_io_read_config16(controller->dev, X58_USB_UHCI_CFG_CA));
}

static void x58_usb_log_usb_runtime_once(void)
{
	const uint32_t fd = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_FD);
	const uint32_t cg = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_CG);
	const uint16_t ppo = read16p(CONFIG_FIXED_RCBA_MMIO_BASE + X58_USB_RCBA_PPO);
	const uint32_t map = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_MAP);
	const uint32_t gpio1 = inl(DEFAULT_GPIOBASE + GP_IO_USE_SEL);
	const uint32_t gpio2 = inl(DEFAULT_GPIOBASE + GP_IO_USE_SEL2);

	if (x58_usb_telemetry_emitted)
		die_with_post_code(POST_X58_USB_EHCI_FAIL,
			"[USB] X58_USB refused duplicate pre-SeaBIOS telemetry\n");
	x58_usb_telemetry_emitted = true;
	printk(BIOS_DEBUG,
	       "[USB] ICH10 PRE-SEABIOS PPO=%04x MAP=%08x UPRWC=%04x GPIO_USE=%08x/%08x\n",
	       read16p(CONFIG_FIXED_RCBA_MMIO_BASE + X58_USB_RCBA_PPO),
	       read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_MAP),
	       inw(DEFAULT_PMBASE + X58_USB_PMBASE_UPRWC),
	       inl(DEFAULT_GPIOBASE + GP_IO_USE_SEL),
	       inl(DEFAULT_GPIOBASE + GP_IO_USE_SEL2));
	printk(BIOS_DEBUG,
	       "[USB-GATE] FD=%08x USB_DIS=%04x CG=%08x CG20=%u "
	       "PPO12=%03x MAP0=%u GPIO_OC=%08x/%08x\n",
	       fd, fd & X58_USB_FD_DISABLE_MASK, cg,
	       !!(cg & X58_USB_CG_DISABLE), ppo & X58_USB_PPO_MASK,
	       !!(map & X58_USB_MAP_MODE),
	       gpio1 & X58_USB_GPIO1_OC_MASK,
	       gpio2 & X58_USB_GPIO2_OC_MASK);

	for (size_t i = 0; i < ARRAY_SIZE(x58_usb_ehci_controllers); i++)
		x58_usb_log_ehci_runtime(&x58_usb_ehci_controllers[i]);
	for (size_t i = 0; i < ARRAY_SIZE(x58_usb_uhci_controllers); i++)
		x58_usb_log_uhci_runtime(&x58_usb_uhci_controllers[i]);
}

struct x58_usb_ich10_required_snapshot {
	uint32_t lpc_id;
	uint32_t rcba;
	bool rcba_mmio_read;
	uint8_t fdsw;
	uint32_t cir8;
	uint32_t fd;
	uint32_t cir9;
	uint32_t cir7;
	uint32_t cir13;
	uint32_t cir10;
};

static struct x58_usb_ich10_required_snapshot x58_usb_ich10_read_required_snapshot(void)
{
	struct x58_usb_ich10_required_snapshot snapshot = {
		.lpc_id = pci_io_read_config32(X58_USB_ICH10_LPC_DEV, PCI_VENDOR_ID),
		.rcba = pci_io_read_config32(X58_USB_ICH10_LPC_DEV, RCBA),
	};

	if (snapshot.lpc_id != X58_USB_ICH10_LPC_ID ||
	    snapshot.rcba != X58_USB_ICH10_RCBA_ENABLED)
		return snapshot;

	snapshot.rcba_mmio_read = true;
	snapshot.fdsw = read8p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_FDSW);
	snapshot.cir8 = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_CIR8);
	snapshot.fd = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_FD);
	snapshot.cir9 = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_CIR9);
	snapshot.cir7 = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_CIR7);
	snapshot.cir13 = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_CIR13);
	snapshot.cir10 = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_CIR10);
	return snapshot;
}

static bool x58_usb_ich10_snapshot_is_pre(
	const struct x58_usb_ich10_required_snapshot *snapshot)
{
	return snapshot->cir8 == X58_USB_ICH10_CIR8_PRE &&
		snapshot->fd == X58_USB_ICH10_FD_PRE &&
		snapshot->cir9 == X58_USB_ICH10_CIR9_PRE &&
		snapshot->cir7 == X58_USB_ICH10_CIR7_PRE &&
		snapshot->cir13 == X58_USB_ICH10_CIR13_PRE &&
		snapshot->cir10 == X58_USB_ICH10_CIR10_PRE;
}

static bool x58_usb_ich10_snapshot_is_target(
	const struct x58_usb_ich10_required_snapshot *snapshot)
{
	return snapshot->cir8 == X58_USB_ICH10_CIR8_TARGET &&
		snapshot->fd == X58_USB_ICH10_FD_TARGET &&
		snapshot->cir9 == X58_USB_ICH10_CIR9_TARGET &&
		snapshot->cir7 == X58_USB_ICH10_CIR7_TARGET &&
		snapshot->cir13 == X58_USB_ICH10_CIR13_TARGET &&
		snapshot->cir10 == X58_USB_ICH10_CIR10_TARGET;
}

static bool x58_usb_ich10_snapshot_has_fixed_gates(
	const struct x58_usb_ich10_required_snapshot *snapshot)
{
	return snapshot->lpc_id == X58_USB_ICH10_LPC_ID &&
		snapshot->rcba == X58_USB_ICH10_RCBA_ENABLED &&
		snapshot->rcba_mmio_read &&
		snapshot->fdsw == X58_USB_ICH10_FDSW_UNLOCKED;
}

static void x58_usb_ich10_log_required_snapshot(const char *phase,
	const struct x58_usb_ich10_required_snapshot *snapshot)
{
	printk(BIOS_NOTICE,
	       "[ICHBASE] %s ID=%08x RCBA=%08x MMIO_READ=%u FDSW=%02x CIR8=%08x "
	       "FD=%08x CIR9=%08x CIR7=%08x CIR13=%08x CIR10=%08x\n",
	       phase, snapshot->lpc_id, snapshot->rcba,
	       (unsigned int)snapshot->rcba_mmio_read, snapshot->fdsw,
	       snapshot->cir8, snapshot->fd, snapshot->cir9,
	       snapshot->cir7, snapshot->cir13, snapshot->cir10);
}

/*
 * The complete PRE and TARGET tuples were measured on X58_INTERRUPT_MODE in one retained
 * CAR session. Reject every partial/mixed tuple before the first mutation.
 * Each selected field was then live-written and read back through ROMMON;
 * see 2026-09-06-x58_interrupt_mode-car-ich10-six-required-fields-live.raw.
 */
static void x58_usb_ich10_program_baseline_once(void)
{
	struct x58_usb_ich10_required_snapshot before;
	struct x58_usb_ich10_required_snapshot after;
	bool wrote = false;

	if (x58_usb_ich10_baseline_attempted)
		die_with_post_code(POST_X58_USB_ICH10_BASE_FAIL,
			"[ICHBASE] X58_USB-ICH10_REQUIRED_FIELDS refused a second attempt\n");
	x58_usb_ich10_baseline_attempted = true;

	before = x58_usb_ich10_read_required_snapshot();
	x58_usb_ich10_log_required_snapshot("PRE", &before);
	if (!x58_usb_ich10_snapshot_has_fixed_gates(&before) ||
	    (!x58_usb_ich10_snapshot_is_pre(&before) &&
	     !x58_usb_ich10_snapshot_is_target(&before)))
		die_with_post_code(POST_X58_USB_ICH10_BASE_FAIL,
			"[ICHBASE] rejected identity/RCBA/FDSW or mixed six-field prestate\n");

	post_code(POST_X58_USB_ICH10_BASE_BEGIN);
	if (x58_usb_ich10_snapshot_is_pre(&before)) {
		i82801jx_program_required_fields();
		wrote = true;
	}
	after = x58_usb_ich10_read_required_snapshot();
	x58_usb_ich10_log_required_snapshot("POST", &after);
	if (!x58_usb_ich10_snapshot_has_fixed_gates(&after) ||
	    !x58_usb_ich10_snapshot_is_target(&after))
		die_with_post_code(POST_X58_USB_ICH10_BASE_FAIL,
			"[ICHBASE] six-field complete target readback failed\n");

	post_code(POST_X58_USB_ICH10_BASE_OK);
	printk(BIOS_NOTICE,
	       "[ICHBASE] X58_USB-ICH10_REQUIRED_FIELDS six required fields exact gate PASS "
	       "(write=%u); GCS/CIR5/FDSW/hide/lock/RPFN/MAP/PMIR/IRQ untouched\n",
	       (unsigned int)wrote);
}

struct x58_ioapic_redir {
	uint32_t low;
	uint32_t high;
};

_Static_assert(X58_IOAPIC_MAX_REDIR + 1 == X58_IOAPIC_REDIR_COUNT,
	"X58_IOAPIC IOAPIC redirection-entry count changed");
_Static_assert(X58_IOAPIC_VERSION_TARGET ==
	((X58_IOAPIC_MAX_REDIR << 16) | X58_IOAPIC_VERSION),
	"X58_IOAPIC exact IOAPIC version tuple changed");
_Static_assert(X58_IOAPIC_LOW_TARGET == X58_IOAPIC_MASK_BIT,
	"X58_IOAPIC canonical low entry must contain only the mask bit");

static bool x58_ioapic_select(uint8_t reg)
{
	write32p(X58_IOAPIC_BASE + X58_IOAPIC_IOREGSEL, reg);
	return read32p(X58_IOAPIC_BASE + X58_IOAPIC_IOREGSEL) == reg;
}

static bool x58_ioapic_read(uint8_t reg, uint32_t *value)
{
	if (!x58_ioapic_select(reg))
		return false;
	*value = read32p(X58_IOAPIC_BASE + X58_IOAPIC_IOWIN);
	return true;
}

static bool x58_ioapic_write_exact(uint8_t reg, uint32_t value)
{
	if (!x58_ioapic_select(reg))
		return false;
	write32p(X58_IOAPIC_BASE + X58_IOAPIC_IOWIN, value);
	return read32p(X58_IOAPIC_BASE + X58_IOAPIC_IOWIN) == value;
}

static void x58_ioapic_fail(const char *reason)
{
	/* Best effort keeps the indirect selector deterministic even on failure. */
	write32p(X58_IOAPIC_BASE + X58_IOAPIC_IOREGSEL, X58_IOAPIC_ID_REG);
	printk(BIOS_ERR,
	       "[IOAPIC] X58_IOAPIC FAIL reason=%s IOREGSEL=%08x\n", reason,
	       read32p(X58_IOAPIC_BASE + X58_IOAPIC_IOREGSEL));
	die_with_post_code(POST_X58_IOAPIC_FAIL,
		"[IOAPIC] X58_IOAPIC fail-closed before PCI enumeration\n");
}


static bool x58_ioapic_redirection_snapshot(
	struct x58_ioapic_redir redir[X58_IOAPIC_REDIR_COUNT],
	const char *phase)
{
	bool all_masked = true;

	for (unsigned int entry = 0;
	     entry < X58_IOAPIC_REDIR_COUNT; entry++) {
		const uint8_t low_reg = X58_IOAPIC_REDIR_BASE + 2 * entry;
		const uint8_t high_reg = low_reg + 1;

		if (!x58_ioapic_read(low_reg, &redir[entry].low) ||
		    !x58_ioapic_read(high_reg, &redir[entry].high))
			x58_ioapic_fail("SELECTOR_READBACK");
		all_masked &= (redir[entry].low & X58_IOAPIC_MASK_BIT) != 0;
		printk(BIOS_NOTICE,
		       "[IOAPIC] X58_IOAPIC %s E%02u LOW=%08x HIGH=%08x MASKED=%u\n",
		       phase, entry, redir[entry].low, redir[entry].high,
		       !!(redir[entry].low & X58_IOAPIC_MASK_BIT));
	}
	return all_masked;
}

/*
 * The two X58_INTERRUPT_MODE cold censuses found different undefined redirection contents,
 * but all 24 entries were masked.  A separate live entry-23 transaction proved
 * HIGH-then-LOW indirect writes, exact readback, and selector restoration.  Do
 * not copy any reset vector/delivery/destination bits: canonicalize only while
 * every entry is still masked, and leave routing to the payload.
 */
static void __maybe_unused x58_ioapic_decode_and_mask_ioapic_once(void)
{
	struct x58_ioapic_redir before[X58_IOAPIC_REDIR_COUNT];
	struct x58_ioapic_redir after[X58_IOAPIC_REDIR_COUNT];
	const uint32_t lpc_id = pci_io_read_config32(X58_IOAPIC_LPC_DEV,
		PCI_VENDOR_ID);
	const uint32_t rcba = pci_io_read_config32(X58_IOAPIC_LPC_DEV, RCBA);
	uint8_t oic;
	uint32_t ioapic_id;
	uint32_t ioapic_version;
	unsigned int changed = 0;
	bool decode_write = false;
	bool mre_lock_write = false;

	if (x58_ioapic_attempted)
		die_with_post_code(POST_X58_IOAPIC_FAIL,
			"[IOAPIC] X58_IOAPIC refused a second initialization attempt\n");
	x58_ioapic_attempted = true;
	if (!x58_usb_ich10_baseline_attempted || !x58_sata_config_ahci_route_attempted)
		die_with_post_code(POST_X58_IOAPIC_FAIL,
			"[IOAPIC] X58_IOAPIC inherited ICHBASE/AHCI-route order failed\n");
	if (lpc_id != X58_IOAPIC_LPC_ID || rcba != X58_IOAPIC_RCBA_ENABLED)
		die_with_post_code(POST_X58_IOAPIC_FAIL,
			"[IOAPIC] X58_IOAPIC LPC/RCBA gate failed: ID=%08x RCBA=%08x\n",
			lpc_id, rcba);

	oic = read8p(CONFIG_FIXED_RCBA_MMIO_BASE + OIC);
	printk(BIOS_NOTICE,
	       "[IOAPIC] X58_IOAPIC PRE LPC_ID=%08x RCBA=%08x OIC=%02x\n",
	       lpc_id, rcba, oic);
	if (oic != X58_IOAPIC_OIC_DISABLED && oic != X58_IOAPIC_OIC_ENABLED)
		die_with_post_code(POST_X58_IOAPIC_FAIL,
			"[IOAPIC] X58_IOAPIC OIC=%02x outside exact {00,03} allowlist\n",
			oic);

	post_code(POST_X58_IOAPIC_DECODE_BEGIN);
	if (oic == X58_IOAPIC_OIC_DISABLED) {
		write8p(CONFIG_FIXED_RCBA_MMIO_BASE + OIC,
			X58_IOAPIC_OIC_ENABLED);
		decode_write = true;
	}
	oic = read8p(CONFIG_FIXED_RCBA_MMIO_BASE + OIC);
	if (oic != X58_IOAPIC_OIC_ENABLED)
		die_with_post_code(POST_X58_IOAPIC_FAIL,
			"[IOAPIC] X58_IOAPIC OIC exact readback failed: %02x\n", oic);

	if (!x58_ioapic_read(X58_IOAPIC_ID_REG, &ioapic_id) ||
	    !x58_ioapic_read(X58_IOAPIC_VERSION_REG, &ioapic_version))
		x58_ioapic_fail("ID_VERSION_SELECTOR");
	printk(BIOS_NOTICE,
	       "[IOAPIC] X58_IOAPIC DECODE OIC=%02x WRITE=%u ID=%08x VERSION=%08x "
	       "VER=%02x MAXRED=%02x COUNT=%u\n",
	       oic, (unsigned int)decode_write, ioapic_id, ioapic_version,
	       ioapic_version & 0xff, (ioapic_version >> 16) & 0xff,
	       ((ioapic_version >> 16) & 0xff) + 1);
	if (ioapic_id != X58_IOAPIC_ID_TARGET ||
	    ioapic_version != X58_IOAPIC_VERSION_TARGET) {
		x58_ioapic_fail("ID_VERSION_TUPLE");
	}
	post_code(POST_X58_IOAPIC_DECODE_OK);

	if (!x58_ioapic_redirection_snapshot(before, "PRE"))
		x58_ioapic_fail("PRE_ENTRY_UNMASKED");
	post_code(POST_X58_IOAPIC_CENSUS_OK);

	for (unsigned int entry = 0;
	     entry < X58_IOAPIC_REDIR_COUNT; entry++) {
		const uint8_t low_reg = X58_IOAPIC_REDIR_BASE + 2 * entry;
		const uint8_t high_reg = low_reg + 1;
		uint32_t low_now;

		/* Reconfirm masking immediately before this entry's first write. */
		if (!x58_ioapic_read(low_reg, &low_now) ||
		    !(low_now & X58_IOAPIC_MASK_BIT))
			x58_ioapic_fail("ENTRY_CHANGED_OR_UNMASKED");
		if (before[entry].high != X58_IOAPIC_HIGH_TARGET ||
		    before[entry].low != X58_IOAPIC_LOW_TARGET)
			changed++;
		if (before[entry].high != X58_IOAPIC_HIGH_TARGET &&
		    !x58_ioapic_write_exact(high_reg,
			X58_IOAPIC_HIGH_TARGET))
			x58_ioapic_fail("HIGH_WRITE_READBACK");
		if (low_now != X58_IOAPIC_LOW_TARGET &&
		    !x58_ioapic_write_exact(low_reg,
			X58_IOAPIC_LOW_TARGET))
			x58_ioapic_fail("LOW_WRITE_READBACK");
	}

	if (!x58_ioapic_redirection_snapshot(after, "POST"))
		x58_ioapic_fail("POST_ENTRY_UNMASKED");
	for (unsigned int entry = 0;
	     entry < X58_IOAPIC_REDIR_COUNT; entry++) {
		if (after[entry].low != X58_IOAPIC_LOW_TARGET ||
		    after[entry].high != X58_IOAPIC_HIGH_TARGET)
			x58_ioapic_fail("POST_ENTRY_NONCANONICAL");
	}
	if (!x58_ioapic_select(X58_IOAPIC_ID_REG))
		x58_ioapic_fail("FINAL_SELECTOR_RESTORE");

	post_code(POST_X58_IOAPIC_READY);
	printk(BIOS_NOTICE,
	       "[IOAPIC] X58_IOAPIC READY ENTRIES=24 CHANGED=%u IOREGSEL=00000000 "
	       "IOAPIC_EXTINT_ROUTE=0 MRE_LOCK_WRITE=%u PIRQ_WRITE=0 "
	       "SCI_WRITE=0 PAYLOAD_OWNS_ROUTING=1\n",
	       changed, (unsigned int)mre_lock_write);
}

struct x58_tco_snapshot {
	uint32_t gcs;
	uint16_t tco_rld;
	uint16_t tco1_sts;
	uint16_t tco2_sts;
	uint16_t tco1_cnt;
	uint16_t tco2_cnt;
	uint16_t tco_tmr;
};

static void x58_tco_read_tco_snapshot(struct x58_tco_snapshot *snapshot)
{
	snapshot->gcs = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + GCS);
	snapshot->tco_rld = inw(X58_TCO_BASE + X58_TCO_RLD);
	snapshot->tco1_sts = inw(X58_TCO_BASE + X58_TCO1_STS);
	snapshot->tco2_sts = inw(X58_TCO_BASE + X58_TCO2_STS);
	snapshot->tco1_cnt = inw(X58_TCO_BASE + X58_TCO1_CNT);
	snapshot->tco2_cnt = inw(X58_TCO_BASE + X58_TCO2_CNT);
	snapshot->tco_tmr = inw(X58_TCO_BASE + X58_TCO_TMR);
}

static void x58_tco_log_tco_snapshot(const char *phase,
	const struct x58_tco_snapshot *snapshot, bool control_write)
{
	printk(BIOS_NOTICE,
	       "[TCO] X58_TCO %s GCS=%08x TCO_RLD=%04x TCO1_STS=%04x "
	       "TCO2_STS=%04x TCO1_CNT=%04x TCO2_CNT=%04x "
	       "TCO_TMR=%04x WRITE=%u\n",
	       phase, snapshot->gcs, (unsigned int)snapshot->tco_rld,
	       (unsigned int)snapshot->tco1_sts,
	       (unsigned int)snapshot->tco2_sts,
	       (unsigned int)snapshot->tco1_cnt,
	       (unsigned int)snapshot->tco2_cnt,
	       (unsigned int)snapshot->tco_tmr,
	       (unsigned int)control_write);
}

static void x58_tco_fail(const char *reason)
{
	die_with_post_code(POST_X58_TCO_FAIL,
		"[TCO] X58_TCO fail-closed: %s\n", reason);
}

/*
 * X58_TCO's only possible hardware mutation is a 16-bit masked/RMW transition
 * of complete TCO1_CNT 0000 -> 0800.  All decode and exact-prestate gates run
 * first.  Status, reload, timer, GCS and TCO_LOCK remain read-only.
 */
static void x58_tco_halt_tco_once(void)
{
	const uint32_t lpc_id = pci_io_read_config32(X58_IOAPIC_LPC_DEV,
		PCI_VENDOR_ID);
	const uint32_t rcba = pci_io_read_config32(X58_IOAPIC_LPC_DEV, RCBA);
	const uint32_t pmbase = pci_io_read_config32(X58_IOAPIC_LPC_DEV,
		X58_TCO_PMBASE_REG);
	const uint8_t acpi_cntl = pci_io_read_config8(X58_IOAPIC_LPC_DEV,
		X58_TCO_ACPI_CNTL_REG);
	struct x58_tco_snapshot before;
	struct x58_tco_snapshot after;
	bool control_write = false;

	if (x58_tco_attempted)
		x58_tco_fail("SECOND_ATTEMPT");
	x58_tco_attempted = true;
	if (x58_usb_ich10_baseline_attempted || x58_sata_config_ahci_route_attempted ||
	    x58_ioapic_attempted || x58_hpet_attempted)
		x58_tco_fail("LATER_STAGE_ALREADY_ATTEMPTED");
	if (lpc_id != X58_IOAPIC_LPC_ID || rcba != X58_IOAPIC_RCBA_ENABLED)
		x58_tco_fail("LPC_RCBA_IDENTITY");
	if (pmbase != X58_TCO_PMBASE_ENABLED ||
	    acpi_cntl != X58_TCO_ACPI_DECODE_ENABLED)
		x58_tco_fail("PM_DECODE_NOT_EXACT");

	x58_tco_read_tco_snapshot(&before);
	if (before.tco1_cnt != X58_TCO1_CNT_PRE &&
	    before.tco1_cnt != X58_TCO1_CNT_TARGET)
		x58_tco_fail("TCO1_CNT_OUTSIDE_EXACT_ALLOWLIST");

	printk(BIOS_NOTICE,
	       "[TCO] X58_TCO DECODE LPC_ID=%08x RCBA=%08x PMBASE=%08x "
	       "ACPI_CNTL=%02x ALLOWLIST=0000,0800\n",
	       lpc_id, rcba, pmbase, (unsigned int)acpi_cntl);
	x58_tco_log_tco_snapshot("PRE", &before, false);
	post_code(POST_X58_TCO_BEGIN);
	if (before.tco1_cnt == X58_TCO1_CNT_PRE) {
		const uint16_t target =
			(before.tco1_cnt & (uint16_t)~X58_TCO1_CNT_HLT) |
			X58_TCO1_CNT_HLT;

		if (target != X58_TCO1_CNT_TARGET)
			x58_tco_fail("TCO1_CNT_TARGET_CONSTRUCTION");
		outw(target, X58_TCO_BASE + X58_TCO1_CNT);
		control_write = true;
	}

	x58_tco_read_tco_snapshot(&after);
	x58_tco_log_tco_snapshot("POST", &after, control_write);
	if (after.tco1_cnt != X58_TCO1_CNT_TARGET)
		x58_tco_fail("TCO1_CNT_EXACT_READBACK");
	if (after.tco1_sts != before.tco1_sts ||
	    after.tco2_sts != before.tco2_sts)
		x58_tco_fail("STATUS_BITS_CHANGED");
	if (after.gcs != before.gcs || after.tco2_cnt != before.tco2_cnt ||
	    after.tco_tmr != before.tco_tmr)
		x58_tco_fail("READ_ONLY_STATE_CHANGED");
	if (pci_io_read_config32(X58_IOAPIC_LPC_DEV, PCI_VENDOR_ID) !=
		X58_IOAPIC_LPC_ID ||
	    pci_io_read_config32(X58_IOAPIC_LPC_DEV, RCBA) !=
		X58_IOAPIC_RCBA_ENABLED ||
	    pci_io_read_config32(X58_IOAPIC_LPC_DEV, X58_TCO_PMBASE_REG) !=
		X58_TCO_PMBASE_ENABLED ||
	    pci_io_read_config8(X58_IOAPIC_LPC_DEV, X58_TCO_ACPI_CNTL_REG) !=
		X58_TCO_ACPI_DECODE_ENABLED ||
	    read32p(CONFIG_FIXED_RCBA_MMIO_BASE + GCS) != after.gcs ||
	    inw(X58_TCO_BASE + X58_TCO1_STS) != after.tco1_sts ||
	    inw(X58_TCO_BASE + X58_TCO2_STS) != after.tco2_sts ||
	    inw(X58_TCO_BASE + X58_TCO1_CNT) !=
		X58_TCO1_CNT_TARGET ||
	    inw(X58_TCO_BASE + X58_TCO2_CNT) != after.tco2_cnt ||
	    inw(X58_TCO_BASE + X58_TCO_TMR) != after.tco_tmr)
		x58_tco_fail("FINAL_STATE_RECHECK");

	x58_tco_ready = true;
	post_code(POST_X58_TCO_READY);
	printk(BIOS_NOTICE,
	       "[TCO] X58_TCO READY TCO1_CNT=0800 STATUS_PRESERVED=1 "
	       "GCS_WRITE=0 STATUS_WRITE=0 RELOAD_WRITE=0 TIMER_WRITE=0 "
	       "TCO_LOCK_WRITE=0\n");
}

static bool x58_acpi_pic_tuple_is_reset(uint8_t master_mask,
	uint8_t slave_mask, uint8_t elcr1, uint8_t elcr2)
{
	return master_mask == X58_ACPI_PIC_MASTER_MASK_RESET &&
		slave_mask == X58_ACPI_PIC_SLAVE_MASK_RESET &&
		elcr1 == X58_ACPI_ELCR1_RESET && elcr2 == X58_ACPI_ELCR2_RESET;
}

static uint32_t x58_acpi_read_eflags(void)
{
	uint32_t flags;

	asm volatile("pushfl; popl %0" : "=rm" (flags));
	return flags;
}

static bool x58_acpi_pic_tuple_is_target(uint8_t master_mask,
	uint8_t slave_mask, uint8_t elcr1, uint8_t elcr2)
{
	return master_mask == X58_ACPI_PIC_MASTER_MASK_TARGET &&
		slave_mask == X58_ACPI_PIC_SLAVE_MASK_TARGET &&
		elcr1 == X58_ACPI_ELCR1_TARGET && elcr2 == X58_ACPI_ELCR2_TARGET;
}

static __noreturn void x58_acpi_pic_fail(const char *reason)
{
	die_with_post_code(POST_X58_ACPI_PIC_FAIL,
		"[PIC] X58_ACPI fail-closed: %s\n", reason);
}

/*
 * Establish a deterministic PC/AT virtual-wire source for SeaBIOS.  The live
 * X58_INTERRUPT_MODE transaction proved this exact upstream sequence and a bounded PIT
 * one-shot proved that IRQ0 reaches the master PIC IRR.  Interrupt delivery is
 * still left disabled here; LAPIC ExtINT/NMI setup remains in the BSP device
 * init callback and SeaBIOS owns the first enabled interrupt window.
 */
static void __maybe_unused x58_acpi_initialize_pic_once(void)
{
	const uint32_t eflags_before = x58_acpi_read_eflags();
	const uint8_t master_before = inb(MASTER_PIC_OCW1);
	const uint8_t slave_before = inb(SLAVE_PIC_OCW1);
	const uint8_t elcr1_before = inb(ELCR1);
	const uint8_t elcr2_before = inb(ELCR2);
	uint8_t master_after;
	uint8_t slave_after;
	uint8_t elcr1_after;
	uint8_t elcr2_after;

	if (x58_acpi_pic_attempted)
		x58_acpi_pic_fail("SECOND_ATTEMPT");
	x58_acpi_pic_attempted = true;
	if (!x58_tco_ready || x58_usb_ich10_baseline_attempted ||
	    x58_sata_config_ahci_route_attempted || x58_ioapic_attempted ||
	    x58_hpet_attempted)
		x58_acpi_pic_fail("STAGE_ORDER");
	if (eflags_before & X86_EFLAGS_IF)
		x58_acpi_pic_fail("CPU_INTERRUPTS_ENABLED");
	if (!x58_acpi_pic_tuple_is_reset(master_before, slave_before,
		elcr1_before, elcr2_before) &&
	    !x58_acpi_pic_tuple_is_target(master_before, slave_before,
		elcr1_before, elcr2_before))
		x58_acpi_pic_fail("PRE_TUPLE_OUTSIDE_ALLOWLIST");

	printk(BIOS_NOTICE,
	       "[PIC] X58_ACPI PRE IMR=%02x/%02x ELCR=%02x/%02x "
	       "EFLAGS=%08x ALLOWLIST=RESET,TARGET IF_ENABLE=0 EOI_WRITE=0\n",
	       master_before, slave_before, elcr1_before, elcr2_before,
	       eflags_before);
	post_code(POST_X58_ACPI_PIC_BEGIN);
	setup_i8259();
	i8259_configure_irq_trigger(IRQ_9, IRQ_LEVEL_TRIGGERED);

	master_after = inb(MASTER_PIC_OCW1);
	slave_after = inb(SLAVE_PIC_OCW1);
	elcr1_after = inb(ELCR1);
	elcr2_after = inb(ELCR2);
	printk(BIOS_NOTICE,
	       "[PIC] X58_ACPI POST IMR=%02x/%02x ELCR=%02x/%02x "
	       "VECTORS=20/28 IRQ9=LEVEL IRQ0=MASKED\n",
	       master_after, slave_after, elcr1_after, elcr2_after);
	if (!x58_acpi_pic_tuple_is_target(master_after, slave_after,
		elcr1_after, elcr2_after))
		x58_acpi_pic_fail("TARGET_READBACK");

	/* Re-read every readable byte immediately before releasing the stage. */
	if (inb(MASTER_PIC_OCW1) != X58_ACPI_PIC_MASTER_MASK_TARGET ||
	    inb(SLAVE_PIC_OCW1) != X58_ACPI_PIC_SLAVE_MASK_TARGET ||
	    inb(ELCR1) != X58_ACPI_ELCR1_TARGET ||
	    inb(ELCR2) != X58_ACPI_ELCR2_TARGET)
		x58_acpi_pic_fail("FINAL_STATE_RECHECK");

	x58_acpi_pic_ready = true;
	post_code(POST_X58_ACPI_PIC_READY);
	printk(BIOS_NOTICE,
	       "[PIC] X58_ACPI READY STANDARD_I8259=1 SCI_IRQ9_LEVEL=1 "
	       "DELIVERY_TESTED_IN_ROMMON=AVAILABLE PAYLOAD_OWNS_IRQS=1\n");
}

static __noreturn void x58_acpi_mode_fail(const char *reason)
{
	die_with_post_code(POST_X58_ACPI_FAIL,
		"[ACPI-MODE] X58_ACPI fail-closed: %s\n", reason);
}

static bool x58_acpi_irq9_is_masked(void)
{
	uint32_t low = 0;
	uint32_t high = 0;
	bool readable;
	bool restored;

	if (!(inb(SLAVE_PIC_OCW1) & BIT(IRQ_9 - 8)))
		return false;
	readable = x58_ioapic_read(X58_ACPI_IOAPIC_SCI_LOW_REG, &low) &&
		x58_ioapic_read(X58_ACPI_IOAPIC_SCI_HIGH_REG, &high);
	restored = x58_ioapic_select(X58_IOAPIC_ID_REG);
	return readable && restored && low == X58_IOAPIC_LOW_TARGET &&
		high == X58_IOAPIC_HIGH_TARGET;
}

static bool x58_acpi_enables_are_quiescent(void)
{
	return inw(DEFAULT_PMBASE + PM1_EN) == 0 &&
		x58_ich10_gpe0_low_quiet(inl(DEFAULT_PMBASE + GPE0_EN)) &&
		inl(DEFAULT_PMBASE + GPE0_EN + 4) == 0 &&
		inl(DEFAULT_PMBASE + SMI_EN) == 0 &&
		inw(DEFAULT_PMBASE + ALT_GP_SMI_EN) == 0 &&
		inw(DEFAULT_PMBASE + X58_ACPI_UPRWC) == 0 &&
		pci_io_read_config32(X58_IOAPIC_LPC_DEV, D31F0_GPIO_ROUT) == 0;
}

/* Intel 319973-003 section13.8.3.10, p473: PME_EN/PME_B0_EN are R/W
 * RTC-well enables, not event status. The captured 2c00h state cannot be
 * admitted as quiet. Establish our no-wake cold-boot policy before SCI_EN,
 * preserving reserved bit10 and refusing every other enabled source.
 * The caller's attempted latch is already set; no retry or status ACK.
 */
static bool x58_acpi_quiesce_pme_enables(void)
{
	_Static_assert(CONFIG(NO_SMM) &&
		!CONFIG(HAVE_ACPI_RESUME), "PME normalization is cold-boot/no-SMM only");
	const uint32_t before = inl(DEFAULT_PMBASE + GPE0_EN);
	const uint32_t target = x58_ich10_gpe0_mask_pme(before);
	uint32_t pm1_cnt, after;

	if (x58_ich10_gpe0_low_quiet(before))
		return false;
	printk(BIOS_NOTICE, "[PME-QUIESCE] PRE GPE_EN_LO=%08x CLEAR_MASK=%08x "
		"TARGET=%08x PM_WRITES=0\n", before, X58_ICH10_GPE0_PME_MASK, target);
	if (!x58_ich10_gpe0_pme_prestate(before))
		x58_acpi_mode_fail("PME_UNSUPPORTED_GPE_ENABLE");
	/* Recheck the complete write prerequisites, not just the changed bits. */
	if (pci_io_read_config32(X58_IOAPIC_LPC_DEV, PCI_VENDOR_ID) != X58_IOAPIC_LPC_ID ||
	    pci_io_read_config32(X58_IOAPIC_LPC_DEV, RCBA) != X58_IOAPIC_RCBA_ENABLED ||
	    pci_io_read_config32(X58_IOAPIC_LPC_DEV, D31F0_PMBASE) != X58_TCO_PMBASE_ENABLED ||
	    pci_io_read_config8(X58_IOAPIC_LPC_DEV, D31F0_ACPI_CNTL) !=
		X58_TCO_ACPI_DECODE_ENABLED ||
	    (x58_acpi_read_eflags() & X86_EFLAGS_IF) ||
	    !x58_acpi_pic_tuple_is_target(inb(MASTER_PIC_OCW1), inb(SLAVE_PIC_OCW1),
		inb(ELCR1), inb(ELCR2)) || !x58_acpi_irq9_is_masked() ||
	    inw(DEFAULT_PMBASE + PM1_EN) != 0 ||
	    inl(DEFAULT_PMBASE + GPE0_EN + 4) != 0 ||
	    inl(DEFAULT_PMBASE + SMI_EN) != 0 ||
	    inw(DEFAULT_PMBASE + ALT_GP_SMI_EN) != 0 ||
	    inw(DEFAULT_PMBASE + X58_ACPI_UPRWC) != 0 ||
	    pci_io_read_config32(X58_IOAPIC_LPC_DEV, D31F0_GPIO_ROUT) != 0)
		x58_acpi_mode_fail("PME_QUIESCE_PREREQUISITES");
	pm1_cnt = inl(DEFAULT_PMBASE + PM1_CNT);
	if (pm1_cnt != X58_ACPI_PM1_CNT_DWORD_PRE && pm1_cnt != X58_ACPI_PM1_CNT_DWORD_TARGET)
		x58_acpi_mode_fail("PME_PM1_CNT_OUTSIDE_ALLOWLIST");
	if (inl(DEFAULT_PMBASE + GPE0_EN) != before)
		x58_acpi_mode_fail("PME_ENABLE_CHANGED_BEFORE_WRITE");
	outl(target, DEFAULT_PMBASE + GPE0_EN);
	after = inl(DEFAULT_PMBASE + GPE0_EN);
	printk(BIOS_NOTICE, "[PME-QUIESCE] POST GPE_EN_LO=%08x EXPECTED=%08x "
		"PM_WRITES=1 GPE_STS_WRITE=0 RETRY=0\n", after, target);
	if (after != target)
		x58_acpi_mode_fail("PME_ENABLE_READBACK");
	if (!x58_acpi_enables_are_quiescent() || !x58_acpi_irq9_is_masked() ||
	    (x58_acpi_read_eflags() & X86_EFLAGS_IF))
		x58_acpi_mode_fail("PME_QUIESCENT_STATE_RECHECK");
	return true;
}

/*
 * Enter ACPI mode without enabling a single event source.  With NO_SMM there
 * is intentionally no APMC/SMI transition: firmware establishes SCI_EN
 * directly, while both possible IRQ9 delivery paths stay masked.  This makes
 * the board FADT's SMI_CMD=0 contract truthful before ACPI tables are emitted.
 */
static void x58_acpi_enter_quiet_acpi_mode_once(void)
{
	const uint32_t lpc_id = pci_io_read_config32(X58_IOAPIC_LPC_DEV,
		PCI_VENDOR_ID);
	const uint32_t rcba = pci_io_read_config32(X58_IOAPIC_LPC_DEV, RCBA);
	const uint32_t pmbase = pci_io_read_config32(X58_IOAPIC_LPC_DEV,
		D31F0_PMBASE);
	const uint8_t acpi_cntl = pci_io_read_config8(X58_IOAPIC_LPC_DEV,
		D31F0_ACPI_CNTL);
	const uint32_t eflags = x58_acpi_read_eflags();
	uint16_t pm1_sts;
	uint32_t pm1_cnt;
	bool status_write = false;
	bool control_write = false;
	bool event_enable_write = false;

	if (x58_acpi_mode_attempted)
		x58_acpi_mode_fail("SECOND_ATTEMPT");
	x58_acpi_mode_attempted = true;
	if (!x58_pci_standard_lpc_attempted || !x58_tco_ready || !x58_hpet_ready)
		x58_acpi_mode_fail("STAGE_ORDER");
	if (lpc_id != X58_IOAPIC_LPC_ID || rcba != X58_IOAPIC_RCBA_ENABLED ||
	    pmbase != X58_TCO_PMBASE_ENABLED ||
	    acpi_cntl != X58_TCO_ACPI_DECODE_ENABLED)
		x58_acpi_mode_fail("LPC_PM_DECODE_IDENTITY");
	if (eflags & X86_EFLAGS_IF)
		x58_acpi_mode_fail("CPU_INTERRUPTS_ENABLED");
	if (!x58_acpi_pic_tuple_is_target(inb(MASTER_PIC_OCW1),
		inb(SLAVE_PIC_OCW1), inb(ELCR1), inb(ELCR2)))
		x58_acpi_mode_fail("PIC_ELCR_TARGET_CHANGED");
	if (!x58_acpi_irq9_is_masked())
		x58_acpi_mode_fail("SCI_DELIVERY_NOT_MASKED");
	event_enable_write = x58_acpi_quiesce_pme_enables();
	if (!x58_acpi_enables_are_quiescent()) {
		/* Control-register snapshot only: never acknowledge status or disable
		 * an unknown source to make the admission predicate pass. The reads
		 * are subsequent, non-atomic observations, not the failed predicate. */
		printk(BIOS_EMERG,
		       "[ACPI-MODE] REJECT_SNAPSHOT PMBASE=%04x PM1_EN=%04x "
		       "GPE_EN=%08x:%08x SMI_EN=%08x ALT_EN=%04x "
		       "UPRWC=%04x GPIO_ROUT=%08x NONATOMIC=1 PM_WRITES=%u\n",
		       DEFAULT_PMBASE, inw(DEFAULT_PMBASE + PM1_EN),
		       inl(DEFAULT_PMBASE + GPE0_EN + 4),
		       inl(DEFAULT_PMBASE + GPE0_EN),
		       inl(DEFAULT_PMBASE + SMI_EN),
		       inw(DEFAULT_PMBASE + ALT_GP_SMI_EN),
		       inw(DEFAULT_PMBASE + X58_ACPI_UPRWC),
		       pci_io_read_config32(X58_IOAPIC_LPC_DEV, D31F0_GPIO_ROUT),
		       (unsigned int)event_enable_write);
		x58_acpi_mode_fail("EVENT_ENABLE_OR_GPIO_ROUTE_ACTIVE");
	}

	pm1_cnt = inl(DEFAULT_PMBASE + PM1_CNT);
	if (pm1_cnt != X58_ACPI_PM1_CNT_DWORD_PRE &&
	    pm1_cnt != X58_ACPI_PM1_CNT_DWORD_TARGET)
		x58_acpi_mode_fail("PM1_CNT_OUTSIDE_EXACT_ALLOWLIST");
	pm1_sts = inw(DEFAULT_PMBASE + PM1_STS);
	printk(BIOS_NOTICE,
	       "[ACPI-MODE] X58_ACPI PRE PM1_STS=%04x PM1_CNT=%08x "
	       "PM1_EN=%04x GPE_EN=%08x:%08x SMI_EN=%08x ALT_EN=%04x "
	       "UPRWC=%04x GPIO_ROUT=%08x IRQ9_MASKED=1 EFLAGS=%08x "
	       "GPE_LOW_IGNORED_MASK=00000400\n",
	       pm1_sts, pm1_cnt, inw(DEFAULT_PMBASE + PM1_EN),
	       inl(DEFAULT_PMBASE + GPE0_EN + 4),
	       inl(DEFAULT_PMBASE + GPE0_EN),
	       inl(DEFAULT_PMBASE + SMI_EN),
	       inw(DEFAULT_PMBASE + ALT_GP_SMI_EN),
	       inw(DEFAULT_PMBASE + X58_ACPI_UPRWC),
	       pci_io_read_config32(X58_IOAPIC_LPC_DEV, D31F0_GPIO_ROUT), eflags);

	post_code(POST_X58_ACPI_BEGIN);
	if (pm1_sts & PRBTNOR_STS) {
		/* W1C only the observed unsafe power-button-override status. */
		outw(PRBTNOR_STS, DEFAULT_PMBASE + PM1_STS);
		status_write = true;
	}
	if (inw(DEFAULT_PMBASE + PM1_STS) & PRBTNOR_STS)
		x58_acpi_mode_fail("PRBTNOR_STS_DID_NOT_CLEAR");
	if (!x58_acpi_enables_are_quiescent() ||
	    !x58_acpi_irq9_is_masked())
		x58_acpi_mode_fail("QUIESCENT_STATE_CHANGED_AFTER_W1C");
	post_code(POST_X58_ACPI_STATUS);

	pm1_cnt = inl(DEFAULT_PMBASE + PM1_CNT);
	if (pm1_cnt == X58_ACPI_PM1_CNT_DWORD_PRE) {
		const uint16_t target =
			(inw(DEFAULT_PMBASE + PM1_CNT) & ~X58_ACPI_PM1_CNT_TARGET) |
			X58_ACPI_PM1_CNT_TARGET;

		if (target != X58_ACPI_PM1_CNT_TARGET)
			x58_acpi_mode_fail("PM1_CNT_TARGET_CONSTRUCTION");
		outw(target, DEFAULT_PMBASE + PM1_CNT);
		control_write = true;
	}
	if (inl(DEFAULT_PMBASE + PM1_CNT) != X58_ACPI_PM1_CNT_DWORD_TARGET)
		x58_acpi_mode_fail("SCI_EN_EXACT_READBACK");
	if (!x58_acpi_enables_are_quiescent() ||
	    !x58_acpi_irq9_is_masked())
		x58_acpi_mode_fail("FINAL_QUIESCENT_STATE_RECHECK");
	if (x58_acpi_read_eflags() & X86_EFLAGS_IF)
		x58_acpi_mode_fail("FINAL_CPU_INTERRUPTS_ENABLED");
	if (pci_io_read_config32(X58_IOAPIC_LPC_DEV, PCI_VENDOR_ID) !=
		X58_IOAPIC_LPC_ID ||
	    pci_io_read_config32(X58_IOAPIC_LPC_DEV, RCBA) !=
		X58_IOAPIC_RCBA_ENABLED ||
	    pci_io_read_config32(X58_IOAPIC_LPC_DEV, D31F0_PMBASE) !=
		X58_TCO_PMBASE_ENABLED ||
	    pci_io_read_config8(X58_IOAPIC_LPC_DEV, D31F0_ACPI_CNTL) !=
		X58_TCO_ACPI_DECODE_ENABLED)
		x58_acpi_mode_fail("FINAL_DECODE_IDENTITY_RECHECK");

	x58_acpi_mode_is_ready = true;
	post_code(POST_X58_ACPI_READY);
	printk(BIOS_NOTICE,
	       "[ACPI-MODE] X58_ACPI READY PM1_STS=%04x PM1_CNT=00000001 "
	       "PRBTNOR_W1C=%u SCI_EN_WRITE=%u EVENT_ENABLE_WRITE=%u "
	       "SMI_WRITE=0 IRQ_ROUTE_WRITE=0 SCI_DELIVERY_PROVEN=0\n",
	       inw(DEFAULT_PMBASE + PM1_STS), (unsigned int)status_write,
	       (unsigned int)control_write, (unsigned int)event_enable_write);
}

bool x58_acpi_mode_ready(void)
{
	return x58_acpi_mode_is_ready;
}

struct x58_hpet_counter_snapshot {
	uint32_t low;
	uint32_t high;
};

static struct x58_hpet_counter_snapshot x58_hpet_read_hpet_counter(void)
{
	return (struct x58_hpet_counter_snapshot) {
		.low = read32p(X58_HPET_BASE + X58_HPET_MAIN_COUNTER_LOW),
		.high = read32p(X58_HPET_BASE + X58_HPET_MAIN_COUNTER_HIGH),
	};
}

static void x58_hpet_fail(const char *reason)
{
	die_with_post_code(POST_X58_HPET_FAIL,
		"[HPET] X58_HPET fail-closed: %s\n", reason);
}

/*
 * The sole X58_HPET hardware mutation is HPTC 00000000 -> 00000080.  The
 * allowlist, LPC identity, fixed RCBA and inherited IOAPIC-decode state are
 * all checked before that first HPTC write.  HPET registers remain read-only.
 */
static void x58_hpet_decode_and_gate_hpet_once(void)
{
	const uint32_t lpc_id = pci_io_read_config32(X58_IOAPIC_LPC_DEV,
		PCI_VENDOR_ID);
	const uint32_t rcba = pci_io_read_config32(X58_IOAPIC_LPC_DEV, RCBA);
	uint32_t hptc;
	uint32_t cap_low;
	uint32_t cap_high;
	uint32_t config_low;
	uint32_t config_high;
	uint32_t interrupt_status_low;
	uint32_t interrupt_status_high;
	uint32_t timer0_config_low;
	uint32_t timer0_config_high;
	struct x58_hpet_counter_snapshot counter_first;
	struct x58_hpet_counter_snapshot counter_second;
	bool decode_write = false;

	if (x58_hpet_attempted)
		x58_hpet_fail("SECOND_ATTEMPT");
	x58_hpet_attempted = true;
	if (!x58_pci_standard_lpc_attempted)
		x58_hpet_fail("X58_IOAPIC_STAGE_NOT_RUN");
	if (lpc_id != X58_IOAPIC_LPC_ID || rcba != X58_IOAPIC_RCBA_ENABLED)
		x58_hpet_fail("LPC_RCBA_IDENTITY");
	if (read8p(CONFIG_FIXED_RCBA_MMIO_BASE + OIC) != X58_IOAPIC_OIC_ENABLED)
		x58_hpet_fail("INHERITED_OIC_STATE");

	hptc = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_HPTC);
	/* Verification only in this profile: never repair a failed standard owner. */
	if (hptc != X58_HPET_HPTC_FED00000)
		x58_hpet_fail("STANDARD_LPC_HPET_NOT_ALREADY_DECODED");
	if (hptc != X58_HPET_HPTC_DISABLED && hptc != X58_HPET_HPTC_FED00000)
		x58_hpet_fail("HPTC_OUTSIDE_EXACT_ALLOWLIST");
	if (hptc == X58_HPET_HPTC_DISABLED &&
	    (read32p(X58_HPET_BASE + X58_HPET_CAP_ID_LOW) != UINT32_MAX ||
	     read32p(X58_HPET_BASE + X58_HPET_CAP_ID_HIGH) != UINT32_MAX))
		x58_hpet_fail("DISABLED_APERTURE_NOT_OPEN_BUS");

	printk(BIOS_NOTICE,
	       "[HPET] X58_HPET PRE LPC_ID=%08x RCBA=%08x OIC=03 HPTC=%08x "
	       "ALLOWLIST=00000000,00000080\n",
	       lpc_id, rcba, hptc);
	post_code(POST_X58_HPET_BEGIN);
	if (hptc == X58_HPET_HPTC_DISABLED) {
		const uint32_t target =
			(hptc & ~X58_HPET_HPTC_CONTROL_MASK) | X58_HPET_HPTC_FED00000;

		if (target != X58_HPET_HPTC_FED00000)
			x58_hpet_fail("HPTC_TARGET_CONSTRUCTION");
		write32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_HPTC, target);
		decode_write = true;
	}
	hptc = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_HPTC);
	if (hptc != X58_HPET_HPTC_FED00000)
		x58_hpet_fail("HPTC_EXACT_READBACK");
	post_code(POST_X58_HPET_DECODED);

	cap_low = read32p(X58_HPET_BASE + X58_HPET_CAP_ID_LOW);
	cap_high = read32p(X58_HPET_BASE + X58_HPET_CAP_ID_HIGH);
	config_low = read32p(X58_HPET_BASE + X58_HPET_GENERAL_CONFIG_LOW);
	config_high = read32p(X58_HPET_BASE + X58_HPET_GENERAL_CONFIG_HIGH);
	interrupt_status_low = read32p(X58_HPET_BASE +
		X58_HPET_GENERAL_INTERRUPT_STATUS_LOW);
	interrupt_status_high = read32p(X58_HPET_BASE +
		X58_HPET_GENERAL_INTERRUPT_STATUS_HIGH);
	timer0_config_low = read32p(X58_HPET_BASE +
		X58_HPET_TIMER0_CONFIG_LOW);
	timer0_config_high = read32p(X58_HPET_BASE +
		X58_HPET_TIMER0_CONFIG_HIGH);
	counter_first = x58_hpet_read_hpet_counter();
	udelay(X58_HPET_COUNTER_STABILITY_DELAY_US);
	counter_second = x58_hpet_read_hpet_counter();

	printk(BIOS_NOTICE,
	       "[HPET] X58_HPET POST HPTC=%08x WRITE=%u GCAP=%08x:%08x "
	       "GENCFG=%08x:%08x ISR=%08x:%08x T0CFG=%08x:%08x "
	       "COUNTER_A=%08x:%08x COUNTER_B=%08x:%08x\n",
	       hptc, (unsigned int)decode_write, cap_high, cap_low,
	       config_high, config_low, interrupt_status_high,
	       interrupt_status_low, timer0_config_high, timer0_config_low,
	       counter_first.high, counter_first.low,
	       counter_second.high, counter_second.low);
	if (cap_low != X58_HPET_CAP_ID_LOW_TARGET ||
	    cap_high != X58_HPET_CAP_ID_HIGH_TARGET)
		x58_hpet_fail("GCAP_ID_TUPLE");
	if (config_low != 0 || config_high != 0)
		x58_hpet_fail("GENERAL_CONFIG_NOT_ZERO");
	if (interrupt_status_low != 0 || interrupt_status_high != 0)
		x58_hpet_fail("GENERAL_INTERRUPT_STATUS_NOT_ZERO");
	if (timer0_config_low & X58_HPET_TIMER_INTERRUPT_ENABLE)
		x58_hpet_fail("TIMER0_INTERRUPT_ENABLED");
	if (counter_first.low != counter_second.low ||
	    counter_first.high != counter_second.high)
		x58_hpet_fail("MAIN_COUNTER_NOT_STOPPED");
	if (read32p(X58_HPET_BASE + X58_HPET_GENERAL_CONFIG_LOW) != 0 ||
	    read32p(X58_HPET_BASE + X58_HPET_GENERAL_CONFIG_HIGH) != 0 ||
	    read32p(X58_HPET_BASE +
		X58_HPET_GENERAL_INTERRUPT_STATUS_LOW) != 0 ||
	    read32p(X58_HPET_BASE +
		X58_HPET_GENERAL_INTERRUPT_STATUS_HIGH) != 0 ||
	    (read32p(X58_HPET_BASE + X58_HPET_TIMER0_CONFIG_LOW) &
		X58_HPET_TIMER_INTERRUPT_ENABLE) != 0 ||
	    read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_HPTC) !=
		X58_HPET_HPTC_FED00000 ||
	    pci_io_read_config32(X58_IOAPIC_LPC_DEV, RCBA) != X58_IOAPIC_RCBA_ENABLED ||
	    pci_io_read_config32(X58_IOAPIC_LPC_DEV, PCI_VENDOR_ID) != X58_IOAPIC_LPC_ID)
		x58_hpet_fail("FINAL_STATE_RECHECK");

	x58_hpet_ready = true;
	post_code(POST_X58_HPET_READY);
	printk(BIOS_NOTICE,
	       "[HPET] X58_HPET READY BASE=fed00000 SIZE=00000400 STOPPED=1 "
	       "TIMER0_IRQ=0 ROUTE_WRITE=0 COUNTER_WRITE=0 ACPI_TABLE=0\n");
}

/* Exact results of lpc.c:enable_clock_gating() from the admitted zero CG,
 * and ioapic.c:route_i8259_irq0(). These are readback contracts, not tables
 * copied into a second initialization implementation. */
#define X58_PCI_STANDARD_LPC_CG 0xbfcf001fu
#define X58_PCI_STANDARD_SPI_CG_REG 0x38c0u
#define X58_PCI_STANDARD_IOAPIC_EXTINT 0x700u

/* Fixed LPC decodes belong to the domain. Never BAR-size/disable COM1 here. */
static bool x58_pci_is_standard_lpc(const struct device *dev)
{
	return dev && dev->path.type == DEVICE_PATH_PCI &&
		dev->path.pci.devfn == PCI_DEVFN(0x1f, 0) &&
		dev->upstream && dev->upstream->dev == x58_pci_domain;
}

static void x58_pci_standard_lpc_probe(struct device *dev)
{
	if (!x58_pci_is_standard_lpc(dev) || !dev->enabled || !dev->mandatory ||
	    dev->ops != &x58_pci_standard_lpc_ops || dev->downstream ||
	    dev->resource_list || pci_read_config32(dev, PCI_VENDOR_ID) != X58_IOAPIC_LPC_ID ||
	    (pci_read_config32(dev, PCI_CLASS_REVISION) >> 16) != PCI_CLASS_BRIDGE_ISA ||
	    (pci_read_config8(dev, PCI_HEADER_TYPE) & 0x7f) != PCI_HEADER_TYPE_NORMAL)
		die("[ICH10_LPC] LPC identity/ownership gate failed; no LPC writes\n");
}

struct device_operations x58_pci_standard_lpc_ops = {
	.read_resources = noop_read_resources,
	.set_resources = noop_set_resources,
	.enable_resources = x58_pci_standard_lpc_probe,
	.enable = x58_pci_standard_lpc_probe,
};

static uint32_t x58_pci_standard_pmir_before;
static uint16_t x58_pci_standard_pmcon1_before;
static bool x58_pci_standard_cf9_global_reset;

static void x58_pci_standard_lpc_acpi_mode(struct device *lpc)
{
	uint32_t value;
	const uint8_t pirq[] = { PIRQA_ROUT, PIRQB_ROUT, PIRQC_ROUT, PIRQD_ROUT,
		PIRQE_ROUT, PIRQF_ROUT, PIRQG_ROUT, PIRQH_ROUT };
	/* ExtINT0 is intentional; SCI9 and every other input remain masked. */
	if (!x58_pci_standard_lpc_attempted || x58_pci_standard_lpc_ready ||
	    !x58_acpi_pic_tuple_is_target(inb(MASTER_PIC_OCW1), inb(SLAVE_PIC_OCW1),
		inb(ELCR1), inb(ELCR2)) ||
	    pci_read_config8(lpc, D31F0_SERIRQ_CNTL) != 0xd0 ||
	    read8p(CONFIG_FIXED_RCBA_MMIO_BASE + OIC) != X58_IOAPIC_OIC_ENABLED ||
	    !x58_ioapic_read(X58_IOAPIC_ID_REG, &value) || value != 0 ||
	    !x58_ioapic_read(X58_IOAPIC_VERSION_REG, &value) ||
	    value != X58_IOAPIC_VERSION_TARGET)
		die("[ICH10_LPC] standard PIC/SERIRQ/APIC readback failed\n");
	for (size_t i = 0; i < ARRAY_SIZE(pirq); i++)
		if (pci_read_config8(lpc, pirq[i]) != 11)
			die("[ICH10_LPC] standard PIRQ11 readback failed\n");
	for (unsigned int i = 0; i < X58_IOAPIC_REDIR_COUNT; i++) {
		if (!x58_ioapic_read(X58_IOAPIC_REDIR_BASE + 2 * i, &value) ||
		    value != (i ? X58_IOAPIC_LOW_TARGET : X58_PCI_STANDARD_IOAPIC_EXTINT) ||
		    !x58_ioapic_read(X58_IOAPIC_REDIR_BASE + 2 * i + 1, &value) ||
		    value != (i ? 0 : lapicid() << 24))
			die("[ICH10_LPC] standard IOAPIC vector readback failed\n");
	}
	if (!x58_ioapic_select(X58_IOAPIC_ID_REG) ||
	    read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_CG) != X58_PCI_STANDARD_LPC_CG ||
	    read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_HPTC) != X58_HPET_HPTC_FED00000 ||
	    (read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_DMIC) & 3) != 3 ||
	    (read32p(CONFIG_FIXED_RCBA_MMIO_BASE + X58_PCI_STANDARD_SPI_CG_REG) & 7) != 7 ||
	    pci_read_config32(lpc, D31F0_PMIR) !=
		(x58_pci_standard_pmir_before | PMIR_FIELD_2 | PMIR_FIELD_0 |
		 (x58_pci_standard_cf9_global_reset ?
		  (PMIR_CF9LOCK | PMIR_CF9GR) : 0)) ||
	    pci_read_config16(lpc, D31F0_GEN_PMCON_1) !=
		((x58_pci_standard_pmcon1_before & ~3u) | BIT(2) | BIT(3) | BIT(5) | BIT(10)))
		die("[ICH10_LPC] standard clock/power policy readback failed\n");
	/* Already decoded by the standard owner: the full HPET gate now observes
	 * capabilities/counter state without another decode write. */
	x58_hpet_decode_and_gate_hpet_once();
	x58_acpi_enter_quiet_acpi_mode_once();
}

static void x58_pci_standard_lpc_init_once(void)
{
	struct device *lpc = pcidev_on_root(0x1f, 0);
	/* Explicit board policy: no GPI event routes/enables or CPU sleep extensions. */
	static struct southbridge_intel_i82801jx_config config;
	_Static_assert(CONFIG(NO_SMM) && !CONFIG(HAVE_ACPI_RESUME) &&
		!CONFIG(DEBUG_PERIODIC_SMI) &&
		!CONFIG(IOAPIC_USE_PRESET_ID), "ICH10_LPC requires isolated cold-boot LPC ownership");
	if (x58_pci_standard_lpc_attempted)
		die("[ICH10_LPC] second standard LPC attempt; cold recovery required\n");
	x58_pci_standard_lpc_attempted = true;
	x58_pci_standard_lpc_probe(lpc);
	if (!x58_tco_ready || !x58_usb_ich10_baseline_attempted || !x58_sata_config_ahci_route_attempted ||
	    x58_acpi_pic_attempted || x58_ioapic_attempted || x58_hpet_attempted ||
	    (x58_acpi_read_eflags() & X86_EFLAGS_IF) ||
	    pci_read_config32(lpc, RCBA) != X58_IOAPIC_RCBA_ENABLED ||
	    pci_read_config32(lpc, D31F0_PMBASE) != X58_TCO_PMBASE_ENABLED ||
	    pci_read_config8(lpc, D31F0_ACPI_CNTL) != X58_TCO_ACPI_DECODE_ENABLED ||
	    read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_CG) != 0 ||
	    inw(DEFAULT_PMBASE + PM1_EN) || inl(DEFAULT_PMBASE + GPE0_EN + 4) ||
	    !x58_ich10_gpe0_pme_prestate(inl(DEFAULT_PMBASE + GPE0_EN)) ||
	    inl(DEFAULT_PMBASE + SMI_EN) || inw(DEFAULT_PMBASE + ALT_GP_SMI_EN) ||
	    inw(DEFAULT_PMBASE + X58_ACPI_UPRWC) ||
	    pci_read_config32(lpc, D31F0_GPIO_ROUT) ||
	    (inl(DEFAULT_PMBASE + PM1_CNT) != 0 && inl(DEFAULT_PMBASE + PM1_CNT) != 1))
		die("[ICH10_LPC] standard LPC preconditions failed; no sequence writes\n");
	x58_pci_standard_pmir_before = pci_read_config32(lpc, D31F0_PMIR);
	x58_pci_standard_pmcon1_before = pci_read_config16(lpc, D31F0_GEN_PMCON_1);
	x58_pci_standard_cf9_global_reset = false;
	/* Standard power_options writes the caller's GPE configuration. Keep
	 * ICH10's observed reserved bit10, disabling only admitted PME sources. */
	config.gpe0_en = inl(DEFAULT_PMBASE + GPE0_EN) & X58_ICH10_GPE0_RESERVED_BIT10;
	printk(BIOS_NOTICE, "[ICH10_LPC] STANDARD_LPC_BEGIN GEN_PMCON3=%02x "
		"PM1_STS=%04x RTC_STATUS_CAPTURED_BEFORE_CLEAR=1 CMOS_PRESERVE=1\n",
		pci_read_config8(lpc, D31F0_GEN_PMCON_3), inw(DEFAULT_PMBASE + PM1_STS));
	lpc->chip_info = &config;
	i82801jx_lpc_init_sequence(lpc, true, x58_pci_standard_lpc_acpi_mode);
	if (!x58_acpi_mode_ready() || (x58_acpi_read_eflags() & X86_EFLAGS_IF))
		die("[ICH10_LPC] standard LPC final ACPI/IF gate failed\n");
	x58_pci_standard_lpc_ready = true;
	printk(BIOS_NOTICE, "[ICH10_LPC] STANDARD_LPC_READY PIRQ=11 IOAPIC_EXTINT0=1 "
		"PIC=fb/ff ELCR=00/02 SERIRQ=d0 CG=bfcf001f NO_SMM=1 "
		"FULL_ICH10_DEVICE_MODEL=0\n");
}



struct x58_sata_config_sata_snapshot {
	uint32_t lpc_id;
	uint32_t rcba;
	uint8_t fdsw;
	uint32_t fd;
	uint32_t f2_id;
	uint32_t f2_classrev;
	uint16_t f2_command;
	uint8_t f2_header;
	uint32_t f2_bar5;
	uint16_t f2_map;
	uint16_t f2_pcs;
	uint32_t f2_sclkcg;
	uint32_t f5_id;
	uint32_t f5_classrev;
	uint16_t f5_command;
	uint8_t f5_header;
	uint32_t f5_bar5;
	uint16_t f5_map;
};

static struct x58_sata_config_sata_snapshot x58_sata_config_read_sata_snapshot(void)
{
	struct x58_sata_config_sata_snapshot snapshot = {
		.lpc_id = pci_io_read_config32(X58_USB_ICH10_LPC_DEV, PCI_VENDOR_ID),
		.rcba = pci_io_read_config32(X58_USB_ICH10_LPC_DEV, RCBA),
	};

	/* Never touch RCBA MMIO until its PCI identity and decode are exact. */
	if (snapshot.lpc_id != X58_USB_ICH10_LPC_ID ||
	    snapshot.rcba != X58_USB_ICH10_RCBA_ENABLED)
		return snapshot;

	snapshot.fdsw = read8p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_FDSW);
	snapshot.fd = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_FD);
	snapshot.f2_id = pci_io_read_config32(X58_SATA_MAP_SATA1_DEV, PCI_VENDOR_ID);
	snapshot.f2_classrev = pci_io_read_config32(X58_SATA_MAP_SATA1_DEV,
		PCI_CLASS_REVISION);
	snapshot.f2_command = pci_io_read_config16(X58_SATA_MAP_SATA1_DEV,
		PCI_COMMAND);
	snapshot.f2_header = pci_io_read_config8(X58_SATA_MAP_SATA1_DEV,
		PCI_HEADER_TYPE);
	snapshot.f2_bar5 = pci_io_read_config32(X58_SATA_MAP_SATA1_DEV,
		I82801JX_SATA_ABAR);
	snapshot.f2_map = pci_io_read_config16(X58_SATA_MAP_SATA1_DEV,
		I82801JX_SATA_MAP);
	snapshot.f2_pcs = pci_io_read_config16(X58_SATA_MAP_SATA1_DEV,
		I82801JX_SATA_PCS);
	snapshot.f2_sclkcg = pci_io_read_config32(X58_SATA_MAP_SATA1_DEV,
		I82801JX_SATA_SCLKCG);
	snapshot.f5_id = pci_io_read_config32(X58_SATA_MAP_SATA2_DEV, PCI_VENDOR_ID);
	snapshot.f5_classrev = pci_io_read_config32(X58_SATA_MAP_SATA2_DEV,
		PCI_CLASS_REVISION);
	snapshot.f5_command = pci_io_read_config16(X58_SATA_MAP_SATA2_DEV,
		PCI_COMMAND);
	snapshot.f5_header = pci_io_read_config8(X58_SATA_MAP_SATA2_DEV,
		PCI_HEADER_TYPE);
	snapshot.f5_bar5 = pci_io_read_config32(X58_SATA_MAP_SATA2_DEV,
		I82801JX_SATA_ABAR);
	snapshot.f5_map = pci_io_read_config16(X58_SATA_MAP_SATA2_DEV,
		I82801JX_SATA_MAP);

	return snapshot;
}

static void x58_sata_config_log_sata_snapshot(const char *phase,
	const struct x58_sata_config_sata_snapshot *snapshot)
{
	printk(BIOS_NOTICE,
	       "[SATA] X58_SATA_CONFIG %s LPC_ID=%08x RCBA=%08x FDSW=%02x FD=%08x "
	       "F2_ID=%08x F2_CLASSREV=%08x F2_CMD=%04x F2_HDR=%02x "
	       "F2_BAR5=%08x F2_MAP=%04x F2_PCS=%04x F2_SCLKCG=%08x "
	       "F5_ID=%08x F5_CLASSREV=%08x F5_CMD=%04x F5_HDR=%02x "
	       "F5_BAR5=%08x F5_MAP=%04x\n",
	       phase, snapshot->lpc_id, snapshot->rcba, snapshot->fdsw,
	       snapshot->fd, snapshot->f2_id, snapshot->f2_classrev,
	       snapshot->f2_command, snapshot->f2_header, snapshot->f2_bar5,
	       snapshot->f2_map, snapshot->f2_pcs, snapshot->f2_sclkcg,
	       snapshot->f5_id, snapshot->f5_classrev, snapshot->f5_command,
	       snapshot->f5_header, snapshot->f5_bar5, snapshot->f5_map);
}

static bool x58_sata_config_fixed_gates_are_exact(
	const struct x58_sata_config_sata_snapshot *snapshot)
{
	return snapshot->lpc_id == X58_USB_ICH10_LPC_ID &&
		snapshot->rcba == X58_USB_ICH10_RCBA_ENABLED &&
		snapshot->fdsw == X58_USB_ICH10_FDSW_UNLOCKED;
}

static bool x58_sata_config_f2_reset_state_is_exact(
	const struct x58_sata_config_sata_snapshot *snapshot)
{
	return snapshot->f2_id == X58_SATA_MAP_SATA1_IDE_ID &&
		snapshot->f2_classrev == X58_SATA_CONFIG_SATA1_IDE_CLASSREV &&
		snapshot->f2_command == X58_SATA_CONFIG_RESET_COMMAND &&
		snapshot->f2_header == X58_SATA_CONFIG_RESET_HEADER &&
		snapshot->f2_bar5 == X58_SATA_CONFIG_RESET_BAR5 &&
		snapshot->f2_map == X58_SATA_CONFIG_RESET_MAP &&
		snapshot->f2_pcs == X58_SATA_CONFIG_RESET_PCS &&
		snapshot->f2_sclkcg == X58_SATA_CONFIG_RESET_SCLKCG;
}

static bool x58_sata_config_f5_reset_state_is_exact(
	const struct x58_sata_config_sata_snapshot *snapshot)
{
	return snapshot->f5_id == X58_SATA_MAP_SATA2_IDE_ID &&
		snapshot->f5_classrev == X58_SATA_CONFIG_SATA2_IDE_CLASSREV &&
		snapshot->f5_command == X58_SATA_CONFIG_RESET_COMMAND &&
		snapshot->f5_header == X58_SATA_CONFIG_RESET_HEADER &&
		snapshot->f5_bar5 == X58_SATA_CONFIG_RESET_BAR5 &&
		snapshot->f5_map == X58_SATA_CONFIG_RESET_MAP;
}

static bool x58_sata_config_f2_ahci_state_is_exact(
	const struct x58_sata_config_sata_snapshot *snapshot)
{
	return snapshot->f2_id == X58_SATA_MAP_SATA1_AHCI_ID &&
		snapshot->f2_classrev == X58_SATA_CONFIG_SATA1_AHCI_CLASSREV &&
		snapshot->f2_command == X58_SATA_CONFIG_RESET_COMMAND &&
		snapshot->f2_header == X58_SATA_CONFIG_RESET_HEADER &&
		snapshot->f2_bar5 == 0 &&
		snapshot->f2_map == I82801JX_SATA_MAP_AHCI_D31F2_VALUE &&
		snapshot->f2_pcs == X58_SATA_CONFIG_RESET_PCS &&
		snapshot->f2_sclkcg == X58_SATA_CONFIG_RESET_SCLKCG;
}

static bool x58_sata_config_raw_reset_topology_is_exact(
	const struct x58_sata_config_sata_snapshot *snapshot)
{
	return x58_sata_config_fixed_gates_are_exact(snapshot) &&
		snapshot->fd == X58_SATA_CONFIG_FD_BASELINE &&
		x58_sata_config_f2_reset_state_is_exact(snapshot) &&
		x58_sata_config_f5_reset_state_is_exact(snapshot);
}

static bool x58_sata_config_mapped_before_hide_is_exact(
	const struct x58_sata_config_sata_snapshot *snapshot)
{
	return x58_sata_config_fixed_gates_are_exact(snapshot) &&
		snapshot->fd == X58_SATA_CONFIG_FD_BASELINE &&
		x58_sata_config_f2_ahci_state_is_exact(snapshot) &&
		x58_sata_config_f5_reset_state_is_exact(snapshot);
}

static bool x58_sata_config_final_route_is_exact(
	const struct x58_sata_config_sata_snapshot *snapshot)
{
	return x58_sata_config_fixed_gates_are_exact(snapshot) &&
		snapshot->fd == X58_SATA_CONFIG_FD_SATA2_DISABLED &&
		x58_sata_config_f2_ahci_state_is_exact(snapshot) &&
		snapshot->f5_id == 0xffffffffu &&
		snapshot->f5_classrev == 0xffffffffu &&
		snapshot->f5_command == 0xffffu &&
		snapshot->f5_header == 0xffu &&
		snapshot->f5_bar5 == 0xffffffffu &&
		snapshot->f5_map == 0xffffu;
}

/*
 * A live X58_INTERRUPT_MODE CAR transaction proved that MAP=0060 changes F2 to AHCI but
 * leaves F5 visible. A separate FD_SAD2 write then hid F5 without changing
 * F2. Preserve that causal ordering and reject every non-reset input tuple.
 */
static void x58_sata_config_program_ahci_route_once(void)
{
	struct x58_sata_config_sata_snapshot snapshot;

	if (x58_sata_config_ahci_route_attempted)
		die_with_post_code(POST_X58_SATA_CONFIG_ROUTE_FAIL,
			"[SATA] X58_SATA_CONFIG refused a second AHCI-route attempt\n");
	x58_sata_config_ahci_route_attempted = true;

	snapshot = x58_sata_config_read_sata_snapshot();
	x58_sata_config_log_sata_snapshot("RESET_PRE", &snapshot);
	if (!x58_sata_config_raw_reset_topology_is_exact(&snapshot))
		die_with_post_code(POST_X58_SATA_CONFIG_ROUTE_FAIL,
			"[SATA] X58_SATA_CONFIG rejected non-reset F2/F5 or non-baseline FD tuple\n");
	post_code(POST_X58_SATA_CONFIG_RESET_TOPOLOGY_OK);
	printk(BIOS_NOTICE,
	       "[SATA] X58_SATA_CONFIG exact dual-IDE reset topology PASS after ICH10_REQUIRED_FIELDS\n");

	post_code(POST_X58_SATA_CONFIG_MAP_BEGIN);
	printk(BIOS_NOTICE,
	       "[SATA] X58_SATA_CONFIG MAP_STAGE begin: MAP[7:5]=011b, then mandatory BAR5 clear\n");
	i82801jx_sata_select_ahci(X58_SATA_MAP_SATA1_DEV);
	snapshot = x58_sata_config_read_sata_snapshot();
	x58_sata_config_log_sata_snapshot("MAP_POST", &snapshot);
	if (!x58_sata_config_mapped_before_hide_is_exact(&snapshot))
		die_with_post_code(POST_X58_SATA_CONFIG_ROUTE_FAIL,
			"[SATA] X58_SATA_CONFIG MAP/BAR5 intermediate readback failed\n");
	post_code(POST_X58_SATA_CONFIG_MAP_OK);
	printk(BIOS_NOTICE,
	       "[SATA] X58_SATA_CONFIG MAP/BAR5 gate PASS; F5 remains visible as measured\n");

	post_code(POST_X58_SATA_CONFIG_FD_SAD2_BEGIN);
	printk(BIOS_NOTICE,
	       "[SATA] X58_SATA_CONFIG FD_SAD2_STAGE begin: set only RCBA FD bit25\n");
	i82801jx_disable_sata2();
	snapshot = x58_sata_config_read_sata_snapshot();
	x58_sata_config_log_sata_snapshot("FD_SAD2_POST", &snapshot);
	if (!x58_sata_config_final_route_is_exact(&snapshot))
		die_with_post_code(POST_X58_SATA_CONFIG_ROUTE_FAIL,
			"[SATA] X58_SATA_CONFIG FD_SAD2/F5-absence complete readback failed\n");
	post_code(POST_X58_SATA_CONFIG_FD_SAD2_OK);
	printk(BIOS_NOTICE,
	       "[SATA] X58_SATA_CONFIG corrected AHCI route PASS; FD=02000001 FDSW=00 F5=absent; "
	       "no FDSW lock, PCS/SCLKCG/AHCI MMIO write\n");
}

static void x58_pci_log_iou(const char *phase, unsigned int dev)
{
	const uint16_t link_status = x58_pci_ecam_read16(dev,
		X58_PCIE_LNKSTA);
	uint32_t expected_id;

	switch (dev) {
	case X58_PCI_IOU2_X4_DEV:
		expected_id = X58_PCI_IOU2_X4_ID;
		break;
	case X58_PCI_IOU0_X16_DEV:
		expected_id = X58_PCI_IOU0_X16_ID;
		break;
	case X58_PCI_AUX_X16_DEV:
		expected_id = X58_PCI_AUX_X16_ID;
		break;
	default:
		expected_id = 0xffffffffu;
		break;
	}

	printk(BIOS_NOTICE,
	       "[PCIE] %s 00:%02x.0 ID=%08x EXPECT=%08x 09c=%08x 0a0=%08x "
	       "0a2=%04x 0aa=%04x 0c0=%08x 190=%08x "
	       "SPEED=%u WIDTH=%u LT=%u DLLA=%u\n",
	       phase, dev, x58_pci_ecam_read32(dev, PCI_VENDOR_ID), expected_id,
	       x58_pci_ecam_read32(dev, X58_PCIE_LNKCAP),
	       x58_pci_ecam_read32(dev, X58_PCIE_LNKCTLSTA), link_status,
	       x58_pci_ecam_read16(dev, X58_PCIE_AA),
	       x58_pci_ecam_read32(dev, X58_PCIE_C0),
	       x58_pci_ecam_read32(dev, X58_PCIE_PRTX_BIF_CTRL),
	       (unsigned int)(link_status & X58_PCI_LNKSTA_SPEED_MASK),
	       (unsigned int)(link_status & X58_PCI_LNKSTA_WIDTH_MASK) >>
		X58_PCI_LNKSTA_WIDTH_SHIFT,
	       (unsigned int)!!(link_status & PCI_EXP_LNKSTA_LT),
	       (unsigned int)!!(link_status & X58_PCI_LNKSTA_DLL_ACTIVE));
}

/*
 * Intel X58 datasheet section 5.1.2 requires BIOS to release an IOU from
 * Wait_on_BIOS through PCIE_PRTx_BIF_CTRL.  X58_PCI changes one hypothesis at
 * a time: only IOU0 / 00:03.0 is started.  Ports 00:01.0 and 00:07.0 are
 * deliberately read-only telemetry.  The action field is observed as zero
 * after a vendor boot, so its readback is diagnostic rather than an equality
 * gate.  No standard Link Control retrain is issued here.
 */
static void x58_pci_start_iou0_once(void)
{
	uint16_t link_status;
	unsigned int polls;
	bool wrote_start = false;

	if (x58_pci_iou0_start_attempted)
		die_with_post_code(POST_X58_PCI_PLATFORM_FAIL,
			"[PCIE] X58_PCI refused a second IOU0 start attempt\n");
	if (x58_pci_ecam_read32(X58_PCI_IOU0_X16_DEV, PCI_VENDOR_ID) !=
	    X58_PCI_IOU0_X16_ID)
		die_with_post_code(POST_X58_PCI_PLATFORM_FAIL,
			"[PCIE] X58_PCI IOU0 identity gate failed\n");
	x58_pci_iou0_start_attempted = true;

	x58_pci_log_iou("PRE", X58_PCI_IOU2_X4_DEV);
	x58_pci_log_iou("PRE", X58_PCI_IOU0_X16_DEV);
	x58_pci_log_iou("PRE", X58_PCI_AUX_X16_DEV);

	link_status = x58_pci_ecam_read16(X58_PCI_IOU0_X16_DEV,
		X58_PCIE_LNKSTA);
	post_code(POST_X58_PCI_IOU0_START);
	if (!(link_status & X58_PCI_LNKSTA_DLL_ACTIVE)) {
		write16p(X58_PCI_ECAM_DEV0(X58_PCI_IOU0_X16_DEV,
			X58_PCIE_PRTX_BIF_CTRL), X58_PCI_IOU0_X16_START);
		wrote_start = true;
		printk(BIOS_NOTICE,
		       "[PCIE] BIF_START 00:03.0 ECAM=e0018190 VALUE=000c\n");
	} else {
		printk(BIOS_NOTICE,
		       "[PCIE] BIF_START 00:03.0 skipped: DLL already active\n");
	}

	for (polls = 0; polls < X58_PCI_LINK_POLL_COUNT; polls++) {
		link_status = x58_pci_ecam_read16(X58_PCI_IOU0_X16_DEV,
			X58_PCIE_LNKSTA);
		if ((link_status & X58_PCI_LNKSTA_DLL_ACTIVE) &&
		    !(link_status & PCI_EXP_LNKSTA_LT))
			break;
		udelay(X58_PCI_LINK_POLL_US);
	}

	post_code(POST_X58_PCI_IOU0_POLLED);
	x58_pci_log_iou("POST", X58_PCI_IOU2_X4_DEV);
	x58_pci_log_iou("POST", X58_PCI_IOU0_X16_DEV);
	x58_pci_log_iou("POST", X58_PCI_AUX_X16_DEV);
	if (polls == X58_PCI_LINK_POLL_COUNT)
		printk(BIOS_WARNING,
		       "[PCIE] IOU0 link timeout after 1000000 us; continuing to serial payload fallback\n");
	else
		printk(BIOS_NOTICE,
		       "[PCIE] IOU0 link ready after %u polls (start-write=%u)\n",
		       polls, (unsigned int)wrote_start);
}

static const struct x58_pci_expected_pci *x58_pci_expected_for_device(
	const struct device *dev)
{
	unsigned int root_devfn;
	bool downstream;

	if (x58_pci_domain == NULL || dev == NULL ||
	    dev->path.type != DEVICE_PATH_PCI || dev->upstream == NULL)
		return NULL;

	if (dev->upstream == x58_pci_domain->downstream) {
		root_devfn = dev->path.pci.devfn;
		downstream = false;
	} else if (dev->upstream->dev != NULL &&
		   dev->upstream->dev->path.type == DEVICE_PATH_PCI &&
		   dev->upstream->dev->upstream == x58_pci_domain->downstream) {
		root_devfn = dev->upstream->dev->path.pci.devfn;
		downstream = true;
	} else {
		return NULL;
	}

	for (size_t i = 0; i < ARRAY_SIZE(x58_pci_expected); i++) {
		const struct x58_pci_expected_pci *expected = &x58_pci_expected[i];

		if (expected->root_devfn == root_devfn &&
		    expected->devfn == dev->path.pci.devfn &&
		    expected->downstream == downstream)
			return expected;
	}

	return NULL;
}

static struct device *x58_pci_device_for_expected(
	const struct x58_pci_expected_pci *expected)
{
	struct device *root;

	if (x58_pci_domain == NULL || x58_pci_domain->downstream == NULL)
		return NULL;
	root = pcidev_path_behind(x58_pci_domain->downstream,
		expected->root_devfn);
	if (!expected->downstream)
		return root;
	if (root == NULL || root->downstream == NULL)
		return NULL;
	return pcidev_path_behind(root->downstream, expected->devfn);
}

static bool x58_pci_id_matches(const struct x58_pci_expected_pci *expected,
	uint32_t id)
{
	if (expected->runtime_optional)
		return (id & 0xffffu) == expected->id;
	return id == expected->id;
}

static bool x58_pci_optional_device_absent(
	const struct x58_pci_expected_pci *expected, const struct device *dev)
{
	return expected->runtime_optional && dev != NULL && !dev->enabled;
}

/* JMICRON preserves the inherited JMicron mode. Linux's early JMicron quirk
 * can expose function one later; a post-Linux inventory is not pre-OS state.
 * Only the generic scanner's legitimately unexposed fn1 may be omitted. */
static bool x58_pci_jmb_single;

static bool x58_pci_function_unexposed(const struct x58_pci_expected_pci *expected)
{
	return x58_pci_jmb_single &&
		expected->downstream && expected->root_devfn == PCI_DEVFN(0x1c, 1) &&
		expected->devfn == PCI_DEVFN(0, 1) && expected->id == 0x2363197bu;
}

static void x58_pci_require_identity(struct device *dev);

static bool x58_pci_jmb_single_valid(void *context)
{
	struct device *fn0 = context;

	return fn0 && fn0->enabled && fn0->mandatory && fn0->upstream &&
		fn0->upstream->dev &&
		fn0->upstream->dev->path.pci.devfn == PCI_DEVFN(0x1c, 1) &&
		fn0->path.pci.devfn == PCI_DEVFN(0, 0) && fn0->hdr_type == 0 &&
		pci_read_config32(fn0, PCI_VENDOR_ID) == 0x2363197bu &&
		(pci_read_config32(fn0, PCI_CLASS_REVISION) & 0xffff00ffu) == 0x01060003u &&
		pci_read_config8(fn0, PCI_HEADER_TYPE) == 0 &&
		pcidev_path_behind(fn0->upstream, PCI_DEVFN(0, 1)) == NULL;
}

static void x58_pci_jmb_admit_mode(struct device *root, struct device *static_fn1)
{
	struct device *fn0 = root->downstream ?
		pcidev_path_behind(root->downstream, PCI_DEVFN(0, 0)) : NULL;
	struct device *fn1 = root->downstream ?
		pcidev_path_behind(root->downstream, PCI_DEVFN(0, 1)) : NULL;
	if (!fn0 || x58_pci_jmb_single)
		die("[JMICRON] JMB363 missing function0 or repeated single-mode admission\n");
	x58_pci_require_identity(fn0);
	const uint8_t header = pci_read_config8(fn0, PCI_HEADER_TYPE);
	printk(BIOS_NOTICE, "[JMICRON] JMB363 MODE HDR=%02x CONF1=%08x CONF5=%08x FN1=%u WRITES=0\n",
		header, pci_read_config32(fn0, 0x40), pci_read_config32(fn0, 0x80), fn1 != NULL);
	if (header != fn0->hdr_type)
		die("[JMICRON] JMB363 header changed after scan\n");
	if (header & 0x80) {
		if (!fn1)
			die("[JMICRON] multifunction JMB363 is missing function1\n");
		x58_pci_require_identity(fn1);
	} else {
		if (!x58_pci_jmb_single_valid(fn0))
			die("[JMICRON] JMB363 single-function graph not admitted\n");
		if (!static_fn1 || static_fn1->upstream != root->downstream ||
		    static_fn1->path.pci.devfn != PCI_DEVFN(0, 1) ||
		    !static_fn1->mandatory || static_fn1->hidden ||
		    static_fn1->vendor != 0 || static_fn1->resource_list != NULL)
			die("[JMICRON] unexposed JMB363 static node is not pristine\n");
		/* pci_scan_bus unlinks the node only from bus->children. It remains
		 * in all_devices, which the standard LPC PIRQ setup walks. Disable
		 * this unprobed software node to prevent later phantom config writes.
		 * No hardware function-disable register is written. */
		static_fn1->enabled = false;
		x58_pci_jmb_single = true;
	}
	printk(BIOS_NOTICE, "[JMICRON] JMB363_MODE_READY FUNCTIONS=%u MODE_WRITES=0\n",
		x58_pci_jmb_single ? 1 : 2);
}

static void x58_pci_jmb_verify_mode(void)
{
	if (!x58_pci_jmb_single)
		return;
	struct device *root = pcidev_path_behind(x58_pci_domain->downstream, PCI_DEVFN(0x1c, 1));
	struct device *fn0 = root && root->downstream ?
		pcidev_path_behind(root->downstream, PCI_DEVFN(0, 0)) : NULL;
	if (!x58_pci_jmb_single_valid(fn0))
		die("[JMICRON] admitted JMB363 single-function mode changed\n");
}

static void x58_pci_report_optional_gpu_absent(void)
{
	if (x58_pci_gpu_absence_reported)
		return;
	x58_pci_gpu_absence_reported = true;
	post_code(POST_X58_PCI_GPU_FALLBACK);
	printk(BIOS_WARNING,
	       "[PCI] GPU_PROBE root=00:03.0 RESULT=ABSENT_SERIAL_SEABIOS_FALLBACK\n");
}

static void x58_pci_require_identity(struct device *dev)
{
	const struct x58_pci_expected_pci *expected =
		x58_pci_expected_for_device(dev);
	const uint32_t id = ((uint32_t)dev->device << 16) | dev->vendor;
	const uint32_t classrev = pci_read_config32(dev, PCI_CLASS_REVISION);

	if (expected == NULL || !dev->enabled || !dev->mandatory ||
	    !x58_pci_id_matches(expected, id) ||
	    (dev->class >> 8) != expected->class_code ||
	    (classrev >> 8) != dev->class ||
	    (dev->hdr_type & 0x7f) != expected->header_type ||
	    (expected->revision >= 0 &&
	     (classrev & 0xff) != (uint8_t)expected->revision))
		die_with_post_code(POST_X58_PCI_IDENTITY_FAIL,
			"[RAMSTAGE] X58_PCI rejected PCI function %s ID=%08x CLASSREV=%08x HDR=%02x\n",
			expected ? expected->name : "outside allowlist", id,
			classrev, dev->hdr_type);
}

static void x58_pci_clear_command(struct device *dev)
{
	const uint16_t decode = PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
		PCI_COMMAND_MASTER;
	const uint16_t command = pci_read_config16(dev, PCI_COMMAND);

	pci_write_config16(dev, PCI_COMMAND, command & ~decode);
	if (pci_read_config16(dev, PCI_COMMAND) & decode)
		die_with_post_code(POST_X58_PCI_COMMAND_FAIL,
			"[RAMSTAGE] X58_PCI could not clear decode/master on %s\n",
			dev_path(dev));
	dev->command &= ~PCI_COMMAND_MASTER;
}

static void x58_pci_probe_gate(struct device *dev)
{
	x58_pci_require_identity(dev);
	x58_pci_clear_command(dev);
}

#include "pcie_dynamic.h"

/*
 * do_pci_scan_bridge() has already assigned/programmed the bridge bus numbers
 * before invoking this callback.  Sample root 00:03.0's secondary 00.0 by
 * both legacy CF8 and direct PCIEXBAR at 0, 1, 10 and 100 ms, then invoke the
 * unchanged generic scanner.  This callback performs no configuration write.
 */
static void x58_pcie_topology_scan_bus_with_endpoint_probe(struct bus *bus,
	unsigned int min_devfn, unsigned int max_devfn)
{
	static const unsigned int sample_delta_us[X58_PCIE_TOPOLOGY_ENDPOINT_SAMPLE_COUNT] = {
		0, 1000, 9000, 90000,
	};
	unsigned int elapsed_us = 0;

	if (bus != NULL && bus->dev != NULL &&
	    bus->dev->path.type == DEVICE_PATH_PCI &&
	    bus->dev->path.pci.devfn == PCI_DEVFN(X58_PCI_IOU0_X16_DEV, 0)) {
		if (bus->secondary == 0 || bus->secondary == 0xff)
			die_with_post_code(POST_X58_PCI_BUS_FAIL,
				"[PCIE] X58_PCIE_TOPOLOGY root03 secondary bus is invalid\n");

		for (size_t i = 0; i < ARRAY_SIZE(sample_delta_us); i++) {
			uint32_t cf8_id;
			uint32_t ecam_id;

			if (sample_delta_us[i] != 0)
				udelay(sample_delta_us[i]);
			elapsed_us += sample_delta_us[i];
			cf8_id = pci_io_read_config32(
				PCI_DEV(bus->secondary, X58_PCIE_TOPOLOGY_PEG_ENDPOINT_DEV, 0),
				PCI_VENDOR_ID);
			ecam_id = x58_pcie_topology_ecam_read_id(bus->secondary,
				X58_PCIE_TOPOLOGY_PEG_ENDPOINT_DEV, 0);
			printk(BIOS_NOTICE,
			       "[PCIE] X58_PCIE_TOPOLOGY PEG_PROBE T_US=%u BUS=%02x "
			       "CF8=%08x ECAM=%08x MATCH=%u\n",
			       elapsed_us, bus->secondary, cf8_id, ecam_id,
			       (unsigned int)(cf8_id == ecam_id));
		}
	}

	x58_pci_dynamic_seed(bus, min_devfn, max_devfn);
	pci_scan_bus(bus, min_devfn, max_devfn);
}

static void x58_pci_scan_bridge(struct device *dev)
{
	const struct x58_pci_expected_pci *expected =
		x58_pci_expected_for_device(dev);
	struct device *static_fn1 = dev->downstream &&
		dev->path.pci.devfn == PCI_DEVFN(0x1c, 1) ?
		pcidev_path_behind(dev->downstream, PCI_DEVFN(0, 1)) : NULL;

	x58_pci_require_identity(dev);
	if (expected == NULL || !expected->bridge)
		die_with_post_code(POST_X58_PCI_TOPOLOGY_FAIL,
			"[RAMSTAGE] X58_PCI refused non-allowlisted bridge scan\n");
	do_pci_scan_bridge(dev, x58_pcie_topology_scan_bus_with_endpoint_probe);
	if (dev->path.pci.devfn == PCI_DEVFN(0x1c, 1))
		x58_pci_jmb_admit_mode(dev, static_fn1);
}

static void x58_pci_require_static_topology(void)
{
	struct device *root;
	size_t root_count = 0;
	size_t downstream_count = 0;

	if (x58_pci_domain == NULL || x58_pci_domain->downstream == NULL)
		die_with_post_code(POST_X58_PCI_TOPOLOGY_FAIL,
			"[RAMSTAGE] X58_PCI domain has no static downstream bus\n");
	x58_pci_jmb_verify_mode();

	for (root = x58_pci_domain->downstream->children; root;
	     root = root->sibling) {
		const struct x58_pci_expected_pci *expected =
			x58_pci_expected_for_device(root);

		root_count++;
		for (struct device *prior = x58_pci_domain->downstream->children;
		     prior != root; prior = prior->sibling)
			if (prior->path.pci.devfn == root->path.pci.devfn)
				die_with_post_code(POST_X58_PCI_TOPOLOGY_FAIL,
					"[RAMSTAGE] duplicate static PCI root\n");
		if (x58_pci_is_standard_lpc(root)) {
			x58_pci_standard_lpc_probe(root);
			continue;
		}
		const struct device_operations *required_ops =
			expected && expected->bridge ? &x58_pci_root_port_ops :
			&x58_pci_endpoint_ops;
		if (root->path.pci.devfn == PCI_DEVFN(0x1f, 2))
			required_ops = &x58_pci_standard_sata_ops;
		if (expected && expected->class_code == PCI_CLASS_SERIAL_USB)
			required_ops = PCI_FUNC(root->path.pci.devfn) == 7 ?
				&x58_pci_standard_ehci_ops : &x58_pci_standard_uhci_ops;
		if (expected == NULL || expected->downstream || !root->enabled ||
		    !root->mandatory ||
		    root->ops != required_ops)
			die_with_post_code(POST_X58_PCI_TOPOLOGY_FAIL,
				"[RAMSTAGE] X58_PCI root devicetree escaped allowlist\n");

		if (expected->bridge) {
			struct device *child;
			const struct x58_pci_expected_pci *child_expected;
			size_t wanted = 0, found = 0;
			for (size_t i = 0; i < ARRAY_SIZE(x58_pci_expected); i++)
				if (x58_pci_expected[i].downstream &&
				    x58_pci_expected[i].root_devfn == expected->root_devfn &&
				    !x58_pci_function_unexposed(&x58_pci_expected[i]))
					wanted++;

			if (root->downstream == NULL && wanted != 0)
				die_with_post_code(POST_X58_PCI_TOPOLOGY_FAIL,
					"[RAMSTAGE] X58_PCI bridge child shape mismatch\n");
			for (child = root->downstream ? root->downstream->children : NULL;
			     child; child = child->sibling) {
				if (x58_pci_dynamic_entry(child)) {
					x58_pci_dynamic_require(child);
					continue;
				}
				child_expected = x58_pci_expected_for_device(child);
				found++;
				if (child_expected == NULL || !child_expected->downstream ||
				    (!child->enabled && !child_expected->runtime_optional) ||
				    !child->mandatory || child->ops != &x58_pci_endpoint_ops)
					die_with_post_code(POST_X58_PCI_TOPOLOGY_FAIL,
						"[RAMSTAGE] X58_PCI bridge child escaped allowlist\n");
				/* Reject duplicate function paths, even before enumeration. */
				for (struct device *prior = root->downstream->children;
				     prior != child; prior = prior->sibling)
					if (prior->path.pci.devfn == child->path.pci.devfn)
						die_with_post_code(POST_X58_PCI_TOPOLOGY_FAIL,
							"[RAMSTAGE] duplicate static PCI child\n");
			}
			if (found != wanted)
				die_with_post_code(POST_X58_PCI_TOPOLOGY_FAIL,
					"[RAMSTAGE] X58_PCI bridge child count mismatch\n");
			downstream_count += found;
		} else if (root->downstream != NULL) {
			die_with_post_code(POST_X58_PCI_TOPOLOGY_FAIL,
				"[RAMSTAGE] X58_PCI endpoint unexpectedly owns a bus\n");
		}
	}

	if (root_count != X58_PCI_ROOT_FUNCTIONS + 1 ||
	    downstream_count != X58_PCI_DOWNSTREAM_FUNCTIONS -
		(unsigned int)x58_pci_jmb_single)
		die_with_post_code(POST_X58_PCI_TOPOLOGY_FAIL,
			"[RAMSTAGE] X58_PCI static PCI function count mismatch\n");
}

static void x58_pci_raw_root_preflight(void)
{
	const uint16_t decode = PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
		PCI_COMMAND_MASTER;

	/* Validate the complete root allowlist before normal PCI programming. */
	for (size_t i = 0; i < ARRAY_SIZE(x58_pci_expected); i++) {
		const struct x58_pci_expected_pci *expected = &x58_pci_expected[i];
		pci_devfn_t raw;
		uint32_t classrev;

		if (expected->downstream)
			continue;
		raw = PCI_DEV(0, PCI_SLOT(expected->root_devfn),
			PCI_FUNC(expected->root_devfn));
		classrev = pci_io_read_config32(raw, PCI_CLASS_REVISION);
		if (pci_io_read_config32(raw, PCI_VENDOR_ID) != expected->id ||
		    (classrev >> 16) != expected->class_code ||
		    (pci_io_read_config8(raw, PCI_HEADER_TYPE) & 0x7f) !=
			expected->header_type)
			die_with_post_code(POST_X58_PCI_IDENTITY_FAIL,
				"[RAMSTAGE] X58_PCI root preflight rejected %s\n",
				expected->name);
	}

	/* Only after every root identity passed may their decodes be quiesced. */
	for (size_t i = 0; i < ARRAY_SIZE(x58_pci_expected); i++) {
		const struct x58_pci_expected_pci *expected = &x58_pci_expected[i];
		pci_devfn_t raw;
		uint16_t command;

		if (expected->downstream)
			continue;
		raw = PCI_DEV(0, PCI_SLOT(expected->root_devfn),
			PCI_FUNC(expected->root_devfn));
		command = pci_io_read_config16(raw, PCI_COMMAND);
		pci_io_write_config16(raw, PCI_COMMAND, command & ~decode);
		if (pci_io_read_config16(raw, PCI_COMMAND) & decode)
			die_with_post_code(POST_X58_PCI_COMMAND_FAIL,
				"[RAMSTAGE] X58_PCI root command clear failed for %s\n",
				expected->name);
	}
}

static void x58_pci_require_bridge_routes(void)
{
	uint8_t secondary[X58_PCI_BRIDGE_FUNCTIONS];
	size_t count = 0;

	for (size_t i = 0; i < ARRAY_SIZE(x58_pci_expected); i++) {
		const struct x58_pci_expected_pci *expected = &x58_pci_expected[i];
		struct device *dev;
		uint32_t buses;

		if (!expected->bridge)
			continue;
		dev = x58_pci_device_for_expected(expected);
		if (dev == NULL || dev->downstream == NULL || count >= ARRAY_SIZE(secondary))
			die_with_post_code(POST_X58_PCI_BUS_FAIL,
				"[RAMSTAGE] X58_PCI bridge route is missing\n");
		buses = pci_read_config32(dev, PCI_PRIMARY_BUS);
		secondary[count++] = (buses >> 8) & 0xff;
		if ((buses & 0xff) != 0 || secondary[count - 1] == 0 ||
		    secondary[count - 1] == 0xff ||
		    ((buses >> 16) & 0xff) != secondary[count - 1] ||
		    dev->downstream->secondary != secondary[count - 1] ||
		    dev->downstream->subordinate != secondary[count - 1])
			die_with_post_code(POST_X58_PCI_BUS_FAIL,
				"[RAMSTAGE] X58_PCI bridge bus-number readback failed\n");
	}
	if (count != ARRAY_SIZE(secondary))
		die_with_post_code(POST_X58_PCI_BUS_FAIL,
			"[RAMSTAGE] X58_PCI bridge route count mismatch\n");
	for (size_t i = 0; i < count; i++)
		for (size_t j = 0; j < i; j++)
			if (secondary[i] == secondary[j])
				die_with_post_code(POST_X58_PCI_BUS_FAIL,
					"[RAMSTAGE] X58_PCI bridge buses are not distinct\n");
}

/* PCIE_ENUM retains the already selected board capability policy. The subsequent
 * reference self-writes consume R/WO opportunities even when values agree.
 */
static struct {
	uint16_t xcap;
	uint32_t slcap, lcap;
} x58_pcie_caps[I82801JX_PCIE_PORT_COUNT];
static unsigned int x58_pcie_initialized;

static void x58_pcie_required_once(void)
{
	static bool attempted;
	struct device *ports[I82801JX_PCIE_PORT_COUNT];
	uint32_t pecr2[I82801JX_PCIE_PORT_COUNT], pec1[I82801JX_PCIE_PORT_COUNT];

	if (attempted)
		die("[PCIE_REQUIRED_FIELDS] duplicate required-field initialization\n");
	attempted = true;
	/* The complete root identity preflight and decode quiesce ran first. */
	for (unsigned int i = 0; i < ARRAY_SIZE(ports); i++) {
		ports[i] = pcidev_on_root(0x1c, i);
		if (!ports[i] || !ports[i]->mandatory || !ports[i]->enabled)
			die("[PCIE_REQUIRED_FIELDS] missing admitted ICH10 root\n");
		pecr2[i] = pci_read_config32(ports[i], I82801JX_PCIE_PECR2);
		pec1[i] = pci_read_config32(ports[i], I82801JX_PCIE_PEC1);
		if (pecr2[i] == UINT32_MAX || pec1[i] == UINT32_MAX)
			die("[PCIE_REQUIRED_FIELDS] inaccessible extended root configuration\n");
		x58_pcie_caps[i].xcap = pci_read_config16(ports[i], I82801JX_PCIE_XCAP);
		x58_pcie_caps[i].slcap = pci_read_config32(ports[i], I82801JX_PCIE_SLCAP);
		x58_pcie_caps[i].lcap = pci_read_config32(ports[i], I82801JX_PCIE_LCAP);
	}
	i82801jx_pcie_setup_retained(ports);
	for (unsigned int i = 0; i < ARRAY_SIZE(ports); i++) {
		uint32_t after_pecr2, after_pec1;

		if (pci_read_config16(ports[i], I82801JX_PCIE_XCAP) != x58_pcie_caps[i].xcap ||
		    pci_read_config32(ports[i], I82801JX_PCIE_SLCAP) != x58_pcie_caps[i].slcap ||
		    pci_read_config32(ports[i], I82801JX_PCIE_LCAP) != x58_pcie_caps[i].lcap)
			die("[PCIE_ENUM] retained capability readback mismatch; no retry\n");
		after_pecr2 = pci_read_config32(ports[i], I82801JX_PCIE_PECR2);
		after_pec1 = pci_read_config32(ports[i], I82801JX_PCIE_PEC1);
		printk(BIOS_NOTICE,
		       "[PCIE_REQUIRED_FIELDS] ROOT=00:1c.%u PECR2=%08x->%08x PEC1=%08x->%08x\n",
		       i, pecr2[i], after_pecr2, pec1[i], after_pec1);
		if (after_pecr2 != (pecr2[i] | I82801JX_PCIE_PECR2_REQUIRED) ||
		    after_pec1 !=
		    ((pec1[i] & ~0xffU) | I82801JX_PCIE_PEC1_REQUIRED))
			die("[PCIE_REQUIRED_FIELDS] required-field readback mismatch; no retry\n");
	}
	printk(BIOS_NOTICE, "[PCIE_ENUM] RETAINED_SETUP_READY PORTS=6 CAP_SELFWRITES=1 HIDING=0\n");
}

static void x58_pci_standard_pcie_init(struct device *dev)
{
	static const struct southbridge_intel_i82801jx_config config;
	const struct x58_pci_expected_pci *expected = x58_pci_expected_for_device(dev);
	unsigned int port = PCI_FUNC(dev->path.pci.devfn);
	uint16_t command, control, lctl, status, secstatus;
	uint32_t mpc, vc0, uem, cem, pecr1;
	uint8_t clocks;

	x58_pci_require_identity(dev);
	/* The IOH graphics root shares the graph ops, not the ICH10 registers. */
	if (dev == pcidev_on_root(3, 0))
		return;
	if (!expected || !expected->bridge || PCI_SLOT(dev->path.pci.devfn) != 0x1c ||
	    port >= I82801JX_PCIE_PORT_COUNT || !dev->enabled ||
	    (x58_pcie_initialized & (1U << port)))
		die("[PCIE_ENUM] reference init order/identity mismatch\n");
	command = pci_read_config16(dev, PCI_COMMAND);
	control = pci_read_config16(dev, PCI_BRIDGE_CONTROL);
	lctl = pci_read_config16(dev, 0x50);
	status = pci_read_config16(dev, PCI_STATUS);
	secstatus = pci_read_config16(dev, PCI_SEC_STATUS);
	mpc = pci_read_config32(dev, 0xd8);
	clocks = pci_read_config8(dev, I82801JX_PCIE_RPDCGEN);
	vc0 = pci_read_config32(dev, 0x114);
	uem = pci_read_config32(dev, 0x148);
	cem = pci_read_config32(dev, 0x154);
	pecr1 = pci_read_config32(dev, 0xe8);
	if (command == UINT16_MAX || control == UINT16_MAX || lctl == UINT16_MAX ||
	    mpc == UINT32_MAX || clocks == UINT8_MAX || vc0 == UINT32_MAX ||
	    uem == UINT32_MAX || cem == UINT32_MAX || pecr1 == UINT32_MAX)
		die("[PCIE_ENUM] post-allocation configuration inaccessible\n");
	if (pci_read_config16(dev, I82801JX_PCIE_XCAP) != x58_pcie_caps[port].xcap ||
	    pci_read_config32(dev, I82801JX_PCIE_SLCAP) != x58_pcie_caps[port].slcap ||
	    pci_read_config32(dev, I82801JX_PCIE_LCAP) != x58_pcie_caps[port].lcap)
		die("[PCIE_ENUM] retained capabilities changed before reference init\n");
	x58_pcie_initialized |= 1U << port;
	i82801jx_pcie_init_sequence_preserve_command(dev, &config);
	if (pci_read_config16(dev, PCI_COMMAND) != command ||
	    pci_read_config8(dev, PCI_CACHE_LINE_SIZE) != 0x10 ||
	    pci_read_config16(dev, PCI_BRIDGE_CONTROL) != (control & ~PCI_BRIDGE_CTL_PARITY) ||
	    pci_read_config16(dev, 0x50) != lctl ||
	    pci_read_config32(dev, 0xd8) != (mpc | 0x80) ||
	    pci_read_config8(dev, I82801JX_PCIE_RPDCGEN) != (clocks | 0x0f) ||
	    pci_read_config32(dev, 0x114) != ((vc0 & ~0xffU) | 1) ||
	    pci_read_config32(dev, 0x148) != (uem | (1U << 14)) ||
	    pci_read_config32(dev, 0x154) != cem ||
	    pci_read_config32(dev, 0xe8) != (pecr1 | ((lctl & 3) == 3 ? 2U : 0U)) ||
	    pci_read_config16(dev, I82801JX_PCIE_XCAP) != x58_pcie_caps[port].xcap ||
	    pci_read_config32(dev, I82801JX_PCIE_SLCAP) != x58_pcie_caps[port].slcap ||
	    pci_read_config32(dev, I82801JX_PCIE_LCAP) != x58_pcie_caps[port].lcap)
		die("[PCIE_ENUM] reference init readback failed; no retry/rollback\n");
	/* Status self-writes acknowledge RW1C errors, unlike the config above. */
	printk(BIOS_NOTICE, "[PCIE_ENUM] ROOT_INIT=00:1c.%u COMMAND=%04x STATUS=%04x->%04x SECSTATUS=%04x->%04x\n",
		port, command, status, pci_read_config16(dev, PCI_STATUS),
		secstatus, pci_read_config16(dev, PCI_SEC_STATUS));
}

static void __maybe_unused x58_pci_standard_pcie_handoff(void *unused)
{
	(void)unused;
	x58_pci_dynamic_verify_isolation();
	if (x58_pcie_initialized != 0x3f)
		die("[PCIE_ENUM] incomplete reference root initialization\n");
	printk(BIOS_NOTICE, "[PCIE_ENUM] REFERENCE_INIT_READY PORTS=6 COMMAND_POLICY=PRESERVED HOTPLUG_CONFIG=0\n");
}

static void x58_pci_require_enumerated_topology(void)
{
	x58_pci_require_static_topology();
	for (size_t i = 0; i < ARRAY_SIZE(x58_pci_expected); i++) {
		const struct x58_pci_expected_pci *expected = &x58_pci_expected[i];
		struct device *dev = x58_pci_device_for_expected(expected);

		if (x58_pci_function_unexposed(expected))
			continue;
		if (dev == NULL)
			die_with_post_code(POST_X58_PCI_TOPOLOGY_FAIL,
				"[RAMSTAGE] X58_PCI selected function disappeared\n");
		if (x58_pci_optional_device_absent(expected, dev)) {
			x58_pci_report_optional_gpu_absent();
			continue;
		}
		x58_pci_require_identity(dev);
		if (pci_read_config16(dev, PCI_COMMAND) &
		    (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER))
			die_with_post_code(POST_X58_PCI_COMMAND_FAIL,
				"[RAMSTAGE] X58_PCI selected function not quiescent\n");
	}
	x58_pci_require_bridge_routes();
	x58_pci_dynamic_quiescent();
}

static void x58_pci_domain_scan_bus(struct device *dev)
{
	if (dev != x58_pci_domain)
		die_with_post_code(POST_X58_PCI_TOPOLOGY_FAIL,
			"[RAMSTAGE] X58_PCI unexpected PCI domain\n");

	post_code(POST_X58_PCI_PREFLIGHT);
	printk(BIOS_NOTICE, "[RAMSTAGE] %s selective PCI preflight\n",
	       CONFIG_MAINBOARD_PART_NUMBER);
	x58_pci_require_platform_state("pre-scan");
	x58_pci_require_static_topology();
	x58_tco_halt_tco_once();
	x58_usb_ich10_program_baseline_once();
	x58_sata_config_program_ahci_route_once();
	x58_pci_raw_root_preflight();
	x58_pcie_required_once();
	x58_pcie_topology_program_ioh_bus_number_once();
	x58_usb_program_ehci_once();
	post_code(POST_X58_PCI_ROOTS_SAFE);
	x58_pci_start_iou0_once();

	pci_host_bridge_scan_bus(dev);
	x58_pci_require_enumerated_topology();
	/* PIRQ walks all_devices, so downstream bus numbers must be real first. */
	x58_pci_standard_lpc_init_once();
	x58_irq_route_program_vendor_irq_once();
	x58_platform_legacy_enable_serirq();
	post_code(POST_X58_PCI_SCAN_OK);
	for (size_t i = 0; i < ARRAY_SIZE(x58_pci_expected); i++) {
		const struct x58_pci_expected_pci *expected = &x58_pci_expected[i];
		struct device *gpu;

		if (!expected->runtime_optional)
			continue;
		gpu = x58_pci_device_for_expected(expected);
		if (x58_pci_optional_device_absent(expected, gpu))
			continue;
		post_code(POST_X58_PCI_GPU_PRESENT);
		printk(BIOS_NOTICE,
		       "[PCI] GPU_PROBE root=00:03.0 ID=%04x:%04x CLASS=%06x HDR=%02x ROMBAR=%08x RESULT=AMD_VGA_PRESENT_PHYSICAL_VBIOS\n",
		       gpu->vendor, gpu->device, gpu->class, gpu->hdr_type,
		       pci_read_config32(gpu, PCI_ROM_ADDRESS));
	}
	printk(BIOS_NOTICE,
	       "[RAMSTAGE] X58_PCI strict roots and required endpoints accepted; optional GPU policy applied\n");
}

static void x58_pci_read_resources(struct device *dev)
{
	x58_pci_require_platform_state("resource-map");
	x58_pci_require_enumerated_topology();

	/* Legacy VGA/option-ROM layout retained from the X58_MEMORY_PROFILE payload path. */
	ram_range(dev, 0, 0x00000000, 0x000a0000);
	mmio_range(dev, 1, 0x000a0000, 0x00020000);
	reserved_ram_range(dev, 2, 0x000c0000, 0x00040000);

	/* Exact gated one-DIMM map: 3 GiB low plus 1 GiB remapped above 4 GiB. */
	ram_from_to(dev, 3, X58_PCI_LOW_RAM_BASE, X58_PCI_LOW_RAM_TOP);
	ram_from_to(dev, 4, X58_PCI_HIGH_RAM_BASE, X58_PCI_HIGH_RAM_TOP);
	x58_platform_reserve(dev);

	/* The allocator receives no subtractive or above-4G PCI window. */
	domain_io_window_from_to(dev, 5, X58_PCI_IO_BASE,
		X58_PCI_IO_TOP);
	domain_mem_window_from_to(dev, 6, X58_PCI_MMIO_BASE,
		X58_PCI_MMIO_TOP);
	mmio_from_to(dev, 7, X58_PCI_ECAM_BASE, X58_PCI_ECAM_TOP);

	post_code(POST_X58_PCI_RESOURCES);
	printk(BIOS_NOTICE,
	       "[RAMSTAGE] X58_PCI RAM=0-640K,1M-3G,4G-5G PCI_IO=1000-ffff PCI_MMIO=c0000000-dfffffff\n");
}

static void x58_platform_resource_read_resources(struct device *dev)
{
	if (!x58_pci_standard_lpc_ready)
		die_with_post_code(POST_X58_PCI_RESOURCE_FAIL,
			"[RESOURCE] X58_PLATFORM_RESOURCE inherited X58_IOAPIC IOAPIC stage did not run\n");

	x58_pci_read_resources(dev);

	/* Resource metadata only: every decode was established by an earlier stage. */
	fixed_io_range_reserved(dev, X58_PLATFORM_RESOURCE_SMBUS_RESOURCE_INDEX,
		X58_PLATFORM_RESOURCE_SMBUS_IO_BASE, X58_PLATFORM_RESOURCE_SMBUS_IO_SIZE);
	fixed_io_range_reserved(dev, X58_PLATFORM_RESOURCE_PM_RESOURCE_INDEX,
		X58_PLATFORM_RESOURCE_PM_IO_BASE, X58_PLATFORM_RESOURCE_PM_IO_SIZE);
	fixed_io_range_reserved(dev, X58_PLATFORM_RESOURCE_GPIO_RESOURCE_INDEX,
		X58_PLATFORM_RESOURCE_GPIO_IO_BASE, X58_PLATFORM_RESOURCE_GPIO_IO_SIZE);
	mmio_range(dev, X58_PLATFORM_RESOURCE_IOAPIC_RESOURCE_INDEX,
		X58_PLATFORM_RESOURCE_IOAPIC_BASE, X58_PLATFORM_RESOURCE_IOAPIC_SIZE);
	mmio_range(dev, X58_PLATFORM_RESOURCE_RCBA_RESOURCE_INDEX,
		X58_PLATFORM_RESOURCE_RCBA_BASE, X58_PLATFORM_RESOURCE_RCBA_SIZE);
	mmio_range(dev, X58_PLATFORM_RESOURCE_LAPIC_RESOURCE_INDEX,
		X58_PLATFORM_RESOURCE_LAPIC_BASE, X58_PLATFORM_RESOURCE_LAPIC_SIZE);
	mmio_range(dev, X58_PLATFORM_RESOURCE_ROM_RESOURCE_INDEX,
		X58_PLATFORM_RESOURCE_ROM_BASE, X58_PLATFORM_RESOURCE_ROM_SIZE);

	printk(BIOS_NOTICE,
	       "[RESOURCE] X58_PLATFORM_RESOURCE fixed reservations 8..14 installed; allocator apertures unchanged\n");
}

static void x58_hpet_read_resources(struct device *dev)
{
	if (!x58_hpet_ready)
		die_with_post_code(POST_X58_PCI_RESOURCE_FAIL,
			"[RESOURCE] X58_HPET HPET decode/identity gate did not finish\n");

	x58_platform_resource_read_resources(dev);
	mmio_range(dev, X58_HPET_RESOURCE_INDEX,
		X58_HPET_BASE, X58_HPET_SIZE);
	printk(BIOS_NOTICE,
	       "[RESOURCE] X58_HPET HPET reservation 15 installed; X58_PLATFORM_RESOURCE resources preserved\n");
}

static void x58_tco_read_resources(struct device *dev)
{
	if (!x58_tco_ready)
		die_with_post_code(POST_X58_PCI_RESOURCE_FAIL,
			"[RESOURCE] X58_TCO exact TCO halt gate did not finish\n");

	x58_hpet_read_resources(dev);
	printk(BIOS_NOTICE,
	       "[RESOURCE] X58_TCO preserves X58_HPET exact resources 0..15 unchanged\n");
}

static void x58_pci_require_domain_resource(unsigned long index, uint64_t base,
	uint64_t top, unsigned long required_flags)
{
	const struct resource *resource = probe_resource(x58_pci_domain, index);
	bool bounds_exact;

	if (resource != NULL && (resource->flags & IORESOURCE_FIXED))
		bounds_exact = resource->base == base &&
			resource->size == top - base;
	else
		bounds_exact = resource != NULL && resource->base == base &&
			resource->limit == top - 1;

	if (!bounds_exact ||
	    (resource->flags & required_flags) != required_flags)
		die_with_post_code(POST_X58_PCI_RESOURCE_FAIL,
			"[RAMSTAGE] X58_PCI domain resource %lx mismatch\n", index);
}

static bool x58_platform_resource_domain_resource_bounds_are_exact(
	const struct resource *resource,
	const struct x58_platform_resource_domain_resource_contract *expected)
{
	if (resource->base != expected->base || resource->index != expected->index)
		return false;
	if (expected->flags & IORESOURCE_FIXED)
		return resource->size == expected->top - expected->base &&
			resource_end(resource) == expected->top - 1;

	return resource->size == 0 && resource->limit == expected->top - 1;
}

static void x58_platform_resource_require_exact_domain_resources(void)
{
	const struct resource *resource;
	size_t actual_count = 0;

	for (resource = x58_pci_domain->resource_list; resource;
	     resource = resource->next)
		actual_count++;
	x58_platform_verify_reservations(x58_pci_domain);
	if (actual_count < X58_PLATFORM_EXTRA_RESOURCES)
		die("[PLATFORM] resource count underflow\n");
	actual_count -= X58_PLATFORM_EXTRA_RESOURCES;
	if (actual_count != X58_PLATFORM_RESOURCE_DOMAIN_RESOURCE_COUNT &&
	    actual_count != X58_PLATFORM_RESOURCE_FULL_DOMAIN_RESOURCE_COUNT)
		die_with_post_code(POST_X58_PCI_RESOURCE_FAIL,
			"[RESOURCE] X58_PLATFORM_RESOURCE domain count=%zu outside base/full contract\n",
			actual_count);

	for (size_t i = 0; i < ARRAY_SIZE(x58_platform_resource_domain_resource_contracts); i++) {
		const struct x58_platform_resource_domain_resource_contract *expected =
			&x58_platform_resource_domain_resource_contracts[i];

		resource = probe_resource(x58_pci_domain, expected->index);
		if (resource == NULL || resource->flags != expected->flags ||
		    !x58_platform_resource_domain_resource_bounds_are_exact(resource, expected))
			die_with_post_code(POST_X58_PCI_RESOURCE_FAIL,
				"[RESOURCE] X58_PLATFORM_RESOURCE exact domain resource %lx mismatch\n",
				expected->index);
	}

	for (size_t i = 0; i < ARRAY_SIZE(x58_platform_resource_domain_resource_contracts); i++) {
		const struct x58_platform_resource_domain_resource_contract *left =
			&x58_platform_resource_domain_resource_contracts[i];
		const unsigned long left_type = left->flags & IORESOURCE_TYPE_MASK;

		for (size_t j = i + 1;
		     j < ARRAY_SIZE(x58_platform_resource_domain_resource_contracts); j++) {
			const struct x58_platform_resource_domain_resource_contract *right =
				&x58_platform_resource_domain_resource_contracts[j];
			const unsigned long right_type = right->flags &
				IORESOURCE_TYPE_MASK;

			if (left_type == right_type && left->base < right->top &&
			    right->base < left->top)
				die_with_post_code(POST_X58_PCI_OVERLAP_FAIL,
					"[RESOURCE] X58_PLATFORM_RESOURCE overlapping domain resources %lx/%lx\n",
					left->index, right->index);
		}
	}
}

static void x58_hpet_require_exact_hpet_resource(void)
{
	const struct resource *resource = probe_resource(x58_pci_domain,
		X58_HPET_RESOURCE_INDEX);
	size_t actual_count = 0;

	for (const struct resource *entry = x58_pci_domain->resource_list; entry;
	     entry = entry->next)
		actual_count++;
	x58_platform_verify_reservations(x58_pci_domain);
	if (actual_count < X58_PLATFORM_EXTRA_RESOURCES)
		die("[PLATFORM] resource count underflow\n");
	actual_count -= X58_PLATFORM_EXTRA_RESOURCES;
	if (actual_count != X58_HPET_DOMAIN_RESOURCE_COUNT)
		die_with_post_code(POST_X58_PCI_RESOURCE_FAIL,
			"[RESOURCE] X58_HPET domain count=%zu expected=16\n",
			actual_count);

	if (resource == NULL ||
	    resource->flags != x58_hpet_resource_contract.flags ||
	    !x58_platform_resource_domain_resource_bounds_are_exact(resource,
		&x58_hpet_resource_contract))
		die_with_post_code(POST_X58_PCI_RESOURCE_FAIL,
			"[RESOURCE] X58_HPET exact HPET resource 15 mismatch\n");

	for (size_t i = 0; i < ARRAY_SIZE(x58_platform_resource_domain_resource_contracts); i++) {
		const struct x58_platform_resource_domain_resource_contract *other =
			&x58_platform_resource_domain_resource_contracts[i];

		if ((other->flags & IORESOURCE_TYPE_MASK) == IORESOURCE_MEM &&
		    x58_hpet_resource_contract.base < other->top &&
		    other->base < x58_hpet_resource_contract.top)
			die_with_post_code(POST_X58_PCI_OVERLAP_FAIL,
				"[RESOURCE] X58_HPET HPET overlaps domain resource %lx\n",
				other->index);
	}
}

static void x58_pci_require_allocated_resource(const struct device *dev,
	const struct resource *resource)
{
	uint64_t top;
	const unsigned long type = resource->flags &
		(IORESOURCE_IO | IORESOURCE_MEM);

	if (!resource->size || !type)
		return;
	if (type == (IORESOURCE_IO | IORESOURCE_MEM) ||
	    resource->base > UINT64_MAX - resource->size)
		die_with_post_code(POST_X58_PCI_RESOURCE_FAIL,
			"[RAMSTAGE] X58_PCI malformed resource on %s\n", dev_path(dev));
	top = resource->base + resource->size;
	if (!(resource->flags & IORESOURCE_ASSIGNED) ||
	    !(resource->flags & IORESOURCE_STORED) ||
	    (resource->flags & IORESOURCE_ABOVE_4G) ||
	    resource_end(resource) != top - 1 ||
	    (type == IORESOURCE_IO &&
	     (resource->base < X58_PCI_IO_BASE || top > X58_PCI_IO_TOP)) ||
	    (type == IORESOURCE_MEM &&
	     (resource->base < X58_PCI_MMIO_BASE ||
	      top > X58_PCI_MMIO_TOP)))
		die_with_post_code(POST_X58_PCI_RESOURCE_FAIL,
			"[RAMSTAGE] X58_PCI resource escaped aperture on %s\n",
			dev_path(dev));
}

static struct device *x58_sata_resource_sata_device(void)
{
	if (x58_pci_domain == NULL || x58_pci_domain->downstream == NULL)
		return NULL;
	return pcidev_path_behind(x58_pci_domain->downstream,
		PCI_DEVFN(0x1f, 2));
}

static bool x58_sata_resource_abar_resource_is_exact(const struct resource *abar,
	uint32_t raw_bar)
{
	uint64_t top;
	const unsigned long required = IORESOURCE_MEM | IORESOURCE_ASSIGNED |
		IORESOURCE_STORED;

	if (abar == NULL || abar->size != X58_SATA_RESOURCE_ABAR_SIZE ||
	    abar->gran != X58_SATA_RESOURCE_ABAR_GRANULARITY ||
	    abar->align < X58_SATA_RESOURCE_ABAR_GRANULARITY ||
	    (abar->base & (X58_SATA_RESOURCE_ABAR_SIZE - 1)) != 0 ||
	    abar->flags != required ||
	    abar->base > UINT64_MAX - abar->size)
		return false;
	top = abar->base + abar->size;

	return abar->base >= X58_PCI_MMIO_BASE &&
		top <= X58_PCI_MMIO_TOP &&
		resource_end(abar) == top - 1 &&
		(raw_bar & PCI_BASE_ADDRESS_MEM_ATTR_MASK) ==
			PCI_BASE_ADDRESS_SPACE_MEMORY &&
		(raw_bar & ~((uint32_t)PCI_BASE_ADDRESS_MEM_ATTR_MASK)) ==
			abar->base;
}


_Static_assert(I82801JX_SATA_PCS_PORT_ENABLE_MASK == 0x3f,
	"X58_SATA_HANDOFF may select only PCS port-enable bits 5:0");
_Static_assert(I82801JX_SATA_PCS_ALL_PORTS_ENABLED == 0x3f,
	"X58_SATA_HANDOFF requires all six ICH10 SATA ports");
_Static_assert(I82801JX_SATA_PCS_LOW_NON_PORT_MASK == 0xc0,
	"X58_SATA_HANDOFF must gate PCS low-byte non-port fields");
_Static_assert(I82801JX_SATA_SCLKCG_FIELD1_MASK == 0x000001ff,
	"X58_SATA_HANDOFF may select only SCLKCG bits 8:0");
_Static_assert(I82801JX_SATA_SCLKCG_FIELD1_REQUIRED == 0x00000193,
	"X58_SATA_HANDOFF requires the proven SCLKCG Field 1 target");
_Static_assert(I82801JX_SATA_SCLKCG_RESERVED_23_9 == 0x00fffe00 &&
	I82801JX_SATA_SCLKCG_PORT_DISABLE_MASK == 0x3f000000 &&
	I82801JX_SATA_SCLKCG_RESERVED_31_30 == 0xc0000000,
	"X58_SATA_HANDOFF SCLKCG reserved/port-disable masks changed");

static bool x58_sata_handoff_snapshot_is_exact(
	const struct x58_sata_config_sata_snapshot *snapshot, struct device *sata,
	const struct resource *abar, uint16_t expected_command)
{
	const uint16_t decode = PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
		PCI_COMMAND_MASTER;

	return x58_sata_config_fixed_gates_are_exact(snapshot) &&
		snapshot->fd == X58_SATA_CONFIG_FD_SATA2_DISABLED &&
		sata != NULL && sata->enabled && sata->mandatory &&
		(sata->command & decode) == PCI_COMMAND_MEMORY &&
		snapshot->f2_id == X58_SATA_MAP_SATA1_AHCI_ID &&
		snapshot->f2_classrev == X58_SATA_CONFIG_SATA1_AHCI_CLASSREV &&
		snapshot->f2_command == expected_command &&
		snapshot->f2_header == PCI_HEADER_TYPE_NORMAL &&
		snapshot->f2_map == I82801JX_SATA_MAP_AHCI_D31F2_VALUE &&
		x58_sata_resource_abar_resource_is_exact(abar, snapshot->f2_bar5) &&
		snapshot->f5_id == 0xffffffffu &&
		snapshot->f5_classrev == 0xffffffffu &&
		snapshot->f5_command == 0xffffu &&
		snapshot->f5_header == 0xffu &&
		snapshot->f5_bar5 == 0xffffffffu &&
		snapshot->f5_map == 0xffffu;
}

static bool x58_sata_handoff_sclk_is_admitted(uint32_t sclkcg)
{
	return sclkcg == 0 ||
		sclkcg == I82801JX_SATA_SCLKCG_FIELD1_REQUIRED;
}

static void x58_sata_handoff_log_state(const char *phase,
	const struct x58_sata_config_sata_snapshot *snapshot, struct device *sata,
	const struct resource *abar)
{
	const uint64_t base = abar != NULL ? abar->base : 0;
	const uint64_t size = abar != NULL ? abar->size : 0;
	const unsigned long flags = abar != NULL ? abar->flags : 0;

	printk(BIOS_NOTICE,
	       "[SATA] X58_SATA_HANDOFF %s LPC_ID=%08x RCBA=%08x FDSW=%02x FD=%08x "
	       "F2_ID=%08x CLASSREV=%08x CMD=%04x HDR=%02x BAR5=%08x "
	       "MAP=%04x PCS=%04x EN=%02x PRESENCE=%02x ORM=%u RSV14=%u "
	       "SCLKCG=%08x RES=%s BASE=%llx SIZE=%llx ALIGN=%u GRAN=%u "
	       "FLAGS=%08lx POLICY_CMD=%04x F5_ID=%08x\n",
	       phase, snapshot->lpc_id, snapshot->rcba, snapshot->fdsw,
	       snapshot->fd, snapshot->f2_id, snapshot->f2_classrev,
	       snapshot->f2_command, snapshot->f2_header, snapshot->f2_bar5,
	       snapshot->f2_map, snapshot->f2_pcs,
	       snapshot->f2_pcs & I82801JX_SATA_PCS_PORT_ENABLE_MASK,
	       (snapshot->f2_pcs & I82801JX_SATA_PCS_PRESENCE_MASK) >> 8,
	       !!(snapshot->f2_pcs & I82801JX_SATA_PCS_OOB_RETRY_MODE),
	       !!(snapshot->f2_pcs & I82801JX_SATA_PCS_RESERVED_14),
	       snapshot->f2_sclkcg, sata != NULL ? dev_path(sata) : "missing",
	       (unsigned long long)base, (unsigned long long)size,
	       abar != NULL ? abar->align : 0,
	       abar != NULL ? abar->gran : 0, flags,
	       sata != NULL ? sata->command : 0, snapshot->f5_id);
}

static bool x58_legacy_input_sata_policy_prestate_is_exact(
	const struct x58_sata_config_sata_snapshot *snapshot, struct device *sata,
	const struct resource *abar)
{
	return x58_sata_config_fixed_gates_are_exact(snapshot) &&
		snapshot->fd == X58_SATA_CONFIG_FD_SATA2_DISABLED &&
		sata != NULL && sata->enabled && sata->mandatory &&
		sata->command == (PCI_COMMAND_IO | PCI_COMMAND_MEMORY) &&
		snapshot->f2_id == X58_SATA_MAP_SATA1_AHCI_ID &&
		snapshot->f2_classrev == X58_SATA_CONFIG_SATA1_AHCI_CLASSREV &&
		snapshot->f2_command == 0 &&
		snapshot->f2_header == PCI_HEADER_TYPE_NORMAL &&
		snapshot->f2_map == I82801JX_SATA_MAP_AHCI_D31F2_VALUE &&
		x58_sata_resource_abar_resource_is_exact(abar, snapshot->f2_bar5) &&
		snapshot->f2_pcs == 0 && snapshot->f2_sclkcg == 0 &&
		snapshot->f5_id == 0xffffffffu &&
		snapshot->f5_classrev == 0xffffffffu &&
		snapshot->f5_command == 0xffffu &&
		snapshot->f5_header == 0xffu &&
		snapshot->f5_bar5 == 0xffffffffu &&
		snapshot->f5_map == 0xffffu;
}

static void x58_legacy_input_correct_sata_policy_once(void)
{
	struct device *sata = x58_sata_resource_sata_device();
	const struct resource *abar = sata != NULL ?
		probe_resource(sata, I82801JX_SATA_ABAR) : NULL;
	const struct x58_sata_config_sata_snapshot before = x58_sata_config_read_sata_snapshot();
	struct x58_sata_config_sata_snapshot after;

	if (x58_legacy_input_sata_policy_ready)
		die_with_post_code(POST_X58_LEGACY_INPUT_POLICY_FAIL,
			"[SATA] X58_LEGACY_INPUT refused a second policy correction\n");
	x58_sata_handoff_log_state("X58_LEGACY_INPUT_POLICY_PRE", &before, sata, abar);
	if (!x58_legacy_input_sata_policy_prestate_is_exact(&before, sata, abar))
		die_with_post_code(POST_X58_LEGACY_INPUT_POLICY_FAIL,
			"[SATA] X58_LEGACY_INPUT rejected exact hardware/policy prestate\n");

	post_code(POST_X58_LEGACY_INPUT_POLICY_BEGIN);
	/* Software policy only: the hardware command register stays at 0000. */
	sata->command &= ~PCI_COMMAND_IO;
	after = x58_sata_config_read_sata_snapshot();
	x58_sata_handoff_log_state("X58_LEGACY_INPUT_POLICY_POST", &after, sata, abar);
	if (!x58_sata_handoff_snapshot_is_exact(&after, sata, abar, 0) ||
	    after.f2_pcs != before.f2_pcs ||
	    after.f2_sclkcg != before.f2_sclkcg)
		die_with_post_code(POST_X58_LEGACY_INPUT_POLICY_FAIL,
			"[SATA] X58_LEGACY_INPUT policy or hardware readback mismatch\n");

	x58_legacy_input_sata_policy_ready = true;
	post_code(POST_X58_LEGACY_INPUT_POLICY_READY);
	printk(BIOS_NOTICE,
	       "[SATA] X58_LEGACY_INPUT POLICY_CMD 0003->0002 HARDWARE_CMD=0000 HARDWARE_WRITE=0\n");
}

static void x58_sata_handoff_log_presence_samples(void)
{
	static const unsigned int delta_us[X58_SATA_RESOURCE_PCS_SAMPLE_COUNT] = {
		0, 1000, 9000, 90000, 400000,
	};
	unsigned int elapsed_us = 0;

	for (size_t i = 0; i < ARRAY_SIZE(delta_us); i++) {
		uint16_t pcs;

		if (delta_us[i] != 0)
			udelay(delta_us[i]);
		elapsed_us += delta_us[i];
		pcs = pci_io_read_config16(X58_SATA_MAP_SATA1_DEV,
			I82801JX_SATA_PCS);
		printk(BIOS_NOTICE,
		       "[SATA] X58_SATA_HANDOFF PCS_SAMPLE T_MS=%u PCS=%04x EN=%02x "
		       "PRESENCE=%02x\n",
		       elapsed_us / 1000, pcs,
		       pcs & I82801JX_SATA_PCS_PORT_ENABLE_MASK,
		       (pcs & I82801JX_SATA_PCS_PRESENCE_MASK) >> 8);
	}
}

static void __maybe_unused x58_sata_handoff_program_pcs_once(void)
{
	struct device *sata = x58_sata_resource_sata_device();
	const struct resource *abar = sata != NULL ?
		probe_resource(sata, I82801JX_SATA_ABAR) : NULL;
	struct x58_sata_config_sata_snapshot before = x58_sata_config_read_sata_snapshot();
	struct x58_sata_config_sata_snapshot after;
	const uint8_t before_low = before.f2_pcs;
	bool wrote = false;

	if (!x58_legacy_input_sata_policy_ready)
		die_with_post_code(POST_X58_LEGACY_INPUT_POLICY_FAIL,
			"[SATA] X58_LEGACY_INPUT policy correction did not precede PCS\n");

	if (x58_sata_handoff_pcs_attempted)
		die_with_post_code(POST_X58_SATA_HANDOFF_FAIL,
			"[SATA] X58_SATA_HANDOFF refused a second PCS attempt\n");
	x58_sata_handoff_pcs_attempted = true;
	x58_sata_handoff_log_state("PCS_PRE", &before, sata, abar);

	if (!x58_sata_handoff_snapshot_is_exact(&before, sata, abar, 0) ||
	    (before_low != 0 &&
	     before_low != I82801JX_SATA_PCS_ALL_PORTS_ENABLED) ||
	    (before_low & I82801JX_SATA_PCS_LOW_NON_PORT_MASK) != 0 ||
	    (before.f2_pcs & I82801JX_SATA_PCS_RESERVED_14) != 0 ||
	    !x58_sata_handoff_sclk_is_admitted(before.f2_sclkcg))
		die_with_post_code(POST_X58_SATA_HANDOFF_FAIL,
			"[SATA] X58_SATA_HANDOFF rejected corrected-route/resource/PCS prestate\n");

	post_code(POST_X58_SATA_HANDOFF_PCS_BEGIN);
	printk(BIOS_NOTICE,
	       "[SATA] X58_SATA_HANDOFF PCS_STAGE begin: select only PCS[5:0]=3f\n");
	if (before_low != I82801JX_SATA_PCS_ALL_PORTS_ENABLED) {
		i82801jx_sata_enable_all_ports(X58_SATA_MAP_SATA1_DEV);
		wrote = true;
	}
	after = x58_sata_config_read_sata_snapshot();
	x58_sata_handoff_log_state("PCS_POST", &after, sata, abar);
	if (!x58_sata_handoff_snapshot_is_exact(&after, sata, abar, 0) ||
	    (uint8_t)after.f2_pcs !=
		I82801JX_SATA_PCS_ALL_PORTS_ENABLED ||
	    (after.f2_pcs & I82801JX_SATA_PCS_RESERVED_14) != 0 ||
	    (after.f2_pcs & I82801JX_SATA_PCS_OOB_RETRY_MODE) !=
		(before.f2_pcs & I82801JX_SATA_PCS_OOB_RETRY_MODE) ||
	    after.f2_sclkcg != before.f2_sclkcg)
		die_with_post_code(POST_X58_SATA_HANDOFF_FAIL,
			"[SATA] X58_SATA_HANDOFF PCS selected-field/full-route readback failed\n");

	post_code(POST_X58_SATA_HANDOFF_PCS_OK);
	printk(BIOS_NOTICE,
	       "[SATA] X58_SATA_HANDOFF PCS[5:0]=3f gate PASS (write=%u); "
	       "SCLKCG unchanged, presence ignored, ABAR MMIO/BME untouched\n",
	       (unsigned int)wrote);
	x58_sata_handoff_log_presence_samples();
}

static void __maybe_unused x58_sata_handoff_program_sclk_once(void)
{
	struct device *sata = x58_sata_resource_sata_device();
	const struct resource *abar = sata != NULL ?
		probe_resource(sata, I82801JX_SATA_ABAR) : NULL;
	struct x58_sata_config_sata_snapshot before = x58_sata_config_read_sata_snapshot();
	struct x58_sata_config_sata_snapshot after;
	const uint16_t pcs_policy_mask =
		I82801JX_SATA_PCS_PORT_ENABLE_MASK |
		I82801JX_SATA_PCS_LOW_NON_PORT_MASK |
		I82801JX_SATA_PCS_RESERVED_14 |
		I82801JX_SATA_PCS_OOB_RETRY_MODE;
	bool wrote = false;

	if (x58_sata_handoff_sclk_attempted)
		die_with_post_code(POST_X58_SATA_HANDOFF_FAIL,
			"[SATA] X58_SATA_HANDOFF refused a second SCLKCG attempt\n");
	x58_sata_handoff_sclk_attempted = true;
	x58_sata_handoff_log_state("SCLK_PRE", &before, sata, abar);

	if (!x58_sata_handoff_pcs_attempted ||
	    !x58_sata_handoff_snapshot_is_exact(&before, sata, abar, 0) ||
	    (uint8_t)before.f2_pcs !=
		I82801JX_SATA_PCS_ALL_PORTS_ENABLED ||
	    (before.f2_pcs & I82801JX_SATA_PCS_RESERVED_14) != 0 ||
	    !x58_sata_handoff_sclk_is_admitted(before.f2_sclkcg))
		die_with_post_code(POST_X58_SATA_HANDOFF_FAIL,
			"[SATA] X58_SATA_HANDOFF rejected PCS-complete/SCLKCG prestate\n");

	post_code(POST_X58_SATA_HANDOFF_SCLK_BEGIN);
	printk(BIOS_NOTICE,
	       "[SATA] X58_SATA_HANDOFF SCLK_STAGE begin: select only SCLKCG[8:0]=193\n");
	if (before.f2_sclkcg != I82801JX_SATA_SCLKCG_FIELD1_REQUIRED) {
		i82801jx_sata_program_clock_field(X58_SATA_MAP_SATA1_DEV);
		wrote = true;
	}
	after = x58_sata_config_read_sata_snapshot();
	x58_sata_handoff_log_state("SCLK_POST", &after, sata, abar);
	if (!x58_sata_handoff_snapshot_is_exact(&after, sata, abar, 0) ||
	    after.f2_sclkcg != I82801JX_SATA_SCLKCG_FIELD1_REQUIRED ||
	    (uint8_t)after.f2_pcs !=
		I82801JX_SATA_PCS_ALL_PORTS_ENABLED ||
	    (after.f2_pcs & pcs_policy_mask) !=
		(before.f2_pcs & pcs_policy_mask))
		die_with_post_code(POST_X58_SATA_HANDOFF_FAIL,
			"[SATA] X58_SATA_HANDOFF SCLKCG full-dword/PCS/route readback failed\n");

	post_code(POST_X58_SATA_HANDOFF_SCLK_OK);
	printk(BIOS_NOTICE,
	       "[SATA] X58_SATA_HANDOFF SCLKCG=00000193 gate PASS (write=%u); "
	       "PCD/reserved=0, corrected route retained, ABAR MMIO/BME untouched\n",
	       (unsigned int)wrote);
	post_code(POST_X58_SATA_HANDOFF_READY);
	printk(BIOS_NOTICE,
	       "[SATA] X58_SATA_HANDOFF corrected route + PCS + SCLKCG READY\n");
}

_Static_assert(X58_SATA_RESOURCE_ABAR_SIZE == 0x800,
	"X58_AHCI requires the ICH10 2-KiB ABAR");
_Static_assert(X58_AHCI_GHC_AE == 0x80000000u,
	"X58_AHCI may select only the AHCI-enable bit in GHC");
_Static_assert(X58_AHCI_PI_TARGET == 0x0000003fu,
	"X58_AHCI implements exactly six AHCI ports");

struct x58_ahci_global_snapshot {
	uint32_t cap;
	uint32_t ghc;
	uint32_t pi;
	uint32_t vs;
};

static struct x58_ahci_global_snapshot x58_ahci_read_global(uintptr_t abar)
{
	return (struct x58_ahci_global_snapshot) {
		.cap = read32p(abar + X58_AHCI_CAP),
		.ghc = read32p(abar + X58_AHCI_GHC),
		.pi = read32p(abar + X58_AHCI_PI),
		.vs = read32p(abar + X58_AHCI_VS),
	};
}

static void x58_ahci_log_global(const char *phase, uintptr_t abar,
	const struct x58_ahci_global_snapshot *snapshot)
{
	printk(BIOS_NOTICE,
	       "[AHCI] X58_AHCI %s ABAR=%08lx CAP=%08x GHC=%08x "
	       "AE=%u HR=%u IE=%u PI=%08x VS=%08x\n",
	       phase, (unsigned long)abar, snapshot->cap, snapshot->ghc,
	       !!(snapshot->ghc & X58_AHCI_GHC_AE),
	       !!(snapshot->ghc & BIT(0)), !!(snapshot->ghc & BIT(1)),
	       snapshot->pi, snapshot->vs);
}

static bool x58_ahci_global_reset_is_exact(
	const struct x58_ahci_global_snapshot *snapshot)
{
	return snapshot->cap == X58_AHCI_CAP_TARGET &&
		snapshot->ghc == 0 && snapshot->pi == 0 &&
		snapshot->vs == X58_AHCI_VS_TARGET;
}

static bool x58_ahci_global_ae_only_is_exact(
	const struct x58_ahci_global_snapshot *snapshot)
{
	return snapshot->cap == X58_AHCI_CAP_TARGET &&
		snapshot->ghc == X58_AHCI_GHC_AE && snapshot->pi == 0 &&
		snapshot->vs == X58_AHCI_VS_TARGET;
}

static bool x58_ahci_global_target_is_exact(
	const struct x58_ahci_global_snapshot *snapshot)
{
	return snapshot->cap == X58_AHCI_CAP_TARGET &&
		snapshot->ghc == X58_AHCI_GHC_AE &&
		snapshot->pi == X58_AHCI_PI_TARGET &&
		snapshot->vs == X58_AHCI_VS_TARGET;
}

static bool x58_ahci_config_is_exact(
	const struct x58_sata_config_sata_snapshot *snapshot, struct device *sata,
	const struct resource *abar, uint16_t expected_command)
{
	return x58_sata_handoff_snapshot_is_exact(snapshot, sata, abar,
			expected_command) &&
		(uint8_t)snapshot->f2_pcs ==
			I82801JX_SATA_PCS_ALL_PORTS_ENABLED &&
		(snapshot->f2_pcs & I82801JX_SATA_PCS_RESERVED_14) == 0 &&
		snapshot->f2_sclkcg ==
			I82801JX_SATA_SCLKCG_FIELD1_REQUIRED;
}

static void __maybe_unused x58_ahci_program_mmio_once(void)
{
	struct device *sata = x58_sata_resource_sata_device();
	const struct resource *abar = sata != NULL ?
		probe_resource(sata, I82801JX_SATA_ABAR) : NULL;
	struct x58_sata_config_sata_snapshot config = x58_sata_config_read_sata_snapshot();
	struct x58_ahci_global_snapshot global;
	uintptr_t base;
	bool retained;

	if (x58_ahci_mmio_attempted)
		die_with_post_code(POST_X58_AHCI_FAIL,
			"[AHCI] X58_AHCI refused a second MMIO attempt\n");
	x58_ahci_mmio_attempted = true;
	x58_sata_handoff_log_state("X58_AHCI_MEM_PRE", &config, sata, abar);
	if (!x58_sata_handoff_pcs_attempted || !x58_sata_handoff_sclk_attempted ||
	    !x58_ahci_config_is_exact(&config, sata, abar, 0))
		die_with_post_code(POST_X58_AHCI_FAIL,
			"[AHCI] X58_AHCI rejected X58_SATA_HANDOFF/resource/MEM prestate\n");
	base = (uintptr_t)abar->base;

	post_code(POST_X58_AHCI_MEM_BEGIN);
	printk(BIOS_NOTICE,
	       "[AHCI] X58_AHCI MEM_STAGE begin: enable only PCI_COMMAND.MEM "
	       "for allocator-selected ABAR=%08lx\n", (unsigned long)base);
	pci_io_write_config16(X58_SATA_MAP_SATA1_DEV, PCI_COMMAND,
		PCI_COMMAND_MEMORY);
	config = x58_sata_config_read_sata_snapshot();
	x58_sata_handoff_log_state("X58_AHCI_MEM_POST", &config, sata, abar);
	if (!x58_ahci_config_is_exact(&config, sata, abar,
			PCI_COMMAND_MEMORY))
		die_with_post_code(POST_X58_AHCI_FAIL,
			"[AHCI] X58_AHCI exact MEM-only decode readback failed\n");
	post_code(POST_X58_AHCI_MEM_OK);

	/* ABAR is accessed only after its resource and MEM-only decode pass. */
	global = x58_ahci_read_global(base);
	x58_ahci_log_global("GLOBAL_PRE", base, &global);
	if (!x58_ahci_global_reset_is_exact(&global) &&
	    !x58_ahci_global_target_is_exact(&global))
		die_with_post_code(POST_X58_AHCI_FAIL,
			"[AHCI] X58_AHCI rejected partial/unexpected CAP/GHC/PI/VS tuple\n");
	retained = x58_ahci_global_target_is_exact(&global);

	post_code(POST_X58_AHCI_AE_BEGIN);
	printk(BIOS_NOTICE,
	       "[AHCI] X58_AHCI AE_STAGE begin: set only GHC.AE\n");
	if (!retained)
		write32p(base + X58_AHCI_GHC, X58_AHCI_GHC_AE);
	global = x58_ahci_read_global(base);
	x58_ahci_log_global("AE_POST", base, &global);
	config = x58_sata_config_read_sata_snapshot();
	if (!x58_ahci_config_is_exact(&config, sata, abar,
			PCI_COMMAND_MEMORY) ||
	    (!retained && !x58_ahci_global_ae_only_is_exact(&global)) ||
	    (retained && !x58_ahci_global_target_is_exact(&global)))
		die_with_post_code(POST_X58_AHCI_FAIL,
			"[AHCI] X58_AHCI GHC.AE full-state readback failed\n");
	post_code(POST_X58_AHCI_AE_OK);

	post_code(POST_X58_AHCI_PI_BEGIN);
	printk(BIOS_NOTICE,
	       "[AHCI] X58_AHCI PI_STAGE begin: write only PI low byte=3f\n");
	if (!retained)
		write8p(base + X58_AHCI_PI,
			(uint8_t)X58_AHCI_PI_TARGET);
	global = x58_ahci_read_global(base);
	x58_ahci_log_global("PI_POST", base, &global);
	config = x58_sata_config_read_sata_snapshot();
	if (!x58_ahci_config_is_exact(&config, sata, abar,
			PCI_COMMAND_MEMORY) ||
	    !x58_ahci_global_target_is_exact(&global))
		die_with_post_code(POST_X58_AHCI_FAIL,
			"[AHCI] X58_AHCI PI/global/configuration readback failed\n");
	post_code(POST_X58_AHCI_PI_OK);
	printk(BIOS_NOTICE,
	       "[AHCI] X58_AHCI AE=1 PI=0000003f PASS (retained=%u); "
	       "BME/HR/IE/CAP/VS/CLB/FB/PxCMD/COMRESET/IDENTIFY untouched\n",
	       (unsigned int)retained);
}

static void __maybe_unused x58_ahci_verify_final_once(void)
{
	struct device *sata = x58_sata_resource_sata_device();
	const struct resource *abar = sata != NULL ?
		probe_resource(sata, I82801JX_SATA_ABAR) : NULL;
	struct x58_sata_config_sata_snapshot config = x58_sata_config_read_sata_snapshot();
	struct x58_ahci_global_snapshot global;

	if (!x58_ahci_mmio_attempted || x58_ahci_final_verified || abar == NULL)
		die_with_post_code(POST_X58_AHCI_FAIL,
			"[AHCI] X58_AHCI final verification order/ABAR failure\n");
	global = x58_ahci_read_global((uintptr_t)abar->base);
	x58_ahci_log_global("ENABLE_POST", (uintptr_t)abar->base, &global);
	if (!x58_ahci_config_is_exact(&config, sata, abar,
			PCI_COMMAND_MEMORY) ||
	    !x58_ahci_global_target_is_exact(&global))
		die_with_post_code(POST_X58_AHCI_FAIL,
			"[AHCI] X58_AHCI post-enable target did not remain exact\n");
	x58_ahci_final_verified = true;
	post_code(POST_X58_AHCI_READY);
	printk(BIOS_NOTICE,
	       "[AHCI] X58_AHCI READY; dynamic ABAR decoded MEM-only, AE=1, PI=3f; "
	       "SeaBIOS owns HBA reset, BME and every port operation\n");
}

static size_t x58_pci_collect_and_audit_resources(
	struct x58_pci_leaf_resource leaves[X58_PCI_MAX_LEAF_RESOURCES])
{
	size_t leaf_count = 0;

	for (size_t i = 0; i < ARRAY_SIZE(x58_pci_expected); i++) {
		const struct x58_pci_expected_pci *expected = &x58_pci_expected[i];
		struct device *dev = x58_pci_device_for_expected(expected);
		const struct resource *resource;
		size_t usable = 0;

		if (x58_pci_function_unexposed(expected))
			continue;
		if (dev == NULL)
			die_with_post_code(POST_X58_PCI_RESOURCE_FAIL,
				"[RAMSTAGE] X58_PCI resource owner missing\n");
		if (x58_pci_optional_device_absent(expected, dev))
			continue;
		for (resource = dev->resource_list; resource;
		     resource = resource->next) {
			const unsigned long type = resource->flags &
				(IORESOURCE_IO | IORESOURCE_MEM);

			if (!resource->size || !type)
				continue;
			usable++;
			x58_pci_require_allocated_resource(dev, resource);
			if (!(resource->flags & IORESOURCE_BRIDGE)) {
				if (leaf_count >= X58_PCI_MAX_LEAF_RESOURCES)
					die_with_post_code(POST_X58_PCI_RESOURCE_FAIL,
						"[RAMSTAGE] X58_PCI resource audit overflow\n");
				leaves[leaf_count++] = (struct x58_pci_leaf_resource) {
					.dev = dev,
					.resource = resource,
				};
			}
		}
		if (!expected->bridge && usable == 0)
			die_with_post_code(POST_X58_PCI_RESOURCE_FAIL,
				"[RAMSTAGE] X58_PCI endpoint has no usable BAR: %s\n",
				expected->name);
	}

	for (size_t i = 0; i < x58_pci_dynamic_count; i++) {
		struct device *dev = x58_pci_dynamic[i].dev;
		x58_pci_dynamic_require(dev);
		for (const struct resource *resource = dev->resource_list; resource;
		     resource = resource->next) {
			if (!resource->size || !(resource->flags & (IORESOURCE_IO | IORESOURCE_MEM)))
				continue;
			x58_pci_require_allocated_resource(dev, resource);
			if ((resource->flags & IORESOURCE_BRIDGE) || leaf_count >= X58_PCI_MAX_LEAF_RESOURCES)
				die("[PCIE_ENUM] dynamic resource overflow or unexpected bridge\n");
			leaves[leaf_count++] = (struct x58_pci_leaf_resource) { .dev = dev, .resource = resource };
		}
	}
	return leaf_count;
}

static void x58_pci_require_no_leaf_overlap(
	const struct x58_pci_leaf_resource leaves[X58_PCI_MAX_LEAF_RESOURCES],
	size_t count)
{
	for (size_t i = 0; i < count; i++) {
		const struct resource *left = leaves[i].resource;
		const unsigned long left_type = left->flags &
			(IORESOURCE_IO | IORESOURCE_MEM);
		const uint64_t left_top = left->base + left->size;

		for (size_t j = i + 1; j < count; j++) {
			const struct resource *right = leaves[j].resource;
			const unsigned long right_type = right->flags &
				(IORESOURCE_IO | IORESOURCE_MEM);
			const uint64_t right_top = right->base + right->size;

			if (left_type == right_type && left->base < right_top &&
			    right->base < left_top)
				die_with_post_code(POST_X58_PCI_OVERLAP_FAIL,
					"[RAMSTAGE] X58_PCI overlapping leaf resources: %s / %s\n",
					dev_path(leaves[i].dev), dev_path(leaves[j].dev));
		}
	}
}

static void x58_platform_resource_require_no_fixed_leaf_overlap(
	const struct x58_pci_leaf_resource leaves[X58_PCI_MAX_LEAF_RESOURCES],
	size_t count)
{
	for (size_t i = X58_PLATFORM_RESOURCE_FIRST_FIXED_RESOURCE_INDEX;
	     i < ARRAY_SIZE(x58_platform_resource_domain_resource_contracts); i++) {
		const struct x58_platform_resource_domain_resource_contract *fixed =
			&x58_platform_resource_domain_resource_contracts[i];
		const unsigned long fixed_type = fixed->flags & IORESOURCE_TYPE_MASK;

		for (size_t j = 0; j < count; j++) {
			const struct resource *leaf = leaves[j].resource;
			const unsigned long leaf_type = leaf->flags &
				IORESOURCE_TYPE_MASK;
			const uint64_t leaf_top = leaf->base + leaf->size;

			if (fixed_type == leaf_type && fixed->base < leaf_top &&
			    leaf->base < fixed->top)
				die_with_post_code(POST_X58_PCI_OVERLAP_FAIL,
					"[RESOURCE] X58_PLATFORM_RESOURCE fixed resource %lx overlaps %s\n",
					fixed->index, dev_path(leaves[j].dev));
		}
	}
}

static void x58_hpet_require_no_hpet_leaf_overlap(
	const struct x58_pci_leaf_resource leaves[X58_PCI_MAX_LEAF_RESOURCES],
	size_t count)
{
	for (size_t i = 0; i < count; i++) {
		const struct resource *leaf = leaves[i].resource;
		const uint64_t leaf_top = leaf->base + leaf->size;

		if ((leaf->flags & IORESOURCE_TYPE_MASK) == IORESOURCE_MEM &&
		    x58_hpet_resource_contract.base < leaf_top &&
		    leaf->base < x58_hpet_resource_contract.top)
			die_with_post_code(POST_X58_PCI_OVERLAP_FAIL,
				"[RESOURCE] X58_HPET HPET overlaps %s\n",
				dev_path(leaves[i].dev));
	}
}

static void __maybe_unused x58_pci_resources_assigned(void *unused)
{
	struct x58_pci_leaf_resource leaves[X58_PCI_MAX_LEAF_RESOURCES];
	size_t leaf_count;

	(void)unused;
	x58_pci_require_platform_state("post-allocation");
	x58_pci_require_enumerated_topology();
	x58_pci_require_domain_resource(0, 0, 0x000a0000,
		IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_CACHEABLE);
	x58_pci_require_domain_resource(1, 0x000a0000, 0x000c0000,
		IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_RESERVE);
	x58_pci_require_domain_resource(2, 0x000c0000, 0x00100000,
		IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_RESERVE);
	x58_pci_require_domain_resource(3, X58_PCI_LOW_RAM_BASE, X58_PCI_LOW_RAM_TOP,
		IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_CACHEABLE);
	x58_pci_require_domain_resource(4, X58_PCI_HIGH_RAM_BASE, X58_PCI_HIGH_RAM_TOP,
		IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_CACHEABLE);
	x58_pci_require_domain_resource(5, X58_PCI_IO_BASE, X58_PCI_IO_TOP,
		IORESOURCE_IO | IORESOURCE_BRIDGE);
	x58_pci_require_domain_resource(6, X58_PCI_MMIO_BASE,
		X58_PCI_MMIO_TOP, IORESOURCE_MEM | IORESOURCE_BRIDGE);
	x58_pci_require_domain_resource(7, X58_PCI_ECAM_BASE, X58_PCI_ECAM_TOP,
		IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_RESERVE);
	x58_platform_resource_require_exact_domain_resources();
	x58_hpet_require_exact_hpet_resource();

	leaf_count = x58_pci_collect_and_audit_resources(leaves);
	x58_pci_require_no_leaf_overlap(leaves, leaf_count);
	x58_platform_resource_require_no_fixed_leaf_overlap(leaves, leaf_count);
	printk(BIOS_NOTICE,
	       "[RESOURCE] X58_PLATFORM_RESOURCE exact resources 0..14 and non-overlap audit PASS\n");
	x58_hpet_require_no_hpet_leaf_overlap(leaves, leaf_count);
	printk(BIOS_NOTICE,
	       "[RESOURCE] X58_HPET exact resources 0..15 and non-overlap audit PASS\n");
	post_code(POST_X58_PCI_ALLOC_OK);
	printk(BIOS_NOTICE,
	       "[RAMSTAGE] X58_PCI PCI allocation audit PASS; endpoint BME remains off\n");
	x58_legacy_input_correct_sata_policy_once();
}

static void __maybe_unused x58_pci_resources_enabled(void *unused)
{
	size_t forwarding_bridges = 0;

	(void)unused;
	x58_pci_require_platform_state("post-enable");
	x58_pci_jmb_verify_mode();
	for (size_t i = 0; i < ARRAY_SIZE(x58_pci_expected); i++) {
		const struct x58_pci_expected_pci *expected = &x58_pci_expected[i];
		struct device *dev = x58_pci_device_for_expected(expected);
		uint16_t command;
		uint16_t expected_master = 0;

		if (x58_pci_function_unexposed(expected))
			continue;
		if (x58_pci_optional_device_absent(expected, dev))
			continue;
		if (dev == NULL)
			die_with_post_code(POST_X58_PCI_ENABLE_FAIL,
				"[RAMSTAGE] X58_PCI enable owner missing\n");
		if (expected->bridge && dev->downstream != NULL &&
		    dev->downstream->children != NULL &&
		    dev->downstream->children->enabled) {
			expected_master = PCI_COMMAND_MASTER;
			forwarding_bridges++;
		}
		command = pci_read_config16(dev, PCI_COMMAND);

		if ((command & PCI_COMMAND_MASTER) != expected_master ||
		    (dev->command & PCI_COMMAND_MASTER) != expected_master ||
		    (command & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY)) !=
		    (dev->command & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY)))
			die_with_post_code(POST_X58_PCI_ENABLE_FAIL,
				"[RAMSTAGE] X58_PCI decode/master enable audit failed\n");
	}
	for (size_t i = 0; i < x58_pci_dynamic_count; i++) {
		struct device *dev = x58_pci_dynamic[i].dev;
		x58_pci_dynamic_require(dev);
		const uint16_t mask = PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
		if ((dev->command & PCI_COMMAND_MASTER) ||
		    (pci_read_config16(dev, PCI_COMMAND) & mask) != (dev->command & mask))
			die("[PCIE_ENUM] dynamic endpoint decode/BME audit failed\n");
	}
	printk(BIOS_NOTICE, "[PCIE_ENUM] ADDIN_READY ROOT_MASK=0d FUNCTIONS=%zu BME=OS SWITCHES=0 HOTPLUG=0\n",
		x58_pci_dynamic_count);
	x58_pci_dynamic_verify_isolation();
	printk(BIOS_NOTICE, "[PCIE_ISOLATION] ADDIN_POLICY_READY SKIPPED_ROOT_MASK=%02x UNKNOWN_IDS=ALLOW UNSUPPORTED=ISOLATE\n",
		x58_pci_dynamic_quarantined);
	x58_usb_log_usb_runtime_once();
	x58_legacy_input_prepare_input();
	x58_platform_legacy_admit_ps2();
	if (!x58_pci_standard_lpc_ready)
		die_with_post_code(POST_X58_ACPI_PIC_FAIL,
			"[PIC] X58_ACPI setup did not reach READY before USB admission\n");
	x58_acpi_usb_admit();
	post_code(POST_X58_PCI_ENABLE_OK);
	printk(BIOS_NOTICE,
	       "[RAMSTAGE] X58_PCI decodes enabled; BME only on %zu forwarding bridges\n",
	       forwarding_bridges);
}

struct x58_interrupt_mode_lapic_snapshot {
	msr_t apic_base;
	uint32_t tpr;
	uint32_t svr;
	uint32_t lvt0;
	uint32_t lvt1;
};

_Static_assert(CONFIG(XAPIC_ONLY), "X58_INTERRUPT_MODE requires fixed xAPIC mode");
_Static_assert(!CONFIG(NO_PCAT_8259),
	"X58_INTERRUPT_MODE requires the legacy 8259 PIC path");
_Static_assert(CONFIG(SEABIOS_HARDWARE_IRQ),
	"X58_INTERRUPT_MODE must retain SeaBIOS hardware interrupts");
/* ICH10_USB's builder validates an externally built prompted diagnostic iPXE ROM;
 * it must not silently rebuild the historical payload under the same name.
 */
_Static_assert((CONFIG(BUILD_IPXE) ||
	CONFIG(PXE_ROM) ||
	!CONFIG(PXE)) &&
	!CONFIG(IPXE_NO_PROMPT),
	"X58_INTERRUPT_MODE needs prompted iPXE or an explicit no-network build");

static bool x58_interrupt_mode_apic_base_is_expected(msr_t apic_base)
{
	return apic_base.hi == 0 &&
		(apic_base.lo & LAPIC_BASE_MSR_ADDR_MASK) == LAPIC_DEFAULT_BASE &&
		(apic_base.lo & LAPIC_BASE_MSR_ENABLE) != 0 &&
		(apic_base.lo & LAPIC_BASE_MSR_X2APIC_MODE) == 0 &&
		(apic_base.lo & LAPIC_BASE_MSR_BOOTSTRAP_PROCESSOR) != 0;
}

static struct x58_interrupt_mode_lapic_snapshot x58_interrupt_mode_read_lapic_snapshot(msr_t apic_base)
{
	return (struct x58_interrupt_mode_lapic_snapshot) {
		.apic_base = apic_base,
		.tpr = lapic_read(LAPIC_TASKPRI),
		.svr = lapic_read(LAPIC_SPIV),
		.lvt0 = lapic_read(LAPIC_LVT0),
		.lvt1 = lapic_read(LAPIC_LVT1),
	};
}

static void x58_interrupt_mode_log_apic_base(const char *phase, msr_t apic_base)
{
	printk(BIOS_NOTICE, "[LAPIC] X58_INTERRUPT_MODE %s IA32_APIC_BASE=%08x:%08x\n",
	       phase, apic_base.hi, apic_base.lo);
}

static void x58_interrupt_mode_log_lapic_snapshot(const char *phase,
	const struct x58_interrupt_mode_lapic_snapshot *snapshot)
{
	printk(BIOS_NOTICE,
	       "[LAPIC] X58_INTERRUPT_MODE %s TPR=%08x SVR=%08x LVT0=%08x LVT1=%08x\n",
	       phase,
	       snapshot->tpr, snapshot->svr, snapshot->lvt0, snapshot->lvt1);
}

static bool x58_interrupt_mode_lapic_state_is_expected(
	const struct x58_interrupt_mode_lapic_snapshot *snapshot)
{
	const uint32_t forbidden_lvt_bits = LAPIC_LVT_MASKED |
		LAPIC_LVT_LEVEL_TRIGGER | LAPIC_INPUT_POLARITY;

	return x58_interrupt_mode_apic_base_is_expected(snapshot->apic_base) &&
		(snapshot->tpr & LAPIC_TPRI_MASK) == 0 &&
		(snapshot->svr & (LAPIC_SPIV_ENABLE | LAPIC_VECTOR_MASK)) ==
			(LAPIC_SPIV_ENABLE | 0x0f) &&
		(snapshot->lvt0 & forbidden_lvt_bits) == 0 &&
		(snapshot->lvt0 & LAPIC_DELIVERY_MODE_MASK) ==
			LAPIC_DELIVERY_MODE_EXTINT &&
		(snapshot->lvt1 & forbidden_lvt_bits) == 0 &&
		(snapshot->lvt1 & LAPIC_DELIVERY_MODE_MASK) ==
			LAPIC_DELIVERY_MODE_NMI;
}

static void x58_interrupt_mode_cpu_cluster_init(struct device *dev)
{
	struct x58_interrupt_mode_lapic_snapshot before;
	struct x58_interrupt_mode_lapic_snapshot after;
	msr_t apic_base;

	(void)dev;
	apic_base = rdmsr(LAPIC_BASE_MSR);
	x58_interrupt_mode_log_apic_base("PRE", apic_base);
	if (!x58_interrupt_mode_apic_base_is_expected(apic_base))
		die_with_post_code(POST_X58_INTERRUPT_MODE_LAPIC_FAIL,
			"[LAPIC] X58_INTERRUPT_MODE PRE IA32_APIC_BASE=%08x:%08x is not BSP xAPIC\n",
			apic_base.hi, apic_base.lo);
	before = x58_interrupt_mode_read_lapic_snapshot(apic_base);
	x58_interrupt_mode_log_lapic_snapshot("PRE", &before);
	if (x58_interrupt_mode_lapic_setup_attempted || !boot_cpu())
		die_with_post_code(POST_X58_INTERRUPT_MODE_LAPIC_FAIL,
			"[LAPIC] X58_INTERRUPT_MODE refused a non-BSP or repeated setup\n");

	x58_interrupt_mode_lapic_setup_attempted = true;
	post_code(POST_X58_INTERRUPT_MODE_LAPIC_BEGIN);
	setup_lapic_interrupts();
	apic_base = rdmsr(LAPIC_BASE_MSR);
	x58_interrupt_mode_log_apic_base("POST", apic_base);
	if (!x58_interrupt_mode_apic_base_is_expected(apic_base))
		die_with_post_code(POST_X58_INTERRUPT_MODE_LAPIC_FAIL,
			"[LAPIC] X58_INTERRUPT_MODE POST IA32_APIC_BASE=%08x:%08x changed\n",
			apic_base.hi, apic_base.lo);
	after = x58_interrupt_mode_read_lapic_snapshot(apic_base);
	x58_interrupt_mode_log_lapic_snapshot("POST", &after);
	if (!x58_interrupt_mode_lapic_state_is_expected(&after))
		die_with_post_code(POST_X58_INTERRUPT_MODE_LAPIC_FAIL,
			"[LAPIC] X58_INTERRUPT_MODE virtual-wire readback gate failed\n");

	post_code(POST_X58_INTERRUPT_MODE_LAPIC_OK);
	printk(BIOS_NOTICE,
	       "[LAPIC] X58_INTERRUPT_MODE BSP virtual-wire ExtINT/NMI gate PASS\n");
	/* An empty static CPU_CLUSTER has downstream=NULL. Match the normal
	 * mp_cpu_bus_init() contract before the driver allocates LAPIC devices.
	 * Keep the driver's empty-bus/one-shot guards; do not bypass them.
	 */
	model_206cx_init(alloc_bus(dev), x58_platform_smp_requested(),
				     x58_platform_msr2e2_requested());
}

/*
 * ICH10_USB admits the standard firmware USB path for all eight real PCI nodes.
 * ICH10 319973-003 USB PCI Command and EHCI Access Control describe the new
 * BME and protected subsystem writes. Existing EHCIIR2/resource/GPIO57/OC
 * owners are retained; no operational HC, legacy-SMI or port write is added.
 * Recovery for any failed one-shot admission is a fresh cold start with the
 * preserved known-good image, never an automatic retry or a guessed reset.
 */
#define X58_PLATFORM_USB_ALL 0xffu
#define X58_PLATFORM_USB_EHCI_ACCESS 0x80u
static uint8_t x58_pci_usb_subsystem_attempted;
static uint8_t x58_pci_usb_subsystem_ready;
static uint8_t x58_pci_usb_init_attempted;
static uint8_t x58_pci_usb_init_ready;

static unsigned int x58_pci_standard_usb_index(struct device *dev)
{
	x58_pci_require_identity(dev);
	const unsigned int slot = PCI_SLOT(dev->path.pci.devfn);
	const unsigned int function = PCI_FUNC(dev->path.pci.devfn);
	if (dev->upstream != x58_pci_domain->downstream ||
	    !dev->enabled || !dev->mandatory || !dev->on_mainboard || dev->downstream ||
	    (slot != 0x1a && slot != 0x1d) || (function > 2 && function != 7) ||
	    dev->ops != (function == 7 ? &x58_pci_standard_ehci_ops :
			&x58_pci_standard_uhci_ops))
		die_with_post_code(POST_X58_USB_EHCI_FAIL, "[ICH10_USB] USB owner mismatch\n");
	return (slot == 0x1d ? 4 : 0) + (function == 7 ? 3 : function);
}

static void x58_pci_standard_usb_resource(struct device *dev, bool ehci)
{
	const unsigned int bar = ehci ? PCI_BASE_ADDRESS_0 : PCI_BASE_ADDRESS_4;
	const struct resource *res = probe_resource(dev, bar);
	const unsigned int size = ehci ? X58_USB_EHCI_BAR_SIZE : X58_USB_UHCI_BAR_SIZE;
	const resource_t bottom = ehci ? X58_PCI_MMIO_BASE : X58_PCI_IO_BASE;
	const resource_t top = ehci ? X58_PCI_MMIO_TOP : X58_PCI_IO_TOP;
	if (res == NULL || res->size != size ||
	    (res->flags & IORESOURCE_TYPE_MASK) != (ehci ? IORESOURCE_MEM : IORESOURCE_IO) ||
	    (res->flags & (IORESOURCE_ASSIGNED | IORESOURCE_STORED)) !=
		(IORESOURCE_ASSIGNED | IORESOURCE_STORED) ||
	    res->base < bottom || res->base > top - size || (res->base & (size - 1)) ||
	    pci_read_config32(dev, bar) !=
		(res->base | (ehci ? 0 : PCI_BASE_ADDRESS_SPACE_IO)))
		die_with_post_code(POST_X58_USB_EHCI_FAIL,
			"[ICH10_USB] USB BAR assignment mismatch; cold recovery required\n");
}

static void x58_pci_standard_usb_subsystem(struct device *dev,
	unsigned int vendor, unsigned int device)
{
	const unsigned int index = x58_pci_standard_usb_index(dev);
	const unsigned int bit = BIT(index);
	const bool ehci = (index % 4) == 3;
	const uint32_t id = pci_read_config32(dev, PCI_VENDOR_ID);
	const uint32_t before = pci_read_config32(dev, PCI_SUBSYSTEM_VENDOR_ID);
	const uint16_t decode = ehci ? PCI_COMMAND_MEMORY : PCI_COMMAND_IO;

	/* Explicit standard fallback: use this function's Intel VID/DID, not an
	 * invented MSI subsystem. UHCI16.1.12/13 requires a fresh R/WO zero pair;
	 * EHCI17.1.11/12 has NO reset and an unspecified retained subsystem value.
	 * That data is explicitly admitted, logged and replaced through protection.
	 * Subsystem configuration happens during enable_resources, before decode.
	 */
	if (x58_pci_usb_subsystem_attempted & bit)
		die_with_post_code(POST_X58_USB_EHCI_FAIL, "[ICH10_USB] repeated USB subsystem write\n");
	x58_pci_usb_subsystem_attempted |= bit;
	x58_pci_standard_usb_resource(dev, ehci);
	printk(BIOS_NOTICE, "[ICH10_USB] USB_SUBSYSTEM_PRE %s OLD=%08x TARGET=%08x\n",
	       dev_path(dev), before, id);
	if (vendor != (id & 0xffff) || device != (id >> 16) ||
	    (!ehci && before != 0) || dev->command != decode ||
	    pci_read_config16(dev, PCI_COMMAND) != 0 ||
	    (ehci && pci_read_config8(dev, X58_PLATFORM_USB_EHCI_ACCESS) != 0))
		die_with_post_code(POST_X58_USB_EHCI_FAIL,
			"[ICH10_USB] USB subsystem prestate rejected; cold recovery required\n");
	if (ehci)
		i82801jx_usb_ehci_set_subsystem(dev, vendor, device);
	else
		i82801jx_usb_uhci_set_subsystem(dev, vendor, device);
	if (pci_read_config32(dev, PCI_SUBSYSTEM_VENDOR_ID) != id ||
	    pci_read_config16(dev, PCI_COMMAND) != 0 ||
	    (ehci && pci_read_config8(dev, X58_PLATFORM_USB_EHCI_ACCESS) != 0))
		die_with_post_code(POST_X58_USB_EHCI_FAIL,
			"[ICH10_USB] USB subsystem/protection readback mismatch\n");
	x58_pci_usb_subsystem_ready |= bit;
}

static void x58_pci_standard_usb_check(struct device *dev, bool initialized)
{
	const unsigned int index = x58_pci_standard_usb_index(dev);
	const bool ehci = (index % 4) == 3;
	const uint16_t command = (ehci ? PCI_COMMAND_MEMORY : PCI_COMMAND_IO) |
		(initialized ? PCI_COMMAND_MASTER : 0);
	x58_pci_standard_usb_resource(dev, ehci);
	if (!(x58_pci_usb_subsystem_ready & BIT(index)) || dev->command != command ||
	    pci_read_config32(dev, PCI_SUBSYSTEM_VENDOR_ID) !=
		pci_read_config32(dev, PCI_VENDOR_ID) ||
	    (ehci && pci_read_config8(dev, X58_PLATFORM_USB_EHCI_ACCESS) != 0) ||
	    !x58_usb_power_standard_usb_check(PCI_DEV(0, PCI_SLOT(dev->path.pci.devfn),
		PCI_FUNC(dev->path.pci.devfn)), initialized))
		die_with_post_code(POST_X58_USB_EHCI_FAIL,
			"[ICH10_USB] USB idle/command/subsystem gate failed; cold recovery required\n");
}

static void x58_pci_standard_usb_init(struct device *dev)
{
	const unsigned int index = x58_pci_standard_usb_index(dev);
	const unsigned int bit = BIT(index);
	const bool ehci = (index % 4) == 3;
	if ((x58_pci_usb_init_attempted & bit) || !x58_pci_standard_lpc_ready ||
	    !x58_usb_ehci_init_attempted)
		die_with_post_code(POST_X58_USB_EHCI_FAIL,
			"[ICH10_USB] repeated or out-of-order USB initialization\n");
	x58_pci_usb_init_attempted |= bit;
	x58_pci_standard_usb_check(dev, false);
	printk(BIOS_NOTICE, "[ICH10_USB] STANDARD_USB_BEGIN %s TYPE=%s DMA_IDLE=1\n",
	       dev_path(dev), ehci ? "EHCI" : "UHCI");
	dev->command |= PCI_COMMAND_MASTER;
	if (ehci)
		i82801jx_usb_ehci_init(dev);
	else
		i82801jx_usb_uhci_init(dev);
	x58_pci_standard_usb_check(dev, true);
	x58_pci_usb_init_ready |= bit;
	printk(BIOS_NOTICE, "[ICH10_USB] STANDARD_USB_READY %s CMD=%04x MASK=%02x\n",
	       dev_path(dev), dev->command, x58_pci_usb_init_ready);
}

void x58_pci_standard_usb_verify(void)
{
	/* Registered chipset callbacks, not the historical adapter counters.
	 * Check assigned BARs and stopped engines before the board power release.
	 * The generic drivers may leave dev->command unchanged when enabling BME.
	 */
	for (unsigned int index = 0; index < 8; index++) {
		const unsigned int fn = index % 4 == 3 ? 7 : index % 4;
		struct device *dev = pcidev_on_root(index < 4 ? 0x1a : 0x1d, fn);
		if (!dev || !dev->enabled || !dev->initialized || !dev->ops ||
		    !dev->ops->init || dev->vendor != PCI_VID_INTEL)
			die("[ICH10] standard USB owner did not initialize\n");
		x58_pci_standard_usb_resource(dev, fn == 7);
		if (!x58_usb_power_standard_usb_check(PCI_DEV(0, index < 4 ? 0x1a : 0x1d, fn), true))
			die("[ICH10] standard USB resource/idle readback failed\n");
	}
	return;
	if (x58_pci_usb_init_ready != X58_PLATFORM_USB_ALL ||
	    x58_pci_usb_init_attempted != X58_PLATFORM_USB_ALL ||
	    x58_pci_usb_subsystem_ready != X58_PLATFORM_USB_ALL ||
	    x58_pci_domain == NULL || x58_pci_domain->downstream == NULL)
		die_with_post_code(POST_X58_USB_EHCI_FAIL,
			"[ICH10_USB] USB callbacks incomplete; payload blocked\n");
	for (unsigned int index = 0; index < 8; index++) {
		const unsigned int function = index % 4 == 3 ? 7 : index % 4;
		struct device *dev = pcidev_path_behind(x58_pci_domain->downstream,
			PCI_DEVFN(index < 4 ? 0x1a : 0x1d, function));
		if (dev == NULL)
			die_with_post_code(POST_X58_USB_EHCI_FAIL, "[ICH10_USB] USB node disappeared\n");
		x58_pci_standard_usb_check(dev, true);
	}
}

static void __maybe_unused x58_pci_standard_usb_handoff(void *unused)
{
	(void)unused;
	x58_pci_standard_usb_verify();
	printk(BIOS_NOTICE, "[ICH10_USB] STANDARD_USB_ALL_READY EHCI=2 UHCI=6 "
	       "SUBSYSTEM=8 GPIO57_OWNER=X58_USB_POWER HC_OWNER=PAYLOAD FULL_CHIPSET=0\n");
}

/* Standard sata.c policy, ICH10 319973-003 sections 14.1 and 14.4.
 * The early MAP/SAD2 route and generic allocator remain their existing owners.
 * No disk command is issued here. SeaBIOS/OS still own operational port startup.
 */
#define X58_PLATFORM_SATA_COMMAND (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER)
#define X58_PLATFORM_SATA_CAP ((X58_AHCI_CAP_TARGET | 0x0c006080u) & ~0x00020060u)
#define X58_PLATFORM_SATA_PORT_BASE 0x100
#define X58_PLATFORM_SATA_PORT_STRIDE 0x80
#define X58_PLATFORM_SATA_PORT_CMD 0x18
#define X58_PLATFORM_SATA_PORT_IE 0x14
#define X58_PLATFORM_SATA_PORT_SACT 0x34
#define X58_PLATFORM_SATA_PORT_CI 0x38
/* ST/FRE are software enables; CR/FR report running DMA engines. */
#define X58_PLATFORM_SATA_ENGINE_MASK (BIT(0) | BIT(4) | BIT(14) | BIT(15))
/* This fixed desktop path does not describe hotplug/interlock/eSATA
 * connectors. Do not lock a retained contradictory PxCMD policy (14.4.3.7).
 */
#define X58_PLATFORM_SATA_CONNECTOR_MASK (BIT(18) | BIT(19) | BIT(21))

static bool x58_pci_standard_sata_attempted;
static bool x58_pci_standard_sata_ready;

static void x58_pci_standard_sata_ports_idle(uintptr_t base)
{
	for (unsigned int port = 0; port < 6; port++) {
		const uintptr_t p = base + X58_PLATFORM_SATA_PORT_BASE + port * X58_PLATFORM_SATA_PORT_STRIDE;
		const uint32_t command = read32p(p + X58_PLATFORM_SATA_PORT_CMD);
		const uint32_t ie = read32p(p + X58_PLATFORM_SATA_PORT_IE);
		const uint32_t sact = read32p(p + X58_PLATFORM_SATA_PORT_SACT);
		const uint32_t ci = read32p(p + X58_PLATFORM_SATA_PORT_CI);

		printk(BIOS_DEBUG, "[ICH10_SATA] SATA_PORT%u CMD=%08x IE=%08x SACT=%08x CI=%08x\n",
		       port, command, ie, sact, ci);
		if ((command & (X58_PLATFORM_SATA_ENGINE_MASK | X58_PLATFORM_SATA_CONNECTOR_MASK)) ||
		    (command & 0xf0000000u) || ie || sact || ci)
			die_with_post_code(POST_X58_AHCI_FAIL,
				"[ICH10_SATA] SATA port%u not idle; cold recovery required\n", port);
	}
}

static void x58_pci_standard_sata_io_resources(struct device *sata)
{
	/* IOSE may only follow valid I/O BAR assignment (14.1.3); include BMBAR. */
	for (unsigned int bar = PCI_BASE_ADDRESS_0; bar <= PCI_BASE_ADDRESS_4; bar += 4) {
		const struct resource *res = probe_resource(sata, bar);
		const uint32_t actual = pci_read_config32(sata, bar);
		if (res == NULL || (res->flags & IORESOURCE_TYPE_MASK) != IORESOURCE_IO ||
		    (res->flags & (IORESOURCE_ASSIGNED | IORESOURCE_STORED)) !=
			(IORESOURCE_ASSIGNED | IORESOURCE_STORED) ||
		    !res->size || res->base < X58_PCI_IO_BASE ||
		    res->base + res->size > X58_PCI_IO_TOP ||
		    actual != (res->base | PCI_BASE_ADDRESS_SPACE_IO))
			die_with_post_code(POST_X58_AHCI_FAIL,
				"[ICH10_SATA] SATA I/O BAR%x not assigned exactly\n", bar);
	}
}

static void x58_pci_standard_sata_verify(struct device *sata)
{
	const struct resource *abar = probe_resource(sata, I82801JX_SATA_ABAR);
	const struct x58_sata_config_sata_snapshot s = x58_sata_config_read_sata_snapshot();

	/* Do not use old MEM-only predicates or mark old owner flags as passed. */
	x58_pci_require_identity(sata);
	x58_sata_handoff_log_state("STANDARD_POST", &s, sata, abar);
	if (!x58_pci_standard_sata_attempted || !x58_sata_config_fixed_gates_are_exact(&s) ||
	    s.fd != X58_SATA_CONFIG_FD_SATA2_DISABLED || s.f5_id != 0xffffffffu ||
	    s.f2_map != I82801JX_SATA_MAP_AHCI_D31F2_VALUE ||
	    !x58_sata_resource_abar_resource_is_exact(abar, s.f2_bar5) ||
	    s.f2_command != X58_PLATFORM_SATA_COMMAND || sata->command != X58_PLATFORM_SATA_COMMAND ||
	    (s.f2_pcs & ~I82801JX_SATA_PCS_PRESENCE_MASK) !=
		(I82801JX_SATA_PCS_OOB_RETRY_MODE | I82801JX_SATA_PCS_ALL_PORTS_ENABLED) ||
	    s.f2_sclkcg != I82801JX_SATA_SCLKCG_FIELD1_REQUIRED ||
	    pci_read_config16(sata, D31F2_IDE_TIM_PRI) != BIT(15) ||
	    pci_read_config16(sata, D31F2_IDE_TIM_SEC) != BIT(15))
		die_with_post_code(POST_X58_AHCI_FAIL,
			"[ICH10_SATA] standard SATA PCI readback failed; cold recovery required\n");
	x58_pci_standard_sata_io_resources(sata);
	const uintptr_t base = abar->base;
	const struct x58_ahci_global_snapshot g = x58_ahci_read_global(base);
	x58_ahci_log_global("STANDARD_POST", base, &g);
	if (g.cap != X58_PLATFORM_SATA_CAP || g.ghc != X58_AHCI_GHC_AE ||
	    g.pi != X58_AHCI_PI_TARGET || g.vs != X58_AHCI_VS_TARGET ||
	    (read32p(base + 0xa0) & BIT(0)))
		die_with_post_code(POST_X58_AHCI_FAIL,
			"[ICH10_SATA] standard SATA AHCI readback failed; cold recovery required\n");
	x58_pci_standard_sata_ports_idle(base);
}

static void x58_pci_standard_sata_init(struct device *sata)
{
	static const struct southbridge_intel_i82801jx_config config = {
		.sata_port_map = 0x3f,
		.sata_clock_request = false,
	};
	const struct resource *abar = probe_resource(sata, I82801JX_SATA_ABAR);
	const struct x58_sata_config_sata_snapshot before = x58_sata_config_read_sata_snapshot();

	if (x58_pci_standard_sata_attempted)
		die_with_post_code(POST_X58_AHCI_FAIL, "[ICH10_SATA] repeated SATA initialization\n");
	x58_pci_standard_sata_attempted = true;
	x58_pci_require_identity(sata);
	x58_sata_handoff_log_state("STANDARD_PRE", &before, sata, abar);
	if (sata != x58_sata_resource_sata_device() || sata->ops != &x58_pci_standard_sata_ops ||
	    !x58_pci_standard_lpc_ready || !x58_legacy_input_sata_policy_ready ||
	    x58_sata_handoff_pcs_attempted || x58_sata_handoff_sclk_attempted || x58_ahci_mmio_attempted ||
	    !x58_sata_handoff_snapshot_is_exact(&before, sata, abar, PCI_COMMAND_MEMORY) ||
	    before.f2_pcs != 0 || before.f2_sclkcg != 0)
		die_with_post_code(POST_X58_AHCI_FAIL,
			"[ICH10_SATA] standard SATA prestate failed; cold recovery required\n");
	x58_pci_standard_sata_io_resources(sata);
	const uintptr_t base = abar->base;
	/* Establish AHCI access before reading any port register (standard driver
	 * requirement). BME is still off; reject a retained/running HBA.
	 */
	if (read32p(base + X58_AHCI_GHC) != 0)
		die_with_post_code(POST_X58_AHCI_FAIL, "[ICH10_SATA] SATA GHC not cold/quiescent\n");
	write32p(base + X58_AHCI_GHC, X58_AHCI_GHC_AE);
	const struct x58_ahci_global_snapshot pre = x58_ahci_read_global(base);
	if (!x58_ahci_global_ae_only_is_exact(&pre))
		die_with_post_code(POST_X58_AHCI_FAIL, "[ICH10_SATA] SATA AHCI-access prestate failed\n");
	x58_pci_standard_sata_ports_idle(base);
	printk(BIOS_NOTICE,
	       "[ICH10_SATA] STANDARD_SATA_BEGIN ABAR=%08lx PORT_MAP=3f DMA_IDLE=1\n",
	       (unsigned long)base);
	/* Use the actual standard device-init body, not a copied tuning table.
	 * The enumerated device's software decode policy follows its new owner.
	 */
	sata->command = X58_PLATFORM_SATA_COMMAND;
	i82801jx_sata_init_sequence(sata, &config, 0);
	x58_pci_standard_sata_verify(sata);
	x58_pci_standard_sata_ready = true;
	printk(BIOS_NOTICE,
	       "[ICH10_SATA] STANDARD_SATA_READY CMD=0007 CAP=%08x PI=0000003f "
	       "PCS_ORM=1 SCLKCG=00000193 INDEXED_TUNING=1 FULL_CHIPSET=0\n",
	       X58_PLATFORM_SATA_CAP);
}

static void __maybe_unused x58_pci_standard_sata_handoff(void *unused)
{
	(void)unused;
	struct device *sata = x58_sata_resource_sata_device();
	if (!x58_pci_standard_sata_ready || sata == NULL)
		die_with_post_code(POST_X58_AHCI_FAIL, "[ICH10_SATA] standard SATA init did not complete\n");
	x58_pci_standard_sata_verify(sata);
}

static struct device_operations __maybe_unused x58_pci_domain_ops = {
	.acpi_fill_ssdt = x58_platform_fill_ssdt,
	.write_acpi_tables = x58_acpi_platform_write_acpi_tables,
	.read_resources = x58_tco_read_resources,
	.set_resources = pci_domain_set_resources,
	.scan_bus = x58_pci_domain_scan_bus,
};

static struct device_operations x58_pci_cpu_cluster_ops = {
	.read_resources = noop_read_resources,
	.set_resources = noop_set_resources,
	.init = x58_interrupt_mode_cpu_cluster_init,
};

struct device_operations x58_pci_root_port_ops = {
	.read_resources = pci_bus_read_resources,
	.set_resources = pci_dev_set_resources,
	.enable_resources = pci_bus_enable_resources,
	.scan_bus = x58_pci_scan_bridge,
	.enable = x58_pci_probe_gate,
	.init = x58_pci_standard_pcie_init,
};


struct device_operations x58_pci_endpoint_ops = {
	.read_resources = pci_dev_read_resources,
	.set_resources = pci_dev_set_resources,
	.enable_resources = pci_dev_enable_resources,
	.enable = x58_pci_probe_gate,
};

static struct pci_operations x58_pci_standard_usb_pci_ops = {
	.set_subsystem = x58_pci_standard_usb_subsystem,
};

struct device_operations x58_pci_standard_ehci_ops = {
	.read_resources = pci_ehci_read_resources,
	.set_resources = pci_dev_set_resources,
	.enable_resources = pci_dev_enable_resources,
	.enable = x58_pci_probe_gate,
	.init = x58_pci_standard_usb_init,
	.ops_pci = &x58_pci_standard_usb_pci_ops,
};

struct device_operations x58_pci_standard_uhci_ops = {
	.read_resources = pci_dev_read_resources,
	.set_resources = pci_dev_set_resources,
	.enable_resources = pci_dev_enable_resources,
	.enable = x58_pci_probe_gate,
	.init = x58_pci_standard_usb_init,
	.ops_pci = &x58_pci_standard_usb_pci_ops,
};


struct device_operations x58_pci_standard_sata_ops = {
	.read_resources = pci_dev_read_resources,
	.set_resources = pci_dev_set_resources,
	.enable_resources = pci_dev_enable_resources,
	.enable = x58_pci_probe_gate,
	.init = x58_pci_standard_sata_init,
};


/* The board chip owns the single admitted host domain and installs the
 * guarded scanner, resource map and ACPI writers through full_pci.c. */
void x58_pci_admit_domain(struct device *dev)
{
	if (dev->path.type != DEVICE_PATH_DOMAIN)
		die_with_post_code(POST_X58_PCI_TOPOLOGY_FAIL,
			"[RAMSTAGE] X58_PCI expected a PCI domain\n");
	if (x58_pci_domain != NULL && x58_pci_domain != dev)
		die_with_post_code(POST_X58_PCI_TOPOLOGY_FAIL,
			"[RAMSTAGE] X58_PCI found multiple PCI domains\n");
	x58_pci_domain = dev;
}

void x58_pci_enable_dev(struct device *dev)
{
	if (dev->path.type == DEVICE_PATH_DOMAIN) {
		x58_pci_admit_domain(dev);
		x58_full_pci_enable_dev(dev);
	} else if (dev->path.type == DEVICE_PATH_CPU_CLUSTER) {
		dev->ops = &x58_pci_cpu_cluster_ops;
	}
}

/* ICH10: narrow compatibility with the unchanged vendor-assisted early path.
 * No LPC/SATA/USB/PCIe device init is called from these adapters. Those are
 * registered chipset drivers, run exactly once by dev_initialize().
 */
void x58_full_platform_verify(const char *phase)
{
	x58_pci_require_platform_state(phase);
}

void x58_full_ioh_prepare(void)
{
	x58_pcie_topology_program_ioh_bus_number_once();
	x58_pci_start_iou0_once();
}

void mainboard_ich10_pre_init(void)
{
	x58_full_ich10_require_native_acpi();
	x58_full_platform_verify("full-chip-preflight");
	x58_full_pci_preflight();
	struct device *lpc = pcidev_on_root(0x1f, 0);

	if (!lpc)
		die("[ICH10] registered LPC device missing\n");
	if (x58_pci_standard_lpc_attempted)
		die("[ICH10] registered LPC initialization repeated\n");
	/* common/spi.c will configure the controller at BS_DEV_INIT. ICH10
	 * SPIBAR=RCBA+3800, HSFS+04 (319973 SPI register map). Do not ask the
	 * standard driver to clear retained SMM protections or race a live cycle.
	 * This is read-only admission, not a flash unlock or protection change.
	 */
	const uint8_t bios_cntl = pci_read_config8(lpc, BIOS_CNTL);
	const uint16_t hsfs = read16p(CONFIG_FIXED_RCBA_MMIO_BASE + 0x3804);
	if ((bios_cntl & (BIOS_CNTL_BLE | BIOS_CNTL_SMM_BWP)) ||
	    (hsfs & (BIT(15) | BIT(5)))) /* FLOCKDN / SPI cycle in progress */
		die("[ICH10] SPI locked/active before standard driver; no unlock attempted\n");
	printk(BIOS_NOTICE, "[ICH10] SPI_PRE BIOS_CNTL=%02x HSFS=%04x LOCK_POLICY=NONE\n",
		bios_cntl, hsfs);
	x58_tco_halt_tco_once();
	x58_pci_standard_pmir_before = pci_read_config32(lpc, D31F0_PMIR);
	x58_pci_standard_pmcon1_before = pci_read_config16(lpc, D31F0_GEN_PMCON_1);
	/* Match i82801jx_early_init(): Linux and payload CF9 reset requests must
	 * reset the host as well as ICH10.  Without CF9GR, an OS reboot leaves a
	 * mixed X58 state (retained QPI endpoint with cleared IMC registers), which
	 * is neither a reusable warm endpoint nor a valid cold-MINIT entry.  Apply
	 * the chipset's documented must-write fields together with CF9GR before
	 * locking PMIR, and require exact readback before exposing the platform to
	 * a payload.
	 */
	if ((x58_pci_standard_pmir_before & PMIR_CF9LOCK) &&
	    !(x58_pci_standard_pmir_before & PMIR_CF9GR))
		die("[ICH10] PMIR already locked without CF9 global reset\n");
	const uint32_t pmir_target = x58_pci_standard_pmir_before |
		PMIR_CF9LOCK | PMIR_FIELD_2 | PMIR_CF9GR | PMIR_FIELD_0;
	pci_write_config32(lpc, D31F0_PMIR, pmir_target);
	if (pci_read_config32(lpc, D31F0_PMIR) != pmir_target)
		die("[ICH10] PMIR CF9 global-reset readback failed\n");
	x58_pci_standard_cf9_global_reset = true;
	x58_pci_standard_lpc_attempted = true;
	printk(BIOS_NOTICE, "[ICH10] CHIP_PREPARED NO_SMM=1 TCO_HALTED=1 "
		"DEVICE_OWNER=STANDARD PMIR=%08x CF9GR=1 CF9LOCK=1\n",
		pmir_target);
	x58_board_health_report("CHIP_PREPARED");
}

void mainboard_ich10_lpc_acpi_mode(struct device *lpc)
{
	/* Retain the already reviewed SCI transition and board RCBA pin wiring.
	 * Standard LPC owns PIC/PIRQ/IOAPIC/clock/RTC; never run its sequence twice.
	 */
	x58_pci_standard_lpc_acpi_mode(lpc);
	x58_pci_standard_lpc_ready = true;
	x58_irq_route_program_vendor_irq_once();
	x58_platform_legacy_enable_serirq();
	printk(BIOS_NOTICE, "[ICH10] LPC_READY NO_SMM=1 CMOS_PRESERVED=1 BOARD_IRQ_READY=1\n");
	x58_board_health_report("LPC_READY");
}

bool mainboard_ich10_pci_bridge_isolated(const struct device *dev)
{
	return x58_full_pci_root_isolated(dev);
}

void mainboard_ich10_pci_scan_bridge(struct device *dev)
{
	x58_full_pci_scan_bridge(dev);
}

static void x58_full_board_post_device(void *unused)
{
	(void)unused;
	if (!x58_pci_standard_lpc_ready || !x58_acpi_mode_ready())
		die("[ICH10] standard LPC callback incomplete\n");
	x58_pci_standard_usb_verify();
	x58_legacy_input_prepare_input();
	x58_platform_legacy_admit_ps2();
	printk(BIOS_NOTICE, "[ICH10] BOARD_INPUT_READY USB_OWNER=STANDARD GPIO57_OWNER=BOARD\n");
}

BOOT_STATE_INIT_ENTRY(BS_POST_DEVICE, BS_ON_ENTRY, x58_full_board_post_device, NULL);
