/* SPDX-License-Identifier: GPL-2.0-only */

#include "development_id.h"

/*
 * B06VN is intentionally not a generic X58 PCI implementation.  It admits
 * only the exact standard-header functions observed on B06VL-HW-02, validates
 * every identity before BAR sizing, and allocates only from two fixed holes.
 */

#if CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT || \
	CONFIG_X58_PRO_E_B06WB_TCO_HALT
#include <arch/io.h>
#endif
#include <arch/pci_io_cfg.h>
#if CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM
#include <arch/cpu.h>
#endif
#include <bootstate.h>
#include <cbmem.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#if CONFIG_X58_PRO_E_B06VP_LAPIC_EXTINT
#include <cpu/x86/lapic.h>
#include <smp/node.h>
#endif
#include <delay.h>
#include <device/device.h>
#include <device/mmio.h>
#include <device/pci.h>
#include <device/pci_def.h>
#include <device/pci_ids.h>
#include <device/pci_type.h>
#include <device/resource.h>
#if CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM
#include <pc80/i8259.h>
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#if CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT || \
	CONFIG_X58_PRO_E_B06WB_TCO_HALT
#if CONFIG_X58_PRO_E_B06VQ_PLATRO1 || \
	CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM
#include <southbridge/intel/common/pmutil.h>
#endif
#include <southbridge/intel/i82801jx/i82801jx.h>
#endif

#include "b06v6_handoff.h"
#include "b06vn_pci.h"
#if CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM
#include "b06wc_usb_admit.h"
#endif
#if CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT
#include "b06wd_input.h"
#endif
#if CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI
#include "b06wi_irq_route.h"
#endif
#if CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM
#include "b06wj_acpi.h"
#endif

#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
#define B06VN_BUILD_ID X58_DEVELOPMENT_BUILD_ID
#elif CONFIG_X58_PRO_E_B06WK_ACPI_REPAIR
#define B06VN_BUILD_ID B06WK_BUILD_ID
#elif CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM
#define B06VN_BUILD_ID B06WJ_BUILD_ID
#elif CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI
#define B06VN_BUILD_ID B06WI_BUILD_ID
#elif CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS
#define B06VN_BUILD_ID "X58PROE-B06WH-AUTO-USB-SEABIOS-20260907"
#elif CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS
#define B06VN_BUILD_ID "X58PROE-B06WG-AUTO-USB-SEABIOS-20260907"
#elif CONFIG_X58_PRO_E_B06WF_USB_LAB
#define B06VN_BUILD_ID "X58PROE-B06WF-LATE-USB-LAB-20260907"
#elif CONFIG_X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX
#define B06VN_BUILD_ID "X58PROE-B06WE-ACPI-SAD-BDF-FIX-20260907"
#elif CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT
#define B06VN_BUILD_ID "X58PROE-B06WD-SEABIOS-INPUT-20260906"
#elif CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM
#define B06VN_BUILD_ID "X58PROE-B06WC-INTEGRATED-PLATFORM-20260906"
#elif CONFIG_X58_PRO_E_B06WB_TCO_HALT
#define B06VN_BUILD_ID "X58PROE-B06WB-ICH10-TCO-HALT-20260906"
#elif CONFIG_X58_PRO_E_B06WA_HPET_DECODE
#define B06VN_BUILD_ID "X58PROE-B06WA-ICH10-HPET-DECODE-20260906"
#elif CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES
#define B06VN_BUILD_ID "X58PROE-B06VZ-FIXED-RESOURCES-20260906"
#elif CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK
#define B06VN_BUILD_ID "X58PROE-B06VY-ICH10-IOAPIC-MASK-20260906"
#elif CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE
#define B06VN_BUILD_ID "X58PROE-B06VX-AHCI-USBTRACE-20260906"
#elif CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO
#define B06VN_BUILD_ID "X58PROE-B06VW-ICH10-AHCI-MMIO-20260906"
#elif CONFIG_X58_PRO_E_B06VV_ICH10_PCS_SCLK
#define B06VN_BUILD_ID "X58PROE-B06VV-ICH10-PCS-SCLK-20260906"
#elif CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE
#define B06VN_BUILD_ID "X58PROE-B06VU-ICH10-AHCI-ROUTE-20260906"
#elif CONFIG_X58_PRO_E_B06VQ_ICHBASE1
#define B06VN_BUILD_ID "X58PROE-B06VQ-ICHBASE1-20260906"
#elif CONFIG_X58_PRO_E_B06VQ_PLATRO1
#define B06VN_BUILD_ID "X58PROE-B06VQ-PLATRO1-20260906"
#elif CONFIG_X58_PRO_E_B06VQ_USB_TRACE1
#define B06VN_BUILD_ID "X58PROE-B06VQ-USBTRACE1-20260906"
#elif CONFIG_X58_PRO_E_B06VT_ICH10_SATA_CLOCK
#define B06VN_BUILD_ID "X58PROE-B06VT-ICH10-SATA-CLOCK-20260906"
#elif CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS
#define B06VN_BUILD_ID "X58PROE-B06VS-ICH10-AHCI-PORTS-20260906"
#elif CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP
#define B06VN_BUILD_ID "X58PROE-B06VR-ICH10-AHCI-MAP-20260906"
#elif CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT
#define B06VN_BUILD_ID "X58PROE-B06VQ-ICH10-EHCI-INIT-20260906"
#elif CONFIG_X58_PRO_E_B06VP_LAPIC_EXTINT
#define B06VN_BUILD_ID "X58PROE-B06VP-LAPIC-EXTINT-PROBE-20260906"
#elif CONFIG_X58_PRO_E_B06VO_IOHBUSNO_ROUTE
#define B06VN_BUILD_ID "X58PROE-B06VO-IOHBUSNO-ROUTE-PROBE-20260906"
#else
#define B06VN_BUILD_ID "X58PROE-B06VN-IOU0-HD5450-PHYSVBIOS-20260906"
#endif

#define B06VN_SAD_DEV		PCI_DEV(0xff, 0, 1)
#define B06VN_SAD_ID		0x2d818086u
#define B06VN_SAD_PCIEXBAR_LO	0x50
#define B06VN_SAD_PCIEXBAR_HI	0x54
#define B06VN_SAD_RULE_FIRST	0x80
#define B06VN_SAD_RULE_COUNT	8
#define B06VN_PCIEXBAR_LO	0xe0000001u

#define B06VN_X58_HOST_DEV	PCI_DEV(0, 0, 0)
#define B06VN_X58_HOST_ID	0x34058086u
#define B06VN_X58_HOST_CLASSREV	0x06000013u

#define B06VN_IOH_HM_DEV	PCI_DEV(0, 0x14, 0)
#define B06VN_IOH_HM_ID		0x342e8086u
#define B06VN_IOH_TOLM		0xd0
#define B06VN_IOH_TOHM_LO	0xd4
#define B06VN_IOH_TOHM_HI	0xd8
#define B06VN_TOLM_VALUE	0xbc000000u
#define B06VN_TOHM_LO_VALUE	0x3c000000u
#define B06VN_TOHM_HI_VALUE	0x00000001u

#define B06VN_LOW_RAM_BASE	0x00100000ULL
#define B06VN_LOW_RAM_TOP	0xc0000000ULL
#define B06VN_HIGH_RAM_BASE	0x100000000ULL
#define B06VN_HIGH_RAM_TOP	0x140000000ULL
#define B06VN_PCI_IO_BASE	0x1000u
#define B06VN_PCI_IO_TOP	0x10000u
#define B06VN_PCI_MMIO_BASE	0xc0000000ULL
#define B06VN_PCI_MMIO_TOP	0xe0000000ULL
#define B06VN_ECAM_BASE		0xe0000000ULL
#define B06VN_ECAM_TOP		0xf0000000ULL

/* Full PCIEXBAR address construction; extended offsets cannot use legacy CF8. */
#define B06VN_ECAM_ADDR(bus, dev, fn, reg) \
	(B06VN_ECAM_BASE + ((uintptr_t)(bus) << 20) + \
	 ((uintptr_t)(dev) << 15) + ((uintptr_t)(fn) << 12) + (reg))
#define B06VN_ECAM_DEV0(dev, reg) B06VN_ECAM_ADDR(0, dev, 0, reg)
#define B06VN_IOU2_X4_DEV	0x01
#define B06VN_IOU0_X16_DEV	0x03
#define B06VN_AUX_X16_DEV	0x07
#define B06VN_IOU2_X4_ID		0x34088086u
#define B06VN_IOU0_X16_ID	0x340a8086u
#define B06VN_AUX_X16_ID		0x340e8086u
#define B06VN_PCIE_LNKCAP	0x09c
#define B06VN_PCIE_LNKCTLSTA	0x0a0
#define B06VN_PCIE_LNKSTA	0x0a2
#define B06VN_PCIE_AA		0x0aa
#define B06VN_PCIE_C0		0x0c0
#define B06VN_PCIE_PRTX_BIF_CTRL 0x190
#define B06VN_IOU0_X16_START	0x0000000cu
#define B06VN_LNKSTA_SPEED_MASK	0x000f
#define B06VN_LNKSTA_WIDTH_MASK	0x03f0
#define B06VN_LNKSTA_WIDTH_SHIFT 4
#define B06VN_LNKSTA_DLL_ACTIVE	BIT(13)
#define B06VN_LINK_POLL_US	100
#define B06VN_LINK_POLL_COUNT	10000
#define B06VN_AMD_VENDOR_ID	0x1002u

/*
 * Intel X58 Datasheet sections 17.6.5.12 and 17.6.5.21/.22/.31/.32.
 * IOHBUSNO.Valid=1 restricts the IOH-internal device/function numbers to the
 * bus in bits 7:0.  The reference board programs bus 0 as exact value 0x0100.
 * The local/global bus-range registers remain read-only telemetry in B06VO.
 */
#define B06VO_IOHBUSNO		0x10a
#define B06VO_IOHBUSNO_BUS0_VALID 0x0100u
#define B06VO_LCFGBUS_BASE	0x11c
#define B06VO_LCFGBUS_LIMIT	0x11d
#define B06VO_GCFGBUS_BASE	0x134
#define B06VO_GCFGBUS_LIMIT	0x135
#define B06VO_ALIAS_DEV		0x0d
#define B06VO_PEG_ENDPOINT_DEV	0x00
#define B06VO_ENDPOINT_SAMPLE_COUNT 4

#if CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP || \
	CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE
#define B06VN_ROOT_FUNCTIONS	11
#else
#define B06VN_ROOT_FUNCTIONS	12
#endif
#define B06VN_DOWNSTREAM_FUNCTIONS 2
#define B06VN_EXPECTED_FUNCTIONS	(B06VN_ROOT_FUNCTIONS + \
					 B06VN_DOWNSTREAM_FUNCTIONS)
#define B06VN_MAX_LEAF_RESOURCES	96

#define POST_B06VN_PREFLIGHT	0x2b
#define POST_B06VN_ROOTS_SAFE	0x2c
#define POST_B06VN_SCAN_OK	0x2d
#define POST_B06VN_RESOURCES	0x2e
#define POST_B06VN_ALLOC_OK	0x2f
#define POST_B06VN_IOU0_START	0x30
#define POST_B06VN_IOU0_POLLED	0x31
#define POST_B06VN_GPU_PRESENT	0x32
#define POST_B06VN_ENABLE_OK	0x34
#define POST_B06VN_GPU_FALLBACK	0x35
#define POST_B06VN_HANDOFF_FAIL	0x36
#define POST_B06VN_PLATFORM_FAIL 0x37
#define POST_B06VN_TOPOLOGY_FAIL 0x38
#define POST_B06VN_IDENTITY_FAIL 0x39
#define POST_B06VN_COMMAND_FAIL	0x3a
#define POST_B06VN_BUS_FAIL	0x3b
#define POST_B06VN_RESOURCE_FAIL 0x3c
#define POST_B06VN_OVERLAP_FAIL	0x3d
#define POST_B06VN_ENABLE_FAIL	0x3e
#define POST_B06VO_IOHBUSNO_OK	0x33
#define POST_B06VO_IOHBUSNO_FAIL 0x3f
#define POST_B06VP_LAPIC_BEGIN	0x40
#define POST_B06VP_LAPIC_OK	0x42
#define POST_B06VP_LAPIC_FAIL	0x43
#define POST_B06VQ_EHCI_BEGIN	0x44
#define POST_B06VQ_EHCI_OK	0x46
#define POST_B06VQ_EHCI_FAIL	0x47
#define POST_B06VR_AHCI_BEGIN	0x48
#define POST_B06VR_AHCI_OK	0x4a
#define POST_B06VR_AHCI_FAIL	0x4b
#define POST_B06VS_PORTS_BEGIN	0x4c
#define POST_B06VS_PORTS_OK	0x4e
#define POST_B06VS_PORTS_FAIL	0x4f
#define POST_B06VT_CLOCK_BEGIN	0x50
#define POST_B06VT_CLOCK_OK	0x52
#define POST_B06VT_CLOCK_FAIL	0x53
#define POST_B06VQI_BASE_BEGIN	0x54
#define POST_B06VQI_BASE_OK	0x56
#define POST_B06VQI_BASE_FAIL	0x57
#define POST_B06VU_RESET_TOPOLOGY_OK 0x58
#define POST_B06VU_MAP_BEGIN	0x59
#define POST_B06VU_MAP_OK	0x5a
#define POST_B06VU_FD_SAD2_BEGIN 0x5b
#define POST_B06VU_FD_SAD2_OK	0x5c
#define POST_B06VU_ROUTE_FAIL	0x5d
#define POST_B06VV_PCS_BEGIN	0x5e
#define POST_B06VV_PCS_OK	0x5f
#define POST_B06VV_SCLK_BEGIN	0x60
#define POST_B06VV_SCLK_OK	0x61
#define POST_B06VV_READY		0x62
#define POST_B06VV_FAIL		0x63
#define POST_B06VW_MEM_BEGIN	0x68
#define POST_B06VW_MEM_OK	0x69
#define POST_B06VW_AE_BEGIN	0x6a
#define POST_B06VW_AE_OK		0x6b
#define POST_B06VW_PI_BEGIN	0x6c
#define POST_B06VW_PI_OK		0x6d
#define POST_B06VW_READY		0x6e
#define POST_B06VW_FAIL		0x6f
#define POST_B06VY_DECODE_BEGIN	0x70
#define POST_B06VY_DECODE_OK	0x71
#define POST_B06VY_CENSUS_OK	0x72
#define POST_B06VY_READY		0x73
#define POST_B06VY_FAIL		0x74
#define POST_B06WA_HPET_BEGIN	0x75
#define POST_B06WA_HPET_DECODED	0x76
#define POST_B06WA_HPET_READY	0x77
#define POST_B06WA_HPET_FAIL	0x78
#define POST_B06WB_TCO_BEGIN	0x79
#define POST_B06WB_TCO_READY	0x7a
#define POST_B06WB_TCO_FAIL	0x7b
#define POST_B06WC_PIC_BEGIN	0x7c
#define POST_B06WC_PIC_READY	0x7d
#define POST_B06WC_PIC_FAIL	0x7e
#define POST_B06WC_ACPI_BEGIN	0x7f
#define POST_B06WC_ACPI_STATUS	0x80
#define POST_B06WC_ACPI_READY	0x81
#define POST_B06WC_ACPI_FAIL	0x82
#define POST_B06WD_POLICY_BEGIN	0x87
#define POST_B06WD_POLICY_READY	0x88
#define POST_B06WD_POLICY_FAIL	0x89

#if CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT
#define B06VQ_EHCI2_DEV		PCI_DEV(0, 0x1a, 7)
#define B06VQ_EHCI1_DEV		PCI_DEV(0, 0x1d, 7)
#define B06VQ_EHCI_BAR_SIZE	0x400u
#define B06VQ_EHCI_CAPLEN_MIN	0x10u
#define B06VQ_EHCI_CAPLEN_MAX	0x80u
#define B06VQ_EHCI_PORT_MAX	6u
#define B06VQ_EHCI_HCIVERSION	0x0100u
#define B06VQ_EHCI_CAP_HCCPARAMS 0x08u
#define B06VQ_EHCI_OP_USBCMD	0x00u
#define B06VQ_EHCI_OP_USBSTS	0x04u
#define B06VQ_EHCI_OP_USBINTR	0x08u
#define B06VQ_EHCI_OP_CONFIGFLAG 0x40u
#define B06VQ_EHCI_OP_PORTSC	0x44u
#define B06VQ_EHCI_LEGACY_CAP	0x68u
#define B06VQ_EHCI_LEGACY_CTLSTS 0x6cu
#define B06VQ_EHCI_LEGACY_EXT	0x70u
#define B06VQ_EHCI_CFG61		0x61u
#define B06VQ_EHCI_CFG84		0x84u
#define B06VQ_UHCI_BAR_SIZE	0x20u
#define B06VQ_UHCI_USBCMD	0x00u
#define B06VQ_UHCI_USBSTS	0x02u
#define B06VQ_UHCI_USBINTR	0x04u
#define B06VQ_UHCI_PORTSC1	0x10u
#define B06VQ_UHCI_PORTSC2	0x12u
#define B06VQ_UHCI_LEGKEY	0xc0u
#define B06VQ_UHCI_CFG_C8	0xc8u
#define B06VQ_UHCI_CFG_CA	0xcau
#define B06VQ_RCBA_PPO		0x3524u
#define B06VQ_PMBASE_UPRWC	0x3cu
#if CONFIG_X58_PRO_E_B06VQ_USB_TRACE1 || \
	CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE
#define B06VQ_USB_FD_DISABLE_MASK 0x0000bf80u
#define B06VQ_USB_CG_DISABLE	BIT(20)
#define B06VQ_USB_PPO_MASK	0x0fffu
#define B06VQ_USB_MAP_MODE	BIT(0)
#define B06VQ_USB_GPIO1_OC_MASK	0xe0000000u
#define B06VQ_USB_GPIO2_OC_MASK	0x0800ff00u
#define B06VQ_EHCI_SMI_MASK	0xe03fu
#define B06VQ_EHCI_EXT_SMI_MASK	0x3fffu
#endif
#endif

#if CONFIG_X58_PRO_E_B06VQ_ICHBASE1
#define B06VQI_LPC_DEV		PCI_DEV(0, 0x1f, 0)
#define B06VQI_LPC_ID		0x3a168086u
#define B06VQI_RCBA_ENABLED	(CONFIG_FIXED_RCBA_MMIO_BASE | 1)
#define B06VQI_FDSW_UNLOCKED	0x00u
#define B06VQI_CIR8_PRE		0x00000000u
#define B06VQI_FD_PRE		0x00000000u
#define B06VQI_CIR9_PRE		0x00000020u
#define B06VQI_CIR7_PRE		0xb2b477ccu
#define B06VQI_CIR13_PRE		0xb2b477ccu
#define B06VQI_CIR10_PRE		0x0008c008u
#define B06VQI_CIR8_TARGET	0x00000002u
#define B06VQI_FD_TARGET		0x00000001u
#define B06VQI_CIR9_TARGET	0x08000020u
#define B06VQI_CIR7_TARGET	0xb2b577ccu
#define B06VQI_CIR13_TARGET	0xb2b577ccu
#define B06VQI_CIR10_TARGET	0x000bc008u
#endif

#if CONFIG_X58_PRO_E_B06VQ_PLATRO1
#define B06VQP_LPC_DEV		PCI_DEV(0, 0x1f, 0)
#define B06VQP_LPC_ID		0x3a168086u
#define B06VQP_SMBUS_DEV	PCI_DEV(0, 0x1f, 3)
#define B06VQP_SMBUS_ID		0x3a308086u
#define B06VQP_SMBUS_BAR4	0x20u
#define B06VQP_PM_TIMER_MASK	0x00ffffffu
#define B06VQP_PM_TIMER_SAMPLES	4096u
#define B06VQP_HPET_BASE0	0xfed00000u
#define B06VQP_HPET_STRIDE	0x1000u
#define B06VQP_HPET_SIZE		0x0400u
#define B06VQP_HPET_DECODE_ENABLE BIT(7)
#define B06VQP_HPET_ADDRESS_MASK	0x3u
#define B06VQP_HPET_CAP_ID	0x000u
#define B06VQP_HPET_GEN_CFG	0x010u
#define B06VQP_HPET_MAIN_COUNTER 0x0f0u
#define B06VQP_HPET_COUNTER_ENABLE BIT(0)
#define B06VQP_DOMAIN_RESOURCE_COUNT 8u
#define B06VQP_RCBA_SIZE	0x4000u
#define B06VQP_PM_IO_SIZE	0x80u
#define B06VQP_GPIO_IO_SIZE	0x40u
#define B06VQP_SMBUS_IO_BASE	0x0400u
#define B06VQP_SMBUS_IO_SIZE	0x20u
#define B06VQP_LAPIC_BASE	0xfee00000u
#define B06VQP_IOAPIC_BASE	0xfec00000u
#define B06VQP_APIC_SIZE	0x1000u
#define B06VQP_ROM_BASE		0xff000000u
#define B06VQP_ROM_TOP		0x100000000ULL
#endif

#if CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP || \
	CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE
#define B06VR_SATA1_DEV		PCI_DEV(0, 0x1f, 2)
#define B06VR_SATA2_DEV		PCI_DEV(0, 0x1f, 5)
#define B06VR_SATA1_IDE_ID	0x3a208086u
#define B06VR_SATA2_IDE_ID	0x3a268086u
#define B06VR_SATA1_AHCI_ID	0x3a228086u
#define B06VR_IDE_CLASS		0x0101u
#define B06VR_AHCI_CLASS_PI	0x010601u
#endif

#if CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE
#define B06VU_SATA1_IDE_CLASSREV	0x01018a00u
#define B06VU_SATA2_IDE_CLASSREV	0x01018500u
#define B06VU_SATA1_AHCI_CLASSREV	0x01060100u
#define B06VU_RESET_COMMAND		0x0000u
#define B06VU_RESET_HEADER		0x00u
#define B06VU_RESET_BAR5		0x00000001u
#define B06VU_RESET_MAP			0x0000u
#define B06VU_RESET_PCS			0x0000u
#define B06VU_RESET_SCLKCG		0x00000000u
#define B06VU_FD_BASELINE		0x00000001u
#define B06VU_FD_SATA2_DISABLED		0x02000001u
#endif

#if CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS || \
	CONFIG_X58_PRO_E_B06VV_ICH10_PCS_SCLK
#define B06VS_ABAR_SIZE		0x800u
#define B06VS_ABAR_GRANULARITY	11
#define B06VS_PCS_SAMPLE_COUNT	5
#endif

#if CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO
#define B06VW_AHCI_CAP		0x00u
#define B06VW_AHCI_GHC		0x04u
#define B06VW_AHCI_PI		0x0cu
#define B06VW_AHCI_VS		0x10u
#define B06VW_AHCI_CAP_TARGET	0xff22ffc5u
#define B06VW_AHCI_GHC_AE	BIT(31)
#define B06VW_AHCI_PI_TARGET	0x0000003fu
#define B06VW_AHCI_VS_TARGET	0x00010200u
#endif

#if CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK
#define B06VY_LPC_DEV		PCI_DEV(0, 0x1f, 0)
#define B06VY_LPC_ID		0x3a168086u
#define B06VY_RCBA_ENABLED	(CONFIG_FIXED_RCBA_MMIO_BASE | 1)
#define B06VY_OIC_DISABLED	0x00u
#define B06VY_OIC_ENABLED	0x03u
#define B06VY_IOAPIC_BASE	0xfec00000u
#define B06VY_IOREGSEL		0x00u
#define B06VY_IOWIN		0x10u
#define B06VY_IOAPIC_ID_REG	0x00u
#define B06VY_IOAPIC_VERSION_REG 0x01u
#define B06VY_IOAPIC_ID_TARGET	0x00000000u
#define B06VY_IOAPIC_VERSION_TARGET 0x00170020u
#define B06VY_IOAPIC_VERSION	0x20u
#define B06VY_IOAPIC_MAX_REDIR	0x17u
#define B06VY_IOAPIC_REDIR_COUNT 24u
#define B06VY_IOAPIC_REDIR_BASE	0x10u
#define B06VY_IOAPIC_MASK_BIT	BIT(16)
#define B06VY_IOAPIC_LOW_TARGET	0x00010000u
#define B06VY_IOAPIC_HIGH_TARGET 0x00000000u
#endif

#if CONFIG_X58_PRO_E_B06WA_HPET_DECODE
/*
 * ICH10 Datasheet HPTC: bits 1:0 select one of four HPET apertures and bit 7
 * enables decode.  The local B06VP transaction proved selector 0 plus enable
 * at FED00000; no other HPTC contents are admitted here.
 */
#define B06WA_HPTC_ADDRESS_SELECT_MASK GENMASK(1, 0)
#define B06WA_HPTC_DECODE_ENABLE BIT(7)
#define B06WA_HPTC_CONTROL_MASK \
	(B06WA_HPTC_ADDRESS_SELECT_MASK | B06WA_HPTC_DECODE_ENABLE)
#define B06WA_HPTC_DISABLED	0x00000000u
#define B06WA_HPTC_FED00000	B06WA_HPTC_DECODE_ENABLE

/* IA-PC HPET 1.0a register offsets and the exact locally observed identity. */
#define B06WA_HPET_BASE		0xfed00000ULL
#define B06WA_HPET_SIZE		0x00000400ULL
#define B06WA_HPET_CAP_ID_LOW	0x000u
#define B06WA_HPET_CAP_ID_HIGH	0x004u
#define B06WA_HPET_CAP_ID_LOW_TARGET 0x8086a301u
#define B06WA_HPET_CAP_ID_HIGH_TARGET 0x0429b17fu
#define B06WA_HPET_GENERAL_CONFIG_LOW 0x010u
#define B06WA_HPET_GENERAL_CONFIG_HIGH 0x014u
#define B06WA_HPET_GENERAL_INTERRUPT_STATUS_LOW 0x020u
#define B06WA_HPET_GENERAL_INTERRUPT_STATUS_HIGH 0x024u
#define B06WA_HPET_MAIN_COUNTER_LOW 0x0f0u
#define B06WA_HPET_MAIN_COUNTER_HIGH 0x0f4u
#define B06WA_HPET_TIMER0_CONFIG_LOW 0x100u
#define B06WA_HPET_TIMER0_CONFIG_HIGH 0x104u
#define B06WA_HPET_TIMER_INTERRUPT_ENABLE BIT(2)
#define B06WA_HPET_COUNTER_STABILITY_DELAY_US 4096u

#define B06WA_HPET_RESOURCE_INDEX 15u
#define B06WA_DOMAIN_RESOURCE_COUNT 16u
#endif

#if CONFIG_X58_PRO_E_B06WB_TCO_HALT
/*
 * ICH10 Datasheet sections 10.1.75 and 13.9.  These neutral, local names
 * deliberately describe only the fixed decode and TCO fields used by B06WB;
 * the broad historical watchdog helper has additional forbidden side effects.
 */
#define B06WB_PMBASE_REG		0x40u
#define B06WB_ACPI_CNTL_REG	0x44u
#define B06WB_PMBASE_ENABLED	0x00000501u
#define B06WB_ACPI_DECODE_ENABLED 0x80u
#define B06WB_TCO_BASE		0x0560u
#define B06WB_TCO_RLD		0x00u
#define B06WB_TCO1_STS		0x04u
#define B06WB_TCO2_STS		0x06u
#define B06WB_TCO1_CNT		0x08u
#define B06WB_TCO2_CNT		0x0au
#define B06WB_TCO_TMR		0x12u
#define B06WB_TCO1_CNT_HLT	BIT(11)
#define B06WB_TCO1_CNT_LOCK	BIT(12)
#define B06WB_TCO1_CNT_PRE	0x0000u
#define B06WB_TCO1_CNT_TARGET	0x0800u
#endif

#if CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM
/*
 * The complete legacy-PIC tuples admitted by B06WC were both observed on the
 * target: power-on reset and the result of coreboot's standard i8259 setup
 * followed by making only SCI/IRQ9 level triggered.  B06WC always replays the
 * standard initialization so the vector bases are known rather than inferred
 * from the readable masks.
 */
#define B06WC_PIC_MASTER_MASK_RESET	0x00u
#define B06WC_PIC_SLAVE_MASK_RESET	0x00u
#define B06WC_ELCR1_RESET		0x00u
#define B06WC_ELCR2_RESET		0x00u
#define B06WC_PIC_MASTER_MASK_TARGET	0xfbu
#define B06WC_PIC_SLAVE_MASK_TARGET	0xffu
#define B06WC_ELCR1_TARGET		0x00u
#define B06WC_ELCR2_TARGET		BIT(IRQ_9 - 8)

/*
 * B06VP-HW-01 measured this completely quiescent PM/SCI prestate.  B06WC
 * admits either the measured SCI_EN-clear state or its own exact retained
 * target, acknowledges only the unsafe pending power-button-override status,
 * and sets only SCI_EN.  No event source is enabled here.
 */
#define B06WC_PM1_CNT_PRE		0x0000u
#define B06WC_PM1_CNT_TARGET		SCI_EN
#define B06WC_PM1_CNT_DWORD_PRE		0x00000000u
#define B06WC_PM1_CNT_DWORD_TARGET	0x00000001u
#define B06WC_UPRWC			0x3cu
#define B06WC_IOAPIC_SCI_ENTRY		9u
#define B06WC_IOAPIC_SCI_LOW_REG \
	(B06VY_IOAPIC_REDIR_BASE + 2u * B06WC_IOAPIC_SCI_ENTRY)
#define B06WC_IOAPIC_SCI_HIGH_REG	(B06WC_IOAPIC_SCI_LOW_REG + 1u)
#endif

#if CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES
#define B06VZ_SMBUS_RESOURCE_INDEX 8u
#define B06VZ_PM_RESOURCE_INDEX	9u
#define B06VZ_GPIO_RESOURCE_INDEX 10u
#define B06VZ_IOAPIC_RESOURCE_INDEX 11u
#define B06VZ_RCBA_RESOURCE_INDEX 12u
#define B06VZ_LAPIC_RESOURCE_INDEX 13u
#define B06VZ_ROM_RESOURCE_INDEX	14u
#define B06VZ_DOMAIN_RESOURCE_COUNT 15u
#define B06VZ_FIRST_FIXED_RESOURCE_INDEX B06VZ_SMBUS_RESOURCE_INDEX

#define B06VZ_SMBUS_IO_BASE	0x0400u
#define B06VZ_SMBUS_IO_SIZE	0x0020u
#define B06VZ_PM_IO_BASE		0x0500u
#define B06VZ_PM_IO_SIZE		0x0080u
#define B06VZ_GPIO_IO_BASE	0x0580u
#define B06VZ_GPIO_IO_SIZE	0x0040u
#define B06VZ_IOAPIC_BASE	0xfec00000ULL
#define B06VZ_IOAPIC_SIZE	0x00001000ULL
#define B06VZ_RCBA_BASE		0xfed1c000ULL
#define B06VZ_RCBA_SIZE		0x00004000ULL
#define B06VZ_LAPIC_BASE	0xfee00000ULL
#define B06VZ_LAPIC_SIZE	0x00001000ULL
#define B06VZ_ROM_BASE		0xff000000ULL
#define B06VZ_ROM_SIZE		0x01000000ULL
#define B06VZ_ROM_TOP		0x100000000ULL

#define B06VZ_RAM_FLAGS \
	(IORESOURCE_ASSIGNED | IORESOURCE_FIXED | IORESOURCE_MEM | \
	 IORESOURCE_CACHEABLE | IORESOURCE_STORED)
#define B06VZ_RESERVED_RAM_FLAGS \
	(B06VZ_RAM_FLAGS | IORESOURCE_RESERVE)
#define B06VZ_MMIO_FLAGS \
	(IORESOURCE_ASSIGNED | IORESOURCE_FIXED | IORESOURCE_MEM | \
	 IORESOURCE_RESERVE | IORESOURCE_STORED)
#define B06VZ_FIXED_IO_FLAGS \
	(IORESOURCE_ASSIGNED | IORESOURCE_FIXED | IORESOURCE_IO | \
	 IORESOURCE_RESERVE)
#define B06VZ_IO_WINDOW_FLAGS \
	(IORESOURCE_ASSIGNED | IORESOURCE_IO | IORESOURCE_BRIDGE)
#define B06VZ_MEM_WINDOW_FLAGS \
	(IORESOURCE_ASSIGNED | IORESOURCE_MEM | IORESOURCE_BRIDGE)
#if CONFIG_X58_PRO_E_B06WA_HPET_DECODE
#define B06VZ_FULL_DOMAIN_RESOURCE_COUNT B06WA_DOMAIN_RESOURCE_COUNT
#else
#define B06VZ_FULL_DOMAIN_RESOURCE_COUNT B06VZ_DOMAIN_RESOURCE_COUNT
#endif
#endif

_Static_assert(CONFIG(MINIMAL_PCI_SCANNING),
	"B06VN must never scan PCI functions absent from its mandatory devicetree");
_Static_assert(CONFIG(PCI_ALLOW_BUS_MASTER) &&
	CONFIG(PCI_SET_BUS_MASTER_PCI_BRIDGES),
	"B06VN requires standard BME on selected forwarding PCI bridges");
_Static_assert(!CONFIG(PCI_ALLOW_BUS_MASTER_ANY_DEVICE),
	"B06VN must not grant blanket endpoint bus mastering");
_Static_assert(X58_B06V6_OBJECT_BASE + X58_B06V6_OBJECT_SIZE <=
	B06VN_PCI_MMIO_BASE, "B06VN PCI MMIO overlaps a B06VL-smoked RAM window");
_Static_assert(B06VN_LOW_RAM_TOP == B06VN_PCI_MMIO_BASE,
	"B06VN PCI MMIO must begin exactly at the gated low-RAM top");
_Static_assert(B06VN_PCI_MMIO_TOP == B06VN_ECAM_BASE,
	"B06VN PCI MMIO must end before PCIEXBAR ECAM");
_Static_assert(B06VN_ECAM_TOP <= B06VN_HIGH_RAM_BASE,
	"B06VN PCIEXBAR ECAM must end before remapped RAM");
_Static_assert(B06VN_ECAM_DEV0(B06VN_IOU2_X4_DEV,
	B06VN_PCIE_PRTX_BIF_CTRL) == 0xe0008190ULL,
	"B06VN IOU2 telemetry address changed");
_Static_assert(B06VN_ECAM_DEV0(B06VN_IOU0_X16_DEV,
	B06VN_PCIE_PRTX_BIF_CTRL) == 0xe0018190ULL,
	"B06VN IOU0 start address changed");
_Static_assert(B06VN_ECAM_DEV0(B06VN_AUX_X16_DEV,
	B06VN_PCIE_PRTX_BIF_CTRL) == 0xe0038190ULL,
	"B06VN auxiliary x16-port telemetry address changed");
_Static_assert(B06VN_ECAM_DEV0(0x14, B06VO_IOHBUSNO) == 0xe00a010aULL,
	"B06VO IOHBUSNO address changed");
_Static_assert(B06VN_ECAM_ADDR(1, B06VO_ALIAS_DEV, 0, 0) == 0xe0168000ULL,
	"B06VO bus-1 alias-probe address changed");
_Static_assert(B06VN_ECAM_ADDR(2, B06VO_ALIAS_DEV, 0, 0) == 0xe0268000ULL,
	"B06VO bus-2 alias-probe address changed");
_Static_assert(B06VN_ECAM_ADDR(1, B06VO_PEG_ENDPOINT_DEV, 0, 0) ==
	0xe0100000ULL, "B06VO bus-1 endpoint address changed");
_Static_assert(B06VN_LINK_POLL_US * B06VN_LINK_POLL_COUNT == 1000000,
	"B06VN link poll must remain bounded to one second");
#if CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES
_Static_assert(CONFIG(X58_PRO_E_B06VY_IOAPIC_MASK),
	"B06VZ must inherit B06VY's deterministic masked IOAPIC state");
_Static_assert(B06VZ_SMBUS_IO_BASE + B06VZ_SMBUS_IO_SIZE <=
	B06VZ_PM_IO_BASE, "B06VZ SMBus and PM resources overlap");
_Static_assert(B06VZ_PM_IO_BASE + B06VZ_PM_IO_SIZE <=
	B06VZ_GPIO_IO_BASE, "B06VZ PM and GPIO resources overlap");
_Static_assert(B06VZ_GPIO_IO_BASE + B06VZ_GPIO_IO_SIZE <=
	B06VN_PCI_IO_BASE,
	"B06VZ fixed I/O resources must remain below the PCI I/O aperture");
_Static_assert(B06VN_ECAM_TOP <= B06VZ_IOAPIC_BASE,
	"B06VZ IOAPIC resource overlaps ECAM");
_Static_assert(B06VZ_IOAPIC_BASE + B06VZ_IOAPIC_SIZE <= B06VZ_RCBA_BASE,
	"B06VZ IOAPIC and RCBA resources overlap");
_Static_assert(B06VZ_RCBA_BASE + B06VZ_RCBA_SIZE <= B06VZ_LAPIC_BASE,
	"B06VZ RCBA and LAPIC resources overlap");
_Static_assert(B06VZ_LAPIC_BASE + B06VZ_LAPIC_SIZE <= B06VZ_ROM_BASE,
	"B06VZ LAPIC and ROM resources overlap");
_Static_assert(B06VZ_ROM_BASE + B06VZ_ROM_SIZE == B06VZ_ROM_TOP &&
	B06VZ_ROM_TOP == B06VN_HIGH_RAM_BASE,
	"B06VZ ROM resource must end exactly where remapped RAM begins");
_Static_assert(B06VZ_ROM_RESOURCE_INDEX + 1 ==
	B06VZ_DOMAIN_RESOURCE_COUNT,
	"B06VZ fixed resource indices must end at 14");
#endif
#if CONFIG_X58_PRO_E_B06WA_HPET_DECODE
_Static_assert(CONFIG(X58_PRO_E_B06VZ_FIXED_RESOURCES),
	"B06WA must inherit B06VZ's exact fixed-resource contract");
_Static_assert((B06WA_HPTC_FED00000 & B06WA_HPTC_CONTROL_MASK) ==
	B06WA_HPTC_DECODE_ENABLE,
	"B06WA must select FED00000 and only enable HPET decode");
_Static_assert(B06VZ_IOAPIC_BASE + B06VZ_IOAPIC_SIZE <= B06WA_HPET_BASE,
	"B06WA HPET resource overlaps the ICH10R IOAPIC");
_Static_assert(B06WA_HPET_BASE + B06WA_HPET_SIZE <= B06VZ_RCBA_BASE,
	"B06WA HPET resource overlaps RCBA");
_Static_assert(B06WA_HPET_RESOURCE_INDEX == B06VZ_DOMAIN_RESOURCE_COUNT &&
	B06WA_HPET_RESOURCE_INDEX + 1 == B06WA_DOMAIN_RESOURCE_COUNT,
	"B06WA must append only resource index 15");
#endif
#if CONFIG_X58_PRO_E_B06WB_TCO_HALT
_Static_assert(CONFIG(X58_PRO_E_B06WA_HPET_DECODE),
	"B06WB must inherit B06WA's exact HPET and resource contract");
_Static_assert(B06WB_TCO_BASE == DEFAULT_TCOBASE,
	"B06WB fixed TCO base must match the ICH10 PM decode");
_Static_assert(B06WB_PMBASE_ENABLED == (DEFAULT_PMBASE | 1),
	"B06WB exact PMBASE gate changed");
_Static_assert(B06WB_TCO1_CNT_TARGET == B06WB_TCO1_CNT_HLT &&
	!(B06WB_TCO1_CNT_TARGET & B06WB_TCO1_CNT_LOCK),
	"B06WB target must set only TCO_TMR_HLT and leave TCO_LOCK clear");
#endif
#if CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM
_Static_assert(CONFIG(X58_PRO_E_B06WB_TCO_HALT),
	"B06WC must inherit B06WB's complete integrated platform path");
_Static_assert(!CONFIG(NO_PCAT_8259),
	"B06WC requires the legacy 8259 virtual-wire source");
_Static_assert(CONFIG(SEABIOS_HARDWARE_IRQ),
	"B06WC must retain SeaBIOS hardware interrupts");
_Static_assert(B06WC_ELCR2_TARGET == BIT(1),
	"B06WC must make only SCI/IRQ9 level triggered in ELCR2");
#endif
#if CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP || \
	CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE
_Static_assert(I82801JX_SATA_MAP_AHCI_D31F2_MASK == 0x00e0,
	"B06VR must select only SATA MAP bits 7:5");
_Static_assert(I82801JX_SATA_MAP_AHCI_D31F2_VALUE == 0x0060,
	"B06VR must select AHCI with all ports on D31:F2");
#endif
#if CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE
_Static_assert(FD_SAD2 == 0x02000000,
	"B06VU must select only FD_SAD2 bit 25");
_Static_assert((B06VQI_FD_TARGET | FD_SAD2) ==
	B06VU_FD_SATA2_DISABLED,
	"B06VU final FD must compose ICHBASE1 and SAD2 exactly");
#endif
#if CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS
_Static_assert(I82801JX_SATA_PCS_PORT_ENABLE_MASK == 0x3f,
	"B06VS may select only PCS port-enable bits 5:0");
_Static_assert(I82801JX_SATA_PCS_ALL_PORTS_ENABLED == 0x3f,
	"B06VS must enable all six ICH10 SATA ports");
_Static_assert(I82801JX_SATA_PCS_LOW_NON_PORT_MASK == 0xc0,
	"B06VS must gate the PCS low-byte non-port fields");
_Static_assert(I82801JX_SATA_PCS_PRESENCE_MASK == 0x3f00,
	"B06VS presence telemetry mask changed");
_Static_assert(I82801JX_SATA_PCS_RESERVED_14 == 0x4000,
	"B06VS must reject the reserved PCS bit 14");
_Static_assert(I82801JX_SATA_PCS_OOB_RETRY_MODE == 0x8000,
	"B06VS may only preserve the PCS OOB retry policy");
_Static_assert(B06VS_ABAR_SIZE == 0x800,
	"B06VS requires the documented 2-KiB AHCI BAR");
#endif
#if CONFIG_X58_PRO_E_B06VT_ICH10_SATA_CLOCK
_Static_assert(I82801JX_SATA_SCLKCG == 0x94,
	"B06VT requires the documented SCLKCG offset");
_Static_assert(I82801JX_SATA_SCLKCG_FIELD1_MASK == 0x000001ff,
	"B06VT may select only SCLKCG bits 8:0");
_Static_assert(I82801JX_SATA_SCLKCG_FIELD1_REQUIRED == 0x00000193,
	"B06VT requires the documented SCLKCG Field 1 value");
_Static_assert(I82801JX_SATA_SCLKCG_RESERVED_23_9 == 0x00fffe00,
	"B06VT reserved SCLKCG bits 23:9 changed");
_Static_assert(I82801JX_SATA_SCLKCG_PORT_DISABLE_MASK == 0x3f000000,
	"B06VT port-clock-disable mask changed");
_Static_assert(I82801JX_SATA_SCLKCG_RESERVED_31_30 == 0xc0000000,
	"B06VT reserved SCLKCG bits 31:30 changed");
#endif

struct b06vn_expected_pci {
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
 * Root IDs/classes come directly from B06VL-HW-02.  The downstream NIC ID
 * and revision come from the live board inventory.  The operator identifies
 * the replacement card as an AMD Radeon HD 5450, but its exact PCI device ID
 * has not yet been observed.  B06VN therefore gates that one endpoint by AMD
 * vendor, VGA class and header shape, and prints the discovered device ID.
 */
static const struct b06vn_expected_pci b06vn_expected[] = {
	{ PCI_DEVFN(0x03, 0), PCI_DEVFN(0x03, 0), 0x340a8086u,
	  PCI_CLASS_BRIDGE_PCI, -1, PCI_HEADER_TYPE_BRIDGE, false, true, false,
	  "X58 PEG3" },
	{ PCI_DEVFN(0x03, 0), PCI_DEVFN(0x00, 0), B06VN_AMD_VENDOR_ID,
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
#if CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP || \
	CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE
	{ PCI_DEVFN(0x1f, 2), PCI_DEVFN(0x1f, 2), B06VR_SATA1_AHCI_ID,
	  PCI_CLASS_STORAGE_SATA, -1, PCI_HEADER_TYPE_NORMAL, false, false, false,
	  "ICH10R SATA AHCI" },
#else
	{ PCI_DEVFN(0x1f, 2), PCI_DEVFN(0x1f, 2), 0x3a208086u,
	  PCI_CLASS_STORAGE_IDE, -1, PCI_HEADER_TYPE_NORMAL, false, false, false,
	  "ICH10R SATA1" },
	{ PCI_DEVFN(0x1f, 5), PCI_DEVFN(0x1f, 5), 0x3a268086u,
	  PCI_CLASS_STORAGE_IDE, -1, PCI_HEADER_TYPE_NORMAL, false, false, false,
	  "ICH10R SATA2" },
#endif
};

static const uint32_t b06vn_sad_rules[B06VN_SAD_RULE_COUNT] = {
	0x00000bc3u, 0x00000fc0u, 0x000013c3u, 0x000013c0u,
	0x000013c0u, 0x000013c0u, 0x000013c0u, 0x000013c0u,
};

struct b06vn_leaf_resource {
	const struct device *dev;
	const struct resource *resource;
};

#if CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES
struct b06vz_domain_resource_contract {
	unsigned long index;
	uint64_t base;
	uint64_t top;
	unsigned long flags;
};

static const struct b06vz_domain_resource_contract
b06vz_domain_resource_contracts[] = {
	{ 0, 0x00000000ULL, 0x000a0000ULL, B06VZ_RAM_FLAGS },
	{ 1, 0x000a0000ULL, 0x000c0000ULL, B06VZ_MMIO_FLAGS },
	{ 2, 0x000c0000ULL, 0x00100000ULL, B06VZ_RESERVED_RAM_FLAGS },
	{ 3, B06VN_LOW_RAM_BASE, B06VN_LOW_RAM_TOP, B06VZ_RAM_FLAGS },
	{ 4, B06VN_HIGH_RAM_BASE, B06VN_HIGH_RAM_TOP, B06VZ_RAM_FLAGS },
	{ 5, B06VN_PCI_IO_BASE, B06VN_PCI_IO_TOP,
	  B06VZ_IO_WINDOW_FLAGS },
	{ 6, B06VN_PCI_MMIO_BASE, B06VN_PCI_MMIO_TOP,
	  B06VZ_MEM_WINDOW_FLAGS },
	{ 7, B06VN_ECAM_BASE, B06VN_ECAM_TOP, B06VZ_MMIO_FLAGS },
	{ B06VZ_SMBUS_RESOURCE_INDEX, B06VZ_SMBUS_IO_BASE,
	  B06VZ_SMBUS_IO_BASE + B06VZ_SMBUS_IO_SIZE,
	  B06VZ_FIXED_IO_FLAGS },
	{ B06VZ_PM_RESOURCE_INDEX, B06VZ_PM_IO_BASE,
	  B06VZ_PM_IO_BASE + B06VZ_PM_IO_SIZE, B06VZ_FIXED_IO_FLAGS },
	{ B06VZ_GPIO_RESOURCE_INDEX, B06VZ_GPIO_IO_BASE,
	  B06VZ_GPIO_IO_BASE + B06VZ_GPIO_IO_SIZE, B06VZ_FIXED_IO_FLAGS },
	{ B06VZ_IOAPIC_RESOURCE_INDEX, B06VZ_IOAPIC_BASE,
	  B06VZ_IOAPIC_BASE + B06VZ_IOAPIC_SIZE, B06VZ_MMIO_FLAGS },
	{ B06VZ_RCBA_RESOURCE_INDEX, B06VZ_RCBA_BASE,
	  B06VZ_RCBA_BASE + B06VZ_RCBA_SIZE, B06VZ_MMIO_FLAGS },
	{ B06VZ_LAPIC_RESOURCE_INDEX, B06VZ_LAPIC_BASE,
	  B06VZ_LAPIC_BASE + B06VZ_LAPIC_SIZE, B06VZ_MMIO_FLAGS },
	{ B06VZ_ROM_RESOURCE_INDEX, B06VZ_ROM_BASE, B06VZ_ROM_TOP,
	  B06VZ_MMIO_FLAGS },
};

_Static_assert(ARRAY_SIZE(b06vz_domain_resource_contracts) ==
	B06VZ_DOMAIN_RESOURCE_COUNT,
	"B06VZ must describe domain resources 0 through 14 exactly");

#if CONFIG_X58_PRO_E_B06WA_HPET_DECODE
static const struct b06vz_domain_resource_contract
b06wa_hpet_resource_contract = {
	B06WA_HPET_RESOURCE_INDEX,
	B06WA_HPET_BASE,
	B06WA_HPET_BASE + B06WA_HPET_SIZE,
	B06VZ_MMIO_FLAGS,
};
#endif
#endif

static struct device *b06vn_domain;
static bool b06vn_iou0_start_attempted;
static bool b06vn_gpu_absence_reported;
#if CONFIG_X58_PRO_E_B06VO_IOHBUSNO_ROUTE
static bool b06vo_iohbusno_attempted;
#endif
#if CONFIG_X58_PRO_E_B06VP_LAPIC_EXTINT
static bool b06vp_lapic_setup_attempted;
#endif
#if CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT
static bool b06vq_ehci_init_attempted;
static bool b06vq_usb_telemetry_emitted;
#endif
#if CONFIG_X58_PRO_E_B06VQ_PLATRO1
static bool b06vqp_platform_census_emitted;
#endif
#if CONFIG_X58_PRO_E_B06VQ_ICHBASE1
static bool b06vqi_baseline_attempted;
#endif
#if CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE
static bool b06vu_ahci_route_attempted;
#endif
#if CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP
static bool b06vr_ahci_map_attempted;
#endif
#if CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS
static bool b06vs_port_enable_attempted;
#endif
#if CONFIG_X58_PRO_E_B06VT_ICH10_SATA_CLOCK
static bool b06vt_clock_attempted;
#endif
#if CONFIG_X58_PRO_E_B06VV_ICH10_PCS_SCLK
static bool b06vv_pcs_attempted;
static bool b06vv_sclk_attempted;
#endif
#if CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT
static bool b06wd_sata_policy_ready;
#endif
#if CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO
static bool b06vw_mmio_attempted;
static bool b06vw_final_verified;
#endif
#if CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK
static bool b06vy_ioapic_attempted;
#endif
#if CONFIG_X58_PRO_E_B06WA_HPET_DECODE
static bool b06wa_hpet_attempted;
static bool b06wa_hpet_ready;
#endif
#if CONFIG_X58_PRO_E_B06WB_TCO_HALT
static bool b06wb_tco_attempted;
static bool b06wb_tco_ready;
#endif
#if CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM
static bool b06wc_pic_attempted;
static bool b06wc_pic_ready;
static bool b06wc_acpi_mode_attempted;
static bool b06wc_acpi_mode_is_ready;
#endif
#if CONFIG_X58_PRO_E_B06VQ_PLATRO1
_Static_assert(B06VQP_PM_TIMER_MASK == 0x00ffffff,
	"B06VQ-PLATRO1 requires the ICH10 24-bit PM timer");
_Static_assert(B06VQP_PM_TIMER_SAMPLES == 4096,
	"B06VQ-PLATRO1 PM timer census must stay bounded");
_Static_assert(B06VQP_HPET_BASE0 +
	B06VQP_HPET_ADDRESS_MASK * B06VQP_HPET_STRIDE < B06VQP_LAPIC_BASE,
	"B06VQ-PLATRO1 HPET decode choices must stay below LAPIC");
_Static_assert(B06VQP_ROM_BASE >= B06VN_ECAM_TOP,
	"B06VQ-PLATRO1 ROM window must not overlap PCIEXBAR");
#endif

#if CONFIG_X58_PRO_E_B06VQ_ICHBASE1
_Static_assert(I82801JX_CIR8_FIELD_1_0_MASK == 0x00000003u &&
	I82801JX_CIR8_FIELD_1_0_REQUIRED == 0x00000002u,
	"B06VQ-ICHBASE1 CIR8 field contract changed");
_Static_assert(I82801JX_FD_REQUIRED_BIT_0 == 0x00000001u,
	"B06VQ-ICHBASE1 FD field contract changed");
_Static_assert(I82801JX_CIR9_FIELD_27_26_MASK == 0x0c000000u &&
	I82801JX_CIR9_FIELD_27_26_REQUIRED == 0x08000000u,
	"B06VQ-ICHBASE1 CIR9 field contract changed");
_Static_assert(I82801JX_CIR7_FIELD_19_16_MASK == 0x000f0000u &&
	I82801JX_CIR7_FIELD_19_16_REQUIRED == 0x00050000u &&
	I82801JX_CIR13_FIELD_19_16_MASK == 0x000f0000u &&
	I82801JX_CIR13_FIELD_19_16_REQUIRED == 0x00050000u,
	"B06VQ-ICHBASE1 CIR7/CIR13 field contract changed");
_Static_assert(I82801JX_CIR10_REQUIRED_BITS_17_16 == 0x00030000u,
	"B06VQ-ICHBASE1 CIR10 field contract changed");
#endif

_Static_assert(ARRAY_SIZE(b06vn_expected) == B06VN_EXPECTED_FUNCTIONS,
	"B06VN expected-function count changed");

static const struct x58_b06v6_handoff *b06vn_verified_handoff(void)
{
	const struct cbmem_entry *entry;
	const struct x58_b06v6_handoff *handoff;
	void *cbmem_base;
	size_t cbmem_size;
	uintptr_t cbmem_start;
	uintptr_t cbmem_end;

	if (!cbmem_online())
		die_with_post_code(POST_B06VN_HANDOFF_FAIL,
			"[RAMSTAGE] B06VN CBMEM is offline\n");
	entry = cbmem_entry_find(X58_B06V6_CBMEM_ID);
	if (entry == NULL || cbmem_entry_size(entry) < sizeof(*handoff) ||
	    cbmem_get_region(&cbmem_base, &cbmem_size))
		die_with_post_code(POST_B06VN_HANDOFF_FAIL,
			"[RAMSTAGE] B06VN handoff entry/CBMEM region invalid\n");

	cbmem_start = (uintptr_t)cbmem_base;
	if (cbmem_start > UINTPTR_MAX - cbmem_size)
		die_with_post_code(POST_B06VN_HANDOFF_FAIL,
			"[RAMSTAGE] B06VN CBMEM region wraps\n");
	cbmem_end = cbmem_start + cbmem_size;
	if (cbmem_start < X58_B06V6_CBMEM_BASE ||
	    cbmem_end != X58_B06V6_CBMEM_TOP || cbmem_end <= cbmem_start)
		die_with_post_code(POST_B06VN_HANDOFF_FAIL,
			"[RAMSTAGE] B06VN CBMEM escaped the B06VL-smoked window\n");

	handoff = cbmem_entry_start(entry);
	if ((uintptr_t)handoff < cbmem_start ||
	    (uintptr_t)handoff > cbmem_end - sizeof(*handoff) ||
	    !x58_b06v6_handoff_is_valid(handoff) ||
	    !(handoff->flags & X58_B06VF_HANDOFF_LOWMEM_SMOKED) ||
	    !(handoff->flags & X58_B06VL_HANDOFF_BROAD_POST_MINIT))
		die_with_post_code(POST_B06VN_HANDOFF_FAIL,
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
			"[RAMSTAGE] B06VN serial-independent v10 handoff gate failed\n");
#else
			"[RAMSTAGE] B06VN exact B06VL v9 handoff gate failed\n");
#endif

	return handoff;
}

static void b06vn_require_platform_state(const char *phase)
{
	uint32_t value;

	(void)b06vn_verified_handoff();
	if (pci_io_read_config32(B06VN_SAD_DEV, PCI_VENDOR_ID) != B06VN_SAD_ID ||
	    pci_io_read_config32(B06VN_SAD_DEV, B06VN_SAD_PCIEXBAR_LO) !=
		B06VN_PCIEXBAR_LO ||
	    pci_io_read_config32(B06VN_SAD_DEV, B06VN_SAD_PCIEXBAR_HI) != 0 ||
	    pci_io_read_config32(B06VN_X58_HOST_DEV, PCI_VENDOR_ID) !=
		B06VN_X58_HOST_ID ||
	    pci_io_read_config32(B06VN_X58_HOST_DEV, PCI_CLASS_REVISION) !=
		B06VN_X58_HOST_CLASSREV)
		die_with_post_code(POST_B06VN_PLATFORM_FAIL,
			"[RAMSTAGE] B06VN %s host/PCIEXBAR gate failed\n", phase);

	for (size_t i = 0; i < ARRAY_SIZE(b06vn_sad_rules); i++) {
		value = pci_io_read_config32(B06VN_SAD_DEV,
			B06VN_SAD_RULE_FIRST + i * sizeof(uint32_t));
		if (value != b06vn_sad_rules[i])
			die_with_post_code(POST_B06VN_PLATFORM_FAIL,
				"[RAMSTAGE] B06VN %s SAD%zu=%08x expected=%08x\n",
				phase, i, value, b06vn_sad_rules[i]);
	}

	if (pci_io_read_config32(B06VN_IOH_HM_DEV, PCI_VENDOR_ID) !=
		B06VN_IOH_HM_ID ||
	    pci_io_read_config32(B06VN_IOH_HM_DEV, B06VN_IOH_TOLM) !=
		B06VN_TOLM_VALUE ||
	    pci_io_read_config32(B06VN_IOH_HM_DEV, B06VN_IOH_TOHM_LO) !=
		B06VN_TOHM_LO_VALUE ||
	    pci_io_read_config32(B06VN_IOH_HM_DEV, B06VN_IOH_TOHM_HI) !=
		B06VN_TOHM_HI_VALUE)
		die_with_post_code(POST_B06VN_PLATFORM_FAIL,
			"[RAMSTAGE] B06VN %s exact TOLM/TOHM gate failed\n", phase);
}

static uint32_t b06vn_ecam_read32(unsigned int dev, unsigned int reg)
{
	return read32p(B06VN_ECAM_DEV0(dev, reg));
}

static uint16_t b06vn_ecam_read16(unsigned int dev, unsigned int reg)
{
	return read16p(B06VN_ECAM_DEV0(dev, reg));
}

#if CONFIG_X58_PRO_E_B06VO_IOHBUSNO_ROUTE
static uint32_t b06vo_ecam_read_id(unsigned int bus, unsigned int dev,
	unsigned int fn)
{
	return read32p(B06VN_ECAM_ADDR(bus, dev, fn, PCI_VENDOR_ID));
}

static void b06vo_log_ioh_routing(const char *phase)
{
	printk(BIOS_NOTICE,
	       "[IOHCFG] %s IOHBUSNO=%04x LCFGBUS=%02x-%02x "
	       "GCFGBUS=%02x-%02x ALIAS01:0d.0=%08x ALIAS02:0d.0=%08x\n",
	       phase, read16p(B06VN_ECAM_DEV0(0x14, B06VO_IOHBUSNO)),
	       read8p(B06VN_ECAM_DEV0(0x14, B06VO_LCFGBUS_BASE)),
	       read8p(B06VN_ECAM_DEV0(0x14, B06VO_LCFGBUS_LIMIT)),
	       read8p(B06VN_ECAM_DEV0(0x14, B06VO_GCFGBUS_BASE)),
	       read8p(B06VN_ECAM_DEV0(0x14, B06VO_GCFGBUS_LIMIT)),
	       b06vo_ecam_read_id(1, B06VO_ALIAS_DEV, 0),
	       b06vo_ecam_read_id(2, B06VO_ALIAS_DEV, 0));
}

/*
 * This is B06VO's sole new write hypothesis.  Accept only the documented
 * reset value or the exact vendor-observed target, never mask unknown bits,
 * and require the complete 16-bit readback before PCI routing continues.
 */
static void b06vo_program_ioh_bus_number_once(void)
{
	const uintptr_t address = B06VN_ECAM_DEV0(0x14, B06VO_IOHBUSNO);
	const uint16_t before = read16p(address);
	uint16_t after;
	bool wrote = false;

	if (b06vo_iohbusno_attempted)
		die_with_post_code(POST_B06VO_IOHBUSNO_FAIL,
			"[IOHCFG] B06VO refused a second IOHBUSNO attempt\n");
	b06vo_iohbusno_attempted = true;
	b06vo_log_ioh_routing("PRE");

	if (before != 0x0000 && before != B06VO_IOHBUSNO_BUS0_VALID)
		die_with_post_code(POST_B06VO_IOHBUSNO_FAIL,
			"[IOHCFG] B06VO IOHBUSNO PRE=%04x is outside {0000,0100}\n",
			before);
	if (before == 0x0000) {
		write16p(address, B06VO_IOHBUSNO_BUS0_VALID);
		wrote = true;
	}

	after = read16p(address);
	b06vo_log_ioh_routing("POST");
	if (after != B06VO_IOHBUSNO_BUS0_VALID)
		die_with_post_code(POST_B06VO_IOHBUSNO_FAIL,
			"[IOHCFG] B06VO IOHBUSNO readback=%04x expected=0100\n",
			after);
	post_code(POST_B06VO_IOHBUSNO_OK);
	printk(BIOS_NOTICE,
	       "[IOHCFG] B06VO IOHBUSNO exact gate PASS (write=%u)\n",
	       (unsigned int)wrote);
}
#endif

#if CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT
struct b06vq_usb_controller {
	pci_devfn_t dev;
	const char *name;
};

static const struct b06vq_usb_controller b06vq_ehci_controllers[] = {
	{ B06VQ_EHCI2_DEV, "EHCI2 00:1a.7" },
	{ B06VQ_EHCI1_DEV, "EHCI1 00:1d.7" },
};

static const struct b06vq_usb_controller b06vq_uhci_controllers[] = {
	{ PCI_DEV(0, 0x1a, 0), "UHCI4 00:1a.0" },
	{ PCI_DEV(0, 0x1a, 1), "UHCI5 00:1a.1" },
	{ PCI_DEV(0, 0x1a, 2), "UHCI6 00:1a.2" },
	{ PCI_DEV(0, 0x1d, 0), "UHCI1 00:1d.0" },
	{ PCI_DEV(0, 0x1d, 1), "UHCI2 00:1d.1" },
	{ PCI_DEV(0, 0x1d, 2), "UHCI3 00:1d.2" },
};

static uint32_t b06vq_ehci_target(uint32_t before)
{
	return (before & ~I82801JX_EHCI_FCREG_REQUIRED_MASK) |
		I82801JX_EHCI_FCREG_REQUIRED_VALUE;
}

static void b06vq_program_ehci_once(void)
{
	uint32_t before[ARRAY_SIZE(b06vq_ehci_controllers)];
	uint32_t after[ARRAY_SIZE(b06vq_ehci_controllers)];

	if (b06vq_ehci_init_attempted)
		die_with_post_code(POST_B06VQ_EHCI_FAIL,
			"[USB] B06VQ refused a second EHCI initialization attempt\n");
	b06vq_ehci_init_attempted = true;

	for (size_t i = 0; i < ARRAY_SIZE(b06vq_ehci_controllers); i++) {
		before[i] = pci_io_read_config32(b06vq_ehci_controllers[i].dev,
			I82801JX_EHCI_FCREG);
		printk(BIOS_NOTICE,
		       "[USB] %s EHCIIR2 PRE=%08x TARGET=%08x\n",
		       b06vq_ehci_controllers[i].name, before[i],
		       b06vq_ehci_target(before[i]));
	}

	post_code(POST_B06VQ_EHCI_BEGIN);
	i82801jx_ehci_init();

	for (size_t i = 0; i < ARRAY_SIZE(b06vq_ehci_controllers); i++) {
		after[i] = pci_io_read_config32(b06vq_ehci_controllers[i].dev,
			I82801JX_EHCI_FCREG);
		printk(BIOS_NOTICE, "[USB] %s EHCIIR2 POST=%08x\n",
		       b06vq_ehci_controllers[i].name, after[i]);
		if (after[i] != b06vq_ehci_target(before[i]))
			die_with_post_code(POST_B06VQ_EHCI_FAIL,
				"[USB] %s EHCIIR2 readback=%08x expected=%08x\n",
				b06vq_ehci_controllers[i].name, after[i],
				b06vq_ehci_target(before[i]));
	}

	post_code(POST_B06VQ_EHCI_OK);
	printk(BIOS_NOTICE,
	       "[USB] B06VQ ICH10 EHCI BIOS-required fields exact gate PASS\n");
}

static bool b06vq_ehci_bar_valid(uint32_t bar)
{
	const uint32_t base = bar & ~PCI_BASE_ADDRESS_MEM_ATTR_MASK;

	return !(bar & PCI_BASE_ADDRESS_SPACE_IO) &&
		base >= B06VN_PCI_MMIO_BASE &&
		base <= B06VN_PCI_MMIO_TOP - B06VQ_EHCI_BAR_SIZE;
}

static void b06vq_log_ehci_runtime(
	const struct b06vq_usb_controller *controller)
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

	if (!b06vq_ehci_bar_valid(bar)) {
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
	printk(BIOS_NOTICE,
	       "[USB] %s CMD=%04x BAR=%08x PMCSR=%04x EHCIIR2=%08x LEGACY=%08x/%08x/%08x CAPLEN=%02x HCIVER=%04x HCSPARAMS=%08x PORTS=%u\n",
	       controller->name, command, bar, pmcsr, fc,
	       pci_io_read_config32(controller->dev, B06VQ_EHCI_LEGACY_CAP),
	       pci_io_read_config32(controller->dev, B06VQ_EHCI_LEGACY_CTLSTS),
	       pci_io_read_config32(controller->dev, B06VQ_EHCI_LEGACY_EXT),
	       caplength, hciversion, hcsparams, ports);
#if CONFIG_X58_PRO_E_B06VQ_USB_TRACE1 || \
	CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE
	printk(BIOS_NOTICE,
	       "[USB-GATE] %s HCCPARAMS=%08x CFG61=%02x LEG68=%08x "
	       "SMI6C=%04x EXT70=%04x CFG84=%08x\n",
	       controller->name,
	       read32p(base + B06VQ_EHCI_CAP_HCCPARAMS),
	       pci_io_read_config8(controller->dev, B06VQ_EHCI_CFG61),
	       pci_io_read_config32(controller->dev, B06VQ_EHCI_LEGACY_CAP),
	       pci_io_read_config16(controller->dev,
			B06VQ_EHCI_LEGACY_CTLSTS) & B06VQ_EHCI_SMI_MASK,
	       pci_io_read_config16(controller->dev,
			B06VQ_EHCI_LEGACY_EXT) & B06VQ_EHCI_EXT_SMI_MASK,
	       pci_io_read_config32(controller->dev, B06VQ_EHCI_CFG84));
#endif

	if (caplength < B06VQ_EHCI_CAPLEN_MIN ||
	    caplength > B06VQ_EHCI_CAPLEN_MAX ||
	    hciversion != B06VQ_EHCI_HCIVERSION) {
		printk(BIOS_ERR, "[USB] %s invalid EHCI capability header\n",
		       controller->name);
		return;
	}

	op = base + caplength;
	printk(BIOS_NOTICE,
	       "[USB] %s PRE-SEABIOS USBCMD=%08x USBSTS=%08x USBINTR=%08x CONFIGFLAG=%08x\n",
	       controller->name,
	       read32p(op + B06VQ_EHCI_OP_USBCMD),
	       read32p(op + B06VQ_EHCI_OP_USBSTS),
	       read32p(op + B06VQ_EHCI_OP_USBINTR),
	       read32p(op + B06VQ_EHCI_OP_CONFIGFLAG));
	if (ports > B06VQ_EHCI_PORT_MAX)
		ports = B06VQ_EHCI_PORT_MAX;
	for (unsigned int port = 0; port < ports; port++)
		printk(BIOS_NOTICE, "[USB] %s PORTSC%u=%08x\n",
		       controller->name, port + 1,
		       read32p(op + B06VQ_EHCI_OP_PORTSC + port * sizeof(uint32_t)));
}

static bool b06vq_uhci_bar_valid(uint32_t bar)
{
	const uint32_t base = bar & ~PCI_BASE_ADDRESS_IO_ATTR_MASK;

	return (bar & PCI_BASE_ADDRESS_SPACE_IO) &&
		base >= B06VN_PCI_IO_BASE &&
		base <= B06VN_PCI_IO_TOP - B06VQ_UHCI_BAR_SIZE;
}

static void b06vq_log_uhci_runtime(
	const struct b06vq_usb_controller *controller)
{
	const uint16_t command = pci_io_read_config16(controller->dev,
		PCI_COMMAND);
	const uint32_t bar = pci_io_read_config32(controller->dev,
		PCI_BASE_ADDRESS_4);
	uint16_t base;

	if (!b06vq_uhci_bar_valid(bar)) {
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
	printk(BIOS_NOTICE,
	       "[USB] %s CMD=%04x BAR4=%08x LEGKEY=%04x PRE-SEABIOS USBCMD=%04x USBSTS=%04x USBINTR=%04x PORTSC1=%04x PORTSC2=%04x\n",
	       controller->name, command, bar,
	       pci_io_read_config16(controller->dev, B06VQ_UHCI_LEGKEY),
	       inw(base + B06VQ_UHCI_USBCMD),
	       inw(base + B06VQ_UHCI_USBSTS),
	       inw(base + B06VQ_UHCI_USBINTR),
	       inw(base + B06VQ_UHCI_PORTSC1),
	       inw(base + B06VQ_UHCI_PORTSC2));
#if CONFIG_X58_PRO_E_B06VQ_USB_TRACE1 || \
	CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE
	printk(BIOS_NOTICE,
	       "[USB-GATE] %s CFGC8=%04x CFGCA=%04x\n",
	       controller->name,
	       pci_io_read_config16(controller->dev, B06VQ_UHCI_CFG_C8),
	       pci_io_read_config16(controller->dev, B06VQ_UHCI_CFG_CA));
#endif
}

static void b06vq_log_usb_runtime_once(void)
{
#if CONFIG_X58_PRO_E_B06VQ_USB_TRACE1 || \
	CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE
	const uint32_t fd = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_FD);
	const uint32_t cg = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_CG);
	const uint16_t ppo = read16p(CONFIG_FIXED_RCBA_MMIO_BASE + B06VQ_RCBA_PPO);
	const uint32_t map = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_MAP);
	const uint32_t gpio1 = inl(DEFAULT_GPIOBASE + GP_IO_USE_SEL);
	const uint32_t gpio2 = inl(DEFAULT_GPIOBASE + GP_IO_USE_SEL2);
#endif

	if (b06vq_usb_telemetry_emitted)
		die_with_post_code(POST_B06VQ_EHCI_FAIL,
			"[USB] B06VQ refused duplicate pre-SeaBIOS telemetry\n");
	b06vq_usb_telemetry_emitted = true;
	printk(BIOS_NOTICE,
	       "[USB] ICH10 PRE-SEABIOS PPO=%04x MAP=%08x UPRWC=%04x GPIO_USE=%08x/%08x\n",
	       read16p(CONFIG_FIXED_RCBA_MMIO_BASE + B06VQ_RCBA_PPO),
	       read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_MAP),
	       inw(DEFAULT_PMBASE + B06VQ_PMBASE_UPRWC),
	       inl(DEFAULT_GPIOBASE + GP_IO_USE_SEL),
	       inl(DEFAULT_GPIOBASE + GP_IO_USE_SEL2));
#if CONFIG_X58_PRO_E_B06VQ_USB_TRACE1 || \
	CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE
	printk(BIOS_NOTICE,
	       "[USB-GATE] FD=%08x USB_DIS=%04x CG=%08x CG20=%u "
	       "PPO12=%03x MAP0=%u GPIO_OC=%08x/%08x\n",
	       fd, fd & B06VQ_USB_FD_DISABLE_MASK, cg,
	       !!(cg & B06VQ_USB_CG_DISABLE), ppo & B06VQ_USB_PPO_MASK,
	       !!(map & B06VQ_USB_MAP_MODE),
	       gpio1 & B06VQ_USB_GPIO1_OC_MASK,
	       gpio2 & B06VQ_USB_GPIO2_OC_MASK);
#endif

	for (size_t i = 0; i < ARRAY_SIZE(b06vq_ehci_controllers); i++)
		b06vq_log_ehci_runtime(&b06vq_ehci_controllers[i]);
	for (size_t i = 0; i < ARRAY_SIZE(b06vq_uhci_controllers); i++)
		b06vq_log_uhci_runtime(&b06vq_uhci_controllers[i]);
}
#endif

#if CONFIG_X58_PRO_E_B06VQ_ICHBASE1
struct b06vqi_required_snapshot {
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

static struct b06vqi_required_snapshot b06vqi_read_required_snapshot(void)
{
	struct b06vqi_required_snapshot snapshot = {
		.lpc_id = pci_io_read_config32(B06VQI_LPC_DEV, PCI_VENDOR_ID),
		.rcba = pci_io_read_config32(B06VQI_LPC_DEV, RCBA),
	};

	if (snapshot.lpc_id != B06VQI_LPC_ID ||
	    snapshot.rcba != B06VQI_RCBA_ENABLED)
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

static bool b06vqi_snapshot_is_pre(
	const struct b06vqi_required_snapshot *snapshot)
{
	return snapshot->cir8 == B06VQI_CIR8_PRE &&
		snapshot->fd == B06VQI_FD_PRE &&
		snapshot->cir9 == B06VQI_CIR9_PRE &&
		snapshot->cir7 == B06VQI_CIR7_PRE &&
		snapshot->cir13 == B06VQI_CIR13_PRE &&
		snapshot->cir10 == B06VQI_CIR10_PRE;
}

static bool b06vqi_snapshot_is_target(
	const struct b06vqi_required_snapshot *snapshot)
{
	return snapshot->cir8 == B06VQI_CIR8_TARGET &&
		snapshot->fd == B06VQI_FD_TARGET &&
		snapshot->cir9 == B06VQI_CIR9_TARGET &&
		snapshot->cir7 == B06VQI_CIR7_TARGET &&
		snapshot->cir13 == B06VQI_CIR13_TARGET &&
		snapshot->cir10 == B06VQI_CIR10_TARGET;
}

static bool b06vqi_snapshot_has_fixed_gates(
	const struct b06vqi_required_snapshot *snapshot)
{
	return snapshot->lpc_id == B06VQI_LPC_ID &&
		snapshot->rcba == B06VQI_RCBA_ENABLED &&
		snapshot->rcba_mmio_read &&
		snapshot->fdsw == B06VQI_FDSW_UNLOCKED;
}

static void b06vqi_log_required_snapshot(const char *phase,
	const struct b06vqi_required_snapshot *snapshot)
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
 * The complete PRE and TARGET tuples were measured on B06VP in one retained
 * CAR session. Reject every partial/mixed tuple before the first mutation.
 * Each selected field was then live-written and read back through ROMMON;
 * see 2026-09-06-b06vp-car-ich10-six-required-fields-live.raw.
 */
static void b06vqi_program_baseline_once(void)
{
	struct b06vqi_required_snapshot before;
	struct b06vqi_required_snapshot after;
	bool wrote = false;

	if (b06vqi_baseline_attempted)
		die_with_post_code(POST_B06VQI_BASE_FAIL,
			"[ICHBASE] B06VQ-ICHBASE1 refused a second attempt\n");
	b06vqi_baseline_attempted = true;

	before = b06vqi_read_required_snapshot();
	b06vqi_log_required_snapshot("PRE", &before);
	if (!b06vqi_snapshot_has_fixed_gates(&before) ||
	    (!b06vqi_snapshot_is_pre(&before) &&
	     !b06vqi_snapshot_is_target(&before)))
		die_with_post_code(POST_B06VQI_BASE_FAIL,
			"[ICHBASE] rejected identity/RCBA/FDSW or mixed six-field prestate\n");

	post_code(POST_B06VQI_BASE_BEGIN);
	if (b06vqi_snapshot_is_pre(&before)) {
		i82801jx_program_required_fields();
		wrote = true;
	}
	after = b06vqi_read_required_snapshot();
	b06vqi_log_required_snapshot("POST", &after);
	if (!b06vqi_snapshot_has_fixed_gates(&after) ||
	    !b06vqi_snapshot_is_target(&after))
		die_with_post_code(POST_B06VQI_BASE_FAIL,
			"[ICHBASE] six-field complete target readback failed\n");

	post_code(POST_B06VQI_BASE_OK);
	printk(BIOS_NOTICE,
	       "[ICHBASE] B06VQ-ICHBASE1 six required fields exact gate PASS "
	       "(write=%u); GCS/CIR5/FDSW/hide/lock/RPFN/MAP/PMIR/IRQ untouched\n",
	       (unsigned int)wrote);
}
#endif

#if CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK
struct b06vy_ioapic_redir {
	uint32_t low;
	uint32_t high;
};

_Static_assert(B06VY_IOAPIC_MAX_REDIR + 1 == B06VY_IOAPIC_REDIR_COUNT,
	"B06VY IOAPIC redirection-entry count changed");
_Static_assert(B06VY_IOAPIC_VERSION_TARGET ==
	((B06VY_IOAPIC_MAX_REDIR << 16) | B06VY_IOAPIC_VERSION),
	"B06VY exact IOAPIC version tuple changed");
_Static_assert(B06VY_IOAPIC_LOW_TARGET == B06VY_IOAPIC_MASK_BIT,
	"B06VY canonical low entry must contain only the mask bit");

static bool b06vy_ioapic_select(uint8_t reg)
{
	write32p(B06VY_IOAPIC_BASE + B06VY_IOREGSEL, reg);
	return read32p(B06VY_IOAPIC_BASE + B06VY_IOREGSEL) == reg;
}

static bool b06vy_ioapic_read(uint8_t reg, uint32_t *value)
{
	if (!b06vy_ioapic_select(reg))
		return false;
	*value = read32p(B06VY_IOAPIC_BASE + B06VY_IOWIN);
	return true;
}

static bool b06vy_ioapic_write_exact(uint8_t reg, uint32_t value)
{
	if (!b06vy_ioapic_select(reg))
		return false;
	write32p(B06VY_IOAPIC_BASE + B06VY_IOWIN, value);
	return read32p(B06VY_IOAPIC_BASE + B06VY_IOWIN) == value;
}

static void b06vy_ioapic_fail(const char *reason)
{
	/* Best effort keeps the indirect selector deterministic even on failure. */
	write32p(B06VY_IOAPIC_BASE + B06VY_IOREGSEL, B06VY_IOAPIC_ID_REG);
	printk(BIOS_ERR,
	       "[IOAPIC] B06VY FAIL reason=%s IOREGSEL=%08x\n", reason,
	       read32p(B06VY_IOAPIC_BASE + B06VY_IOREGSEL));
	die_with_post_code(POST_B06VY_FAIL,
		"[IOAPIC] B06VY fail-closed before PCI enumeration\n");
}

static bool b06vy_redirection_snapshot(
	struct b06vy_ioapic_redir redir[B06VY_IOAPIC_REDIR_COUNT],
	const char *phase)
{
	bool all_masked = true;

	for (unsigned int entry = 0;
	     entry < B06VY_IOAPIC_REDIR_COUNT; entry++) {
		const uint8_t low_reg = B06VY_IOAPIC_REDIR_BASE + 2 * entry;
		const uint8_t high_reg = low_reg + 1;

		if (!b06vy_ioapic_read(low_reg, &redir[entry].low) ||
		    !b06vy_ioapic_read(high_reg, &redir[entry].high))
			b06vy_ioapic_fail("SELECTOR_READBACK");
		all_masked &= (redir[entry].low & B06VY_IOAPIC_MASK_BIT) != 0;
		printk(BIOS_NOTICE,
		       "[IOAPIC] B06VY %s E%02u LOW=%08x HIGH=%08x MASKED=%u\n",
		       phase, entry, redir[entry].low, redir[entry].high,
		       !!(redir[entry].low & B06VY_IOAPIC_MASK_BIT));
	}
	return all_masked;
}

/*
 * The two B06VP cold censuses found different undefined redirection contents,
 * but all 24 entries were masked.  A separate live entry-23 transaction proved
 * HIGH-then-LOW indirect writes, exact readback, and selector restoration.  Do
 * not copy any reset vector/delivery/destination bits: canonicalize only while
 * every entry is still masked, and leave routing to the payload.
 */
static void b06vy_decode_and_mask_ioapic_once(void)
{
	struct b06vy_ioapic_redir before[B06VY_IOAPIC_REDIR_COUNT];
	struct b06vy_ioapic_redir after[B06VY_IOAPIC_REDIR_COUNT];
	const uint32_t lpc_id = pci_io_read_config32(B06VY_LPC_DEV,
		PCI_VENDOR_ID);
	const uint32_t rcba = pci_io_read_config32(B06VY_LPC_DEV, RCBA);
	uint8_t oic;
	uint32_t ioapic_id;
	uint32_t ioapic_version;
	unsigned int changed = 0;
	bool decode_write = false;

	if (b06vy_ioapic_attempted)
		die_with_post_code(POST_B06VY_FAIL,
			"[IOAPIC] B06VY refused a second initialization attempt\n");
	b06vy_ioapic_attempted = true;
	if (!b06vqi_baseline_attempted || !b06vu_ahci_route_attempted)
		die_with_post_code(POST_B06VY_FAIL,
			"[IOAPIC] B06VY inherited ICHBASE/AHCI-route order failed\n");
	if (lpc_id != B06VY_LPC_ID || rcba != B06VY_RCBA_ENABLED)
		die_with_post_code(POST_B06VY_FAIL,
			"[IOAPIC] B06VY LPC/RCBA gate failed: ID=%08x RCBA=%08x\n",
			lpc_id, rcba);

	oic = read8p(CONFIG_FIXED_RCBA_MMIO_BASE + OIC);
	printk(BIOS_NOTICE,
	       "[IOAPIC] B06VY PRE LPC_ID=%08x RCBA=%08x OIC=%02x\n",
	       lpc_id, rcba, oic);
	if (oic != B06VY_OIC_DISABLED && oic != B06VY_OIC_ENABLED)
		die_with_post_code(POST_B06VY_FAIL,
			"[IOAPIC] B06VY OIC=%02x outside exact {00,03} allowlist\n",
			oic);

	post_code(POST_B06VY_DECODE_BEGIN);
	if (oic == B06VY_OIC_DISABLED) {
		write8p(CONFIG_FIXED_RCBA_MMIO_BASE + OIC,
			B06VY_OIC_ENABLED);
		decode_write = true;
	}
	oic = read8p(CONFIG_FIXED_RCBA_MMIO_BASE + OIC);
	if (oic != B06VY_OIC_ENABLED)
		die_with_post_code(POST_B06VY_FAIL,
			"[IOAPIC] B06VY OIC exact readback failed: %02x\n", oic);

	if (!b06vy_ioapic_read(B06VY_IOAPIC_ID_REG, &ioapic_id) ||
	    !b06vy_ioapic_read(B06VY_IOAPIC_VERSION_REG, &ioapic_version))
		b06vy_ioapic_fail("ID_VERSION_SELECTOR");
	printk(BIOS_NOTICE,
	       "[IOAPIC] B06VY DECODE OIC=%02x WRITE=%u ID=%08x VERSION=%08x "
	       "VER=%02x MAXRED=%02x COUNT=%u\n",
	       oic, (unsigned int)decode_write, ioapic_id, ioapic_version,
	       ioapic_version & 0xff, (ioapic_version >> 16) & 0xff,
	       ((ioapic_version >> 16) & 0xff) + 1);
	if (ioapic_id != B06VY_IOAPIC_ID_TARGET ||
	    ioapic_version != B06VY_IOAPIC_VERSION_TARGET)
		b06vy_ioapic_fail("ID_VERSION_TUPLE");
	post_code(POST_B06VY_DECODE_OK);

	if (!b06vy_redirection_snapshot(before, "PRE"))
		b06vy_ioapic_fail("PRE_ENTRY_UNMASKED");
	post_code(POST_B06VY_CENSUS_OK);

	for (unsigned int entry = 0;
	     entry < B06VY_IOAPIC_REDIR_COUNT; entry++) {
		const uint8_t low_reg = B06VY_IOAPIC_REDIR_BASE + 2 * entry;
		const uint8_t high_reg = low_reg + 1;
		uint32_t low_now;

		/* Reconfirm masking immediately before this entry's first write. */
		if (!b06vy_ioapic_read(low_reg, &low_now) ||
		    !(low_now & B06VY_IOAPIC_MASK_BIT))
			b06vy_ioapic_fail("ENTRY_CHANGED_OR_UNMASKED");
		if (before[entry].high != B06VY_IOAPIC_HIGH_TARGET ||
		    before[entry].low != B06VY_IOAPIC_LOW_TARGET)
			changed++;
		if (before[entry].high != B06VY_IOAPIC_HIGH_TARGET &&
		    !b06vy_ioapic_write_exact(high_reg,
			B06VY_IOAPIC_HIGH_TARGET))
			b06vy_ioapic_fail("HIGH_WRITE_READBACK");
		if (low_now != B06VY_IOAPIC_LOW_TARGET &&
		    !b06vy_ioapic_write_exact(low_reg,
			B06VY_IOAPIC_LOW_TARGET))
			b06vy_ioapic_fail("LOW_WRITE_READBACK");
	}

	if (!b06vy_redirection_snapshot(after, "POST"))
		b06vy_ioapic_fail("POST_ENTRY_UNMASKED");
	for (unsigned int entry = 0;
	     entry < B06VY_IOAPIC_REDIR_COUNT; entry++) {
		if (after[entry].low != B06VY_IOAPIC_LOW_TARGET ||
		    after[entry].high != B06VY_IOAPIC_HIGH_TARGET)
			b06vy_ioapic_fail("POST_ENTRY_NONCANONICAL");
	}
	if (!b06vy_ioapic_select(B06VY_IOAPIC_ID_REG))
		b06vy_ioapic_fail("FINAL_SELECTOR_RESTORE");

	post_code(POST_B06VY_READY);
	printk(BIOS_NOTICE,
	       "[IOAPIC] B06VY READY ENTRIES=24 CHANGED=%u IOREGSEL=00000000 "
	       "IOAPIC_EXTINT_ROUTE=0 MRE_LOCK_WRITE=0 PIRQ_WRITE=0 "
	       "SCI_WRITE=0 PAYLOAD_OWNS_ROUTING=1\n",
	       changed);
}
#endif

#if CONFIG_X58_PRO_E_B06WB_TCO_HALT
struct b06wb_tco_snapshot {
	uint32_t gcs;
	uint16_t tco_rld;
	uint16_t tco1_sts;
	uint16_t tco2_sts;
	uint16_t tco1_cnt;
	uint16_t tco2_cnt;
	uint16_t tco_tmr;
};

static void b06wb_read_tco_snapshot(struct b06wb_tco_snapshot *snapshot)
{
	snapshot->gcs = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + GCS);
	snapshot->tco_rld = inw(B06WB_TCO_BASE + B06WB_TCO_RLD);
	snapshot->tco1_sts = inw(B06WB_TCO_BASE + B06WB_TCO1_STS);
	snapshot->tco2_sts = inw(B06WB_TCO_BASE + B06WB_TCO2_STS);
	snapshot->tco1_cnt = inw(B06WB_TCO_BASE + B06WB_TCO1_CNT);
	snapshot->tco2_cnt = inw(B06WB_TCO_BASE + B06WB_TCO2_CNT);
	snapshot->tco_tmr = inw(B06WB_TCO_BASE + B06WB_TCO_TMR);
}

static void b06wb_log_tco_snapshot(const char *phase,
	const struct b06wb_tco_snapshot *snapshot, bool control_write)
{
	printk(BIOS_NOTICE,
	       "[TCO] B06WB %s GCS=%08x TCO_RLD=%04x TCO1_STS=%04x "
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

static void b06wb_tco_fail(const char *reason)
{
	die_with_post_code(POST_B06WB_TCO_FAIL,
		"[TCO] B06WB fail-closed: %s\n", reason);
}

/*
 * B06WB's only possible hardware mutation is a 16-bit masked/RMW transition
 * of complete TCO1_CNT 0000 -> 0800.  All decode and exact-prestate gates run
 * first.  Status, reload, timer, GCS and TCO_LOCK remain read-only.
 */
static void b06wb_halt_tco_once(void)
{
	const uint32_t lpc_id = pci_io_read_config32(B06VY_LPC_DEV,
		PCI_VENDOR_ID);
	const uint32_t rcba = pci_io_read_config32(B06VY_LPC_DEV, RCBA);
	const uint32_t pmbase = pci_io_read_config32(B06VY_LPC_DEV,
		B06WB_PMBASE_REG);
	const uint8_t acpi_cntl = pci_io_read_config8(B06VY_LPC_DEV,
		B06WB_ACPI_CNTL_REG);
	struct b06wb_tco_snapshot before;
	struct b06wb_tco_snapshot after;
	bool control_write = false;

	if (b06wb_tco_attempted)
		b06wb_tco_fail("SECOND_ATTEMPT");
	b06wb_tco_attempted = true;
	if (b06vqi_baseline_attempted || b06vu_ahci_route_attempted ||
	    b06vy_ioapic_attempted || b06wa_hpet_attempted)
		b06wb_tco_fail("LATER_STAGE_ALREADY_ATTEMPTED");
	if (lpc_id != B06VY_LPC_ID || rcba != B06VY_RCBA_ENABLED)
		b06wb_tco_fail("LPC_RCBA_IDENTITY");
	if (pmbase != B06WB_PMBASE_ENABLED ||
	    acpi_cntl != B06WB_ACPI_DECODE_ENABLED)
		b06wb_tco_fail("PM_DECODE_NOT_EXACT");

	b06wb_read_tco_snapshot(&before);
	if (before.tco1_cnt != B06WB_TCO1_CNT_PRE &&
	    before.tco1_cnt != B06WB_TCO1_CNT_TARGET)
		b06wb_tco_fail("TCO1_CNT_OUTSIDE_EXACT_ALLOWLIST");

	printk(BIOS_NOTICE,
	       "[TCO] B06WB DECODE LPC_ID=%08x RCBA=%08x PMBASE=%08x "
	       "ACPI_CNTL=%02x ALLOWLIST=0000,0800\n",
	       lpc_id, rcba, pmbase, (unsigned int)acpi_cntl);
	b06wb_log_tco_snapshot("PRE", &before, false);
	post_code(POST_B06WB_TCO_BEGIN);
	if (before.tco1_cnt == B06WB_TCO1_CNT_PRE) {
		const uint16_t target =
			(before.tco1_cnt & (uint16_t)~B06WB_TCO1_CNT_HLT) |
			B06WB_TCO1_CNT_HLT;

		if (target != B06WB_TCO1_CNT_TARGET)
			b06wb_tco_fail("TCO1_CNT_TARGET_CONSTRUCTION");
		outw(target, B06WB_TCO_BASE + B06WB_TCO1_CNT);
		control_write = true;
	}

	b06wb_read_tco_snapshot(&after);
	b06wb_log_tco_snapshot("POST", &after, control_write);
	if (after.tco1_cnt != B06WB_TCO1_CNT_TARGET)
		b06wb_tco_fail("TCO1_CNT_EXACT_READBACK");
	if (after.tco1_sts != before.tco1_sts ||
	    after.tco2_sts != before.tco2_sts)
		b06wb_tco_fail("STATUS_BITS_CHANGED");
	if (after.gcs != before.gcs || after.tco2_cnt != before.tco2_cnt ||
	    after.tco_tmr != before.tco_tmr)
		b06wb_tco_fail("READ_ONLY_STATE_CHANGED");
	if (pci_io_read_config32(B06VY_LPC_DEV, PCI_VENDOR_ID) !=
		B06VY_LPC_ID ||
	    pci_io_read_config32(B06VY_LPC_DEV, RCBA) !=
		B06VY_RCBA_ENABLED ||
	    pci_io_read_config32(B06VY_LPC_DEV, B06WB_PMBASE_REG) !=
		B06WB_PMBASE_ENABLED ||
	    pci_io_read_config8(B06VY_LPC_DEV, B06WB_ACPI_CNTL_REG) !=
		B06WB_ACPI_DECODE_ENABLED ||
	    read32p(CONFIG_FIXED_RCBA_MMIO_BASE + GCS) != after.gcs ||
	    inw(B06WB_TCO_BASE + B06WB_TCO1_STS) != after.tco1_sts ||
	    inw(B06WB_TCO_BASE + B06WB_TCO2_STS) != after.tco2_sts ||
	    inw(B06WB_TCO_BASE + B06WB_TCO1_CNT) !=
		B06WB_TCO1_CNT_TARGET ||
	    inw(B06WB_TCO_BASE + B06WB_TCO2_CNT) != after.tco2_cnt ||
	    inw(B06WB_TCO_BASE + B06WB_TCO_TMR) != after.tco_tmr)
		b06wb_tco_fail("FINAL_STATE_RECHECK");

	b06wb_tco_ready = true;
	post_code(POST_B06WB_TCO_READY);
	printk(BIOS_NOTICE,
	       "[TCO] B06WB READY TCO1_CNT=0800 STATUS_PRESERVED=1 "
	       "GCS_WRITE=0 STATUS_WRITE=0 RELOAD_WRITE=0 TIMER_WRITE=0 "
	       "TCO_LOCK_WRITE=0\n");
}
#endif

#if CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM
static bool b06wc_pic_tuple_is_reset(uint8_t master_mask,
	uint8_t slave_mask, uint8_t elcr1, uint8_t elcr2)
{
	return master_mask == B06WC_PIC_MASTER_MASK_RESET &&
		slave_mask == B06WC_PIC_SLAVE_MASK_RESET &&
		elcr1 == B06WC_ELCR1_RESET && elcr2 == B06WC_ELCR2_RESET;
}

static uint32_t b06wc_read_eflags(void)
{
	uint32_t flags;

	asm volatile("pushfl; popl %0" : "=rm" (flags));
	return flags;
}

static bool b06wc_pic_tuple_is_target(uint8_t master_mask,
	uint8_t slave_mask, uint8_t elcr1, uint8_t elcr2)
{
	return master_mask == B06WC_PIC_MASTER_MASK_TARGET &&
		slave_mask == B06WC_PIC_SLAVE_MASK_TARGET &&
		elcr1 == B06WC_ELCR1_TARGET && elcr2 == B06WC_ELCR2_TARGET;
}

static __noreturn void b06wc_pic_fail(const char *reason)
{
	die_with_post_code(POST_B06WC_PIC_FAIL,
		"[PIC] B06WC fail-closed: %s\n", reason);
}

/*
 * Establish a deterministic PC/AT virtual-wire source for SeaBIOS.  The live
 * B06VP transaction proved this exact upstream sequence and a bounded PIT
 * one-shot proved that IRQ0 reaches the master PIC IRR.  Interrupt delivery is
 * still left disabled here; LAPIC ExtINT/NMI setup remains in the BSP device
 * init callback and SeaBIOS owns the first enabled interrupt window.
 */
static void b06wc_initialize_pic_once(void)
{
	const uint32_t eflags_before = b06wc_read_eflags();
	const uint8_t master_before = inb(MASTER_PIC_OCW1);
	const uint8_t slave_before = inb(SLAVE_PIC_OCW1);
	const uint8_t elcr1_before = inb(ELCR1);
	const uint8_t elcr2_before = inb(ELCR2);
	uint8_t master_after;
	uint8_t slave_after;
	uint8_t elcr1_after;
	uint8_t elcr2_after;

	if (b06wc_pic_attempted)
		b06wc_pic_fail("SECOND_ATTEMPT");
	b06wc_pic_attempted = true;
	if (!b06wb_tco_ready || b06vqi_baseline_attempted ||
	    b06vu_ahci_route_attempted || b06vy_ioapic_attempted ||
	    b06wa_hpet_attempted)
		b06wc_pic_fail("STAGE_ORDER");
	if (eflags_before & X86_EFLAGS_IF)
		b06wc_pic_fail("CPU_INTERRUPTS_ENABLED");
	if (!b06wc_pic_tuple_is_reset(master_before, slave_before,
		elcr1_before, elcr2_before) &&
	    !b06wc_pic_tuple_is_target(master_before, slave_before,
		elcr1_before, elcr2_before))
		b06wc_pic_fail("PRE_TUPLE_OUTSIDE_ALLOWLIST");

	printk(BIOS_NOTICE,
	       "[PIC] B06WC PRE IMR=%02x/%02x ELCR=%02x/%02x "
	       "EFLAGS=%08x ALLOWLIST=RESET,TARGET IF_ENABLE=0 EOI_WRITE=0\n",
	       master_before, slave_before, elcr1_before, elcr2_before,
	       eflags_before);
	post_code(POST_B06WC_PIC_BEGIN);
	setup_i8259();
	i8259_configure_irq_trigger(IRQ_9, IRQ_LEVEL_TRIGGERED);

	master_after = inb(MASTER_PIC_OCW1);
	slave_after = inb(SLAVE_PIC_OCW1);
	elcr1_after = inb(ELCR1);
	elcr2_after = inb(ELCR2);
	printk(BIOS_NOTICE,
	       "[PIC] B06WC POST IMR=%02x/%02x ELCR=%02x/%02x "
	       "VECTORS=20/28 IRQ9=LEVEL IRQ0=MASKED\n",
	       master_after, slave_after, elcr1_after, elcr2_after);
	if (!b06wc_pic_tuple_is_target(master_after, slave_after,
		elcr1_after, elcr2_after))
		b06wc_pic_fail("TARGET_READBACK");

	/* Re-read every readable byte immediately before releasing the stage. */
	if (inb(MASTER_PIC_OCW1) != B06WC_PIC_MASTER_MASK_TARGET ||
	    inb(SLAVE_PIC_OCW1) != B06WC_PIC_SLAVE_MASK_TARGET ||
	    inb(ELCR1) != B06WC_ELCR1_TARGET ||
	    inb(ELCR2) != B06WC_ELCR2_TARGET)
		b06wc_pic_fail("FINAL_STATE_RECHECK");

	b06wc_pic_ready = true;
	post_code(POST_B06WC_PIC_READY);
	printk(BIOS_NOTICE,
	       "[PIC] B06WC READY STANDARD_I8259=1 SCI_IRQ9_LEVEL=1 "
	       "DELIVERY_TESTED_IN_ROMMON=AVAILABLE PAYLOAD_OWNS_IRQS=1\n");
}

static __noreturn void b06wc_acpi_mode_fail(const char *reason)
{
	die_with_post_code(POST_B06WC_ACPI_FAIL,
		"[ACPI-MODE] B06WC fail-closed: %s\n", reason);
}

static bool b06wc_acpi_irq9_is_masked(void)
{
	uint32_t low = 0;
	uint32_t high = 0;
	bool readable;
	bool restored;

	if (!(inb(SLAVE_PIC_OCW1) & BIT(IRQ_9 - 8)))
		return false;
	readable = b06vy_ioapic_read(B06WC_IOAPIC_SCI_LOW_REG, &low) &&
		b06vy_ioapic_read(B06WC_IOAPIC_SCI_HIGH_REG, &high);
	restored = b06vy_ioapic_select(B06VY_IOAPIC_ID_REG);
	return readable && restored && low == B06VY_IOAPIC_LOW_TARGET &&
		high == B06VY_IOAPIC_HIGH_TARGET;
}

static bool b06wc_acpi_enables_are_quiescent(void)
{
	return inw(DEFAULT_PMBASE + PM1_EN) == 0 &&
		inl(DEFAULT_PMBASE + GPE0_EN) == 0 &&
		inl(DEFAULT_PMBASE + GPE0_EN + 4) == 0 &&
		inl(DEFAULT_PMBASE + SMI_EN) == 0 &&
		inw(DEFAULT_PMBASE + ALT_GP_SMI_EN) == 0 &&
		inw(DEFAULT_PMBASE + B06WC_UPRWC) == 0 &&
		pci_io_read_config32(B06VY_LPC_DEV, D31F0_GPIO_ROUT) == 0;
}

/*
 * Enter ACPI mode without enabling a single event source.  With NO_SMM there
 * is intentionally no APMC/SMI transition: firmware establishes SCI_EN
 * directly, while both possible IRQ9 delivery paths stay masked.  This makes
 * the board FADT's SMI_CMD=0 contract truthful before ACPI tables are emitted.
 */
static void b06wc_enter_quiet_acpi_mode_once(void)
{
	const uint32_t lpc_id = pci_io_read_config32(B06VY_LPC_DEV,
		PCI_VENDOR_ID);
	const uint32_t rcba = pci_io_read_config32(B06VY_LPC_DEV, RCBA);
	const uint32_t pmbase = pci_io_read_config32(B06VY_LPC_DEV,
		D31F0_PMBASE);
	const uint8_t acpi_cntl = pci_io_read_config8(B06VY_LPC_DEV,
		D31F0_ACPI_CNTL);
	const uint32_t eflags = b06wc_read_eflags();
	uint16_t pm1_sts;
	uint32_t pm1_cnt;
	bool status_write = false;
	bool control_write = false;

	if (b06wc_acpi_mode_attempted)
		b06wc_acpi_mode_fail("SECOND_ATTEMPT");
	b06wc_acpi_mode_attempted = true;
	if (!b06wb_tco_ready || !b06wc_pic_ready ||
	    !b06vqi_baseline_attempted || !b06vu_ahci_route_attempted ||
	    !b06vy_ioapic_attempted || !b06wa_hpet_ready)
		b06wc_acpi_mode_fail("STAGE_ORDER");
	if (lpc_id != B06VY_LPC_ID || rcba != B06VY_RCBA_ENABLED ||
	    pmbase != B06WB_PMBASE_ENABLED ||
	    acpi_cntl != B06WB_ACPI_DECODE_ENABLED)
		b06wc_acpi_mode_fail("LPC_PM_DECODE_IDENTITY");
	if (eflags & X86_EFLAGS_IF)
		b06wc_acpi_mode_fail("CPU_INTERRUPTS_ENABLED");
	if (!b06wc_pic_tuple_is_target(inb(MASTER_PIC_OCW1),
		inb(SLAVE_PIC_OCW1), inb(ELCR1), inb(ELCR2)))
		b06wc_acpi_mode_fail("PIC_ELCR_TARGET_CHANGED");
	if (!b06wc_acpi_irq9_is_masked())
		b06wc_acpi_mode_fail("SCI_DELIVERY_NOT_MASKED");
	if (!b06wc_acpi_enables_are_quiescent())
		b06wc_acpi_mode_fail("EVENT_ENABLE_OR_GPIO_ROUTE_ACTIVE");

	pm1_cnt = inl(DEFAULT_PMBASE + PM1_CNT);
	if (pm1_cnt != B06WC_PM1_CNT_DWORD_PRE &&
	    pm1_cnt != B06WC_PM1_CNT_DWORD_TARGET)
		b06wc_acpi_mode_fail("PM1_CNT_OUTSIDE_EXACT_ALLOWLIST");
	pm1_sts = inw(DEFAULT_PMBASE + PM1_STS);
	printk(BIOS_NOTICE,
	       "[ACPI-MODE] B06WC PRE PM1_STS=%04x PM1_CNT=%08x "
	       "PM1_EN=%04x GPE_EN=%08x:%08x SMI_EN=%08x ALT_EN=%04x "
	       "UPRWC=%04x GPIO_ROUT=%08x IRQ9_MASKED=1 EFLAGS=%08x\n",
	       pm1_sts, pm1_cnt, inw(DEFAULT_PMBASE + PM1_EN),
	       inl(DEFAULT_PMBASE + GPE0_EN + 4),
	       inl(DEFAULT_PMBASE + GPE0_EN),
	       inl(DEFAULT_PMBASE + SMI_EN),
	       inw(DEFAULT_PMBASE + ALT_GP_SMI_EN),
	       inw(DEFAULT_PMBASE + B06WC_UPRWC),
	       pci_io_read_config32(B06VY_LPC_DEV, D31F0_GPIO_ROUT), eflags);

	post_code(POST_B06WC_ACPI_BEGIN);
	if (pm1_sts & PRBTNOR_STS) {
		/* W1C only the observed unsafe power-button-override status. */
		outw(PRBTNOR_STS, DEFAULT_PMBASE + PM1_STS);
		status_write = true;
	}
	if (inw(DEFAULT_PMBASE + PM1_STS) & PRBTNOR_STS)
		b06wc_acpi_mode_fail("PRBTNOR_STS_DID_NOT_CLEAR");
	if (!b06wc_acpi_enables_are_quiescent() ||
	    !b06wc_acpi_irq9_is_masked())
		b06wc_acpi_mode_fail("QUIESCENT_STATE_CHANGED_AFTER_W1C");
	post_code(POST_B06WC_ACPI_STATUS);

	pm1_cnt = inl(DEFAULT_PMBASE + PM1_CNT);
	if (pm1_cnt == B06WC_PM1_CNT_DWORD_PRE) {
		const uint16_t target =
			(inw(DEFAULT_PMBASE + PM1_CNT) & ~B06WC_PM1_CNT_TARGET) |
			B06WC_PM1_CNT_TARGET;

		if (target != B06WC_PM1_CNT_TARGET)
			b06wc_acpi_mode_fail("PM1_CNT_TARGET_CONSTRUCTION");
		outw(target, DEFAULT_PMBASE + PM1_CNT);
		control_write = true;
	}
	if (inl(DEFAULT_PMBASE + PM1_CNT) != B06WC_PM1_CNT_DWORD_TARGET)
		b06wc_acpi_mode_fail("SCI_EN_EXACT_READBACK");
	if (!b06wc_acpi_enables_are_quiescent() ||
	    !b06wc_acpi_irq9_is_masked())
		b06wc_acpi_mode_fail("FINAL_QUIESCENT_STATE_RECHECK");
	if (b06wc_read_eflags() & X86_EFLAGS_IF)
		b06wc_acpi_mode_fail("FINAL_CPU_INTERRUPTS_ENABLED");
	if (pci_io_read_config32(B06VY_LPC_DEV, PCI_VENDOR_ID) !=
		B06VY_LPC_ID ||
	    pci_io_read_config32(B06VY_LPC_DEV, RCBA) !=
		B06VY_RCBA_ENABLED ||
	    pci_io_read_config32(B06VY_LPC_DEV, D31F0_PMBASE) !=
		B06WB_PMBASE_ENABLED ||
	    pci_io_read_config8(B06VY_LPC_DEV, D31F0_ACPI_CNTL) !=
		B06WB_ACPI_DECODE_ENABLED)
		b06wc_acpi_mode_fail("FINAL_DECODE_IDENTITY_RECHECK");

	b06wc_acpi_mode_is_ready = true;
	post_code(POST_B06WC_ACPI_READY);
	printk(BIOS_NOTICE,
	       "[ACPI-MODE] B06WC READY PM1_STS=%04x PM1_CNT=00000001 "
	       "PRBTNOR_W1C=%u SCI_EN_WRITE=%u EVENT_ENABLE_WRITE=0 "
	       "SMI_WRITE=0 IRQ_ROUTE_WRITE=0 SCI_DELIVERY_PROVEN=0\n",
	       inw(DEFAULT_PMBASE + PM1_STS), (unsigned int)status_write,
	       (unsigned int)control_write);
}

bool b06wc_acpi_mode_ready(void)
{
	return b06wc_acpi_mode_is_ready;
}
#endif

#if CONFIG_X58_PRO_E_B06WA_HPET_DECODE
struct b06wa_hpet_counter_snapshot {
	uint32_t low;
	uint32_t high;
};

static struct b06wa_hpet_counter_snapshot b06wa_read_hpet_counter(void)
{
	return (struct b06wa_hpet_counter_snapshot) {
		.low = read32p(B06WA_HPET_BASE + B06WA_HPET_MAIN_COUNTER_LOW),
		.high = read32p(B06WA_HPET_BASE + B06WA_HPET_MAIN_COUNTER_HIGH),
	};
}

static void b06wa_hpet_fail(const char *reason)
{
	die_with_post_code(POST_B06WA_HPET_FAIL,
		"[HPET] B06WA fail-closed: %s\n", reason);
}

/*
 * The sole B06WA hardware mutation is HPTC 00000000 -> 00000080.  The
 * allowlist, LPC identity, fixed RCBA and inherited IOAPIC-decode state are
 * all checked before that first HPTC write.  HPET registers remain read-only.
 */
static void b06wa_decode_and_gate_hpet_once(void)
{
	const uint32_t lpc_id = pci_io_read_config32(B06VY_LPC_DEV,
		PCI_VENDOR_ID);
	const uint32_t rcba = pci_io_read_config32(B06VY_LPC_DEV, RCBA);
	uint32_t hptc;
	uint32_t cap_low;
	uint32_t cap_high;
	uint32_t config_low;
	uint32_t config_high;
	uint32_t interrupt_status_low;
	uint32_t interrupt_status_high;
	uint32_t timer0_config_low;
	uint32_t timer0_config_high;
	struct b06wa_hpet_counter_snapshot counter_first;
	struct b06wa_hpet_counter_snapshot counter_second;
	bool decode_write = false;

	if (b06wa_hpet_attempted)
		b06wa_hpet_fail("SECOND_ATTEMPT");
	b06wa_hpet_attempted = true;
	if (!b06vy_ioapic_attempted)
		b06wa_hpet_fail("B06VY_STAGE_NOT_RUN");
	if (lpc_id != B06VY_LPC_ID || rcba != B06VY_RCBA_ENABLED)
		b06wa_hpet_fail("LPC_RCBA_IDENTITY");
	if (read8p(CONFIG_FIXED_RCBA_MMIO_BASE + OIC) != B06VY_OIC_ENABLED)
		b06wa_hpet_fail("INHERITED_OIC_STATE");

	hptc = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_HPTC);
	if (hptc != B06WA_HPTC_DISABLED && hptc != B06WA_HPTC_FED00000)
		b06wa_hpet_fail("HPTC_OUTSIDE_EXACT_ALLOWLIST");
	if (hptc == B06WA_HPTC_DISABLED &&
	    (read32p(B06WA_HPET_BASE + B06WA_HPET_CAP_ID_LOW) != UINT32_MAX ||
	     read32p(B06WA_HPET_BASE + B06WA_HPET_CAP_ID_HIGH) != UINT32_MAX))
		b06wa_hpet_fail("DISABLED_APERTURE_NOT_OPEN_BUS");

	printk(BIOS_NOTICE,
	       "[HPET] B06WA PRE LPC_ID=%08x RCBA=%08x OIC=03 HPTC=%08x "
	       "ALLOWLIST=00000000,00000080\n",
	       lpc_id, rcba, hptc);
	post_code(POST_B06WA_HPET_BEGIN);
	if (hptc == B06WA_HPTC_DISABLED) {
		const uint32_t target =
			(hptc & ~B06WA_HPTC_CONTROL_MASK) | B06WA_HPTC_FED00000;

		if (target != B06WA_HPTC_FED00000)
			b06wa_hpet_fail("HPTC_TARGET_CONSTRUCTION");
		write32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_HPTC, target);
		decode_write = true;
	}
	hptc = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_HPTC);
	if (hptc != B06WA_HPTC_FED00000)
		b06wa_hpet_fail("HPTC_EXACT_READBACK");
	post_code(POST_B06WA_HPET_DECODED);

	cap_low = read32p(B06WA_HPET_BASE + B06WA_HPET_CAP_ID_LOW);
	cap_high = read32p(B06WA_HPET_BASE + B06WA_HPET_CAP_ID_HIGH);
	config_low = read32p(B06WA_HPET_BASE + B06WA_HPET_GENERAL_CONFIG_LOW);
	config_high = read32p(B06WA_HPET_BASE + B06WA_HPET_GENERAL_CONFIG_HIGH);
	interrupt_status_low = read32p(B06WA_HPET_BASE +
		B06WA_HPET_GENERAL_INTERRUPT_STATUS_LOW);
	interrupt_status_high = read32p(B06WA_HPET_BASE +
		B06WA_HPET_GENERAL_INTERRUPT_STATUS_HIGH);
	timer0_config_low = read32p(B06WA_HPET_BASE +
		B06WA_HPET_TIMER0_CONFIG_LOW);
	timer0_config_high = read32p(B06WA_HPET_BASE +
		B06WA_HPET_TIMER0_CONFIG_HIGH);
	counter_first = b06wa_read_hpet_counter();
	udelay(B06WA_HPET_COUNTER_STABILITY_DELAY_US);
	counter_second = b06wa_read_hpet_counter();

	printk(BIOS_NOTICE,
	       "[HPET] B06WA POST HPTC=%08x WRITE=%u GCAP=%08x:%08x "
	       "GENCFG=%08x:%08x ISR=%08x:%08x T0CFG=%08x:%08x "
	       "COUNTER_A=%08x:%08x COUNTER_B=%08x:%08x\n",
	       hptc, (unsigned int)decode_write, cap_high, cap_low,
	       config_high, config_low, interrupt_status_high,
	       interrupt_status_low, timer0_config_high, timer0_config_low,
	       counter_first.high, counter_first.low,
	       counter_second.high, counter_second.low);
	if (cap_low != B06WA_HPET_CAP_ID_LOW_TARGET ||
	    cap_high != B06WA_HPET_CAP_ID_HIGH_TARGET)
		b06wa_hpet_fail("GCAP_ID_TUPLE");
	if (config_low != 0 || config_high != 0)
		b06wa_hpet_fail("GENERAL_CONFIG_NOT_ZERO");
	if (interrupt_status_low != 0 || interrupt_status_high != 0)
		b06wa_hpet_fail("GENERAL_INTERRUPT_STATUS_NOT_ZERO");
	if (timer0_config_low & B06WA_HPET_TIMER_INTERRUPT_ENABLE)
		b06wa_hpet_fail("TIMER0_INTERRUPT_ENABLED");
	if (counter_first.low != counter_second.low ||
	    counter_first.high != counter_second.high)
		b06wa_hpet_fail("MAIN_COUNTER_NOT_STOPPED");
	if (read32p(B06WA_HPET_BASE + B06WA_HPET_GENERAL_CONFIG_LOW) != 0 ||
	    read32p(B06WA_HPET_BASE + B06WA_HPET_GENERAL_CONFIG_HIGH) != 0 ||
	    read32p(B06WA_HPET_BASE +
		B06WA_HPET_GENERAL_INTERRUPT_STATUS_LOW) != 0 ||
	    read32p(B06WA_HPET_BASE +
		B06WA_HPET_GENERAL_INTERRUPT_STATUS_HIGH) != 0 ||
	    (read32p(B06WA_HPET_BASE + B06WA_HPET_TIMER0_CONFIG_LOW) &
		B06WA_HPET_TIMER_INTERRUPT_ENABLE) != 0 ||
	    read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_HPTC) !=
		B06WA_HPTC_FED00000 ||
	    pci_io_read_config32(B06VY_LPC_DEV, RCBA) != B06VY_RCBA_ENABLED ||
	    pci_io_read_config32(B06VY_LPC_DEV, PCI_VENDOR_ID) != B06VY_LPC_ID)
		b06wa_hpet_fail("FINAL_STATE_RECHECK");

	b06wa_hpet_ready = true;
	post_code(POST_B06WA_HPET_READY);
	printk(BIOS_NOTICE,
	       "[HPET] B06WA READY BASE=fed00000 SIZE=00000400 STOPPED=1 "
	       "TIMER0_IRQ=0 ROUTE_WRITE=0 COUNTER_WRITE=0 ACPI_TABLE=0\n");
}
#endif

#if CONFIG_X58_PRO_E_B06VQ_PLATRO1
struct b06vqp_fixed_range {
	const char *name;
	uint64_t base;
	uint64_t top;
	/* Reserve only ranges whose decode is proven, or architecturally fixed. */
	bool reserved;
};

static bool b06vqp_ranges_overlap(uint64_t left_base, uint64_t left_top,
	uint64_t right_base, uint64_t right_top)
{
	return left_base < right_top && right_base < left_top;
}

static void b06vqp_log_lpc_state(void)
{
	printk(BIOS_NOTICE,
	       "[PLATRO] LPC ID=%08x PMBASE=%08x ACPI=%02x GPIOBASE=%08x "
	       "GPIOCNTL=%02x SERIRQ=%02x RCBA=%08x\n",
	       pci_io_read_config32(B06VQP_LPC_DEV, PCI_VENDOR_ID),
	       pci_io_read_config32(B06VQP_LPC_DEV, D31F0_PMBASE),
	       pci_io_read_config8(B06VQP_LPC_DEV, D31F0_ACPI_CNTL),
	       pci_io_read_config32(B06VQP_LPC_DEV, GPIOBASE),
	       pci_io_read_config8(B06VQP_LPC_DEV, D31F0_GPIO_CNTL),
	       pci_io_read_config8(B06VQP_LPC_DEV, D31F0_SERIRQ_CNTL),
	       pci_io_read_config32(B06VQP_LPC_DEV, RCBA));
	printk(BIOS_NOTICE,
	       "[PLATRO] LPC IO_DEC=%04x EN=%04x GEN=%08x/%08x/%08x/%08x\n",
	       pci_io_read_config16(B06VQP_LPC_DEV, LPC_IO_DEC),
	       pci_io_read_config16(B06VQP_LPC_DEV, LPC_EN),
	       pci_io_read_config32(B06VQP_LPC_DEV, LPC_GEN1_DEC),
	       pci_io_read_config32(B06VQP_LPC_DEV, LPC_GEN2_DEC),
	       pci_io_read_config32(B06VQP_LPC_DEV, LPC_GEN3_DEC),
	       pci_io_read_config32(B06VQP_LPC_DEV, LPC_GEN4_DEC));
	printk(BIOS_NOTICE,
	       "[PLATRO] PIRQ A=%02x B=%02x C=%02x D=%02x E=%02x F=%02x G=%02x H=%02x\n",
	       pci_io_read_config8(B06VQP_LPC_DEV, PIRQA_ROUT),
	       pci_io_read_config8(B06VQP_LPC_DEV, PIRQB_ROUT),
	       pci_io_read_config8(B06VQP_LPC_DEV, PIRQC_ROUT),
	       pci_io_read_config8(B06VQP_LPC_DEV, PIRQD_ROUT),
	       pci_io_read_config8(B06VQP_LPC_DEV, PIRQE_ROUT),
	       pci_io_read_config8(B06VQP_LPC_DEV, PIRQF_ROUT),
	       pci_io_read_config8(B06VQP_LPC_DEV, PIRQG_ROUT),
	       pci_io_read_config8(B06VQP_LPC_DEV, PIRQH_ROUT));
}

static void b06vqp_log_pm_state(void)
{
	const uint32_t id = pci_io_read_config32(B06VQP_LPC_DEV,
		PCI_VENDOR_ID);
	const uint32_t pmbase_reg = pci_io_read_config32(B06VQP_LPC_DEV,
		D31F0_PMBASE);
	const uint8_t acpi_cntl = pci_io_read_config8(B06VQP_LPC_DEV,
		D31F0_ACPI_CNTL);
	const uint16_t pmbase = DEFAULT_PMBASE;
	uint32_t first;
	uint32_t last;
	uint32_t changes = 0;

	if (id != B06VQP_LPC_ID || pmbase_reg != (DEFAULT_PMBASE | 1) ||
	    acpi_cntl != 0x80) {
		printk(BIOS_ERR,
		       "[PLATRO] PM unavailable ID=%08x PMBASE=%08x ACPI=%02x\n",
		       id, pmbase_reg, acpi_cntl);
		return;
	}

	printk(BIOS_NOTICE,
	       "[PLATRO] PM PM1_STS=%04x PM1_EN=%04x PM1_CNT=%08x "
	       "GPE_STS=%08x:%08x GPE_EN=%08x:%08x SMI_EN=%08x "
	       "SMI_STS=%08x ALT=%04x/%04x\n",
	       inw(pmbase + PM1_STS), inw(pmbase + PM1_EN),
	       inl(pmbase + PM1_CNT),
	       inl(pmbase + GPE0_STS + 4), inl(pmbase + GPE0_STS),
	       inl(pmbase + GPE0_EN + 4), inl(pmbase + GPE0_EN),
	       inl(pmbase + SMI_EN), inl(pmbase + SMI_STS),
	       inw(pmbase + ALT_GP_SMI_STS), inw(pmbase + ALT_GP_SMI_EN));

	first = inl(pmbase + PM1_TMR) & B06VQP_PM_TIMER_MASK;
	last = first;
	for (uint32_t i = 0; i < B06VQP_PM_TIMER_SAMPLES; i++) {
		const uint32_t sample = inl(pmbase + PM1_TMR) &
			B06VQP_PM_TIMER_MASK;

		if (sample != last)
			changes++;
		last = sample;
	}
	printk(BIOS_NOTICE,
	       "[PLATRO] PMTMR BASE=%04x FIRST=%06x LAST=%06x DELTA=%06x "
	       "CHANGES=%08x SAMPLES=%08x MOVING=%u\n",
	       pmbase, first, last, (last - first) & B06VQP_PM_TIMER_MASK,
	       changes, B06VQP_PM_TIMER_SAMPLES, changes != 0);
}

static void b06vqp_log_rcba_hpet_state(void)
{
	const uint32_t id = pci_io_read_config32(B06VQP_LPC_DEV,
		PCI_VENDOR_ID);
	const uint32_t rcba_reg = pci_io_read_config32(B06VQP_LPC_DEV, RCBA);
	uint8_t oic;
	uint32_t hptc;
	uintptr_t hpet_base;
	uint32_t first;
	uint32_t last;
	uint32_t changes = 0;

	if (id != B06VQP_LPC_ID ||
	    rcba_reg != (CONFIG_FIXED_RCBA_MMIO_BASE | 1)) {
		printk(BIOS_ERR,
		       "[PLATRO] RCBA unavailable ID=%08x RCBA=%08x MMIO_SKIPPED=1\n",
		       id, rcba_reg);
		return;
	}
	oic = read8p(CONFIG_FIXED_RCBA_MMIO_BASE + OIC);
	hptc = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_HPTC);
	printk(BIOS_NOTICE, "[PLATRO] RCBA OIC=%02x HPTC=%08x\n", oic,
	       hptc);
	if (!(hptc & B06VQP_HPET_DECODE_ENABLE)) {
		printk(BIOS_NOTICE,
		       "[PLATRO] HPET DECODE=0 MMIO_SKIPPED=1\n");
		return;
	}

	hpet_base = B06VQP_HPET_BASE0 +
		(hptc & B06VQP_HPET_ADDRESS_MASK) * B06VQP_HPET_STRIDE;
	first = read32p(hpet_base + B06VQP_HPET_MAIN_COUNTER);
	last = first;
	for (uint32_t i = 0; i < B06VQP_PM_TIMER_SAMPLES; i++) {
		const uint32_t sample = read32p(hpet_base +
			B06VQP_HPET_MAIN_COUNTER);

		if (sample != last)
			changes++;
		last = sample;
	}
	printk(BIOS_NOTICE,
	       "[PLATRO] HPET BASE=%08lx CAP=%08x:%08x CFG=%08x:%08x "
	       "FIRST=%08x LAST=%08x DELTA=%08x CHANGES=%08x ENABLE=%u\n",
	       (unsigned long)hpet_base,
	       read32p(hpet_base + B06VQP_HPET_CAP_ID + 4),
	       read32p(hpet_base + B06VQP_HPET_CAP_ID),
	       read32p(hpet_base + B06VQP_HPET_GEN_CFG + 4),
	       read32p(hpet_base + B06VQP_HPET_GEN_CFG),
	       first, last, last - first, changes,
	       !!(read32p(hpet_base + B06VQP_HPET_GEN_CFG) &
		  B06VQP_HPET_COUNTER_ENABLE));
}

static void b06vqp_log_resource_state(void)
{
	const uint32_t lpc_id = pci_io_read_config32(B06VQP_LPC_DEV,
		PCI_VENDOR_ID);
	const uint32_t pmbase_reg = pci_io_read_config32(B06VQP_LPC_DEV,
		D31F0_PMBASE);
	const uint8_t acpi_cntl = pci_io_read_config8(B06VQP_LPC_DEV,
		D31F0_ACPI_CNTL);
	const uint32_t gpiobase_reg = pci_io_read_config32(B06VQP_LPC_DEV,
		GPIOBASE);
	const uint8_t gpio_cntl = pci_io_read_config8(B06VQP_LPC_DEV,
		D31F0_GPIO_CNTL);
	const uint32_t rcba_reg = pci_io_read_config32(B06VQP_LPC_DEV, RCBA);
	const uint32_t smbus_id = pci_io_read_config32(B06VQP_SMBUS_DEV,
		PCI_VENDOR_ID);
	const uint32_t smbus_bar4 = pci_io_read_config32(B06VQP_SMBUS_DEV,
		B06VQP_SMBUS_BAR4);
	const uint16_t smbus_command = pci_io_read_config16(B06VQP_SMBUS_DEV,
		PCI_COMMAND);
	const uint8_t smbus_hostc = pci_io_read_config8(B06VQP_SMBUS_DEV,
		HOSTC);
	const bool lpc_valid = lpc_id == B06VQP_LPC_ID;
	const bool rcba_valid = lpc_valid &&
		rcba_reg == (CONFIG_FIXED_RCBA_MMIO_BASE | 1);
	const bool pmbase_valid = lpc_valid &&
		pmbase_reg == (DEFAULT_PMBASE | 1) && acpi_cntl == 0x80;
	const bool gpiobase_valid = lpc_valid &&
		gpiobase_reg == (DEFAULT_GPIOBASE | 1) && gpio_cntl == 0x10;
	const bool smbus_valid = smbus_id == B06VQP_SMBUS_ID &&
		smbus_bar4 == (B06VQP_SMBUS_IO_BASE | 1) &&
		(smbus_command & PCI_COMMAND_IO) && (smbus_hostc & HST_EN);
	const uint32_t hptc = rcba_valid ?
		read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_HPTC) : 0;
	const bool hpet_valid = rcba_valid &&
		(hptc & B06VQP_HPET_DECODE_ENABLE);
	const struct b06vqp_fixed_range fixed_io[] = {
		{ "SMBUS", B06VQP_SMBUS_IO_BASE,
		  B06VQP_SMBUS_IO_BASE + B06VQP_SMBUS_IO_SIZE, smbus_valid },
		{ "PM", DEFAULT_PMBASE,
		  DEFAULT_PMBASE + B06VQP_PM_IO_SIZE, pmbase_valid },
		{ "GPIO", DEFAULT_GPIOBASE,
		  DEFAULT_GPIOBASE + B06VQP_GPIO_IO_SIZE, gpiobase_valid },
	};
	const uint64_t hpet_base = B06VQP_HPET_BASE0 +
		(hptc & B06VQP_HPET_ADDRESS_MASK) * B06VQP_HPET_STRIDE;
	const struct b06vqp_fixed_range fixed_mmio[] = {
		{ "IOAPIC", B06VQP_IOAPIC_BASE,
		  B06VQP_IOAPIC_BASE + B06VQP_APIC_SIZE, true },
		{ "HPET", hpet_base, hpet_base + B06VQP_HPET_SIZE, hpet_valid },
		{ "RCBA", CONFIG_FIXED_RCBA_MMIO_BASE,
		  CONFIG_FIXED_RCBA_MMIO_BASE + B06VQP_RCBA_SIZE, rcba_valid },
		{ "LAPIC", B06VQP_LAPIC_BASE,
		  B06VQP_LAPIC_BASE + B06VQP_APIC_SIZE, true },
		{ "ROM16M", B06VQP_ROM_BASE, B06VQP_ROM_TOP, true },
	};
	unsigned int collisions = 0;

	for (unsigned int index = 0;
	     index < B06VQP_DOMAIN_RESOURCE_COUNT; index++) {
		const struct resource *resource = probe_resource(b06vn_domain,
			index);

		if (resource == NULL) {
			printk(BIOS_ERR,
			       "[PLATRO] RESOURCE index=%u MISSING=1\n", index);
			continue;
		}
		printk(BIOS_NOTICE,
		       "[PLATRO] RESOURCE index=%u BASE=%llx SIZE=%llx "
		       "LIMIT=%llx FLAGS=%lx\n",
		       index, resource->base, resource->size, resource->limit,
		       resource->flags);
	}

	for (size_t i = 0; i < ARRAY_SIZE(fixed_io); i++) {
		const bool overlap = fixed_io[i].reserved &&
			b06vqp_ranges_overlap(fixed_io[i].base, fixed_io[i].top,
				B06VN_PCI_IO_BASE, B06VN_PCI_IO_TOP);

		collisions += overlap;
		printk(BIOS_NOTICE,
		       "[PLATRO] FIXED_IO %s=%llx-%llx RESERVED=%u PCI_WINDOW_OVERLAP=%u\n",
		       fixed_io[i].name, fixed_io[i].base,
		       fixed_io[i].top - 1, fixed_io[i].reserved, overlap);
	}
	for (size_t i = 0; i < ARRAY_SIZE(fixed_mmio); i++) {
		const bool pci_overlap = fixed_mmio[i].reserved &&
			b06vqp_ranges_overlap(
			fixed_mmio[i].base, fixed_mmio[i].top,
			B06VN_PCI_MMIO_BASE, B06VN_PCI_MMIO_TOP);
		const bool ecam_overlap = fixed_mmio[i].reserved &&
			b06vqp_ranges_overlap(
			fixed_mmio[i].base, fixed_mmio[i].top,
			B06VN_ECAM_BASE, B06VN_ECAM_TOP);

		collisions += pci_overlap + ecam_overlap;
		printk(BIOS_NOTICE,
		       "[PLATRO] FIXED_MMIO %s=%llx-%llx RESERVED=%u PCI_OVERLAP=%u "
		       "ECAM_OVERLAP=%u\n",
		       fixed_mmio[i].name, fixed_mmio[i].base,
		       fixed_mmio[i].top - 1, fixed_mmio[i].reserved,
		       pci_overlap, ecam_overlap);
	}
	printk(BIOS_NOTICE,
	       "[PLATRO] FIXED_RANGE_COLLISIONS=%u PLATFORM_RESOURCES_MODELLED=0\n",
	       collisions);
}

static void b06vqp_log_platform_census_once(void)
{
	if (b06vqp_platform_census_emitted) {
		printk(BIOS_ERR,
		       "[PLATRO] duplicate census suppressed without changing boot flow\n");
		return;
	}
	b06vqp_platform_census_emitted = true;
	printk(BIOS_NOTICE,
	       "[PLATRO] BEGIN %s TARGET_STATE_READ_ONLY=1\n",
	       B06VN_BUILD_ID);
	b06vqp_log_lpc_state();
	b06vqp_log_pm_state();
	b06vqp_log_rcba_hpet_state();
	b06vqp_log_resource_state();
	printk(BIOS_NOTICE, "[PLATRO] END NON_FATAL=1 IOAPIC_MMIO_SKIPPED=1\n");
}
#endif

#if CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP
static uint16_t b06vr_sata_map_target(const uint16_t before)
{
	(void)before;
	return I82801JX_SATA_MAP_AHCI_D31F2_VALUE;
}

static void b06vr_log_sata_state(const char *phase)
{
	printk(BIOS_NOTICE,
	       "[SATA] B06VR %s F2_ID=%08x F2_CLASSREV=%08x F2_CMD=%04x "
	       "F2_HDR=%02x MAP=%04x BAR5=%08x F5_ID=%08x F5_CLASSREV=%08x "
	       "F5_CMD=%04x F5_HDR=%02x\n",
	       phase,
	       pci_io_read_config32(B06VR_SATA1_DEV, PCI_VENDOR_ID),
	       pci_io_read_config32(B06VR_SATA1_DEV, PCI_CLASS_REVISION),
	       pci_io_read_config16(B06VR_SATA1_DEV, PCI_COMMAND),
	       pci_io_read_config8(B06VR_SATA1_DEV, PCI_HEADER_TYPE),
	       pci_io_read_config16(B06VR_SATA1_DEV, I82801JX_SATA_MAP),
	       pci_io_read_config32(B06VR_SATA1_DEV, I82801JX_SATA_ABAR),
	       pci_io_read_config32(B06VR_SATA2_DEV, PCI_VENDOR_ID),
	       pci_io_read_config32(B06VR_SATA2_DEV, PCI_CLASS_REVISION),
	       pci_io_read_config16(B06VR_SATA2_DEV, PCI_COMMAND),
	       pci_io_read_config8(B06VR_SATA2_DEV, PCI_HEADER_TYPE));
}

static bool b06vr_sata_is_quiescent(const pci_devfn_t dev)
{
	return !(pci_io_read_config16(dev, PCI_COMMAND) &
		(PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER));
}

static bool b06vr_sata_legacy_prestate(const uint16_t map)
{
	return map == 0 &&
		pci_io_read_config32(B06VR_SATA1_DEV, PCI_VENDOR_ID) ==
			B06VR_SATA1_IDE_ID &&
		(pci_io_read_config32(B06VR_SATA1_DEV, PCI_CLASS_REVISION) >> 16) ==
			B06VR_IDE_CLASS &&
		(pci_io_read_config8(B06VR_SATA1_DEV, PCI_HEADER_TYPE) & 0x7f) ==
			PCI_HEADER_TYPE_NORMAL &&
		b06vr_sata_is_quiescent(B06VR_SATA1_DEV) &&
		pci_io_read_config32(B06VR_SATA2_DEV, PCI_VENDOR_ID) ==
			B06VR_SATA2_IDE_ID &&
		(pci_io_read_config32(B06VR_SATA2_DEV, PCI_CLASS_REVISION) >> 16) ==
			B06VR_IDE_CLASS &&
		(pci_io_read_config8(B06VR_SATA2_DEV, PCI_HEADER_TYPE) & 0x7f) ==
			PCI_HEADER_TYPE_NORMAL &&
		b06vr_sata_is_quiescent(B06VR_SATA2_DEV);
}

static bool b06vr_sata_ahci_state(const uint16_t map)
{
	return map == I82801JX_SATA_MAP_AHCI_D31F2_VALUE &&
		pci_io_read_config32(B06VR_SATA1_DEV, PCI_VENDOR_ID) ==
			B06VR_SATA1_AHCI_ID &&
		(pci_io_read_config32(B06VR_SATA1_DEV, PCI_CLASS_REVISION) >> 8) ==
			B06VR_AHCI_CLASS_PI &&
		(pci_io_read_config8(B06VR_SATA1_DEV, PCI_HEADER_TYPE) & 0x7f) ==
			PCI_HEADER_TYPE_NORMAL &&
		b06vr_sata_is_quiescent(B06VR_SATA1_DEV) &&
		pci_io_read_config32(B06VR_SATA2_DEV, PCI_VENDOR_ID) == 0xffffffffu;
}

/*
 * Intel ICH10 section 14.1.30 permits this mode change during POST. Admit
 * only the measured reset topology or this image's exact retained topology;
 * the normal complete root allowlist is checked immediately afterwards.
 */
static void b06vr_program_ahci_map_once(void)
{
	const uint16_t before = pci_io_read_config16(B06VR_SATA1_DEV,
		I82801JX_SATA_MAP);
	const uint16_t target = b06vr_sata_map_target(before);
	uint16_t after;
	bool wrote = false;

	if (b06vr_ahci_map_attempted)
		die_with_post_code(POST_B06VR_AHCI_FAIL,
			"[SATA] B06VR refused a second AHCI MAP attempt\n");
	b06vr_ahci_map_attempted = true;
	b06vr_log_sata_state("PRE");

	if (!b06vr_sata_legacy_prestate(before) &&
	    !b06vr_sata_ahci_state(before))
		die_with_post_code(POST_B06VR_AHCI_FAIL,
			"[SATA] B06VR prestate is outside IDE-reset/AHCI-retained allowlist\n");

	post_code(POST_B06VR_AHCI_BEGIN);
	if (before != target) {
		i82801jx_sata_select_ahci(B06VR_SATA1_DEV);
		wrote = true;
	}
	after = pci_io_read_config16(B06VR_SATA1_DEV, I82801JX_SATA_MAP);
	b06vr_log_sata_state("POST");
	if (after != target || !b06vr_sata_ahci_state(after) ||
	    (wrote && pci_io_read_config32(B06VR_SATA1_DEV,
		I82801JX_SATA_ABAR) != 0))
		die_with_post_code(POST_B06VR_AHCI_FAIL,
			"[SATA] B06VR AHCI MAP identity/readback gate failed: "
			"after=%04x target=%04x\n", after, target);

	post_code(POST_B06VR_AHCI_OK);
	printk(BIOS_NOTICE,
	       "[SATA] B06VR AHCI MAP/type-transition gate PASS (write=%u); "
	       "ports/clocks/AHCI MMIO untouched; BAR5 cleared per ICH10 section 14.1.16\n",
	       (unsigned int)wrote);
}
#endif

#if CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE
struct b06vu_sata_snapshot {
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

static struct b06vu_sata_snapshot b06vu_read_sata_snapshot(void)
{
	struct b06vu_sata_snapshot snapshot = {
		.lpc_id = pci_io_read_config32(B06VQI_LPC_DEV, PCI_VENDOR_ID),
		.rcba = pci_io_read_config32(B06VQI_LPC_DEV, RCBA),
	};

	/* Never touch RCBA MMIO until its PCI identity and decode are exact. */
	if (snapshot.lpc_id != B06VQI_LPC_ID ||
	    snapshot.rcba != B06VQI_RCBA_ENABLED)
		return snapshot;

	snapshot.fdsw = read8p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_FDSW);
	snapshot.fd = read32p(CONFIG_FIXED_RCBA_MMIO_BASE + RCBA_FD);
	snapshot.f2_id = pci_io_read_config32(B06VR_SATA1_DEV, PCI_VENDOR_ID);
	snapshot.f2_classrev = pci_io_read_config32(B06VR_SATA1_DEV,
		PCI_CLASS_REVISION);
	snapshot.f2_command = pci_io_read_config16(B06VR_SATA1_DEV,
		PCI_COMMAND);
	snapshot.f2_header = pci_io_read_config8(B06VR_SATA1_DEV,
		PCI_HEADER_TYPE);
	snapshot.f2_bar5 = pci_io_read_config32(B06VR_SATA1_DEV,
		I82801JX_SATA_ABAR);
	snapshot.f2_map = pci_io_read_config16(B06VR_SATA1_DEV,
		I82801JX_SATA_MAP);
	snapshot.f2_pcs = pci_io_read_config16(B06VR_SATA1_DEV,
		I82801JX_SATA_PCS);
	snapshot.f2_sclkcg = pci_io_read_config32(B06VR_SATA1_DEV,
		I82801JX_SATA_SCLKCG);
	snapshot.f5_id = pci_io_read_config32(B06VR_SATA2_DEV, PCI_VENDOR_ID);
	snapshot.f5_classrev = pci_io_read_config32(B06VR_SATA2_DEV,
		PCI_CLASS_REVISION);
	snapshot.f5_command = pci_io_read_config16(B06VR_SATA2_DEV,
		PCI_COMMAND);
	snapshot.f5_header = pci_io_read_config8(B06VR_SATA2_DEV,
		PCI_HEADER_TYPE);
	snapshot.f5_bar5 = pci_io_read_config32(B06VR_SATA2_DEV,
		I82801JX_SATA_ABAR);
	snapshot.f5_map = pci_io_read_config16(B06VR_SATA2_DEV,
		I82801JX_SATA_MAP);

	return snapshot;
}

static void b06vu_log_sata_snapshot(const char *phase,
	const struct b06vu_sata_snapshot *snapshot)
{
	printk(BIOS_NOTICE,
	       "[SATA] B06VU %s LPC_ID=%08x RCBA=%08x FDSW=%02x FD=%08x "
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

static bool b06vu_fixed_gates_are_exact(
	const struct b06vu_sata_snapshot *snapshot)
{
	return snapshot->lpc_id == B06VQI_LPC_ID &&
		snapshot->rcba == B06VQI_RCBA_ENABLED &&
		snapshot->fdsw == B06VQI_FDSW_UNLOCKED;
}

static bool b06vu_f2_reset_state_is_exact(
	const struct b06vu_sata_snapshot *snapshot)
{
	return snapshot->f2_id == B06VR_SATA1_IDE_ID &&
		snapshot->f2_classrev == B06VU_SATA1_IDE_CLASSREV &&
		snapshot->f2_command == B06VU_RESET_COMMAND &&
		snapshot->f2_header == B06VU_RESET_HEADER &&
		snapshot->f2_bar5 == B06VU_RESET_BAR5 &&
		snapshot->f2_map == B06VU_RESET_MAP &&
		snapshot->f2_pcs == B06VU_RESET_PCS &&
		snapshot->f2_sclkcg == B06VU_RESET_SCLKCG;
}

static bool b06vu_f5_reset_state_is_exact(
	const struct b06vu_sata_snapshot *snapshot)
{
	return snapshot->f5_id == B06VR_SATA2_IDE_ID &&
		snapshot->f5_classrev == B06VU_SATA2_IDE_CLASSREV &&
		snapshot->f5_command == B06VU_RESET_COMMAND &&
		snapshot->f5_header == B06VU_RESET_HEADER &&
		snapshot->f5_bar5 == B06VU_RESET_BAR5 &&
		snapshot->f5_map == B06VU_RESET_MAP;
}

static bool b06vu_f2_ahci_state_is_exact(
	const struct b06vu_sata_snapshot *snapshot)
{
	return snapshot->f2_id == B06VR_SATA1_AHCI_ID &&
		snapshot->f2_classrev == B06VU_SATA1_AHCI_CLASSREV &&
		snapshot->f2_command == B06VU_RESET_COMMAND &&
		snapshot->f2_header == B06VU_RESET_HEADER &&
		snapshot->f2_bar5 == 0 &&
		snapshot->f2_map == I82801JX_SATA_MAP_AHCI_D31F2_VALUE &&
		snapshot->f2_pcs == B06VU_RESET_PCS &&
		snapshot->f2_sclkcg == B06VU_RESET_SCLKCG;
}

static bool b06vu_raw_reset_topology_is_exact(
	const struct b06vu_sata_snapshot *snapshot)
{
	return b06vu_fixed_gates_are_exact(snapshot) &&
		snapshot->fd == B06VU_FD_BASELINE &&
		b06vu_f2_reset_state_is_exact(snapshot) &&
		b06vu_f5_reset_state_is_exact(snapshot);
}

static bool b06vu_mapped_before_hide_is_exact(
	const struct b06vu_sata_snapshot *snapshot)
{
	return b06vu_fixed_gates_are_exact(snapshot) &&
		snapshot->fd == B06VU_FD_BASELINE &&
		b06vu_f2_ahci_state_is_exact(snapshot) &&
		b06vu_f5_reset_state_is_exact(snapshot);
}

static bool b06vu_final_route_is_exact(
	const struct b06vu_sata_snapshot *snapshot)
{
	return b06vu_fixed_gates_are_exact(snapshot) &&
		snapshot->fd == B06VU_FD_SATA2_DISABLED &&
		b06vu_f2_ahci_state_is_exact(snapshot) &&
		snapshot->f5_id == 0xffffffffu &&
		snapshot->f5_classrev == 0xffffffffu &&
		snapshot->f5_command == 0xffffu &&
		snapshot->f5_header == 0xffu &&
		snapshot->f5_bar5 == 0xffffffffu &&
		snapshot->f5_map == 0xffffu;
}

/*
 * A live B06VP CAR transaction proved that MAP=0060 changes F2 to AHCI but
 * leaves F5 visible. A separate FD_SAD2 write then hid F5 without changing
 * F2. Preserve that causal ordering and reject every non-reset input tuple.
 */
static void b06vu_program_ahci_route_once(void)
{
	struct b06vu_sata_snapshot snapshot;

	if (b06vu_ahci_route_attempted)
		die_with_post_code(POST_B06VU_ROUTE_FAIL,
			"[SATA] B06VU refused a second AHCI-route attempt\n");
	b06vu_ahci_route_attempted = true;

	snapshot = b06vu_read_sata_snapshot();
	b06vu_log_sata_snapshot("RESET_PRE", &snapshot);
	if (!b06vu_raw_reset_topology_is_exact(&snapshot))
		die_with_post_code(POST_B06VU_ROUTE_FAIL,
			"[SATA] B06VU rejected non-reset F2/F5 or non-baseline FD tuple\n");
	post_code(POST_B06VU_RESET_TOPOLOGY_OK);
	printk(BIOS_NOTICE,
	       "[SATA] B06VU exact dual-IDE reset topology PASS after ICHBASE1\n");

	post_code(POST_B06VU_MAP_BEGIN);
	printk(BIOS_NOTICE,
	       "[SATA] B06VU MAP_STAGE begin: MAP[7:5]=011b, then mandatory BAR5 clear\n");
	i82801jx_sata_select_ahci(B06VR_SATA1_DEV);
	snapshot = b06vu_read_sata_snapshot();
	b06vu_log_sata_snapshot("MAP_POST", &snapshot);
	if (!b06vu_mapped_before_hide_is_exact(&snapshot))
		die_with_post_code(POST_B06VU_ROUTE_FAIL,
			"[SATA] B06VU MAP/BAR5 intermediate readback failed\n");
	post_code(POST_B06VU_MAP_OK);
	printk(BIOS_NOTICE,
	       "[SATA] B06VU MAP/BAR5 gate PASS; F5 remains visible as measured\n");

	post_code(POST_B06VU_FD_SAD2_BEGIN);
	printk(BIOS_NOTICE,
	       "[SATA] B06VU FD_SAD2_STAGE begin: set only RCBA FD bit25\n");
	i82801jx_disable_sata2();
	snapshot = b06vu_read_sata_snapshot();
	b06vu_log_sata_snapshot("FD_SAD2_POST", &snapshot);
	if (!b06vu_final_route_is_exact(&snapshot))
		die_with_post_code(POST_B06VU_ROUTE_FAIL,
			"[SATA] B06VU FD_SAD2/F5-absence complete readback failed\n");
	post_code(POST_B06VU_FD_SAD2_OK);
	printk(BIOS_NOTICE,
	       "[SATA] B06VU corrected AHCI route PASS; FD=02000001 FDSW=00 F5=absent; "
	       "no FDSW lock, PCS/SCLKCG/AHCI MMIO write\n");
}
#endif

static void b06vn_log_iou(const char *phase, unsigned int dev)
{
	const uint16_t link_status = b06vn_ecam_read16(dev,
		B06VN_PCIE_LNKSTA);
	uint32_t expected_id;

	switch (dev) {
	case B06VN_IOU2_X4_DEV:
		expected_id = B06VN_IOU2_X4_ID;
		break;
	case B06VN_IOU0_X16_DEV:
		expected_id = B06VN_IOU0_X16_ID;
		break;
	case B06VN_AUX_X16_DEV:
		expected_id = B06VN_AUX_X16_ID;
		break;
	default:
		expected_id = 0xffffffffu;
		break;
	}

	printk(BIOS_NOTICE,
	       "[PCIE] %s 00:%02x.0 ID=%08x EXPECT=%08x 09c=%08x 0a0=%08x "
	       "0a2=%04x 0aa=%04x 0c0=%08x 190=%08x "
	       "SPEED=%u WIDTH=%u LT=%u DLLA=%u\n",
	       phase, dev, b06vn_ecam_read32(dev, PCI_VENDOR_ID), expected_id,
	       b06vn_ecam_read32(dev, B06VN_PCIE_LNKCAP),
	       b06vn_ecam_read32(dev, B06VN_PCIE_LNKCTLSTA), link_status,
	       b06vn_ecam_read16(dev, B06VN_PCIE_AA),
	       b06vn_ecam_read32(dev, B06VN_PCIE_C0),
	       b06vn_ecam_read32(dev, B06VN_PCIE_PRTX_BIF_CTRL),
	       (unsigned int)(link_status & B06VN_LNKSTA_SPEED_MASK),
	       (unsigned int)(link_status & B06VN_LNKSTA_WIDTH_MASK) >>
		B06VN_LNKSTA_WIDTH_SHIFT,
	       (unsigned int)!!(link_status & PCI_EXP_LNKSTA_LT),
	       (unsigned int)!!(link_status & B06VN_LNKSTA_DLL_ACTIVE));
}

/*
 * Intel X58 datasheet section 5.1.2 requires BIOS to release an IOU from
 * Wait_on_BIOS through PCIE_PRTx_BIF_CTRL.  B06VN changes one hypothesis at
 * a time: only IOU0 / 00:03.0 is started.  Ports 00:01.0 and 00:07.0 are
 * deliberately read-only telemetry.  The action field is observed as zero
 * after a vendor boot, so its readback is diagnostic rather than an equality
 * gate.  No standard Link Control retrain is issued here.
 */
static void b06vn_start_iou0_once(void)
{
	uint16_t link_status;
	unsigned int polls;
	bool wrote_start = false;

	if (b06vn_iou0_start_attempted)
		die_with_post_code(POST_B06VN_PLATFORM_FAIL,
			"[PCIE] B06VN refused a second IOU0 start attempt\n");
	if (b06vn_ecam_read32(B06VN_IOU0_X16_DEV, PCI_VENDOR_ID) !=
	    B06VN_IOU0_X16_ID)
		die_with_post_code(POST_B06VN_PLATFORM_FAIL,
			"[PCIE] B06VN IOU0 identity gate failed\n");
	b06vn_iou0_start_attempted = true;

	b06vn_log_iou("PRE", B06VN_IOU2_X4_DEV);
	b06vn_log_iou("PRE", B06VN_IOU0_X16_DEV);
	b06vn_log_iou("PRE", B06VN_AUX_X16_DEV);

	link_status = b06vn_ecam_read16(B06VN_IOU0_X16_DEV,
		B06VN_PCIE_LNKSTA);
	post_code(POST_B06VN_IOU0_START);
	if (!(link_status & B06VN_LNKSTA_DLL_ACTIVE)) {
		write16p(B06VN_ECAM_DEV0(B06VN_IOU0_X16_DEV,
			B06VN_PCIE_PRTX_BIF_CTRL), B06VN_IOU0_X16_START);
		wrote_start = true;
		printk(BIOS_NOTICE,
		       "[PCIE] BIF_START 00:03.0 ECAM=e0018190 VALUE=000c\n");
	} else {
		printk(BIOS_NOTICE,
		       "[PCIE] BIF_START 00:03.0 skipped: DLL already active\n");
	}

	for (polls = 0; polls < B06VN_LINK_POLL_COUNT; polls++) {
		link_status = b06vn_ecam_read16(B06VN_IOU0_X16_DEV,
			B06VN_PCIE_LNKSTA);
		if ((link_status & B06VN_LNKSTA_DLL_ACTIVE) &&
		    !(link_status & PCI_EXP_LNKSTA_LT))
			break;
		udelay(B06VN_LINK_POLL_US);
	}

	post_code(POST_B06VN_IOU0_POLLED);
	b06vn_log_iou("POST", B06VN_IOU2_X4_DEV);
	b06vn_log_iou("POST", B06VN_IOU0_X16_DEV);
	b06vn_log_iou("POST", B06VN_AUX_X16_DEV);
	if (polls == B06VN_LINK_POLL_COUNT)
		printk(BIOS_WARNING,
		       "[PCIE] IOU0 link timeout after 1000000 us; continuing to serial payload fallback\n");
	else
		printk(BIOS_NOTICE,
		       "[PCIE] IOU0 link ready after %u polls (start-write=%u)\n",
		       polls, (unsigned int)wrote_start);
}

static const struct b06vn_expected_pci *b06vn_expected_for_device(
	const struct device *dev)
{
	unsigned int root_devfn;
	bool downstream;

	if (b06vn_domain == NULL || dev == NULL ||
	    dev->path.type != DEVICE_PATH_PCI || dev->upstream == NULL)
		return NULL;

	if (dev->upstream == b06vn_domain->downstream) {
		root_devfn = dev->path.pci.devfn;
		downstream = false;
	} else if (dev->upstream->dev != NULL &&
		   dev->upstream->dev->path.type == DEVICE_PATH_PCI &&
		   dev->upstream->dev->upstream == b06vn_domain->downstream) {
		root_devfn = dev->upstream->dev->path.pci.devfn;
		downstream = true;
	} else {
		return NULL;
	}

	for (size_t i = 0; i < ARRAY_SIZE(b06vn_expected); i++) {
		const struct b06vn_expected_pci *expected = &b06vn_expected[i];

		if (expected->root_devfn == root_devfn &&
		    expected->devfn == dev->path.pci.devfn &&
		    expected->downstream == downstream)
			return expected;
	}

	return NULL;
}

static struct device *b06vn_device_for_expected(
	const struct b06vn_expected_pci *expected)
{
	struct device *root;

	if (b06vn_domain == NULL || b06vn_domain->downstream == NULL)
		return NULL;
	root = pcidev_path_behind(b06vn_domain->downstream,
		expected->root_devfn);
	if (!expected->downstream)
		return root;
	if (root == NULL || root->downstream == NULL)
		return NULL;
	return pcidev_path_behind(root->downstream, expected->devfn);
}

static bool b06vn_id_matches(const struct b06vn_expected_pci *expected,
	uint32_t id)
{
	if (expected->runtime_optional)
		return (id & 0xffffu) == expected->id;
	return id == expected->id;
}

static bool b06vn_optional_device_absent(
	const struct b06vn_expected_pci *expected, const struct device *dev)
{
	return expected->runtime_optional && dev != NULL && !dev->enabled;
}

static void b06vn_report_optional_gpu_absent(void)
{
	if (b06vn_gpu_absence_reported)
		return;
	b06vn_gpu_absence_reported = true;
	post_code(POST_B06VN_GPU_FALLBACK);
	printk(BIOS_WARNING,
	       "[PCI] GPU_PROBE root=00:03.0 RESULT=ABSENT_SERIAL_SEABIOS_FALLBACK\n");
}

static void b06vn_require_identity(struct device *dev)
{
	const struct b06vn_expected_pci *expected =
		b06vn_expected_for_device(dev);
	const uint32_t id = ((uint32_t)dev->device << 16) | dev->vendor;
	const uint32_t classrev = pci_read_config32(dev, PCI_CLASS_REVISION);

	if (expected == NULL || !dev->enabled || !dev->mandatory ||
	    !b06vn_id_matches(expected, id) ||
	    (dev->class >> 8) != expected->class_code ||
	    (classrev >> 8) != dev->class ||
	    (dev->hdr_type & 0x7f) != expected->header_type ||
	    (expected->revision >= 0 &&
	     (classrev & 0xff) != (uint8_t)expected->revision))
		die_with_post_code(POST_B06VN_IDENTITY_FAIL,
			"[RAMSTAGE] B06VN rejected PCI function %s ID=%08x CLASSREV=%08x HDR=%02x\n",
			expected ? expected->name : "outside allowlist", id,
			classrev, dev->hdr_type);
}

static void b06vn_clear_command(struct device *dev)
{
	const uint16_t decode = PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
		PCI_COMMAND_MASTER;
	const uint16_t command = pci_read_config16(dev, PCI_COMMAND);

	pci_write_config16(dev, PCI_COMMAND, command & ~decode);
	if (pci_read_config16(dev, PCI_COMMAND) & decode)
		die_with_post_code(POST_B06VN_COMMAND_FAIL,
			"[RAMSTAGE] B06VN could not clear decode/master on %s\n",
			dev_path(dev));
	dev->command &= ~PCI_COMMAND_MASTER;
}

static void b06vn_probe_gate(struct device *dev)
{
	b06vn_require_identity(dev);
	b06vn_clear_command(dev);
}

#if CONFIG_X58_PRO_E_B06VO_IOHBUSNO_ROUTE
/*
 * do_pci_scan_bridge() has already assigned/programmed the bridge bus numbers
 * before invoking this callback.  Sample root 00:03.0's secondary 00.0 by
 * both legacy CF8 and direct PCIEXBAR at 0, 1, 10 and 100 ms, then invoke the
 * unchanged generic scanner.  This callback performs no configuration write.
 */
static void b06vo_scan_bus_with_endpoint_probe(struct bus *bus,
	unsigned int min_devfn, unsigned int max_devfn)
{
	static const unsigned int sample_delta_us[B06VO_ENDPOINT_SAMPLE_COUNT] = {
		0, 1000, 9000, 90000,
	};
	unsigned int elapsed_us = 0;

	if (bus != NULL && bus->dev != NULL &&
	    bus->dev->path.type == DEVICE_PATH_PCI &&
	    bus->dev->path.pci.devfn == PCI_DEVFN(B06VN_IOU0_X16_DEV, 0)) {
		if (bus->secondary == 0 || bus->secondary == 0xff)
			die_with_post_code(POST_B06VN_BUS_FAIL,
				"[PCIE] B06VO root03 secondary bus is invalid\n");

		for (size_t i = 0; i < ARRAY_SIZE(sample_delta_us); i++) {
			uint32_t cf8_id;
			uint32_t ecam_id;

			if (sample_delta_us[i] != 0)
				udelay(sample_delta_us[i]);
			elapsed_us += sample_delta_us[i];
			cf8_id = pci_io_read_config32(
				PCI_DEV(bus->secondary, B06VO_PEG_ENDPOINT_DEV, 0),
				PCI_VENDOR_ID);
			ecam_id = b06vo_ecam_read_id(bus->secondary,
				B06VO_PEG_ENDPOINT_DEV, 0);
			printk(BIOS_NOTICE,
			       "[PCIE] B06VO PEG_PROBE T_US=%u BUS=%02x "
			       "CF8=%08x ECAM=%08x MATCH=%u\n",
			       elapsed_us, bus->secondary, cf8_id, ecam_id,
			       (unsigned int)(cf8_id == ecam_id));
		}
	}

	pci_scan_bus(bus, min_devfn, max_devfn);
}
#endif

static void b06vn_scan_bridge(struct device *dev)
{
	const struct b06vn_expected_pci *expected =
		b06vn_expected_for_device(dev);

	b06vn_require_identity(dev);
	if (expected == NULL || !expected->bridge)
		die_with_post_code(POST_B06VN_TOPOLOGY_FAIL,
			"[RAMSTAGE] B06VN refused non-allowlisted bridge scan\n");
#if CONFIG_X58_PRO_E_B06VO_IOHBUSNO_ROUTE
	do_pci_scan_bridge(dev, b06vo_scan_bus_with_endpoint_probe);
#else
	pci_scan_bridge(dev);
#endif
}

static void b06vn_require_static_topology(void)
{
	struct device *root;
	size_t root_count = 0;
	size_t downstream_count = 0;

	if (b06vn_domain == NULL || b06vn_domain->downstream == NULL)
		die_with_post_code(POST_B06VN_TOPOLOGY_FAIL,
			"[RAMSTAGE] B06VN domain has no static downstream bus\n");

	for (root = b06vn_domain->downstream->children; root;
	     root = root->sibling) {
		const struct b06vn_expected_pci *expected =
			b06vn_expected_for_device(root);

		root_count++;
		if (expected == NULL || expected->downstream || !root->enabled ||
		    !root->mandatory ||
		    root->ops != (expected->bridge ? &b06vn_root_port_ops :
						&b06vn_endpoint_ops))
			die_with_post_code(POST_B06VN_TOPOLOGY_FAIL,
				"[RAMSTAGE] B06VN root devicetree escaped allowlist\n");

		if (expected->bridge) {
			struct device *child;
			const struct b06vn_expected_pci *child_expected;

			if (root->downstream == NULL ||
			    root->downstream->children == NULL ||
			    root->downstream->children->sibling != NULL)
				die_with_post_code(POST_B06VN_TOPOLOGY_FAIL,
					"[RAMSTAGE] B06VN bridge child shape mismatch\n");
			child = root->downstream->children;
			child_expected = b06vn_expected_for_device(child);
			downstream_count++;
			if (child_expected == NULL ||
			    (!child->enabled && !child_expected->runtime_optional) ||
			    !child->mandatory ||
			    child->ops != &b06vn_endpoint_ops)
				die_with_post_code(POST_B06VN_TOPOLOGY_FAIL,
					"[RAMSTAGE] B06VN bridge child escaped allowlist\n");
		} else if (root->downstream != NULL) {
			die_with_post_code(POST_B06VN_TOPOLOGY_FAIL,
				"[RAMSTAGE] B06VN endpoint unexpectedly owns a bus\n");
		}
	}

	if (root_count != B06VN_ROOT_FUNCTIONS ||
	    downstream_count != B06VN_DOWNSTREAM_FUNCTIONS)
		die_with_post_code(POST_B06VN_TOPOLOGY_FAIL,
			"[RAMSTAGE] B06VN static PCI function count mismatch\n");
}

static void b06vn_raw_root_preflight(void)
{
	const uint16_t decode = PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
		PCI_COMMAND_MASTER;

	/* Validate the complete root allowlist before normal PCI programming. */
	for (size_t i = 0; i < ARRAY_SIZE(b06vn_expected); i++) {
		const struct b06vn_expected_pci *expected = &b06vn_expected[i];
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
			die_with_post_code(POST_B06VN_IDENTITY_FAIL,
				"[RAMSTAGE] B06VN root preflight rejected %s\n",
				expected->name);
	}

	/* Only after every root identity passed may their decodes be quiesced. */
	for (size_t i = 0; i < ARRAY_SIZE(b06vn_expected); i++) {
		const struct b06vn_expected_pci *expected = &b06vn_expected[i];
		pci_devfn_t raw;
		uint16_t command;

		if (expected->downstream)
			continue;
		raw = PCI_DEV(0, PCI_SLOT(expected->root_devfn),
			PCI_FUNC(expected->root_devfn));
		command = pci_io_read_config16(raw, PCI_COMMAND);
		pci_io_write_config16(raw, PCI_COMMAND, command & ~decode);
		if (pci_io_read_config16(raw, PCI_COMMAND) & decode)
			die_with_post_code(POST_B06VN_COMMAND_FAIL,
				"[RAMSTAGE] B06VN root command clear failed for %s\n",
				expected->name);
	}
}

static void b06vn_require_bridge_routes(void)
{
	uint8_t secondary[2];
	size_t count = 0;

	for (size_t i = 0; i < ARRAY_SIZE(b06vn_expected); i++) {
		const struct b06vn_expected_pci *expected = &b06vn_expected[i];
		struct device *dev;
		uint32_t buses;

		if (!expected->bridge)
			continue;
		dev = b06vn_device_for_expected(expected);
		if (dev == NULL || dev->downstream == NULL || count >= 2)
			die_with_post_code(POST_B06VN_BUS_FAIL,
				"[RAMSTAGE] B06VN bridge route is missing\n");
		buses = pci_read_config32(dev, PCI_PRIMARY_BUS);
		secondary[count++] = (buses >> 8) & 0xff;
		if ((buses & 0xff) != 0 || secondary[count - 1] == 0 ||
		    secondary[count - 1] == 0xff ||
		    ((buses >> 16) & 0xff) != secondary[count - 1] ||
		    dev->downstream->secondary != secondary[count - 1] ||
		    dev->downstream->subordinate != secondary[count - 1])
			die_with_post_code(POST_B06VN_BUS_FAIL,
				"[RAMSTAGE] B06VN bridge bus-number readback failed\n");
	}
	if (count != 2 || secondary[0] == secondary[1])
		die_with_post_code(POST_B06VN_BUS_FAIL,
			"[RAMSTAGE] B06VN bridge buses are not distinct\n");
}

static void b06vn_require_enumerated_topology(void)
{
	b06vn_require_static_topology();
	for (size_t i = 0; i < ARRAY_SIZE(b06vn_expected); i++) {
		const struct b06vn_expected_pci *expected = &b06vn_expected[i];
		struct device *dev = b06vn_device_for_expected(expected);

		if (dev == NULL)
			die_with_post_code(POST_B06VN_TOPOLOGY_FAIL,
				"[RAMSTAGE] B06VN selected function disappeared\n");
		if (b06vn_optional_device_absent(expected, dev)) {
			b06vn_report_optional_gpu_absent();
			continue;
		}
		b06vn_require_identity(dev);
		if (pci_read_config16(dev, PCI_COMMAND) &
		    (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER))
			die_with_post_code(POST_B06VN_COMMAND_FAIL,
				"[RAMSTAGE] B06VN selected function not quiescent\n");
	}
	b06vn_require_bridge_routes();
}

static void b06vn_domain_scan_bus(struct device *dev)
{
	if (dev != b06vn_domain)
		die_with_post_code(POST_B06VN_TOPOLOGY_FAIL,
			"[RAMSTAGE] B06VN unexpected PCI domain\n");

	post_code(POST_B06VN_PREFLIGHT);
	printk(BIOS_NOTICE, "[RAMSTAGE] %s selective PCI preflight\n",
	       B06VN_BUILD_ID);
	b06vn_require_platform_state("pre-scan");
	b06vn_require_static_topology();
#if CONFIG_X58_PRO_E_B06WB_TCO_HALT
	b06wb_halt_tco_once();
#endif
#if CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM
	b06wc_initialize_pic_once();
#endif
#if CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE
	b06vqi_program_baseline_once();
	b06vu_program_ahci_route_once();
#if CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK
	b06vy_decode_and_mask_ioapic_once();
#endif
#if CONFIG_X58_PRO_E_B06WA_HPET_DECODE
	b06wa_decode_and_gate_hpet_once();
#endif
#if CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM
	b06wc_enter_quiet_acpi_mode_once();
#endif
#if CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI
	b06wi_program_vendor_irq_once();
#endif
#else
#if CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP
	b06vr_program_ahci_map_once();
#endif
#endif
	b06vn_raw_root_preflight();
#if CONFIG_X58_PRO_E_B06VQ_ICHBASE1 && \
	!CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE
	b06vqi_program_baseline_once();
#endif
#if CONFIG_X58_PRO_E_B06VO_IOHBUSNO_ROUTE
	b06vo_program_ioh_bus_number_once();
#endif
#if CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT
	b06vq_program_ehci_once();
#endif
	post_code(POST_B06VN_ROOTS_SAFE);
	b06vn_start_iou0_once();

	pci_host_bridge_scan_bus(dev);
	b06vn_require_enumerated_topology();
	post_code(POST_B06VN_SCAN_OK);
	for (size_t i = 0; i < ARRAY_SIZE(b06vn_expected); i++) {
		const struct b06vn_expected_pci *expected = &b06vn_expected[i];
		struct device *gpu;

		if (!expected->runtime_optional)
			continue;
		gpu = b06vn_device_for_expected(expected);
		if (b06vn_optional_device_absent(expected, gpu))
			continue;
		post_code(POST_B06VN_GPU_PRESENT);
		printk(BIOS_NOTICE,
		       "[PCI] GPU_PROBE root=00:03.0 ID=%04x:%04x CLASS=%06x HDR=%02x ROMBAR=%08x RESULT=AMD_VGA_PRESENT_PHYSICAL_VBIOS\n",
		       gpu->vendor, gpu->device, gpu->class, gpu->hdr_type,
		       pci_read_config32(gpu, PCI_ROM_ADDRESS));
	}
	printk(BIOS_NOTICE,
	       "[RAMSTAGE] B06VN strict roots and required endpoints accepted; optional GPU policy applied\n");
}

static void b06vn_read_resources(struct device *dev)
{
	b06vn_require_platform_state("resource-map");
	b06vn_require_enumerated_topology();

	/* Legacy VGA/option-ROM layout retained from the B06VL payload path. */
	ram_range(dev, 0, 0x00000000, 0x000a0000);
	mmio_range(dev, 1, 0x000a0000, 0x00020000);
	reserved_ram_range(dev, 2, 0x000c0000, 0x00040000);

	/* Exact gated one-DIMM map: 3 GiB low plus 1 GiB remapped above 4 GiB. */
	ram_from_to(dev, 3, B06VN_LOW_RAM_BASE, B06VN_LOW_RAM_TOP);
	ram_from_to(dev, 4, B06VN_HIGH_RAM_BASE, B06VN_HIGH_RAM_TOP);

	/* The allocator receives no subtractive or above-4G PCI window. */
	domain_io_window_from_to(dev, 5, B06VN_PCI_IO_BASE,
		B06VN_PCI_IO_TOP);
	domain_mem_window_from_to(dev, 6, B06VN_PCI_MMIO_BASE,
		B06VN_PCI_MMIO_TOP);
	mmio_from_to(dev, 7, B06VN_ECAM_BASE, B06VN_ECAM_TOP);

	post_code(POST_B06VN_RESOURCES);
	printk(BIOS_NOTICE,
	       "[RAMSTAGE] B06VN RAM=0-640K,1M-3G,4G-5G PCI_IO=1000-ffff PCI_MMIO=c0000000-dfffffff\n");
}

#if CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES
static void b06vz_read_resources(struct device *dev)
{
	if (!b06vy_ioapic_attempted)
		die_with_post_code(POST_B06VN_RESOURCE_FAIL,
			"[RESOURCE] B06VZ inherited B06VY IOAPIC stage did not run\n");

	b06vn_read_resources(dev);

	/* Resource metadata only: every decode was established by an earlier stage. */
	fixed_io_range_reserved(dev, B06VZ_SMBUS_RESOURCE_INDEX,
		B06VZ_SMBUS_IO_BASE, B06VZ_SMBUS_IO_SIZE);
	fixed_io_range_reserved(dev, B06VZ_PM_RESOURCE_INDEX,
		B06VZ_PM_IO_BASE, B06VZ_PM_IO_SIZE);
	fixed_io_range_reserved(dev, B06VZ_GPIO_RESOURCE_INDEX,
		B06VZ_GPIO_IO_BASE, B06VZ_GPIO_IO_SIZE);
	mmio_range(dev, B06VZ_IOAPIC_RESOURCE_INDEX,
		B06VZ_IOAPIC_BASE, B06VZ_IOAPIC_SIZE);
	mmio_range(dev, B06VZ_RCBA_RESOURCE_INDEX,
		B06VZ_RCBA_BASE, B06VZ_RCBA_SIZE);
	mmio_range(dev, B06VZ_LAPIC_RESOURCE_INDEX,
		B06VZ_LAPIC_BASE, B06VZ_LAPIC_SIZE);
	mmio_range(dev, B06VZ_ROM_RESOURCE_INDEX,
		B06VZ_ROM_BASE, B06VZ_ROM_SIZE);

	printk(BIOS_NOTICE,
	       "[RESOURCE] B06VZ fixed reservations 8..14 installed; allocator apertures unchanged\n");
}
#endif

#if CONFIG_X58_PRO_E_B06WA_HPET_DECODE
static void b06wa_read_resources(struct device *dev)
{
	if (!b06wa_hpet_ready)
		die_with_post_code(POST_B06VN_RESOURCE_FAIL,
			"[RESOURCE] B06WA HPET decode/identity gate did not finish\n");

	b06vz_read_resources(dev);
	mmio_range(dev, B06WA_HPET_RESOURCE_INDEX,
		B06WA_HPET_BASE, B06WA_HPET_SIZE);
	printk(BIOS_NOTICE,
	       "[RESOURCE] B06WA HPET reservation 15 installed; B06VZ resources preserved\n");
}
#endif

#if CONFIG_X58_PRO_E_B06WB_TCO_HALT
static void b06wb_read_resources(struct device *dev)
{
	if (!b06wb_tco_ready)
		die_with_post_code(POST_B06VN_RESOURCE_FAIL,
			"[RESOURCE] B06WB exact TCO halt gate did not finish\n");

	b06wa_read_resources(dev);
	printk(BIOS_NOTICE,
	       "[RESOURCE] B06WB preserves B06WA exact resources 0..15 unchanged\n");
}
#endif

static void b06vn_require_domain_resource(unsigned long index, uint64_t base,
	uint64_t top, unsigned long required_flags)
{
	const struct resource *resource = probe_resource(b06vn_domain, index);
	bool bounds_exact;

	if (resource != NULL && (resource->flags & IORESOURCE_FIXED))
		bounds_exact = resource->base == base &&
			resource->size == top - base;
	else
		bounds_exact = resource != NULL && resource->base == base &&
			resource->limit == top - 1;

	if (!bounds_exact ||
	    (resource->flags & required_flags) != required_flags)
		die_with_post_code(POST_B06VN_RESOURCE_FAIL,
			"[RAMSTAGE] B06VN domain resource %lx mismatch\n", index);
}

#if CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES
static bool b06vz_domain_resource_bounds_are_exact(
	const struct resource *resource,
	const struct b06vz_domain_resource_contract *expected)
{
	if (resource->base != expected->base || resource->index != expected->index)
		return false;
	if (expected->flags & IORESOURCE_FIXED)
		return resource->size == expected->top - expected->base &&
			resource_end(resource) == expected->top - 1;

	return resource->size == 0 && resource->limit == expected->top - 1;
}

static void b06vz_require_exact_domain_resources(void)
{
	const struct resource *resource;
	size_t actual_count = 0;

	for (resource = b06vn_domain->resource_list; resource;
	     resource = resource->next)
		actual_count++;
	if (actual_count != B06VZ_DOMAIN_RESOURCE_COUNT &&
	    actual_count != B06VZ_FULL_DOMAIN_RESOURCE_COUNT)
		die_with_post_code(POST_B06VN_RESOURCE_FAIL,
			"[RESOURCE] B06VZ domain count=%zu outside base/full contract\n",
			actual_count);

	for (size_t i = 0; i < ARRAY_SIZE(b06vz_domain_resource_contracts); i++) {
		const struct b06vz_domain_resource_contract *expected =
			&b06vz_domain_resource_contracts[i];

		resource = probe_resource(b06vn_domain, expected->index);
		if (resource == NULL || resource->flags != expected->flags ||
		    !b06vz_domain_resource_bounds_are_exact(resource, expected))
			die_with_post_code(POST_B06VN_RESOURCE_FAIL,
				"[RESOURCE] B06VZ exact domain resource %lx mismatch\n",
				expected->index);
	}

	for (size_t i = 0; i < ARRAY_SIZE(b06vz_domain_resource_contracts); i++) {
		const struct b06vz_domain_resource_contract *left =
			&b06vz_domain_resource_contracts[i];
		const unsigned long left_type = left->flags & IORESOURCE_TYPE_MASK;

		for (size_t j = i + 1;
		     j < ARRAY_SIZE(b06vz_domain_resource_contracts); j++) {
			const struct b06vz_domain_resource_contract *right =
				&b06vz_domain_resource_contracts[j];
			const unsigned long right_type = right->flags &
				IORESOURCE_TYPE_MASK;

			if (left_type == right_type && left->base < right->top &&
			    right->base < left->top)
				die_with_post_code(POST_B06VN_OVERLAP_FAIL,
					"[RESOURCE] B06VZ overlapping domain resources %lx/%lx\n",
					left->index, right->index);
		}
	}
}

#if CONFIG_X58_PRO_E_B06WA_HPET_DECODE
static void b06wa_require_exact_hpet_resource(void)
{
	const struct resource *resource = probe_resource(b06vn_domain,
		B06WA_HPET_RESOURCE_INDEX);
	size_t actual_count = 0;

	for (const struct resource *entry = b06vn_domain->resource_list; entry;
	     entry = entry->next)
		actual_count++;
	if (actual_count != B06WA_DOMAIN_RESOURCE_COUNT)
		die_with_post_code(POST_B06VN_RESOURCE_FAIL,
			"[RESOURCE] B06WA domain count=%zu expected=16\n",
			actual_count);

	if (resource == NULL ||
	    resource->flags != b06wa_hpet_resource_contract.flags ||
	    !b06vz_domain_resource_bounds_are_exact(resource,
		&b06wa_hpet_resource_contract))
		die_with_post_code(POST_B06VN_RESOURCE_FAIL,
			"[RESOURCE] B06WA exact HPET resource 15 mismatch\n");

	for (size_t i = 0; i < ARRAY_SIZE(b06vz_domain_resource_contracts); i++) {
		const struct b06vz_domain_resource_contract *other =
			&b06vz_domain_resource_contracts[i];

		if ((other->flags & IORESOURCE_TYPE_MASK) == IORESOURCE_MEM &&
		    b06wa_hpet_resource_contract.base < other->top &&
		    other->base < b06wa_hpet_resource_contract.top)
			die_with_post_code(POST_B06VN_OVERLAP_FAIL,
				"[RESOURCE] B06WA HPET overlaps domain resource %lx\n",
				other->index);
	}
}
#endif
#endif

static void b06vn_require_allocated_resource(const struct device *dev,
	const struct resource *resource)
{
	uint64_t top;
	const unsigned long type = resource->flags &
		(IORESOURCE_IO | IORESOURCE_MEM);

	if (!resource->size || !type)
		return;
	if (type == (IORESOURCE_IO | IORESOURCE_MEM) ||
	    resource->base > UINT64_MAX - resource->size)
		die_with_post_code(POST_B06VN_RESOURCE_FAIL,
			"[RAMSTAGE] B06VN malformed resource on %s\n", dev_path(dev));
	top = resource->base + resource->size;
	if (!(resource->flags & IORESOURCE_ASSIGNED) ||
	    !(resource->flags & IORESOURCE_STORED) ||
	    (resource->flags & IORESOURCE_ABOVE_4G) ||
	    resource_end(resource) != top - 1 ||
	    (type == IORESOURCE_IO &&
	     (resource->base < B06VN_PCI_IO_BASE || top > B06VN_PCI_IO_TOP)) ||
	    (type == IORESOURCE_MEM &&
	     (resource->base < B06VN_PCI_MMIO_BASE ||
	      top > B06VN_PCI_MMIO_TOP)))
		die_with_post_code(POST_B06VN_RESOURCE_FAIL,
			"[RAMSTAGE] B06VN resource escaped aperture on %s\n",
			dev_path(dev));
}

#if CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS || \
	CONFIG_X58_PRO_E_B06VV_ICH10_PCS_SCLK
static struct device *b06vs_sata_device(void)
{
	if (b06vn_domain == NULL || b06vn_domain->downstream == NULL)
		return NULL;
	return pcidev_path_behind(b06vn_domain->downstream,
		PCI_DEVFN(0x1f, 2));
}

static bool b06vs_abar_resource_is_exact(const struct resource *abar,
	uint32_t raw_bar)
{
	uint64_t top;
	const unsigned long required = IORESOURCE_MEM | IORESOURCE_ASSIGNED |
		IORESOURCE_STORED;

	if (abar == NULL || abar->size != B06VS_ABAR_SIZE ||
	    abar->gran != B06VS_ABAR_GRANULARITY ||
	    abar->align < B06VS_ABAR_GRANULARITY ||
	    (abar->base & (B06VS_ABAR_SIZE - 1)) != 0 ||
	    abar->flags != required ||
	    abar->base > UINT64_MAX - abar->size)
		return false;
	top = abar->base + abar->size;

	return abar->base >= B06VN_PCI_MMIO_BASE &&
		top <= B06VN_PCI_MMIO_TOP &&
		resource_end(abar) == top - 1 &&
		(raw_bar & PCI_BASE_ADDRESS_MEM_ATTR_MASK) ==
			PCI_BASE_ADDRESS_SPACE_MEMORY &&
		(raw_bar & ~((uint32_t)PCI_BASE_ADDRESS_MEM_ATTR_MASK)) ==
			abar->base;
}

#if CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS
static bool b06vs_sata_state_is_exact(struct device *sata,
	const struct resource *abar)
{
	const uint16_t decode = PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
		PCI_COMMAND_MASTER;
	const uint32_t raw_bar = pci_io_read_config32(B06VR_SATA1_DEV,
		I82801JX_SATA_ABAR);

	return sata != NULL && sata->enabled && sata->mandatory &&
		pci_io_read_config32(B06VR_SATA1_DEV, PCI_VENDOR_ID) ==
			B06VR_SATA1_AHCI_ID &&
		(pci_io_read_config32(B06VR_SATA1_DEV,
			PCI_CLASS_REVISION) >> 8) == B06VR_AHCI_CLASS_PI &&
		(pci_io_read_config8(B06VR_SATA1_DEV, PCI_HEADER_TYPE) & 0x7f) ==
			PCI_HEADER_TYPE_NORMAL &&
		pci_io_read_config32(B06VR_SATA2_DEV, PCI_VENDOR_ID) ==
			0xffffffffu &&
		pci_io_read_config16(B06VR_SATA1_DEV, I82801JX_SATA_MAP) ==
			I82801JX_SATA_MAP_AHCI_D31F2_VALUE &&
		!(pci_io_read_config16(B06VR_SATA1_DEV, PCI_COMMAND) & decode) &&
		b06vs_abar_resource_is_exact(abar, raw_bar);
}

static void b06vs_log_sata_state(const char *phase, struct device *sata,
	const struct resource *abar)
{
	const uint16_t pcs = pci_io_read_config16(B06VR_SATA1_DEV,
		I82801JX_SATA_PCS);
	const uint64_t base = abar != NULL ? abar->base : 0;
	const uint64_t size = abar != NULL ? abar->size : 0;
	const unsigned long flags = abar != NULL ? abar->flags : 0;

	printk(BIOS_NOTICE,
	       "[SATA] B06VS %s F2_ID=%08x F2_CLASSREV=%08x F2_CMD=%04x "
	       "MAP=%04x PCS=%04x EN=%02x PRESENCE=%02x ORM=%u RSV14=%u "
	       "BAR5=%08x RES=%s BASE=%llx SIZE=%llx ALIGN=%u GRAN=%u "
	       "FLAGS=%08lx\n",
	       phase,
	       pci_io_read_config32(B06VR_SATA1_DEV, PCI_VENDOR_ID),
	       pci_io_read_config32(B06VR_SATA1_DEV, PCI_CLASS_REVISION),
	       pci_io_read_config16(B06VR_SATA1_DEV, PCI_COMMAND),
	       pci_io_read_config16(B06VR_SATA1_DEV, I82801JX_SATA_MAP), pcs,
	       pcs & I82801JX_SATA_PCS_PORT_ENABLE_MASK,
	       (pcs & I82801JX_SATA_PCS_PRESENCE_MASK) >> 8,
	       !!(pcs & I82801JX_SATA_PCS_OOB_RETRY_MODE),
	       !!(pcs & I82801JX_SATA_PCS_RESERVED_14),
	       pci_io_read_config32(B06VR_SATA1_DEV, I82801JX_SATA_ABAR),
	       sata != NULL ? dev_path(sata) : "missing",
	       (unsigned long long)base, (unsigned long long)size,
	       abar != NULL ? abar->align : 0,
	       abar != NULL ? abar->gran : 0, flags);
}

/*
 * Read-only presence samples are diagnostic, never admission criteria. The
 * deltas produce absolute samples at 0, 1, 10, 100 and 500 milliseconds.
 */
static void b06vs_log_presence_samples(void)
{
	static const unsigned int delta_us[B06VS_PCS_SAMPLE_COUNT] = {
		0, 1000, 9000, 90000, 400000,
	};
	unsigned int elapsed_us = 0;

	for (size_t i = 0; i < ARRAY_SIZE(delta_us); i++) {
		uint16_t pcs;

		if (delta_us[i] != 0)
			udelay(delta_us[i]);
		elapsed_us += delta_us[i];
		pcs = pci_io_read_config16(B06VR_SATA1_DEV,
			I82801JX_SATA_PCS);
		printk(BIOS_NOTICE,
		       "[SATA] B06VS PCS_SAMPLE T_MS=%u PCS=%04x EN=%02x "
		       "PRESENCE=%02x\n",
		       elapsed_us / 1000, pcs,
		       pcs & I82801JX_SATA_PCS_PORT_ENABLE_MASK,
		       (pcs & I82801JX_SATA_PCS_PRESENCE_MASK) >> 8);
	}
}

/*
 * Intel ICH10 section 14.1.31 requires supported ports to be enabled before
 * an AHCI-aware operating system is given control. This isolated experiment
 * runs only after the existing resource-allocation audit has succeeded. It
 * writes the PCS low byte only, so presence and high policy fields are never
 * write targets; all unselected low-byte fields are preserved by the helper.
 */
static void b06vs_program_ports_once(void)
{
	struct device *sata = b06vs_sata_device();
	const struct resource *abar = sata != NULL ?
		probe_resource(sata, I82801JX_SATA_ABAR) : NULL;
	const uint16_t before = pci_io_read_config16(B06VR_SATA1_DEV,
		I82801JX_SATA_PCS);
	const uint8_t before_low = before;
	uint16_t after;
	bool wrote = false;

	if (b06vs_port_enable_attempted)
		die_with_post_code(POST_B06VS_PORTS_FAIL,
			"[SATA] B06VS refused a second PCS attempt\n");
	b06vs_port_enable_attempted = true;
	b06vs_log_sata_state("PRE", sata, abar);

	if (!b06vs_sata_state_is_exact(sata, abar) ||
	    (before_low != 0 &&
	     before_low != I82801JX_SATA_PCS_ALL_PORTS_ENABLED) ||
	    (before_low & I82801JX_SATA_PCS_LOW_NON_PORT_MASK) != 0 ||
	    (before & I82801JX_SATA_PCS_RESERVED_14) != 0)
		die_with_post_code(POST_B06VS_PORTS_FAIL,
			"[SATA] B06VS rejected identity/decode/BAR/PCS prestate\n");

	post_code(POST_B06VS_PORTS_BEGIN);
	if (before_low != I82801JX_SATA_PCS_ALL_PORTS_ENABLED) {
		i82801jx_sata_enable_all_ports(B06VR_SATA1_DEV);
		wrote = true;
	}
	after = pci_io_read_config16(B06VR_SATA1_DEV, I82801JX_SATA_PCS);
	b06vs_log_sata_state("POST", sata, abar);
	if (!b06vs_sata_state_is_exact(sata, abar) ||
	    (uint8_t)after != I82801JX_SATA_PCS_ALL_PORTS_ENABLED ||
	    (after & I82801JX_SATA_PCS_RESERVED_14) != 0 ||
	    (after & I82801JX_SATA_PCS_OOB_RETRY_MODE) !=
		(before & I82801JX_SATA_PCS_OOB_RETRY_MODE))
		die_with_post_code(POST_B06VS_PORTS_FAIL,
			"[SATA] B06VS PCS selected-field/resource readback failed: "
			"before=%04x after=%04x\n", before, after);

	post_code(POST_B06VS_PORTS_OK);
	printk(BIOS_NOTICE,
	       "[SATA] B06VS PCS[5:0]=3f gate PASS (write=%u); "
	       "reserved14=0, ORM preserved, presence ignored; clocks/PI/AHCI MMIO untouched\n",
	       (unsigned int)wrote);
	b06vs_log_presence_samples();
}

#if CONFIG_X58_PRO_E_B06VT_ICH10_SATA_CLOCK
static void b06vt_log_clock_state(const char *phase, struct device *sata,
	const struct resource *abar)
{
	const uint32_t sclkcg = pci_io_read_config32(B06VR_SATA1_DEV,
		I82801JX_SATA_SCLKCG);
	const uint16_t pcs = pci_io_read_config16(B06VR_SATA1_DEV,
		I82801JX_SATA_PCS);

	printk(BIOS_NOTICE,
	       "[SATA] B06VT %s SCLKCG=%08x FIELD1=%03x PCD=%02x "
	       "RSV23_9=%04x RSV31_30=%u PCS=%04x MAP=%04x CMD=%04x "
	       "BAR5=%08x RES=%s\n",
	       phase, sclkcg,
	       sclkcg & I82801JX_SATA_SCLKCG_FIELD1_MASK,
	       (sclkcg & I82801JX_SATA_SCLKCG_PORT_DISABLE_MASK) >> 24,
	       (sclkcg & I82801JX_SATA_SCLKCG_RESERVED_23_9) >> 9,
	       (sclkcg & I82801JX_SATA_SCLKCG_RESERVED_31_30) >> 30,
	       pcs,
	       pci_io_read_config16(B06VR_SATA1_DEV, I82801JX_SATA_MAP),
	       pci_io_read_config16(B06VR_SATA1_DEV, PCI_COMMAND),
	       pci_io_read_config32(B06VR_SATA1_DEV, I82801JX_SATA_ABAR),
	       sata != NULL && abar != NULL ? dev_path(sata) : "missing");
}

/*
 * Intel ICH10 section 14.1.32 requires SCLKCG Field 1 to contain 0x193.
 * This successor accepts only the complete reset value or the retained exact
 * target. Thus the selected-field RMW cannot retain undocumented policy, gate
 * a port clock, or set the reserved bit 30 used by the historical broad driver.
 */
static void b06vt_program_clock_once(void)
{
	struct device *sata = b06vs_sata_device();
	const struct resource *abar = sata != NULL ?
		probe_resource(sata, I82801JX_SATA_ABAR) : NULL;
	const uint32_t before = pci_io_read_config32(B06VR_SATA1_DEV,
		I82801JX_SATA_SCLKCG);
	const uint16_t pcs_before = pci_io_read_config16(B06VR_SATA1_DEV,
		I82801JX_SATA_PCS);
	uint32_t after;
	uint16_t pcs_after;
	bool wrote = false;

	if (b06vt_clock_attempted)
		die_with_post_code(POST_B06VT_CLOCK_FAIL,
			"[SATA] B06VT refused a second SCLKCG attempt\n");
	b06vt_clock_attempted = true;
	b06vt_log_clock_state("PRE", sata, abar);

	if (!b06vs_sata_state_is_exact(sata, abar) ||
	    (uint8_t)pcs_before != I82801JX_SATA_PCS_ALL_PORTS_ENABLED ||
	    (pcs_before & I82801JX_SATA_PCS_RESERVED_14) != 0 ||
	    (before != 0 &&
	     before != I82801JX_SATA_SCLKCG_FIELD1_REQUIRED) ||
	    (before & I82801JX_SATA_SCLKCG_RESERVED_23_9) != 0 ||
	    (before & I82801JX_SATA_SCLKCG_PORT_DISABLE_MASK) != 0 ||
	    (before & I82801JX_SATA_SCLKCG_RESERVED_31_30) != 0)
		die_with_post_code(POST_B06VT_CLOCK_FAIL,
			"[SATA] B06VT rejected identity/resource/PCS/SCLKCG prestate\n");

	post_code(POST_B06VT_CLOCK_BEGIN);
	if (before != I82801JX_SATA_SCLKCG_FIELD1_REQUIRED) {
		i82801jx_sata_program_clock_field(B06VR_SATA1_DEV);
		wrote = true;
	}
	after = pci_io_read_config32(B06VR_SATA1_DEV,
		I82801JX_SATA_SCLKCG);
	pcs_after = pci_io_read_config16(B06VR_SATA1_DEV,
		I82801JX_SATA_PCS);
	b06vt_log_clock_state("POST", sata, abar);
	if (!b06vs_sata_state_is_exact(sata, abar) ||
	    after != I82801JX_SATA_SCLKCG_FIELD1_REQUIRED ||
	    (uint8_t)pcs_after != I82801JX_SATA_PCS_ALL_PORTS_ENABLED ||
	    (pcs_after & (I82801JX_SATA_PCS_RESERVED_14 |
		I82801JX_SATA_PCS_OOB_RETRY_MODE)) !=
		(pcs_before & (I82801JX_SATA_PCS_RESERVED_14 |
		 I82801JX_SATA_PCS_OOB_RETRY_MODE)))
		die_with_post_code(POST_B06VT_CLOCK_FAIL,
			"[SATA] B06VT exact SCLKCG/PCS/resource readback failed: "
			"before=%08x after=%08x pcs_before=%04x pcs_after=%04x\n",
			before, after, pcs_before, pcs_after);

	post_code(POST_B06VT_CLOCK_OK);
	printk(BIOS_NOTICE,
	       "[SATA] B06VT SCLKCG=00000193 gate PASS (write=%u); "
	       "PCD=0, reserved=0; PCS/MAP/BAR/CMD and AHCI MMIO untouched\n",
	       (unsigned int)wrote);
}
#endif
#endif

#if CONFIG_X58_PRO_E_B06VV_ICH10_PCS_SCLK
_Static_assert(I82801JX_SATA_PCS_PORT_ENABLE_MASK == 0x3f,
	"B06VV may select only PCS port-enable bits 5:0");
_Static_assert(I82801JX_SATA_PCS_ALL_PORTS_ENABLED == 0x3f,
	"B06VV requires all six ICH10 SATA ports");
_Static_assert(I82801JX_SATA_PCS_LOW_NON_PORT_MASK == 0xc0,
	"B06VV must gate PCS low-byte non-port fields");
_Static_assert(I82801JX_SATA_SCLKCG_FIELD1_MASK == 0x000001ff,
	"B06VV may select only SCLKCG bits 8:0");
_Static_assert(I82801JX_SATA_SCLKCG_FIELD1_REQUIRED == 0x00000193,
	"B06VV requires the proven SCLKCG Field 1 target");
_Static_assert(I82801JX_SATA_SCLKCG_RESERVED_23_9 == 0x00fffe00 &&
	I82801JX_SATA_SCLKCG_PORT_DISABLE_MASK == 0x3f000000 &&
	I82801JX_SATA_SCLKCG_RESERVED_31_30 == 0xc0000000,
	"B06VV SCLKCG reserved/port-disable masks changed");

static bool b06vv_snapshot_is_exact(
	const struct b06vu_sata_snapshot *snapshot, struct device *sata,
	const struct resource *abar, uint16_t expected_command)
{
	const uint16_t decode = PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
		PCI_COMMAND_MASTER;

	return b06vu_fixed_gates_are_exact(snapshot) &&
		snapshot->fd == B06VU_FD_SATA2_DISABLED &&
		sata != NULL && sata->enabled && sata->mandatory &&
		(sata->command & decode) == PCI_COMMAND_MEMORY &&
		snapshot->f2_id == B06VR_SATA1_AHCI_ID &&
		snapshot->f2_classrev == B06VU_SATA1_AHCI_CLASSREV &&
		snapshot->f2_command == expected_command &&
		snapshot->f2_header == PCI_HEADER_TYPE_NORMAL &&
		snapshot->f2_map == I82801JX_SATA_MAP_AHCI_D31F2_VALUE &&
		b06vs_abar_resource_is_exact(abar, snapshot->f2_bar5) &&
		snapshot->f5_id == 0xffffffffu &&
		snapshot->f5_classrev == 0xffffffffu &&
		snapshot->f5_command == 0xffffu &&
		snapshot->f5_header == 0xffu &&
		snapshot->f5_bar5 == 0xffffffffu &&
		snapshot->f5_map == 0xffffu;
}

static bool b06vv_sclk_is_admitted(uint32_t sclkcg)
{
	return sclkcg == 0 ||
		sclkcg == I82801JX_SATA_SCLKCG_FIELD1_REQUIRED;
}

static void b06vv_log_state(const char *phase,
	const struct b06vu_sata_snapshot *snapshot, struct device *sata,
	const struct resource *abar)
{
	const uint64_t base = abar != NULL ? abar->base : 0;
	const uint64_t size = abar != NULL ? abar->size : 0;
	const unsigned long flags = abar != NULL ? abar->flags : 0;

	printk(BIOS_NOTICE,
	       "[SATA] B06VV %s LPC_ID=%08x RCBA=%08x FDSW=%02x FD=%08x "
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

#if CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT
static bool b06wd_sata_policy_prestate_is_exact(
	const struct b06vu_sata_snapshot *snapshot, struct device *sata,
	const struct resource *abar)
{
	return b06vu_fixed_gates_are_exact(snapshot) &&
		snapshot->fd == B06VU_FD_SATA2_DISABLED &&
		sata != NULL && sata->enabled && sata->mandatory &&
		sata->command == (PCI_COMMAND_IO | PCI_COMMAND_MEMORY) &&
		snapshot->f2_id == B06VR_SATA1_AHCI_ID &&
		snapshot->f2_classrev == B06VU_SATA1_AHCI_CLASSREV &&
		snapshot->f2_command == 0 &&
		snapshot->f2_header == PCI_HEADER_TYPE_NORMAL &&
		snapshot->f2_map == I82801JX_SATA_MAP_AHCI_D31F2_VALUE &&
		b06vs_abar_resource_is_exact(abar, snapshot->f2_bar5) &&
		snapshot->f2_pcs == 0 && snapshot->f2_sclkcg == 0 &&
		snapshot->f5_id == 0xffffffffu &&
		snapshot->f5_classrev == 0xffffffffu &&
		snapshot->f5_command == 0xffffu &&
		snapshot->f5_header == 0xffu &&
		snapshot->f5_bar5 == 0xffffffffu &&
		snapshot->f5_map == 0xffffu;
}

static void b06wd_correct_sata_policy_once(void)
{
	struct device *sata = b06vs_sata_device();
	const struct resource *abar = sata != NULL ?
		probe_resource(sata, I82801JX_SATA_ABAR) : NULL;
	const struct b06vu_sata_snapshot before = b06vu_read_sata_snapshot();
	struct b06vu_sata_snapshot after;

	if (b06wd_sata_policy_ready)
		die_with_post_code(POST_B06WD_POLICY_FAIL,
			"[SATA] B06WD refused a second policy correction\n");
	b06vv_log_state("B06WD_POLICY_PRE", &before, sata, abar);
	if (!b06wd_sata_policy_prestate_is_exact(&before, sata, abar))
		die_with_post_code(POST_B06WD_POLICY_FAIL,
			"[SATA] B06WD rejected exact hardware/policy prestate\n");

	post_code(POST_B06WD_POLICY_BEGIN);
	/* Software policy only: the hardware command register stays at 0000. */
	sata->command &= ~PCI_COMMAND_IO;
	after = b06vu_read_sata_snapshot();
	b06vv_log_state("B06WD_POLICY_POST", &after, sata, abar);
	if (!b06vv_snapshot_is_exact(&after, sata, abar, 0) ||
	    after.f2_pcs != before.f2_pcs ||
	    after.f2_sclkcg != before.f2_sclkcg)
		die_with_post_code(POST_B06WD_POLICY_FAIL,
			"[SATA] B06WD policy or hardware readback mismatch\n");

	b06wd_sata_policy_ready = true;
	post_code(POST_B06WD_POLICY_READY);
	printk(BIOS_NOTICE,
	       "[SATA] B06WD POLICY_CMD 0003->0002 HARDWARE_CMD=0000 HARDWARE_WRITE=0\n");
}
#endif

static void b06vv_log_presence_samples(void)
{
	static const unsigned int delta_us[B06VS_PCS_SAMPLE_COUNT] = {
		0, 1000, 9000, 90000, 400000,
	};
	unsigned int elapsed_us = 0;

	for (size_t i = 0; i < ARRAY_SIZE(delta_us); i++) {
		uint16_t pcs;

		if (delta_us[i] != 0)
			udelay(delta_us[i]);
		elapsed_us += delta_us[i];
		pcs = pci_io_read_config16(B06VR_SATA1_DEV,
			I82801JX_SATA_PCS);
		printk(BIOS_NOTICE,
		       "[SATA] B06VV PCS_SAMPLE T_MS=%u PCS=%04x EN=%02x "
		       "PRESENCE=%02x\n",
		       elapsed_us / 1000, pcs,
		       pcs & I82801JX_SATA_PCS_PORT_ENABLE_MASK,
		       (pcs & I82801JX_SATA_PCS_PRESENCE_MASK) >> 8);
	}
}

static void b06vv_program_pcs_once(void)
{
	struct device *sata = b06vs_sata_device();
	const struct resource *abar = sata != NULL ?
		probe_resource(sata, I82801JX_SATA_ABAR) : NULL;
	struct b06vu_sata_snapshot before = b06vu_read_sata_snapshot();
	struct b06vu_sata_snapshot after;
	const uint8_t before_low = before.f2_pcs;
	bool wrote = false;

#if CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT
	if (!b06wd_sata_policy_ready)
		die_with_post_code(POST_B06WD_POLICY_FAIL,
			"[SATA] B06WD policy correction did not precede PCS\n");
#endif

	if (b06vv_pcs_attempted)
		die_with_post_code(POST_B06VV_FAIL,
			"[SATA] B06VV refused a second PCS attempt\n");
	b06vv_pcs_attempted = true;
	b06vv_log_state("PCS_PRE", &before, sata, abar);

	if (!b06vv_snapshot_is_exact(&before, sata, abar, 0) ||
	    (before_low != 0 &&
	     before_low != I82801JX_SATA_PCS_ALL_PORTS_ENABLED) ||
	    (before_low & I82801JX_SATA_PCS_LOW_NON_PORT_MASK) != 0 ||
	    (before.f2_pcs & I82801JX_SATA_PCS_RESERVED_14) != 0 ||
	    !b06vv_sclk_is_admitted(before.f2_sclkcg))
		die_with_post_code(POST_B06VV_FAIL,
			"[SATA] B06VV rejected corrected-route/resource/PCS prestate\n");

	post_code(POST_B06VV_PCS_BEGIN);
	printk(BIOS_NOTICE,
	       "[SATA] B06VV PCS_STAGE begin: select only PCS[5:0]=3f\n");
	if (before_low != I82801JX_SATA_PCS_ALL_PORTS_ENABLED) {
		i82801jx_sata_enable_all_ports(B06VR_SATA1_DEV);
		wrote = true;
	}
	after = b06vu_read_sata_snapshot();
	b06vv_log_state("PCS_POST", &after, sata, abar);
	if (!b06vv_snapshot_is_exact(&after, sata, abar, 0) ||
	    (uint8_t)after.f2_pcs !=
		I82801JX_SATA_PCS_ALL_PORTS_ENABLED ||
	    (after.f2_pcs & I82801JX_SATA_PCS_RESERVED_14) != 0 ||
	    (after.f2_pcs & I82801JX_SATA_PCS_OOB_RETRY_MODE) !=
		(before.f2_pcs & I82801JX_SATA_PCS_OOB_RETRY_MODE) ||
	    after.f2_sclkcg != before.f2_sclkcg)
		die_with_post_code(POST_B06VV_FAIL,
			"[SATA] B06VV PCS selected-field/full-route readback failed\n");

	post_code(POST_B06VV_PCS_OK);
	printk(BIOS_NOTICE,
	       "[SATA] B06VV PCS[5:0]=3f gate PASS (write=%u); "
	       "SCLKCG unchanged, presence ignored, ABAR MMIO/BME untouched\n",
	       (unsigned int)wrote);
	b06vv_log_presence_samples();
}

static void b06vv_program_sclk_once(void)
{
	struct device *sata = b06vs_sata_device();
	const struct resource *abar = sata != NULL ?
		probe_resource(sata, I82801JX_SATA_ABAR) : NULL;
	struct b06vu_sata_snapshot before = b06vu_read_sata_snapshot();
	struct b06vu_sata_snapshot after;
	const uint16_t pcs_policy_mask =
		I82801JX_SATA_PCS_PORT_ENABLE_MASK |
		I82801JX_SATA_PCS_LOW_NON_PORT_MASK |
		I82801JX_SATA_PCS_RESERVED_14 |
		I82801JX_SATA_PCS_OOB_RETRY_MODE;
	bool wrote = false;

	if (b06vv_sclk_attempted)
		die_with_post_code(POST_B06VV_FAIL,
			"[SATA] B06VV refused a second SCLKCG attempt\n");
	b06vv_sclk_attempted = true;
	b06vv_log_state("SCLK_PRE", &before, sata, abar);

	if (!b06vv_pcs_attempted ||
	    !b06vv_snapshot_is_exact(&before, sata, abar, 0) ||
	    (uint8_t)before.f2_pcs !=
		I82801JX_SATA_PCS_ALL_PORTS_ENABLED ||
	    (before.f2_pcs & I82801JX_SATA_PCS_RESERVED_14) != 0 ||
	    !b06vv_sclk_is_admitted(before.f2_sclkcg))
		die_with_post_code(POST_B06VV_FAIL,
			"[SATA] B06VV rejected PCS-complete/SCLKCG prestate\n");

	post_code(POST_B06VV_SCLK_BEGIN);
	printk(BIOS_NOTICE,
	       "[SATA] B06VV SCLK_STAGE begin: select only SCLKCG[8:0]=193\n");
	if (before.f2_sclkcg != I82801JX_SATA_SCLKCG_FIELD1_REQUIRED) {
		i82801jx_sata_program_clock_field(B06VR_SATA1_DEV);
		wrote = true;
	}
	after = b06vu_read_sata_snapshot();
	b06vv_log_state("SCLK_POST", &after, sata, abar);
	if (!b06vv_snapshot_is_exact(&after, sata, abar, 0) ||
	    after.f2_sclkcg != I82801JX_SATA_SCLKCG_FIELD1_REQUIRED ||
	    (uint8_t)after.f2_pcs !=
		I82801JX_SATA_PCS_ALL_PORTS_ENABLED ||
	    (after.f2_pcs & pcs_policy_mask) !=
		(before.f2_pcs & pcs_policy_mask))
		die_with_post_code(POST_B06VV_FAIL,
			"[SATA] B06VV SCLKCG full-dword/PCS/route readback failed\n");

	post_code(POST_B06VV_SCLK_OK);
	printk(BIOS_NOTICE,
	       "[SATA] B06VV SCLKCG=00000193 gate PASS (write=%u); "
	       "PCD/reserved=0, corrected route retained, ABAR MMIO/BME untouched\n",
	       (unsigned int)wrote);
	post_code(POST_B06VV_READY);
	printk(BIOS_NOTICE,
	       "[SATA] B06VV corrected route + PCS + SCLKCG READY\n");
}

#if CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO
_Static_assert(B06VS_ABAR_SIZE == 0x800,
	"B06VW requires the ICH10 2-KiB ABAR");
_Static_assert(B06VW_AHCI_GHC_AE == 0x80000000u,
	"B06VW may select only the AHCI-enable bit in GHC");
_Static_assert(B06VW_AHCI_PI_TARGET == 0x0000003fu,
	"B06VW implements exactly six AHCI ports");

struct b06vw_global_snapshot {
	uint32_t cap;
	uint32_t ghc;
	uint32_t pi;
	uint32_t vs;
};

static struct b06vw_global_snapshot b06vw_read_global(uintptr_t abar)
{
	return (struct b06vw_global_snapshot) {
		.cap = read32p(abar + B06VW_AHCI_CAP),
		.ghc = read32p(abar + B06VW_AHCI_GHC),
		.pi = read32p(abar + B06VW_AHCI_PI),
		.vs = read32p(abar + B06VW_AHCI_VS),
	};
}

static void b06vw_log_global(const char *phase, uintptr_t abar,
	const struct b06vw_global_snapshot *snapshot)
{
	printk(BIOS_NOTICE,
	       "[AHCI] B06VW %s ABAR=%08lx CAP=%08x GHC=%08x "
	       "AE=%u HR=%u IE=%u PI=%08x VS=%08x\n",
	       phase, (unsigned long)abar, snapshot->cap, snapshot->ghc,
	       !!(snapshot->ghc & B06VW_AHCI_GHC_AE),
	       !!(snapshot->ghc & BIT(0)), !!(snapshot->ghc & BIT(1)),
	       snapshot->pi, snapshot->vs);
}

static bool b06vw_global_reset_is_exact(
	const struct b06vw_global_snapshot *snapshot)
{
	return snapshot->cap == B06VW_AHCI_CAP_TARGET &&
		snapshot->ghc == 0 && snapshot->pi == 0 &&
		snapshot->vs == B06VW_AHCI_VS_TARGET;
}

static bool b06vw_global_ae_only_is_exact(
	const struct b06vw_global_snapshot *snapshot)
{
	return snapshot->cap == B06VW_AHCI_CAP_TARGET &&
		snapshot->ghc == B06VW_AHCI_GHC_AE && snapshot->pi == 0 &&
		snapshot->vs == B06VW_AHCI_VS_TARGET;
}

static bool b06vw_global_target_is_exact(
	const struct b06vw_global_snapshot *snapshot)
{
	return snapshot->cap == B06VW_AHCI_CAP_TARGET &&
		snapshot->ghc == B06VW_AHCI_GHC_AE &&
		snapshot->pi == B06VW_AHCI_PI_TARGET &&
		snapshot->vs == B06VW_AHCI_VS_TARGET;
}

static bool b06vw_config_is_exact(
	const struct b06vu_sata_snapshot *snapshot, struct device *sata,
	const struct resource *abar, uint16_t expected_command)
{
	return b06vv_snapshot_is_exact(snapshot, sata, abar,
			expected_command) &&
		(uint8_t)snapshot->f2_pcs ==
			I82801JX_SATA_PCS_ALL_PORTS_ENABLED &&
		(snapshot->f2_pcs & I82801JX_SATA_PCS_RESERVED_14) == 0 &&
		snapshot->f2_sclkcg ==
			I82801JX_SATA_SCLKCG_FIELD1_REQUIRED;
}

static void b06vw_program_mmio_once(void)
{
	struct device *sata = b06vs_sata_device();
	const struct resource *abar = sata != NULL ?
		probe_resource(sata, I82801JX_SATA_ABAR) : NULL;
	struct b06vu_sata_snapshot config = b06vu_read_sata_snapshot();
	struct b06vw_global_snapshot global;
	uintptr_t base;
	bool retained;

	if (b06vw_mmio_attempted)
		die_with_post_code(POST_B06VW_FAIL,
			"[AHCI] B06VW refused a second MMIO attempt\n");
	b06vw_mmio_attempted = true;
	b06vv_log_state("B06VW_MEM_PRE", &config, sata, abar);
	if (!b06vv_pcs_attempted || !b06vv_sclk_attempted ||
	    !b06vw_config_is_exact(&config, sata, abar, 0))
		die_with_post_code(POST_B06VW_FAIL,
			"[AHCI] B06VW rejected B06VV/resource/MEM prestate\n");
	base = (uintptr_t)abar->base;

	post_code(POST_B06VW_MEM_BEGIN);
	printk(BIOS_NOTICE,
	       "[AHCI] B06VW MEM_STAGE begin: enable only PCI_COMMAND.MEM "
	       "for allocator-selected ABAR=%08lx\n", (unsigned long)base);
	pci_io_write_config16(B06VR_SATA1_DEV, PCI_COMMAND,
		PCI_COMMAND_MEMORY);
	config = b06vu_read_sata_snapshot();
	b06vv_log_state("B06VW_MEM_POST", &config, sata, abar);
	if (!b06vw_config_is_exact(&config, sata, abar,
			PCI_COMMAND_MEMORY))
		die_with_post_code(POST_B06VW_FAIL,
			"[AHCI] B06VW exact MEM-only decode readback failed\n");
	post_code(POST_B06VW_MEM_OK);

	/* ABAR is accessed only after its resource and MEM-only decode pass. */
	global = b06vw_read_global(base);
	b06vw_log_global("GLOBAL_PRE", base, &global);
	if (!b06vw_global_reset_is_exact(&global) &&
	    !b06vw_global_target_is_exact(&global))
		die_with_post_code(POST_B06VW_FAIL,
			"[AHCI] B06VW rejected partial/unexpected CAP/GHC/PI/VS tuple\n");
	retained = b06vw_global_target_is_exact(&global);

	post_code(POST_B06VW_AE_BEGIN);
	printk(BIOS_NOTICE,
	       "[AHCI] B06VW AE_STAGE begin: set only GHC.AE\n");
	if (!retained)
		write32p(base + B06VW_AHCI_GHC, B06VW_AHCI_GHC_AE);
	global = b06vw_read_global(base);
	b06vw_log_global("AE_POST", base, &global);
	config = b06vu_read_sata_snapshot();
	if (!b06vw_config_is_exact(&config, sata, abar,
			PCI_COMMAND_MEMORY) ||
	    (!retained && !b06vw_global_ae_only_is_exact(&global)) ||
	    (retained && !b06vw_global_target_is_exact(&global)))
		die_with_post_code(POST_B06VW_FAIL,
			"[AHCI] B06VW GHC.AE full-state readback failed\n");
	post_code(POST_B06VW_AE_OK);

	post_code(POST_B06VW_PI_BEGIN);
	printk(BIOS_NOTICE,
	       "[AHCI] B06VW PI_STAGE begin: write only PI low byte=3f\n");
	if (!retained)
		write8p(base + B06VW_AHCI_PI,
			(uint8_t)B06VW_AHCI_PI_TARGET);
	global = b06vw_read_global(base);
	b06vw_log_global("PI_POST", base, &global);
	config = b06vu_read_sata_snapshot();
	if (!b06vw_config_is_exact(&config, sata, abar,
			PCI_COMMAND_MEMORY) ||
	    !b06vw_global_target_is_exact(&global))
		die_with_post_code(POST_B06VW_FAIL,
			"[AHCI] B06VW PI/global/configuration readback failed\n");
	post_code(POST_B06VW_PI_OK);
	printk(BIOS_NOTICE,
	       "[AHCI] B06VW AE=1 PI=0000003f PASS (retained=%u); "
	       "BME/HR/IE/CAP/VS/CLB/FB/PxCMD/COMRESET/IDENTIFY untouched\n",
	       (unsigned int)retained);
}

static void b06vw_verify_final_once(void)
{
	struct device *sata = b06vs_sata_device();
	const struct resource *abar = sata != NULL ?
		probe_resource(sata, I82801JX_SATA_ABAR) : NULL;
	struct b06vu_sata_snapshot config = b06vu_read_sata_snapshot();
	struct b06vw_global_snapshot global;

	if (!b06vw_mmio_attempted || b06vw_final_verified || abar == NULL)
		die_with_post_code(POST_B06VW_FAIL,
			"[AHCI] B06VW final verification order/ABAR failure\n");
	global = b06vw_read_global((uintptr_t)abar->base);
	b06vw_log_global("ENABLE_POST", (uintptr_t)abar->base, &global);
	if (!b06vw_config_is_exact(&config, sata, abar,
			PCI_COMMAND_MEMORY) ||
	    !b06vw_global_target_is_exact(&global))
		die_with_post_code(POST_B06VW_FAIL,
			"[AHCI] B06VW post-enable target did not remain exact\n");
	b06vw_final_verified = true;
	post_code(POST_B06VW_READY);
	printk(BIOS_NOTICE,
	       "[AHCI] B06VW READY; dynamic ABAR decoded MEM-only, AE=1, PI=3f; "
	       "SeaBIOS owns HBA reset, BME and every port operation\n");
}
#endif
#endif
#endif

static size_t b06vn_collect_and_audit_resources(
	struct b06vn_leaf_resource leaves[B06VN_MAX_LEAF_RESOURCES])
{
	size_t leaf_count = 0;

	for (size_t i = 0; i < ARRAY_SIZE(b06vn_expected); i++) {
		const struct b06vn_expected_pci *expected = &b06vn_expected[i];
		struct device *dev = b06vn_device_for_expected(expected);
		const struct resource *resource;
		size_t usable = 0;

		if (dev == NULL)
			die_with_post_code(POST_B06VN_RESOURCE_FAIL,
				"[RAMSTAGE] B06VN resource owner missing\n");
		if (b06vn_optional_device_absent(expected, dev))
			continue;
		for (resource = dev->resource_list; resource;
		     resource = resource->next) {
			const unsigned long type = resource->flags &
				(IORESOURCE_IO | IORESOURCE_MEM);

			if (!resource->size || !type)
				continue;
			usable++;
			b06vn_require_allocated_resource(dev, resource);
			if (!(resource->flags & IORESOURCE_BRIDGE)) {
				if (leaf_count >= B06VN_MAX_LEAF_RESOURCES)
					die_with_post_code(POST_B06VN_RESOURCE_FAIL,
						"[RAMSTAGE] B06VN resource audit overflow\n");
				leaves[leaf_count++] = (struct b06vn_leaf_resource) {
					.dev = dev,
					.resource = resource,
				};
			}
		}
		if (!expected->bridge && usable == 0)
			die_with_post_code(POST_B06VN_RESOURCE_FAIL,
				"[RAMSTAGE] B06VN endpoint has no usable BAR: %s\n",
				expected->name);
	}

	return leaf_count;
}

static void b06vn_require_no_leaf_overlap(
	const struct b06vn_leaf_resource leaves[B06VN_MAX_LEAF_RESOURCES],
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
				die_with_post_code(POST_B06VN_OVERLAP_FAIL,
					"[RAMSTAGE] B06VN overlapping leaf resources: %s / %s\n",
					dev_path(leaves[i].dev), dev_path(leaves[j].dev));
		}
	}
}

#if CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES
static void b06vz_require_no_fixed_leaf_overlap(
	const struct b06vn_leaf_resource leaves[B06VN_MAX_LEAF_RESOURCES],
	size_t count)
{
	for (size_t i = B06VZ_FIRST_FIXED_RESOURCE_INDEX;
	     i < ARRAY_SIZE(b06vz_domain_resource_contracts); i++) {
		const struct b06vz_domain_resource_contract *fixed =
			&b06vz_domain_resource_contracts[i];
		const unsigned long fixed_type = fixed->flags & IORESOURCE_TYPE_MASK;

		for (size_t j = 0; j < count; j++) {
			const struct resource *leaf = leaves[j].resource;
			const unsigned long leaf_type = leaf->flags &
				IORESOURCE_TYPE_MASK;
			const uint64_t leaf_top = leaf->base + leaf->size;

			if (fixed_type == leaf_type && fixed->base < leaf_top &&
			    leaf->base < fixed->top)
				die_with_post_code(POST_B06VN_OVERLAP_FAIL,
					"[RESOURCE] B06VZ fixed resource %lx overlaps %s\n",
					fixed->index, dev_path(leaves[j].dev));
		}
	}
}
#endif

#if CONFIG_X58_PRO_E_B06WA_HPET_DECODE
static void b06wa_require_no_hpet_leaf_overlap(
	const struct b06vn_leaf_resource leaves[B06VN_MAX_LEAF_RESOURCES],
	size_t count)
{
	for (size_t i = 0; i < count; i++) {
		const struct resource *leaf = leaves[i].resource;
		const uint64_t leaf_top = leaf->base + leaf->size;

		if ((leaf->flags & IORESOURCE_TYPE_MASK) == IORESOURCE_MEM &&
		    b06wa_hpet_resource_contract.base < leaf_top &&
		    leaf->base < b06wa_hpet_resource_contract.top)
			die_with_post_code(POST_B06VN_OVERLAP_FAIL,
				"[RESOURCE] B06WA HPET overlaps %s\n",
				dev_path(leaves[i].dev));
	}
}
#endif

static void b06vn_resources_assigned(void *unused)
{
	struct b06vn_leaf_resource leaves[B06VN_MAX_LEAF_RESOURCES];
	size_t leaf_count;

	(void)unused;
	b06vn_require_platform_state("post-allocation");
	b06vn_require_enumerated_topology();
	b06vn_require_domain_resource(0, 0, 0x000a0000,
		IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_CACHEABLE);
	b06vn_require_domain_resource(1, 0x000a0000, 0x000c0000,
		IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_RESERVE);
	b06vn_require_domain_resource(2, 0x000c0000, 0x00100000,
		IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_RESERVE);
	b06vn_require_domain_resource(3, B06VN_LOW_RAM_BASE, B06VN_LOW_RAM_TOP,
		IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_CACHEABLE);
	b06vn_require_domain_resource(4, B06VN_HIGH_RAM_BASE, B06VN_HIGH_RAM_TOP,
		IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_CACHEABLE);
	b06vn_require_domain_resource(5, B06VN_PCI_IO_BASE, B06VN_PCI_IO_TOP,
		IORESOURCE_IO | IORESOURCE_BRIDGE);
	b06vn_require_domain_resource(6, B06VN_PCI_MMIO_BASE,
		B06VN_PCI_MMIO_TOP, IORESOURCE_MEM | IORESOURCE_BRIDGE);
	b06vn_require_domain_resource(7, B06VN_ECAM_BASE, B06VN_ECAM_TOP,
		IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_RESERVE);
#if CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES
	b06vz_require_exact_domain_resources();
#if CONFIG_X58_PRO_E_B06WA_HPET_DECODE
	b06wa_require_exact_hpet_resource();
#endif
#endif

	leaf_count = b06vn_collect_and_audit_resources(leaves);
	b06vn_require_no_leaf_overlap(leaves, leaf_count);
#if CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES
	b06vz_require_no_fixed_leaf_overlap(leaves, leaf_count);
	printk(BIOS_NOTICE,
	       "[RESOURCE] B06VZ exact resources 0..14 and non-overlap audit PASS\n");
#if CONFIG_X58_PRO_E_B06WA_HPET_DECODE
	b06wa_require_no_hpet_leaf_overlap(leaves, leaf_count);
	printk(BIOS_NOTICE,
	       "[RESOURCE] B06WA exact resources 0..15 and non-overlap audit PASS\n");
#endif
#endif
	post_code(POST_B06VN_ALLOC_OK);
	printk(BIOS_NOTICE,
	       "[RAMSTAGE] B06VN PCI allocation audit PASS; endpoint BME remains off\n");
#if CONFIG_X58_PRO_E_B06VV_ICH10_PCS_SCLK
#if CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT
	b06wd_correct_sata_policy_once();
#endif
	b06vv_program_pcs_once();
	b06vv_program_sclk_once();
#if CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO
	b06vw_program_mmio_once();
#endif
#else
#if CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS
	b06vs_program_ports_once();
#endif
#if CONFIG_X58_PRO_E_B06VT_ICH10_SATA_CLOCK
	b06vt_program_clock_once();
#endif
#endif
}

static void b06vn_resources_enabled(void *unused)
{
	size_t forwarding_bridges = 0;

	(void)unused;
	b06vn_require_platform_state("post-enable");
	for (size_t i = 0; i < ARRAY_SIZE(b06vn_expected); i++) {
		const struct b06vn_expected_pci *expected = &b06vn_expected[i];
		struct device *dev = b06vn_device_for_expected(expected);
		uint16_t command;
		uint16_t expected_master = 0;

		if (b06vn_optional_device_absent(expected, dev))
			continue;
		if (dev == NULL)
			die_with_post_code(POST_B06VN_ENABLE_FAIL,
				"[RAMSTAGE] B06VN enable owner missing\n");
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
			die_with_post_code(POST_B06VN_ENABLE_FAIL,
				"[RAMSTAGE] B06VN decode/master enable audit failed\n");
	}
#if CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO
	b06vw_verify_final_once();
#endif
#if CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT
	b06vq_log_usb_runtime_once();
#endif
#if CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT
	b06wd_prepare_input();
#endif
#if CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM
	if (!b06wc_pic_ready)
		die_with_post_code(POST_B06WC_PIC_FAIL,
			"[PIC] B06WC setup did not reach READY before USB admission\n");
	b06wc_usb_admit();
#endif
#if CONFIG_X58_PRO_E_B06VQ_PLATRO1
	b06vqp_log_platform_census_once();
#endif
	post_code(POST_B06VN_ENABLE_OK);
	printk(BIOS_NOTICE,
	       "[RAMSTAGE] B06VN decodes enabled; BME only on %zu forwarding bridges\n",
	       forwarding_bridges);
}

#if CONFIG_X58_PRO_E_B06VP_LAPIC_EXTINT
struct b06vp_lapic_snapshot {
	msr_t apic_base;
	uint32_t tpr;
	uint32_t svr;
	uint32_t lvt0;
	uint32_t lvt1;
};

_Static_assert(!CONFIG(SMP), "B06VP must remain a single-BSP experiment");
_Static_assert(CONFIG(XAPIC_ONLY), "B06VP requires fixed xAPIC mode");
_Static_assert(!CONFIG(NO_PCAT_8259),
	"B06VP requires the legacy 8259 PIC path");
_Static_assert(CONFIG(SEABIOS_HARDWARE_IRQ),
	"B06VP must retain SeaBIOS hardware interrupts");
_Static_assert(CONFIG(BUILD_IPXE) && !CONFIG(IPXE_NO_PROMPT),
	"B06VP requires the prompted iPXE acceptance path");

static bool b06vp_apic_base_is_expected(msr_t apic_base)
{
	return apic_base.hi == 0 &&
		(apic_base.lo & LAPIC_BASE_MSR_ADDR_MASK) == LAPIC_DEFAULT_BASE &&
		(apic_base.lo & LAPIC_BASE_MSR_ENABLE) != 0 &&
		(apic_base.lo & LAPIC_BASE_MSR_X2APIC_MODE) == 0 &&
		(apic_base.lo & LAPIC_BASE_MSR_BOOTSTRAP_PROCESSOR) != 0;
}

static struct b06vp_lapic_snapshot b06vp_read_lapic_snapshot(msr_t apic_base)
{
	return (struct b06vp_lapic_snapshot) {
		.apic_base = apic_base,
		.tpr = lapic_read(LAPIC_TASKPRI),
		.svr = lapic_read(LAPIC_SPIV),
		.lvt0 = lapic_read(LAPIC_LVT0),
		.lvt1 = lapic_read(LAPIC_LVT1),
	};
}

static void b06vp_log_apic_base(const char *phase, msr_t apic_base)
{
	printk(BIOS_NOTICE, "[LAPIC] B06VP %s IA32_APIC_BASE=%08x:%08x\n",
	       phase, apic_base.hi, apic_base.lo);
}

static void b06vp_log_lapic_snapshot(const char *phase,
	const struct b06vp_lapic_snapshot *snapshot)
{
	printk(BIOS_NOTICE,
	       "[LAPIC] B06VP %s TPR=%08x SVR=%08x LVT0=%08x LVT1=%08x\n",
	       phase,
	       snapshot->tpr, snapshot->svr, snapshot->lvt0, snapshot->lvt1);
}

static bool b06vp_lapic_state_is_expected(
	const struct b06vp_lapic_snapshot *snapshot)
{
	const uint32_t forbidden_lvt_bits = LAPIC_LVT_MASKED |
		LAPIC_LVT_LEVEL_TRIGGER | LAPIC_INPUT_POLARITY;

	return b06vp_apic_base_is_expected(snapshot->apic_base) &&
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

static void b06vp_cpu_cluster_init(struct device *dev)
{
	struct b06vp_lapic_snapshot before;
	struct b06vp_lapic_snapshot after;
	msr_t apic_base;

	(void)dev;
	apic_base = rdmsr(LAPIC_BASE_MSR);
	b06vp_log_apic_base("PRE", apic_base);
	if (!b06vp_apic_base_is_expected(apic_base))
		die_with_post_code(POST_B06VP_LAPIC_FAIL,
			"[LAPIC] B06VP PRE IA32_APIC_BASE=%08x:%08x is not BSP xAPIC\n",
			apic_base.hi, apic_base.lo);
	before = b06vp_read_lapic_snapshot(apic_base);
	b06vp_log_lapic_snapshot("PRE", &before);
	if (b06vp_lapic_setup_attempted || !boot_cpu())
		die_with_post_code(POST_B06VP_LAPIC_FAIL,
			"[LAPIC] B06VP refused a non-BSP or repeated setup\n");

	b06vp_lapic_setup_attempted = true;
	post_code(POST_B06VP_LAPIC_BEGIN);
	setup_lapic_interrupts();
	apic_base = rdmsr(LAPIC_BASE_MSR);
	b06vp_log_apic_base("POST", apic_base);
	if (!b06vp_apic_base_is_expected(apic_base))
		die_with_post_code(POST_B06VP_LAPIC_FAIL,
			"[LAPIC] B06VP POST IA32_APIC_BASE=%08x:%08x changed\n",
			apic_base.hi, apic_base.lo);
	after = b06vp_read_lapic_snapshot(apic_base);
	b06vp_log_lapic_snapshot("POST", &after);
	if (!b06vp_lapic_state_is_expected(&after))
		die_with_post_code(POST_B06VP_LAPIC_FAIL,
			"[LAPIC] B06VP virtual-wire readback gate failed\n");

	post_code(POST_B06VP_LAPIC_OK);
	printk(BIOS_NOTICE,
	       "[LAPIC] B06VP BSP virtual-wire ExtINT/NMI gate PASS\n");
}
#endif

static struct device_operations b06vn_domain_ops = {
#if CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM
	.write_acpi_tables = b06wj_write_acpi_tables,
#endif
	#if CONFIG_X58_PRO_E_B06WB_TCO_HALT
	.read_resources = b06wb_read_resources,
	#elif CONFIG_X58_PRO_E_B06WA_HPET_DECODE
	.read_resources = b06wa_read_resources,
	#elif CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES
	.read_resources = b06vz_read_resources,
#else
	.read_resources = b06vn_read_resources,
#endif
	.set_resources = pci_domain_set_resources,
	.scan_bus = b06vn_domain_scan_bus,
};

static struct device_operations b06vn_cpu_cluster_ops = {
	.read_resources = noop_read_resources,
	.set_resources = noop_set_resources,
#if CONFIG_X58_PRO_E_B06VP_LAPIC_EXTINT
	.init = b06vp_cpu_cluster_init,
#endif
};

struct device_operations b06vn_root_port_ops = {
	.read_resources = pci_bus_read_resources,
	.set_resources = pci_dev_set_resources,
	.enable_resources = pci_bus_enable_resources,
	.scan_bus = b06vn_scan_bridge,
	.enable = b06vn_probe_gate,
};

struct device_operations b06vn_endpoint_ops = {
	.read_resources = pci_dev_read_resources,
	.set_resources = pci_dev_set_resources,
	.enable_resources = pci_dev_enable_resources,
	.enable = b06vn_probe_gate,
};

void x58_b06vn_enable_dev(struct device *dev)
{
	if (dev->path.type == DEVICE_PATH_DOMAIN) {
		if (b06vn_domain != NULL && b06vn_domain != dev)
			die_with_post_code(POST_B06VN_TOPOLOGY_FAIL,
				"[RAMSTAGE] B06VN found multiple PCI domains\n");
		b06vn_domain = dev;
		dev->ops = &b06vn_domain_ops;
	} else if (dev->path.type == DEVICE_PATH_CPU_CLUSTER) {
		dev->ops = &b06vn_cpu_cluster_ops;
	}
}

BOOT_STATE_INIT_ENTRY(BS_DEV_RESOURCES, BS_ON_EXIT,
	b06vn_resources_assigned, NULL);
BOOT_STATE_INIT_ENTRY(BS_DEV_ENABLE, BS_ON_EXIT,
	b06vn_resources_enabled, NULL);
