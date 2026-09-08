/* SPDX-License-Identifier: GPL-2.0-only */

#include "development_id.h"

#include <arch/cpuid.h>
#include <arch/io.h>
#include <arch/pci_io_cfg.h>
#include <arch/romstage.h>
#if CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
#include <arch/symbols.h>
#endif
#include <commonlib/bsd/compiler.h>
#include <commonlib/helpers.h>
#include <device/pci_def.h>
#include <device/pci_type.h>
#include <device/dram/common.h>
#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 8 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 10 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 11
#include <device/dram/ddr3.h>
#endif
#include <device/smbus_host.h>
#include <drivers/uart/uart8250reg.h>
#include <stdbool.h>
#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 11
#include <cpu/x86/msr.h>
#endif
#include <southbridge/intel/common/lpc_def.h>
#include <southbridge/intel/common/pmutil.h>
#include <southbridge/intel/i82801jx/i82801jx.h>
#if CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
#include "vendor_init.h"
#endif
#if CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
#include "rommon_script.h"
#endif
#if CONFIG_X58_PRO_E_ROMMON_IRQPROBE_PIT
#include "rommon_irqprobe.h"
#endif
#if CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF
#include "b06v6_handoff.h"
#endif
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
_Static_assert(X58_SPD_PROFILE_SIZE == SPD_SIZE_MAX_DDR3 &&
	X58_SPD_PROFILE_SERIAL_OFFSET == SPD_DDR3_SERIAL_NUM &&
	X58_SPD_PROFILE_SERIAL_SIZE == SPD_DDR3_SERIAL_LEN,
	"SPD profile normalization must match DDR3 serial layout");
#endif

#define POST_B00_UNEXPECTED_ROMSTAGE	0xee
#define POST_B03_ROMSTAGE_ENTRY		0xd5
#define POST_B03_UART_SENT		0xd6
#define POST_B03_SUCCESS		0xde
#define POST_B03_UART_ERROR		0xed

#define POST_B04_ICH10R_BEGIN		0xc4
#define POST_B04_ICH10R_READY		0xc5
#define POST_B04_SMBUS_READY		0xc6
#define POST_B04_UART_SENT		0xd7
#define POST_B04_SUCCESS		0xdd
#define POST_B04_RCBA_ERROR		0xf0
#define POST_B04_SMBUS_ABSENT		0xf1
#define POST_B04_SMBUS_ID_ERROR		0xf2
#define POST_B04_SMBUS_CONFIG_ERROR	0xf3
#define POST_B04_PMBASE_ERROR		0xf4
#define POST_B04_GPIOBASE_ERROR		0xf5
#define POST_B04_LPC_INVARIANT_ERROR	0xf6
#define POST_B04_UART_ERROR		0xf7

#define POST_B05_WINDOW_READY		0xc7
#define POST_B05_START_ARMED		0xc8
#define POST_B05_TRANSACTION_DONE	0xc9
#define POST_B05_UART_SENT		0xd8
#define POST_B05_SUCCESS		0xdc
#define POST_B05_CPU_ERROR		0xe0
#define POST_B05_HOST_STATE_ERROR	0xf8
#define POST_B05_TIMEOUT		0xf9
#define POST_B05_NO_DEVICE		0xfa
#define POST_B05_BUS_ERROR		0xfb
#define POST_B05_TRANSACTION_ERROR	0xfc
#define POST_B05_NOT_DDR3		0xfd
#define POST_B05_UART_ERROR		0xfe

#define POST_B06_SINGLE_BASE		0x41
#define POST_B06_MULTIPLE_DDR3		0x49
#define POST_B06_NO_RESPONSE		0x4a
#define POST_B06_BAD_TYPE		0x4b
#define POST_B06_CLOCK_ERROR		0x4c
#define POST_B06_DATA_ERROR		0x4d
#define POST_B06_BOTH_PINS_ERROR	0x4e
#define POST_B06_HOST_STATE_ERROR	0x4f
#define POST_B06_TIMEOUT			0x50
#define POST_B06_BUS_ERROR		0x51
#define POST_B06_TRANSACTION_ERROR	0x52
#define POST_B06_UART_ERROR		0x53
#define POST_B06_PIN_PHASE		0x54
#define POST_B06_CLOCK_RELEASED		0x55
#define POST_B06_PINS_IDLE		0x56
#define POST_B06_CPU_ERROR		0x57
#define POST_B06_PROBE_BASE		0x80
#define POST_B06_DEV_ERR_BASE		0x88
#define POST_B06_DDR3_BASE		0x90
#define POST_B06_OTHER_TYPE_BASE	0x98

#define POST_B06B_READ_BEGIN		0x58
#define POST_B06B_TYPE_OK		0x59
#define POST_B06B_BLOCK_DONE_BASE	0x5a
#define POST_B06B_CRC_OK		0x62
#define POST_B06B_UART_SENT		0x63
#define POST_B06B_SUCCESS		0x64
#define POST_B06B_DEV_ERROR		0x65
#define POST_B06B_NOT_DDR3		0x66
#define POST_B06B_CRC_ERROR		0x67

#define POST_B06C_HEADER_OK		0x68
#define POST_B06C_UPPER_PASS1_BEGIN	0x69
#define POST_B06C_PASS1_BLOCK_BASE	0x6a
#define POST_B06C_UPPER_PASS2_BEGIN	0x6d
#define POST_B06C_PASS2_BLOCK_BASE	0x6e
#define POST_B06C_UPPER_MATCH		0x71
#define POST_B06C_DECODE_OK		0x72
#define POST_B06C_POLICY_OK		0x73
#define POST_B06C_UART_SENT		0x74
#define POST_B06C_SUCCESS		0x75
#define POST_B06C_HEADER_ERROR		0x76
#define POST_B06C_UPPER_MISMATCH	0x77
#define POST_B06C_DECODE_ERROR		0x78
#define POST_B06C_POLICY_ERROR		0x79

#define POST_B06H_HEADER_MARKER		0x7a
#define B06H_HEADER_FIELD_MASK		0x7f
#define B06H_HEADER_FIELD_TAG		0x80

#define POST_B06I_HEADER_OK		0xa0
#define POST_B06I_UPPER_PASS1_BEGIN	0xa1
#define POST_B06I_PASS1_BLOCK_BASE	0xa2
#define POST_B06I_UPPER_PASS2_BEGIN	0xaa
#define POST_B06I_PASS2_BLOCK_BASE	0xab
#define POST_B06I_UPPER_MATCH		0xb3
#define POST_B06I_DECODE_OK		0xb4
#define POST_B06I_POLICY_OK		0xb5
#define POST_B06I_UART_SENT		0xb6
#define POST_B06I_SUCCESS		0xb7
#define POST_B06I_HEADER_ERROR		0xb8
#define POST_B06I_UPPER_MISMATCH	0xb9
#define POST_B06I_DECODE_ERROR		0xba
#define POST_B06I_POLICY_ERROR		0xbb

#define POST_B06J_READY			0xbc
#define POST_B06J_COMMAND		0xbd
#define POST_B06J_RX_ERROR		0xbe
#define POST_B06J_COMMAND_ERROR		0xbf
#define POST_B06J_SERIALICE		0xc7
#define POST_B06V_RESET_INIT		0xcc
#define POST_B06V_RESET_WARM		0xcd
#define POST_B06V_RESET_FULL		0xce
#define POST_B06V0_PCIEXBAR_READY	0xd0
#define POST_B06V0_CSI_CALL		0xd1
#define POST_B06V0_CSI_RETURN		0xd2
#define POST_B06V0_MINIT_CALL		0xd3
#define POST_B06V0_MINIT_RETURN		0xd4
#define POST_B06V6_AUTO_BEGIN		0x02
#define POST_B06V6_PLATFORM_READY	0x03
#define POST_B06V6_CSI_PASS1_ARMED	0x04
#define POST_B06V6_CSI_PASS2_ARMED	0x05
#define POST_B06V6_CSI_ACCEPTED		0x06
#define POST_B06V6_POLICY_INSTALLED	0x07
#define POST_B06V6_MINIT_ACCEPTED	0x08
#define POST_B06V6_GUARD_STOP		0x1f
#define POST_B06V6_AUTO_FALLBACK	0x20
#define POST_B06V8_PASS2_ACCEPTED	0xca
#define POST_B06V8_OUTER_RESET_ARMED	0xcb
#define POST_B06V8_CSI_PASS3_ARMED	0xcf
#define POST_B06V8_CSI_PASS3_RETURN	0xd9
#define POST_B06V8_TERMINAL		0xda
#define POST_B06V8_IOH_SYRE		0xfe
#define POST_B06V9_RTC_PRECHECK		0x10
#define POST_B06V9_RTC_ENABLED		0x11
#define POST_B06V9_PROFILE_READY	0x12
#define POST_B06VA_HIGH_PROFILE_READY	0x13
#define POST_B06V0_ERROR			0xdf
#define POST_B06V1_SCRIPT_RUN		0xe5
#define POST_B06V1_SCRIPT_FAILED	0xe6
#define POST_B06V1_SCRIPT_DONE		0xe7
#define POST_B06V1_ROLLBACK_RUN		0xe8
#define POST_B06V1_ROLLBACK_DONE	0xe9
#define POST_B06V1_ROLLBACK_FAILED	0xea

#define B03_UART_BASE			0x3f8
#define B03_UART_TX_POLL_LIMIT		100000
#define B03_UART_FLUSH_POLL_LIMIT	1000000

#define PCI_VENDOR_ID_INTEL		0x8086
#define PCI_DEVICE_ID_ICH10R_SMBUS	0x3a30
#define PCI_ID_ICH10R_SMBUS \
	((PCI_DEVICE_ID_ICH10R_SMBUS << 16) | PCI_VENDOR_ID_INTEL)
#define ICH10R_LPC_DEV			PCI_DEV(0, 0x1f, 0)
#define ICH10R_SMBUS_DEV		PCI_DEV(0, 0x1f, 3)

#define B04_LPC_IO_DEC			0x0010
#define B04_LPC_EN			(CNF2_LPC_EN | COMA_LPC_EN)

#define B04_RCBA_BASE_MASK		0xffffc000U
#define B04_PMBASE_MASK			0xfffcU
#define B04_GPIOBASE_MASK		0xfffeU
#define B06V9_ICH10_RC			0x3400
#define B06V9_ICH10_RC_U128E		BIT(2)

#if (CONFIG_X58_PRO_E_BRINGUP_STAGE == 4 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 5 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 6 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 7 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 8 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 9 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 10 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 11) && \
	CONFIG_FIXED_SMBUS_IO_BASE != 0x400
#error "B04-B06J have been reviewed only for ICH10R SMBus base 0x400"
#endif

#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 3
static const char b03_romstage_banner[] =
	"\r\n[X58PROE-B03-XIP-ROMSTAGE-20260830]\r\n"
	"[ROMSTAGE] XIP/CAR entry; DRAM/QPI untouched\r\n"
	"POST=de HALT\r\n";

static void __noreturn stop_with_post(uint8_t code)
{
	outb(code, CONFIG_POST_IO_PORT);
	asm volatile (
		"cli\n\t"
		"1: hlt\n\t"
		"jmp 1b"
	);
	__builtin_unreachable();
}

static bool uart_wait_for(uint8_t mask, unsigned int limit)
{
	while (limit--) {
		if ((inb(B03_UART_BASE + UART8250_LSR) & mask) == mask)
			return true;
	}

	return false;
}

static bool uart_puts(const char *string)
{
	while (*string) {
		if (!uart_wait_for(UART8250_LSR_THRE, B03_UART_TX_POLL_LIMIT))
			return false;
		outb(*string++, B03_UART_BASE + UART8250_TBR);
	}

	return uart_wait_for(UART8250_LSR_TEMT, B03_UART_FLUSH_POLL_LIMIT);
}
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 4 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 5 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 6 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 7 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 8 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 9 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 10 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 11
#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 4
static const char b04_romstage_build_id[] =
	"X58PROE-B04-ICH10R-EARLY-CORE-20260831";
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 5
static const char b04_romstage_build_id[] =
	"X58PROE-B05-SINGLE-SPD-PROBE-20260831";
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 6
static const char b04_romstage_build_id[] =
	"X58PROE-B06A-SPD-ADDRESS-DIAG-20260831";
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 7
static const char b04_romstage_build_id[] =
	"X58PROE-B06B-SPD54-BASE128-CRC-20260831";
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 8
static const char b04_romstage_build_id[] =
	"X58PROE-B06C-SPD54-USED176-DECODE-20260831";
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 9
static const char b04_romstage_build_id[] =
	"X58PROE-B06H-SPD54-HEADER-TELEMETRY-20260831";
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 10
static const char b04_romstage_build_id[] =
	"X58PROE-B06I-SPD54-USED256-DECODE-20260831";
#else
static const char b04_romstage_build_id[] =
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
	X58_DEVELOPMENT_BUILD_ID;
#elif CONFIG_X58_PRO_E_B06WK_ACPI_REPAIR
	"X58PROE-B06WK-ACPI-REPAIR-20260908";
#elif CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM
	"X58PROE-B06WJ-ACPI-PLATFORM-20260907";
#elif CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI
	"X58PROE-B06WI-VENDOR-IRQ-ACPI-20260907";
#elif CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS
	"X58PROE-B06WH-AUTO-USB-SEABIOS-20260907";
#elif CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS
	"X58PROE-B06WG-AUTO-USB-SEABIOS-20260907";
#elif CONFIG_X58_PRO_E_B06WF_USB_LAB
	"X58PROE-B06WF-LATE-USB-LAB-20260907";
#elif CONFIG_X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX
	"X58PROE-B06WE-ACPI-SAD-BDF-FIX-20260907";
#elif CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT
	"X58PROE-B06WD-SEABIOS-INPUT-20260906";
#elif CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM
	"X58PROE-B06WC-INTEGRATED-PLATFORM-20260906";
#elif CONFIG_X58_PRO_E_B06WB_TCO_HALT
	"X58PROE-B06WB-ICH10-TCO-HALT-20260906";
#elif CONFIG_X58_PRO_E_B06WA_HPET_DECODE
	"X58PROE-B06WA-ICH10-HPET-DECODE-20260906";
#elif CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES
	"X58PROE-B06VZ-FIXED-RESOURCES-20260906";
#elif CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK
	"X58PROE-B06VY-ICH10-IOAPIC-MASK-20260906";
#elif CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE
	"X58PROE-B06VX-AHCI-USBTRACE-20260906";
#elif CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO
	"X58PROE-B06VW-ICH10-AHCI-MMIO-20260906";
#elif CONFIG_X58_PRO_E_B06VV_ICH10_PCS_SCLK
	"X58PROE-B06VV-ICH10-PCS-SCLK-20260906";
#elif CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE
	"X58PROE-B06VU-ICH10-AHCI-ROUTE-20260906";
#elif CONFIG_X58_PRO_E_B06VQ_ICHBASE1
	"X58PROE-B06VQ-ICHBASE1-20260906";
#elif CONFIG_X58_PRO_E_B06VQ_PLATRO1
	"X58PROE-B06VQ-PLATRO1-20260906";
#elif CONFIG_X58_PRO_E_B06VQ_USB_TRACE1
	"X58PROE-B06VQ-USBTRACE1-20260906";
#elif CONFIG_X58_PRO_E_B06VT_ICH10_SATA_CLOCK
	"X58PROE-B06VT-ICH10-SATA-CLOCK-20260906";
#elif CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS
	"X58PROE-B06VS-ICH10-AHCI-PORTS-20260906";
#elif CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP
	"X58PROE-B06VR-ICH10-AHCI-MAP-20260906";
#elif CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT
	"X58PROE-B06VQ-ICH10-EHCI-INIT-20260906";
#elif CONFIG_X58_PRO_E_B06VP_LAPIC_EXTINT
	"X58PROE-B06VP-LAPIC-EXTINT-PROBE-20260906";
#elif CONFIG_X58_PRO_E_B06VO_IOHBUSNO_ROUTE
	"X58PROE-B06VO-IOHBUSNO-ROUTE-PROBE-20260906";
#elif CONFIG_X58_PRO_E_B06VN_IOU0_PCIE_PHYS_VBIOS
	"X58PROE-B06VN-IOU0-HD5450-PHYSVBIOS-20260906";
#elif CONFIG_X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE
	"X58PROE-B06VM-AUTO-PCI-VGA-IPXE-20260905";
#elif CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	"X58PROE-B06VL-BROAD-HARD-GATE-SEABIOS-20260905";
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
	"X58PROE-B06VK-Q-CANONICAL-CLASS-SEABIOS-20260905";
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
	"X58PROE-B06VJ-COUPLED-PROFILE-Q-SEABIOS-20260905";
#elif CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	"X58PROE-B06VI-COUPLED-PROFILE-O-SEABIOS-20260905";
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
	"X58PROE-B06VH-PRIMARY-WORKSPACE-REARM-SEABIOS-20260905";
#elif CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	"X58PROE-B06VG-COUPLED-MINIT-SEABIOS-20260905";
#elif CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
	"X58PROE-B06VF-SEABIOS-ENTRY-PROBE-20260905";
#elif CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
		"X58PROE-B06VE-HIGHQPI-AUTO-RAMSTAGE-ROMMON-20260905";
#elif CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
		"X58PROE-B06VD-HIGHQPI-CSI2A6-MINIT-ROMMON-20260905";
#elif CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT
	"X58PROE-B06VC-HIGHQPI-A0-4SET-MINIT-ROMMON-20260904";
#elif CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
	"X58PROE-B06VB-HIGHQPI-MINIT-OBSERVE-ROMMON-20260904";
#elif CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET
	"X58PROE-B06VA-HIGHQPI-CPU-A0-ALLOWLIST-ROMMON-20260904";
#elif CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
	"X58PROE-B06V9-DETERMINISTIC-HIGHQPI-3PASS-ROMMON-20260904";
#elif CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
	"X58PROE-B06V8-HIGHQPI-3PASS-PROBE-ROMMON-20260904";
#elif CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
	"X58PROE-B06V7-ROBUST-SLOWQPI-RAMSTAGE-ROMMON-20260904";
#elif CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF
	"X58PROE-B06V6-AUTO-RAMSTAGE-ROMMON-20260904";
#elif CONFIG_X58_PRO_E_B06V5_COLD_MINIT
	"X58PROE-B06V5-COLD-MINIT-GATE-20260904";
#elif CONFIG_X58_PRO_E_B06V4_POLICY_TELEMETRY
	"X58PROE-B06V4-POLICY-TELEMETRY-20260904";
#elif CONFIG_X58_PRO_E_B06V3_WARM_RESUME
	"X58PROE-B06V3-CSI-WARM-RESUME-20260904";
#elif CONFIG_X58_PRO_E_B06V2_CSI_HEADER_FIX
	"X58PROE-B06V2-CSI-HEADER-FIX-20260904";
#elif CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
	"X58PROE-B06V1-TRANSACTIONAL-ROMMON-20260903";
#elif CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
	"X58PROE-B06V0-VENDOR-ASSISTED-PROBE-20260901";
#elif CONFIG_X58_PRO_E_B06N_TIMED_MRS
	"X58PROE-B06N-TIMED-MRS-BASEINIT-20260901";
#elif CONFIG_X58_PRO_E_B06M_BASEINIT
	"X58PROE-B06M-BASEINIT-EXACT-RD-20260901";
#else
	"X58PROE-B06L-EXACT-RD-SWEEP-20260901";
#endif
#endif

struct b04_ich10_snapshot {
	uint32_t rcba;
	uint32_t pmbase;
	uint32_t gpiobase;
	uint32_t generic[4];
	uint16_t lpc_io_dec;
	uint16_t lpc_en;
	uint8_t acpi_cntl;
	uint8_t gpio_cntl;
	uint8_t serirq_cntl;
};

struct b04_smbus_snapshot {
	uint32_t id;
	uint32_t bar4;
	uint16_t command;
	uint8_t hostc;
};

#if CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
struct b06v9_rtc_upper_bank_state {
	uint32_t before;
	uint32_t after;
	bool enabled;
};
#endif

static void __noreturn stop_with_post(uint8_t code)
{
	outb(code, CONFIG_POST_IO_PORT);
	asm volatile (
		"cli\n\t"
		"1: hlt\n\t"
		"jmp 1b"
	);
	__builtin_unreachable();
}

static void capture_ich10(struct b04_ich10_snapshot *snapshot)
{
	static const uint16_t generic_regs[] = {
		LPC_GEN1_DEC, LPC_GEN2_DEC, LPC_GEN3_DEC, LPC_GEN4_DEC,
	};
	size_t i;

	snapshot->rcba = pci_io_read_config32(ICH10R_LPC_DEV, RCBA);
	snapshot->pmbase = pci_io_read_config32(ICH10R_LPC_DEV, D31F0_PMBASE);
	snapshot->gpiobase = pci_io_read_config32(ICH10R_LPC_DEV, GPIOBASE);
	snapshot->lpc_io_dec = pci_io_read_config16(ICH10R_LPC_DEV, LPC_IO_DEC);
	snapshot->lpc_en = pci_io_read_config16(ICH10R_LPC_DEV, LPC_EN);
	snapshot->acpi_cntl =
		pci_io_read_config8(ICH10R_LPC_DEV, D31F0_ACPI_CNTL);
	snapshot->gpio_cntl =
		pci_io_read_config8(ICH10R_LPC_DEV, D31F0_GPIO_CNTL);
	snapshot->serirq_cntl =
		pci_io_read_config8(ICH10R_LPC_DEV, D31F0_SERIRQ_CNTL);

	for (i = 0; i < ARRAY_SIZE(generic_regs); i++)
		snapshot->generic[i] =
			pci_io_read_config32(ICH10R_LPC_DEV, generic_regs[i]);
}

static void capture_smbus(struct b04_smbus_snapshot *snapshot)
{
	snapshot->id = pci_io_read_config32(ICH10R_SMBUS_DEV, PCI_VENDOR_ID);
	snapshot->bar4 = pci_io_read_config32(ICH10R_SMBUS_DEV, SMB_BASE);
	snapshot->command =
		pci_io_read_config16(ICH10R_SMBUS_DEV, PCI_COMMAND);
	snapshot->hostc = pci_io_read_config8(ICH10R_SMBUS_DEV, HOSTC);
}

static uint8_t ich10_preflight_error(const struct b04_ich10_snapshot *before)
{
	/* Never relocate a window that an earlier reset phase left decoding. */
	if ((before->rcba & 1) &&
	    (before->rcba & B04_RCBA_BASE_MASK) != CONFIG_FIXED_RCBA_MMIO_BASE)
		return POST_B04_RCBA_ERROR;
	if ((before->acpi_cntl & 0x80) &&
	    (before->pmbase & B04_PMBASE_MASK) != DEFAULT_PMBASE)
		return POST_B04_PMBASE_ERROR;
	if ((before->gpio_cntl & 0x10) &&
	    (before->gpiobase & B04_GPIOBASE_MASK) != DEFAULT_GPIOBASE)
		return POST_B04_GPIOBASE_ERROR;

	return 0;
}

static uint8_t ich10_readback_error(const struct b04_ich10_snapshot *before,
				    const struct b04_ich10_snapshot *after)
{
	size_t i;

	if ((after->rcba & B04_RCBA_BASE_MASK) != CONFIG_FIXED_RCBA_MMIO_BASE ||
	    !(after->rcba & 1))
		return POST_B04_RCBA_ERROR;
	if ((after->pmbase & B04_PMBASE_MASK) != DEFAULT_PMBASE ||
	    !(after->pmbase & 1) ||
	    after->acpi_cntl != 0x80)
		return POST_B04_PMBASE_ERROR;
	if ((after->gpiobase & B04_GPIOBASE_MASK) != DEFAULT_GPIOBASE ||
	    after->gpio_cntl != (before->gpio_cntl | 0x10))
		return POST_B04_GPIOBASE_ERROR;
	if (after->serirq_cntl != before->serirq_cntl ||
	    after->lpc_io_dec != B04_LPC_IO_DEC ||
	    after->lpc_en != B04_LPC_EN)
		return POST_B04_LPC_INVARIANT_ERROR;

	/* This board intentionally has no generic LPC windows in B04. */
	for (i = 0; i < ARRAY_SIZE(after->generic); i++) {
		if (after->generic[i] != before->generic[i] ||
		    (after->generic[i] & LPC_LGIR_EN))
			return POST_B04_LPC_INVARIANT_ERROR;
	}

	return 0;
}

static bool smbus_readback_valid(const struct b04_smbus_snapshot *after)
{
	return after->id == PCI_ID_ICH10R_SMBUS &&
		(after->bar4 & ~PCI_BASE_ADDRESS_IO_ATTR_MASK) ==
			CONFIG_FIXED_SMBUS_IO_BASE &&
		(after->bar4 & PCI_BASE_ADDRESS_SPACE_IO) &&
		after->hostc == HST_EN && after->command == PCI_COMMAND_IO;
}

#if CONFIG_X58_PRO_E_B06V3_WARM_RESUME
enum b06v3_smbus_entry_state {
	B06V3_SMBUS_ENTRY_COLD,
	B06V3_SMBUS_ENTRY_CONFIGURED_AFTER_RESET,
};

static bool smbus_is_exact_cold_default(const struct b04_smbus_snapshot *before)
{
	return before->bar4 == PCI_BASE_ADDRESS_SPACE_IO &&
		before->hostc == 0 && before->command == 0;
}

static bool smbus_is_exact_configured(const struct b04_smbus_snapshot *before)
{
	return before->bar4 ==
		(CONFIG_FIXED_SMBUS_IO_BASE | PCI_BASE_ADDRESS_SPACE_IO) &&
		before->hostc == HST_EN && before->command == PCI_COMMAND_IO;
}
#endif

static bool b04_uart_wait_for(uint8_t mask, unsigned int limit)
{
	while (limit--) {
		if ((inb(B03_UART_BASE + UART8250_LSR) & mask) == mask)
			return true;
	}

	return false;
}

static bool b04_uart_putc(uint8_t value)
{
	if (!b04_uart_wait_for(UART8250_LSR_THRE, B03_UART_TX_POLL_LIMIT))
		return false;
	outb(value, B03_UART_BASE + UART8250_TBR);
	return true;
}

static bool b04_uart_puts(const char *string)
{
	while (*string) {
		if (!b04_uart_putc(*string++))
			return false;
	}

	return true;
}

static bool b04_uart_put_hex(uint32_t value, unsigned int digits)
{
	static const char hex[] = "0123456789abcdef";

	while (digits--) {
		const unsigned int shift = digits * 4;

		if (!b04_uart_putc(hex[(value >> shift) & 0xf]))
			return false;
	}

	return true;
}

#if CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
static void b06v9_enable_rtc_upper_bank(
	struct b06v9_rtc_upper_bank_state *state)
{
	outb(POST_B06V9_RTC_PRECHECK, CONFIG_POST_IO_PORT);
	state->before = RCBA32(B06V9_ICH10_RC);
	state->after = state->before;
	state->enabled = false;

	/*
	 * ICH10 Datasheet 319973-003 sections 10.1.73 and 13.6: RC bit 2
	 * selects the upper 128-byte RTC bank at ports 0x72/0x73.  Accept only
	 * the two states observed on the target and vendor-booted reference.
	 */
	if (state->before != 0 && state->before != B06V9_ICH10_RC_U128E)
		return;
	RCBA32(B06V9_ICH10_RC) = state->before | B06V9_ICH10_RC_U128E;
	state->after = RCBA32(B06V9_ICH10_RC);
	if (state->after != B06V9_ICH10_RC_U128E)
		return;

	state->enabled = true;
	outb(POST_B06V9_RTC_ENABLED, CONFIG_POST_IO_PORT);
}

static bool b06v9_report_rtc_upper_bank(
	const struct b06v9_rtc_upper_bank_state *state)
{
	return b04_uart_puts("\r\n[ICH10] RTC_RC PRE=") &&
		b04_uart_put_hex(state->before, 8) &&
		b04_uart_puts(" POST=") && b04_uart_put_hex(state->after, 8) &&
		b04_uart_puts(" U128E=") &&
		b04_uart_puts(state->enabled ? "PASS" : "FAIL") &&
		b04_uart_puts(
			"; exact gate before vendor code; no RTC data read/write\r\n");
}
#endif

static bool report_ich10(const struct b04_ich10_snapshot *before,
			 const struct b04_ich10_snapshot *after,
			 const struct b04_smbus_snapshot *smbus_before,
			 const struct b04_smbus_snapshot *smbus_after)
{
	if (!b04_uart_puts("\r\n[") || !b04_uart_puts(b04_romstage_build_id) ||
	    !b04_uart_puts("]\r\n[ICH10] upstream i82801jx early BAR core\r\n"
			   "[ICH10] PRE  RCBA="))
		return false;
	if (!b04_uart_put_hex(before->rcba, 8) ||
	    !b04_uart_puts(" PMBASE=") || !b04_uart_put_hex(before->pmbase, 8) ||
	    !b04_uart_puts(" ACPI=") || !b04_uart_put_hex(before->acpi_cntl, 2) ||
	    !b04_uart_puts(" GPIOBASE=") || !b04_uart_put_hex(before->gpiobase, 8) ||
	    !b04_uart_puts(" GPIOCNTL=") || !b04_uart_put_hex(before->gpio_cntl, 2) ||
	    !b04_uart_puts(" SERIRQ=") || !b04_uart_put_hex(before->serirq_cntl, 2) ||
	    !b04_uart_puts(" LPCIO=") || !b04_uart_put_hex(before->lpc_io_dec, 4) ||
	    !b04_uart_puts(" LPCEN=") || !b04_uart_put_hex(before->lpc_en, 4))
		return false;
	if (!b04_uart_puts("\r\n[ICH10] POST RCBA=") ||
	    !b04_uart_put_hex(after->rcba, 8) ||
	    !b04_uart_puts(" PMBASE=") || !b04_uart_put_hex(after->pmbase, 8) ||
	    !b04_uart_puts(" ACPI=") || !b04_uart_put_hex(after->acpi_cntl, 2) ||
	    !b04_uart_puts(" GPIOBASE=") || !b04_uart_put_hex(after->gpiobase, 8) ||
	    !b04_uart_puts(" GPIOCNTL=") || !b04_uart_put_hex(after->gpio_cntl, 2) ||
	    !b04_uart_puts(" SERIRQ=") || !b04_uart_put_hex(after->serirq_cntl, 2) ||
	    !b04_uart_puts(" LPCIO=") || !b04_uart_put_hex(after->lpc_io_dec, 4) ||
	    !b04_uart_puts(" LPCEN=") || !b04_uart_put_hex(after->lpc_en, 4))
		return false;
	if (!b04_uart_puts("\r\n[SMBUS] ID=8086:3a30 PRE BAR4=") ||
	    !b04_uart_put_hex(smbus_before->bar4, 8) ||
	    !b04_uart_puts(" HOSTC=") || !b04_uart_put_hex(smbus_before->hostc, 2) ||
	    !b04_uart_puts(" CMD=") || !b04_uart_put_hex(smbus_before->command, 4) ||
	    !b04_uart_puts(" POST BAR4=") || !b04_uart_put_hex(smbus_after->bar4, 8) ||
	    !b04_uart_puts(" HOSTC=") || !b04_uart_put_hex(smbus_after->hostc, 2) ||
	    !b04_uart_puts(" CMD=") || !b04_uart_put_hex(smbus_after->command, 4))
		return false;

#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 4
	return b04_uart_puts("\r\n[SMBUS] controller only; no status/SPD transaction\r\n"
			       "POST=dd HALT\r\n") &&
		b04_uart_wait_for(UART8250_LSR_TEMT,
				  B03_UART_FLUSH_POLL_LIMIT);
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 5
	return b04_uart_puts("\r\n[SMBUS] controller ready; one bounded SPD probe follows\r\n") &&
		b04_uart_wait_for(UART8250_LSR_TEMT,
				  B03_UART_FLUSH_POLL_LIMIT);
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 6
	return b04_uart_puts("\r\n[SMBUS] controller ready; bounded SPD address diagnostic follows\r\n") &&
		b04_uart_wait_for(UART8250_LSR_TEMT,
				  B03_UART_FLUSH_POLL_LIMIT);
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 7
	return b04_uart_puts("\r\n[SMBUS] controller ready; SPD 0x54 base-block CRC read follows\r\n") &&
		b04_uart_wait_for(UART8250_LSR_TEMT,
				  B03_UART_FLUSH_POLL_LIMIT);
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 8
	return b04_uart_puts("\r\n[SMBUS] controller ready; SPD 0x54 used-176 verification follows\r\n") &&
		b04_uart_wait_for(UART8250_LSR_TEMT,
				  B03_UART_FLUSH_POLL_LIMIT);
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 9
	return b04_uart_puts("\r\n[SMBUS] controller ready; SPD 0x54 header telemetry follows\r\n") &&
		b04_uart_wait_for(UART8250_LSR_TEMT,
				  B03_UART_FLUSH_POLL_LIMIT);
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 10
	return b04_uart_puts("\r\n[SMBUS] controller ready; SPD 0x54 used-256 verification follows\r\n") &&
		b04_uart_wait_for(UART8250_LSR_TEMT,
				  B03_UART_FLUSH_POLL_LIMIT);
#else
	return b04_uart_puts("\r\n[ROMMON] controller ready; interactive CAR monitor follows\r\n") &&
		b04_uart_wait_for(UART8250_LSR_TEMT,
				  B03_UART_FLUSH_POLL_LIMIT);
#endif
}

#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 5 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 6 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 7 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 8 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 9 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 10 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 11
/* ICH10 I801 host-register offsets used by coreboot's common SMBus driver. */
#define I801_HSTSTAT			0x00
#define I801_HSTCTL			0x02
#define I801_HSTCMD			0x03
#define I801_XMITADD			0x04
#define I801_HSTDAT0			0x05
#define I801_HSTDAT1			0x06

#define I801_BYTE_DATA			(2 << 2)
#define I801_HSTCTL_START		BIT(6)

#define I801_HSTSTAT_BYTE_DONE		BIT(7)
#define I801_HSTSTAT_INUSE		BIT(6)
#define I801_HSTSTAT_SMBALERT		BIT(5)
#define I801_HSTSTAT_FAILED		BIT(4)
#define I801_HSTSTAT_BUS_ERR		BIT(3)
#define I801_HSTSTAT_DEV_ERR		BIT(2)
#define I801_HSTSTAT_INTR		BIT(1)
#define I801_HSTSTAT_HOST_BUSY		BIT(0)

#define I801_HSTSTAT_TERMINAL \
	(I801_HSTSTAT_FAILED | I801_HSTSTAT_BUS_ERR | \
	 I801_HSTSTAT_DEV_ERR | I801_HSTSTAT_INTR)
#define I801_HSTSTAT_RESULT \
	(I801_HSTSTAT_FAILED | I801_HSTSTAT_BUS_ERR | I801_HSTSTAT_DEV_ERR | \
	 I801_HSTSTAT_INTR | I801_HSTSTAT_HOST_BUSY)
#define I801_HSTSTAT_FLAGS \
	(I801_HSTSTAT_BYTE_DONE | I801_HSTSTAT_FAILED | \
	 I801_HSTSTAT_BUS_ERR | I801_HSTSTAT_DEV_ERR | I801_HSTSTAT_INTR)

#define I801_PIN_CTL			0x0f
#define I801_PIN_CLK_STATUS		BIT(0)
#define I801_PIN_DATA_STATUS		BIT(1)
#define I801_PIN_CLK_CTL			BIT(2)

#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 5

#define B05_I801_POLL_LIMIT		1000000U
#define B05_SPD_ADDRESS			0x50
#define B05_SPD_MEMORY_TYPE_OFFSET	0x02
#define B05_SPD_DDR3_MEMORY_TYPE		0x0b

struct b05_i801_snapshot {
	uint8_t status;
	uint8_t control;
	uint8_t command;
	uint8_t xmit_address;
	uint8_t data0;
	uint8_t data1;
};

struct b05_probe_result {
	struct b05_i801_snapshot before;
	struct b05_i801_snapshot armed;
	uint32_t polls;
	uint8_t transaction_status;
	uint8_t value;
};

static void b05_capture_i801_command(struct b05_i801_snapshot *snapshot)
{
	const uintptr_t base = CONFIG_FIXED_SMBUS_IO_BASE;

	/* Do not read HSTSTAT here: that register owns/releases the semaphore. */
	snapshot->control = inb(base + I801_HSTCTL);
	snapshot->command = inb(base + I801_HSTCMD);
	snapshot->xmit_address = inb(base + I801_XMITADD);
	snapshot->data0 = inb(base + I801_HSTDAT0);
	snapshot->data1 = inb(base + I801_HSTDAT1);
}

static void b05_release_host(uint8_t status)
{
	const uintptr_t base = CONFIG_FIXED_SMBUS_IO_BASE;

	/* INUSE releases ownership; the observed completion bits are W1C. */
	outb(I801_HSTSTAT_INUSE | (status & I801_HSTSTAT_FLAGS),
	     base + I801_HSTSTAT);
}

static uint8_t b05_probe_spd_type(struct b05_probe_result *result)
{
	const uintptr_t base = CONFIG_FIXED_SMBUS_IO_BASE;
	uint32_t loops;
	uint8_t status = 0;
	uint8_t masked_status;

	/* Read HSTSTAT first; touch no other host register if another owner is busy. */
	result->before.status = inb(base + I801_HSTSTAT);
	if (result->before.status &
	    (I801_HSTSTAT_INUSE | I801_HSTSTAT_HOST_BUSY))
		return POST_B05_HOST_STATE_ERROR;

	/* A clear first read acquired the host semaphore for this one command. */
	b05_capture_i801_command(&result->before);
	if ((result->before.status & I801_HSTSTAT_FLAGS) ||
	    result->before.control != 0) {
		/* Release the semaphore but preserve the unexpected cold-state flags. */
		b05_release_host(0);
		return POST_B05_HOST_STATE_ERROR;
	}
	outb(POST_B05_WINDOW_READY, CONFIG_POST_IO_PORT);

	/* Exactly one SMBus read-byte-data command: SPD 0x50, byte 2. */
	outb(I801_BYTE_DATA, base + I801_HSTCTL);
	outb((B05_SPD_ADDRESS << 1) | 1, base + I801_XMITADD);
	outb(B05_SPD_MEMORY_TYPE_OFFSET, base + I801_HSTCMD);
	outb(0, base + I801_HSTDAT0);
	outb(0, base + I801_HSTDAT1);
	b05_capture_i801_command(&result->armed);
	if (result->armed.control != I801_BYTE_DATA ||
	    result->armed.xmit_address != ((B05_SPD_ADDRESS << 1) | 1) ||
	    result->armed.command != B05_SPD_MEMORY_TYPE_OFFSET ||
	    result->armed.data0 != 0 || result->armed.data1 != 0) {
		b05_release_host(0);
		return POST_B05_HOST_STATE_ERROR;
	}

	/* Mark the last safe boundary immediately before the one START write. */
	outb(POST_B05_START_ARMED, CONFIG_POST_IO_PORT);
	outb(I801_BYTE_DATA | I801_HSTCTL_START, base + I801_HSTCTL);

	/*
	 * There is no verified pre-RAM clock yet.  Bound the experiment by one
	 * million serialized I/O reads; on timeout do not guess at KILL/recovery.
	 */
	for (loops = B05_I801_POLL_LIMIT; loops; loops--) {
		status = inb(base + I801_HSTSTAT);
		if (!(status & I801_HSTSTAT_HOST_BUSY) &&
		    (status & I801_HSTSTAT_TERMINAL))
			break;
	}
	result->polls = loops ? B05_I801_POLL_LIMIT - loops + 1 :
		B05_I801_POLL_LIMIT;
	result->transaction_status = status;
	if (!loops)
		return POST_B05_TIMEOUT;

	masked_status = status & I801_HSTSTAT_RESULT;
	if (masked_status == I801_HSTSTAT_INTR)
		result->value = inb(base + I801_HSTDAT0);

	/* Release only after BUSY cleared.  A timeout deliberately skips this. */
	b05_release_host(status);

	if (masked_status == I801_HSTSTAT_INTR) {
		outb(POST_B05_TRANSACTION_DONE, CONFIG_POST_IO_PORT);
		if (result->value != B05_SPD_DDR3_MEMORY_TYPE)
			return POST_B05_NOT_DDR3;
		return 0;
	}
	if (masked_status == I801_HSTSTAT_DEV_ERR)
		return POST_B05_NO_DEVICE;
	if (masked_status == I801_HSTSTAT_BUS_ERR)
		return POST_B05_BUS_ERROR;

	return POST_B05_TRANSACTION_ERROR;
}

static bool b05_report_preflight(const struct b05_i801_snapshot *snapshot)
{
	return b04_uart_puts("[SMBUS] PRE   STS=") &&
		b04_uart_put_hex(snapshot->status, 2) &&
		b04_uart_puts(" CTL=") && b04_uart_put_hex(snapshot->control, 2) &&
		b04_uart_puts(" CMD=") && b04_uart_put_hex(snapshot->command, 2) &&
		b04_uart_puts(" XMIT=") &&
		b04_uart_put_hex(snapshot->xmit_address, 2) &&
		b04_uart_puts(" D0=") && b04_uart_put_hex(snapshot->data0, 2) &&
		b04_uart_puts(" D1=") && b04_uart_put_hex(snapshot->data1, 2) &&
		b04_uart_puts("\r\n");
}

static bool b05_report_armed(const struct b05_i801_snapshot *snapshot)
{
	return b04_uart_puts("[SMBUS] ARMED CTL=") &&
		b04_uart_put_hex(snapshot->control, 2) &&
		b04_uart_puts(" CMD=") && b04_uart_put_hex(snapshot->command, 2) &&
		b04_uart_puts(" XMIT=") &&
		b04_uart_put_hex(snapshot->xmit_address, 2) &&
		b04_uart_puts(" D0=") && b04_uart_put_hex(snapshot->data0, 2) &&
		b04_uart_puts(" D1=") && b04_uart_put_hex(snapshot->data1, 2) &&
		b04_uart_puts("\r\n");
}

static bool b05_report_probe(const struct b05_probe_result *result,
			     uint8_t error)
{
	if (!b05_report_preflight(&result->before) ||
	    !b05_report_armed(&result->armed) ||
	    !b04_uart_puts("[SMBUS] DONE STS=") ||
	    !b04_uart_put_hex(result->transaction_status, 2) ||
	    !b04_uart_puts(" POLLS=") || !b04_uart_put_hex(result->polls, 8) ||
	    !b04_uart_puts(" DATA=") || !b04_uart_put_hex(result->value, 2) ||
	    !b04_uart_puts("\r\n[SPD] READ_BYTE_DATA A=50 OFFSET=02"))
		return false;

	if (!error) {
		if (!b04_uart_puts(" TYPE=0b DDR3 VERIFIED\r\n"
				   "[SPD] no EEPROM data-payload write; no IMC/QPI/DDR\r\n"
				   "POST=dc HALT\r\n"))
			return false;
	} else {
		if (!b04_uart_puts(" ERROR POST=") ||
		    !b04_uart_put_hex(error, 2) ||
		    !b04_uart_puts("\r\n"))
			return false;
		if (error == POST_B05_TIMEOUT &&
		    !b04_uart_puts("[SMBUS] timeout not recovered; remove AC power\r\n"))
			return false;
	}

	return b04_uart_wait_for(UART8250_LSR_TEMT,
				 B03_UART_FLUSH_POLL_LIMIT);
}
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 6
#define B06_I801_POLL_LIMIT		1000000U
#define B06_PIN_POLL_LIMIT		100000U
#define B06_SPD_FIRST_ADDRESS		0x50
#define B06_SPD_ADDRESS_COUNT		8
#define B06_SPD_MEMORY_TYPE_OFFSET	0x02
#define B06_SPD_DDR3_MEMORY_TYPE	0x0b

struct b06_probe_record {
	uint32_t polls;
	uint8_t status;
	uint8_t value;
	uint8_t pin_before;
	uint8_t pin_after;
	bool value_valid;
	bool pin_after_valid;
};

struct b06_scan_result {
	struct b06_probe_record probe[B06_SPD_ADDRESS_COUNT];
	uint8_t initial_status;
	uint8_t initial_control;
	uint8_t initial_pin;
	uint8_t enabled_pin;
	uint8_t final_pin;
	uint32_t pin_polls;
	uint8_t last_address;
	uint8_t attempts;
	uint8_t response_bitmap;
	uint8_t dev_err_bitmap;
	uint8_t ddr3_bitmap;
	uint8_t unexpected_type_bitmap;
	bool initial_control_valid;
	bool initial_pin_valid;
	bool enabled_pin_valid;
	bool final_pin_valid;
	bool last_address_valid;
};

static uint8_t b06_pin_error(uint8_t pin)
{
	const bool clock_error = !(pin & I801_PIN_CLK_CTL) ||
		!(pin & I801_PIN_CLK_STATUS);
	const bool data_error = !(pin & I801_PIN_DATA_STATUS);

	if (clock_error && data_error)
		return POST_B06_BOTH_PINS_ERROR;
	if (clock_error)
		return POST_B06_CLOCK_ERROR;
	if (data_error)
		return POST_B06_DATA_ERROR;

	return 0;
}

static void b06_release_host(uint8_t status)
{
	const uintptr_t base = CONFIG_FIXED_SMBUS_IO_BASE;

	/* INUSE releases ownership; completion flags are write-one-to-clear. */
	outb(I801_HSTSTAT_INUSE | (status & I801_HSTSTAT_FLAGS),
	     base + I801_HSTSTAT);
}

static uint8_t b06_scan_spd_types(struct b06_scan_result *result)
{
	const uintptr_t base = CONFIG_FIXED_SMBUS_IO_BASE;
	uint8_t terminal;
	unsigned int index;

	/* The first HSTSTAT read acquires the I801 software semaphore. */
	result->initial_status = inb(base + I801_HSTSTAT);
	if (result->initial_status & I801_HSTSTAT_INUSE)
		return POST_B06_HOST_STATE_ERROR;
	if (result->initial_status & I801_HSTSTAT_HOST_BUSY) {
		/* This read acquired a previously free semaphore; give it back. */
		b06_release_host(0);
		return POST_B06_HOST_STATE_ERROR;
	}

	result->initial_control = inb(base + I801_HSTCTL);
	result->initial_control_valid = true;
	if ((result->initial_status & I801_HSTSTAT_FLAGS) ||
	    result->initial_control != 0) {
		b06_release_host(0);
		return POST_B06_HOST_STATE_ERROR;
	}

	outb(POST_B06_PIN_PHASE, CONFIG_POST_IO_PORT);
	result->initial_pin = inb(base + I801_PIN_CTL);
	result->initial_pin_valid = true;
	/*
	 * ICH10 EDS SMBus_PIN_CTL[2] releases SMBCLK for normal operation;
	 * Specification Update 319974-017, documentation change 13, corrects
	 * its reset default to one. Bits 1:0 are read-only pin levels and
	 * reserved bits are written as zero.
	 */
	outb(I801_PIN_CLK_CTL, base + I801_PIN_CTL);
	outb(POST_B06_CLOCK_RELEASED, CONFIG_POST_IO_PORT);
	do {
		result->enabled_pin = inb(base + I801_PIN_CTL);
		result->enabled_pin_valid = true;
		result->pin_polls++;
		if ((result->enabled_pin &
		     (I801_PIN_CLK_CTL | I801_PIN_DATA_STATUS |
		      I801_PIN_CLK_STATUS)) ==
		    (I801_PIN_CLK_CTL | I801_PIN_DATA_STATUS |
		     I801_PIN_CLK_STATUS))
			break;
	} while (result->pin_polls < B06_PIN_POLL_LIMIT);
	terminal = b06_pin_error(result->enabled_pin);
	if (terminal) {
		result->final_pin = result->enabled_pin;
		result->final_pin_valid = true;
		b06_release_host(0);
		return terminal;
	}
	outb(POST_B06_PINS_IDLE, CONFIG_POST_IO_PORT);

	/* Keep the semaphore until all eight one-shot probes have completed. */
	for (index = 0; index < B06_SPD_ADDRESS_COUNT; index++) {
		struct b06_probe_record *record = &result->probe[index];
		const uint8_t address = B06_SPD_FIRST_ADDRESS + index;
		uint32_t loops;
		uint8_t masked_status;

		result->last_address = address;
		result->last_address_valid = true;
		record->pin_before = inb(base + I801_PIN_CTL);
		terminal = b06_pin_error(record->pin_before);
		if (terminal) {
			result->final_pin = record->pin_before;
			result->final_pin_valid = true;
			b06_release_host(0);
			return terminal;
		}

		outb(I801_BYTE_DATA, base + I801_HSTCTL);
		outb((address << 1) | 1, base + I801_XMITADD);
		outb(B06_SPD_MEMORY_TYPE_OFFSET, base + I801_HSTCMD);
		outb(0, base + I801_HSTDAT0);
		outb(0, base + I801_HSTDAT1);
		if (inb(base + I801_HSTCTL) != I801_BYTE_DATA ||
		    inb(base + I801_XMITADD) != ((address << 1) | 1) ||
		    inb(base + I801_HSTCMD) != B06_SPD_MEMORY_TYPE_OFFSET ||
		    inb(base + I801_HSTDAT0) != 0 ||
		    inb(base + I801_HSTDAT1) != 0) {
			result->final_pin = inb(base + I801_PIN_CTL);
			result->final_pin_valid = true;
			b06_release_host(0);
			return POST_B06_HOST_STATE_ERROR;
		}

		result->attempts = index + 1;
		outb(POST_B06_PROBE_BASE + index, CONFIG_POST_IO_PORT);
		outb(I801_BYTE_DATA | I801_HSTCTL_START,
		     base + I801_HSTCTL);

		for (loops = B06_I801_POLL_LIMIT; loops; loops--) {
			record->status = inb(base + I801_HSTSTAT);
			if (!(record->status & I801_HSTSTAT_HOST_BUSY) &&
			    (record->status & I801_HSTSTAT_TERMINAL))
				break;
		}
		record->polls = loops ? B06_I801_POLL_LIMIT - loops + 1 :
			B06_I801_POLL_LIMIT;
		if (!loops)
			return POST_B06_TIMEOUT;

		masked_status = record->status & I801_HSTSTAT_RESULT;
		if (masked_status == I801_HSTSTAT_INTR) {
			record->value = inb(base + I801_HSTDAT0);
			record->value_valid = true;
		}
		record->pin_after = inb(base + I801_PIN_CTL);
		record->pin_after_valid = true;

		/* Clear this completed command, but retain semaphore ownership. */
		outb(record->status & I801_HSTSTAT_FLAGS,
		     base + I801_HSTSTAT);

		terminal = b06_pin_error(record->pin_after);
		if (terminal) {
			result->final_pin = record->pin_after;
			result->final_pin_valid = true;
			b06_release_host(0);
			return terminal;
		}

		if (masked_status == I801_HSTSTAT_DEV_ERR) {
			result->dev_err_bitmap |= BIT(index);
			outb(POST_B06_DEV_ERR_BASE + index, CONFIG_POST_IO_PORT);
			continue;
		}
		if (masked_status == I801_HSTSTAT_BUS_ERR) {
			result->final_pin = record->pin_after;
			result->final_pin_valid = true;
			b06_release_host(0);
			return POST_B06_BUS_ERROR;
		}
		if (masked_status != I801_HSTSTAT_INTR) {
			result->final_pin = record->pin_after;
			result->final_pin_valid = true;
			b06_release_host(0);
			return POST_B06_TRANSACTION_ERROR;
		}

		result->response_bitmap |= BIT(index);
		if (record->value == B06_SPD_DDR3_MEMORY_TYPE) {
			result->ddr3_bitmap |= BIT(index);
			outb(POST_B06_DDR3_BASE + index, CONFIG_POST_IO_PORT);
		} else {
			result->unexpected_type_bitmap |= BIT(index);
			outb(POST_B06_OTHER_TYPE_BASE + index, CONFIG_POST_IO_PORT);
		}
	}

	result->final_pin = inb(base + I801_PIN_CTL);
	result->final_pin_valid = true;
	terminal = b06_pin_error(result->final_pin);
	b06_release_host(0);
	if (terminal)
		return terminal;
	if (result->unexpected_type_bitmap)
		return POST_B06_BAD_TYPE;
	if (!result->ddr3_bitmap)
		return POST_B06_NO_RESPONSE;
	if (result->ddr3_bitmap & (result->ddr3_bitmap - 1))
		return POST_B06_MULTIPLE_DDR3;

	for (index = 0; index < B06_SPD_ADDRESS_COUNT; index++) {
		if (result->ddr3_bitmap & BIT(index))
			return POST_B06_SINGLE_BASE + index;
	}

	return POST_B06_TRANSACTION_ERROR;
}

static bool b06_report_optional_byte(bool valid, uint8_t value)
{
	if (!valid)
		return b04_uart_puts("NA");

	return b04_uart_put_hex(value, 2);
}

static bool b06_report_scan(const struct b06_scan_result *result,
			    uint8_t terminal)
{
	unsigned int index;

	if (!b04_uart_puts("[SMBUS] PRE STS=") ||
	    !b04_uart_put_hex(result->initial_status, 2) ||
	    !b04_uart_puts(" CTL=") ||
	    !b06_report_optional_byte(result->initial_control_valid,
				      result->initial_control) ||
	    !b04_uart_puts(" PIN_PRE=") ||
	    !b06_report_optional_byte(result->initial_pin_valid,
				      result->initial_pin) ||
	    !b04_uart_puts(" PIN_ENABLED=") ||
	    !b06_report_optional_byte(result->enabled_pin_valid,
				      result->enabled_pin) ||
	    !b04_uart_puts(" PIN_POLLS=") ||
	    !b04_uart_put_hex(result->pin_polls, 8) ||
	    !b04_uart_puts(" LAST_A=") ||
	    !b06_report_optional_byte(result->last_address_valid,
				      result->last_address) ||
	    !b04_uart_puts("\r\n"))
		return false;

	for (index = 0; index < result->attempts; index++) {
		const struct b06_probe_record *record = &result->probe[index];

		if (!b04_uart_puts("[SPD] A=") ||
		    !b04_uart_put_hex(B06_SPD_FIRST_ADDRESS + index, 2) ||
		    !b04_uart_puts(" PIN0=") ||
		    !b04_uart_put_hex(record->pin_before, 2) ||
		    !b04_uart_puts(" STS=") ||
		    !b04_uart_put_hex(record->status, 2) ||
		    !b04_uart_puts(" POLLS=") ||
		    !b04_uart_put_hex(record->polls, 8) ||
		    !b04_uart_puts(" DATA=") ||
		    !b06_report_optional_byte(record->value_valid,
					      record->value) ||
		    !b04_uart_puts(" PIN1=") ||
		    !b06_report_optional_byte(record->pin_after_valid,
					      record->pin_after) ||
		    !b04_uart_puts("\r\n"))
			return false;
	}

	if (!b04_uart_puts("[SPD] ACKMAP=") ||
	    !b04_uart_put_hex(result->response_bitmap, 2) ||
	    !b04_uart_puts(" DEVERRMAP=") ||
	    !b04_uart_put_hex(result->dev_err_bitmap, 2) ||
	    !b04_uart_puts(" DDR3MAP=") ||
	    !b04_uart_put_hex(result->ddr3_bitmap, 2) ||
	    !b04_uart_puts(" BADTYPE=") ||
	    !b04_uart_put_hex(result->unexpected_type_bitmap, 2) ||
	    !b04_uart_puts(" PIN_LAST=") ||
	    !b06_report_optional_byte(result->final_pin_valid,
				      result->final_pin) ||
	    !b04_uart_puts("\r\n[SPD] type-byte discovery only; no dump/IMC/QPI/DDR\r\n"
			    "POST=") ||
	    !b04_uart_put_hex(terminal, 2) || !b04_uart_puts(" HALT\r\n"))
		return false;

	if (terminal == POST_B06_TIMEOUT &&
	    !b04_uart_puts("[SMBUS] timeout not recovered; remove AC power\r\n"))
		return false;

	return b04_uart_wait_for(UART8250_LSR_TEMT,
				 B03_UART_FLUSH_POLL_LIMIT);
}
#else
#define B06B_I801_POLL_LIMIT		1000000U
#define B06B_PIN_POLL_LIMIT		100000U
#define B06B_SPD_ADDRESS		0x54
#define B06B_SPD_SIZE			128
#define B06B_SPD_MEMORY_TYPE_OFFSET	0x02
#define B06B_SPD_DDR3_MEMORY_TYPE	0x0b

#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 8 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 10 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 11
#define B06_SPD_BUFFER_SIZE		SPD_SIZE_MAX_DDR3
#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 8
#define B06C_SPD_USED_SIZE		176
#define POST_B06X_HEADER_OK		POST_B06C_HEADER_OK
#define POST_B06X_UPPER_PASS1_BEGIN	POST_B06C_UPPER_PASS1_BEGIN
#define POST_B06X_PASS1_BLOCK_BASE	POST_B06C_PASS1_BLOCK_BASE
#define POST_B06X_UPPER_PASS2_BEGIN	POST_B06C_UPPER_PASS2_BEGIN
#define POST_B06X_PASS2_BLOCK_BASE	POST_B06C_PASS2_BLOCK_BASE
#define POST_B06X_UPPER_MATCH		POST_B06C_UPPER_MATCH
#define POST_B06X_DECODE_OK		POST_B06C_DECODE_OK
#define POST_B06X_POLICY_OK		POST_B06C_POLICY_OK
#define POST_B06X_HEADER_ERROR		POST_B06C_HEADER_ERROR
#define POST_B06X_UPPER_MISMATCH	POST_B06C_UPPER_MISMATCH
#define POST_B06X_DECODE_ERROR		POST_B06C_DECODE_ERROR
#define POST_B06X_POLICY_ERROR		POST_B06C_POLICY_ERROR
#define B06X_UPPER_CRC_LABEL		"[SPD] UNPROTECTED_UPPER48_CRC16="
#define B06X_USED_CRC_LABEL		" USED176_CRC16="
#else
#define B06C_SPD_USED_SIZE		SPD_SIZE_MAX_DDR3
#define POST_B06X_HEADER_OK		POST_B06I_HEADER_OK
#define POST_B06X_UPPER_PASS1_BEGIN	POST_B06I_UPPER_PASS1_BEGIN
#define POST_B06X_PASS1_BLOCK_BASE	POST_B06I_PASS1_BLOCK_BASE
#define POST_B06X_UPPER_PASS2_BEGIN	POST_B06I_UPPER_PASS2_BEGIN
#define POST_B06X_PASS2_BLOCK_BASE	POST_B06I_PASS2_BLOCK_BASE
#define POST_B06X_UPPER_MATCH		POST_B06I_UPPER_MATCH
#define POST_B06X_DECODE_OK		POST_B06I_DECODE_OK
#define POST_B06X_POLICY_OK		POST_B06I_POLICY_OK
#define POST_B06X_HEADER_ERROR		POST_B06I_HEADER_ERROR
#define POST_B06X_UPPER_MISMATCH	POST_B06I_UPPER_MISMATCH
#define POST_B06X_DECODE_ERROR		POST_B06I_DECODE_ERROR
#define POST_B06X_POLICY_ERROR		POST_B06I_POLICY_ERROR
#define B06X_UPPER_CRC_LABEL		"[SPD] UNPROTECTED_UPPER128_CRC16="
#define B06X_USED_CRC_LABEL		" USED256_CRC16="
#endif
#define B06C_SPD_UPPER_OFFSET		128
#define B06C_SPD_UPPER_SIZE		(B06C_SPD_USED_SIZE - B06C_SPD_UPPER_OFFSET)
#define B06C_TARGET_TCK			TCK_400MHZ
#define B06C_MIN_CAS			4
#define B06C_MAX_CAS			18
#define B06C_MIN_TRRD_CYCLES		4
#define B06C_MIN_TWTR_CYCLES		4
#define B06C_MIN_TRTP_CYCLES		4

struct b06c_policy {
	uint16_t tck;
	uint16_t trcd;
	uint16_t trp;
	uint16_t tras;
	uint16_t trc;
	uint16_t trfc;
	uint16_t twr;
	uint16_t trrd;
	uint16_t twtr;
	uint16_t trtp;
	uint16_t tfaw;
	uint8_t cas;
};
#else
#define B06_SPD_BUFFER_SIZE		B06B_SPD_SIZE
#endif

struct b06b_result {
	uint8_t spd[B06_SPD_BUFFER_SIZE];
	uint32_t pin_polls;
	uint32_t total_polls;
	uint32_t maximum_polls;
	uint32_t last_polls;
	uint16_t crc_calculated;
	uint16_t crc_stored;
	uint8_t initial_status;
	uint8_t initial_control;
	uint8_t initial_pin;
	uint8_t enabled_pin;
	uint8_t pin_before;
	uint8_t pin_after;
	uint8_t last_offset;
	uint8_t last_status;
	uint8_t completed;
	uint8_t crc_coverage;
	bool initial_control_valid;
	bool initial_pin_valid;
	bool enabled_pin_valid;
	bool pin_before_valid;
	bool pin_after_valid;
	bool last_offset_valid;
	bool last_status_valid;
	bool crc_valid;
#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 8 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 10 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 11
	struct dimm_attr_ddr3_st dimm;
	struct b06c_policy policy;
	uint16_t declared_total;
	uint16_t declared_used;
	uint16_t unique_completed;
	uint16_t upper_crc;
	uint16_t used_crc;
	uint8_t verification_completed;
	uint8_t mismatch_offset;
	uint8_t mismatch_first;
	uint8_t mismatch_second;
	uint8_t decode_status;
	bool header_valid;
	bool mismatch_valid;
	bool upper_match;
	bool fingerprint_valid;
	bool decode_attempted;
	bool decode_valid;
	bool policy_valid;
#endif
};

static uint8_t b06b_pin_error(uint8_t pin)
{
	const bool clock_error = !(pin & I801_PIN_CLK_CTL) ||
		!(pin & I801_PIN_CLK_STATUS);
	const bool data_error = !(pin & I801_PIN_DATA_STATUS);

	if (clock_error && data_error)
		return POST_B06_BOTH_PINS_ERROR;
	if (clock_error)
		return POST_B06_CLOCK_ERROR;
	if (data_error)
		return POST_B06_DATA_ERROR;

	return 0;
}

static void b06b_release_host(uint8_t status)
{
	const uintptr_t base = CONFIG_FIXED_SMBUS_IO_BASE;

	/* INUSE releases ownership; completion flags are write-one-to-clear. */
	outb(I801_HSTSTAT_INUSE | (status & I801_HSTSTAT_FLAGS),
	     base + I801_HSTSTAT);
}

static bool b06b_control_idle(uint8_t control)
{
	if (control == 0)
		return true;
#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 11
	/* I801 retains the completed byte-data protocol bits while idle. */
	if (control == I801_BYTE_DATA)
		return true;
#endif
	return false;
}

#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 8 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 10 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 11
static uint8_t b06c_read_byte(struct b06b_result *result, uint8_t offset,
			      uint8_t *value)
{
	const uintptr_t base = CONFIG_FIXED_SMBUS_IO_BASE;
	uint32_t loops;
	uint8_t masked_status;
	uint8_t terminal;

	result->last_offset = offset;
	result->last_offset_valid = true;
	result->last_status_valid = false;
	result->pin_after_valid = false;
	result->last_polls = 0;
	result->pin_before = inb(base + I801_PIN_CTL);
	result->pin_before_valid = true;
	terminal = b06b_pin_error(result->pin_before);
	if (terminal) {
		b06b_release_host(0);
		return terminal;
	}

	outb(I801_BYTE_DATA, base + I801_HSTCTL);
	outb((B06B_SPD_ADDRESS << 1) | 1, base + I801_XMITADD);
	outb(offset, base + I801_HSTCMD);
	outb(0, base + I801_HSTDAT0);
	outb(0, base + I801_HSTDAT1);
	if (inb(base + I801_HSTCTL) != I801_BYTE_DATA ||
	    inb(base + I801_XMITADD) != ((B06B_SPD_ADDRESS << 1) | 1) ||
	    inb(base + I801_HSTCMD) != offset ||
	    inb(base + I801_HSTDAT0) != 0 ||
	    inb(base + I801_HSTDAT1) != 0) {
		b06b_release_host(0);
		return POST_B06_HOST_STATE_ERROR;
	}

	outb(I801_BYTE_DATA | I801_HSTCTL_START, base + I801_HSTCTL);
	for (loops = B06B_I801_POLL_LIMIT; loops; loops--) {
		result->last_status = inb(base + I801_HSTSTAT);
		result->last_status_valid = true;
		if (!(result->last_status & I801_HSTSTAT_HOST_BUSY) &&
		    (result->last_status & I801_HSTSTAT_TERMINAL))
			break;
	}
	result->last_polls = loops ? B06B_I801_POLL_LIMIT - loops + 1 :
		B06B_I801_POLL_LIMIT;
	result->total_polls += result->last_polls;
	if (result->last_polls > result->maximum_polls)
		result->maximum_polls = result->last_polls;
	if (!loops)
		/* Do not touch any I801 register after an unresolved timeout. */
		return POST_B06_TIMEOUT;

	masked_status = result->last_status & I801_HSTSTAT_RESULT;
	if (masked_status == I801_HSTSTAT_INTR)
		*value = inb(base + I801_HSTDAT0);
	result->pin_after = inb(base + I801_PIN_CTL);
	result->pin_after_valid = true;
	/* Clear this command's completion flags but retain semaphore ownership. */
	outb(result->last_status & I801_HSTSTAT_FLAGS,
	     base + I801_HSTSTAT);

	terminal = b06b_pin_error(result->pin_after);
	if (terminal) {
		b06b_release_host(0);
		return terminal;
	}
	if (masked_status != I801_HSTSTAT_INTR) {
		b06b_release_host(0);
		if (masked_status == I801_HSTSTAT_DEV_ERR)
			return POST_B06B_DEV_ERROR;
		if (masked_status == I801_HSTSTAT_BUS_ERR)
			return POST_B06_BUS_ERROR;
		return POST_B06_TRANSACTION_ERROR;
	}

	return 0;
}

static bool b06c_decoder_input_valid(const uint8_t *spd)
{
	/*
	 * Fail closed before the generic decoder.  Its invalid-field reporting
	 * does not stop all subsequent arithmetic, so malformed organization
	 * fields can otherwise reach a division by a truncated zero width.
	 * These are the exact raw structural values of the observed 4-GiB,
	 * two-rank x8, 64-bit non-ECC, 1.5-V UDIMM at address 0x54.
	 */
	return spd[2] == SPD_MEMORY_TYPE_SDRAM_DDR3 &&
		spd[3] == 0x02 && spd[4] == 0x03 && spd[5] == 0x19 &&
		spd[6] == 0x00 && spd[7] == 0x09 && spd[8] == 0x03 &&
		spd[10] == 0x01 && spd[11] == 0x08;
}

static bool b06c_timing_to_cycles(uint32_t timing, uint16_t tck,
				  uint16_t *cycles)
{
	uint32_t rounded;

	if (!timing || !tck)
		return false;
	rounded = timing / tck + !!(timing % tck);
	if (!rounded || rounded > UINT16_MAX)
		return false;
	*cycles = rounded;

	return true;
}

static bool b06c_derive_policy(struct b06b_result *result)
{
	const struct dimm_attr_ddr3_st *dimm = &result->dimm;
	struct b06c_policy *policy = &result->policy;
	unsigned int cas;
	uint16_t taa_cycles;

	/* Keep the first writable experiment constrained to the observed module. */
	if (dimm->dram_type != SPD_MEMORY_TYPE_SDRAM_DDR3 ||
	    dimm->dimm_type != SPD_DDR3_DIMM_TYPE_UDIMM ||
	    dimm->size_mb != 4096 || dimm->ranks != 2 || dimm->width != 8 ||
	    dimm->row_bits != 15 || dimm->col_bits != 10 ||
	    dimm->flags.is_ecc || !dimm->flags.operable_1_50V ||
	    !dimm->flags.pins_mirrored ||
	    ((result->spd[4] >> 4) & 0x7) != 0 ||
	    (result->spd[8] & 0x7) != 3 || ((result->spd[8] >> 3) & 0x3) != 0)
		return false;

	if (!dimm->tCK || !dimm->tAA || !dimm->tRCD || !dimm->tRP ||
	    !dimm->tRAS || !dimm->tRC || !dimm->tRFC || !dimm->tWR ||
	    !dimm->tRRD || !dimm->tWTR || !dimm->tRTP || !dimm->tFAW ||
	    dimm->tCK > B06C_TARGET_TCK)
		return false;

	if (!b06c_timing_to_cycles(dimm->tAA, B06C_TARGET_TCK, &taa_cycles))
		return false;
	cas = MAX(B06C_MIN_CAS, taa_cycles);
	for (; cas <= B06C_MAX_CAS; cas++) {
		if (dimm->cas_supported & BIT(cas - B06C_MIN_CAS))
			break;
	}
	if (cas > B06C_MAX_CAS || cas * B06C_TARGET_TCK >= 20 * 256)
		return false;

	policy->tck = B06C_TARGET_TCK;
	policy->cas = cas;
	if (!b06c_timing_to_cycles(dimm->tRCD, policy->tck, &policy->trcd) ||
	    !b06c_timing_to_cycles(dimm->tRP, policy->tck, &policy->trp) ||
	    !b06c_timing_to_cycles(dimm->tRAS, policy->tck, &policy->tras) ||
	    !b06c_timing_to_cycles(dimm->tRC, policy->tck, &policy->trc) ||
	    !b06c_timing_to_cycles(dimm->tRFC, policy->tck, &policy->trfc) ||
	    !b06c_timing_to_cycles(dimm->tWR, policy->tck, &policy->twr) ||
	    !b06c_timing_to_cycles(dimm->tRRD, policy->tck, &policy->trrd) ||
	    !b06c_timing_to_cycles(dimm->tWTR, policy->tck, &policy->twtr) ||
	    !b06c_timing_to_cycles(dimm->tRTP, policy->tck, &policy->trtp) ||
	    !b06c_timing_to_cycles(dimm->tFAW, policy->tck, &policy->tfaw))
		return false;

	/* DDR3 also specifies cycle-count floors beyond the absolute-time SPD. */
	policy->trrd = MAX(policy->trrd, B06C_MIN_TRRD_CYCLES);
	policy->twtr = MAX(policy->twtr, B06C_MIN_TWTR_CYCLES);
	policy->trtp = MAX(policy->trtp, B06C_MIN_TRTP_CYCLES);

	return true;
}

static uint8_t b06c_read_upper_and_decode(struct b06b_result *result)
{
	unsigned int offset;
	uint8_t terminal;

	switch (result->spd[0] & 0xf) {
	case 1:
		result->declared_used = 128;
		break;
	case 2:
		result->declared_used = 176;
		break;
	case 3:
		result->declared_used = 256;
		break;
	default:
		result->declared_used = 0;
		break;
	}
	switch ((result->spd[0] >> 4) & 0x7) {
	case 1:
		result->declared_total = 256;
		break;
	case 2:
		result->declared_total = 512;
		break;
	default:
		result->declared_total = 0;
		break;
	}
	if (result->declared_used != B06C_SPD_USED_SIZE ||
	    result->declared_total != SPD_SIZE_MAX_DDR3) {
		b06b_release_host(0);
		return POST_B06X_HEADER_ERROR;
	}
	result->header_valid = true;
	outb(POST_B06X_HEADER_OK, CONFIG_POST_IO_PORT);
	outb(POST_B06X_UPPER_PASS1_BEGIN, CONFIG_POST_IO_PORT);

	for (offset = B06C_SPD_UPPER_OFFSET; offset < B06C_SPD_USED_SIZE;
	     offset++) {
		terminal = b06c_read_byte(result, offset, &result->spd[offset]);
		if (terminal)
			return terminal;
		result->unique_completed = offset + 1;
		if ((offset & 0xf) == 0xf)
			outb(POST_B06X_PASS1_BLOCK_BASE +
			     ((offset - B06C_SPD_UPPER_OFFSET) >> 4),
			     CONFIG_POST_IO_PORT);
	}

	outb(POST_B06X_UPPER_PASS2_BEGIN, CONFIG_POST_IO_PORT);
	for (offset = B06C_SPD_UPPER_OFFSET; offset < B06C_SPD_USED_SIZE;
	     offset++) {
		uint8_t verification;

		terminal = b06c_read_byte(result, offset, &verification);
		if (terminal)
			return terminal;
		result->verification_completed =
			offset - B06C_SPD_UPPER_OFFSET + 1;
		if (verification != result->spd[offset]) {
			result->mismatch_offset = offset;
			result->mismatch_first = result->spd[offset];
			result->mismatch_second = verification;
			result->mismatch_valid = true;
			b06b_release_host(0);
			return POST_B06X_UPPER_MISMATCH;
		}
		if ((offset & 0xf) == 0xf)
			outb(POST_B06X_PASS2_BLOCK_BASE +
			     ((offset - B06C_SPD_UPPER_OFFSET) >> 4),
			     CONFIG_POST_IO_PORT);
	}

	/* All SMBus work is complete before any semantic decoding. */
	b06b_release_host(0);
	result->upper_match = true;
	outb(POST_B06X_UPPER_MATCH, CONFIG_POST_IO_PORT);
	result->upper_crc = ddr_crc16(&result->spd[B06C_SPD_UPPER_OFFSET],
				      B06C_SPD_UPPER_SIZE);
	result->used_crc = ddr_crc16(result->spd, B06C_SPD_USED_SIZE);
	result->fingerprint_valid = true;

	/* The decoder has no length argument and is not fail-fast on all fields. */
	if (!b06c_decoder_input_valid(result->spd))
		return POST_B06X_DECODE_ERROR;
	result->decode_attempted = true;
	result->decode_status = spd_decode_ddr3(&result->dimm, result->spd);
	if (result->decode_status != SPD_STATUS_OK)
		return POST_B06X_DECODE_ERROR;
	result->decode_valid = true;
	outb(POST_B06X_DECODE_OK, CONFIG_POST_IO_PORT);

	if (!b06c_derive_policy(result))
		return POST_B06X_POLICY_ERROR;
	result->policy_valid = true;
	outb(POST_B06X_POLICY_OK, CONFIG_POST_IO_PORT);

	return 0;
}
#endif

static uint8_t b06b_read_base_spd(struct b06b_result *result)
{
	const uintptr_t base = CONFIG_FIXED_SMBUS_IO_BASE;
	unsigned int offset;
	uint8_t terminal;

	/* The first HSTSTAT read acquires the I801 software semaphore. */
	result->initial_status = inb(base + I801_HSTSTAT);
	if (result->initial_status & I801_HSTSTAT_INUSE)
		return POST_B06_HOST_STATE_ERROR;
	if (result->initial_status & I801_HSTSTAT_HOST_BUSY) {
		/* This read acquired a previously free semaphore; give it back. */
		b06b_release_host(0);
		return POST_B06_HOST_STATE_ERROR;
	}

	result->initial_control = inb(base + I801_HSTCTL);
	result->initial_control_valid = true;
	if ((result->initial_status & I801_HSTSTAT_FLAGS) ||
	    !b06b_control_idle(result->initial_control)) {
		b06b_release_host(0);
		return POST_B06_HOST_STATE_ERROR;
	}

	outb(POST_B06_PIN_PHASE, CONFIG_POST_IO_PORT);
	result->initial_pin = inb(base + I801_PIN_CTL);
	result->initial_pin_valid = true;
	/* Release SMBCLK; bits 1:0 are read-only levels and reserved bits are zero. */
	outb(I801_PIN_CLK_CTL, base + I801_PIN_CTL);
	outb(POST_B06_CLOCK_RELEASED, CONFIG_POST_IO_PORT);
	do {
		result->enabled_pin = inb(base + I801_PIN_CTL);
		result->enabled_pin_valid = true;
		result->pin_polls++;
		if ((result->enabled_pin &
		     (I801_PIN_CLK_CTL | I801_PIN_DATA_STATUS |
		      I801_PIN_CLK_STATUS)) ==
		    (I801_PIN_CLK_CTL | I801_PIN_DATA_STATUS |
		     I801_PIN_CLK_STATUS))
			break;
	} while (result->pin_polls < B06B_PIN_POLL_LIMIT);
	terminal = b06b_pin_error(result->enabled_pin);
	if (terminal) {
		b06b_release_host(0);
		return terminal;
	}
	outb(POST_B06_PINS_IDLE, CONFIG_POST_IO_PORT);
	outb(POST_B06B_READ_BEGIN, CONFIG_POST_IO_PORT);

	/* Keep one semaphore across exactly 128 one-shot read-byte-data commands. */
	for (offset = 0; offset < B06B_SPD_SIZE; offset++) {
		uint32_t loops;
		uint8_t masked_status;

		result->last_offset = offset;
		result->last_offset_valid = true;
		result->last_status_valid = false;
		result->pin_after_valid = false;
		result->last_polls = 0;
		result->pin_before = inb(base + I801_PIN_CTL);
		result->pin_before_valid = true;
		terminal = b06b_pin_error(result->pin_before);
		if (terminal) {
			b06b_release_host(0);
			return terminal;
		}

		outb(I801_BYTE_DATA, base + I801_HSTCTL);
		outb((B06B_SPD_ADDRESS << 1) | 1, base + I801_XMITADD);
		outb(offset, base + I801_HSTCMD);
		outb(0, base + I801_HSTDAT0);
		outb(0, base + I801_HSTDAT1);
		if (inb(base + I801_HSTCTL) != I801_BYTE_DATA ||
		    inb(base + I801_XMITADD) != ((B06B_SPD_ADDRESS << 1) | 1) ||
		    inb(base + I801_HSTCMD) != offset ||
		    inb(base + I801_HSTDAT0) != 0 ||
		    inb(base + I801_HSTDAT1) != 0) {
			b06b_release_host(0);
			return POST_B06_HOST_STATE_ERROR;
		}

		outb(I801_BYTE_DATA | I801_HSTCTL_START, base + I801_HSTCTL);
		for (loops = B06B_I801_POLL_LIMIT; loops; loops--) {
			result->last_status = inb(base + I801_HSTSTAT);
			result->last_status_valid = true;
			if (!(result->last_status & I801_HSTSTAT_HOST_BUSY) &&
			    (result->last_status & I801_HSTSTAT_TERMINAL))
				break;
		}
		result->last_polls = loops ? B06B_I801_POLL_LIMIT - loops + 1 :
			B06B_I801_POLL_LIMIT;
		result->total_polls += result->last_polls;
		if (result->last_polls > result->maximum_polls)
			result->maximum_polls = result->last_polls;
		if (!loops)
			/* Do not touch any I801 register after an unresolved timeout. */
			return POST_B06_TIMEOUT;

		masked_status = result->last_status & I801_HSTSTAT_RESULT;
		if (masked_status == I801_HSTSTAT_INTR)
			result->spd[offset] = inb(base + I801_HSTDAT0);
		result->pin_after = inb(base + I801_PIN_CTL);
		result->pin_after_valid = true;
		/* Clear this command's completion flags but retain semaphore ownership. */
		outb(result->last_status & I801_HSTSTAT_FLAGS,
		     base + I801_HSTSTAT);

		terminal = b06b_pin_error(result->pin_after);
		if (terminal) {
			b06b_release_host(0);
			return terminal;
		}
		if (masked_status != I801_HSTSTAT_INTR) {
			b06b_release_host(0);
			if (masked_status == I801_HSTSTAT_DEV_ERR)
				return POST_B06B_DEV_ERROR;
			if (masked_status == I801_HSTSTAT_BUS_ERR)
				return POST_B06_BUS_ERROR;
			return POST_B06_TRANSACTION_ERROR;
		}

		result->completed = offset + 1;
		if (offset == B06B_SPD_MEMORY_TYPE_OFFSET) {
			if (result->spd[offset] != B06B_SPD_DDR3_MEMORY_TYPE) {
				b06b_release_host(0);
				return POST_B06B_NOT_DDR3;
			}
			outb(POST_B06B_TYPE_OK, CONFIG_POST_IO_PORT);
		}
		if ((offset & 0xf) == 0xf)
			outb(POST_B06B_BLOCK_DONE_BASE + (offset >> 4),
			     CONFIG_POST_IO_PORT);
	}

	/* The last completed transaction already supplied the final pin sample. */
#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 7 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 9
	b06b_release_host(0);
#else
	result->unique_completed = result->completed;
#endif
	result->crc_coverage = (result->spd[0] & BIT(7)) ? 117 : 126;
	result->crc_calculated = ddr_crc16(result->spd, result->crc_coverage);
	result->crc_stored = result->spd[126] |
		((uint16_t)result->spd[127] << 8);
	result->crc_valid = true;
	if (result->crc_calculated != result->crc_stored) {
#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 8 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 10 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 11
		b06b_release_host(0);
#endif
		return POST_B06B_CRC_ERROR;
	}

	outb(POST_B06B_CRC_OK, CONFIG_POST_IO_PORT);
#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 8 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 10 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 11
	return b06c_read_upper_and_decode(result);
#else
	return 0;
#endif
}

static bool b06b_report_optional_byte(bool valid, uint8_t value)
{
	if (!valid)
		return b04_uart_puts("NA");

	return b04_uart_put_hex(value, 2);
}

#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 7 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 9
static bool b06b_report(const struct b06b_result *result, uint8_t terminal)
{
	unsigned int offset;

	if (!b04_uart_puts("[SMBUS] PRE STS=") ||
	    !b04_uart_put_hex(result->initial_status, 2) ||
	    !b04_uart_puts(" CTL=") ||
	    !b06b_report_optional_byte(result->initial_control_valid,
				       result->initial_control) ||
	    !b04_uart_puts(" PIN_PRE=") ||
	    !b06b_report_optional_byte(result->initial_pin_valid,
				       result->initial_pin) ||
	    !b04_uart_puts(" PIN_ENABLED=") ||
	    !b06b_report_optional_byte(result->enabled_pin_valid,
				       result->enabled_pin) ||
	    !b04_uart_puts(" PIN_POLLS=") ||
	    !b04_uart_put_hex(result->pin_polls, 8) ||
	    !b04_uart_puts("\r\n[SPD] A=54 READ=") ||
	    !b04_uart_put_hex(result->completed, 4) ||
	    !b04_uart_puts("/0080 LAST_OFF=") ||
	    !b06b_report_optional_byte(result->last_offset_valid,
				       result->last_offset) ||
	    !b04_uart_puts(" STS=") ||
	    !b06b_report_optional_byte(result->last_status_valid,
				       result->last_status) ||
	    !b04_uart_puts(" POLLS=") ||
	    !b04_uart_put_hex(result->last_polls, 8) ||
	    !b04_uart_puts(" TOTAL=") ||
	    !b04_uart_put_hex(result->total_polls, 8) ||
	    !b04_uart_puts(" MAX=") ||
	    !b04_uart_put_hex(result->maximum_polls, 8) ||
	    !b04_uart_puts(" PIN0=") ||
	    !b06b_report_optional_byte(result->pin_before_valid,
				       result->pin_before) ||
	    !b04_uart_puts(" PIN1=") ||
	    !b06b_report_optional_byte(result->pin_after_valid,
				       result->pin_after) ||
	    !b04_uart_puts("\r\n"))
		return false;

	if (result->completed >= 3 &&
	    (!b04_uart_puts("[SPD] B0=") ||
	     !b04_uart_put_hex(result->spd[0], 2) ||
	     !b04_uart_puts(" REV=") ||
	     !b04_uart_put_hex(result->spd[1], 2) ||
	     !b04_uart_puts(" TYPE=") ||
	     !b04_uart_put_hex(result->spd[2], 2) ||
	     !b04_uart_puts("\r\n")))
		return false;

	for (offset = 0; offset < result->completed; offset++) {
		if ((offset & 0xf) == 0 &&
		    (!b04_uart_puts("[SPD] ") ||
		     !b04_uart_put_hex(offset, 2) || !b04_uart_putc(':')))
			return false;
		if (!b04_uart_put_hex(result->spd[offset], 2))
			return false;
		if ((offset & 0xf) == 0xf || offset + 1 == result->completed) {
			if (!b04_uart_puts("\r\n"))
				return false;
		}
	}

	if (result->crc_valid &&
	    (!b04_uart_puts("[SPD] CRC COVER_HEX=") ||
	     !b04_uart_put_hex(result->crc_coverage, 2) ||
	     !b04_uart_puts(" CALC=") ||
	     !b04_uart_put_hex(result->crc_calculated, 4) ||
	     !b04_uart_puts(" STORED=") ||
	     !b04_uart_put_hex(result->crc_stored, 4) ||
	     !b04_uart_puts(result->crc_calculated == result->crc_stored ?
			      " OK\r\n" : " MISMATCH\r\n")))
		return false;

	if (!b04_uart_puts("[SPD] read-only base block; no upper SPD/IMC/QPI/DDR\r\n"
			    "POST=") ||
	    !b04_uart_put_hex(terminal, 2) || !b04_uart_puts(" HALT\r\n"))
		return false;
	if (terminal == POST_B06_TIMEOUT &&
	    !b04_uart_puts("[SMBUS] timeout not recovered; remove AC power\r\n"))
		return false;

	return b04_uart_wait_for(UART8250_LSR_TEMT,
				 B03_UART_FLUSH_POLL_LIMIT);
}
#endif

#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 8 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 10 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 11
static bool b06c_report_part_number(const struct b06b_result *result)
{
	unsigned int offset;

	if (!b04_uart_puts("[SPD] PART="))
		return false;
	for (offset = SPD_DDR3_PART_NUM;
	     offset < SPD_DDR3_PART_NUM + SPD_DDR3_PART_LEN; offset++) {
		uint8_t value = result->spd[offset];

		if (value < 0x20 || value > 0x7e)
			value = '.';
		if (!b04_uart_putc(value))
			return false;
	}

	return b04_uart_puts("\r\n");
}

static bool b06c_report(const struct b06b_result *result, uint8_t terminal)
{
	const unsigned int unique = result->unique_completed ?
		result->unique_completed : result->completed;
	unsigned int offset;

	if (!b04_uart_puts("[SMBUS] PRE STS=") ||
	    !b04_uart_put_hex(result->initial_status, 2) ||
	    !b04_uart_puts(" CTL=") ||
	    !b06b_report_optional_byte(result->initial_control_valid,
				       result->initial_control) ||
	    !b04_uart_puts(" PIN_PRE=") ||
	    !b06b_report_optional_byte(result->initial_pin_valid,
				       result->initial_pin) ||
	    !b04_uart_puts(" PIN_ENABLED=") ||
	    !b06b_report_optional_byte(result->enabled_pin_valid,
				       result->enabled_pin) ||
	    !b04_uart_puts(" PIN_POLLS=") ||
	    !b04_uart_put_hex(result->pin_polls, 8) ||
	    !b04_uart_puts("\r\n[SPD] A=54 UNIQUE=") ||
	    !b04_uart_put_hex(unique, 4) ||
	    !b04_uart_puts(" VERIFY=") ||
	    !b04_uart_put_hex(result->verification_completed, 4) ||
	    !b04_uart_puts(" LAST_OFF=") ||
	    !b06b_report_optional_byte(result->last_offset_valid,
				       result->last_offset) ||
	    !b04_uart_puts(" STS=") ||
	    !b06b_report_optional_byte(result->last_status_valid,
				       result->last_status) ||
	    !b04_uart_puts(" POLLS=") ||
	    !b04_uart_put_hex(result->last_polls, 8) ||
	    !b04_uart_puts(" TOTAL=") ||
	    !b04_uart_put_hex(result->total_polls, 8) ||
	    !b04_uart_puts(" MAX=") ||
	    !b04_uart_put_hex(result->maximum_polls, 8) ||
	    !b04_uart_puts(" PIN0=") ||
	    !b06b_report_optional_byte(result->pin_before_valid,
				       result->pin_before) ||
	    !b04_uart_puts(" PIN1=") ||
	    !b06b_report_optional_byte(result->pin_after_valid,
				       result->pin_after) ||
	    !b04_uart_puts("\r\n"))
		return false;

	if (result->completed >= 3 &&
	    (!b04_uart_puts("[SPD] B0=") ||
	     !b04_uart_put_hex(result->spd[0], 2) ||
	     !b04_uart_puts(" REV=") ||
	     !b04_uart_put_hex(result->spd[1], 2) ||
	     !b04_uart_puts(" TYPE=") ||
	     !b04_uart_put_hex(result->spd[2], 2) ||
	     !b04_uart_puts(" USED=") ||
	     !b04_uart_put_hex(result->declared_used, 4) ||
	     !b04_uart_puts(" TOTAL=") ||
	     !b04_uart_put_hex(result->declared_total, 4) ||
	     !b04_uart_puts(result->header_valid ? " HEADER_OK\r\n" :
						   " HEADER_BAD\r\n")))
		return false;

	for (offset = 0; offset < unique; offset++) {
		if ((offset & 0xf) == 0 &&
		    (!b04_uart_puts("[SPD] ") ||
		     !b04_uart_put_hex(offset, 2) || !b04_uart_putc(':')))
			return false;
		if (!b04_uart_put_hex(result->spd[offset], 2))
			return false;
		if ((offset & 0xf) == 0xf || offset + 1 == unique) {
			if (!b04_uart_puts("\r\n"))
				return false;
		}
	}

	if (unique >= SPD_DDR3_PART_NUM + SPD_DDR3_PART_LEN &&
	    !b06c_report_part_number(result))
		return false;
	if (result->crc_valid &&
	    (!b04_uart_puts("[SPD] BASE_CRC COVER_HEX=") ||
	     !b04_uart_put_hex(result->crc_coverage, 2) ||
	     !b04_uart_puts(" CALC=") ||
	     !b04_uart_put_hex(result->crc_calculated, 4) ||
	     !b04_uart_puts(" STORED=") ||
	     !b04_uart_put_hex(result->crc_stored, 4) ||
	     !b04_uart_puts(result->crc_calculated == result->crc_stored ?
			      " OK\r\n" : " MISMATCH\r\n")))
		return false;
	if (result->fingerprint_valid &&
	    (!b04_uart_puts(B06X_UPPER_CRC_LABEL) ||
	     !b04_uart_put_hex(result->upper_crc, 4) ||
	     !b04_uart_puts(B06X_USED_CRC_LABEL) ||
	     !b04_uart_put_hex(result->used_crc, 4) ||
	     !b04_uart_puts(" DOUBLE_READ_MATCH\r\n")))
		return false;
	if (result->mismatch_valid &&
	    (!b04_uart_puts("[SPD] VERIFY_MISMATCH OFF=") ||
	     !b04_uart_put_hex(result->mismatch_offset, 2) ||
	     !b04_uart_puts(" FIRST=") ||
	     !b04_uart_put_hex(result->mismatch_first, 2) ||
	     !b04_uart_puts(" SECOND=") ||
	     !b04_uart_put_hex(result->mismatch_second, 2) ||
	     !b04_uart_puts("\r\n")))
		return false;

	if (result->decode_attempted &&
	    (!b04_uart_puts("[SPD] DECODE_STATUS=") ||
	     !b04_uart_put_hex(result->decode_status, 2) ||
	     !b04_uart_puts("\r\n")))
		return false;
	if (result->decode_valid &&
	    (!b04_uart_puts("[SPD] SIZE_MIB=") ||
	     !b04_uart_put_hex(result->dimm.size_mb, 8) ||
	     !b04_uart_puts(" RANKS=") ||
	     !b04_uart_put_hex(result->dimm.ranks, 2) ||
	     !b04_uart_puts(" WIDTH=") ||
	     !b04_uart_put_hex(result->dimm.width, 2) ||
	     !b04_uart_puts(" ROW=") ||
	     !b04_uart_put_hex(result->dimm.row_bits, 2) ||
	     !b04_uart_puts(" COL=") ||
	     !b04_uart_put_hex(result->dimm.col_bits, 2) ||
	     !b04_uart_puts(" FLAGS=") ||
	     !b04_uart_put_hex(result->dimm.flags.raw, 8) ||
	     !b04_uart_puts(" CASMAP=") ||
	     !b04_uart_put_hex(result->dimm.cas_supported, 4) ||
	     !b04_uart_puts(" TCK=") ||
	     !b04_uart_put_hex(result->dimm.tCK, 8) ||
	     !b04_uart_puts(" TAA=") ||
	     !b04_uart_put_hex(result->dimm.tAA, 8) ||
	     !b04_uart_puts("\r\n")))
		return false;

	if (result->policy_valid &&
	    (!b04_uart_puts("[SPD] DDR3-800 JEDEC_CYCLE_CANDIDATE TCK=") ||
	     !b04_uart_put_hex(result->policy.tck, 4) ||
	     !b04_uart_puts(" CL=") ||
	     !b04_uart_put_hex(result->policy.cas, 2) ||
	     !b04_uart_puts(" RCD=") ||
	     !b04_uart_put_hex(result->policy.trcd, 2) ||
	     !b04_uart_puts(" RP=") ||
	     !b04_uart_put_hex(result->policy.trp, 2) ||
	     !b04_uart_puts(" RAS=") ||
	     !b04_uart_put_hex(result->policy.tras, 2) ||
	     !b04_uart_puts(" RC=") ||
	     !b04_uart_put_hex(result->policy.trc, 2) ||
	     !b04_uart_puts(" RFC=") ||
	     !b04_uart_put_hex(result->policy.trfc, 2) ||
	     !b04_uart_puts(" WR=") ||
	     !b04_uart_put_hex(result->policy.twr, 2) ||
	     !b04_uart_puts(" RRD=") ||
	     !b04_uart_put_hex(result->policy.trrd, 2) ||
	     !b04_uart_puts(" WTR=") ||
	     !b04_uart_put_hex(result->policy.twtr, 2) ||
	     !b04_uart_puts(" RTP=") ||
	     !b04_uart_put_hex(result->policy.trtp, 2) ||
	     !b04_uart_puts(" FAW=") ||
	     !b04_uart_put_hex(result->policy.tfaw, 2) ||
	     !b04_uart_puts("\r\n[SPD] candidate is not an X58 register encoding\r\n")))
		return false;

#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 8
	if (!b04_uart_puts("[SPD] bytes b0..ff/XMP unread; no IMC/QPI/DDR writes\r\n"
			    "POST=") ||
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 10
	if (!b04_uart_puts("[SPD] full declared 256 bytes read twice; no IMC/QPI/DDR writes\r\n"
			    "POST=") ||
#else
	if (!b04_uart_puts("[SPD] full declared 256 bytes read twice; returning to ROMMON\r\n"
			    "RESULT=") ||
#endif
	    !b04_uart_put_hex(terminal, 2) ||
#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 11
	    !b04_uart_puts("\r\n"))
#else
	    !b04_uart_puts(" HALT\r\n"))
#endif
		return false;
	if (terminal == POST_B06_TIMEOUT &&
	    !b04_uart_puts("[SMBUS] timeout not recovered; remove AC power\r\n"))
		return false;

	return b04_uart_wait_for(UART8250_LSR_TEMT,
				 B03_UART_FLUSH_POLL_LIMIT);
}
#endif

#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 11
#if CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
#define B06J_LINE_SIZE		128
#define B06J_MAX_ARGS		10
#else
#define B06J_LINE_SIZE		96
#define B06J_MAX_ARGS		8
#endif
#define B06J_RX_POLL_LIMIT	1000000U
#define B06J_DUMP_MAX		64

#define B06V_RST_CNT_PORT	0xcf9

#if CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
#define B06V0_SAD_DEV		PCI_DEV(0xff, 0, 1)
#define B06V0_SAD_ID		0x2d818086U
#define B06V0_SAD_PCIEXBAR_LO	0x50
#define B06V0_SAD_PCIEXBAR_HI	0x54
#define B06V0_PCIEXBAR_VALUE	0xe0000001U
#define B06V0_IOH_MMCONFIG_ID	0x34058086U
#define B06V0_TARGET_SPD_HEADER	0x93
#define B06V0_TARGET_SPD_CRC	0xec4f
#define B06V0_VENDOR_DUMP_MAX	X58_VENDOR_MINIT_POLICY_SIZE
#define B06V0_SPD_FIRST_ADDRESS	0x50
#define B06V0_SPD_ADDRESS_COUNT	8
#define B06V0_TARGET_SPD_BITMAP	BIT(B06B_SPD_ADDRESS - B06V0_SPD_FIRST_ADDRESS)

static const uint8_t b06v0_target_part[SPD_DDR3_PART_LEN] = {
	'B', 'L', 'S', '4', 'G', '3', 'D', '1', '6', '0', '9', 'D', 'S',
	'1', 'S', '0', '0', '.',
};
#endif

#define B06K_UNCORE_COMMON_DEV	PCI_DEV(0xff, 3, 4)
#define B06K_CHANNEL2_DEV	PCI_DEV(0xff, 6, 0)
#define B06K_PHY_SELECT_REG	0x5c
#define B06K_PHY_COMMAND_REG	0xf8
#define B06K_PHY_DATA_REG	0xfc
#define B06K_PHY_CHANNEL_SHIFT	25
#define B06K_PHY_CHANNEL_MASK	(3U << B06K_PHY_CHANNEL_SHIFT)
#define B06K_PHY_WRITE_BUSY	BIT(30)
#define B06K_PHY_READ_BUSY	BIT(31)
#define B06K_PHY_ANY_BUSY	(B06K_PHY_WRITE_BUSY | B06K_PHY_READ_BUSY)
#define B06K_PHY_CHAIN_BASE	0x162a
#define B06K_PHY_LAST_FIELD_BIT	0x1603
#define B06K_PHY_POLL_LIMIT	100000U
#define B06K_TRAIN_POLL_LIMIT	100000U

#define B06K_MC_INIT_CMD		0x54
#define B06K_MC_INIT_STATUS	0x5c
#define B06K_MC_DDR3_CMD		0x60
#define B06K_MC_MRS_VALUE_0_1	0x70
#define B06K_MC_MRS_VALUE_2	0x74
#define B06K_MC_BASE_TIMING	0x78
#define B06K_MC_RANK_PRESENT	0x7c
#define B06K_MC_ODT_PARAMS2	0xa0
#define B06K_MC_TRAIN_ACTION	BIT(0)
#define B06K_MC_INIT_COMPLETE	BIT(8)

#define B06L_QPI_LINK0_PHY_DEV	PCI_DEV(0xff, 2, 1)
#define B06L_QPI_PH_PIS		0x80
#define B06L_QPI_SLOW_PH_PIS	0x030f0f03
#define B06L_MRS_VALID		BIT(23)
#define B06L_DDR3_RANK_SHIFT	20
#define B06L_DDR3_MRS_BA_SHIFT	16
#define B06L_INIT_RANK_SHIFT	5
#define B06L_ASSERT_CKE		BIT(17)
#define B06L_DO_ZQCL		BIT(15)
#define B06L_IGNORE_RX		BIT(9)
#define B06L_STOP_ON_FAIL	BIT(8)
#define B06L_ZQCL_COMPLETE	BIT(7)
#define B06L_RANK_PRESENT	0x03
#define B06L_MR0		0x1528
#define B06L_MR1		0x0806
#define B06L_MR2		0x0000
#define B06L_RD_INIT_PARAMS	0x063f4031
#define B06L_BASE_TIMING		0x00000643

#if CONFIG_X58_PRO_E_B06M_BASEINIT
#define B06M_MC_COMMON_DEV	PCI_DEV(0xff, 3, 0)
#define B06M_CHANNEL2_ADDR_DEV	PCI_DEV(0xff, 6, 1)
#define B06M_MC_CONTROL		0x48
#define B06M_MC_RESET_CONTROL	0x5c
#define B06M_MC_CHANNEL_MAPPER	0x60
#define B06M_MC_MAX_DOD		0x64
#define B06M_MC_DOD_DIMM0	0x48
#define B06M_MC_DIMM_RESET_CMD	0x50
#define B06M_RCOMP_COMPLETE	BIT(9)
#define B06M_MC_INIT_DONE	BIT(7)
#define B06M_CPUID_E5645	0x000206c2
#define B06M_DDR_RATIO_STATUS	0x0a000006
#define B06M_DDR_RATIO		0x00000006
#define B06M_DO_RCOMP		BIT(16)

struct b06m_pci_value {
	pci_devfn_t dev;
	uint16_t reg;
	uint32_t value;
};
#endif

struct b06k_phy_seed {
	uint16_t start;
	uint8_t width;
	uint16_t value;
	uint8_t mode;
};

/* MSI MINITDLL V8.14B8 descriptor tables, channel 2, rank 0, non-ECC. */
static const struct b06k_phy_seed b06k_rd_pulse[] = {
	{ 0x15fd, 7, 2, 1 }, { 0x13ae, 7, 2, 1 },
	{ 0x115f, 7, 2, 1 }, { 0x0f10, 7, 2, 1 },
	{ 0x0936, 7, 2, 1 }, { 0x06e7, 7, 2, 1 },
	{ 0x0498, 7, 2, 1 }, { 0x0249, 7, 2, 1 },
	{ 0x0cb7, 7, 2, 1 },
};

static const struct b06k_phy_seed b06k_rd_lane_seed[] = {
	{ 0x13f9, 12, 0, 0 }, { 0x1405, 11, 0, 0 },
	{ 0x11aa, 12, 0, 0 }, { 0x11b6, 11, 0, 0 },
	{ 0x0f5b, 12, 0, 0 }, { 0x0f67, 11, 0, 0 },
	{ 0x0d0c, 12, 0, 0 }, { 0x0d18, 11, 0, 0 },
	{ 0x0732, 12, 0, 0 }, { 0x073e, 11, 0, 0 },
	{ 0x04e3, 12, 0, 0 }, { 0x04ef, 11, 0, 0 },
	{ 0x0294, 12, 0, 0 }, { 0x02a0, 11, 0, 0 },
	{ 0x0045, 12, 0, 0 }, { 0x0051, 11, 0, 0 },
};

static const struct b06k_phy_seed b06k_rcven_lane_seed[] = {
	{ 0x13bd, 9, 0, 0 }, { 0x13c6, 11, 0x15e, 0 },
	{ 0x116e, 9, 0, 0 }, { 0x1177, 11, 0x15e, 0 },
	{ 0x0f1f, 9, 0, 0 }, { 0x0f28, 11, 0x15e, 0 },
	{ 0x0cd0, 9, 0, 0 }, { 0x0cd9, 11, 0x15e, 0 },
	{ 0x06f6, 9, 0, 0 }, { 0x06ff, 11, 0x15e, 0 },
	{ 0x04a7, 9, 0, 0 }, { 0x04b0, 11, 0x15e, 0 },
	{ 0x0258, 9, 0, 0 }, { 0x0261, 11, 0x15e, 0 },
	{ 0x0009, 9, 0, 0 }, { 0x0012, 11, 0x15e, 0 },
};

#if CONFIG_X58_PRO_E_B06M_BASEINIT
/* B06J-HW-01: live-proven one-channel policy from MSI ratio-6 captures. */
static const struct b06m_pci_value b06m_base_policy[] = {
	{ B06M_MC_COMMON_DEV, B06M_MC_CHANNEL_MAPPER, 0x00024489 },
	{ B06M_MC_COMMON_DEV, B06M_MC_MAX_DOD, 0x000000d4 },
	{ B06M_MC_COMMON_DEV, B06M_MC_CONTROL, 0x00000400 },
	{ B06M_CHANNEL2_ADDR_DEV, B06M_MC_DOD_DIMM0, 0x000002ac },
	{ B06K_CHANNEL2_DEV, B06K_MC_RANK_PRESENT, 0x00000003 },
	{ B06K_CHANNEL2_DEV, 0x58, 0x063f7431 },
	{ B06K_CHANNEL2_DEV, 0x68, 0x00000001 },
	{ B06K_CHANNEL2_DEV, B06K_MC_MRS_VALUE_0_1, 0x08061528 },
	{ B06K_CHANNEL2_DEV, B06K_MC_MRS_VALUE_2, 0x00000000 },
	{ B06K_CHANNEL2_DEV, B06K_MC_BASE_TIMING, B06L_BASE_TIMING },
	{ B06K_CHANNEL2_DEV, 0x80, 0x169bbbd8 },
	{ B06K_CHANNEL2_DEV, 0x84, 0x0000b510 },
	{ B06K_CHANNEL2_DEV, 0x88, 0x001eacf6 },
	{ B06K_CHANNEL2_DEV, 0x8c, 0x0202fc40 },
	{ B06K_CHANNEL2_DEV, 0x90, 0x0010023b },
	{ B06K_CHANNEL2_DEV, 0x94, 0x60401a90 },
	{ B06K_CHANNEL2_DEV, 0x98, 0x00013df0 },
	{ B06K_CHANNEL2_DEV, 0x9c, 0x03e16150 },
	{ B06K_CHANNEL2_DEV, B06K_MC_ODT_PARAMS2, 0x00000000 },
	{ B06K_CHANNEL2_DEV, 0xa4, 0x01010000 },
	{ B06K_CHANNEL2_DEV, 0xa8, 0x01010404 },
	{ B06K_CHANNEL2_DEV, 0xac, 0x09050201 },
	{ B06K_CHANNEL2_DEV, 0xb0, 0x09050605 },
	{ B06K_CHANNEL2_DEV, 0xb4, 0x0daef718 },
	{ B06K_CHANNEL2_DEV, 0xb8, 0x000021e0 },
	{ B06K_CHANNEL2_DEV, 0xbc, 0x00000c30 },
	{ B06K_CHANNEL2_DEV, 0xc0, 0x00050310 },
	{ B06K_CHANNEL2_DEV, 0xc4, 0x00020104 },
	{ B06K_CHANNEL2_DEV, 0xc8, 0x00050610 },
	{ B06K_CHANNEL2_DEV, 0xcc, 0x00000600 },
	{ B06K_CHANNEL2_DEV, 0xd0, 0x00000f05 },
	{ B06K_CHANNEL2_DEV, 0xd4, 0x00000038 },
	{ B06K_CHANNEL2_DEV, 0xd8, 0x00002003 },
	{ B06K_CHANNEL2_DEV, 0xe0, 0x00000310 },
	{ B06K_CHANNEL2_DEV, 0xe4, 0x00000103 },
	{ B06K_CHANNEL2_DEV, 0xe8, 0x00000000 },
};

static const uint16_t b06m_pattern_select[] = {
	0x0000, 0x1000, 0x0200, 0x1200, 0x0400, 0x1400,
};

static const uint32_t b06m_pattern_value[] = {
	0x89abcdef, 0x01234567, 0x5cccc993,
	0xaaaa294c, 0x0005a5a5, 0x000c6c6c,
};
#endif

struct b06j_state {
	char line[B06J_LINE_SIZE];
	unsigned int length;
	bool write_armed;
	bool reset_armed;
	bool smbus_timed_out;
#if CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
	bool script_armed;
#endif
#if CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
	uint8_t vendor_policy[X58_VENDOR_MINIT_POLICY_SIZE];
	bool vendor_armed;
	bool vendor_runtime_ready;
	enum x58_vendor_status vendor_prepare_status;
	bool vendor_spd_valid;
	uint32_t vendor_spd_digest;
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
	uint32_t vendor_spd_profile_digest;
#endif
	uint8_t vendor_spd_ackmap;
	uint8_t vendor_spd_ddr3map;
	uint8_t vendor_spd_topology_status;
	bool vendor_pciexbar_ready;
	bool vendor_policy_ready;
	bool vendor_policy_modified;
	bool vendor_external_write;
#endif
};

static void b06j_state_initialize(struct b06j_state *state)
{
	(void)state;
#if CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
	x58_rs_initialize();
#endif
#if CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
	state->vendor_prepare_status = x58_vendor_runtime_prepare();
	state->vendor_runtime_ready =
		state->vendor_prepare_status == X58_VENDOR_OK;
#endif
}

enum b06j_rx_result {
	B06J_RX_IDLE,
	B06J_RX_BYTE,
	B06J_RX_FAULT,
};

static enum b06j_rx_result b06j_getc(uint8_t *value)
{
	unsigned int polls;

	for (polls = 0; polls < B06J_RX_POLL_LIMIT; polls++) {
		const uint8_t status = inb(B03_UART_BASE + UART8250_LSR);

		if (status & (UART8250_LSR_OE | UART8250_LSR_PE |
			      UART8250_LSR_FE | UART8250_LSR_BI)) {
			if (status & UART8250_LSR_DR)
				(void)inb(B03_UART_BASE + UART8250_RBR);
			return B06J_RX_FAULT;
		}
		if (status & UART8250_LSR_DR) {
			*value = inb(B03_UART_BASE + UART8250_RBR);
			return B06J_RX_BYTE;
		}
	}

	return B06J_RX_IDLE;
}

static bool b06j_streq(const char *left, const char *right)
{
	while (*left && *left == *right) {
		left++;
		right++;
	}

	return *left == *right;
}

static bool b06j_parse_hex(const char *text, uint32_t *value)
{
	uint32_t parsed = 0;
	bool any = false;

	if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
		text += 2;
	while (*text) {
		uint8_t digit;

		if (*text >= '0' && *text <= '9')
			digit = *text - '0';
		else if (*text >= 'a' && *text <= 'f')
			digit = *text - 'a' + 10;
		else if (*text >= 'A' && *text <= 'F')
			digit = *text - 'A' + 10;
		else
			return false;
		if (parsed > (UINT32_MAX >> 4))
			return false;
		parsed = (parsed << 4) | digit;
		any = true;
		text++;
	}
	if (!any)
		return false;
	*value = parsed;

	return true;
}

static unsigned int b06j_split(char *line, char **argv)
{
	unsigned int argc = 0;

	while (*line) {
		while (*line == ' ' || *line == '\t')
			*line++ = '\0';
		if (!*line)
			break;
		if (argc == B06J_MAX_ARGS)
			return B06J_MAX_ARGS + 1;
		argv[argc++] = line;
		while (*line && *line != ' ' && *line != '\t')
			line++;
	}

	return argc;
}

static bool b06j_width(const char *text, unsigned int *width)
{
	if (!text || b06j_streq(text, "l"))
		*width = 4;
	else if (b06j_streq(text, "w"))
		*width = 2;
	else if (b06j_streq(text, "b"))
		*width = 1;
	else
		return false;

	return true;
}

static bool b06j_error(const char *message)
{
	outb(POST_B06J_COMMAND_ERROR, CONFIG_POST_IO_PORT);
	return b04_uart_puts("ERR ") && b04_uart_puts(message) &&
		b04_uart_puts("\r\n");
}

#if CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
static void b06v0_mark_vendor_state_dirty(struct b06j_state *state)
{
	state->vendor_external_write = true;
	state->vendor_spd_valid = false;
	state->vendor_spd_digest = 0;
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
	state->vendor_spd_profile_digest = 0;
#endif
	state->vendor_pciexbar_ready = false;
	state->vendor_policy_ready = false;
	state->vendor_policy_modified = false;
}
#endif

static bool b06j_require_write(struct b06j_state *state)
{
	if (!state->write_armed)
		return false;
	state->write_armed = false;
#if CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
	/* A generic hardware write invalidates every vendor-call precondition. */
	b06v0_mark_vendor_state_dirty(state);
#endif

	return true;
}

static bool b06j_require_reset(struct b06j_state *state)
{
	if (!state->reset_armed)
		return false;
	state->reset_armed = false;

	return true;
}

#if CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
static bool b06j_require_script(struct b06j_state *state)
{
	if (!state->script_armed)
		return false;
	state->script_armed = false;
	return true;
}
#endif

#if CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
static bool b06j_require_vendor(struct b06j_state *state)
{
	if (!state->vendor_armed)
		return false;
	state->vendor_armed = false;

	return true;
}

static bool b06v0_vendor_state_clean(const struct b06j_state *state)
{
	return !state->vendor_external_write;
}

static bool b06v0_vendor_path_ready(const struct b06j_state *state)
{
	return state->vendor_spd_valid && state->vendor_pciexbar_ready;
}
#endif

static bool b06j_print_u32(const char *label, uint32_t value,
			   unsigned int digits)
{
	return b04_uart_puts(label) && b04_uart_put_hex(value, digits) &&
		b04_uart_puts("\r\n");
}

#if CONFIG_X58_PRO_E_ROMMON_IRQPROBE_PIT
static bool b06j_run_irqprobe_pit(void)
{
	struct x58_irqprobe_report r;
	const enum x58_irqprobe_result result = x58_rommon_irqprobe_pit(&r);

	return b04_uart_puts("[IRQPROBE] RESULT=") &&
		b04_uart_puts(x58_irqprobe_result_name(result)) &&
		b04_uart_puts(" LPC=") && b04_uart_put_hex(r.lpc_id, 8) &&
		b04_uart_puts(" APICBASE=") && b04_uart_put_hex(r.apic_base_lo, 8) &&
		b04_uart_puts(" EFLAGS=") && b04_uart_put_hex(r.eflags, 8) &&
		b04_uart_puts("\r\n[IRQPROBE] PRE IMR=") &&
		b04_uart_put_hex(r.master_mask_before, 2) &&
		b04_uart_putc('/') && b04_uart_put_hex(r.slave_mask_before, 2) &&
		b04_uart_puts(" ELCR=") && b04_uart_put_hex(r.elcr1_before, 2) &&
		b04_uart_putc('/') && b04_uart_put_hex(r.elcr2_before, 2) &&
		b04_uart_puts(" IRR=") && b04_uart_put_hex(r.pic_irr_before, 2) &&
		b04_uart_puts(" TPR=") && b04_uart_put_hex(r.tpr_before, 8) &&
		b04_uart_puts(" SVR=") && b04_uart_put_hex(r.svr_before, 8) &&
		b04_uart_puts(" LVT0=") && b04_uart_put_hex(r.lvt0_before, 8) &&
		b04_uart_puts(" LVT1=") && b04_uart_put_hex(r.lvt1_before, 8) &&
		b04_uart_puts("\r\n[IRQPROBE] OBS PIT_IRR=") &&
		b04_uart_put_hex(r.pic_irr_armed, 2) &&
		b04_uart_puts(" PIT_POLLS=") && b04_uart_put_hex(r.pit_polls, 8) &&
		b04_uart_puts(" HITS=") && b04_uart_put_hex(r.hits, 8) &&
		b04_uart_puts(" PIC_ISR=") && b04_uart_put_hex(r.pic_isr_at_entry, 2) &&
		b04_uart_puts(" IRQ_POLLS=") && b04_uart_put_hex(r.irq_polls, 8) &&
		b04_uart_puts("\r\n[IRQPROBE] POST IMR=") &&
		b04_uart_put_hex(r.master_mask_after, 2) &&
		b04_uart_putc('/') && b04_uart_put_hex(r.slave_mask_after, 2) &&
		b04_uart_puts(" SVR=") && b04_uart_put_hex(r.svr_after, 8) &&
		b04_uart_puts(" LVT0=") && b04_uart_put_hex(r.lvt0_after, 8) &&
		b04_uart_puts(" CLEANUP=") && b04_uart_puts(r.cleanup_ok ? "OK" : "FAIL") &&
		b04_uart_puts(" DIRTY=1 COLD_RESET_REQUIRED=1\r\n");
}
#endif

static uint32_t b06j_io_read(uint16_t port, unsigned int width)
{
	if (width == 1)
		return inb(port);
	if (width == 2)
		return inw(port);
	return inl(port);
}

static void b06j_io_write(uint16_t port, uint32_t value, unsigned int width)
{
	if (width == 1)
		outb(value, port);
	else if (width == 2)
		outw(value, port);
	else
		outl(value, port);
}

static uint32_t b06j_pci_read(pci_devfn_t dev, uint16_t reg,
			      unsigned int width)
{
	if (width == 1)
		return pci_io_read_config8(dev, reg);
	if (width == 2)
		return pci_io_read_config16(dev, reg);
	return pci_io_read_config32(dev, reg);
}

static void b06j_pci_write(pci_devfn_t dev, uint16_t reg, uint32_t value,
			   unsigned int width)
{
	if (width == 1)
		pci_io_write_config8(dev, reg, value);
	else if (width == 2)
		pci_io_write_config16(dev, reg, value);
	else
		pci_io_write_config32(dev, reg, value);
}

static bool b06k_phy_wait(uint32_t busy_mask)
{
	unsigned int polls;

	for (polls = 0; polls < B06K_PHY_POLL_LIMIT; polls++) {
		if (!(b06j_pci_read(B06K_UNCORE_COMMON_DEV,
				     B06K_PHY_COMMAND_REG, 4) & busy_mask))
			return true;
	}

	return false;
}

static bool b06k_phy_select(unsigned int channel)
{
	uint32_t value;

	if (channel > 2 || !b06k_phy_wait(B06K_PHY_ANY_BUSY))
		return false;
	value = b06j_pci_read(B06K_UNCORE_COMMON_DEV,
			       B06K_PHY_SELECT_REG, 4);
	value &= ~B06K_PHY_CHANNEL_MASK;
	value |= channel << B06K_PHY_CHANNEL_SHIFT;
	b06j_pci_write(B06K_UNCORE_COMMON_DEV, B06K_PHY_SELECT_REG, value, 4);

	return true;
}

static bool b06k_phy_field_valid(uint16_t start, unsigned int width)
{
	return width >= 2 && width <= 30 &&
		start <= B06K_PHY_LAST_FIELD_BIT &&
		width - 1 <= B06K_PHY_LAST_FIELD_BIT - start;
}

static uint32_t b06k_phy_value_mask(unsigned int width)
{
	return (1U << (width - 2)) - 1;
}

static bool b06k_phy_write(unsigned int channel, uint16_t start,
			   unsigned int width, uint32_t value,
			   unsigned int mode)
{
	uint32_t payload;
	uint16_t end;

	if (!b06k_phy_field_valid(start, width) || mode > 1 ||
	    !b06k_phy_select(channel))
		return false;
	end = start + width - 1;
	payload = value & b06k_phy_value_mask(width);
	payload |= (mode + 2) << (width - 2);
	b06j_pci_write(B06K_UNCORE_COMMON_DEV, B06K_PHY_DATA_REG, payload, 4);
	b06j_pci_write(B06K_UNCORE_COMMON_DEV, B06K_PHY_COMMAND_REG,
			  B06K_PHY_WRITE_BUSY | end, 4);

	return b06k_phy_wait(B06K_PHY_WRITE_BUSY);
}

static bool b06k_phy_read(unsigned int channel, uint16_t start,
			  unsigned int width, uint32_t *raw,
			  uint32_t *value)
{
	uint16_t end;

	if (!b06k_phy_field_valid(start, width) ||
	    !b06k_phy_select(channel))
		return false;
	end = start + width - 1;
	b06j_pci_write(B06K_UNCORE_COMMON_DEV, B06K_PHY_COMMAND_REG,
			  B06K_PHY_READ_BUSY | (B06K_PHY_CHAIN_BASE - end), 4);
	if (!b06k_phy_wait(B06K_PHY_READ_BUSY))
		return false;
	*raw = b06j_pci_read(B06K_UNCORE_COMMON_DEV, B06K_PHY_DATA_REG, 4);
	*value = *raw & b06k_phy_value_mask(width);

	return true;
}

static bool b06k_phy_apply(const struct b06k_phy_seed *seed,
			   unsigned int count)
{
	unsigned int index;

	for (index = 0; index < count; index++) {
		if (!b06k_phy_write(2, seed[index].start, seed[index].width,
				    seed[index].value, seed[index].mode))
			return false;
	}

	return true;
}

static bool b06k_train(uint32_t command, uint32_t *status)
{
	unsigned int polls;

	b06j_pci_write(B06K_CHANNEL2_DEV, B06K_MC_INIT_CMD, 0x20600, 4);
	b06j_pci_write(B06K_CHANNEL2_DEV, B06K_MC_INIT_CMD, command, 4);
	for (polls = 0; polls < B06K_TRAIN_POLL_LIMIT; polls++) {
		const uint32_t action = b06j_pci_read(B06K_CHANNEL2_DEV,
						      B06K_MC_INIT_CMD, 4);

		*status = b06j_pci_read(B06K_CHANNEL2_DEV,
					 B06K_MC_INIT_STATUS, 4);
		if (!(action & B06K_MC_TRAIN_ACTION) &&
		    (*status & B06K_MC_INIT_COMPLETE))
			return true;
	}

	return false;
}

static bool b06l_issue_mrs(unsigned int rank, unsigned int bank,
			   uint16_t value)
{
	uint32_t command;

	command = B06L_MRS_VALID | (rank << B06L_DDR3_RANK_SHIFT) |
		(bank << B06L_DDR3_MRS_BA_SHIFT) | value;
	b06j_pci_write(B06K_CHANNEL2_DEV, B06K_MC_DDR3_CMD, command, 4);
	/*
	 * MSI fffcc43a writes and delays.  Bit 23 remains set on both the live
	 * target and a successful vendor-initialized capture; it encodes the
	 * direct command and is not a completion bit to poll clear.
	 */
#if CONFIG_X58_PRO_E_B06N_TIMED_MRS
	/* One 115200-baud frame is about 86.8 us; TEMT proves it completed. */
	if (!b04_uart_putc('.') ||
	    !b04_uart_wait_for(UART8250_LSR_TEMT, B03_UART_FLUSH_POLL_LIMIT))
		return false;
#else
	/* B06L/B06M compatibility path; B06N replaces this unmeasured gap. */
	(void)b06j_pci_read(B06K_CHANNEL2_DEV, B06K_MC_DDR3_CMD, 4);
#endif

	return true;
}

static bool b06l_issue_mode_update(void)
{
	unsigned int polls;

	b06j_pci_write(B06K_CHANNEL2_DEV, B06K_MC_INIT_CMD, 0x30200, 4);
	for (polls = 0; polls < B06K_TRAIN_POLL_LIMIT; polls++) {
		if (b06j_pci_read(B06K_CHANNEL2_DEV, B06K_MC_INIT_STATUS, 4) &
		    BIT(9))
			return true;
	}

	return false;
}

static bool b06l_issue_zqcl(unsigned int rank)
{
	unsigned int polls;
	uint32_t command = B06L_ASSERT_CKE | B06L_DO_ZQCL | B06L_IGNORE_RX |
		(rank << B06L_INIT_RANK_SHIFT);

	b06j_pci_write(B06K_CHANNEL2_DEV, B06K_MC_INIT_CMD, command, 4);
	for (polls = 0; polls < B06K_TRAIN_POLL_LIMIT; polls++) {
		if (b06j_pci_read(B06K_CHANNEL2_DEV, B06K_MC_INIT_STATUS, 4) &
		    B06L_ZQCL_COMPLETE) {
			b06j_pci_write(B06K_CHANNEL2_DEV, B06K_MC_INIT_CMD,
					  B06L_ASSERT_CKE | B06L_IGNORE_RX |
					  B06L_STOP_ON_FAIL, 4);
			return true;
		}
	}

	return false;
}

static bool b06l_prepare_rd_point(void)
{
	unsigned int rank;

	if ((b06j_pci_read(B06K_CHANNEL2_DEV, B06K_MC_RANK_PRESENT, 1) & 0xff) !=
	    B06L_RANK_PRESENT ||
	    b06j_pci_read(B06K_CHANNEL2_DEV, B06K_MC_MRS_VALUE_0_1, 4) !=
		((B06L_MR1 << 16) | B06L_MR0) ||
	    (b06j_pci_read(B06K_CHANNEL2_DEV, B06K_MC_MRS_VALUE_2, 2) & 0xffff) !=
		B06L_MR2 ||
	    b06j_pci_read(B06K_CHANNEL2_DEV, B06K_MC_BASE_TIMING, 4) !=
		B06L_BASE_TIMING ||
	    b06j_pci_read(B06L_QPI_LINK0_PHY_DEV, B06L_QPI_PH_PIS, 4) !=
		B06L_QPI_SLOW_PH_PIS)
		return false;

	/* fffc5f0b + fffc8065: temporary training gate and FIFO reset. */
	b06j_pci_write(B06K_CHANNEL2_DEV, B06K_MC_INIT_CMD, 0x200, 4);
	b06j_pci_write(B06K_CHANNEL2_DEV, B06K_MC_INIT_STATUS, 1, 1);
	b06j_pci_write(B06K_CHANNEL2_DEV, 0x58, B06L_RD_INIT_PARAMS, 4);
	b06j_pci_write(B06K_CHANNEL2_DEV, 0x50, 1, 4);
	(void)b06j_pci_read(B06K_CHANNEL2_DEV, 0x50, 4);
	b06j_pci_write(B06K_CHANNEL2_DEV, B06K_MC_INIT_CMD, 0x20600, 4);
	(void)b06j_pci_read(B06K_CHANNEL2_DEV, B06K_MC_INIT_CMD, 4);

	/*
	 * Intel DX58SO ffe37125 asserts CKE and issues MRS2/MRS3/MRS1/MRS0
	 * without the MSI-only bank-4 command.  MSI fffd2bbd derives that
	 * command's low word from a runtime DIMM table; no constant value has
	 * been reconstructed for this configuration, so do not invent one.
	 */
	b06j_pci_write(B06K_CHANNEL2_DEV, B06K_MC_INIT_CMD,
			  B06L_ASSERT_CKE | B06L_IGNORE_RX, 4);
	(void)b06j_pci_read(B06K_CHANNEL2_DEV, B06K_MC_INIT_CMD, 4);
	for (rank = 0; rank < 2; rank++) {
		if (!b06l_issue_mrs(rank, 2, B06L_MR2) ||
		    !b06l_issue_mrs(rank, 3, 0) ||
		    !b06l_issue_mrs(rank, 1, B06L_MR1) ||
		    !b06l_issue_mrs(rank, 0, B06L_MR0) ||
		    !b06l_issue_mode_update())
			return false;
	}
	for (rank = 0; rank < 2; rank++) {
		if (!b06l_issue_zqcl(rank))
			return false;
	}

	return true;
}

static bool b06l_train_rd(uint32_t *status)
{
	unsigned int polls;

	/* MSI fffd5fc5 issues 0x26b01 directly after fffd2bbd. */
	b06j_pci_write(B06K_CHANNEL2_DEV, B06K_MC_INIT_CMD, 0x26b01, 4);
	for (polls = 0; polls < B06K_TRAIN_POLL_LIMIT; polls++) {
		const uint32_t action = b06j_pci_read(B06K_CHANNEL2_DEV,
						      B06K_MC_INIT_CMD, 4);

		*status = b06j_pci_read(B06K_CHANNEL2_DEV,
					 B06K_MC_INIT_STATUS, 4);
		if (!(action & B06K_MC_TRAIN_ACTION) &&
		    (*status & B06K_MC_INIT_COMPLETE))
			return true;
	}

	return false;
}

#if CONFIG_X58_PRO_E_B06M_BASEINIT
#if CONFIG_X58_PRO_E_B06N_TIMED_MRS
static bool b06m_phase_marker(const char *message)
{
	if (!b04_uart_puts(message))
		return false;
	return b04_uart_wait_for(UART8250_LSR_TEMT,
				   B03_UART_FLUSH_POLL_LIMIT);
}
#else
#define b06m_phase_marker b04_uart_puts
#endif

static bool b06m_write_verify(const struct b06m_pci_value *setting)
{
	b06j_pci_write(setting->dev, setting->reg, setting->value, 4);
	if (b06j_pci_read(setting->dev, setting->reg, 4) == setting->value)
		return true;

	return b04_uart_puts("[BASE] READBACK_FAIL REG=") &&
		b04_uart_put_hex(setting->reg, 2) && b04_uart_puts("\r\n") &&
		false;
}

static bool b06m_initial_state_valid(void)
{
	return cpuid_eax(1) == B06M_CPUID_E5645 &&
		b06j_pci_read(B06L_QPI_LINK0_PHY_DEV, B06L_QPI_PH_PIS, 4) ==
			B06L_QPI_SLOW_PH_PIS &&
		b06j_pci_read(B06K_UNCORE_COMMON_DEV, 0x50, 4) ==
			B06M_DDR_RATIO_STATUS &&
		b06j_pci_read(B06K_UNCORE_COMMON_DEV, 0x54, 4) ==
			B06M_DDR_RATIO &&
		b06j_pci_read(B06M_MC_COMMON_DEV, B06M_MC_CONTROL, 4) == 0 &&
		b06j_pci_read(B06M_CHANNEL2_ADDR_DEV, B06M_MC_DOD_DIMM0, 4) == 0 &&
		b06j_pci_read(B06K_CHANNEL2_DEV, 0x58, 4) == 0 &&
		b06j_pci_read(B06K_CHANNEL2_DEV, B06K_MC_MRS_VALUE_0_1, 4) == 0 &&
		b06j_pci_read(B06K_CHANNEL2_DEV, B06K_MC_MRS_VALUE_2, 4) == 0 &&
		b06j_pci_read(B06K_CHANNEL2_DEV, B06K_MC_RANK_PRESENT, 4) == 0;
}

static bool b06m_program_patterns(void)
{
	unsigned int index;

	b06j_pci_write(B06K_UNCORE_COMMON_DEV, 0xa8, 0x01000000, 4);
	for (index = 0; index < ARRAY_SIZE(b06m_pattern_select); index++) {
		b06j_pci_write(B06K_UNCORE_COMMON_DEV, 0xa8,
				  b06m_pattern_select[index], 2);
		b06j_pci_write(B06K_UNCORE_COMMON_DEV, 0xb0,
				  b06m_pattern_value[index], 4);
		if (b06j_pci_read(B06K_UNCORE_COMMON_DEV, 0xb0, 4) !=
		    b06m_pattern_value[index])
			return false;
	}
	b06j_pci_write(B06K_UNCORE_COMMON_DEV, 0xa8, 0x0018, 2);
	b06j_pci_write(B06K_UNCORE_COMMON_DEV, 0xa8, 0x0100001c, 4);
	b06j_pci_write(B06K_UNCORE_COMMON_DEV, 0xac, 0x00380000, 4);
	b06j_pci_write(B06K_UNCORE_COMMON_DEV, 0xbc, 0, 4);
	b06j_pci_write(B06K_UNCORE_COMMON_DEV, 0xc0, 0, 4);
	b06j_pci_write(B06K_UNCORE_COMMON_DEV, B06K_PHY_SELECT_REG, 1, 4);

	return b06j_pci_read(B06K_UNCORE_COMMON_DEV, 0xa8, 4) == 0x0100001c &&
		b06j_pci_read(B06K_UNCORE_COMMON_DEV, 0xac, 4) == 0x00380000 &&
		b06j_pci_read(B06K_UNCORE_COMMON_DEV, B06K_PHY_SELECT_REG, 4) == 1;
}

static bool b06m_wait_rcomp(void)
{
	unsigned int polls;

	for (polls = 0; polls < B06K_TRAIN_POLL_LIMIT; polls++) {
		if (b06j_pci_read(B06K_CHANNEL2_DEV, B06K_MC_INIT_STATUS, 4) &
		    B06M_RCOMP_COMPLETE)
			return true;
	}

	return false;
}

static bool b06m_run_baseinit(void)
{
	unsigned int index;
	unsigned int rank;
	uint32_t status;

	if (!b06m_initial_state_valid())
		return false;
	if (!b04_uart_puts("[BASE] PREFLIGHT_OK\r\n"))
		return false;

	for (index = 0; index < ARRAY_SIZE(b06m_base_policy); index++) {
		if (!b06m_write_verify(&b06m_base_policy[index]))
			return false;
	}
	if (!b06m_program_patterns() ||
	    !b04_uart_puts("[BASE] POLICY_PATTERN_OK\r\n"))
		return false;

	/* B06N explicitly proves each phase marker reached UART TEMT. */
	b06j_pci_write(B06K_CHANNEL2_DEV, B06K_MC_INIT_CMD,
			  B06L_IGNORE_RX, 4);
	if (!b06m_phase_marker("[BASE] IGNORE_RX\r\n"))
		return false;
	b06j_pci_write(B06M_MC_COMMON_DEV, B06M_MC_RESET_CONTROL, 1, 1);
	if (!b06m_phase_marker("[BASE] MC_RESET=1\r\n"))
		return false;
	b06j_pci_write(B06K_CHANNEL2_DEV, B06M_MC_DIMM_RESET_CMD, 1, 4);
	if (!b06m_phase_marker("[BASE] DIMM_RESET=1\r\n"))
		return false;
	b06j_pci_write(B06K_CHANNEL2_DEV, B06M_MC_DIMM_RESET_CMD, 4, 4);
	if (!b06m_phase_marker("[BASE] BLOCK_CKE=1\r\n"))
		return false;
	b06j_pci_write(B06K_CHANNEL2_DEV, B06M_MC_DIMM_RESET_CMD, 0, 4);
	if (!b06m_phase_marker("[BASE] RESET_RELEASED\r\n"))
		return false;
	b06j_pci_write(B06K_CHANNEL2_DEV, B06K_MC_INIT_CMD,
			  B06L_ASSERT_CKE | B06L_IGNORE_RX, 4);
	if (!b06m_phase_marker("[BASE] CKE_ASSERTED\r\n"))
		return false;

#if CONFIG_X58_PRO_E_B06N_TIMED_MRS
	if (!b04_uart_puts("[BASE] MRS_GAPS="))
		return false;
#endif
	for (rank = 0; rank < 2; rank++) {
		if (!b06l_issue_mrs(rank, 2, B06L_MR2) ||
		    !b06l_issue_mrs(rank, 3, 0) ||
		    !b06l_issue_mrs(rank, 1, B06L_MR1) ||
		    !b06l_issue_mrs(rank, 0, B06L_MR0))
			return false;
	}
#if CONFIG_X58_PRO_E_B06N_TIMED_MRS
	if (!b04_uart_puts("\r\n"))
		return false;
#endif
	if (!b04_uart_puts("[BASE] MRS_RANKS_0_1_OK\r\n"))
		return false;

	b06j_pci_write(B06K_CHANNEL2_DEV, B06K_MC_INIT_CMD,
			  B06M_DO_RCOMP | B06L_IGNORE_RX, 4);
	if (!b06m_wait_rcomp() || !b04_uart_puts("[BASE] RCOMP_OK\r\n"))
		return false;
	for (rank = 0; rank < 2; rank++) {
		if (!b06l_issue_zqcl(rank) ||
		    !b04_uart_puts(rank ? "[BASE] ZQCL_R1_OK\r\n" :
					   "[BASE] ZQCL_R0_OK\r\n"))
			return false;
	}

	status = b06j_pci_read(B06K_CHANNEL2_DEV, B06K_MC_INIT_STATUS, 4);
	if ((b06j_pci_read(B06M_MC_COMMON_DEV, B06M_MC_CONTROL, 4) &
	     B06M_MC_INIT_DONE) ||
	    b06j_pci_read(B06L_QPI_LINK0_PHY_DEV, B06L_QPI_PH_PIS, 4) !=
		B06L_QPI_SLOW_PH_PIS ||
	    (b06j_pci_read(B06K_CHANNEL2_DEV, B06K_MC_RANK_PRESENT, 1) & 0xff) !=
		B06L_RANK_PRESENT)
		return false;

	return b04_uart_puts("[BASE] READY STATUS=") &&
		b04_uart_put_hex(status, 8) &&
		b04_uart_puts(" QPI=030f0f03 INIT_DONE=0\r\n");
}
#endif

static bool b06k_report_lane_pair(const char *prefix, unsigned int lane,
				  const struct b06k_phy_seed *pair)
{
	uint32_t raw;
	uint32_t first;
	uint32_t second;

	if (!b06k_phy_read(2, pair[0].start, pair[0].width, &raw, &first) ||
	    !b06k_phy_read(2, pair[1].start, pair[1].width, &raw, &second))
		return false;

	return b04_uart_puts(prefix) && b04_uart_put_hex(lane, 1) &&
		b04_uart_puts(" A=") && b04_uart_put_hex(first, 4) &&
		b04_uart_puts(" B=") && b04_uart_put_hex(second, 4) &&
		b04_uart_puts("\r\n");
}

static bool b06k_run_rd_try(uint32_t sweep)
{
	struct b06k_phy_seed global = { 0x09c7, 9, sweep, 1 };
	uint32_t status = 0;
	unsigned int lane;

	if (sweep > 0x80)
		return b06j_error("rdtry SWEEP must be <= 80");
	if (!b06k_phy_apply(b06k_rd_pulse, ARRAY_SIZE(b06k_rd_pulse)) ||
	    !b06k_phy_apply(&global, 1) ||
	    !b06k_phy_apply(b06k_rd_lane_seed,
			    ARRAY_SIZE(b06k_rd_lane_seed)))
		return b06j_error("RD PHY write timeout");
	if (!b06k_train(0x26b01, &status))
		return b06j_error("RD training timeout");
	if (!b06j_print_u32("RD_STATUS=", status, 8))
		return false;
	for (lane = 0; lane < 8; lane++) {
		if (!b06k_report_lane_pair("RD", lane,
					   &b06k_rd_lane_seed[lane * 2]))
			return b06j_error("RD PHY read timeout");
	}

	return true;
}

static bool b06l_set_rd_pulses(uint32_t value)
{
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(b06k_rd_pulse); index++) {
		if (!b06k_phy_write(2, b06k_rd_pulse[index].start,
				    b06k_rd_pulse[index].width, value,
				    b06k_rd_pulse[index].mode))
			return false;
	}

	return true;
}

static bool b06l_seed_rd_point(uint32_t sweep)
{
	struct b06k_phy_seed global = { 0x09c7, 9, sweep, 1 };

	return sweep <= 0x80 &&
		b06k_phy_apply(&global, 1) &&
		b06k_phy_apply(b06k_rd_lane_seed,
				 ARRAY_SIZE(b06k_rd_lane_seed));
}

static bool b06l_report_rd_point(uint32_t sweep, uint32_t status)
{
	unsigned int lane;

	if (!b04_uart_puts("RD_SWEEP=") || !b04_uart_put_hex(sweep, 2) ||
	    !b04_uart_puts(" STATUS=") || !b04_uart_put_hex(status, 8))
		return false;
	for (lane = 0; lane < 8; lane++) {
		const struct b06k_phy_seed *pair =
			&b06k_rd_lane_seed[lane * 2];
		uint32_t raw;
		uint32_t first;
		uint32_t second;

		if (!b06k_phy_read(2, pair[0].start, pair[0].width,
				   &raw, &first) ||
		    !b06k_phy_read(2, pair[1].start, pair[1].width,
				   &raw, &second) ||
		    !b04_uart_puts(" L") || !b04_uart_put_hex(lane, 1) ||
		    !b04_uart_putc('=') || !b04_uart_put_hex(first, 3) ||
		    !b04_uart_putc('/') || !b04_uart_put_hex(second, 3))
			return false;
	}

	return b04_uart_puts("\r\n");
}

static bool b06l_run_rd_point(uint32_t sweep, uint32_t *status)
{
	if (!b06l_seed_rd_point(sweep))
		return false;
	if (!b06l_prepare_rd_point())
		return false;
	if (!b06l_train_rd(status))
		return false;

	return b06l_report_rd_point(sweep, *status);
}

static bool b06l_run_rd_exact(uint32_t sweep, uint32_t *status)
{
	bool result;

	if (!b06l_set_rd_pulses(2))
		return false;
	result = b06l_run_rd_point(sweep, status);
	if (!b06l_set_rd_pulses(0))
		return false;

	return result;
}

static bool b06l_run_rd_sweep(void)
{
	uint32_t sweep;
	bool result = false;

	/* MSI fffd82bf holds these nine fields across the complete loop. */
	if (!b06l_set_rd_pulses(2))
		return false;

	for (sweep = 0; sweep <= 0x80; sweep += 2) {
		uint32_t status = 0;

		if (!b06l_run_rd_point(sweep, &status))
			break;
		if (status & BIT(3)) {
			result = b04_uart_puts("RD_SWEEP_PASS=") &&
				b04_uart_put_hex(sweep, 2) &&
				b04_uart_puts("\r\n");
			break;
		}
	}
	if (!b06l_set_rd_pulses(0))
		return b06j_error("RD pulse clear timeout");
	if (sweep <= 0x80)
		return result || b06j_error("exact RD sweep gate/timeout");

	return b04_uart_puts("RD_SWEEP_NO_PASS\r\n");
}

static bool b06k_run_rcven_try(uint32_t coarse)
{
	struct b06k_phy_seed global = { 0x09fa, 8, coarse, 1 };
	uint32_t status = 0;
	unsigned int lane;

	if (coarse > 0x3f)
		return b06j_error("rcvtry COARSE must be <= 3f");
	/* MSI and DX58SO both write 0x0100 to the low word before RCVEN. */
	b06j_pci_write(B06K_CHANNEL2_DEV, B06K_MC_ODT_PARAMS2, 0x100, 2);
	if (!b06k_phy_apply(&global, 1) ||
	    !b06k_phy_apply(b06k_rcven_lane_seed,
			    ARRAY_SIZE(b06k_rcven_lane_seed)))
		return b06j_error("RCVEN PHY write timeout");
	if (!b06k_train(0x27301, &status))
		return b06j_error("RCVEN training timeout");
	if (!b06j_print_u32("RCV_STATUS=", status, 8))
		return false;
	for (lane = 0; lane < 8; lane++) {
		if (!b06k_report_lane_pair("RCV", lane,
					   &b06k_rcven_lane_seed[lane * 2]))
			return b06j_error("RCVEN PHY read timeout");
	}

	return true;
}

static uint32_t b06j_mem_read(uint32_t address, unsigned int width)
{
	if (width == 1)
		return *(volatile uint8_t *)(uintptr_t)address;
	if (width == 2)
		return *(volatile uint16_t *)(uintptr_t)address;
	return *(volatile uint32_t *)(uintptr_t)address;
}

static void b06j_mem_write(uint32_t address, uint32_t value,
			   unsigned int width)
{
	if (width == 1)
		*(volatile uint8_t *)(uintptr_t)address = value;
	else if (width == 2)
		*(volatile uint16_t *)(uintptr_t)address = value;
	else
		*(volatile uint32_t *)(uintptr_t)address = value;
}

#if CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
static bool b06v1_parse_value(char *text, struct x58_rs_value *value)
{
	char *separator = NULL;
	bool result;

	for (char *cursor = text; *cursor; cursor++) {
		if (*cursor != ':')
			continue;
		if (separator != NULL)
			return false;
		separator = cursor;
	}
	value->hi = 0;
	if (separator == NULL)
		return b06j_parse_hex(text, &value->lo);
	*separator = '\0';
	result = b06j_parse_hex(text, &value->hi) &&
		b06j_parse_hex(separator + 1, &value->lo);
	*separator = ':';
	return result;
}

static bool b06v1_parse_space(const char *text, enum x58_rs_space *space)
{
	if (b06j_streq(text, "io"))
		*space = X58_RS_SPACE_IO;
	else if (b06j_streq(text, "pci"))
		*space = X58_RS_SPACE_PCI;
	else if (b06j_streq(text, "mem"))
		*space = X58_RS_SPACE_MEM;
	else if (b06j_streq(text, "msr"))
		*space = X58_RS_SPACE_MSR;
	else
		return false;
	return true;
}

static bool b06v1_parse_width(const char *text, enum x58_rs_space space,
	unsigned int *width)
{
	if (space == X58_RS_SPACE_MSR) {
		if (!b06j_streq(text, "q"))
			return false;
		*width = 8;
		return true;
	}
	return b06j_width(text, width);
}

static bool b06v1_parse_kind(const char *text, enum x58_rs_kind *kind)
{
	if (b06j_streq(text, "read"))
		*kind = X58_RS_OP_READ;
	else if (b06j_streq(text, "write"))
		*kind = X58_RS_OP_WRITE;
	else if (b06j_streq(text, "mask"))
		*kind = X58_RS_OP_MASK;
	else if (b06j_streq(text, "poll"))
		*kind = X58_RS_OP_POLL;
	else if (b06j_streq(text, "delay"))
		*kind = X58_RS_OP_DELAY;
	else if (b06j_streq(text, "assert"))
		*kind = X58_RS_OP_ASSERT;
	else
		return false;
	return true;
}

static bool b06v1_parse_reversibility(const char *text, uint8_t *flags)
{
	if (b06j_streq(text, "rev"))
		*flags = X58_RS_FLAG_REVERSIBLE;
	else if (b06j_streq(text, "nr"))
		*flags = 0;
	else
		return false;
	return true;
}

static bool b06v1_print_value(const struct x58_rs_value *value,
	unsigned int width)
{
	if (width == 8)
		return b04_uart_put_hex(value->hi, 8) &&
			b04_uart_putc(':') &&
			b04_uart_put_hex(value->lo, 8);
	return b04_uart_put_hex(value->lo, width * 2);
}

static bool b06v1_print_op(const char *prefix, size_t index,
	const struct x58_rs_op *op)
{
	if (!b04_uart_puts(prefix) || !b04_uart_puts("I=") ||
	    !b04_uart_put_hex(index, 2) || !b04_uart_puts(" OP=") ||
	    !b04_uart_puts(x58_rs_kind_name(op->kind)))
		return false;
	if (op->kind == X58_RS_OP_DELAY)
		return b04_uart_puts(" ITER=") &&
			b04_uart_put_hex(op->limit, 8) && b04_uart_puts("\r\n");
	if (!b04_uart_puts(" SPACE=") ||
	    !b04_uart_puts(x58_rs_space_name(op->space)) ||
	    !b04_uart_puts(" TARGET=") || !b04_uart_put_hex(op->target, 8) ||
	    !b04_uart_puts(" WIDTH=") ||
	    !b04_uart_putc(op->width == 8 ? 'q' :
		(op->width == 4 ? 'l' : (op->width == 2 ? 'w' : 'b'))))
		return false;
	if (op->kind == X58_RS_OP_WRITE || op->kind == X58_RS_OP_MASK) {
		if (!b04_uart_puts(" REV=") ||
		    !b04_uart_put_hex(!!(op->flags & X58_RS_FLAG_REVERSIBLE), 2))
			return false;
	}
	if (op->kind == X58_RS_OP_MASK || op->kind == X58_RS_OP_POLL ||
	    op->kind == X58_RS_OP_ASSERT) {
		if (!b04_uart_puts(op->kind == X58_RS_OP_MASK ? " CLEAR=" :
				   " MASK=") ||
		    !b06v1_print_value(&op->mask, op->width))
			return false;
	}
	if (op->kind != X58_RS_OP_READ) {
		if (!b04_uart_puts(op->kind == X58_RS_OP_MASK ? " SET=" :
				   " VALUE=") ||
		    !b06v1_print_value(&op->value, op->width))
			return false;
	}
	if (op->kind == X58_RS_OP_POLL &&
	    (!b04_uart_puts(" LIMIT=") || !b04_uart_put_hex(op->limit, 8)))
		return false;
	return b04_uart_puts("\r\n");
}

static bool b06v1_print_trace(const char *prefix, size_t index,
	const struct x58_rs_trace *trace)
{
	const unsigned int width = trace->op.width ? trace->op.width : 4;

	if (!b04_uart_puts(prefix) || !b04_uart_puts("I=") ||
	    !b04_uart_put_hex(index, 2) || !b04_uart_puts(" STATUS=") ||
	    !b04_uart_puts(x58_rs_trace_name(trace->status)) ||
	    !b04_uart_puts(" BEFORE=") ||
	    !b06v1_print_value(&trace->before, width) ||
	    !b04_uart_puts(" OBS=") ||
	    !b06v1_print_value(&trace->observed, width) ||
	    !b04_uart_puts(" ITER=") || !b04_uart_put_hex(trace->iterations, 8) ||
	    !b04_uart_puts(" WROTE=") ||
	    !b04_uart_put_hex(trace->write_performed, 2) ||
	    !b04_uart_puts(" RB=") ||
	    !b04_uart_puts(x58_rs_trace_name(trace->rollback_status)))
		return false;
	if (trace->rollback_status != X58_RS_TRACE_EMPTY &&
	    (!b04_uart_puts(" RB_OBS=") ||
	     !b06v1_print_value(&trace->rollback_observed, trace->op.width)))
		return false;
	return b04_uart_puts("\r\n");
}

static bool b06v1_backend_read(void *context, const struct x58_rs_op *op,
	struct x58_rs_value *value)
{
	(void)context;
	value->hi = 0;
	if (op->space == X58_RS_SPACE_IO) {
		value->lo = b06j_io_read(op->target, op->width);
	} else if (op->space == X58_RS_SPACE_PCI) {
		value->lo = b06j_pci_read(PCI_DEV(op->target >> 24,
			(op->target >> 16) & 0xff, (op->target >> 8) & 0xff),
			op->target & 0xff, op->width);
	} else if (op->space == X58_RS_SPACE_MEM) {
		value->lo = b06j_mem_read(op->target, op->width);
	} else if (op->space == X58_RS_SPACE_MSR) {
		const msr_t msr = rdmsr(op->target);

		value->lo = msr.lo;
		value->hi = msr.hi;
	} else {
		return false;
	}
	return true;
}

static bool b06v1_backend_write(void *context, const struct x58_rs_op *op,
	const struct x58_rs_value *value)
{
	(void)context;
	if (op->space == X58_RS_SPACE_MEM) {
		const uintptr_t start = op->target;
		const uintptr_t end = start + op->width;

		/* A scripted write must not destroy the monitor, stack, or log in CAR. */
		if (start < (uintptr_t)_car_region_end &&
		    end > (uintptr_t)_car_region_start)
			return false;
	}
	if (op->space == X58_RS_SPACE_IO) {
		b06j_io_write(op->target, value->lo, op->width);
	} else if (op->space == X58_RS_SPACE_PCI) {
		b06j_pci_write(PCI_DEV(op->target >> 24,
			(op->target >> 16) & 0xff, (op->target >> 8) & 0xff),
			op->target & 0xff, value->lo, op->width);
	} else if (op->space == X58_RS_SPACE_MEM) {
		b06j_mem_write(op->target, value->lo, op->width);
	} else if (op->space == X58_RS_SPACE_MSR) {
		const msr_t msr = { .lo = value->lo, .hi = value->hi };

		wrmsr(op->target, msr);
	} else {
		return false;
	}
	return true;
}

static void b06v1_backend_delay(void *context, uint32_t iterations)
{
	(void)context;
	while (iterations--)
		asm volatile("pause" ::: "memory");
}

static bool b06v1_backend_event(void *context, enum x58_rs_event event,
	size_t index, const struct x58_rs_trace *trace)
{
	bool result;

	(void)context;
	if (event == X58_RS_EVENT_PRE) {
		outb(POST_B06V1_SCRIPT_RUN, CONFIG_POST_IO_PORT);
		result = b06v1_print_op("[SCRIPT] PRE ", index, &trace->op);
	} else if (event == X58_RS_EVENT_POST) {
		result = b06v1_print_trace("[SCRIPT] POST ", index, trace);
	} else if (event == X58_RS_EVENT_ROLLBACK_PRE) {
		outb(POST_B06V1_ROLLBACK_RUN, CONFIG_POST_IO_PORT);
		result = b06v1_print_op("[SCRIPT] ROLLBACK_PRE ", index,
			&trace->op);
	} else {
		result = b06v1_print_trace("[SCRIPT] ROLLBACK_POST ", index,
			trace);
	}
	return result && b04_uart_wait_for(UART8250_LSR_TEMT,
		B03_UART_FLUSH_POLL_LIMIT);
}

static const struct x58_rs_backend b06v1_backend = {
	.context = NULL,
	.read = b06v1_backend_read,
	.write = b06v1_backend_write,
	.delay = b06v1_backend_delay,
	.event = b06v1_backend_event,
};

static bool b06v1_print_result(const char *label, enum x58_rs_result result)
{
	return b04_uart_puts("[SCRIPT] ") && b04_uart_puts(label) &&
		b04_uart_puts("=") && b04_uart_puts(x58_rs_result_name(result)) &&
		b04_uart_puts("\r\n");
}

static bool b06v1_report_result(const char *label, enum x58_rs_result result)
{
	if (result != X58_RS_OK)
		outb(POST_B06J_COMMAND_ERROR, CONFIG_POST_IO_PORT);
	return b06v1_print_result(label, result);
}

static bool b06v1_print_status(void)
{
	struct x58_rs_info info;

	x58_rs_get_info(&info);
	return b04_uart_puts("[SCRIPT] FORMAT=") &&
		b04_uart_put_hex(X58_RS_FORMAT_VERSION, 2) &&
		b04_uart_puts(" OPS=") &&
		b04_uart_put_hex(info.op_count, 2) &&
		b04_uart_puts(" PROGRAM_FNV=") &&
		b04_uart_put_hex(info.program_digest, 8) &&
		b04_uart_puts(" SEALED=") &&
		b04_uart_put_hex(info.program_sealed, 2) &&
		b04_uart_puts(" TXN_VALID=") &&
		b04_uart_put_hex(info.transaction_valid, 2) &&
		b04_uart_puts(" TRACE=") && b04_uart_put_hex(info.trace_count, 2) &&
		b04_uart_puts(" TXN_FNV=") &&
		b04_uart_put_hex(info.transaction_digest, 8) &&
		b04_uart_puts(" LAST=") && b04_uart_puts(x58_rs_result_name(
			info.last_result)) &&
		b04_uart_puts(" MUT=") && b04_uart_put_hex(info.mutations, 2) &&
		b04_uart_puts(" NONREV=") &&
		b04_uart_put_hex(info.nonreversible_mutations, 2) &&
		b04_uart_puts(" RB_AVAILABLE=") &&
		b04_uart_put_hex(info.rollback_available, 2) &&
		b04_uart_puts(" RB_RESULT=") &&
		b04_uart_puts(x58_rs_result_name(info.rollback_result)) &&
		b04_uart_puts(" RB_ATTEMPTED=") &&
		b04_uart_put_hex(info.rollback_attempted, 2) &&
		b04_uart_puts(" RB_TRIES=") &&
		b04_uart_put_hex(info.rollback_attempts, 8) &&
		b04_uart_puts(" ROLLED_BACK=") &&
		b04_uart_put_hex(info.rolled_back, 2) && b04_uart_puts("\r\n");
}

static bool b06v1_script_add(unsigned int argc, char **argv)
{
	struct x58_rs_op op = { 0 };
	enum x58_rs_kind kind;
	enum x58_rs_space space;
	unsigned int width;
	enum x58_rs_result result;

	if (argc < 4 || !b06v1_parse_kind(argv[2], &kind))
		return b06j_error("script add read|write|mask|poll|delay|assert ...");
	op.kind = kind;
	if (kind == X58_RS_OP_DELAY) {
		if (argc != 4 || !b06j_parse_hex(argv[3], &op.limit))
			return b06j_error("use: script add delay ITER");
		op.space = X58_RS_SPACE_NONE;
	} else {
		if (argc < 6 || !b06v1_parse_space(argv[3], &space) ||
		    !b06j_parse_hex(argv[4], &op.target) ||
		    !b06v1_parse_width(argv[5], space, &width))
			return b06j_error("script target: SPACE TARGET b|w|l; MSR uses q");
		op.space = space;
		op.width = width;
		if (kind == X58_RS_OP_READ && argc != 6)
			return b06j_error("use: script add read SPACE TARGET WIDTH");
		if (kind == X58_RS_OP_WRITE &&
		    (argc != 8 || !b06v1_parse_value(argv[6], &op.value) ||
		     !b06v1_parse_reversibility(argv[7], &op.flags)))
			return b06j_error(
				"use: script add write SPACE TARGET WIDTH VALUE rev|nr");
		if (kind == X58_RS_OP_MASK &&
		    (argc != 9 || !b06v1_parse_value(argv[6], &op.mask) ||
		     !b06v1_parse_value(argv[7], &op.value) ||
		     !b06v1_parse_reversibility(argv[8], &op.flags)))
			return b06j_error(
				"use: script add mask SPACE TARGET WIDTH CLEAR SET rev|nr");
		if (kind == X58_RS_OP_POLL &&
		    (argc != 9 || !b06v1_parse_value(argv[6], &op.mask) ||
		     !b06v1_parse_value(argv[7], &op.value) ||
		     !b06j_parse_hex(argv[8], &op.limit)))
			return b06j_error(
				"use: script add poll SPACE TARGET WIDTH MASK EXPECT LIMIT");
		if (kind == X58_RS_OP_ASSERT &&
		    (argc != 8 || !b06v1_parse_value(argv[6], &op.mask) ||
		     !b06v1_parse_value(argv[7], &op.value)))
			return b06j_error(
				"use: script add assert SPACE TARGET WIDTH MASK EXPECT");
	}
	result = x58_rs_add(&op);
	if (!b06v1_report_result("ADD", result))
		return false;
	return result != X58_RS_OK || b06v1_print_status();
}

static bool b06v1_script_list(bool trace_log, unsigned int argc, char **argv)
{
	struct x58_rs_info info;
	uint32_t start = 0;
	uint32_t count;

	x58_rs_get_info(&info);
	count = trace_log ? info.trace_count : info.op_count;
	if (argc == 4) {
		if (!b06j_parse_hex(argv[2], &start) ||
		    !b06j_parse_hex(argv[3], &count))
			return b06j_error("use: script list|trace [START COUNT]");
	} else if (argc != 2) {
		return b06j_error("use: script list|trace [START COUNT]");
	}
	if (!count || start >= (trace_log ? info.trace_count : info.op_count) ||
	    count > (trace_log ? info.trace_count : info.op_count) - start)
		return b06j_error("script list/trace range");
	for (size_t i = start; i < start + count; i++) {
		if (trace_log) {
			struct x58_rs_trace trace;

			if (!x58_rs_get_trace(i, &trace) ||
			    !b06v1_print_op("[SCRIPT] TRACE_OP ", i, &trace.op) ||
			    !b06v1_print_trace("[SCRIPT] TRACE ", i, &trace))
				return false;
		} else {
			struct x58_rs_op op;

			if (!x58_rs_get_op(i, &op) ||
			    !b06v1_print_op("[SCRIPT] LIST ", i, &op))
				return false;
		}
	}
	return true;
}
#endif

#if CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
static void b06v0_policy_write16(uint8_t *policy, unsigned int offset,
				 uint16_t value)
{
	policy[offset] = value;
	policy[offset + 1] = value >> 8;
}

static void b06v0_policy_write32(uint8_t *policy, unsigned int offset,
				 uint32_t value)
{
	policy[offset] = value;
	policy[offset + 1] = value >> 8;
	policy[offset + 2] = value >> 16;
	policy[offset + 3] = value >> 24;
}

#if CONFIG_X58_PRO_E_B06V5_COLD_MINIT
static uint32_t b06v0_policy_read32(const uint8_t *policy, unsigned int offset)
{
	return (uint32_t)policy[offset] |
		((uint32_t)policy[offset + 1] << 8) |
		((uint32_t)policy[offset + 2] << 16) |
		((uint32_t)policy[offset + 3] << 24);
}
#endif

static uint32_t b06v0_buffer_digest(const uint8_t *buffer, size_t size)
{
	uint32_t digest = 2166136261U;
	unsigned int offset;

	for (offset = 0; offset < size; offset++) {
		digest ^= buffer[offset];
		digest *= 16777619U;
	}

	return digest;
}

static uint32_t b06v0_policy_digest(const uint8_t *policy)
{
	return b06v0_buffer_digest(policy, X58_VENDOR_MINIT_POLICY_SIZE);
}

static uint8_t b06v0_cmos_direct(uint8_t selector)
{
	outb(selector, 0x72);
	return inb(0x73);
}

#if CONFIG_X58_PRO_E_B06V4_POLICY_TELEMETRY
#define B06V4_CMOS_DIAGNOSTIC_SELECTOR	0x0e
#define B06V4_CMOS_DIAGNOSTIC_ERROR_MASK	0xc0

struct b06v4_policy_inputs {
	uint8_t diagnostic_0e;
	uint8_t extended_81;
	uint8_t extended_82;
	uint8_t extended_88;
	uint8_t extended_89;
	uint8_t extended_8e;
	uint8_t extended_ca;
	uint8_t extended_f1;
	uint8_t extended_f5;
	uint8_t alt_gp_smi_en_low;
};

static void b06v4_capture_policy_inputs(struct b06v4_policy_inputs *inputs)
{
	const uint8_t saved_standard_selector = inb(0x70);
	const uint8_t saved_extended_selector = inb(0x72);

	/* The original wrapper disables NMI before reading CMOS diagnostic 0x0e. */
	outb(B06V4_CMOS_DIAGNOSTIC_SELECTOR | BIT(7), 0x70);
	(void)inb(0x61);
	inputs->diagnostic_0e = inb(0x71);
	inputs->extended_81 = b06v0_cmos_direct(0x81);
	inputs->extended_82 = b06v0_cmos_direct(0x82);
	inputs->extended_88 = b06v0_cmos_direct(0x88);
	inputs->extended_89 = b06v0_cmos_direct(0x89);
	inputs->extended_8e = b06v0_cmos_direct(0x8e);
	inputs->extended_ca = b06v0_cmos_direct(0xca);
	inputs->extended_f1 = b06v0_cmos_direct(0xf1);
	inputs->extended_f5 = b06v0_cmos_direct(0xf5);
	/* The MSI wrapper reads only the low byte before extracting bits 6:4. */
	inputs->alt_gp_smi_en_low = inb(DEFAULT_PMBASE + ALT_GP_SMI_EN);

	/* Telemetry restores both selector/NMI states and never writes CMOS data. */
	outb(saved_extended_selector, 0x72);
	outb(saved_standard_selector, 0x70);
}

static bool b06v4_print_policy_inputs(const struct b06v4_policy_inputs *inputs)
{
	const bool diagnostic_valid =
		!(inputs->diagnostic_0e & B06V4_CMOS_DIAGNOSTIC_ERROR_MASK);
	const uint8_t effective_8e =
		inputs->extended_8e > 0x30 ? 0x30 : inputs->extended_8e;

	return b04_uart_puts("[VENDOR] INPUT CMOS_DIAG_0E=") &&
		b04_uart_put_hex(inputs->diagnostic_0e, 2) &&
		b04_uart_puts(" VALID=") && b04_uart_put_hex(diagnostic_valid, 2) &&
		b04_uart_puts(" EXT81/82/88/89=") &&
		b04_uart_put_hex(inputs->extended_81, 2) && b04_uart_putc('/') &&
		b04_uart_put_hex(inputs->extended_82, 2) && b04_uart_putc('/') &&
		b04_uart_put_hex(inputs->extended_88, 2) && b04_uart_putc('/') &&
		b04_uart_put_hex(inputs->extended_89, 2) &&
		b04_uart_puts("\r\n[VENDOR] INPUT EXT8E=") &&
		b04_uart_put_hex(inputs->extended_8e, 2) &&
		b04_uart_puts(" EFFECTIVE8E=") && b04_uart_put_hex(effective_8e, 2) &&
		b04_uart_puts(" EXTCA/F1/F5=") &&
		b04_uart_put_hex(inputs->extended_ca, 2) && b04_uart_putc('/') &&
		b04_uart_put_hex(inputs->extended_f1, 2) && b04_uart_putc('/') &&
		b04_uart_put_hex(inputs->extended_f5, 2) &&
		b04_uart_puts(" ALT_GP_SMI_EN_LOW=") &&
		b04_uart_put_hex(inputs->alt_gp_smi_en_low, 2) &&
		b04_uart_puts(" FIELD_6_4=") &&
		b04_uart_put_hex((inputs->alt_gp_smi_en_low >> 4) & 7, 2) &&
		b04_uart_puts(" CMOS_DATA_WRITES=00\r\n");
}
#endif

static uint8_t b06v0_cmos_descriptor(uint16_t descriptor)
{
	const unsigned int width = (descriptor >> 12) & 0xf;
	uint8_t selector = (descriptor & 0xfff) >> 3;
	const unsigned int shift = descriptor & 7;
	uint8_t value;
	uint16_t mask = 0;
	unsigned int bit;

	if (width == 0 || width > 8)
		return 0;
	if (selector < 0x80) {
		/* CSI returns with NMI disabled; keep bit 7 set during CMOS reads. */
		outb(selector | BIT(7), 0x70);
		value = inb(0x71);
	} else {
		selector -= 0x80;
		outb(selector, 0x72);
		value = inb(0x73);
	}
	for (bit = 0; bit < width; bit++)
		mask = (mask << 1) | 1;

	return (value >> shift) & mask;
}

static void b06v0_policy_setup_fields(uint8_t *policy)
{
	uint8_t value;

	if (b06v0_cmos_direct(0xf5) & 3)
		policy[0xc3] = b06v0_cmos_descriptor(0x2546);
	policy[0xc5] |= b06v0_cmos_descriptor(0x1624);
	value = b06v0_cmos_descriptor(0x2540);
	if (value == 0) {
		const uint8_t fallback = b06v0_cmos_direct(0xf1);

		if (fallback & BIT(1))
			value = 2;
		if (fallback & BIT(2))
			value = 3;
	}
	policy[0xc6] = value;
	value = b06v0_cmos_descriptor(0x443c);
	policy[0xc7] = value ? value + 4 : 0;
	value = b06v0_cmos_descriptor(0x4450);
	policy[0xc8] = value ? value + 2 : 0;
	value = b06v0_cmos_descriptor(0x4454);
	policy[0xc9] = value ? value + 2 : 0;
	value = b06v0_cmos_descriptor(0x5388);
	policy[0xca] = value ? value + 8 : 0;
	value = b06v0_cmos_descriptor(0x7348);
	b06v0_policy_write16(policy, 0xcb, value ? value + 0x0e : 0);
	policy[0xcd] = b06v0_cmos_descriptor(0x4458);
	policy[0xce] = b06v0_cmos_descriptor(0x445c);
	policy[0xcf] = b06v0_cmos_descriptor(0x34f0);
	policy[0xd0] = b06v0_cmos_descriptor(0x4460);
	policy[0xd1] = b06v0_cmos_descriptor(0x2542);
	policy[0xd2] = b06v0_cmos_descriptor(0x4464);
	policy[0xd3] = b06v0_cmos_descriptor(0x4468);
	policy[0xd4] = b06v0_cmos_descriptor(0x5390);
	policy[0xd5] = b06v0_cmos_descriptor(0x5398);
	policy[0xd6] = b06v0_cmos_descriptor(0x53a0);
	policy[0xd7] = b06v0_cmos_descriptor(0x53a8);
	policy[0xd8] = b06v0_cmos_descriptor(0x446c);
	policy[0xd9] = b06v0_cmos_descriptor(0x4480);
	policy[0xda] = b06v0_cmos_descriptor(0x2544);
	policy[0xdb] = b06v0_cmos_descriptor(0x4484);
	policy[0xdc] = b06v0_cmos_descriptor(0x4488);
	policy[0xdd] = b06v0_cmos_descriptor(0x62c2);
}

static bool b06v0_spd_matches_target(const struct b06b_result *result,
				      uint8_t terminal)
{
	unsigned int offset;

	if (terminal != POST_B06I_SUCCESS || !result->header_valid ||
	    !result->upper_match || !result->decode_valid ||
	    !result->policy_valid || result->spd[0] != B06V0_TARGET_SPD_HEADER ||
	    result->crc_calculated != B06V0_TARGET_SPD_CRC ||
	    result->crc_stored != B06V0_TARGET_SPD_CRC ||
	    result->declared_used != SPD_SIZE_MAX_DDR3 ||
	    result->declared_total != SPD_SIZE_MAX_DDR3 ||
	    result->dimm.size_mb != 4096 || result->dimm.ranks != 2 ||
	    result->dimm.width != 8 || result->dimm.row_bits != 15 ||
	    result->dimm.col_bits != 10 || result->dimm.flags.is_ecc ||
	    result->policy.cas != 5 || result->policy.trcd != 5 ||
	    result->policy.trp != 5 || result->policy.tras != 12)
		return false;

	for (offset = 0; offset < SPD_DDR3_PART_LEN; offset++) {
		if (result->spd[SPD_DDR3_PART_NUM + offset] !=
		    b06v0_target_part[offset])
			return false;
	}

	return true;
}

static uint8_t b06v0_scan_spd_topology(uint8_t *ackmap, uint8_t *ddr3map)
{
	const uintptr_t base = CONFIG_FIXED_SMBUS_IO_BASE;
	uint8_t status;
	uint8_t terminal;
	unsigned int index;

	*ackmap = 0;
	*ddr3map = 0;
	/* The first HSTSTAT read acquires the I801 software semaphore. */
	status = inb(base + I801_HSTSTAT);
	if (status & I801_HSTSTAT_INUSE)
		return POST_B06_HOST_STATE_ERROR;
	if (status & I801_HSTSTAT_HOST_BUSY) {
		b06b_release_host(0);
		return POST_B06_HOST_STATE_ERROR;
	}
	if ((status & I801_HSTSTAT_FLAGS) ||
	    !b06b_control_idle(inb(base + I801_HSTCTL))) {
		b06b_release_host(0);
		return POST_B06_HOST_STATE_ERROR;
	}
	terminal = b06b_pin_error(inb(base + I801_PIN_CTL));
	if (terminal) {
		b06b_release_host(0);
		return terminal;
	}

	for (index = 0; index < B06V0_SPD_ADDRESS_COUNT; index++) {
		const uint8_t address = B06V0_SPD_FIRST_ADDRESS + index;
		uint32_t loops;
		uint8_t masked_status;
		uint8_t value = 0;

		outb(I801_BYTE_DATA, base + I801_HSTCTL);
		outb((address << 1) | 1, base + I801_XMITADD);
		outb(B06B_SPD_MEMORY_TYPE_OFFSET, base + I801_HSTCMD);
		outb(0, base + I801_HSTDAT0);
		outb(0, base + I801_HSTDAT1);
		if (inb(base + I801_HSTCTL) != I801_BYTE_DATA ||
		    inb(base + I801_XMITADD) != ((address << 1) | 1) ||
		    inb(base + I801_HSTCMD) != B06B_SPD_MEMORY_TYPE_OFFSET ||
		    inb(base + I801_HSTDAT0) != 0 ||
		    inb(base + I801_HSTDAT1) != 0) {
			b06b_release_host(0);
			return POST_B06_HOST_STATE_ERROR;
		}

		outb(I801_BYTE_DATA | I801_HSTCTL_START, base + I801_HSTCTL);
		for (loops = B06B_I801_POLL_LIMIT; loops; loops--) {
			status = inb(base + I801_HSTSTAT);
			if (!(status & I801_HSTSTAT_HOST_BUSY) &&
			    (status & I801_HSTSTAT_TERMINAL))
				break;
		}
		if (!loops)
			/* Preserve the unresolved controller state for diagnosis. */
			return POST_B06_TIMEOUT;

		masked_status = status & I801_HSTSTAT_RESULT;
		if (masked_status == I801_HSTSTAT_INTR) {
			value = inb(base + I801_HSTDAT0);
			*ackmap |= BIT(index);
			if (value == B06B_SPD_DDR3_MEMORY_TYPE)
				*ddr3map |= BIT(index);
		}
		terminal = b06b_pin_error(inb(base + I801_PIN_CTL));
		/* Clear completion flags while retaining semaphore ownership. */
		outb(status & I801_HSTSTAT_FLAGS, base + I801_HSTSTAT);
		if (terminal) {
			b06b_release_host(0);
			return terminal;
		}
		if (masked_status == I801_HSTSTAT_DEV_ERR)
			continue;
		if (masked_status == I801_HSTSTAT_BUS_ERR) {
			b06b_release_host(0);
			return POST_B06_BUS_ERROR;
		}
		if (masked_status != I801_HSTSTAT_INTR) {
			b06b_release_host(0);
			return POST_B06_TRANSACTION_ERROR;
		}
	}

	terminal = b06b_pin_error(inb(base + I801_PIN_CTL));
	b06b_release_host(0);
	return terminal;
}

static bool b06v0_emit_status(const char *operation,
			      enum x58_vendor_status status)
{
	return b04_uart_puts("[VENDOR] ") && b04_uart_puts(operation) &&
		b04_uart_puts(" STATUS=") &&
		b04_uart_puts(x58_vendor_status_name(status)) &&
		b04_uart_puts(" CODE=") && b04_uart_put_hex(status, 2) &&
		b04_uart_puts("\r\n");
}

static bool b06v0_print_status(const char *operation,
			       enum x58_vendor_status status)
{
	if (status != X58_VENDOR_OK)
		outb(POST_B06V0_ERROR, CONFIG_POST_IO_PORT);

	return b06v0_emit_status(operation, status);
}

static bool b06v0_print_call_result(const char *name,
				    const struct x58_vendor_call_result *result)
{
	return b04_uart_puts("[VENDOR] ") && b04_uart_puts(name) &&
		b04_uart_puts(" RETURN EAX=") && b04_uart_put_hex(result->eax, 8) &&
		b04_uart_puts(" EBX=") && b04_uart_put_hex(result->ebx, 8) &&
		b04_uart_puts(" ECX=") && b04_uart_put_hex(result->ecx, 8) &&
		b04_uart_puts(" EDX=") && b04_uart_put_hex(result->edx, 8) &&
		b04_uart_puts(" EDI=") && b04_uart_put_hex(result->edi, 8) &&
		b04_uart_puts(" EFLAGS=") && b04_uart_put_hex(result->eflags, 8) &&
		b04_uart_puts(" VESP=") &&
		b04_uart_put_hex(result->vendor_esp_after_return, 8) &&
		b04_uart_puts("\r\n");
}

static bool b06v0_print_runtime(const struct b06j_state *state)
{
	struct x58_vendor_runtime_info info;
	const enum x58_vendor_status status =
		x58_vendor_runtime_probe(&info);

	if (!b06v0_emit_status("PROBE", status) ||
	    !b04_uart_puts("[VENDOR] SIG WRAPPER=") ||
	    !b04_uart_put_hex(info.wrapper_signature_valid, 2) ||
	    !b04_uart_puts(" CSI=") ||
	    !b04_uart_put_hex(info.csi_signature_valid, 2) ||
	    !b04_uart_puts(" MINIT=") ||
	    !b04_uart_put_hex(info.minit_signature_valid, 2) ||
	    !b04_uart_puts(" CANARY=") ||
	    !b04_uart_put_hex(info.canaries_valid, 2) ||
	    !b04_uart_puts("\r\n[VENDOR] CPU CPUID1=") ||
	    !b04_uart_put_hex(info.cpuid_1_eax, 8) ||
	    !b04_uart_puts(" UCODE=") ||
	    !b04_uart_put_hex(info.microcode_revision, 8) ||
	    !b04_uart_puts(" APIC_BASE=") ||
	    !b04_uart_put_hex(info.apic_base_high, 8) || !b04_uart_putc(':') ||
	    !b04_uart_put_hex(info.apic_base_low, 8) ||
	    !b04_uart_puts(" CR0=") || !b04_uart_put_hex(info.cr0, 8) ||
	    !b04_uart_puts(" CR4=") || !b04_uart_put_hex(info.cr4, 8) ||
	    !b04_uart_puts(" EFLAGS=") ||
	    !b04_uart_put_hex(info.eflags_before_call, 8) ||
	    !b04_uart_puts(" CS/DS/ES/SS=") ||
	    !b04_uart_put_hex(info.cs_selector, 4) || !b04_uart_putc('/') ||
	    !b04_uart_put_hex(info.ds_selector, 4) || !b04_uart_putc('/') ||
	    !b04_uart_put_hex(info.es_selector, 4) || !b04_uart_putc('/') ||
	    !b04_uart_put_hex(info.ss_selector, 4) ||
	    !b04_uart_puts("\r\n[VENDOR] CAR=") ||
	    !b04_uart_put_hex(info.car_scratch_begin, 8) ||
	    !b04_uart_putc('-') || !b04_uart_put_hex(info.car_scratch_end, 8) ||
	    !b04_uart_puts(" STACK=") ||
	    !b04_uart_put_hex(info.vendor_stack_low, 8) ||
	    !b04_uart_putc('-') || b04_uart_put_hex(info.vendor_stack_top, 8) == false ||
	    !b04_uart_puts(" SIZE=") ||
	    !b04_uart_put_hex(info.vendor_stack_size, 8) ||
	    !b04_uart_puts(" HIGHWATER=") ||
	    !b04_uart_put_hex(info.vendor_stack_high_water, 8) ||
	    !b04_uart_puts("\r\n[VENDOR] SAD_ID=") ||
	    !b04_uart_put_hex(info.uncore_sad_id, 8) ||
	    !b04_uart_puts(" PCIEXBAR=") ||
	    !b04_uart_put_hex(info.pciexbar_high, 8) ||
	    !b04_uart_putc(':') || !b04_uart_put_hex(info.pciexbar_low, 8) ||
	    !b04_uart_puts(" X58_ID=") ||
	    !b04_uart_put_hex(info.x58_hostbridge_id, 8) ||
	    !b04_uart_puts(" CLASSREV=") ||
	    !b04_uart_put_hex(info.x58_hostbridge_class_revision, 8) ||
	    !b04_uart_puts("\r\n[VENDOR] QPI80=") ||
	    !b04_uart_put_hex(info.qpi_phy_observed_80, 8) ||
	    !b04_uart_puts(" MC50=") ||
	    !b04_uart_put_hex(info.memory_clock_observed_50, 8) ||
	    !b04_uart_puts(" MC54=") ||
	    !b04_uart_put_hex(info.memory_clock_observed_54, 8) ||
	    !b04_uart_puts("\r\n[VENDOR] PHASE CSI_ARMED=") ||
	    !b04_uart_put_hex(info.csi_wrapper_armed, 2) ||
	    !b04_uart_puts(" CSI_ATTEMPTED=") ||
	    !b04_uart_put_hex(info.csi_call_attempted, 2) ||
	    !b04_uart_puts(" CSI_RETURNED=") ||
	    !b04_uart_put_hex(info.csi_returned, 2) ||
	    !b04_uart_puts(" CSI_ACCEPTED=") ||
	    !b04_uart_put_hex(info.csi_result_accepted, 2) ||
#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
	    !b04_uart_puts(" HIGH_MINIT_AUTH=") ||
	    !b04_uart_put_hex(info.b06vb_high_qpi_minit_authorized, 2) ||
#endif
	    !b04_uart_puts(" POLICY=") ||
	    !b04_uart_put_hex(info.minit_policy_confirmed, 2) ||
	    !b04_uart_puts(" MINIT_ARMED=") ||
	    !b04_uart_put_hex(info.minit_armed, 2) ||
	    !b04_uart_puts(" MINIT_ATTEMPTED=") ||
	    !b04_uart_put_hex(info.minit_call_attempted, 2) ||
	    !b04_uart_puts(" MINIT_RETURNED=") ||
	    !b04_uart_put_hex(info.minit_returned, 2) ||
	    !b04_uart_puts(" SEED=") ||
	    !b04_uart_put_hex(info.experimental_status_seed, 2) ||
	    !b04_uart_puts(" CSI_FNV=") ||
	    !b04_uart_put_hex(info.csi_state_digest, 8) ||
	    !b04_uart_puts(" POLICY_IN_FNV=") ||
	    !b04_uart_put_hex(info.minit_policy_digest, 8) ||
	    !b04_uart_puts(" POLICY_NOW_FNV=") ||
	    !b04_uart_put_hex(info.minit_policy_current_digest, 8) ||
	    !b04_uart_puts(" WORK_FNV=") ||
	    !b04_uart_put_hex(info.minit_workspace_digest, 8) ||
	    !b04_uart_puts(" ROMMON_DIRTY=") ||
	    !b04_uart_put_hex(state->vendor_external_write, 2) ||
	    !b04_uart_puts("\r\n"))
		return false;

	if (info.csi_call_attempted || info.minit_call_attempted)
		return b06v0_print_call_result("LAST", &info.last_call);

	return true;
}

static bool b06v0_dump_buffer(const char *name, const uint8_t *buffer,
			      unsigned int size, unsigned int start,
			      unsigned int count)
{
	unsigned int offset;

	if (buffer == NULL || start >= size || count == 0 ||
	    count > B06V0_VENDOR_DUMP_MAX || count > size - start)
		return b06j_error("vendor dump range");

	for (offset = 0; offset < count; offset++) {
		if ((offset & 0xf) == 0 &&
		    (!b04_uart_puts("[VENDOR] ") || !b04_uart_puts(name) ||
		     !b04_uart_putc(' ') ||
		     !b04_uart_put_hex(start + offset, 4) ||
		     !b04_uart_putc(':')))
			return false;
		if (!b04_uart_put_hex(buffer[start + offset], 2))
			return false;
		if ((offset & 0xf) == 0xf || offset + 1 == count) {
			if (!b04_uart_puts("\r\n"))
				return false;
		}
	}

	return true;
}

static bool b06v0_policy_reset(struct b06j_state *state)
{
	struct x58_vendor_runtime_info info;
	enum x58_vendor_status status;
	uint32_t policy_flags;
	uint8_t policy[X58_VENDOR_MINIT_POLICY_SIZE];
	uint8_t combined;
	uint8_t csi_result;
#if CONFIG_X58_PRO_E_B06V4_POLICY_TELEMETRY
	struct b06v4_policy_inputs inputs;
	bool diagnostic_valid;
	uint8_t effective_8e;
#else
	uint8_t cmos_8e;
	uint8_t cmos_ca;
#endif
	unsigned int offset;

	state->vendor_policy_ready = false;
	state->vendor_policy_modified = false;
	status = x58_vendor_invalidate_minit_policy();
	if (status != X58_VENDOR_OK)
		return b06v0_print_status("POLICY_INVALIDATE", status);

	status = x58_vendor_runtime_probe(&info);
	if (status != X58_VENDOR_OK) {
		if (!b06v0_print_status("POLICY_PROBE", status))
			return false;
		return true;
	}
	if (!info.csi_returned || !info.csi_result_accepted)
		return b06j_error("vpolicy reset requires accepted CSI result");
	if (info.last_call.eax > UINT8_MAX)
		return b06j_error("CSI EAX does not fit policy status byte");

	for (offset = 0; offset < sizeof(policy); offset++)
		policy[offset] = 0;

	csi_result = info.last_call.eax;
	if (csi_result == 3)
		csi_result = 4;
	combined = info.experimental_status_seed | csi_result;
#if CONFIG_X58_PRO_E_B06V4_POLICY_TELEMETRY
	b06v4_capture_policy_inputs(&inputs);
	if (!b06v4_print_policy_inputs(&inputs))
		return false;
	diagnostic_valid =
		!(inputs.diagnostic_0e & B06V4_CMOS_DIAGNOSTIC_ERROR_MASK);
	effective_8e = inputs.extended_8e > 0x30 ? 0x30 : inputs.extended_8e;
	if (diagnostic_valid && ((inputs.alt_gp_smi_en_low >> 4) & 7) != 1)
		return b06j_error(
			"valid-CMOS policy is reconstructed only for ALT_GP_SMI_EN[6:4]=1");
#else
	cmos_8e = b06v0_cmos_direct(0x8e);
	cmos_ca = b06v0_cmos_direct(0xca);
	if (cmos_8e & 0xc0)
		return b06j_error("RTC/CMOS status invalid for policy decode");
	if (((inb(0x538) >> 4) & 7) != 1)
		return b06j_error("policy hypothesis requires strap selector 1");
#endif

	policy[0x00] = 0xff;
#if CONFIG_X58_PRO_E_B06V4_POLICY_TELEMETRY
	policy[0x01] = diagnostic_valid ? inputs.extended_81 & 3 : 0;
	policy[0x02] = -effective_8e & 0x3f;
#else
	policy[0x01] = b06v0_cmos_direct(0x81) & 3;
	policy[0x02] = cmos_8e > 0x30 ? 0x10 : (-cmos_8e & 0x3f);
#endif
	policy[0x03] = 0x01;
#if CONFIG_X58_PRO_E_B06V4_POLICY_TELEMETRY
	/* The original wrapper leaves this zero when CMOS diagnostic 0x0e is bad. */
	policy[0x04] = diagnostic_valid ? 0x01 : 0x00;
	policy[0x05] = diagnostic_valid && (inputs.extended_ca & 0xc0) ? 0x03 : 0x06;
#else
	/* Fields 0x04 and 0xbc are inferred from the field-1 reference capture. */
	policy[0x04] = 0x01;
	policy[0x05] = (cmos_ca & 0xc0) ? 0x03 : 0x06;
#endif
	policy[0x06] = 0x04;
	policy[0x07] = 0x02;
	policy[0x08] = 0x02;
	policy[0x0a] = combined;
	policy[0x0b] = 0x01;
	policy[0x0c] = 0x12;
	policy[0x0d] = 0x09;
	b06v0_policy_write16(policy, 0x10, 0x1388);
	b06v0_policy_write16(policy, 0x14, 0x1000);
	b06v0_policy_write32(policy, 0x20, 0xe0000000);
	policy_flags = 0x01024486 | (combined ? 0x00040000 : 0);
#if CONFIG_X58_PRO_E_B06V4_POLICY_TELEMETRY
	if (diagnostic_valid) {
		policy_flags &= ~0x6;
		policy_flags |= inputs.extended_82 & 0x6;
		if (!(inputs.extended_ca & 0xc0))
			policy_flags &= ~BIT(14);
	}
#else
	policy_flags &= ~0x6;
	policy_flags |= b06v0_cmos_direct(0x82) & 0x6;
	if (!(cmos_ca & 0xc0))
		policy_flags &= ~BIT(14);
#endif
	b06v0_policy_write32(policy, 0x24, policy_flags);
	policy[0x28] = 0x01;
	policy[0x29] = 0x06;
	policy[0x2a] = 0x46;
	policy[0x2b] = 0x14;
	b06v0_policy_write16(policy, 0x2c, 0x05dc);
	b06v0_policy_write16(policy, 0x30, 0x0190);
	policy[0x32] = 0x46;
	policy[0x33] = 0x14;
	b06v0_policy_write16(policy, 0x34, 0x05dc);
	b06v0_policy_write16(policy, 0x38, 0x0190);
	b06v0_policy_write32(policy, 0x46, 0);
	policy[0x4e] = 0x01;
	policy[0x50] = 0x40;
	b06v0_policy_write16(policy, 0x51, 0x0003);
	policy[0x53] = 0x5c;
	policy[0x55] = 0x01;
	policy[0x73] = 0x01;
	policy[0x91] = 0x01;
#if CONFIG_X58_PRO_E_B06V4_POLICY_TELEMETRY
	if (diagnostic_valid) {
		/* Still inferred from the sole captured field-1 vendor profile. */
		b06v0_policy_write16(policy, 0xbc, 0x0085);
		b06v0_policy_setup_fields(policy);
	}
#else
	b06v0_policy_write16(policy, 0xbc, 0x0085);
	b06v0_policy_setup_fields(policy);
#endif
	/* The original CSI wrapper's returned CMOS/NMI state is port 0x70 = 0x8e. */
	outb(0x8e, 0x70);
	policy[0xde] = 0x00;
	for (offset = 0; offset < sizeof(policy); offset++)
		state->vendor_policy[offset] = policy[offset];
	state->vendor_policy_ready = true;

	return b04_uart_puts("[VENDOR] policy template=") &&
#if CONFIG_X58_PRO_E_B06V4_POLICY_TELEMETRY
		b04_uart_puts(diagnostic_valid ?
			"VALID_CMOS_FIELD1_INFERRED" : "INVALID_CMOS_MSI_DEFAULT") &&
		b04_uart_puts(" EXT8E_CLAMPED_IN_POLICY=") &&
		b04_uart_put_hex(inputs.extended_8e > 0x30, 2) &&
		b04_uart_puts(" SEED_OR_CSI=") &&
#else
		b04_uart_puts(
			"INFERRED MSI caller; current CMOS decoded; fields04/bc reference-only SEED_OR_CSI=") &&
#endif
		b04_uart_put_hex(combined, 2) && b04_uart_puts(" FNV1A=") &&
		b04_uart_put_hex(b06v0_policy_digest(state->vendor_policy), 8) &&
		b04_uart_puts(
			"; inspect/edit before install; CMOS70=8e NMI_DISABLED\r\n");
}

#if CONFIG_X58_PRO_E_B06V5_COLD_MINIT
#define B06V5_POLICY_STATUS_OFFSET	0x0a
#define B06V5_POLICY_FLAGS_OFFSET	0x24
#define B06V5_POLICY_B3_SKIP		BIT(18)
#define B06V5_POLICY_B06V4_CANDIDATE_FLAGS	0x01060480
#define B06V5_POLICY_B06V4_CANDIDATE_FNV	0x2e0ccce9
#define B06V5_POLICY_COLD_FNV		0x3c0f3a0b
#define B06V5_WORK_B3_FLAGS_OFFSET	0x4f
#define B06V5_WORK_COMPLETE_OFFSET	0xe79
#define B06V5_MC_COMMON_DEV		PCI_DEV(0xff, 3, 0)
#define B06V5_CHANNEL2_ADDR_DEV		PCI_DEV(0xff, 6, 1)
#define B06V5_MC_CHANNEL_MAPPER		0x60
#define B06V5_MC_DOD_DIMM0		0x48

static bool b06v5_policy_force_cold(struct b06j_state *state)
{
	struct x58_vendor_runtime_info info;
	enum x58_vendor_status status;
	uint32_t digest;
	uint32_t flags;

	if (!state->vendor_policy_ready || !state->vendor_policy_modified)
		return b06j_error(
			"vpolicy cold requires the reviewed B06V4 candidate policy");
	status = x58_vendor_runtime_probe(&info);
	if (status != X58_VENDOR_OK)
		return b06v0_print_status("COLD_POLICY_PROBE", status);
	if (!info.csi_returned || !info.csi_result_accepted ||
	    info.last_call.eax != 2 || info.last_call.ebx != 2 ||
	    info.last_call.ecx != 0x106)
		return b06j_error("cold override requires accepted CSI tuple 2/2/106");

	digest = b06v0_policy_digest(state->vendor_policy);
	flags = b06v0_policy_read32(state->vendor_policy,
		B06V5_POLICY_FLAGS_OFFSET);
	if (digest != B06V5_POLICY_B06V4_CANDIDATE_FNV ||
	    state->vendor_policy[B06V5_POLICY_STATUS_OFFSET] != 2 ||
	    flags != B06V5_POLICY_B06V4_CANDIDATE_FLAGS)
		return b06j_error(
			"policy must exactly match B06V4 returning candidate FNV 2e0ccce9");

	status = x58_vendor_invalidate_minit_policy();
	if (!b06v0_print_status("POLICY_INVALIDATE", status))
		return false;
	if (status != X58_VENDOR_OK)
		return true;

	state->vendor_policy[B06V5_POLICY_STATUS_OFFSET] = 0;
	flags &= ~B06V5_POLICY_B3_SKIP;
	b06v0_policy_write32(state->vendor_policy, B06V5_POLICY_FLAGS_OFFSET,
		flags);
	if (b06v0_policy_digest(state->vendor_policy) != B06V5_POLICY_COLD_FNV)
		return b06j_error("cold policy digest invariant failed");
	state->vendor_policy_modified = true;

	return b04_uart_puts(
		"[VENDOR] POLICY override=COLD_FORCE_UNVERIFIED STATUS=00 FLAGS=") &&
		b04_uart_put_hex(flags, 8) && b04_uart_puts(" FNV1A=") &&
		b04_uart_put_hex(b06v0_policy_digest(state->vendor_policy), 8) &&
		b04_uart_puts(
			"; B3 skip cleared; inspect/dump/install separately; no call made\r\n");
}
#endif

static bool b06v0_prepare_pciexbar(struct b06j_state *state)
{
	const uint32_t sad_id = pci_io_read_config32(B06V0_SAD_DEV, 0);
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
	const uint32_t qpi_state = b06j_pci_read(B06L_QPI_LINK0_PHY_DEV,
		B06L_QPI_PH_PIS, 4);
#endif
	uint32_t low = pci_io_read_config32(B06V0_SAD_DEV,
		B06V0_SAD_PCIEXBAR_LO);
	const uint32_t high = pci_io_read_config32(B06V0_SAD_DEV,
		B06V0_SAD_PCIEXBAR_HI);
	uint32_t x58_id;
	uint32_t class_revision;
	struct x58_vendor_runtime_info info;
	enum x58_vendor_status status;

	state->vendor_pciexbar_ready = false;
	if (!state->vendor_spd_valid)
		return b06j_error("run spd and pass exact B06V0 DIMM gate first");
	status = x58_vendor_preflight_pciexbar();
	if (!b06v0_print_status("PCIEXBAR_PREFLIGHT", status))
		return false;
	if (status != X58_VENDOR_OK)
		return true;
	if (sad_id != B06V0_SAD_ID || high != 0 ||
	    (low != 0 && low != B06V0_PCIEXBAR_VALUE))
		return b06j_error("SAD/PCIEXBAR pre-state rejected");
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
	if ((qpi_state != B06L_QPI_SLOW_PH_PIS &&
	     qpi_state != 0x070f0f03) ||
#else
	if (b06j_pci_read(B06L_QPI_LINK0_PHY_DEV,
		B06L_QPI_PH_PIS, 4) != B06L_QPI_SLOW_PH_PIS ||
#endif
	    b06j_pci_read(B06K_UNCORE_COMMON_DEV, 0x50, 4) != 0x0a000006 ||
	    b06j_pci_read(B06K_UNCORE_COMMON_DEV, 0x54, 4) != 0x00000006)
		return b06j_error("Slow-QPI/ratio-6 pre-state rejected");

	if (!b04_uart_puts("[VENDOR] PCIEXBAR PRE ID=") ||
	    !b04_uart_put_hex(sad_id, 8) || !b04_uart_puts(" HI=") ||
	    !b04_uart_put_hex(high, 8) || !b04_uart_puts(" LO=") ||
	    !b04_uart_put_hex(low, 8) || !b04_uart_puts("\r\n"))
		return false;

	if (low == 0) {
		pci_io_write_config32(B06V0_SAD_DEV, B06V0_SAD_PCIEXBAR_HI, 0);
		pci_io_write_config32(B06V0_SAD_DEV, B06V0_SAD_PCIEXBAR_LO,
			B06V0_PCIEXBAR_VALUE);
	}
	low = pci_io_read_config32(B06V0_SAD_DEV, B06V0_SAD_PCIEXBAR_LO);
	if (low != B06V0_PCIEXBAR_VALUE ||
	    pci_io_read_config32(B06V0_SAD_DEV, B06V0_SAD_PCIEXBAR_HI) != 0)
		return b06j_error("PCIEXBAR readback rejected");

	x58_id = b06j_mem_read(0xe0000000, 4);
	class_revision = b06j_mem_read(0xe0000008, 4);
	if (x58_id != B06V0_IOH_MMCONFIG_ID ||
	    (class_revision & 0xffffff00) != 0x06000000)
		return b06j_error("X58 MMCONFIG identity rejected");

	status = x58_vendor_runtime_probe(&info);
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
	if (status == X58_VENDOR_OK && qpi_state == 0x070f0f03 &&
	    !info.b06v8_high_qpi_csi_profile)
		status = X58_VENDOR_ERR_PLATFORM_STATE;
#endif
	state->vendor_pciexbar_ready = status == X58_VENDOR_OK;
	if (!b04_uart_puts("[VENDOR] PCIEXBAR POST LO=") ||
	    !b04_uart_put_hex(low, 8) || !b04_uart_puts(" X58_ID=") ||
	    !b04_uart_put_hex(x58_id, 8) || !b04_uart_puts(" CLASSREV=") ||
	    !b04_uart_put_hex(class_revision, 8) || !b04_uart_puts("\r\n") ||
	    !b06v0_print_status("PCIEXBAR", status))
		return false;
	if (status != X58_VENDOR_OK)
		return true;

	outb(POST_B06V0_PCIEXBAR_READY, CONFIG_POST_IO_PORT);
	return true;
}
#endif

static bool b06v_report_reset_registers(void)
{
	const uint32_t pmbase_reg =
		pci_io_read_config32(ICH10R_LPC_DEV, D31F0_PMBASE);
	const uint8_t acpi_cntl =
		pci_io_read_config8(ICH10R_LPC_DEV, D31F0_ACPI_CNTL);
	const uint16_t pmbase = pmbase_reg & B04_PMBASE_MASK;
	const uint16_t gen_pmcon_1 =
		pci_io_read_config16(ICH10R_LPC_DEV, D31F0_GEN_PMCON_1);
	const uint16_t gen_pmcon_2 =
		pci_io_read_config16(ICH10R_LPC_DEV, D31F0_GEN_PMCON_2);
	const uint16_t gen_pmcon_3 =
		pci_io_read_config16(ICH10R_LPC_DEV, D31F0_GEN_PMCON_3);
	const uint32_t etr3 =
		pci_io_read_config32(ICH10R_LPC_DEV, D31F0_ETR3);
	const uint8_t rst_cnt = inb(B06V_RST_CNT_PORT);
	uint16_t pm1_sts = 0;
	uint32_t pm1_cnt = 0;
	uint32_t smi_sts = 0;
	uint32_t gpe0_sts_lo = 0;
	uint32_t gpe0_sts_hi = 0;
	const bool pm_io_available = (acpi_cntl & 0x80) && pmbase;

	/* Snapshot first so UART output cannot spread the reads over many events. */
	if (pm_io_available) {
		pm1_sts = inw(pmbase + PM1_STS);
		pm1_cnt = inl(pmbase + PM1_CNT);
		smi_sts = inl(pmbase + SMI_STS);
		gpe0_sts_lo = inl(pmbase + GPE0_STS);
		gpe0_sts_hi = inl(pmbase + GPE0_STS + 4);
	}

	/*
	 * Report exact register names and raw values only.  Several fields are
	 * sticky or write-one-to-clear, so this command deliberately decodes and
	 * clears nothing.
	 */
	if (!b04_uart_puts("[RESET] RAW D31F0 PMBASE=") ||
	    !b04_uart_put_hex(pmbase_reg, 8) ||
	    !b04_uart_puts(" ACPI_CNTL=") ||
	    !b04_uart_put_hex(acpi_cntl, 2) ||
	    !b04_uart_puts(" GEN_PMCON_1=") ||
	    !b04_uart_put_hex(gen_pmcon_1, 4) ||
	    !b04_uart_puts(" GEN_PMCON_2=") ||
	    !b04_uart_put_hex(gen_pmcon_2, 4) ||
	    !b04_uart_puts(" GEN_PMCON_3=") ||
	    !b04_uart_put_hex(gen_pmcon_3, 4) ||
	    !b04_uart_puts(" ETR3=") ||
	    !b04_uart_put_hex(etr3, 8) ||
	    !b04_uart_puts(" RST_CNT=") ||
	    !b04_uart_put_hex(rst_cnt, 2) ||
	    !b04_uart_puts("\r\n"))
		return false;

	if (!pm_io_available)
		return b04_uart_puts(
			"[RESET] RAW PM I/O unavailable: ACPI decode disabled or base zero\r\n"
			"[RESET] no cause decoded; no status bits cleared\r\n");

	return b04_uart_puts("[RESET] RAW PMIO BASE=") &&
		b04_uart_put_hex(pmbase, 4) &&
		b04_uart_puts(" PM1_STS=") &&
		b04_uart_put_hex(pm1_sts, 4) &&
		b04_uart_puts(" PM1_CNT=") &&
		b04_uart_put_hex(pm1_cnt, 8) &&
		b04_uart_puts(" SMI_STS=") &&
		b04_uart_put_hex(smi_sts, 8) &&
		b04_uart_puts(" GPE0_STS[31:0]=") &&
		b04_uart_put_hex(gpe0_sts_lo, 8) &&
		b04_uart_puts(" GPE0_STS[63:32]=") &&
		b04_uart_put_hex(gpe0_sts_hi, 8) &&
		b04_uart_puts(
			"\r\n[RESET] no cause decoded; no status bits cleared\r\n");
}

static void __noreturn b06v_cf9_reset(const char *message, uint8_t post,
				      uint8_t first, uint8_t second)
{
	/*
	 * CAR is the only writable memory at this point.  In particular, do not
	 * call the generic reset path: it cleans the cache before touching CF9.
	 */
	(void)b04_uart_puts(message);
	(void)b04_uart_wait_for(UART8250_LSR_TEMT,
				 B03_UART_FLUSH_POLL_LIMIT);
	outb(post, CONFIG_POST_IO_PORT);
	asm volatile ("cli" ::: "memory");
	outb(first, B06V_RST_CNT_PORT);
	outb(second, B06V_RST_CNT_PORT);

	/* Retain the requested reset's POST code if the chipset does not act. */
	for (;;)
		asm volatile ("hlt");
}

static bool b06j_si_getc(uint8_t *value)
{
	for (;;) {
		const enum b06j_rx_result rx = b06j_getc(value);

		if (rx == B06J_RX_IDLE)
			continue;
		if (rx == B06J_RX_FAULT)
			return false;
		return b04_uart_putc(*value);
	}
}

static bool b06j_si_hex(unsigned int digits, uint32_t *value)
{
	uint32_t parsed = 0;

	while (digits--) {
		uint8_t byte;
		uint8_t digit;

		if (!b06j_si_getc(&byte))
			return false;
		if (byte >= '0' && byte <= '9')
			digit = byte - '0';
		else if (byte >= 'a' && byte <= 'f')
			digit = byte - 'a' + 10;
		else if (byte >= 'A' && byte <= 'F')
			digit = byte - 'A' + 10;
		else
			return false;
		parsed = (parsed << 4) | digit;
	}
	*value = parsed;

	return true;
}

static bool b06j_si_expect(uint8_t expected)
{
	uint8_t value;

	return b06j_si_getc(&value) && value == expected;
}

static bool b06j_si_width(uint8_t width, unsigned int *bytes)
{
	if (width == 'b')
		*bytes = 1;
	else if (width == 'w')
		*bytes = 2;
	else if (width == 'l')
		*bytes = 4;
	else
		return false;

	return true;
}

static bool b06j_si_command(void)
{
	uint8_t first;
	uint8_t second;
	uint8_t width_byte;
	uint32_t address;
	uint32_t value;
	uint32_t key;
	unsigned int width;

	if (!b06j_si_getc(&first) || !b06j_si_getc(&second))
		return false;

	if (first == 'm' && second == 'b')
		return b04_uart_puts("\r\nMSI X58 Pro-E (MS-7522)         ");
	if (first == 'v' && second == 'i')
		return b04_uart_puts("\nSerialICE v1.5 B06J-X58 (2026-08-31)\n");
	if (first == 'c' && second == 'i') {
		struct cpuid_result result;

		if (!b06j_si_hex(8, &address) || !b06j_si_expect('.') ||
		    !b06j_si_hex(8, &value))
			return false;
		result = cpuid_ext(address, value);
		return b04_uart_puts("\r\n") &&
			b04_uart_put_hex(result.eax, 8) && b04_uart_putc('.') &&
			b04_uart_put_hex(result.ebx, 8) && b04_uart_putc('.') &&
			b04_uart_put_hex(result.ecx, 8) && b04_uart_putc('.') &&
			b04_uart_put_hex(result.edx, 8);
	}
	if ((first == 'r' || first == 'w') && second == 'm') {
		if (!b06j_si_hex(8, &address) || !b06j_si_expect('.') ||
		    !b06j_si_getc(&width_byte) ||
		    !b06j_si_width(width_byte, &width))
			return false;
		if (first == 'r')
			return b04_uart_puts("\r\n") &&
				b04_uart_put_hex(b06j_mem_read(address, width),
						 width * 2);
		if (!b06j_si_expect('=') || !b06j_si_hex(width * 2, &value))
			return false;
		b06j_mem_write(address, value, width);
		return true;
	}
	if ((first == 'r' || first == 'w') && second == 'i') {
		if (!b06j_si_hex(4, &address) || !b06j_si_expect('.') ||
		    !b06j_si_getc(&width_byte) ||
		    !b06j_si_width(width_byte, &width))
			return false;
		if (first == 'r')
			return b04_uart_puts("\r\n") &&
				b04_uart_put_hex(b06j_io_read(address, width),
						 width * 2);
		if (!b06j_si_expect('=') || !b06j_si_hex(width * 2, &value))
			return false;
		b06j_io_write(address, value, width);
		return true;
	}
	if ((first == 'r' || first == 'w') && second == 'c') {
		msr_t msr;

		if (!b06j_si_hex(8, &address) || !b06j_si_expect('.') ||
		    !b06j_si_hex(8, &key))
			return false;
		/* SerialICE carries its historical EDI key; RDMSR/WRMSR do not use it. */
		(void)key;
		if (first == 'r') {
			msr = rdmsr(address);
			return b04_uart_puts("\r\n") &&
				b04_uart_put_hex(msr.hi, 8) && b04_uart_putc('.') &&
				b04_uart_put_hex(msr.lo, 8);
		}
		if (!b06j_si_expect('=') || !b06j_si_hex(8, &msr.hi) ||
		    !b06j_si_expect('.') || !b06j_si_hex(8, &msr.lo))
			return false;
		wrmsr(address, msr);
		return true;
	}

	return false;
}

static void __noreturn b06j_serialice(bool command_started)
{
	outb(POST_B06J_SERIALICE, CONFIG_POST_IO_PORT);
	if (!command_started &&
	    !b04_uart_puts("\nSerialICE v1.5 B06J-X58 (2026-08-31)\n"))
		stop_with_post(POST_B06J_RX_ERROR);

	for (;;) {
		uint8_t value;

		if (!command_started) {
			if (!b04_uart_puts("\r\n> ") || !b06j_si_getc(&value))
				stop_with_post(POST_B06J_RX_ERROR);
			if (value != '*')
				continue;
		}
		if (!b06j_si_command()) {
			outb(POST_B06J_COMMAND_ERROR, CONFIG_POST_IO_PORT);
			if (!b04_uart_puts("ERROR\r\n"))
				stop_with_post(POST_B06J_RX_ERROR);
		}
		command_started = false;
	}
}

static bool b06j_help(void)
{
	return b04_uart_puts(
		"Read:  cpuid LEAF [SUB] | msr INDEX | io PORT [b|w|l]\r\n"
		"       pci BUS DEV FN REG [b|w|l] | mem ADDR [b|w|l]\r\n"
		"       dump ADDR COUNT | phyr CH START WIDTH | spd | resetcause\r\n"
		"       id | help\r\n"
		"Write: unlock WRITE arms exactly one of:\r\n"
		"       msrw INDEX HI LO | iow PORT VALUE [b|w|l]\r\n"
		"       pciw BUS DEV FN REG VALUE [b|w|l]\r\n"
		"       memw ADDR VALUE [b|w|l] | phyw CH START WIDTH VALUE MODE\r\n"
#if CONFIG_X58_PRO_E_B06M_BASEINIT
		"       baseinit\r\n"
#endif
		"       rdtry SWEEP | rdexact SWEEP | rdsweep | rcvtry COARSE\r\n"
#if CONFIG_X58_PRO_E_ROMMON_IRQPROBE_PIT
		"       irqprobe pit  (destructive diagnostic; cold reset required)\r\n"
#endif
		"       post VALUE | halt\r\n"
#if CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
		"Script: script clear|status|seal|list [START COUNT]\r\n"
		"        script trace [START COUNT] | script add delay ITER\r\n"
		"        script add read SPACE TARGET WIDTH\r\n"
		"        script add write SPACE TARGET WIDTH VALUE rev|nr\r\n"
		"        script add mask SPACE TARGET WIDTH CLEAR SET rev|nr\r\n"
		"        script add poll SPACE TARGET WIDTH MASK EXPECT LIMIT\r\n"
		"        script add assert SPACE TARGET WIDTH MASK EXPECT\r\n"
		"Seal is mandatory. Armed: unlock SCRIPT; script run PROGRAM_FNV keep|auto\r\n"
		"       script rollback TXN_FNV | script discard TXN_FNV\r\n"
		"SPACE io|pci|mem uses b|w|l; msr uses q and HI:LO values.\r\n"
		"PCI TARGET packs BB DD FF RR as eight hex digits, e.g. ff020180.\r\n"
		"auto rolls back only on failure and rejects every nr mutation.\r\n"
		"Scripted mem writes overlapping CAR are rejected; rollback is best-effort.\r\n"
#endif
#if CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
		"Vendor read: vinfo | vstate OFF COUNT | vwork OFF COUNT\r\n"
#if CONFIG_X58_PRO_E_B06V4_POLICY_TELEMETRY
		"       vinputs (CMOS banks plus ALT_GP_SMI_EN; no CMOS data write)\r\n"
#endif
		"       vpolicy dump OFF COUNT\r\n"
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
#if CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
		"B06VE locks manual vendor mutation/calls; automatic handoff is exact-gated.\r\n"
#elif CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
		"B06VD locks manual vendor mutation/calls; automatic MINIT is exact-gated.\r\n"
#elif CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT
		"B06VC locks manual vendor mutation/calls; automatic MINIT is exact-gated.\r\n"
#elif CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
		"B06VB locks manual vendor mutation/calls; automatic MINIT is exact-gated.\r\n"
#elif CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET
		"B06VA locks every manual vendor mutation/call, including MINIT.\r\n"
#else
		"B06V9 locks every manual vendor mutation/call, including MINIT.\r\n"
#endif
#else
		"B06V8 locks every manual vendor mutation/call, including MINIT.\r\n"
#endif
#else
		"Vendor: unlock VENDOR arms exactly one of:\r\n"
		"       vprep SPD_FNV | vcsi SEED | vaccept EAX EBX ECX CSI_FNV\r\n"
		"       vpolicy reset | vpolicy set OFF BYTE"
#if CONFIG_X58_PRO_E_B06V5_COLD_MINIT
		" | vpolicy cold"
#endif
		"\r\n"
		"       vpolicy install POLICY_FNV | vminit\r\n"
		"SEED is explicit/unknown; CSI may reset or HLT without returning.\r\n"
		"MINIT success remains a candidate only; B06V0 never accesses DRAM.\r\n"
#endif
#endif
		"Reset: unlock RESET arms exactly one reset or auto-recovery command.\r\n"
		"       reset init|warm|full\r\n"
#if CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF
		"Auto recovery: unlock RESET; autoguard clear; then remove AC power.\r\n"
#endif
		"lock cancels every arm. All numbers and delay counts are hexadecimal.\r\n"
		"PHY: CH 0..2; MODE 0..1; RD/RCV profiles are CH2 rank0 only.\r\n"
		"rdexact/rdsweep require ranks=03, MR=0806:1528/0000, Slow QPI.\r\n"
		"Reset uses direct CF9 writes and does not flush CAR; full is not AC-off.\r\n"
		"serialice enters permanent v1.5; leading * command or @ QEMU handshake.\r\n"
		"WARNING: invalid MSR/MMIO/I/O/PCI access may hang or reset hardware.\r\n");
}

static bool b06j_run_spd(struct b06j_state *state)
{
	struct b06b_result result = { 0 };
	uint8_t terminal;

#if CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
	state->vendor_spd_valid = false;
	state->vendor_spd_digest = 0;
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
	state->vendor_spd_profile_digest = 0;
#endif
	state->vendor_pciexbar_ready = false;
	state->vendor_policy_ready = false;
	state->vendor_policy_modified = false;
#endif
	if (state->smbus_timed_out)
		return b06j_error("SMBus timeout latched; remove AC power");
	terminal = b06b_read_base_spd(&result);
	if (!terminal)
		terminal = POST_B06I_SUCCESS;
	if (terminal == POST_B06_TIMEOUT)
		state->smbus_timed_out = true;
	if (!b06c_report(&result, terminal))
		return false;
#if CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
	state->vendor_spd_topology_status = terminal;
	state->vendor_spd_ackmap = 0;
	state->vendor_spd_ddr3map = 0;
	if (terminal == POST_B06I_SUCCESS)
		state->vendor_spd_topology_status = b06v0_scan_spd_topology(
			&state->vendor_spd_ackmap, &state->vendor_spd_ddr3map);
	if (state->vendor_spd_topology_status == POST_B06_TIMEOUT)
		state->smbus_timed_out = true;
	state->vendor_spd_valid = b06v0_spd_matches_target(&result, terminal) &&
		state->vendor_spd_topology_status == 0 &&
		state->vendor_spd_ackmap == B06V0_TARGET_SPD_BITMAP &&
		state->vendor_spd_ddr3map == B06V0_TARGET_SPD_BITMAP;
	if (state->vendor_spd_valid) {
		state->vendor_spd_digest = b06v0_buffer_digest(result.spd,
			sizeof(result.spd));
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
		state->vendor_spd_profile_digest = x58_spd_profile_digest(result.spd,
			sizeof(result.spd));
#endif
	}
	if (!b04_uart_puts("[VENDOR] SPD_TOPOLOGY ACKMAP=") ||
	    !b04_uart_put_hex(state->vendor_spd_ackmap, 2) ||
	    !b04_uart_puts(" DDR3MAP=") ||
	    !b04_uart_put_hex(state->vendor_spd_ddr3map, 2) ||
	    !b04_uart_puts(" RESULT=") ||
	    !b04_uart_put_hex(state->vendor_spd_topology_status, 2) ||
	    !b04_uart_puts(" EXACT_ONE_54=") ||
	    !b04_uart_puts((state->vendor_spd_topology_status == 0 &&
		state->vendor_spd_ackmap == B06V0_TARGET_SPD_BITMAP &&
		state->vendor_spd_ddr3map == B06V0_TARGET_SPD_BITMAP) ?
		"PASS\r\n" : "FAIL\r\n") ||
	    !b04_uart_puts("[VENDOR] SPD54_TARGET_GATE=") ||
	    !b04_uart_puts(state->vendor_spd_valid ? "PASS" : "FAIL") ||
	    !b04_uart_puts(" FULL256_FNV1A=") ||
	    !b04_uart_put_hex(state->vendor_spd_digest, 8) ||
	    !b04_uart_puts(
		" expected=BLS4G3D1609DS1S00. hdr93 crcEC4F 4GiB 2Rx8 CL5\r\n"))
		return false;
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
	if (!b04_uart_puts("[SPD] PROFILE_FNV1A=") ||
	    !b04_uart_put_hex(state->vendor_spd_profile_digest, 8) ||
	    !b04_uart_puts(" SERIAL_122_125=IGNORED COMPAT_GATE=") ||
	    !b04_uart_puts((state->vendor_spd_valid &&
		x58_b06v6_spd_digest_is_exact(state->vendor_spd_digest,
			state->vendor_spd_profile_digest)) ? "PASS\r\n" : "FAIL\r\n"))
		return false;
#endif
#endif
	outb(terminal, CONFIG_POST_IO_PORT);

	return true;
}

#if CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF
static bool b06v6_spd_is_exact(const struct b06j_state *state)
{
	return state->vendor_spd_valid &&
		x58_b06v6_spd_digest_is_exact(state->vendor_spd_digest,
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
			state->vendor_spd_profile_digest);
#else
			0);
#endif
}

/*
 * Every value below is an exact observation from B06V4/B06V5 on the single
 * supported lab configuration.  They are gates, not generalized register
 * definitions or native X58 policy.
 */
#define B06V6_POLICY_BASE_FNV		0x36981039
#define B06V6_POLICY_COLD_FLAGS		0x01020480
#define B06V6_CMOS_INDEX_PORT		0x70
#define B06V6_CMOS_DATA_PORT		0x71
#define B06V6_CMOS_INDEX_READBACK_PORT	0x74
#define B06V6_CMOS_DIAGNOSTIC_SELECTOR	0x0e
#define B06V6_CMOS_DIAGNOSTIC_OBSERVED	0x6c
#define B06V6_CMOS_GUARD_IN_PROGRESS	0xec
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
#define B06V8_CMOS_COLD_AUTHORIZATION	0x2c
#define B06V8_CMOS_PHASE_FAILED		0xed
#endif
#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
#define B06VB_CSI_OBSERVATION_FNV	0x8b38506a
#define B06VB_POLICY_BASE_FNV		0xbc4268bf
#define B06VB_CSI_CONTEXT_DELTA		0x1b9
#if CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
#define B06VD_CSI_DYNAMIC_OFFSET	0x02a6
#define B06VD_CSI_RAW_08_FNV		0x03e3d24e
#define B06VD_CSI_RAW_0C_FNV		0x8b38506a
#define B06VD_CSI_CANONICAL_FNV		0x908dabb6
_Static_assert(B06VD_CSI_DYNAMIC_OFFSET < X58_VENDOR_CSI_STATE_SIZE,
	"B06VD CSI canonical offset is outside the state buffer");
#endif
#endif
struct b06v6_policy_byte {
	uint8_t offset;
	uint8_t value;
};

struct b06v6_i801_signature {
	uint8_t control;
	uint8_t command;
	uint8_t xmit_address;
	uint8_t data0;
	uint8_t data1;
};

/*
 * These values are only provenance tags in host registers while START is
 * clear.  The complemented multi-register tuples cannot be produced by this
 * board's ordinary SPD reads, unlike a one-byte HSTCMD marker.
 */
static const struct b06v6_i801_signature b06v6_first_pass_signature = {
	.control = I801_BYTE_DATA,
	.command = 0xa5,
	.xmit_address = 0x3c,
	.data0 = 0x5a,
	.data1 = 0xc3,
};

static const struct b06v6_i801_signature b06v6_in_progress_signature = {
	.control = I801_BYTE_DATA,
	.command = 0x5a,
	.xmit_address = 0xc3,
	.data0 = 0xa5,
	.data1 = 0x3c,
};

static const struct b06v6_i801_signature b06v6_clear_signature = {
	.control = I801_BYTE_DATA,
};

#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
/*
 * B06V8 deliberately does not reuse a B06V6/V7 signature.  Each command/
 * address and data pair is complemented, START stays clear, and the complete
 * five-byte tuple is checked.  Register retention is proven for CSI's first
 * reset; retention of these exact values across the outer IOH SYRE reset is
 * the hardware hypothesis tested by B06V8.
 */
static const struct b06v6_i801_signature b06v8_pass2_signature = {
	.control = I801_BYTE_DATA,
	.command = 0x31,
	.xmit_address = 0xce,
	.data0 = 0x68,
	.data1 = 0x97,
};

static const struct b06v6_i801_signature b06v8_consumed_signature = {
	.control = I801_BYTE_DATA,
	.command = 0x42,
	.xmit_address = 0xbd,
	.data0 = 0x7a,
	.data1 = 0x85,
};

static const struct b06v6_i801_signature b06v8_pass3_signature = {
	.control = I801_BYTE_DATA,
	.command = 0x53,
	.xmit_address = 0xac,
	.data0 = 0x6b,
	.data1 = 0x94,
};

#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
/*
 * Dedicated fail-closed phase tag committed immediately before POST d3.
 * Both payload pairs are complements and cannot alias an ordinary SPD read.
 */
static const struct b06v6_i801_signature b06vb_minit_in_progress_signature = {
	.control = I801_BYTE_DATA,
	.command = 0x64,
	.xmit_address = 0x9b,
	.data0 = 0x5c,
	.data1 = 0xa3,
};
#if CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
/* Exact tuple left by both controlled B06VG PRIMARY MINIT returns. */
static const struct b06v6_i801_signature b06vh_minit_return_signature = {
	.control = I801_BYTE_DATA,
	.command = 0x00,
	.xmit_address = 0x5d,
	.data0 = 0x01,
	.data1 = 0xa3,
};
#endif
#endif
#endif

/* Neutral offset names are intentional: their semantics remain unproven. */
static const struct b06v6_policy_byte b06v6_candidate_edits[] = {
	{ 0x04, 0x01 },
	{ 0x08, 0x01 },
	{ 0x24, 0x80 },
	{ 0x25, 0x04 },
	{ 0xbc, 0x85 },
	{ 0xd9, 0x0a },
	{ 0xdb, 0x05 },
};

#if CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
#define B06V7_CSI_DYNAMIC_OFFSET	0x02a6

struct b06v7_zero_range {
	uint16_t first;
	uint16_t last;
};

/*
 * These are byte ranges observed to vary between two successful Slow-QPI
 * MINIT returns.  Their meanings are not yet established.  Canonicalization
 * affects only the digest input; the vendor buffers themselves stay intact.
 */
static const struct b06v7_zero_range b06v7_workspace_dynamic_ranges[] = {
	{ 0x132c, 0x136d },
	{ 0x13be, 0x13fd },
	{ 0x1859, 0x185c },
	{ 0x18e7, 0x18e9 },
	{ 0x1aa4, 0x1aa7 },
	{ 0x23a6, 0x23a6 },
	{ 0x2411, 0x2434 },
	{ 0x24a1, 0x24c4 },
	{ 0x251f, 0x252e },
};

#if CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
/*
 * Separate High-QPI canonicalization for the two successful B06VE hardware
 * returns promoted by B06VF.  It preserves every Slow-QPI range above and
 * adds only the five newly observed variable bytes.  The raw workspace is
 * never modified.
 */
static const struct b06v7_zero_range b06vf_workspace_dynamic_ranges[] = {
	{ 0x132c, 0x136d },
	{ 0x13be, 0x13fd },
	{ 0x1859, 0x185c },
	{ 0x18e7, 0x18e9 },
	{ 0x1aa4, 0x1aa7 },
	{ 0x23a6, 0x23a6 },
	{ 0x2411, 0x2434 },
	{ 0x2459, 0x245c },
	{ 0x2461, 0x2461 },
	{ 0x24a1, 0x24c4 },
	{ 0x251f, 0x252e },
};
#endif

_Static_assert(B06V7_CSI_DYNAMIC_OFFSET < X58_VENDOR_CSI_STATE_SIZE,
	"B06V7 CSI canonical offset is outside the state buffer");
_Static_assert(0x252e < X58_VENDOR_MINIT_WORKSPACE_SIZE,
	"B06V7 workspace canonical range is outside the workspace");
#if CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
_Static_assert(0x2461 < X58_VENDOR_MINIT_WORKSPACE_SIZE,
	"B06VF workspace canonical range is outside the workspace");
#endif

static uint32_t b06v7_canonical_digest(const uint8_t *buffer, size_t size,
	const struct b06v7_zero_range *ranges, size_t range_count)
{
	uint32_t digest = 2166136261u;
	size_t range = 0;

	for (size_t offset = 0; offset < size; offset++) {
		uint8_t value = buffer[offset];

		while (range < range_count && offset > ranges[range].last)
			range++;
		if (range < range_count && offset >= ranges[range].first)
			value = 0;
		digest ^= value;
		digest *= 16777619u;
	}

	return digest;
}

static uint32_t b06v7_csi_canonical_digest(const uint8_t *csi_state)
{
	static const struct b06v7_zero_range dynamic_byte = {
		.first = B06V7_CSI_DYNAMIC_OFFSET,
		.last = B06V7_CSI_DYNAMIC_OFFSET,
	};

	return b06v7_canonical_digest(csi_state, X58_VENDOR_CSI_STATE_SIZE,
		&dynamic_byte, 1);
}

static uint32_t b06v7_workspace_canonical_digest(const uint8_t *workspace)
{
	return b06v7_canonical_digest(workspace,
		X58_VENDOR_MINIT_WORKSPACE_SIZE,
		b06v7_workspace_dynamic_ranges,
		ARRAY_SIZE(b06v7_workspace_dynamic_ranges));
}

#if CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
static uint32_t b06vf_workspace_canonical_digest(const uint8_t *workspace)
{
	return b06v7_canonical_digest(workspace,
		X58_VENDOR_MINIT_WORKSPACE_SIZE,
		b06vf_workspace_dynamic_ranges,
		ARRAY_SIZE(b06vf_workspace_dynamic_ranges));
}

static bool b06vf_workspace_raw_pattern_exact(const uint8_t *workspace,
	uint32_t raw_digest)
{
	if (workspace == NULL)
		return false;

	/* Couple each admitted raw FNV to its exact observed byte tuple. */
	if (raw_digest == X58_B06VF_EXPECTED_WORKSPACE_RAW_A_FNV)
		return workspace[0x2459] == 0x00 &&
			workspace[0x245a] == 0x00 &&
			workspace[0x245b] == 0x00 &&
			workspace[0x245c] == 0x00 &&
			workspace[0x2461] == 0xfe;
	if (raw_digest == X58_B06VF_EXPECTED_WORKSPACE_RAW_B_FNV)
		return workspace[0x2459] == 0xff &&
			workspace[0x245a] == 0xff &&
			workspace[0x245b] == 0xff &&
			workspace[0x245c] == 0xff &&
			workspace[0x2461] == 0xff;

	return false;
}

#if CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
static bool b06vh_workspace_pattern_exact(const uint8_t *workspace,
	uint32_t raw_digest, uint32_t canonical_digest)
{
	if (workspace == NULL ||
	    !x58_b06vh_workspace_digest_pair_exact(raw_digest,
		canonical_digest))
		return false;

	/* Preserve both already admitted B06VF exact raw/pattern pairs. */
	if (raw_digest == X58_B06VF_EXPECTED_WORKSPACE_RAW_A_FNV ||
	    raw_digest == X58_B06VF_EXPECTED_WORKSPACE_RAW_B_FNV)
		return b06vf_workspace_raw_pattern_exact(workspace, raw_digest);

	/* B06VG G3-04 PRIMARY capture P.  Neutral offsets are intentional. */
	if (raw_digest == X58_B06VH_EXPECTED_WORKSPACE_RAW_P_FNV)
		return workspace[0x18d1] == 0x45 &&
			workspace[0x18d2] == 0x18 &&
			workspace[0x18d3] == 0x00 &&
			workspace[0x23a2] == 0x07 &&
			workspace[0x2402] == 0x00 &&
			workspace[0x2459] == 0xff &&
			workspace[0x245a] == 0xff &&
			workspace[0x245b] == 0xff &&
			workspace[0x245c] == 0xff &&
			workspace[0x245d] == 0xff &&
			workspace[0x245e] == 0xff &&
			workspace[0x245f] == 0xff &&
			workspace[0x2460] == 0xff &&
			workspace[0x2461] == 0xff &&
			workspace[0x26b6] == 0x0e &&
			workspace[0x26b7] == 0x7a &&
			workspace[0x26b8] == 0x37 &&
			workspace[0x26b9] == 0x0e &&
			workspace[0x26ba] == 0x7a &&
			workspace[0x26bb] == 0x37 &&
			workspace[0x26c0] == 0x0c &&
			workspace[0x26c1] == 0x7a &&
			workspace[0x26c2] == 0x38 &&
			workspace[0x26c3] == 0x0c &&
			workspace[0x26c4] == 0x7a &&
			workspace[0x26c5] == 0x38;

	/* B06VG G3-05 PRIMARY capture N. */
	if (raw_digest == X58_B06VH_EXPECTED_WORKSPACE_RAW_N_FNV)
		return workspace[0x18d1] == 0x44 &&
			workspace[0x18d2] == 0x1a &&
			workspace[0x18d3] == 0x02 &&
			workspace[0x23a2] == 0x06 &&
			workspace[0x2402] == 0x40 &&
			workspace[0x2459] == 0x00 &&
			workspace[0x245a] == 0x00 &&
			workspace[0x245b] == 0x00 &&
			workspace[0x245c] == 0x00 &&
			workspace[0x245d] == 0x00 &&
			workspace[0x245e] == 0x00 &&
			workspace[0x245f] == 0x00 &&
			workspace[0x2460] == 0x00 &&
			workspace[0x2461] == 0xff &&
			workspace[0x26b6] == 0x0a &&
			workspace[0x26b7] == 0x78 &&
			workspace[0x26b8] == 0x38 &&
			workspace[0x26b9] == 0x0a &&
			workspace[0x26ba] == 0x78 &&
			workspace[0x26bb] == 0x38 &&
			workspace[0x26c0] == 0x0c &&
			workspace[0x26c1] == 0x7a &&
			workspace[0x26c2] == 0x38 &&
			workspace[0x26c3] == 0x0c &&
			workspace[0x26c4] == 0x7a &&
			workspace[0x26c5] == 0x38;

	return false;
}

#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
static bool b06vi_o1_workspace_pattern_exact(const uint8_t *workspace,
	uint32_t raw_digest, uint32_t canonical_digest)
{
	if (workspace == NULL ||
	    raw_digest != X58_B06VI_EXPECTED_WORKSPACE_RAW_O_FNV ||
	    canonical_digest != X58_B06VI_EXPECTED_WORKSPACE_CANONICAL_O_FNV)
		return false;

	/* B06VH-HW-G3-04 profile O.  Neutral offsets are intentional. */
	return workspace[0x18d1] == 0x44 &&
		workspace[0x18d2] == 0x18 &&
		workspace[0x18d3] == 0x00 &&
		workspace[0x23a2] == 0x06 &&
		workspace[0x2402] == 0x00 &&
		workspace[0x2459] == 0xff &&
		workspace[0x245a] == 0xff &&
		workspace[0x245b] == 0xff &&
		workspace[0x245c] == 0xff &&
		workspace[0x245d] == 0xff &&
		workspace[0x245e] == 0xff &&
		workspace[0x245f] == 0xff &&
		workspace[0x2460] == 0xff &&
		workspace[0x2461] == 0xff &&
		workspace[0x26b6] == 0x0c &&
		workspace[0x26b7] == 0x7a &&
		workspace[0x26b8] == 0x38 &&
		workspace[0x26b9] == 0x0c &&
		workspace[0x26ba] == 0x7a &&
		workspace[0x26bb] == 0x38 &&
		workspace[0x26c0] == 0x0c &&
		workspace[0x26c1] == 0x7a &&
		workspace[0x26c2] == 0x38 &&
		workspace[0x26c3] == 0x0c &&
		workspace[0x26c4] == 0x7a &&
		workspace[0x26c5] == 0x38;
}

#if CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
static bool b06vj_q_workspace_pattern_exact(const uint8_t *workspace,
	uint32_t raw_digest, uint32_t canonical_digest)
{
	if (workspace == NULL || canonical_digest !=
		X58_B06VJ_EXPECTED_WORKSPACE_CANONICAL_Q_FNV)
		return false;
#if !CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
	if (raw_digest != X58_B06VJ_EXPECTED_WORKSPACE_RAW_Q_FNV)
		return false;
#else
	/* B06VK preserves the raw digest as telemetry inside the v8 handoff. */
	(void)raw_digest;
#endif

	/* B06VI-HW-G3-02 profile Q/O2.  Neutral offsets are intentional. */
	return workspace[0x18d1] == 0x43 &&
		workspace[0x18d2] == 0x18 &&
		workspace[0x18d3] == 0x00 &&
		workspace[0x23a2] == 0x06 &&
		workspace[0x2402] == 0x00 &&
		workspace[0x2459] == 0x00 &&
		workspace[0x245a] == 0x00 &&
		workspace[0x245b] == 0x00 &&
		workspace[0x245c] == 0x00 &&
		workspace[0x245d] == 0xff &&
		workspace[0x245e] == 0xff &&
		workspace[0x245f] == 0xff &&
		workspace[0x2460] == 0xff &&
		workspace[0x2461] == 0xff &&
		workspace[0x26b6] == 0x0c &&
		workspace[0x26b7] == 0x78 &&
		workspace[0x26b8] == 0x37 &&
		workspace[0x26b9] == 0x0c &&
		workspace[0x26ba] == 0x78 &&
		workspace[0x26bb] == 0x37 &&
		workspace[0x26c0] == 0x0a &&
		workspace[0x26c1] == 0x7a &&
		workspace[0x26c2] == 0x39 &&
		workspace[0x26c3] == 0x0a &&
		workspace[0x26c4] == 0x7a &&
		workspace[0x26c5] == 0x39;
}
#endif

static bool b06vi_o_workspace_pattern_exact(const uint8_t *workspace,
	uint32_t raw_digest, uint32_t canonical_digest)
{
	if (b06vi_o1_workspace_pattern_exact(workspace, raw_digest,
		canonical_digest))
		return true;
#if CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
	return b06vj_q_workspace_pattern_exact(workspace, raw_digest,
		canonical_digest);
#else
	return false;
#endif
}

static bool b06vi_workspace_pattern_exact(const uint8_t *workspace,
	uint32_t raw_digest, uint32_t canonical_digest)
{
	return b06vh_workspace_pattern_exact(workspace, raw_digest,
		canonical_digest) ||
		b06vi_o_workspace_pattern_exact(workspace, raw_digest,
			canonical_digest);
}
#endif
#endif
#endif

static bool b06v7_csi_dynamic_byte_is_known(uint8_t value)
{
	return value == 0x08 || value == 0x0c;
}
#endif

enum b06v6_auto_result {
	B06V6_AUTO_READY,
	B06V6_AUTO_FALLBACK,
	B06V6_AUTO_UART_ERROR,
};

static void b06v6_capture_i801_signature(
	struct b06v6_i801_signature *signature)
{
	const uintptr_t base = CONFIG_FIXED_SMBUS_IO_BASE;

	/* HSTSTAT is deliberately not read here: it owns the SW semaphore. */
	signature->control = inb(base + I801_HSTCTL);
	signature->command = inb(base + I801_HSTCMD);
	signature->xmit_address = inb(base + I801_XMITADD);
	signature->data0 = inb(base + I801_HSTDAT0);
	signature->data1 = inb(base + I801_HSTDAT1);
}

static bool b06v6_i801_signature_matches(
	const struct b06v6_i801_signature *observed,
	const struct b06v6_i801_signature *expected)
{
	return observed->control == expected->control &&
		observed->command == expected->command &&
		observed->xmit_address == expected->xmit_address &&
		observed->data0 == expected->data0 &&
		observed->data1 == expected->data1;
}

static uint8_t b06v6_read_cmos_diagnostic(void)
{
	/* ICH10 exposes the write-only 0x70 index/NMI state at port 0x74. */
	const uint8_t saved_selector = inb(B06V6_CMOS_INDEX_READBACK_PORT);
	uint8_t value;

	/* Match the MSI wrapper: select standard 0x0e while NMI is disabled. */
	outb(B06V6_CMOS_DIAGNOSTIC_SELECTOR | BIT(7), B06V6_CMOS_INDEX_PORT);
	(void)inb(0x61);
	value = inb(B06V6_CMOS_DATA_PORT);
	outb(saved_selector, B06V6_CMOS_INDEX_PORT);

	return value;
}

static bool b06v6_write_cmos_diagnostic(uint8_t value)
{
	/* ICH10 exposes the write-only 0x70 index/NMI state at port 0x74. */
	const uint8_t saved_selector = inb(B06V6_CMOS_INDEX_READBACK_PORT);
	uint8_t readback;

	/*
	 * B06V6/V7 observed 0x6c; the separately selected B06V8 High-QPI trial
	 * uses the live opt-in value 0x2c.  The in-progress value 0xec has bits
	 * 7:6 set.  Restore the selector/NMI state after the verified write.
	 */
	outb(B06V6_CMOS_DIAGNOSTIC_SELECTOR | BIT(7), B06V6_CMOS_INDEX_PORT);
	(void)inb(0x61);
	outb(value, B06V6_CMOS_DATA_PORT);
	readback = inb(B06V6_CMOS_DATA_PORT);
	outb(saved_selector, B06V6_CMOS_INDEX_PORT);

	return readback == value;
}

static bool b06v6_set_i801_signature(
	const struct b06v6_i801_signature *signature)
{
	const uintptr_t base = CONFIG_FIXED_SMBUS_IO_BASE;
	const uint8_t status = inb(base + I801_HSTSTAT);
	struct b06v6_i801_signature readback;

	/* The first status read owns the semaphore only when INUSE was clear. */
	if (status & I801_HSTSTAT_INUSE)
		return false;
	if ((status & (I801_HSTSTAT_HOST_BUSY | I801_HSTSTAT_FLAGS)) ||
	    !b06b_control_idle(inb(base + I801_HSTCTL))) {
		b06b_release_host(0);
		return false;
	}

	/* These host registers have no bus side effect while START stays clear. */
	outb(signature->control, base + I801_HSTCTL);
	outb(signature->command, base + I801_HSTCMD);
	outb(signature->xmit_address, base + I801_XMITADD);
	outb(signature->data0, base + I801_HSTDAT0);
	outb(signature->data1, base + I801_HSTDAT1);
	b06v6_capture_i801_signature(&readback);
	if (!b06v6_i801_signature_matches(&readback, signature)) {
		b06b_release_host(0);
		return false;
	}
	b06b_release_host(0);
	b06v6_capture_i801_signature(&readback);

	return b06v6_i801_signature_matches(&readback, signature);
}

#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
bool x58_b06vi_rearm_o_guard_after_postmem(void)
{
	struct b06v6_i801_signature before = { 0 };
	struct b06v6_i801_signature after = { 0 };
	uint8_t cmos;
	bool exact;

	/* The deferred profiles retain MINIT's return tuple through all DRAM tests. */
	b06v6_capture_i801_signature(&before);
	cmos = b06v6_read_cmos_diagnostic();
	if (!b06v6_i801_signature_matches(&before,
		&b06vh_minit_return_signature) ||
	    cmos != B06V6_CMOS_GUARD_IN_PROGRESS)
		return false;
	if (!b06v6_set_i801_signature(&b06vb_minit_in_progress_signature))
		return false;
	b06v6_capture_i801_signature(&after);
	exact = b06v6_i801_signature_matches(&after,
		&b06vb_minit_in_progress_signature);
	cmos = b06v6_read_cmos_diagnostic();
	if (!b04_uart_puts(
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
		"[RAMINIT] B06VL BROAD POSTMEM_REARM I801_PRE=") ||
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
		"[RAMINIT] B06VK O-family POSTMEM_REARM I801_PRE=") ||
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
		"[RAMINIT] B06VJ O-family POSTMEM_REARM I801_PRE=") ||
#else
		"[RAMINIT] B06VI O POSTMEM_REARM I801_PRE=") ||
#endif
	    !b04_uart_put_hex(before.control, 2) || !b04_uart_putc(':') ||
	    !b04_uart_put_hex(before.command, 2) || !b04_uart_putc(':') ||
	    !b04_uart_put_hex(before.xmit_address, 2) || !b04_uart_putc(':') ||
	    !b04_uart_put_hex(before.data0, 2) || !b04_uart_putc(':') ||
	    !b04_uart_put_hex(before.data1, 2) ||
	    !b04_uart_puts(" I801_POST=") ||
	    !b04_uart_put_hex(after.control, 2) || !b04_uart_putc(':') ||
	    !b04_uart_put_hex(after.command, 2) || !b04_uart_putc(':') ||
	    !b04_uart_put_hex(after.xmit_address, 2) || !b04_uart_putc(':') ||
	    !b04_uart_put_hex(after.data0, 2) || !b04_uart_putc(':') ||
	    !b04_uart_put_hex(after.data1, 2) ||
	    !b04_uart_puts(" REARMED_EXACT=") ||
	    !b04_uart_put_hex(exact, 2) || !b04_uart_puts(" CMOS0E=") ||
	    !b04_uart_put_hex(cmos, 2) ||
	    !b04_uart_puts(" AFTER_FULL_POSTMEM=01\r\n"))
		return false;

	return exact && cmos == B06V6_CMOS_GUARD_IN_PROGRESS;
}
#endif

#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
bool x58_b06ve_finalize_persistent_guard(void)
{
	struct b06v6_i801_signature i801;

	/* Refuse to clear anything unless the exact MINIT guard is still armed. */
	b06v6_capture_i801_signature(&i801);
	if (!b06v6_i801_signature_matches(&i801,
		&b06vb_minit_in_progress_signature) ||
	    b06v6_read_cmos_diagnostic() != B06V6_CMOS_GUARD_IN_PROGRESS)
		return false;

	/* Clear I801 first, then restore the explicit High-QPI cold cookie. */
	if (!b06v6_set_i801_signature(&b06v6_clear_signature))
		return false;
	if (!b06v6_write_cmos_diagnostic(B06V8_CMOS_COLD_AUTHORIZATION))
		return false;

	/* Both setters already completed an immediate exact readback. */
	return true;
}
#endif

static void __noreturn b06v6_guard_stop(const char *reason)
{
	outb(POST_B06V6_GUARD_STOP, CONFIG_POST_IO_PORT);
	(void)b04_uart_puts("[RAMINIT] AUTO_GUARD_STOP=");
	(void)b04_uart_puts(reason);
	(void)b04_uart_puts(
		"; persistent guard retained; reboot to ROMMON, unlock RESET; "
		"autoguard clear; then remove AC power\r\n");
	(void)b04_uart_wait_for(UART8250_LSR_TEMT,
		B03_UART_FLUSH_POLL_LIMIT);
	stop_with_post(POST_B06V6_GUARD_STOP);
}

#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
static void __noreturn b06v8_phase_guard_stop(const char *reason)
{
	/*
	 * An I801 transition or the outer SYRE sequence can fail after the old
	 * phase still looks valid.  Poison the independent CMOS authorization so
	 * no warm reset can promote that stale signature to the next pass.
	 */
	const bool fail_lock_set = b06v6_write_cmos_diagnostic(
		B06V8_CMOS_PHASE_FAILED);

	outb(POST_B06V6_GUARD_STOP, CONFIG_POST_IO_PORT);
#if CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
	(void)b04_uart_puts("[QPI] B06VE_PHASE_GUARD_STOP=");
#elif CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
	(void)b04_uart_puts("[QPI] B06VD_PHASE_GUARD_STOP=");
#elif CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT
	(void)b04_uart_puts("[QPI] B06VC_PHASE_GUARD_STOP=");
#elif CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
	(void)b04_uart_puts("[QPI] B06VB_PHASE_GUARD_STOP=");
#elif CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET
	(void)b04_uart_puts("[QPI] B06VA_PHASE_GUARD_STOP=");
#else
	(void)b04_uart_puts("[QPI] B06V9_PHASE_GUARD_STOP=");
#endif
#else
	(void)b04_uart_puts("[QPI] B06V8_PHASE_GUARD_STOP=");
#endif
	(void)b04_uart_puts(reason);
	(void)b04_uart_puts(fail_lock_set ?
		"; CMOS0E=ed fail-lock set; " :
		"; CMOS fail-lock WRITE FAILED; ");
	(void)b04_uart_puts(
		"DO NOT WARM RESET; remove AC before recovery boot; clear autoguard in ROMMON; remove AC again before retry\r\n");
	(void)b04_uart_wait_for(UART8250_LSR_TEMT,
		B03_UART_FLUSH_POLL_LIMIT);
	stop_with_post(POST_B06V6_GUARD_STOP);
}
#endif

static enum b06v6_auto_result b06v6_fallback(const char *reason)
{
	outb(POST_B06V6_AUTO_FALLBACK, CONFIG_POST_IO_PORT);
	if (!b04_uart_puts("[RAMINIT] AUTO_FALLBACK=") ||
	    !b04_uart_puts(reason) ||
	    !b04_uart_puts(
		"; no further automatic vendor call this boot; entering ROMMON\r\n") ||
	    !b04_uart_wait_for(UART8250_LSR_TEMT,
		B03_UART_FLUSH_POLL_LIMIT))
		return B06V6_AUTO_UART_ERROR;

	return B06V6_AUTO_FALLBACK;
}

#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
#define B06V8_CPU_QPI_LINK_DEV		PCI_DEV(0xff, 2, 0)
#define B06V8_CPU_QPI_PHY_DEV		PCI_DEV(0xff, 2, 1)
#define B06V8_IOH_QPI_LINK_DEV		PCI_DEV(0, 0x10, 0)
#define B06V8_IOH_SR_DEV		PCI_DEV(0, 0x14, 1)
#define B06V8_IOH_SYRE_DEV		PCI_DEV(0, 0x14, 2)
#define B06V8_IOH_QPI0_ECAM		0xe0068000u
#define B06V8_IOH_SYRE_REQUEST		BIT(10)
#define B06V8_PH_PIS_HIGH_MASK		0x041f1f01u
#define B06V8_PH_PIS_HIGH_VALUE	0x040f0f01u
#if !CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
#define B06V8_EXT_CMOS_INDEX_PORT	0x72
#define B06V8_EXT_CMOS_DATA_PORT	0x73
#define B06V8_EXT80_EXPECTED		0xb3
#define B06V8_EXT81_EXPECTED		0x19
#define B06V8_EXT82_EXPECTED		0xd7
#define B06V8_EXT88_EXPECTED		0x67
#define B06V8_EXT89_EXPECTED		0xcb
#endif

enum b06v8_phase {
	B06V8_PHASE_PASS1 = 1,
	B06V8_PHASE_PASS2,
	B06V8_PHASE_PASS3,
};

struct b06v8_qpi_tuple {
	uint32_t cpu_50;
	uint32_t cpu_54;
	uint32_t cpu_6c;
	uint32_t cpu_80;
	uint32_t cpu_94;
	uint32_t cpu_9c;
	uint32_t cpu_a0;
	uint32_t cpu_a4;
	uint32_t cpu_link_50;
	uint32_t cpu_link_58;
	uint32_t ioh_82c;
	uint32_t ioh_840;
	uint32_t ioh_854;
	uint32_t ioh_85c;
	uint32_t ioh_864;
	uint32_t ioh_link_c8;
	uint32_t ioh_sr0_7c;
	uint32_t ioh_sr1_80;
	uint32_t ioh_syre_cc;
};

#if !CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
struct b06v8_cmos_inputs {
	uint8_t ext80;
	uint8_t ext81;
	uint8_t ext82;
	uint8_t ext88;
	uint8_t ext89;
};

static uint8_t b06v8_read_extended_cmos(uint8_t selector)
{
	const uint8_t saved_selector = inb(B06V8_EXT_CMOS_INDEX_PORT);
	uint8_t value;

	outb(selector, B06V8_EXT_CMOS_INDEX_PORT);
	value = inb(B06V8_EXT_CMOS_DATA_PORT);
	outb(saved_selector, B06V8_EXT_CMOS_INDEX_PORT);

	return value;
}

static void b06v8_capture_cmos_inputs(struct b06v8_cmos_inputs *inputs)
{
	inputs->ext80 = b06v8_read_extended_cmos(0x80);
	inputs->ext81 = b06v8_read_extended_cmos(0x81);
	inputs->ext82 = b06v8_read_extended_cmos(0x82);
	inputs->ext88 = b06v8_read_extended_cmos(0x88);
	inputs->ext89 = b06v8_read_extended_cmos(0x89);
}

static bool b06v8_cmos_inputs_exact(const struct b06v8_cmos_inputs *inputs)
{
	return inputs->ext80 == B06V8_EXT80_EXPECTED &&
		inputs->ext81 == B06V8_EXT81_EXPECTED &&
		inputs->ext82 == B06V8_EXT82_EXPECTED &&
		inputs->ext88 == B06V8_EXT88_EXPECTED &&
		inputs->ext89 == B06V8_EXT89_EXPECTED;
}

static bool b06v8_print_cmos_inputs(const struct b06v8_cmos_inputs *inputs)
{
	return b04_uart_puts("[QPI] READ_ONLY EXT80/81/82/88/89=") &&
		b04_uart_put_hex(inputs->ext80, 2) && b04_uart_putc('/') &&
		b04_uart_put_hex(inputs->ext81, 2) && b04_uart_putc('/') &&
		b04_uart_put_hex(inputs->ext82, 2) && b04_uart_putc('/') &&
		b04_uart_put_hex(inputs->ext88, 2) && b04_uart_putc('/') &&
		b04_uart_put_hex(inputs->ext89, 2) &&
		b04_uart_puts(" EXPECT=b3/19/d7/67/cb CMOS_DATA_WRITES=00\r\n");
}
#endif

static void b06v8_capture_qpi_tuple(struct b06v8_qpi_tuple *tuple)
{
	tuple->cpu_50 = b06j_pci_read(B06V8_CPU_QPI_PHY_DEV, 0x50, 4);
	tuple->cpu_54 = b06j_pci_read(B06V8_CPU_QPI_PHY_DEV, 0x54, 4);
	tuple->cpu_6c = b06j_pci_read(B06V8_CPU_QPI_PHY_DEV, 0x6c, 4);
	tuple->cpu_80 = b06j_pci_read(B06V8_CPU_QPI_PHY_DEV, 0x80, 4);
	tuple->cpu_94 = b06j_pci_read(B06V8_CPU_QPI_PHY_DEV, 0x94, 4);
	tuple->cpu_9c = b06j_pci_read(B06V8_CPU_QPI_PHY_DEV, 0x9c, 4);
	tuple->cpu_a0 = b06j_pci_read(B06V8_CPU_QPI_PHY_DEV, 0xa0, 4);
	tuple->cpu_a4 = b06j_pci_read(B06V8_CPU_QPI_PHY_DEV, 0xa4, 4);
	tuple->cpu_link_50 = b06j_pci_read(B06V8_CPU_QPI_LINK_DEV, 0x50, 4);
	tuple->cpu_link_58 = b06j_pci_read(B06V8_CPU_QPI_LINK_DEV, 0x58, 4);
	/* PCIEXBAR is exact-gated before these read-only extended accesses. */
	tuple->ioh_82c = b06j_mem_read(B06V8_IOH_QPI0_ECAM + 0x82c, 4);
	tuple->ioh_840 = b06j_mem_read(B06V8_IOH_QPI0_ECAM + 0x840, 4);
	tuple->ioh_854 = b06j_mem_read(B06V8_IOH_QPI0_ECAM + 0x854, 4);
	tuple->ioh_85c = b06j_mem_read(B06V8_IOH_QPI0_ECAM + 0x85c, 4);
	tuple->ioh_864 = b06j_mem_read(B06V8_IOH_QPI0_ECAM + 0x864, 4);
	tuple->ioh_link_c8 = b06j_pci_read(B06V8_IOH_QPI_LINK_DEV, 0xc8, 4);
	tuple->ioh_sr0_7c = b06j_pci_read(B06V8_IOH_SR_DEV, 0x7c, 4);
	tuple->ioh_sr1_80 = b06j_pci_read(B06V8_IOH_SR_DEV, 0x80, 4);
	tuple->ioh_syre_cc = b06j_pci_read(B06V8_IOH_SYRE_DEV, 0xcc, 4);
}

#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
static bool b06vg_capture_stable_pre_a0(uint32_t *saved_pre_a0)
{
	const uint32_t pre_a0_first =
		b06j_pci_read(B06V8_CPU_QPI_PHY_DEV, 0xa0, 4);
	const uint32_t pre_a0_second =
		b06j_pci_read(B06V8_CPU_QPI_PHY_DEV, 0xa0, 4);
	const bool exact = saved_pre_a0 != NULL &&
		pre_a0_first == pre_a0_second &&
		x58_vendor_b06vg_high_qpi_cpu_a0_exact(pre_a0_first);

	if (!b04_uart_puts("[QPI] B06VG PRE_A0 FIRST/SECOND=") ||
	    !b04_uart_put_hex(pre_a0_first, 8) || !b04_uart_putc('/') ||
	    !b04_uart_put_hex(pre_a0_second, 8) ||
	    !b04_uart_puts(
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
		" B06VL_EXACT_SET=00017000|00017200|00017400|00017600|00017800|00017a00|00017c00 STABLE=") ||
#else
		" EXACT_SET=00017000|00017400|00017600|00017800|00017a00|00017c00 STABLE=") ||
#endif
	    !b04_uart_put_hex(exact, 2) || !b04_uart_puts("\r\n"))
		return false;
	if (pre_a0_first != pre_a0_second || !exact)
		return false;

	*saved_pre_a0 = pre_a0_first;
	return true;
}
#endif

static bool b06v8_print_qpi_tuple(const char *label,
	const struct b06v8_qpi_tuple *tuple)
{
	return b04_uart_puts("[QPI] ") && b04_uart_puts(label) &&
		b04_uart_puts(" CPU ff:02.1 50/54/6c/80=") &&
		b04_uart_put_hex(tuple->cpu_50, 8) && b04_uart_putc('/') &&
		b04_uart_put_hex(tuple->cpu_54, 8) && b04_uart_putc('/') &&
		b04_uart_put_hex(tuple->cpu_6c, 8) && b04_uart_putc('/') &&
		b04_uart_put_hex(tuple->cpu_80, 8) &&
		b04_uart_puts(" 94/9c/a0/a4=") &&
		b04_uart_put_hex(tuple->cpu_94, 8) && b04_uart_putc('/') &&
		b04_uart_put_hex(tuple->cpu_9c, 8) && b04_uart_putc('/') &&
		b04_uart_put_hex(tuple->cpu_a0, 8) && b04_uart_putc('/') &&
		b04_uart_put_hex(tuple->cpu_a4, 8) && b04_uart_puts("\r\n") &&
		b04_uart_puts("[QPI] ") && b04_uart_puts(label) &&
		b04_uart_puts(" IOH 00:0d.0 82c/840/854/85c/864=") &&
		b04_uart_put_hex(tuple->ioh_82c, 8) && b04_uart_putc('/') &&
		b04_uart_put_hex(tuple->ioh_840, 8) && b04_uart_putc('/') &&
		b04_uart_put_hex(tuple->ioh_854, 8) && b04_uart_putc('/') &&
		b04_uart_put_hex(tuple->ioh_85c, 8) && b04_uart_putc('/') &&
		b04_uart_put_hex(tuple->ioh_864, 8) && b04_uart_puts("\r\n") &&
		b04_uart_puts("[QPI] ") && b04_uart_puts(label) &&
		b04_uart_puts(" LINK CPU50/58=") &&
		b04_uart_put_hex(tuple->cpu_link_50, 8) && b04_uart_putc('/') &&
		b04_uart_put_hex(tuple->cpu_link_58, 8) &&
		b04_uart_puts(" IOH_C8=") &&
		b04_uart_put_hex(tuple->ioh_link_c8, 8) &&
		b04_uart_puts(" SR0/SR1=") &&
		b04_uart_put_hex(tuple->ioh_sr0_7c, 8) && b04_uart_putc('/') &&
		b04_uart_put_hex(tuple->ioh_sr1_80, 8) &&
		b04_uart_puts(" SYRE_CC=") &&
		b04_uart_put_hex(tuple->ioh_syre_cc, 8) && b04_uart_puts("\r\n");
}

static bool b06v8_pre_outer_reset_tuple_exact(
	const struct b06v8_qpi_tuple *tuple)
{
	return tuple->cpu_50 == 0x160c0110 &&
		tuple->cpu_54 == 0x00000012 &&
		tuple->cpu_6c == 0x0040a0a0 &&
		tuple->cpu_80 == 0x030f0f03 &&
		tuple->cpu_94 == 0x00010202 &&
		tuple->cpu_9c == 0x00000502 &&
		tuple->cpu_a0 == 0x00000c00 &&
		tuple->cpu_a4 == 0x00322808 &&
		tuple->ioh_82c == 0x004060a0 &&
		tuple->ioh_840 == 0x030f0f00 &&
		tuple->ioh_854 == 0x00010102 &&
		tuple->ioh_85c == 0x00000002 &&
		tuple->ioh_864 == 0x00322808 &&
		tuple->ioh_sr0_7c == 0 && tuple->ioh_sr1_80 == 0 &&
		tuple->ioh_syre_cc == 0x00000600;
}

static bool b06v8_slow_qpi_entry_tuple_exact(
	const struct b06v8_qpi_tuple *tuple, enum b06v8_phase phase)
{
	/*
	 * Strict composite Slow-QPI hypothesis assembled from immutable B06V6
	 * observations.  The individual values are evidence-backed, but every
	 * field was not captured jointly at both AC-cold and pass-2 entry.  Fail
	 * closed on any mismatch.  SYRE is the only phase-dependent predicate.
	 */
	return (phase == B06V8_PHASE_PASS1 || phase == B06V8_PHASE_PASS2) &&
		tuple->cpu_50 == 0x160c0110 &&
		tuple->cpu_54 == 0x00000010 &&
		tuple->cpu_6c == 0x0000a020 &&
		tuple->cpu_80 == 0x030f0f03 &&
		tuple->cpu_94 == 0x00000102 &&
		tuple->cpu_9c == 0x00000502 &&
		tuple->cpu_a0 == 0x00000c00 &&
		tuple->cpu_a4 == 0x001d2c03 &&
		tuple->ioh_82c == 0x00006020 &&
		tuple->ioh_840 == 0x030f0f03 &&
		tuple->ioh_854 == 0x00000102 &&
		tuple->ioh_85c == 0x00000002 &&
		tuple->ioh_864 == 0x00322808 &&
		tuple->ioh_sr0_7c == 0 && tuple->ioh_sr1_80 == 0 &&
		tuple->ioh_syre_cc == (phase == B06V8_PHASE_PASS1 ?
			0x00000200 : 0x00000600);
}

static bool b06v8_high_qpi_tuple_exact(const struct b06v8_qpi_tuple *tuple)
{
	return tuple->cpu_50 == 0x160c0112 &&
		tuple->cpu_54 == 0x00000012 &&
		tuple->cpu_6c == 0x0040a0a0 &&
		tuple->cpu_80 == 0x070f0f03 &&
		tuple->cpu_94 == 0x00010202 &&
		tuple->cpu_9c == 0x00000502 &&
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
		x58_vendor_b06vg_high_qpi_cpu_a0_exact(tuple->cpu_a0) &&
#else
#if CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
		x58_vendor_b06vd_high_qpi_cpu_a0_exact(tuple->cpu_a0) &&
#else
#if CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT
		x58_vendor_b06vc_high_qpi_cpu_a0_exact(tuple->cpu_a0) &&
#else
#if CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET
		x58_vendor_high_qpi_cpu_a0_exact(tuple->cpu_a0) &&
#else
		tuple->cpu_a0 == 0x00017600 &&
#endif
#endif
#endif
#endif
		tuple->cpu_a4 == 0x00322808 &&
		(tuple->cpu_link_50 == 0x86000000 ||
		 tuple->cpu_link_50 == 0x96000000) &&
		tuple->cpu_link_58 == 0x00064555 &&
		tuple->ioh_82c == 0x004060a0 &&
		tuple->ioh_840 == 0x070f0f03 &&
		tuple->ioh_854 == 0x00010102 &&
		tuple->ioh_85c == 0x00000002 &&
		tuple->ioh_864 == 0x00322808 &&
		tuple->ioh_link_c8 == 0x0606fc00 &&
		tuple->ioh_sr0_7c == 0 && tuple->ioh_sr1_80 == 0 &&
		tuple->ioh_syre_cc == 0x00000600 &&
		(tuple->cpu_80 & B06V8_PH_PIS_HIGH_MASK) ==
			B06V8_PH_PIS_HIGH_VALUE &&
		(tuple->ioh_840 & B06V8_PH_PIS_HIGH_MASK) ==
			B06V8_PH_PIS_HIGH_VALUE;
}

#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
struct b06vb_stage_fingerprints {
	uint32_t cpu_context_80;
	uint32_t cpu_stage_d0;
	uint32_t ioh_stage_9c;
	uint32_t memory_clock_50;
	uint32_t memory_clock_54;
};

static void b06vb_capture_stage_fingerprints(
	struct b06vb_stage_fingerprints *stage)
{
	stage->cpu_context_80 = b06j_pci_read(B06V8_CPU_QPI_LINK_DEV, 0x80, 4);
	stage->cpu_stage_d0 = b06j_pci_read(B06V8_CPU_QPI_LINK_DEV, 0xd0, 4);
	stage->ioh_stage_9c = b06j_pci_read(B06V8_IOH_SR_DEV, 0x9c, 4);
	stage->memory_clock_50 = b06j_pci_read(B06K_UNCORE_COMMON_DEV, 0x50, 4);
	stage->memory_clock_54 = b06j_pci_read(B06K_UNCORE_COMMON_DEV, 0x54, 4);
}

static bool b06vb_print_stage_fingerprints(const char *label,
	const struct b06vb_stage_fingerprints *stage,
	const struct x58_vendor_call_result *csi_result)
{
	const uint32_t expected_context =
		(csi_result->edx + B06VB_CSI_CONTEXT_DELTA) & 0xffff;

	return b04_uart_puts("[QPI] ") && b04_uart_puts(label) &&
		b04_uart_puts(" STAGE_TELEMETRY CPU ff:02.0 80/d0=") &&
		b04_uart_put_hex(stage->cpu_context_80, 8) && b04_uart_putc('/') &&
		b04_uart_put_hex(stage->cpu_stage_d0, 8) &&
		b04_uart_puts(" PTR_LOW16_EXPECT=") &&
		b04_uart_put_hex(expected_context, 4) &&
		b04_uart_puts(" PTR_MATCH=") &&
		b04_uart_put_hex(stage->cpu_context_80 == expected_context, 2) &&
		b04_uart_puts(" IOH 00:14.1 9c=") &&
		b04_uart_put_hex(stage->ioh_stage_9c, 8) &&
		b04_uart_puts(" MC50/54=") &&
		b04_uart_put_hex(stage->memory_clock_50, 8) && b04_uart_putc('/') &&
		b04_uart_put_hex(stage->memory_clock_54, 8) &&
		b04_uart_puts("; CPU80/D0 READ_ONLY_NOT_GATES\r\n");
}

#if !CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
static bool b06vb_post_csi_tuple_exact(const struct b06v8_qpi_tuple *tuple,
	const struct b06vb_stage_fingerprints *stage)
{
	return tuple->cpu_50 == 0x160c0112 &&
		tuple->cpu_54 == 0x00000012 &&
		tuple->cpu_6c == 0x0040a0a8 &&
		tuple->cpu_80 == 0x070f0f03 &&
		tuple->cpu_94 == 0x00010202 &&
		tuple->cpu_9c == 0x00b00502 &&
		tuple->cpu_a0 == 0x00017000 &&
		tuple->cpu_a4 == 0x00322808 &&
		tuple->cpu_link_50 == 0x86000000 &&
		tuple->cpu_link_58 == 0x00064555 &&
		tuple->ioh_82c == 0x004060a0 &&
		tuple->ioh_840 == 0x070f0f03 &&
		tuple->ioh_854 == 0x00010102 &&
		tuple->ioh_85c == 0x00000002 &&
		tuple->ioh_864 == 0x00322808 &&
		tuple->ioh_link_c8 == 0x0616fc00 &&
		tuple->ioh_sr0_7c == 0 && tuple->ioh_sr1_80 == 0 &&
		tuple->ioh_syre_cc == 0x00000600 &&
		stage->ioh_stage_9c == 0xea000000 &&
		stage->memory_clock_50 == 0x0a000006 &&
		stage->memory_clock_54 == 0x00000006;
}
#endif

#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
#if !CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
static bool b06ve_post_minit_tuple_exact(
	const struct b06v8_qpi_tuple *tuple,
	const struct b06vb_stage_fingerprints *stage)
{
	/* CPU ff:02.0 +80/+d0 remain telemetry-only and are not gates. */
	return tuple->cpu_50 == 0x160c0112 &&
		tuple->cpu_54 == 0x00000012 &&
		tuple->cpu_6c == 0x0040a0a8 &&
		tuple->cpu_80 == X58_B06VE_EXPECTED_QPI_STATUS &&
		tuple->cpu_94 == 0x00010202 &&
		tuple->cpu_9c == 0x00b00502 &&
		tuple->cpu_a0 == 0x00017000 &&
		tuple->cpu_a4 == 0x00322808 &&
		tuple->cpu_link_50 == 0x86000000 &&
		tuple->cpu_link_58 == 0x00064555 &&
		tuple->ioh_82c == 0x004060a0 &&
		tuple->ioh_840 == 0x070f0f03 &&
		tuple->ioh_854 == 0x00010102 &&
		tuple->ioh_85c == 0x00000002 &&
		tuple->ioh_864 == 0x00322808 &&
		tuple->ioh_link_c8 == 0x0616fc00 &&
		tuple->ioh_sr0_7c == 0 && tuple->ioh_sr1_80 == 0 &&
		tuple->ioh_syre_cc == 0x00000600 &&
		stage->ioh_stage_9c ==
			X58_B06VE_EXPECTED_POST_MINIT_IOH_STAGE_9C &&
		stage->memory_clock_50 == 0x0a000006 &&
		stage->memory_clock_54 == 0x00000006;
}
#endif

#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
static bool b06vi_post_minit_common_endpoint_exact(
	const struct b06v8_qpi_tuple *tuple,
	const struct b06vb_stage_fingerprints *stage)
{
	/* A0 and CPU 9c are coupled by x58_b06vi_profile_for_tuple(). */
	return tuple != NULL && stage != NULL &&
		tuple->cpu_50 == 0x160c0112 &&
		tuple->cpu_54 == 0x00000012 &&
		tuple->cpu_6c == 0x0040a0a8 &&
		tuple->cpu_80 == X58_B06VE_EXPECTED_QPI_STATUS &&
		tuple->cpu_94 == 0x00010202 &&
		tuple->cpu_a4 == 0x00322808 &&
		tuple->cpu_link_50 == 0x86000000 &&
		tuple->cpu_link_58 == 0x00064555 &&
		tuple->ioh_82c == 0x004060a0 &&
		tuple->ioh_840 == 0x070f0f03 &&
		tuple->ioh_854 == 0x00010102 &&
		tuple->ioh_85c == 0x00000002 &&
		tuple->ioh_864 == 0x00322808 &&
		tuple->ioh_link_c8 == 0x0616fc00 &&
		tuple->ioh_sr0_7c == 0 && tuple->ioh_sr1_80 == 0 &&
		tuple->ioh_syre_cc == 0x00000600 &&
		stage->ioh_stage_9c ==
			X58_B06VE_EXPECTED_POST_MINIT_IOH_STAGE_9C &&
		stage->memory_clock_50 == 0x0a000006 &&
		stage->memory_clock_54 == 0x00000006;
}
#endif
#endif

#if CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
static uint32_t b06vd_csi_canonical_digest(const uint8_t *state)
{
	uint32_t digest = 2166136261u;

	/* Canonicalize only the digest stream; never modify the raw CSI state. */
	for (size_t offset = 0; offset < X58_VENDOR_CSI_STATE_SIZE; offset++) {
		const uint8_t value = offset == B06VD_CSI_DYNAMIC_OFFSET ?
			0 : state[offset];

		digest ^= value;
		digest *= 16777619u;
	}

	return digest;
}

static bool b06vd_select_expected_csi_raw_digest(const uint8_t *state,
	uint32_t observed_raw_digest, uint32_t canonical_digest,
	uint32_t *expected_raw_digest)
{
	if (state == NULL || expected_raw_digest == NULL ||
	    canonical_digest != B06VD_CSI_CANONICAL_FNV)
		return false;

	/* Select only a constant from an exact byte/raw-digest pair. */
	if (state[B06VD_CSI_DYNAMIC_OFFSET] == 0x08 &&
	    observed_raw_digest == B06VD_CSI_RAW_08_FNV) {
		*expected_raw_digest = B06VD_CSI_RAW_08_FNV;
		return true;
	}
	if (state[B06VD_CSI_DYNAMIC_OFFSET] == 0x0c &&
	    observed_raw_digest == B06VD_CSI_RAW_0C_FNV) {
		*expected_raw_digest = B06VD_CSI_RAW_0C_FNV;
		return true;
	}

	return false;
}
#endif

static bool b06vb_csi_state_exact(const uint8_t *state)
{
#if CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
	return state != NULL &&
		state[0x06] == 0x01 && state[0x07] == 0x01 &&
		(state[0x2a6] == 0x08 || state[0x2a6] == 0x0c) &&
		state[0x2ef] == 0x00 && state[0x301] == 0x00 &&
		state[0x1c] == 0x00 && state[0x1d] == 0x01 &&
		state[0x70] == 0x00 && state[0x71] == 0x01 &&
		state[0xcd] == 0x00 && state[0xce] == 0x01 &&
		state[0x121] == 0x00 && state[0x122] == 0x01 &&
		state[0x030] == 0x00 && state[0x031] == 0x00 &&
		state[0x1db] == 0x01 && state[0x206] == 0x12 &&
		state[0x275] == 0x01 && state[0x285] == 0x0e &&
		state[0x2ed] == 0x01 && state[0x2ef] == 0x00 &&
		state[0x2f8] == 0x11 && state[0x2f9] == 0x00 &&
		state[0x2fc] == 0x00 && state[0x302] == 0x01;
#else
	return state != NULL &&
		state[0x06] == 0x01 && state[0x07] == 0x01 &&
		state[0x2a6] == 0x0c && state[0x2ef] == 0x00 &&
		state[0x301] == 0x00 &&
		state[0x1c] == 0x00 && state[0x1d] == 0x01 &&
		state[0x70] == 0x00 && state[0x71] == 0x01 &&
		state[0xcd] == 0x00 && state[0xce] == 0x01 &&
		state[0x121] == 0x00 && state[0x122] == 0x01 &&
		state[0x030] == 0x00 && state[0x031] == 0x00 &&
		state[0x1db] == 0x01 && state[0x206] == 0x12 &&
		state[0x275] == 0x01 && state[0x285] == 0x0e &&
		state[0x2ed] == 0x01 && state[0x2ef] == 0x00 &&
		state[0x2f8] == 0x11 && state[0x2f9] == 0x00 &&
		state[0x2fc] == 0x00 && state[0x302] == 0x01;
#endif
}

#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
enum b06vg_csi_profile {
	B06VG_CSI_REJECTED,
	B06VG_CSI_PRIMARY,
	B06VG_CSI_OBSERVATION,
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	B06VL_CSI_BROAD_HARD_GATED,
#endif
};

static enum b06vg_csi_profile b06vg_select_csi_profile(
	uint32_t saved_pre_a0, const struct b06v8_qpi_tuple *tuple,
	const uint8_t *state, uint32_t raw_digest)
{
	bool pair_08;
	bool pair_0c;

	if (tuple == NULL || state == NULL ||
	    !x58_vendor_b06vg_high_qpi_cpu_a0_exact(saved_pre_a0) ||
	    !b06vb_csi_state_exact(state) ||
	    b06vd_csi_canonical_digest(state) != B06VD_CSI_CANONICAL_FNV)
		return B06VG_CSI_REJECTED;

	pair_08 = state[B06VD_CSI_DYNAMIC_OFFSET] == 0x08 &&
		raw_digest == B06VD_CSI_RAW_08_FNV;
	pair_0c = state[B06VD_CSI_DYNAMIC_OFFSET] == 0x0c &&
		raw_digest == B06VD_CSI_RAW_0C_FNV;
	if (!pair_08 && !pair_0c)
		return B06VG_CSI_REJECTED;

	/* Every profile shares the exact final High-QPI platform endpoint. */
	if (tuple->cpu_50 != 0x160c0112 || tuple->cpu_54 != 0x00000012 ||
	    tuple->cpu_6c != 0x0040a0a8 || tuple->cpu_80 != 0x070f0f03 ||
	    tuple->cpu_94 != 0x00010202 || tuple->cpu_a4 != 0x00322808 ||
	    tuple->cpu_link_50 != 0x86000000 ||
	    tuple->cpu_link_58 != 0x00064555 ||
	    tuple->ioh_82c != 0x004060a0 || tuple->ioh_840 != 0x070f0f03 ||
	    tuple->ioh_854 != 0x00010102 || tuple->ioh_85c != 0x00000002 ||
	    tuple->ioh_864 != 0x00322808 ||
	    tuple->ioh_link_c8 != 0x0616fc00 || tuple->ioh_sr0_7c != 0 ||
	    tuple->ioh_sr1_80 != 0 || tuple->ioh_syre_cc != 0x00000600)
		return B06VG_CSI_REJECTED;

#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	/*
	 * User-authorized broad experiment: couple a stable seven-value A0 candidate
	 * to one of the two observed CPU-9c values and exact CSI pairs.  These
	 * values authorize one guarded MINIT call only; they are not a RAM claim.
	 */
	if (x58_vendor_b06vl_high_qpi_cpu_a0_candidate(saved_pre_a0) &&
	    tuple->cpu_a0 == saved_pre_a0 &&
	    (tuple->cpu_9c == 0x00a00502u || tuple->cpu_9c == 0x00b00502u) &&
	    (pair_08 || pair_0c))
		return B06VL_CSI_BROAD_HARD_GATED;
	return B06VG_CSI_REJECTED;
#else

	if (saved_pre_a0 == 0x00017000 &&
	    tuple->cpu_a0 == saved_pre_a0 && tuple->cpu_9c == 0x00b00502 &&
	    pair_08)
		return B06VG_CSI_PRIMARY;

	if (saved_pre_a0 != 0x00017000 &&
	    tuple->cpu_a0 == saved_pre_a0 && tuple->cpu_9c == 0x00a00502 &&
	    (pair_08 || pair_0c))
		return B06VG_CSI_OBSERVATION;

	/* In particular, reject a post-CSI A0 which drifted from its saved pre-A0. */
	if (tuple->cpu_a0 != saved_pre_a0)
		return B06VG_CSI_REJECTED;
	return B06VG_CSI_REJECTED;
#endif
}

#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
static bool b06vi_o_pre_minit_candidate_exact(uint32_t saved_pre_a0,
	const struct b06v8_qpi_tuple *tuple, const uint8_t *state,
	uint32_t raw_digest)
{
	/* This authorizes only the attempt; the shared full truth table promotes. */
	return tuple != NULL && state != NULL &&
		saved_pre_a0 == 0x00017c00 &&
		tuple->cpu_a0 == 0x00017c00 &&
		tuple->cpu_9c == 0x00a00502 &&
		state[B06VD_CSI_DYNAMIC_OFFSET] ==
			X58_B06VI_EXPECTED_CSI_DYNAMIC_O &&
		raw_digest == X58_B06VI_EXPECTED_CSI_RAW_O_FNV &&
		b06vd_csi_canonical_digest(state) ==
			X58_B06VE_EXPECTED_CSI_CANONICAL_FNV;
}
#endif

static bool b06vg_post_csi_stage_exact(
	const struct b06vb_stage_fingerprints *stage)
{
	return stage->ioh_stage_9c == 0xea000000 &&
		stage->memory_clock_50 == 0x0a000006 &&
		stage->memory_clock_54 == 0x00000006;
}
#endif
#endif

static bool b06v8_print_csi_state(const uint8_t *state)
{
	for (unsigned int offset = 0; offset < X58_VENDOR_CSI_STATE_SIZE;
	     offset++) {
		if ((offset & 0xf) == 0 &&
		    (!b04_uart_puts("[QPI] CSI[") ||
		     !b04_uart_put_hex(offset, 4) || !b04_uart_puts("]=")))
			return false;
		if (!b04_uart_put_hex(state[offset], 2))
			return false;
		if ((offset & 0xf) == 0xf ||
		    offset + 1 == X58_VENDOR_CSI_STATE_SIZE) {
			if (!b04_uart_puts("\r\n"))
				return false;
		} else if (!b04_uart_putc(' ')) {
			return false;
		}
	}

	return true;
}

static bool b06v8_result_is_self_consistent(
	const struct x58_vendor_call_result *result,
	const struct x58_vendor_runtime_info *info)
{
	return info->last_call.eax == result->eax &&
		info->last_call.ebx == result->ebx &&
		info->last_call.ecx == result->ecx &&
		info->last_call.edx == result->edx &&
		info->last_call.edi == result->edi &&
		info->last_call.eflags == result->eflags &&
		info->last_call.vendor_esp_after_return ==
			result->vendor_esp_after_return &&
		result->vendor_esp_after_return ==
			info->vendor_stack_top - 3 * sizeof(uint32_t);
}

#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
static bool b06vb_print_complete_buffer(const char *name,
	const uint8_t *buffer, size_t size)
{
	if (buffer == NULL)
		return b04_uart_puts("[RAMINIT] BUFFER_UNAVAILABLE\r\n");

	for (size_t offset = 0; offset < size; offset++) {
		if ((offset & 0xf) == 0 &&
		    (!b04_uart_puts("[RAMINIT] ") || !b04_uart_puts(name) ||
		     !b04_uart_putc('[') || !b04_uart_put_hex(offset, 4) ||
		     !b04_uart_puts("]=")))
			return false;
		if (!b04_uart_put_hex(buffer[offset], 2))
			return false;
		if ((offset & 0xf) == 0xf || offset + 1 == size) {
			if (!b04_uart_puts("\r\n"))
				return false;
		} else if (!b04_uart_putc(' ')) {
			return false;
		}
	}

	return true;
}

static bool b06vb_minit_result_is_self_consistent(
	const struct x58_vendor_call_result *result,
	const struct x58_vendor_runtime_info *info)
{
	return info->last_call.eax == result->eax &&
		info->last_call.ebx == result->ebx &&
		info->last_call.ecx == result->ecx &&
		info->last_call.edx == result->edx &&
		info->last_call.edi == result->edi &&
		info->last_call.eflags == result->eflags &&
		info->last_call.vendor_esp_after_return ==
			result->vendor_esp_after_return &&
		result->vendor_esp_after_return ==
			info->vendor_stack_top - 2 * sizeof(uint32_t);
}

#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
static enum b06v6_auto_result b06ve_promote_post_minit(
	struct b06j_state *state, enum x58_vendor_status minit_call_status,
	const struct x58_vendor_call_result *csi_result,
	const struct x58_vendor_call_result *minit_result,
	const uint8_t *final_csi_state, const uint8_t *policy,
	const uint8_t *workspace, bool post_minit_i801_exact,
	uint8_t post_minit_cmos
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	, uint32_t saved_pre_a0,
	const struct b06v8_qpi_tuple *pre_minit_tuple
#endif
	)
{
	struct x58_vendor_runtime_info info = { 0 };
	struct b06v8_qpi_tuple post_minit_tuple;
	struct b06vb_stage_fingerprints post_minit_stage;
	const enum x58_vendor_status probe_status =
		x58_vendor_b06ve_post_minit_probe(&info);
	uint32_t workspace_fnv = 0;
#if CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
	uint32_t workspace_canonical_fnv = 0;
	bool workspace_pattern_exact = false;
	bool probe_status_admitted;
#endif
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	uint32_t final_csi_raw_fnv = 0;
	uint32_t final_csi_canonical_fnv = 0;
	enum x58_b06vi_profile_id profile_id = X58_B06VI_PROFILE_INVALID;
#endif
	uint32_t mc_mapper;
	uint32_t mc_common_f8;
	uint32_t ch2_dod;
	uint32_t ch2_ranks;
	uint32_t ch2_status;

	if (workspace != NULL) {
		workspace_fnv = b06v0_buffer_digest(workspace,
			X58_VENDOR_MINIT_WORKSPACE_SIZE);
#if CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
		workspace_canonical_fnv =
			b06vf_workspace_canonical_digest(workspace);
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
		workspace_pattern_exact = b06vi_workspace_pattern_exact(workspace,
			workspace_fnv, workspace_canonical_fnv);
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
		workspace_pattern_exact = b06vh_workspace_pattern_exact(workspace,
			workspace_fnv, workspace_canonical_fnv);
#else
		workspace_pattern_exact = b06vf_workspace_raw_pattern_exact(
			workspace, workspace_fnv);
#endif
#endif
	}
#if CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
	/*
	 * The unchanged vendor B06VE platform gate can report PLATFORM_STATE for a
	 * later build's otherwise exact workspace.  Older builds admit that status
	 * only through their workspace contract.  B06VL instead admits the status
	 * here because every non-workspace platform field is independently hard-gated
	 * below; it still rejects every other probe status.
	 */
	probe_status_admitted = probe_status == X58_VENDOR_OK ||
		(probe_status == X58_VENDOR_ERR_PLATFORM_STATE &&
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
		 /* Every platform field is independently hard-gated below. */
		 true);
#else
		 b06vi_o_workspace_pattern_exact(workspace, workspace_fnv,
			workspace_canonical_fnv) &&
		 workspace_pattern_exact);
#endif
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
		 workspace_pattern_exact &&
		 x58_b06vh_workspace_digest_pair_exact(workspace_fnv,
			workspace_canonical_fnv));
#else
		 workspace_fnv == X58_B06VF_EXPECTED_WORKSPACE_RAW_B_FNV &&
		 workspace_pattern_exact &&
		 workspace_canonical_fnv ==
			X58_B06VF_EXPECTED_WORKSPACE_CANONICAL_FNV);
#endif
#endif
	b06v8_capture_qpi_tuple(&post_minit_tuple);
	b06vb_capture_stage_fingerprints(&post_minit_stage);
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	if (final_csi_state != NULL) {
		final_csi_raw_fnv = b06v0_buffer_digest(final_csi_state,
			X58_VENDOR_CSI_STATE_SIZE);
		final_csi_canonical_fnv =
			b06vd_csi_canonical_digest(final_csi_state);
	}
	profile_id = x58_b06vi_profile_for_tuple(saved_pre_a0,
		post_minit_tuple.cpu_a0, post_minit_tuple.cpu_9c,
		final_csi_state == NULL ? 0xff :
			final_csi_state[B06VD_CSI_DYNAMIC_OFFSET],
		final_csi_raw_fnv, final_csi_canonical_fnv, workspace_fnv,
		workspace_canonical_fnv);
#endif
	if (!b06v0_print_runtime(state) ||
	    !b04_uart_puts(
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
		"[RAMINIT] B06VL BROAD RETURN POLICY_IN/NOW=") ||
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
		"[RAMINIT] B06VK COUPLED RETURN POLICY_IN/NOW=") ||
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
		"[RAMINIT] B06VJ COUPLED RETURN POLICY_IN/NOW=") ||
#else
		"[RAMINIT] B06VI COUPLED RETURN POLICY_IN/NOW=") ||
#endif
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
		"[RAMINIT] B06VH PRIMARY RETURN POLICY_IN/NOW=") ||
#elif CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
		"[RAMINIT] B06VG PRIMARY RETURN POLICY_IN/NOW=") ||
#elif CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
		"[RAMINIT] B06VF RETURN POLICY_IN/NOW=") ||
#else
		"[RAMINIT] B06VE RETURN POLICY_IN/NOW=") ||
#endif
	    !b04_uart_put_hex(info.minit_policy_digest, 8) ||
	    !b04_uart_putc('/') ||
	    !b04_uart_put_hex(info.minit_policy_current_digest, 8) ||
	    !b04_uart_puts(" WORK_FNV=") ||
	    !b04_uart_put_hex(workspace_fnv, 8) ||
	    !b04_uart_puts(" DRAM_ACCESSES=00\r\n") ||
	    !b06v8_print_qpi_tuple("POST_MINIT", &post_minit_tuple) ||
	    !b06vb_print_stage_fingerprints("POST_MINIT", &post_minit_stage,
		csi_result) ||
	    !b06v0_emit_status(
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
		"B06VL_BROAD_POST_MINIT_PROBE",
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
		"B06VK_COUPLED_POST_MINIT_PROBE",
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
		"B06VJ_COUPLED_POST_MINIT_PROBE",
#else
		"B06VI_COUPLED_POST_MINIT_PROBE",
#endif
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
		"B06VH_PRIMARY_POST_MINIT_PROBE",
#elif CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
		"B06VG_PRIMARY_POST_MINIT_PROBE",
#elif CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
		"B06VF_POST_MINIT_PROBE",
#else
		"B06VE_POST_MINIT_PROBE",
#endif
		probe_status))
		return B06V6_AUTO_UART_ERROR;
#if CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
	if (!b04_uart_puts(
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
		"[RAMINIT] B06VL WORK_CANON_TELEMETRY=") ||
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
		"[RAMINIT] B06VK COUPLED WORK_CANON=") ||
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
		"[RAMINIT] B06VJ COUPLED WORK_CANON=") ||
#else
		"[RAMINIT] B06VI COUPLED WORK_CANON=") ||
#endif
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
		"[RAMINIT] B06VH PRIMARY WORK_CANON=") ||
#elif CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
		"[RAMINIT] B06VG PRIMARY WORK_CANON=") ||
#else
		"[RAMINIT] B06VF WORK_CANON=") ||
#endif
	    !b04_uart_put_hex(workspace_canonical_fnv, 8) ||
	    !b04_uart_puts(" WORK[2459..245c/2461]=") ||
	    !b04_uart_put_hex(workspace == NULL ? 0 : workspace[0x2459], 2) ||
	    !b04_uart_putc(':') ||
	    !b04_uart_put_hex(workspace == NULL ? 0 : workspace[0x245a], 2) ||
	    !b04_uart_putc(':') ||
	    !b04_uart_put_hex(workspace == NULL ? 0 : workspace[0x245b], 2) ||
	    !b04_uart_putc(':') ||
	    !b04_uart_put_hex(workspace == NULL ? 0 : workspace[0x245c], 2) ||
	    !b04_uart_putc('/') ||
	    !b04_uart_put_hex(workspace == NULL ? 0 : workspace[0x2461], 2) ||
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	    !b04_uart_puts(" WORKSPACE_PATTERN_TELEMETRY=") ||
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
	    !b04_uart_puts(" CANONICAL_PATTERN_GATE=") ||
#else
	    !b04_uart_puts(" RAW_PATTERN_GATE=") ||
#endif
	    !b04_uart_put_hex(workspace_pattern_exact, 2) ||
	    !b04_uart_puts(" PROBE_STATUS_ADMITTED=") ||
	    !b04_uart_put_hex(probe_status_admitted, 2) ||
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	    !b04_uart_puts(" WORKSPACE_HASH_MODE=RAW_AND_CANONICAL_TELEMETRY_ONLY") ||
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
	    !b04_uart_puts(" Q_RAW_MODE=TELEMETRY_ONLY_FOR_PROFILE_06") ||
#endif
	    !b04_uart_puts("\r\n"))
		return B06V6_AUTO_UART_ERROR;
#endif

	mc_mapper = b06j_pci_read(B06V5_MC_COMMON_DEV,
		B06V5_MC_CHANNEL_MAPPER, 4);
	mc_common_f8 = b06j_pci_read(B06K_UNCORE_COMMON_DEV, 0xf8, 4);
	ch2_dod = b06j_pci_read(B06V5_CHANNEL2_ADDR_DEV,
		B06V5_MC_DOD_DIMM0, 4);
	ch2_ranks = b06j_pci_read(B06K_CHANNEL2_DEV,
		B06K_MC_RANK_PRESENT, 4);
	ch2_status = b06j_pci_read(B06K_CHANNEL2_DEV,
		B06K_MC_INIT_STATUS, 4);
	if (!b04_uart_puts(
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
		"[RAMINIT] B06VL BROAD WORK_RAW_TELEMETRY=") ||
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
		"[RAMINIT] B06VK COUPLED WORK_RAW_TELEMETRY=") ||
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
		"[RAMINIT] B06VJ COUPLED EXACT WORK_RAW=") ||
#else
		"[RAMINIT] B06VI COUPLED EXACT WORK_RAW=") ||
#endif
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
		"[RAMINIT] B06VH PRIMARY EXACT WORK_RAW=") ||
#elif CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
		"[RAMINIT] B06VG PRIMARY EXACT WORK_RAW=") ||
#elif CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
		"[RAMINIT] B06VF EXACT WORK_RAW=") ||
#else
		"[RAMINIT] B06VE EXACT WORK_FNV=") ||
#endif
	    !b04_uart_put_hex(workspace_fnv, 8) ||
	    !b04_uart_puts(" MC_MAP60=") || !b04_uart_put_hex(mc_mapper, 8) ||
	    !b04_uart_puts(" MC_F8=") || !b04_uart_put_hex(mc_common_f8, 8) ||
	    !b04_uart_puts(" CH2_DOD=") || !b04_uart_put_hex(ch2_dod, 8) ||
	    !b04_uart_puts(" CH2_RANKS=") || !b04_uart_put_hex(ch2_ranks, 8) ||
	    !b04_uart_puts(" CH2_STATUS=") || !b04_uart_put_hex(ch2_status, 8) ||
	    !b04_uart_puts(" IOH_STAGE9C=") ||
	    !b04_uart_put_hex(post_minit_stage.ioh_stage_9c, 8) ||
	    !b04_uart_puts("\r\n"))
		return B06V6_AUTO_UART_ERROR;

#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	/*
	 * B06VI/B06VJ/B06VK admit complete rows from the shared truth table.
	 * B06VL instead selects its version-9 broad CPU/CSI hard class and treats
	 * both workspace digests and legacy workspace patterns as telemetry.
	 * Universal completion bytes and every ABI/platform/guard predicate below
	 * remain mandatory before any ordinary DRAM access.
	 * The pre-MINIT endpoint must also be identical to the saved and returned
	 * endpoint.
	 */
	if (minit_call_status != X58_VENDOR_OK ||
	    !probe_status_admitted ||
	    final_csi_state == NULL || policy == NULL || workspace == NULL ||
	    pre_minit_tuple == NULL ||
	    !info.prepared || !info.canaries_valid ||
	    !info.wrapper_signature_valid || !info.csi_signature_valid ||
	    !info.minit_signature_valid || info.csi_wrapper_armed ||
	    !info.csi_call_attempted || !info.csi_returned ||
	    !info.csi_result_accepted || !info.b06v8_high_qpi_csi_profile ||
	    !info.b06vb_high_qpi_minit_authorized ||
	    !info.minit_policy_confirmed || info.minit_armed ||
	    !info.minit_call_attempted || !info.minit_returned ||
	    info.experimental_status_seed != 0 ||
	    info.cpuid_1_eax != X58_B06V6_EXPECTED_CPUID ||
	    info.microcode_revision != X58_B06V6_EXPECTED_UCODE ||
	    info.uncore_sad_id != 0x2d818086 ||
	    info.pciexbar_low != 0xe0000001 || info.pciexbar_high != 0 ||
	    info.x58_hostbridge_id != 0x34058086 ||
	    info.x58_hostbridge_class_revision != 0x06000013 ||
	    info.qpi_phy_observed_80 != X58_B06VE_EXPECTED_QPI_STATUS ||
	    info.memory_clock_observed_50 != 0x0a000006 ||
	    info.memory_clock_observed_54 != 0x00000006 ||
	    !b06vb_csi_state_exact(final_csi_state) ||
	    info.csi_state_digest != final_csi_raw_fnv ||
	    b06v0_buffer_digest(final_csi_state,
		X58_VENDOR_CSI_STATE_SIZE) != final_csi_raw_fnv ||
	    b06vd_csi_canonical_digest(final_csi_state) !=
		final_csi_canonical_fnv ||
	    info.minit_policy_digest != X58_B06V6_EXPECTED_POLICY_FNV ||
	    info.minit_policy_current_digest !=
		X58_B06V6_EXPECTED_POLICY_FNV ||
	    b06v0_policy_digest(policy) != X58_B06V6_EXPECTED_POLICY_FNV ||
	    info.minit_workspace_digest != workspace_fnv ||
#if !CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	    !workspace_pattern_exact ||
	    profile_id == X58_B06VI_PROFILE_INVALID ||
#endif
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	    profile_id != X58_B06VL_PROFILE_BROAD ||
#endif
	    !x58_b06vi_profile_tuple_exact(profile_id, saved_pre_a0,
		post_minit_tuple.cpu_a0, post_minit_tuple.cpu_9c,
		final_csi_raw_fnv, final_csi_canonical_fnv, workspace_fnv,
		workspace_canonical_fnv) ||
	    pre_minit_tuple->cpu_a0 != saved_pre_a0 ||
	    pre_minit_tuple->cpu_a0 != post_minit_tuple.cpu_a0 ||
	    pre_minit_tuple->cpu_9c != post_minit_tuple.cpu_9c ||
	    minit_result->eax != 0 || workspace[1] != 0 || workspace[2] != 0 ||
	    workspace[B06V5_WORK_B3_FLAGS_OFFSET] != 0x02 ||
	    workspace[B06V5_WORK_COMPLETE_OFFSET] != 0x01 ||
	    policy[B06V5_POLICY_STATUS_OFFSET] != 0 ||
	    !b06vb_minit_result_is_self_consistent(minit_result, &info) ||
	    !post_minit_i801_exact ||
	    post_minit_cmos != B06V6_CMOS_GUARD_IN_PROGRESS ||
	    !b06vi_post_minit_common_endpoint_exact(&post_minit_tuple,
		&post_minit_stage) ||
	    mc_mapper != X58_B06V6_EXPECTED_MC_MAPPER ||
	    mc_common_f8 != X58_B06V6_EXPECTED_MC_COMMON_F8 ||
	    ch2_dod != X58_B06V6_EXPECTED_CH2_DOD ||
	    ch2_ranks != X58_B06V6_EXPECTED_CH2_RANKS ||
	    ch2_status != X58_B06V6_EXPECTED_CH2_STATUS ||
	    !b06v6_spd_is_exact(state))
		return b06v6_fallback(
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
			"B06VL_BROAD_POST_MINIT_HARD_GATE");
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
			"B06VK_COUPLED_POST_MINIT_EXACT_GATE");
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
			"B06VJ_COUPLED_POST_MINIT_EXACT_GATE");
#else
			"B06VI_COUPLED_POST_MINIT_EXACT_GATE");
#endif
#else
	if (minit_call_status != X58_VENDOR_OK ||
#if CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
	    !probe_status_admitted ||
#else
	    probe_status != X58_VENDOR_OK ||
#endif
	    final_csi_state == NULL || policy == NULL || workspace == NULL ||
	    !info.prepared || !info.canaries_valid ||
	    !info.wrapper_signature_valid || !info.csi_signature_valid ||
	    !info.minit_signature_valid || info.csi_wrapper_armed ||
	    !info.csi_call_attempted || !info.csi_returned ||
	    !info.csi_result_accepted || !info.b06v8_high_qpi_csi_profile ||
	    !info.b06vb_high_qpi_minit_authorized ||
	    !info.minit_policy_confirmed || info.minit_armed ||
	    !info.minit_call_attempted || !info.minit_returned ||
	    info.experimental_status_seed != 0 ||
	    info.cpuid_1_eax != X58_B06V6_EXPECTED_CPUID ||
	    info.microcode_revision != X58_B06V6_EXPECTED_UCODE ||
	    info.uncore_sad_id != 0x2d818086 ||
	    info.pciexbar_low != 0xe0000001 || info.pciexbar_high != 0 ||
	    info.x58_hostbridge_id != 0x34058086 ||
	    info.x58_hostbridge_class_revision != 0x06000013 ||
	    info.qpi_phy_observed_80 != X58_B06VE_EXPECTED_QPI_STATUS ||
	    info.memory_clock_observed_50 != 0x0a000006 ||
	    info.memory_clock_observed_54 != 0x00000006 ||
	    final_csi_state[B06VD_CSI_DYNAMIC_OFFSET] != 0x08 ||
	    info.csi_state_digest != X58_B06VE_EXPECTED_CSI_FNV ||
	    b06v0_buffer_digest(final_csi_state,
		X58_VENDOR_CSI_STATE_SIZE) != X58_B06VE_EXPECTED_CSI_FNV ||
	    b06vd_csi_canonical_digest(final_csi_state) !=
		X58_B06VE_EXPECTED_CSI_CANONICAL_FNV ||
	    info.minit_policy_digest != X58_B06V6_EXPECTED_POLICY_FNV ||
	    info.minit_policy_current_digest !=
		X58_B06V6_EXPECTED_POLICY_FNV ||
	    b06v0_policy_digest(policy) != X58_B06V6_EXPECTED_POLICY_FNV ||
#if CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
	    info.minit_workspace_digest != workspace_fnv ||
	    !workspace_pattern_exact ||
#if CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
	    !x58_b06vh_workspace_digest_pair_exact(workspace_fnv,
		workspace_canonical_fnv) ||
#else
	    workspace_canonical_fnv !=
		X58_B06VF_EXPECTED_WORKSPACE_CANONICAL_FNV ||
#endif
#else
	    info.minit_workspace_digest != X58_B06VE_EXPECTED_WORKSPACE_FNV ||
	    workspace_fnv != X58_B06VE_EXPECTED_WORKSPACE_FNV ||
#endif
	    minit_result->eax != 0 || workspace[1] != 0 || workspace[2] != 0 ||
	    workspace[B06V5_WORK_B3_FLAGS_OFFSET] != 0x02 ||
	    workspace[B06V5_WORK_COMPLETE_OFFSET] != 0x01 ||
	    policy[B06V5_POLICY_STATUS_OFFSET] != 0 ||
	    !b06vb_minit_result_is_self_consistent(minit_result, &info) ||
	    !post_minit_i801_exact ||
	    post_minit_cmos != B06V6_CMOS_GUARD_IN_PROGRESS ||
	    !b06ve_post_minit_tuple_exact(&post_minit_tuple,
		&post_minit_stage) ||
	    mc_mapper != X58_B06V6_EXPECTED_MC_MAPPER ||
	    mc_common_f8 != X58_B06V6_EXPECTED_MC_COMMON_F8 ||
	    ch2_dod != X58_B06V6_EXPECTED_CH2_DOD ||
	    ch2_ranks != X58_B06V6_EXPECTED_CH2_RANKS ||
	    ch2_status != X58_B06V6_EXPECTED_CH2_STATUS ||
	    !b06v6_spd_is_exact(state))
		return b06v6_fallback(
#if CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
			"B06VH_PRIMARY_POST_MINIT_EXACT_GATE"
#elif CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
			"B06VG_PRIMARY_POST_MINIT_EXACT_GATE"
#elif CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
			"B06VF_POST_MINIT_CANONICAL_EXACT_GATE"
#else
			"B06VE_POST_MINIT_EXACT_GATE"
#endif
			);
#endif

#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	if (!b04_uart_puts(
		"[RAMINIT] B06VL BROAD_UNSAFE HARD_RETURN_GATE=PASS; WORKSPACE_HASHES=TELEMETRY_ONLY; DRAM_NOT_YET_PROVEN; bounded UC tests follow\r\n") ||
	    !b04_uart_wait_for(UART8250_LSR_TEMT,
		B03_UART_FLUSH_POLL_LIMIT))
		return B06V6_AUTO_UART_ERROR;
#endif

#if CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
	/*
	 * MINIT owns these otherwise idle I801 host registers and reproducibly
	 * leaves the exact return tuple checked by the caller.  Rearm only after
	 * every PRIMARY workspace, CPU, IMC, IOH, QPI, ABI, and CMOS gate above
	 * has passed.  The post-memory finalizer therefore still has a unique
	 * in-progress marker to consume after all destructive DRAM readbacks.
	 */
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	if (x58_b06vi_profile_is_primary(profile_id))
#endif
	{
		struct b06v6_i801_signature rearmed_i801 = { 0 };
		uint8_t rearmed_cmos;
		bool rearmed_exact;

		if (!b06v6_set_i801_signature(
			&b06vb_minit_in_progress_signature))
			b06v8_phase_guard_stop("B06VH_POST_MINIT_I801_REARM_WRITE");
		b06v6_capture_i801_signature(&rearmed_i801);
		rearmed_exact = b06v6_i801_signature_matches(&rearmed_i801,
			&b06vb_minit_in_progress_signature);
		rearmed_cmos = b06v6_read_cmos_diagnostic();
		if (!b04_uart_puts(
			"[RAMINIT] B06VH POST_MINIT_REARM I801=") ||
		    !b04_uart_put_hex(rearmed_i801.control, 2) ||
		    !b04_uart_putc(':') ||
		    !b04_uart_put_hex(rearmed_i801.command, 2) ||
		    !b04_uart_putc(':') ||
		    !b04_uart_put_hex(rearmed_i801.xmit_address, 2) ||
		    !b04_uart_putc(':') ||
		    !b04_uart_put_hex(rearmed_i801.data0, 2) ||
		    !b04_uart_putc(':') ||
		    !b04_uart_put_hex(rearmed_i801.data1, 2) ||
		    !b04_uart_puts(" REARMED_EXACT=") ||
		    !b04_uart_put_hex(rearmed_exact, 2) ||
		    !b04_uart_puts(" CMOS0E=") ||
		    !b04_uart_put_hex(rearmed_cmos, 2) ||
		    !b04_uart_puts("\r\n"))
			return B06V6_AUTO_UART_ERROR;
		if (!rearmed_exact ||
		    rearmed_cmos != B06V6_CMOS_GUARD_IN_PROGRESS)
			b06v8_phase_guard_stop("B06VH_POST_MINIT_I801_REARM_READBACK");
	}
#endif

	{
		const struct x58_b06v6_raminit_result handoff = {
			.cpuid = info.cpuid_1_eax,
			.microcode_revision = info.microcode_revision,
			.spd_fnv = state->vendor_spd_digest,
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
			.spd_profile_fnv = state->vendor_spd_profile_digest,
#endif
			.csi_state_fnv = info.csi_state_digest,
			.policy_fnv = info.minit_policy_digest,
			.workspace_fnv = info.minit_workspace_digest,
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
			.csi_state_canonical_fnv = final_csi_canonical_fnv,
#else
			.csi_state_canonical_fnv =
				X58_B06VE_EXPECTED_CSI_CANONICAL_FNV,
#endif
#if CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
			.workspace_canonical_fnv = workspace_canonical_fnv,
#else
			.workspace_canonical_fnv = 0,
#endif
			.minit_eax = minit_result->eax,
			.mc_mapper = mc_mapper,
			.mc_common_f8 = mc_common_f8,
			.ch2_dod = ch2_dod,
			.ch2_ranks = ch2_ranks,
			.ch2_status = ch2_status,
			.qpi_status = post_minit_tuple.cpu_80,
			.post_minit_ioh_stage_9c = post_minit_stage.ioh_stage_9c,
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
			.profile_id = profile_id,
			.saved_pre_a0 = saved_pre_a0,
			.post_minit_cpu_a0 = post_minit_tuple.cpu_a0,
			.post_minit_cpu_9c = post_minit_tuple.cpu_9c,
#endif
		};

		x58_b06v6_handoff_record_success(&handoff);
	}
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	if (!b04_uart_puts(
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
		"[RAMINIT] B06VL_BROAD_AUTO_HANDOFF=READY PROFILE=") ||
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
		"[RAMINIT] B06VK_COUPLED_AUTO_HANDOFF=READY PROFILE=") ||
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
		"[RAMINIT] B06VJ_COUPLED_AUTO_HANDOFF=READY PROFILE=") ||
#else
		"[RAMINIT] B06VI_COUPLED_AUTO_HANDOFF=READY PROFILE=") ||
#endif
	    !b04_uart_put_hex(profile_id, 2) ||
	    !b04_uart_puts(x58_b06vi_profile_requires_deferred_rearm(profile_id) ?
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
		"; BROAD I801 rearm deferred until full v10 postmem; provisional payload attempt\r\n" :
#else
		"; BROAD I801 rearm deferred until full v9 postmem; provisional payload attempt\r\n" :
#endif
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
		"; O-family I801 rearm deferred until full v8 postmem; provisional payload attempt\r\n" :
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
		"; O-family I801 rearm deferred until full v7 postmem; provisional payload attempt\r\n" :
#else
		"; O I801 rearm deferred until full v6 postmem; provisional payload attempt\r\n" :
#endif
		"; inherited PRIMARY I801 rearm complete; provisional payload attempt\r\n") ||
	    !b04_uart_wait_for(UART8250_LSR_TEMT,
		B03_UART_FLUSH_POLL_LIMIT))
		return B06V6_AUTO_UART_ERROR;
#else
	if (!b04_uart_puts(
#if CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
		"[RAMINIT] B06VH_PRIMARY_AUTO_HANDOFF=READY; exact four-pair workspace gate; I801 rearmed; persistent guard retained through v5 postmem\r\n") ||
#elif CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
		"[RAMINIT] B06VG_PRIMARY_AUTO_HANDOFF=READY; exact inherited B06VF gates; persistent guard retained through v4 postmem\r\n") ||
#elif CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
		"[RAMINIT] B06VF_AUTO_HANDOFF=READY; canonical High-QPI workspace; persistent guard retained through v4 postmem\r\n") ||
#else
		"[RAMINIT] B06VE_AUTO_HANDOFF=READY; exact High-QPI post-MINIT state; persistent guard retained through postmem\r\n") ||
#endif
	    !b04_uart_wait_for(UART8250_LSR_TEMT,
		B03_UART_FLUSH_POLL_LIMIT))
		return B06V6_AUTO_UART_ERROR;
#endif
	outb(POST_B06V6_MINIT_ACCEPTED, CONFIG_POST_IO_PORT);

	return B06V6_AUTO_READY;
}
#endif

static enum b06v6_auto_result b06vb_high_qpi_minit_observe(
	struct b06j_state *state,
	const struct x58_vendor_call_result *csi_result,
	const struct x58_vendor_runtime_info *csi_info,
	const uint8_t *csi_state, uint32_t csi_raw_fnv,
	const struct b06v8_qpi_tuple *csi_tuple,
	enum x58_vendor_status csi_call_status,
	enum x58_vendor_status csi_probe_status
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	, uint32_t saved_pre_a0
#endif
	)
{
	struct x58_vendor_call_result minit_result = { 0 };
	struct x58_vendor_runtime_info info = { 0 };
	struct b06v6_i801_signature i801 = { 0 };
	struct b06v8_qpi_tuple post_minit_tuple;
	struct b06vb_stage_fingerprints csi_stage;
	struct b06vb_stage_fingerprints post_minit_stage;
	const uint8_t *final_csi_state;
	const uint8_t *policy;
	const uint8_t *workspace;
	enum x58_vendor_status status;
	uint32_t policy_base_fnv;
	uint32_t policy_final_fnv;
	uint32_t workspace_fnv = 0;
#if CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
	uint32_t csi_canonical_fnv = 0;
	uint32_t expected_csi_raw_fnv = 0;
	bool csi_digest_pair_exact = false;
#endif
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	enum b06vg_csi_profile csi_profile = B06VG_CSI_REJECTED;
#endif
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	bool b06vi_o_candidate = false;
#endif
	uint8_t post_minit_cmos;
	bool post_minit_i801_exact;
	bool pre_minit_i801_consumed;
	bool full_path_observed;

	b06vb_capture_stage_fingerprints(&csi_stage);
	if (!b06vb_print_stage_fingerprints("PASS3_RETURN", &csi_stage,
		csi_result))
		return B06V6_AUTO_UART_ERROR;
#if CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
	if (csi_state != NULL) {
		csi_canonical_fnv = b06vd_csi_canonical_digest(csi_state);
		csi_digest_pair_exact = b06vd_select_expected_csi_raw_digest(
			csi_state, csi_raw_fnv, csi_canonical_fnv,
			&expected_csi_raw_fnv);
	}
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	csi_profile = b06vg_select_csi_profile(saved_pre_a0, csi_tuple,
		csi_state, csi_raw_fnv);
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	b06vi_o_candidate = b06vi_o_pre_minit_candidate_exact(saved_pre_a0, csi_tuple,
			csi_state, csi_raw_fnv);
#endif
#endif
	if (!b04_uart_puts("[QPI] B06VD CSI2A6=") ||
	    !(csi_state != NULL ?
		b04_uart_put_hex(csi_state[B06VD_CSI_DYNAMIC_OFFSET], 2) :
		b04_uart_puts("NA")) ||
	    !b04_uart_puts(" RAW_FNV=") ||
	    !b04_uart_put_hex(csi_raw_fnv, 8) ||
	    !b04_uart_puts(" CANONICAL_2A6_ZERO_FNV=") ||
	    !b04_uart_put_hex(csi_canonical_fnv, 8) ||
	    !b04_uart_puts(" EXPECT=908dabb6 PAIRED=") ||
	    !b04_uart_put_hex(csi_digest_pair_exact, 2) ||
	    !b04_uart_puts(" RAW_BUFFER_MUTATED=00\r\n"))
		return B06V6_AUTO_UART_ERROR;
	if (csi_call_status != X58_VENDOR_OK ||
	    csi_probe_status != X58_VENDOR_OK || csi_state == NULL ||
	    !csi_info->prepared || !csi_info->canaries_valid ||
	    !csi_info->wrapper_signature_valid ||
	    !csi_info->csi_signature_valid ||
	    !csi_info->minit_signature_valid ||
	    !csi_info->csi_call_attempted || !csi_info->csi_returned ||
	    csi_info->csi_result_accepted ||
	    csi_info->experimental_status_seed != 0 ||
	    csi_result->eax != 0 || csi_result->ebx != 0 ||
	    csi_result->ecx != 0x11 ||
	    csi_info->csi_state_digest != csi_raw_fnv ||
	    !csi_digest_pair_exact ||
	    !b06vb_csi_state_exact(csi_state) ||
	    !b06v8_result_is_self_consistent(csi_result, csi_info) ||
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	    !csi_info->b06vg_pre_csi_cpu_a0_valid ||
	    csi_info->b06vg_pre_csi_cpu_a0 != saved_pre_a0 ||
	    csi_profile == B06VG_CSI_REJECTED ||
	    !b06vg_post_csi_stage_exact(&csi_stage))
		return b06v6_fallback("B06VG_COUPLED_HIGH_CSI_GATE");
#else
	    !b06vb_post_csi_tuple_exact(csi_tuple, &csi_stage))
		return b06v6_fallback("B06VD_HIGH_CSI_OBSERVATION_GATE");
#endif
#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
#if !CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	/* Keep B06VD's pair/tuple audit, then reject its 0c diagnostic branch. */
	if (csi_state[B06VD_CSI_DYNAMIC_OFFSET] == 0x0c &&
	    expected_csi_raw_fnv == B06VD_CSI_RAW_0C_FNV)
		return b06v6_fallback("B06VE_CSI_0C_DIAGNOSTIC_FALLBACK");
	if (csi_state[B06VD_CSI_DYNAMIC_OFFSET] != 0x08 ||
	    expected_csi_raw_fnv != B06VD_CSI_RAW_08_FNV)
		return b06v6_fallback("B06VE_CSI_08_EXACT_GATE");
#endif
#endif
#else
	if (csi_call_status != X58_VENDOR_OK ||
	    csi_probe_status != X58_VENDOR_OK || csi_state == NULL ||
	    !csi_info->prepared || !csi_info->canaries_valid ||
	    !csi_info->wrapper_signature_valid ||
	    !csi_info->csi_signature_valid ||
	    !csi_info->minit_signature_valid ||
	    !csi_info->csi_call_attempted || !csi_info->csi_returned ||
	    csi_info->csi_result_accepted ||
	    csi_info->experimental_status_seed != 0 ||
	    csi_result->eax != 0 || csi_result->ebx != 0 ||
	    csi_result->ecx != 0x11 ||
	    csi_info->csi_state_digest != B06VB_CSI_OBSERVATION_FNV ||
	    csi_raw_fnv != B06VB_CSI_OBSERVATION_FNV ||
	    !b06vb_csi_state_exact(csi_state) ||
	    !b06v8_result_is_self_consistent(csi_result, csi_info) ||
	    !b06vb_post_csi_tuple_exact(csi_tuple, &csi_stage))
		return b06v6_fallback("B06VB_HIGH_CSI_OBSERVATION_GATE");
#endif

#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	if (!b04_uart_puts(
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
		"[QPI] B06VL BROAD_UNSAFE_PROFILE=") ||
	    !b04_uart_puts(csi_profile == B06VL_CSI_BROAD_HARD_GATED ?
		"BROAD_HARD_GATED" : "REJECTED") ||
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
		"[QPI] B06VK COUPLED_PROFILE=") ||
	    !b04_uart_puts(b06vi_o_candidate ? "O-FAMILY" :
		(csi_profile == B06VG_CSI_PRIMARY ? "PRIMARY" : "OBSERVATION")) ||
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
		"[QPI] B06VJ COUPLED_PROFILE=") ||
	    !b04_uart_puts(b06vi_o_candidate ? "O-FAMILY" :
		(csi_profile == B06VG_CSI_PRIMARY ? "PRIMARY" : "OBSERVATION")) ||
#else
		"[QPI] B06VI COUPLED_PROFILE=") ||
	    !b04_uart_puts(b06vi_o_candidate ? "O" :
		(csi_profile == B06VG_CSI_PRIMARY ? "PRIMARY" : "OBSERVATION")) ||
#endif
#else
		"[QPI] B06VG COUPLED_PROFILE=") ||
	    !b04_uart_puts(csi_profile == B06VG_CSI_PRIMARY ?
		"PRIMARY" : "OBSERVATION") ||
#endif
	    !b04_uart_puts(" PRE_A0=") ||
	    !b04_uart_put_hex(saved_pre_a0, 8) ||
	    !b04_uart_puts(" POST_A0/9C=") ||
	    !b04_uart_put_hex(csi_tuple->cpu_a0, 8) || !b04_uart_putc('/') ||
	    !b04_uart_put_hex(csi_tuple->cpu_9c, 8) ||
	    !b04_uart_puts("\r\n"))
		return B06V6_AUTO_UART_ERROR;
#endif

	status = x58_vendor_accept_csi_result(0, 0, 0x11,
#if CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
		expected_csi_raw_fnv,
#else
		B06VB_CSI_OBSERVATION_FNV,
#endif
		X58_VENDOR_EXPERIMENT_CONFIRMATION);
	if (!b06v0_emit_status("B06VB_HIGH_CSI_OBSERVATION_ACCEPT", status))
		return B06V6_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK)
		return b06v6_fallback("B06VB_HIGH_CSI_OBSERVATION_ACCEPT");
	status = x58_vendor_b06vb_authorize_high_qpi_minit(
		X58_VENDOR_EXPERIMENT_CONFIRMATION);
	if (!b06v0_emit_status("B06VB_HIGH_QPI_MINIT_AUTHORIZE", status))
		return B06V6_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK)
		return b06v6_fallback("B06VB_HIGH_QPI_MINIT_AUTHORIZE");
	status = x58_vendor_runtime_probe(&info);
	if (status != X58_VENDOR_OK || !info.csi_result_accepted ||
	    !info.b06vb_high_qpi_minit_authorized ||
#if CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
	    info.csi_state_digest != expected_csi_raw_fnv)
#else
	    info.csi_state_digest != B06VB_CSI_OBSERVATION_FNV)
#endif
		return b06v6_fallback("B06VB_HIGH_CSI_AUTH_READBACK");
	if (!b04_uart_puts(
		"[QPI] HIGH_CSI_OBSERVATION=ACCEPTED_FOR_ONE_MINIT_CALL; not semantic CSI success\r\n"))
		return B06V6_AUTO_UART_ERROR;

	/*
	 * CSI EAX=0 plus diagnostic-invalid CMOS already constructs a cold-policy
	 * base.  Apply the same seven reviewed edits directly; do not call the
	 * B06V5 tuple-2/2/106 cold-conversion helper.
	 */
	if (!b06v0_policy_reset(state))
		return B06V6_AUTO_UART_ERROR;
	policy_base_fnv = b06v0_policy_digest(state->vendor_policy);
	if (!state->vendor_policy_ready || state->vendor_policy_modified ||
	    policy_base_fnv != B06VB_POLICY_BASE_FNV)
		return b06v6_fallback("B06VB_POLICY_BASE_GATE");
	for (size_t index = 0; index < ARRAY_SIZE(b06v6_candidate_edits); index++)
		state->vendor_policy[b06v6_candidate_edits[index].offset] =
			b06v6_candidate_edits[index].value;
	state->vendor_policy_modified = true;
	policy_final_fnv = b06v0_policy_digest(state->vendor_policy);
	if (policy_final_fnv != X58_B06V6_EXPECTED_POLICY_FNV ||
	    state->vendor_policy[B06V5_POLICY_STATUS_OFFSET] != 0 ||
	    b06v0_policy_read32(state->vendor_policy,
		B06V5_POLICY_FLAGS_OFFSET) != B06V6_POLICY_COLD_FLAGS)
		return b06v6_fallback("B06VB_POLICY_FINAL_GATE");
	if (!b04_uart_puts("[RAMINIT] B06VB POLICY BASE_FNV=") ||
	    !b04_uart_put_hex(policy_base_fnv, 8) ||
	    !b04_uart_puts(" SEVEN_EDIT_FNV=") ||
	    !b04_uart_put_hex(policy_final_fnv, 8) ||
	    !b04_uart_puts(" FORCE_COLD_HELPER=00\r\n"))
		return B06V6_AUTO_UART_ERROR;

	status = x58_vendor_install_confirmed_minit_policy(
		state->vendor_policy, sizeof(state->vendor_policy),
		X58_B06V6_EXPECTED_POLICY_FNV,
		X58_VENDOR_EXPERIMENT_CONFIRMATION);
	if (!b06v0_emit_status("B06VB_POLICY_INSTALL", status))
		return B06V6_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK)
		return b06v6_fallback("B06VB_POLICY_INSTALL");
	status = x58_vendor_runtime_probe(&info);
	if (status != X58_VENDOR_OK || !info.minit_policy_confirmed ||
	    info.minit_policy_digest != X58_B06V6_EXPECTED_POLICY_FNV ||
	    info.minit_policy_current_digest != X58_B06V6_EXPECTED_POLICY_FNV)
		return b06v6_fallback("B06VB_POLICY_INSTALL_READBACK");

	status = x58_vendor_arm_minit(X58_VENDOR_EXPERIMENT_CONFIRMATION);
	if (!b06v0_emit_status("B06VB_MINIT_ARM", status))
		return B06V6_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK)
		return b06v6_fallback("B06VB_MINIT_ARM");
	b06v6_capture_i801_signature(&i801);
	pre_minit_i801_consumed = b06v6_i801_signature_matches(&i801,
		&b06v8_consumed_signature);
	if (!b04_uart_puts("[RAMINIT] PRE_MINIT_I801=") ||
	    !b04_uart_put_hex(i801.control, 2) || !b04_uart_putc(':') ||
	    !b04_uart_put_hex(i801.command, 2) || !b04_uart_putc(':') ||
	    !b04_uart_put_hex(i801.xmit_address, 2) || !b04_uart_putc(':') ||
	    !b04_uart_put_hex(i801.data0, 2) || !b04_uart_putc(':') ||
	    !b04_uart_put_hex(i801.data1, 2) ||
	    !b04_uart_puts(" EXPECT_CONSUMED_08:42:bd:7a:85_MATCH=") ||
	    !b04_uart_put_hex(pre_minit_i801_consumed, 2) ||
	    !b04_uart_puts(" CMOS0E=") ||
	    !b04_uart_put_hex(b06v6_read_cmos_diagnostic(), 2) ||
	    !b04_uart_puts("\r\n"))
		return B06V6_AUTO_UART_ERROR;
	if (!pre_minit_i801_consumed ||
	    b06v6_read_cmos_diagnostic() != B06V6_CMOS_GUARD_IN_PROGRESS)
		return b06v6_fallback("B06VB_PRE_MINIT_PERSISTENCE_GATE");
	if (!b04_uart_puts(
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
		"[RAMINIT] B06VL BROAD_UNSAFE ONE_MINIT_CALL=AUTHORIZED; not trained; I801=08:64:9b:5c:a3 CMOS0E=ec; POST=d3/d4; reset/HLT/hang possible\r\n") ||
#else
		"[RAMINIT] B06VB_MINIT_IN_PROGRESS I801=08:64:9b:5c:a3 CMOS0E=ec; POST=d3/d4; reset/HLT/hang possible\r\n") ||
#endif
	    !b04_uart_wait_for(UART8250_LSR_TEMT,
		B03_UART_FLUSH_POLL_LIMIT))
		return B06V6_AUTO_UART_ERROR;

	/* No fallible serial operation may separate this phase commit from d3. */
	if (!b06v6_set_i801_signature(&b06vb_minit_in_progress_signature))
		b06v8_phase_guard_stop("B06VB_MINIT_SIGNATURE_WRITE");
	if (b06v6_read_cmos_diagnostic() != B06V6_CMOS_GUARD_IN_PROGRESS)
		b06v8_phase_guard_stop("B06VB_CMOS_GUARD_LOST_BEFORE_MINIT");
	outb(POST_B06V0_MINIT_CALL, CONFIG_POST_IO_PORT);
	status = x58_vendor_call_minit(&minit_result);
	outb(POST_B06V0_MINIT_RETURN, CONFIG_POST_IO_PORT);

	/*
	 * Capture the host-register state immediately.  B06VH separately accepts
	 * only MINIT's measured return tuple.  Persistent CMOS 0xec still rejects
	 * reset continuation; B06VL defers rearm until all post-memory tests pass.
	 */
	b06v6_capture_i801_signature(&i801);
#if CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
	post_minit_i801_exact = b06v6_i801_signature_matches(&i801,
		&b06vh_minit_return_signature);
#else
	post_minit_i801_exact = b06v6_i801_signature_matches(&i801,
		&b06vb_minit_in_progress_signature);
#endif
	post_minit_cmos = b06v6_read_cmos_diagnostic();
	if (!b06v0_emit_status("B06VB_MINIT_CALL", status) ||
	    !b06v0_print_call_result("B06VB_MINIT", &minit_result) ||
#if CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
	    !b04_uart_puts("[RAMINIT] POST_MINIT_I801_RAW=") ||
	    !b04_uart_put_hex(i801.control, 2) || !b04_uart_putc(':') ||
	    !b04_uart_put_hex(i801.command, 2) || !b04_uart_putc(':') ||
	    !b04_uart_put_hex(i801.xmit_address, 2) || !b04_uart_putc(':') ||
	    !b04_uart_put_hex(i801.data0, 2) || !b04_uart_putc(':') ||
	    !b04_uart_put_hex(i801.data1, 2) ||
	    !b04_uart_puts(" EXPECT_RETURN_08:00:5d:01:a3_MATCH=") ||
#else
	    !b04_uart_puts("[RAMINIT] POST_MINIT_GUARD I801_EXACT=") ||
#endif
	    !b04_uart_put_hex(post_minit_i801_exact, 2) ||
	    !b04_uart_puts(" CMOS0E=") ||
	    !b04_uart_put_hex(post_minit_cmos, 2) ||
	#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	    !b04_uart_puts(
		" AUTO_CLEAR=00 REARM_AFTER_FULL_POSTMEM=01 BROAD_UNSAFE=01\r\n"))
	#elif CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	    !b04_uart_puts(b06vi_o_candidate ?
		" AUTO_CLEAR=00 REARM_AFTER_FULL_POSTMEM=01\r\n" :
		" AUTO_CLEAR=00 REARM_AFTER_PRIMARY_GATE=01\r\n"))
	#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
	    !b04_uart_puts(" AUTO_CLEAR=00 REARM_AFTER_PRIMARY_GATE=01\r\n"))
#else
	    !b04_uart_puts(" AUTO_CLEAR=00\r\n"))
#endif
		return B06V6_AUTO_UART_ERROR;

	final_csi_state = x58_vendor_csi_state_snapshot();
	policy = x58_vendor_minit_policy();
	workspace = x58_vendor_minit_workspace();
#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	if (csi_profile == B06VG_CSI_OBSERVATION
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	    && csi_profile != B06VL_CSI_BROAD_HARD_GATED
#endif
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	    && !b06vi_o_candidate
#endif
	    ) {
		(void)x58_vendor_runtime_probe(&info);
		if (workspace != NULL)
			workspace_fnv = b06v0_buffer_digest(workspace,
				X58_VENDOR_MINIT_WORKSPACE_SIZE);
		b06v8_capture_qpi_tuple(&post_minit_tuple);
		b06vb_capture_stage_fingerprints(&post_minit_stage);
		if (!b06v0_print_runtime(state) ||
		    !b04_uart_puts(
			"[RAMINIT] B06VG OBSERVATION RETURN POLICY_IN/NOW=") ||
		    !b04_uart_put_hex(info.minit_policy_digest, 8) ||
		    !b04_uart_putc('/') ||
		    !b04_uart_put_hex(info.minit_policy_current_digest, 8) ||
		    !b04_uart_puts(" WORK_FNV=") ||
		    !b04_uart_put_hex(workspace_fnv, 8) ||
		    !b04_uart_puts(" DRAM_ACCESSES=00\r\n") ||
		    !b06v8_print_qpi_tuple("POST_MINIT", &post_minit_tuple) ||
		    !b06vb_print_stage_fingerprints("POST_MINIT",
			&post_minit_stage, csi_result))
			return B06V6_AUTO_UART_ERROR;

		full_path_observed = status == X58_VENDOR_OK &&
			workspace != NULL && policy != NULL &&
			final_csi_state != NULL && info.canaries_valid &&
			info.csi_result_accepted &&
			info.b06vb_high_qpi_minit_authorized &&
			info.minit_policy_confirmed && info.minit_call_attempted &&
			info.minit_returned &&
			info.minit_policy_digest == X58_B06V6_EXPECTED_POLICY_FNV &&
			info.minit_policy_current_digest ==
				X58_B06V6_EXPECTED_POLICY_FNV &&
			minit_result.eax == 0 && workspace[1] == 0 &&
			workspace[2] == 0 &&
			workspace[B06V5_WORK_B3_FLAGS_OFFSET] == 0x02 &&
			workspace[B06V5_WORK_COMPLETE_OFFSET] == 0x01 &&
			policy[B06V5_POLICY_STATUS_OFFSET] == 0 &&
			b06vb_minit_result_is_self_consistent(&minit_result, &info);
		if (!b04_uart_puts(
			"[RAMINIT] B06VG MINIT_OBSERVATION RESULT=") ||
		    !b04_uart_puts(full_path_observed ?
			"FULL_PATH_RETURN_DRAM_UNTESTED" :
			"RETURNED_UNKNOWN_OR_INCOMPLETE_DRAM_UNTESTED") ||
		    !b04_uart_puts(
			"; terminal profile; no success gate/handoff\r\n") ||
		    !b06vb_print_complete_buffer("CSI", final_csi_state,
			X58_VENDOR_CSI_STATE_SIZE) ||
		    !b06vb_print_complete_buffer("WORK", workspace,
			X58_VENDOR_MINIT_WORKSPACE_SIZE) ||
		    !b04_uart_puts(
			"[RAMINIT] B06VG_OBSERVATION_TERMINAL; guard retained; no ordinary DRAM, postcar, ramstage or payload\r\n") ||
		    !b04_uart_wait_for(UART8250_LSR_TEMT,
			B03_UART_FLUSH_POLL_LIMIT))
			return B06V6_AUTO_UART_ERROR;
		outb(POST_B06V8_TERMINAL, CONFIG_POST_IO_PORT);

		return b06v6_fallback("B06VG_OBSERVATION_MINIT_TERMINAL");
	}
	if (csi_profile != B06VG_CSI_PRIMARY
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	    && csi_profile != B06VL_CSI_BROAD_HARD_GATED
#endif
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	    && !b06vi_o_candidate
#endif
	    )
		return b06v6_fallback("B06VG_PRIMARY_PROFILE_REQUIRED");
#endif
	return b06ve_promote_post_minit(state, status, csi_result,
		&minit_result, final_csi_state, policy, workspace,
		post_minit_i801_exact, post_minit_cmos
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
		, saved_pre_a0, csi_tuple
#endif
		);
#endif
	(void)x58_vendor_runtime_probe(&info);
	if (workspace != NULL)
		workspace_fnv = b06v0_buffer_digest(workspace,
			X58_VENDOR_MINIT_WORKSPACE_SIZE);
	b06v8_capture_qpi_tuple(&post_minit_tuple);
	b06vb_capture_stage_fingerprints(&post_minit_stage);
	if (!b06v0_print_runtime(state) ||
	    !b04_uart_puts("[RAMINIT] B06VB RETURN POLICY_IN/NOW=") ||
	    !b04_uart_put_hex(info.minit_policy_digest, 8) ||
	    !b04_uart_putc('/') ||
	    !b04_uart_put_hex(info.minit_policy_current_digest, 8) ||
	    !b04_uart_puts(" WORK_FNV=") ||
	    !b04_uart_put_hex(workspace_fnv, 8) ||
	    !b04_uart_puts(" DRAM_ACCESSES=00\r\n") ||
	    !b06v8_print_qpi_tuple("POST_MINIT", &post_minit_tuple) ||
	    !b06vb_print_stage_fingerprints("POST_MINIT", &post_minit_stage,
		csi_result))
		return B06V6_AUTO_UART_ERROR;

	full_path_observed = status == X58_VENDOR_OK &&
		workspace != NULL && policy != NULL && final_csi_state != NULL &&
		info.canaries_valid && info.csi_result_accepted &&
		info.b06vb_high_qpi_minit_authorized &&
		info.minit_policy_confirmed && info.minit_call_attempted &&
		info.minit_returned &&
		info.minit_policy_digest == X58_B06V6_EXPECTED_POLICY_FNV &&
		info.minit_policy_current_digest == X58_B06V6_EXPECTED_POLICY_FNV &&
		minit_result.eax == 0 && workspace[1] == 0 && workspace[2] == 0 &&
		workspace[B06V5_WORK_B3_FLAGS_OFFSET] == 0x02 &&
		workspace[B06V5_WORK_COMPLETE_OFFSET] == 0x01 &&
		policy[B06V5_POLICY_STATUS_OFFSET] == 0 &&
		b06vb_minit_result_is_self_consistent(&minit_result, &info);
	if (!b04_uart_puts("[RAMINIT] B06VB MINIT_OBSERVATION RESULT=") ||
	    !b04_uart_puts(full_path_observed ?
		"FULL_PATH_RETURN_DRAM_UNTESTED" :
		"RETURNED_UNKNOWN_OR_INCOMPLETE_DRAM_UNTESTED") ||
	    !b04_uart_puts("; observational only; no success gate/handoff\r\n") ||
	    !b06vb_print_complete_buffer("CSI", final_csi_state,
		X58_VENDOR_CSI_STATE_SIZE) ||
	    !b06vb_print_complete_buffer("WORK", workspace,
		X58_VENDOR_MINIT_WORKSPACE_SIZE) ||
	    !b04_uart_puts(
		"[RAMINIT] B06VB_TERMINAL; guard retained; no ordinary DRAM, postcar or ramstage\r\n") ||
	    !b04_uart_wait_for(UART8250_LSR_TEMT,
		B03_UART_FLUSH_POLL_LIMIT))
		return B06V6_AUTO_UART_ERROR;
	outb(POST_B06V8_TERMINAL, CONFIG_POST_IO_PORT);

	return b06v6_fallback("B06VB_MINIT_OBSERVATION_TERMINAL");
}
#endif

static void __noreturn b06v8_issue_outer_syre_reset(void)
{
	uint32_t syre = b06j_pci_read(B06V8_IOH_SYRE_DEV, 0xcc, 4);

	/*
	 * Exact MSI caller sequence at fffc1330..fffc137c; never use CF9 here.
	 * The immutable live trace entered with bit 10 set.  Every mutation is
	 * bounded by an exact last-moment readback; ambiguity is a terminal stop.
	 */
	if (syre != 0x00000600)
		b06v8_phase_guard_stop("B06V8_SYRE_PRE_EDGE_GATE");
	b06j_pci_write(B06V8_IOH_SYRE_DEV, 0xcc,
		syre & ~B06V8_IOH_SYRE_REQUEST, 4);
	syre = b06j_pci_read(B06V8_IOH_SYRE_DEV, 0xcc, 4);
	if (syre != 0x00000200)
		b06v8_phase_guard_stop("B06V8_SYRE_CLEAR_READBACK");
	b06j_pci_write(B06V8_IOH_SR_DEV, 0x7c, 0, 4);
	b06j_pci_write(B06V8_IOH_SR_DEV, 0x80, 0, 4);
	if (b06j_pci_read(B06V8_IOH_SR_DEV, 0x7c, 4) != 0 ||
	    b06j_pci_read(B06V8_IOH_SR_DEV, 0x80, 4) != 0)
		b06v8_phase_guard_stop("B06V8_SR_CLEAR_READBACK");
	syre = b06j_pci_read(B06V8_IOH_SYRE_DEV, 0xcc, 4);
	if (syre != 0x00000200)
		b06v8_phase_guard_stop("B06V8_SYRE_FINAL_PRE_EDGE_GATE");
	outb(POST_B06V8_OUTER_RESET_ARMED, CONFIG_POST_IO_PORT);
	outb(POST_B06V8_IOH_SYRE, CONFIG_POST_IO_PORT);
	b06j_pci_write(B06V8_IOH_SYRE_DEV, 0xcc,
		syre | B06V8_IOH_SYRE_REQUEST, 4);

	/* A successful request resets the CPU; never mutate persistence afterward. */
	asm volatile("cli" ::: "memory");
	for (;;)
		asm volatile("hlt");
}

static enum b06v6_auto_result b06v8_high_qpi_probe(
	struct b06j_state *state, enum b06v3_smbus_entry_state smbus_entry,
	const struct b06v6_i801_signature *entry_signature
#if CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
	, const struct b06v9_rtc_upper_bank_state *rtc_state
#endif
	)
{
	struct x58_vendor_call_result result = { 0 };
	struct x58_vendor_runtime_info info = { 0 };
	struct b06v8_qpi_tuple tuple;
#if !CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
	struct b06v8_cmos_inputs cmos_inputs;
#endif
	const struct b06v6_i801_signature *next_signature;
	const uint8_t *csi_state;
	enum x58_vendor_status call_status;
	enum x58_vendor_status probe_status;
	enum b06v8_phase phase;
	uint8_t cmos_guard = b06v6_read_cmos_diagnostic();
	uint32_t csi_raw_fnv = 0;
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	uint32_t saved_pre_a0 = 0;
#endif

	outb(POST_B06V6_AUTO_BEGIN, CONFIG_POST_IO_PORT);
	if (!b04_uart_puts(
#if CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
		"[QPI] B06VL BROAD/UNSAFE seven-A0 hard-gated SeaBIOS attempt; "
		"workspace hashes telemetry-only; local hash-pinned modules; default-off\r\n") ||
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
		"[QPI] B06VK profile-Q canonical-class/O/PRIMARY SeaBIOS gate; "
		"local hash-pinned modules; default-off\r\n") ||
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
		"[QPI] B06VJ coupled profile-Q/O/PRIMARY SeaBIOS gate; "
		"local hash-pinned modules; default-off\r\n") ||
#else
		"[QPI] B06VI coupled profile-O/PRIMARY SeaBIOS gate; "
		"local hash-pinned modules; default-off\r\n") ||
#endif
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
		"[QPI] B06VH exact PRIMARY workspace/rearm SeaBIOS gate; "
		"local hash-pinned modules; default-off\r\n") ||
#elif CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
		"[QPI] B06VG coupled High-QPI MINIT/SeaBIOS gate; "
		"local hash-pinned modules; default-off\r\n") ||
#elif CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
		"[QPI] B06VE exact byte-08 High-QPI MINIT automatic handoff; "
		"local hash-pinned modules; default-off\r\n") ||
#elif CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
		"[QPI] B06VD five-value pre-pass-three A0 set plus paired CSI2A6 "
		"MINIT observation; local hash-pinned modules; default-off\r\n") ||
#elif CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT
		"[QPI] B06VC four-value pre-pass-three A0 set plus exact B06VB "
		"MINIT observation; local hash-pinned modules; default-off\r\n") ||
#elif CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
		"[QPI] B06VB exact High-QPI MINIT observation; local hash-pinned modules; default-off\r\n") ||
#elif CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET
		"[QPI] B06VA High-QPI A0-allowlist three-pass probe; local hash-pinned wrapper; default-off\r\n") ||
#else
		"[QPI] B06V9 deterministic High-QPI three-pass probe; local hash-pinned wrapper; default-off\r\n") ||
#endif
#else
		"[QPI] B06V8 High-QPI three-pass probe; local patched wrapper; default-off\r\n") ||
#endif
	    !b04_uart_puts("[QPI] ENTRY=") ||
	    !b04_uart_puts(smbus_entry == B06V3_SMBUS_ENTRY_COLD ?
		"COLD_DEFAULT" : "CONFIGURED_AFTER_RESET") ||
	    !b04_uart_puts(" I801_SIG=") ||
	    !b04_uart_put_hex(entry_signature->control, 2) ||
	    !b04_uart_putc(':') ||
	    !b04_uart_put_hex(entry_signature->command, 2) ||
	    !b04_uart_putc(':') ||
	    !b04_uart_put_hex(entry_signature->xmit_address, 2) ||
	    !b04_uart_putc(':') ||
	    !b04_uart_put_hex(entry_signature->data0, 2) ||
	    !b04_uart_putc(':') ||
	    !b04_uart_put_hex(entry_signature->data1, 2) ||
	    !b04_uart_puts(" CMOS0E=") || !b04_uart_put_hex(cmos_guard, 2) ||
	    !b04_uart_puts("\r\n") ||
	    !b06v0_emit_status("CAR_PREPARE", state->vendor_prepare_status))
		return B06V6_AUTO_UART_ERROR;
	if (!state->vendor_runtime_ready)
		return b06v6_fallback("B06V8_CAR_PREPARE");
#if CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
	if (rtc_state == NULL || !rtc_state->enabled)
		return b06v6_fallback("B06V9_RTC_UPPER_BANK_GATE");
#endif

	if (smbus_entry == B06V3_SMBUS_ENTRY_COLD) {
		if (cmos_guard == B06V6_CMOS_GUARD_IN_PROGRESS)
			return b06v6_fallback("B06V8_PERSISTENT_RESET_LOOP_GUARD");
		if (cmos_guard != B06V8_CMOS_COLD_AUTHORIZATION)
			return b06v6_fallback("B06V8_CMOS_DIAGNOSTIC_GATE");
		if (b06v6_i801_signature_matches(entry_signature,
			&b06v8_pass2_signature) ||
		    b06v6_i801_signature_matches(entry_signature,
			&b06v8_consumed_signature) ||
		    b06v6_i801_signature_matches(entry_signature,
			&b06v8_pass3_signature)
#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
		    || b06v6_i801_signature_matches(entry_signature,
			&b06vb_minit_in_progress_signature)
#endif
		   )
			return b06v6_fallback("B06V8_COLD_RETAINED_PHASE_SIGNATURE");
		phase = B06V8_PHASE_PASS1;
	} else {
		if (cmos_guard != B06V6_CMOS_GUARD_IN_PROGRESS)
			return b06v6_fallback("B06V8_SIGNATURE_WITHOUT_CMOS_GUARD");
		if (b06v6_i801_signature_matches(entry_signature,
			&b06v8_pass2_signature))
			phase = B06V8_PHASE_PASS2;
		else if (b06v6_i801_signature_matches(entry_signature,
			&b06v8_pass3_signature))
			phase = B06V8_PHASE_PASS3;
		else if (b06v6_i801_signature_matches(entry_signature,
			&b06v8_consumed_signature))
			return b06v6_fallback("B06V8_ONE_SHOT_CONSUMED");
#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
		else if (b06v6_i801_signature_matches(entry_signature,
			&b06vb_minit_in_progress_signature))
			return b06v6_fallback("B06VB_MINIT_RESET_LOOP_GUARD");
#endif
		else
			return b06v6_fallback("B06V8_UNKNOWN_PHASE_SIGNATURE");
	}

	if (!b04_uart_puts("[QPI] PHASE=") ||
	    !b04_uart_put_hex(phase, 2) || !b04_uart_puts("\r\n"))
		return B06V6_AUTO_UART_ERROR;
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
	if (phase == B06V8_PHASE_PASS3) {
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
		if (!b06vg_capture_stable_pre_a0(&saved_pre_a0))
			return b06v6_fallback("B06VG_PRE_A0_STABILITY_GATE");
#elif CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
		const uint32_t cpu_a0 = b06j_pci_read(B06V8_CPU_QPI_PHY_DEV,
			0xa0, 4);

		if (!b04_uart_puts("[QPI] PASS3_PRESELECT CPU_A0=") ||
		    !b04_uart_put_hex(cpu_a0, 8) ||
		    !b04_uart_puts(
			" EXACT_SET=00017000|00017600|00017800|00017a00|00017c00; DUAL_GATE_FOLLOWS\r\n"))
			return B06V6_AUTO_UART_ERROR;
#elif CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT
		const uint32_t cpu_a0 = b06j_pci_read(B06V8_CPU_QPI_PHY_DEV,
			0xa0, 4);

		if (!b04_uart_puts("[QPI] PASS3_PRESELECT CPU_A0=") ||
		    !b04_uart_put_hex(cpu_a0, 8) ||
		    !b04_uart_puts(
			" EXACT_SET=00017000|00017600|00017800|00017a00 MATCH=") ||
		    !b04_uart_puts(
			x58_vendor_b06vc_high_qpi_cpu_a0_exact(cpu_a0) ?
			"PASS\r\n" : "FAIL\r\n"))
			return B06V6_AUTO_UART_ERROR;
#elif CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET
		const uint32_t cpu_a0 = b06j_pci_read(B06V8_CPU_QPI_PHY_DEV,
			0xa0, 4);

		if (!b04_uart_puts("[QPI] PASS3_PRESELECT CPU_A0=") ||
		    !b04_uart_put_hex(cpu_a0, 8) ||
		    !b04_uart_puts(" EXACT_SET=00017000|00017600 MATCH=") ||
		    !b04_uart_puts(x58_vendor_high_qpi_cpu_a0_exact(cpu_a0) ?
			"PASS\r\n" : "FAIL\r\n"))
			return B06V6_AUTO_UART_ERROR;
#endif
		call_status = x58_vendor_b06v8_select_high_qpi_csi(
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
			saved_pre_a0,
#endif
			X58_VENDOR_EXPERIMENT_CONFIRMATION);
		if (!b06v0_emit_status(
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
			"B06VG_COUPLED_HIGH_CSI_PROFILE",
#elif CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
			"B06VE_HIGH_CSI_PROFILE",
#elif CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
			"B06VD_HIGH_CSI_PROFILE",
#elif CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT
			"B06VC_HIGH_CSI_PROFILE",
#elif CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
			"B06VB_HIGH_CSI_PROFILE",
#elif CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET
			"B06VA_HIGH_CSI_PROFILE",
#else
			"B06V8_HIGH_CSI_PROFILE",
#endif
			call_status))
			return B06V6_AUTO_UART_ERROR;
		if (call_status != X58_VENDOR_OK)
			return b06v6_fallback(
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
				"B06VG_COUPLED_HIGH_CSI_PROFILE_GATE"
#elif CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
				"B06VE_HIGH_CSI_PROFILE_GATE"
#elif CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
				"B06VD_HIGH_CSI_PROFILE_GATE"
#elif CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT
				"B06VC_HIGH_CSI_PROFILE_GATE"
#elif CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
				"B06VB_HIGH_CSI_PROFILE_GATE"
#elif CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET
				"B06VA_HIGH_CSI_PROFILE_GATE"
#else
				"B06V8_HIGH_CSI_PROFILE_GATE"
#endif
				);
#if CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET
		outb(POST_B06VA_HIGH_PROFILE_READY, CONFIG_POST_IO_PORT);
#endif
	}
#endif
	if (!b06j_run_spd(state))
		return B06V6_AUTO_UART_ERROR;
	if (!state->vendor_spd_valid ||
	    !b06v6_spd_is_exact(state) ||
	    state->vendor_spd_topology_status != 0 ||
	    state->vendor_spd_ackmap != B06V0_TARGET_SPD_BITMAP ||
	    state->vendor_spd_ddr3map != B06V0_TARGET_SPD_BITMAP)
		return b06v6_fallback("B06V8_SPD_EXACT_GATE");

	if (!b06v0_prepare_pciexbar(state))
		return B06V6_AUTO_UART_ERROR;
	if (!state->vendor_pciexbar_ready)
		return b06v6_fallback("B06V8_PCIEXBAR_PLATFORM_GATE");
	if (b06j_mem_read(0xe0000008u, 4) != 0x06000013u)
		return b06v6_fallback("B06V8_X58_REVISION_GATE");
#if CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
	if (!b04_uart_puts(
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
		"[QPI] B06VG_PROFILE=STATE06/07:01/01 PAIRS:08/03e3d24e|0c/8b38506a; "
#elif CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
		"[QPI] B06VE_PROFILE=STATE06/07:01/01 PAIRS:00/01; "
#elif CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
		"[QPI] B06VD_PROFILE=STATE06/07:01/01 PAIRS:00/01; "
#elif CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT
		"[QPI] B06VC_PROFILE=STATE06/07:01/01 PAIRS:00/01; "
#elif CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
		"[QPI] B06VB_PROFILE=STATE06/07:01/01 PAIRS:00/01; "
#elif CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET
		"[QPI] B06VA_PROFILE=STATE06/07:01/01 PAIRS:00/01; "
#else
		"[QPI] B06V9_PROFILE=STATE06/07:01/01 PAIRS:00/01; "
#endif
		"RTC_INPUT_DEPENDENCY=NONE U128E=PASS\r\n"))
		return B06V6_AUTO_UART_ERROR;
	outb(POST_B06V9_PROFILE_READY, CONFIG_POST_IO_PORT);
#else
	b06v8_capture_cmos_inputs(&cmos_inputs);
	if (!b06v8_print_cmos_inputs(&cmos_inputs))
		return B06V6_AUTO_UART_ERROR;
	if (!b06v8_cmos_inputs_exact(&cmos_inputs))
		return b06v6_fallback("B06V8_EXT_CMOS_INPUT_GATE");
#endif
	outb(POST_B06V6_PLATFORM_READY, CONFIG_POST_IO_PORT);
	b06v8_capture_qpi_tuple(&tuple);
	if (!b06v8_print_qpi_tuple("ENTRY", &tuple))
		return B06V6_AUTO_UART_ERROR;
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	if (phase == B06V8_PHASE_PASS3 && tuple.cpu_a0 != saved_pre_a0)
		return b06v6_fallback("B06VG_PRE_CSI_A0_DRIFT");
#endif
	if (phase == B06V8_PHASE_PASS3 && !b06v8_high_qpi_tuple_exact(&tuple))
		return b06v6_fallback("B06V8_HIGH_QPI_ENTRY_GATE");
	if (phase != B06V8_PHASE_PASS3 &&
	    !b06v8_slow_qpi_entry_tuple_exact(&tuple, phase))
		return b06v6_fallback("B06V8_SLOW_QPI_ENTRY_GATE");

	call_status = x58_vendor_arm_csi_wrapper(0,
		X58_VENDOR_EXPERIMENT_CONFIRMATION);
	if (!b06v0_emit_status("B06V8_CSI_ARM", call_status))
		return B06V6_AUTO_UART_ERROR;
	if (call_status != X58_VENDOR_OK)
		return b06v6_fallback("B06V8_CSI_ARM");

	next_signature = phase == B06V8_PHASE_PASS1 ?
		&b06v8_pass2_signature : &b06v8_consumed_signature;
	if (!b04_uart_puts(phase == B06V8_PHASE_PASS1 ?
		"[QPI] CSI_PASS=1; expecting CSI-internal SYRE reset; no caller reset\r\n" :
		(phase == B06V8_PHASE_PASS2 ?
		 "[QPI] CSI_PASS=2; one-shot consumed before call; exact return required\r\n" :
		 "[QPI] CSI_PASS=3; one-shot consumed; return tuple intentionally unconstrained\r\n")) ||
	    !b04_uart_wait_for(UART8250_LSR_TEMT,
		B03_UART_FLUSH_POLL_LIMIT))
		return B06V6_AUTO_UART_ERROR;
	/* No fallible return may separate persistent phase commit from the call. */
	if (!b06v6_write_cmos_diagnostic(B06V6_CMOS_GUARD_IN_PROGRESS))
		b06v8_phase_guard_stop("B06V8_CMOS_GUARD_WRITE");
	if (!b06v6_set_i801_signature(next_signature))
		b06v8_phase_guard_stop("B06V8_I801_PHASE_WRITE");
	outb(phase == B06V8_PHASE_PASS1 ? POST_B06V6_CSI_PASS1_ARMED :
		(phase == B06V8_PHASE_PASS2 ? POST_B06V6_CSI_PASS2_ARMED :
		 POST_B06V8_CSI_PASS3_ARMED), CONFIG_POST_IO_PORT);
	outb(POST_B06V0_CSI_CALL, CONFIG_POST_IO_PORT);
	call_status = x58_vendor_call_csi_wrapper(&result);
	outb(phase == B06V8_PHASE_PASS3 ? POST_B06V8_CSI_PASS3_RETURN :
		POST_B06V0_CSI_RETURN, CONFIG_POST_IO_PORT);
	state->vendor_policy_ready = false;
	state->vendor_policy_modified = false;

	/* A pass-one return must first consume the retained pass-two signature. */
	if (phase == B06V8_PHASE_PASS1 &&
	    !b06v6_set_i801_signature(&b06v8_consumed_signature))
		b06v8_phase_guard_stop("B06V8_PASS1_RETURN_CONSUME");
	if (b06v6_read_cmos_diagnostic() != B06V6_CMOS_GUARD_IN_PROGRESS)
		b06v8_phase_guard_stop("B06V8_CMOS_GUARD_LOST_AFTER_CSI");
	if (!b06v0_emit_status("B06V8_CSI_CALL", call_status) ||
	    !b06v0_print_call_result("B06V8_CSI", &result))
		return B06V6_AUTO_UART_ERROR;
	if (phase == B06V8_PHASE_PASS1)
		return b06v6_fallback("B06V8_CSI_PASS1_RETURNED");

	probe_status = x58_vendor_runtime_probe(&info);
	csi_state = x58_vendor_csi_state_snapshot();
	if (!b06v0_emit_status("B06V8_CSI_PROBE", probe_status))
		return B06V6_AUTO_UART_ERROR;
	if (csi_state != NULL)
		csi_raw_fnv = b06v0_buffer_digest(csi_state,
			X58_VENDOR_CSI_STATE_SIZE);
	b06v8_capture_qpi_tuple(&tuple);
	if (!b04_uart_puts("[QPI] CSI_RAW_FNV=") ||
	    !b04_uart_put_hex(csi_raw_fnv, 8) ||
	    !b04_uart_puts(" STATE06/07/2a6/2ef/301=") ||
	    !(csi_state != NULL ?
		(b04_uart_put_hex(csi_state[0x06], 2) && b04_uart_putc('/') &&
		 b04_uart_put_hex(csi_state[0x07], 2) && b04_uart_putc('/') &&
		 b04_uart_put_hex(csi_state[0x2a6], 2) && b04_uart_putc('/') &&
		 b04_uart_put_hex(csi_state[0x2ef], 2) && b04_uart_putc('/') &&
		 b04_uart_put_hex(csi_state[0x301], 2)) :
		b04_uart_puts("NA")) ||
	    !b04_uart_puts("\r\n") ||
	    !b04_uart_puts("[QPI] CSI STATE1c/1d 70/71 cd/ce 121/122=") ||
	    !(csi_state != NULL ?
		(b04_uart_put_hex(csi_state[0x1c], 2) && b04_uart_putc('/') &&
		 b04_uart_put_hex(csi_state[0x1d], 2) && b04_uart_putc(' ') &&
		 b04_uart_put_hex(csi_state[0x70], 2) && b04_uart_putc('/') &&
		 b04_uart_put_hex(csi_state[0x71], 2) && b04_uart_putc(' ') &&
		 b04_uart_put_hex(csi_state[0xcd], 2) && b04_uart_putc('/') &&
		 b04_uart_put_hex(csi_state[0xce], 2) && b04_uart_putc(' ') &&
		 b04_uart_put_hex(csi_state[0x121], 2) && b04_uart_putc('/') &&
		 b04_uart_put_hex(csi_state[0x122], 2)) :
		b04_uart_puts("NA")) ||
	    !b04_uart_puts("\r\n") ||
	    !b06v8_print_qpi_tuple(phase == B06V8_PHASE_PASS2 ?
		"PASS2_RETURN" : "PASS3_RETURN", &tuple))
		return B06V6_AUTO_UART_ERROR;

	if (phase == B06V8_PHASE_PASS2) {
		if (call_status != X58_VENDOR_OK ||
		    probe_status != X58_VENDOR_OK || csi_state == NULL ||
		    !info.prepared || !info.canaries_valid ||
		    !info.wrapper_signature_valid || !info.csi_signature_valid ||
		    !info.csi_call_attempted || !info.csi_returned ||
		    info.csi_result_accepted || info.experimental_status_seed != 0 ||
		    result.eax != 1 || result.ebx != 0 || result.ecx != 0x2a6 ||
		    info.csi_state_digest != csi_raw_fnv ||
		    csi_state[0x06] != 1 || csi_state[0x07] != 1 ||
		    csi_state[0x1c] != 0 || csi_state[0x1d] != 1 ||
		    csi_state[0x70] != 0 || csi_state[0x71] != 1 ||
		    csi_state[0xcd] != 0 || csi_state[0xce] != 1 ||
		    csi_state[0x121] != 0 || csi_state[0x122] != 1 ||
		    csi_state[0x2ef] != 1 || csi_state[0x301] != 0 ||
		    !b06v8_result_is_self_consistent(&result, &info))
			return b06v6_fallback("B06V8_CSI_PASS2_ABI_GATE");
		if (!b06v8_pre_outer_reset_tuple_exact(&tuple))
			return b06v6_fallback("B06V8_PRE_OUTER_RESET_TUPLE_GATE");
		outb(POST_B06V8_PASS2_ACCEPTED, CONFIG_POST_IO_PORT);
		if (!b04_uart_puts(
			"[QPI] PASS2_ACCEPTED EAX/EBX/ECX=1/0/2a6; S3 commit follows; issuing exact IOH SYRE edge once\r\n") ||
		    !b04_uart_wait_for(UART8250_LSR_TEMT,
			B03_UART_FLUSH_POLL_LIMIT))
			return B06V6_AUTO_UART_ERROR;
		/* Commit S3 only after all serial work, immediately before SYRE. */
		if (!b06v6_set_i801_signature(&b06v8_pass3_signature))
			b06v8_phase_guard_stop("B06V8_PASS3_SIGNATURE_WRITE");
		b06v8_issue_outer_syre_reset();
	}

	/*
	 * Pass-three output is observational: no return-register value, raw state
	 * digest, or resulting endpoint tuple is promoted to a success meaning.
	 */
	if (!b04_uart_puts("[QPI] PASS3_RETURNED CALL_STATUS=") ||
	    !b04_uart_puts(x58_vendor_status_name(call_status)) ||
	    !b04_uart_puts(" PROBE_STATUS=") ||
	    !b04_uart_puts(x58_vendor_status_name(probe_status)) ||
	    !b04_uart_puts(" CANARIES=") ||
	    !b04_uart_put_hex(info.canaries_valid, 2) ||
	    !b04_uart_puts(" RESULT_SELF_CONSISTENT=") ||
	    !b04_uart_put_hex(b06v8_result_is_self_consistent(&result, &info), 2) ||
	    !b04_uart_puts("\r\n"))
		return B06V6_AUTO_UART_ERROR;
	if (csi_state != NULL) {
		if (!b06v8_print_csi_state(csi_state))
			return B06V6_AUTO_UART_ERROR;
	} else if (!b04_uart_puts(
		"[QPI] CSI_SNAPSHOT=UNAVAILABLE; call/runtime validation did not retain it\r\n")) {
		return B06V6_AUTO_UART_ERROR;
	}
#if CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
	return b06vb_high_qpi_minit_observe(state, &result, &info, csi_state,
		csi_raw_fnv, &tuple, call_status, probe_status
#if CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
		, saved_pre_a0
#endif
		);
#else
	if (!b04_uart_puts(
		"[QPI] PASS3_TERMINAL; no CSI acceptance, MINIT, caller reset, postcar or ramstage\r\n") ||
	    !b04_uart_wait_for(UART8250_LSR_TEMT,
		B03_UART_FLUSH_POLL_LIMIT))
		return B06V6_AUTO_UART_ERROR;
	outb(POST_B06V8_TERMINAL, CONFIG_POST_IO_PORT);

	return b06v6_fallback("B06V8_PASS3_TERMINAL");
#endif
}
#endif

static enum b06v6_auto_result __maybe_unused b06v6_auto_handoff(
	struct b06j_state *state, enum b06v3_smbus_entry_state smbus_entry,
	const struct b06v6_i801_signature *entry_signature)
{
	struct x58_vendor_call_result result = { 0 };
	struct x58_vendor_runtime_info info;
	const uint8_t *csi_state;
	const uint8_t *policy;
	const uint8_t *workspace;
	enum x58_vendor_status status;
	bool second_pass;
	uint8_t cmos_guard;
	uint32_t mc_mapper;
	uint32_t mc_common_f8;
	uint32_t ch2_dod;
	uint32_t ch2_ranks;
	uint32_t ch2_status;
	uint32_t qpi_status;
#if CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
	uint32_t csi_raw_fnv;
	uint32_t csi_canonical_fnv;
	uint32_t final_csi_raw_fnv;
	uint32_t final_csi_canonical_fnv;
	uint32_t workspace_raw_fnv;
	uint32_t workspace_canonical_fnv;
	uint8_t csi_dynamic_byte;
	uint8_t final_csi_dynamic_byte;
#endif
	unsigned int index;

	outb(POST_B06V6_AUTO_BEGIN, CONFIG_POST_IO_PORT);
	cmos_guard = b06v6_read_cmos_diagnostic();
	if (!b04_uart_puts(
#if CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
		"[RAMINIT] B06V7 robust Slow-QPI automatic path; local vendor-assisted, default-off\r\n") ||
#else
		"[RAMINIT] B06V6 exact automatic path; local vendor-assisted, default-off\r\n") ||
#endif
	    !b04_uart_puts("[RAMINIT] ENTRY=") ||
	    !b04_uart_puts(smbus_entry == B06V3_SMBUS_ENTRY_COLD ?
		"COLD_DEFAULT" : "CONFIGURED_AFTER_RESET") ||
	    !b04_uart_puts(" I801_SIG=") ||
	    !(smbus_entry == B06V3_SMBUS_ENTRY_COLD ?
		b04_uart_puts("NA") :
		(b04_uart_put_hex(entry_signature->control, 2) &&
		 b04_uart_putc(':') &&
		 b04_uart_put_hex(entry_signature->command, 2) &&
		 b04_uart_putc(':') &&
		 b04_uart_put_hex(entry_signature->xmit_address, 2) &&
		 b04_uart_putc(':') &&
		 b04_uart_put_hex(entry_signature->data0, 2) &&
		 b04_uart_putc(':') &&
		 b04_uart_put_hex(entry_signature->data1, 2))) ||
	    !b04_uart_puts(" CMOS0E=") ||
	    !b04_uart_put_hex(cmos_guard, 2) ||
	    !b04_uart_puts("\r\n") ||
	    !b06v0_emit_status("CAR_PREPARE", state->vendor_prepare_status))
		return B06V6_AUTO_UART_ERROR;
	if (!state->vendor_runtime_ready)
		return b06v6_fallback("CAR_PREPARE");

	if (smbus_entry == B06V3_SMBUS_ENTRY_COLD) {
		if (cmos_guard == B06V6_CMOS_GUARD_IN_PROGRESS)
			return b06v6_fallback("PERSISTENT_RESET_LOOP_GUARD");
		if (cmos_guard != B06V6_CMOS_DIAGNOSTIC_OBSERVED)
			return b06v6_fallback("CMOS_DIAGNOSTIC_GATE");
		second_pass = false;
	} else if (b06v6_i801_signature_matches(entry_signature,
		&b06v6_first_pass_signature)) {
		if (cmos_guard != B06V6_CMOS_GUARD_IN_PROGRESS)
			return b06v6_fallback("CSI_SIGNATURE_WITHOUT_CMOS_GUARD");
		second_pass = true;
	} else if (b06v6_i801_signature_matches(entry_signature,
		&b06v6_in_progress_signature)) {
		return b06v6_fallback("RESET_LOOP_GUARD");
	} else {
		return b06v6_fallback("CONFIGURED_WITHOUT_CSI_COOKIE");
	}

	if (!b06j_run_spd(state))
		return B06V6_AUTO_UART_ERROR;
	if (!state->vendor_spd_valid ||
	    !b06v6_spd_is_exact(state) ||
	    state->vendor_spd_topology_status != 0 ||
	    state->vendor_spd_ackmap != B06V0_TARGET_SPD_BITMAP ||
	    state->vendor_spd_ddr3map != B06V0_TARGET_SPD_BITMAP)
		return b06v6_fallback("SPD_EXACT_GATE");

	if (!b06v0_prepare_pciexbar(state))
		return B06V6_AUTO_UART_ERROR;
	if (!state->vendor_pciexbar_ready)
		return b06v6_fallback("PCIEXBAR_PLATFORM_GATE");
	outb(POST_B06V6_PLATFORM_READY, CONFIG_POST_IO_PORT);

	status = x58_vendor_arm_csi_wrapper(0,
		X58_VENDOR_EXPERIMENT_CONFIRMATION);
	if (!b06v0_emit_status("AUTO_CSI_ARM", status))
		return B06V6_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK)
		return b06v6_fallback("CSI_ARM");

	if (!b04_uart_puts(second_pass ?
		"[RAMINIT] CSI_PASS=2; complemented guard blocks another automatic call\r\n" :
		"[RAMINIT] CSI_PASS=1; complete I801 signature must survive the expected reset\r\n") ||
	    !b04_uart_wait_for(UART8250_LSR_TEMT,
		B03_UART_FLUSH_POLL_LIMIT))
		return B06V6_AUTO_UART_ERROR;
	if (!b06v6_write_cmos_diagnostic(B06V6_CMOS_GUARD_IN_PROGRESS))
		b06v6_guard_stop("CMOS_GUARD_WRITE");
	if (!b06v6_set_i801_signature(second_pass ?
		&b06v6_in_progress_signature : &b06v6_first_pass_signature))
		b06v6_guard_stop("I801_SIGNATURE_WRITE");

	outb(second_pass ? POST_B06V6_CSI_PASS2_ARMED :
		POST_B06V6_CSI_PASS1_ARMED, CONFIG_POST_IO_PORT);
	outb(POST_B06V0_CSI_CALL, CONFIG_POST_IO_PORT);
	status = x58_vendor_call_csi_wrapper(&result);
	outb(POST_B06V0_CSI_RETURN, CONFIG_POST_IO_PORT);
	state->vendor_policy_ready = false;
	state->vendor_policy_modified = false;
	/* Consume pass 1 before any fallible UART work after an unexpected return. */
	if (!second_pass &&
	    !b06v6_set_i801_signature(&b06v6_in_progress_signature))
		b06v6_guard_stop("CSI_PASS1_RETURN_GUARD_WRITE");
	if (b06v6_read_cmos_diagnostic() != B06V6_CMOS_GUARD_IN_PROGRESS)
		b06v6_guard_stop("CMOS_GUARD_LOST_AFTER_CSI");
	if (!b06v0_emit_status("AUTO_CSI_CALL", status) ||
	    !b06v0_print_call_result("AUTO_CSI", &result))
		return B06V6_AUTO_UART_ERROR;

	/* A first-pass return is outside the only hardware-observed sequence. */
	if (!second_pass)
		return b06v6_fallback("CSI_PASS1_RETURNED");
	if (status != X58_VENDOR_OK)
		return b06v6_fallback("CSI_PASS2_CALL");

	status = x58_vendor_runtime_probe(&info);
	if (!b06v0_emit_status("AUTO_CSI_PROBE", status))
		return B06V6_AUTO_UART_ERROR;
#if CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
	csi_state = x58_vendor_csi_state_snapshot();
	if (status != X58_VENDOR_OK || csi_state == NULL ||
	    !info.csi_returned || info.csi_result_accepted ||
	    info.last_call.eax != 2 || info.last_call.ebx != 2 ||
	    info.last_call.ecx != 0x106 || info.experimental_status_seed != 0)
		return b06v6_fallback("CSI_ABI_GATE");
	csi_raw_fnv = b06v0_buffer_digest(csi_state,
		X58_VENDOR_CSI_STATE_SIZE);
	csi_canonical_fnv = b06v7_csi_canonical_digest(csi_state);
	csi_dynamic_byte = csi_state[B06V7_CSI_DYNAMIC_OFFSET];
	if (!b04_uart_puts("[RAMINIT] CSI RAW_FNV=") ||
	    !b04_uart_put_hex(csi_raw_fnv, 8) ||
	    !b04_uart_puts(" BYTE2A6=") ||
	    !b04_uart_put_hex(csi_dynamic_byte, 2) ||
	    !b04_uart_puts(" CANON_FNV=") ||
	    !b04_uart_put_hex(csi_canonical_fnv, 8) ||
	    !b04_uart_puts("\r\n"))
		return B06V6_AUTO_UART_ERROR;
	if (info.csi_state_digest != csi_raw_fnv ||
	    !b06v7_csi_dynamic_byte_is_known(csi_dynamic_byte) ||
	    csi_canonical_fnv != X58_B06V7_EXPECTED_CSI_CANONICAL_FNV)
		return b06v6_fallback("CSI_CANONICAL_GATE");
#else
	if (status != X58_VENDOR_OK || !info.csi_returned ||
	    info.csi_result_accepted || info.last_call.eax != 2 ||
	    info.last_call.ebx != 2 || info.last_call.ecx != 0x106 ||
	    info.experimental_status_seed != 0 ||
	    info.csi_state_digest != X58_B06V6_EXPECTED_CSI_FNV)
		return b06v6_fallback("CSI_EXACT_GATE");
#endif

	status = x58_vendor_accept_csi_result(2, 2, 0x106,
#if CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
		csi_raw_fnv,
#else
		X58_B06V6_EXPECTED_CSI_FNV,
#endif
		X58_VENDOR_EXPERIMENT_CONFIRMATION);
	if (!b06v0_emit_status("AUTO_CSI_ACCEPT", status))
		return B06V6_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK)
		return b06v6_fallback("CSI_ACCEPT");
	status = x58_vendor_runtime_probe(&info);
#if CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
	csi_state = x58_vendor_csi_state_snapshot();
	if (status != X58_VENDOR_OK || csi_state == NULL ||
	    !info.csi_result_accepted ||
	    info.csi_state_digest != csi_raw_fnv ||
	    b06v0_buffer_digest(csi_state, X58_VENDOR_CSI_STATE_SIZE) !=
		csi_raw_fnv ||
	    csi_state[B06V7_CSI_DYNAMIC_OFFSET] != csi_dynamic_byte ||
	    b06v7_csi_canonical_digest(csi_state) != csi_canonical_fnv ||
	    csi_canonical_fnv != X58_B06V7_EXPECTED_CSI_CANONICAL_FNV)
		return b06v6_fallback("CSI_ACCEPT_READBACK");
#else
	if (status != X58_VENDOR_OK || !info.csi_result_accepted ||
	    info.csi_state_digest != X58_B06V6_EXPECTED_CSI_FNV)
		return b06v6_fallback("CSI_ACCEPT_READBACK");
#endif
	outb(POST_B06V6_CSI_ACCEPTED, CONFIG_POST_IO_PORT);

	if (!b06v0_policy_reset(state))
		return B06V6_AUTO_UART_ERROR;
	if (!state->vendor_policy_ready || state->vendor_policy_modified ||
	    b06v0_policy_digest(state->vendor_policy) != B06V6_POLICY_BASE_FNV)
		return b06v6_fallback("POLICY_BASE_GATE");
	for (index = 0; index < ARRAY_SIZE(b06v6_candidate_edits); index++)
		state->vendor_policy[b06v6_candidate_edits[index].offset] =
			b06v6_candidate_edits[index].value;
	state->vendor_policy_modified = true;
	if (b06v0_policy_digest(state->vendor_policy) !=
		B06V5_POLICY_B06V4_CANDIDATE_FNV ||
	    state->vendor_policy[B06V5_POLICY_STATUS_OFFSET] != 2 ||
	    b06v0_policy_read32(state->vendor_policy,
		B06V5_POLICY_FLAGS_OFFSET) !=
		B06V5_POLICY_B06V4_CANDIDATE_FLAGS)
		return b06v6_fallback("POLICY_CANDIDATE_GATE");

	if (!b06v5_policy_force_cold(state))
		return B06V6_AUTO_UART_ERROR;
	if (!state->vendor_policy_ready || !state->vendor_policy_modified ||
	    b06v0_policy_digest(state->vendor_policy) != B06V5_POLICY_COLD_FNV ||
	    state->vendor_policy[B06V5_POLICY_STATUS_OFFSET] != 0 ||
	    b06v0_policy_read32(state->vendor_policy,
		B06V5_POLICY_FLAGS_OFFSET) != B06V6_POLICY_COLD_FLAGS)
		return b06v6_fallback("POLICY_COLD_GATE");

	status = x58_vendor_install_confirmed_minit_policy(
		state->vendor_policy, sizeof(state->vendor_policy),
		X58_B06V6_EXPECTED_POLICY_FNV,
		X58_VENDOR_EXPERIMENT_CONFIRMATION);
	if (!b06v0_emit_status("AUTO_POLICY_INSTALL", status))
		return B06V6_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK)
		return b06v6_fallback("POLICY_INSTALL");
	status = x58_vendor_runtime_probe(&info);
	if (status != X58_VENDOR_OK || !info.minit_policy_confirmed ||
	    info.minit_policy_digest != X58_B06V6_EXPECTED_POLICY_FNV ||
	    info.minit_policy_current_digest != X58_B06V6_EXPECTED_POLICY_FNV)
		return b06v6_fallback("POLICY_INSTALL_READBACK");
	outb(POST_B06V6_POLICY_INSTALLED, CONFIG_POST_IO_PORT);

	status = x58_vendor_arm_minit(X58_VENDOR_EXPERIMENT_CONFIRMATION);
	if (!b06v0_emit_status("AUTO_MINIT_ARM", status))
		return B06V6_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK)
		return b06v6_fallback("MINIT_ARM");
	if (!b04_uart_puts(
		"[RAMINIT] calling exact cold MINIT; reset, HLT or hang remains possible\r\n") ||
	    !b04_uart_wait_for(UART8250_LSR_TEMT,
		B03_UART_FLUSH_POLL_LIMIT))
		return B06V6_AUTO_UART_ERROR;
	/* Keep the loop guard armed until every returned MINIT gate has passed. */
	if (!b06v6_write_cmos_diagnostic(B06V6_CMOS_GUARD_IN_PROGRESS))
		b06v6_guard_stop("CMOS_MINIT_GUARD_WRITE");
	if (!b06v6_set_i801_signature(&b06v6_in_progress_signature))
		b06v6_guard_stop("I801_MINIT_GUARD_WRITE");
	outb(POST_B06V0_MINIT_CALL, CONFIG_POST_IO_PORT);
	status = x58_vendor_call_minit(&result);
	outb(POST_B06V0_MINIT_RETURN, CONFIG_POST_IO_PORT);
	if (b06v6_read_cmos_diagnostic() != B06V6_CMOS_GUARD_IN_PROGRESS)
		b06v6_guard_stop("CMOS_GUARD_LOST_AFTER_MINIT");
	if (!b06v0_emit_status("AUTO_MINIT_CALL", status) ||
	    !b06v0_print_call_result("AUTO_MINIT", &result))
		return B06V6_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK)
		return b06v6_fallback("MINIT_CALL");

	csi_state = x58_vendor_csi_state_snapshot();
	policy = x58_vendor_minit_policy();
	workspace = x58_vendor_minit_workspace();
	status = x58_vendor_runtime_probe(&info);
	if (!b06v0_emit_status("AUTO_MINIT_PROBE", status))
		return B06V6_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK || csi_state == NULL || policy == NULL ||
	    workspace == NULL || !info.prepared || !info.canaries_valid ||
	    !info.wrapper_signature_valid || !info.csi_signature_valid ||
	    !info.minit_signature_valid || info.csi_wrapper_armed ||
	    !info.csi_call_attempted || !info.csi_returned ||
	    !info.csi_result_accepted || !info.minit_policy_confirmed ||
	    info.minit_armed || !info.minit_call_attempted ||
	    !info.minit_returned || info.experimental_status_seed != 0 ||
	    result.eax != 0 ||
	    workspace[1] != 0 || workspace[2] != 0 ||
	    workspace[B06V5_WORK_B3_FLAGS_OFFSET] != 0x02 ||
	    workspace[B06V5_WORK_COMPLETE_OFFSET] != 0x01 ||
	    policy[B06V5_POLICY_STATUS_OFFSET] != 0)
#if CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
		return b06v6_fallback("MINIT_RUNTIME_GATE");
#else
		return b06v6_fallback("MINIT_BUFFER_GATE");
#endif
#if CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
	final_csi_raw_fnv = b06v0_buffer_digest(csi_state,
		X58_VENDOR_CSI_STATE_SIZE);
	final_csi_canonical_fnv = b06v7_csi_canonical_digest(csi_state);
	final_csi_dynamic_byte = csi_state[B06V7_CSI_DYNAMIC_OFFSET];
	workspace_raw_fnv = b06v0_buffer_digest(workspace,
		X58_VENDOR_MINIT_WORKSPACE_SIZE);
	workspace_canonical_fnv = b06v7_workspace_canonical_digest(workspace);
	if (!b04_uart_puts("[RAMINIT] FINAL CSI_RAW=") ||
	    !b04_uart_put_hex(final_csi_raw_fnv, 8) ||
	    !b04_uart_puts(" CSI_BYTE2A6=") ||
	    !b04_uart_put_hex(final_csi_dynamic_byte, 2) ||
	    !b04_uart_puts(" CSI_CANON=") ||
	    !b04_uart_put_hex(final_csi_canonical_fnv, 8) ||
	    !b04_uart_puts(" WORK_RAW=") ||
	    !b04_uart_put_hex(workspace_raw_fnv, 8) ||
	    !b04_uart_puts(" WORK_CANON=") ||
	    !b04_uart_put_hex(workspace_canonical_fnv, 8) ||
	    !b04_uart_puts("\r\n"))
		return B06V6_AUTO_UART_ERROR;
	if (info.csi_state_digest != final_csi_raw_fnv ||
	    final_csi_raw_fnv != csi_raw_fnv ||
	    final_csi_dynamic_byte != csi_dynamic_byte ||
	    !b06v7_csi_dynamic_byte_is_known(final_csi_dynamic_byte) ||
	    final_csi_canonical_fnv != csi_canonical_fnv ||
	    final_csi_canonical_fnv !=
		X58_B06V7_EXPECTED_CSI_CANONICAL_FNV ||
	    info.minit_policy_digest != X58_B06V6_EXPECTED_POLICY_FNV ||
	    info.minit_policy_current_digest !=
		X58_B06V6_EXPECTED_POLICY_FNV ||
	    b06v0_policy_digest(policy) != X58_B06V6_EXPECTED_POLICY_FNV ||
	    info.minit_workspace_digest != workspace_raw_fnv ||
	    workspace_canonical_fnv !=
		X58_B06V7_EXPECTED_WORKSPACE_CANONICAL_FNV)
		return b06v6_fallback("MINIT_CANONICAL_BUFFER_GATE");
#else
	if (info.csi_state_digest != X58_B06V6_EXPECTED_CSI_FNV ||
	    b06v0_buffer_digest(csi_state, X58_VENDOR_CSI_STATE_SIZE) !=
		X58_B06V6_EXPECTED_CSI_FNV ||
	    info.minit_policy_digest != X58_B06V6_EXPECTED_POLICY_FNV ||
	    info.minit_policy_current_digest != X58_B06V6_EXPECTED_POLICY_FNV ||
	    b06v0_policy_digest(policy) != X58_B06V6_EXPECTED_POLICY_FNV ||
	    info.minit_workspace_digest != X58_B06V6_EXPECTED_WORKSPACE_FNV ||
	    b06v0_buffer_digest(workspace, X58_VENDOR_MINIT_WORKSPACE_SIZE) !=
		X58_B06V6_EXPECTED_WORKSPACE_FNV)
		return b06v6_fallback("MINIT_BUFFER_GATE");
#endif
	if (info.last_call.eax != result.eax ||
	    info.last_call.ebx != result.ebx ||
	    info.last_call.ecx != result.ecx ||
	    info.last_call.edx != result.edx ||
	    info.last_call.edi != result.edi ||
	    info.last_call.eflags != result.eflags ||
	    info.last_call.vendor_esp_after_return !=
		result.vendor_esp_after_return ||
	    result.vendor_esp_after_return !=
		info.vendor_stack_top - 2 * sizeof(uint32_t))
		return b06v6_fallback("MINIT_CALL_STATE_GATE");

	mc_mapper = b06j_pci_read(B06V5_MC_COMMON_DEV,
		B06V5_MC_CHANNEL_MAPPER, 4);
	mc_common_f8 = b06j_pci_read(B06K_UNCORE_COMMON_DEV, 0xf8, 4);
	ch2_dod = b06j_pci_read(B06V5_CHANNEL2_ADDR_DEV,
		B06V5_MC_DOD_DIMM0, 4);
	ch2_ranks = b06j_pci_read(B06K_CHANNEL2_DEV,
		B06K_MC_RANK_PRESENT, 4);
	ch2_status = b06j_pci_read(B06K_CHANNEL2_DEV,
		B06K_MC_INIT_STATUS, 4);
	qpi_status = b06j_pci_read(B06L_QPI_LINK0_PHY_DEV,
		B06L_QPI_PH_PIS, 4);
	if (!b04_uart_puts(
#if CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
		"[RAMINIT] ROBUST WORK_RAW=") ||
	    !b04_uart_put_hex(workspace_raw_fnv, 8) ||
	    !b04_uart_puts(" WORK_CANON=") ||
	    !b04_uart_put_hex(workspace_canonical_fnv, 8) ||
#else
		"[RAMINIT] EXACT WORK_FNV=") ||
	    !b04_uart_put_hex(info.minit_workspace_digest, 8) ||
#endif
	    !b04_uart_puts(" MC_MAP60=") || !b04_uart_put_hex(mc_mapper, 8) ||
	    !b04_uart_puts(" MC_F8=") || !b04_uart_put_hex(mc_common_f8, 8) ||
	    !b04_uart_puts(" CH2_DOD=") || !b04_uart_put_hex(ch2_dod, 8) ||
	    !b04_uart_puts(" CH2_RANKS=") || !b04_uart_put_hex(ch2_ranks, 8) ||
	    !b04_uart_puts(" CH2_STATUS=") || !b04_uart_put_hex(ch2_status, 8) ||
	    !b04_uart_puts(" QPI80=") || !b04_uart_put_hex(qpi_status, 8) ||
	    !b04_uart_puts("\r\n"))
		return B06V6_AUTO_UART_ERROR;
	if (info.cpuid_1_eax != X58_B06V6_EXPECTED_CPUID ||
	    info.microcode_revision != X58_B06V6_EXPECTED_UCODE ||
	    mc_mapper != X58_B06V6_EXPECTED_MC_MAPPER ||
	    mc_common_f8 != X58_B06V6_EXPECTED_MC_COMMON_F8 ||
	    ch2_dod != X58_B06V6_EXPECTED_CH2_DOD ||
	    ch2_ranks != X58_B06V6_EXPECTED_CH2_RANKS ||
	    ch2_status != X58_B06V6_EXPECTED_CH2_STATUS ||
	    qpi_status != X58_B06V6_EXPECTED_QPI_STATUS)
		return b06v6_fallback("MINIT_UNCORE_GATE");
	if (!b06v6_set_i801_signature(&b06v6_clear_signature))
		b06v6_guard_stop("I801_GUARD_CLEAR");
	if (!b06v6_write_cmos_diagnostic(B06V6_CMOS_DIAGNOSTIC_OBSERVED))
		b06v6_guard_stop("CMOS_GUARD_CLEAR");
	{
		const struct x58_b06v6_raminit_result handoff = {
			.cpuid = info.cpuid_1_eax,
			.microcode_revision = info.microcode_revision,
			.spd_fnv = state->vendor_spd_digest,
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
			.spd_profile_fnv = state->vendor_spd_profile_digest,
#endif
			.csi_state_fnv = info.csi_state_digest,
			.policy_fnv = info.minit_policy_digest,
			.workspace_fnv = info.minit_workspace_digest,
#if CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
			.csi_state_canonical_fnv = final_csi_canonical_fnv,
			.workspace_canonical_fnv = workspace_canonical_fnv,
#endif
			.minit_eax = result.eax,
			.mc_mapper = mc_mapper,
			.mc_common_f8 = mc_common_f8,
			.ch2_dod = ch2_dod,
			.ch2_ranks = ch2_ranks,
			.ch2_status = ch2_status,
			.qpi_status = qpi_status,
		};

		x58_b06v6_handoff_record_success(&handoff);
	}
	if (!b04_uart_puts(
#if CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
		"[RAMINIT] AUTO_HANDOFF=READY; robust Slow-QPI canonical state; returning to coreboot\r\n") ||
#else
		"[RAMINIT] AUTO_HANDOFF=READY; exact observed state; returning to coreboot\r\n") ||
#endif
	    !b04_uart_wait_for(UART8250_LSR_TEMT,
		B03_UART_FLUSH_POLL_LIMIT))
		return B06V6_AUTO_UART_ERROR;
	outb(POST_B06V6_MINIT_ACCEPTED, CONFIG_POST_IO_PORT);

	return B06V6_AUTO_READY;
}
#endif

static bool b06j_execute(struct b06j_state *state, unsigned int argc,
			 char **argv)
{
	uint32_t a, b, c, d, e;
	unsigned int width;

	if (!argc)
		return true;
	outb(POST_B06J_COMMAND, CONFIG_POST_IO_PORT);

	if (b06j_streq(argv[0], "help") && argc == 1)
		return b06j_help();
	if (b06j_streq(argv[0], "id") && argc == 1)
		return b04_uart_puts(
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
		#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
			X58_DEVELOPMENT_BUILD_ID " CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06WK_ACPI_REPAIR
			"X58PROE-B06WK-ACPI-REPAIR-20260908 CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM
			"X58PROE-B06WJ-ACPI-PLATFORM-20260907 CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI
			"X58PROE-B06WI-VENDOR-IRQ-ACPI-20260907 CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS
			"X58PROE-B06WH-AUTO-USB-SEABIOS-20260907 CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS
			"X58PROE-B06WG-AUTO-USB-SEABIOS-20260907 CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06WF_USB_LAB
			"X58PROE-B06WF-LATE-USB-LAB-20260907 CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX
			"X58PROE-B06WE-ACPI-SAD-BDF-FIX-20260907 CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT
			"X58PROE-B06WD-SEABIOS-INPUT-20260906 CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM
			"X58PROE-B06WC-INTEGRATED-PLATFORM-20260906 CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06WB_TCO_HALT
			"X58PROE-B06WB-ICH10-TCO-HALT-20260906 CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06WA_HPET_DECODE
			"X58PROE-B06WA-ICH10-HPET-DECODE-20260906 CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES
			"X58PROE-B06VZ-FIXED-RESOURCES-20260906 CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK
			"X58PROE-B06VY-ICH10-IOAPIC-MASK-20260906 CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE
			"X58PROE-B06VX-AHCI-USBTRACE-20260906 CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO
			"X58PROE-B06VW-ICH10-AHCI-MMIO-20260906 CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06VV_ICH10_PCS_SCLK
			"X58PROE-B06VV-ICH10-PCS-SCLK-20260906 CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE
			"X58PROE-B06VU-ICH10-AHCI-ROUTE-20260906 CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06VQ_USB_TRACE1
			"X58PROE-B06VQ-USBTRACE1-20260906 CPUID=") &&
		#elif CONFIG_X58_PRO_E_B06VT_ICH10_SATA_CLOCK
			"X58PROE-B06VT-ICH10-SATA-CLOCK-20260906 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS
			"X58PROE-B06VS-ICH10-AHCI-PORTS-20260906 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP
			"X58PROE-B06VR-ICH10-AHCI-MAP-20260906 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT
			"X58PROE-B06VQ-ICH10-EHCI-INIT-20260906 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06VP_LAPIC_EXTINT
			"X58PROE-B06VP-LAPIC-EXTINT-PROBE-20260906 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06VO_IOHBUSNO_ROUTE
			"X58PROE-B06VO-IOHBUSNO-ROUTE-PROBE-20260906 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06VN_IOU0_PCIE_PHYS_VBIOS
			"X58PROE-B06VN-IOU0-HD5450-PHYSVBIOS-20260906 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE
			"X58PROE-B06VM-AUTO-PCI-VGA-IPXE-20260905 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
			"X58PROE-B06VL-BROAD-HARD-GATE-SEABIOS-20260905 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
			"X58PROE-B06VK-Q-CANONICAL-CLASS-SEABIOS-20260905 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
			"X58PROE-B06VJ-COUPLED-PROFILE-Q-SEABIOS-20260905 CPUID=") &&
#else
			"X58PROE-B06VI-COUPLED-PROFILE-O-SEABIOS-20260905 CPUID=") &&
#endif
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
			"X58PROE-B06VH-PRIMARY-WORKSPACE-REARM-SEABIOS-20260905 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
			"X58PROE-B06VG-COUPLED-MINIT-SEABIOS-20260905 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
			"X58PROE-B06VF-SEABIOS-ENTRY-PROBE-20260905 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
			"X58PROE-B06VE-HIGHQPI-AUTO-RAMSTAGE-ROMMON-20260905 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
			"X58PROE-B06VD-HIGHQPI-CSI2A6-MINIT-ROMMON-20260905 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT
			"X58PROE-B06VC-HIGHQPI-A0-4SET-MINIT-ROMMON-20260904 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
			"X58PROE-B06VB-HIGHQPI-MINIT-OBSERVE-ROMMON-20260904 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET
			"X58PROE-B06VA-HIGHQPI-CPU-A0-ALLOWLIST-ROMMON-20260904 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
			"X58PROE-B06V9-DETERMINISTIC-HIGHQPI-3PASS-ROMMON-20260904 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
			"X58PROE-B06V8-HIGHQPI-3PASS-PROBE-ROMMON-20260904 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
			"X58PROE-B06V7-ROBUST-SLOWQPI-RAMSTAGE-ROMMON-20260904 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF
			"X58PROE-B06V6-AUTO-RAMSTAGE-ROMMON-20260904 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06V5_COLD_MINIT
			"X58PROE-B06V5-COLD-MINIT-GATE-ROMMON-20260904 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06V4_POLICY_TELEMETRY
			"X58PROE-B06V4-POLICY-TELEMETRY-ROMMON-20260904 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06V3_WARM_RESUME
			"X58PROE-B06V3-CSI-WARM-RESUME-ROMMON-20260904 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06V2_CSI_HEADER_FIX
			"X58PROE-B06V2-CSI-HEADER-FIX-ROMMON-20260904 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
			"X58PROE-B06V1-TRANSACTIONAL-SCRIPT-ROMMON-20260901 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
			"X58PROE-B06V0-VENDOR-ASSISTED-PROBE-20260901 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06N_TIMED_MRS
			"X58PROE-B06N-TIMED-MRS-BASEINIT-20260901 CPUID=") &&
#elif CONFIG_X58_PRO_E_B06M_BASEINIT
			"X58PROE-B06M-BASEINIT-EXACT-RD-20260901 CPUID=") &&
#else
			"X58PROE-B06L-EXACT-RD-SWEEP-20260901 CPUID=") &&
#endif
			b04_uart_put_hex(cpuid_eax(1), 8) &&
			b04_uart_puts(" UART=03f8 115200 8N1\r\n");
	if (b06j_streq(argv[0], "unlock") && argc == 2) {
		if (b06j_streq(argv[1], "WRITE")) {
			state->write_armed = true;
#if CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
			state->script_armed = false;
#endif
			return b04_uart_puts(
				"ARMED for one write; verify target and width\r\n");
		}
		if (b06j_streq(argv[1], "RESET")) {
			state->reset_armed = true;
#if CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
			state->script_armed = false;
#endif
			return b04_uart_puts(
				"[RESET] ARMED for one reset command\r\n");
		}
#if CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
		if (b06j_streq(argv[1], "SCRIPT")) {
			state->write_armed = false;
			state->reset_armed = false;
			state->vendor_armed = false;
			state->script_armed = true;
			return b04_uart_puts(
				"[SCRIPT] ARMED for one run, rollback, or discard\r\n");
		}
#endif
#if CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
		if (b06j_streq(argv[1], "VENDOR")) {
			state->vendor_armed = true;
#if CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
			state->script_armed = false;
#endif
			return b04_uart_puts(
				"[VENDOR] ARMED for one vendor operation\r\n");
		}
#if CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
		return b06j_error("use: unlock WRITE|RESET|VENDOR|SCRIPT");
#else
		return b06j_error("use: unlock WRITE|RESET|VENDOR");
#endif
#else
		return b06j_error("use: unlock WRITE|RESET");
#endif
	}
	if (b06j_streq(argv[0], "lock") && argc == 1) {
		state->write_armed = false;
		state->reset_armed = false;
#if CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
		state->script_armed = false;
#endif
#if CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
		state->vendor_armed = false;
#endif
		return b04_uart_puts("LOCKED\r\n");
	}
	if (b06j_streq(argv[0], "resetcause") && argc == 1)
		return b06v_report_reset_registers();
#if CONFIG_X58_PRO_E_ROMMON_IRQPROBE_PIT
	if (b06j_streq(argv[0], "irqprobe") && argc == 2 &&
	    b06j_streq(argv[1], "pit")) {
		struct x58_rs_info script_info;

		x58_rs_get_info(&script_info);
		if (script_info.transaction_valid)
			return b06j_error(
				"irqprobe rejected; resolve pending script transaction");
		if (!b06j_require_write(state))
			return b06j_error(
				"irqprobe locked; use: unlock WRITE");
		if (!b04_uart_puts(
			"[IRQPROBE] MUTATES PIT/PIC/LAPIC; COLD RESET REQUIRED\r\n"))
			return false;
		return b06j_run_irqprobe_pit();
	}
#endif
#if CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF
	if (b06j_streq(argv[0], "autoguard") && argc == 2 &&
	    b06j_streq(argv[1], "clear")) {
		if (!b06j_require_reset(state))
			return b06j_error(
				"autoguard clear locked; use: unlock RESET");
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
		const uint8_t current_guard = b06v6_read_cmos_diagnostic();

		if (current_guard != B06V6_CMOS_GUARD_IN_PROGRESS &&
		    current_guard != B06V8_CMOS_PHASE_FAILED)
			return b06j_error(
				"CMOS auto guard is neither 0xec nor B06V8 fail-lock 0xed");
#else
		if (b06v6_read_cmos_diagnostic() !=
		    B06V6_CMOS_GUARD_IN_PROGRESS)
			return b06j_error("CMOS auto guard is not exact 0xec");
#endif
		if (!b06v6_write_cmos_diagnostic(
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
			B06V8_CMOS_COLD_AUTHORIZATION))
#else
			B06V6_CMOS_DIAGNOSTIC_OBSERVED))
#endif
			return b06j_error(
				"CMOS auto guard clear/readback failed");

		return b04_uart_puts(
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
			"[RAMINIT] persistent auto guard 0xec/0xed->0x2c cleared; "
#else
			"[RAMINIT] persistent auto guard 0xec->0x6c cleared; "
#endif
			"remove AC power before retry\r\n");
	}
#endif
	if (b06j_streq(argv[0], "reset") && argc == 2) {
		if (!b06j_streq(argv[1], "init") &&
		    !b06j_streq(argv[1], "warm") &&
		    !b06j_streq(argv[1], "full"))
			return b06j_error("use: reset init|warm|full");
		if (!b06j_require_reset(state))
			return b06j_error("reset locked; use: unlock RESET");
		if (b06j_streq(argv[1], "init"))
			b06v_cf9_reset(
				"[RESET] request=init CF9=00->04; no cache flush\r\n",
				POST_B06V_RESET_INIT, 0x00, 0x04);
		if (b06j_streq(argv[1], "warm"))
			b06v_cf9_reset(
				"[RESET] request=warm CF9=02->06; no cache flush\r\n",
				POST_B06V_RESET_WARM, 0x02, 0x06);
		b06v_cf9_reset(
			"[RESET] request=full CF9=0a->0e; no cache flush\r\n",
			POST_B06V_RESET_FULL, 0x0a, 0x0e);
	}
#if CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
	if (b06j_streq(argv[0], "script")) {
		if (argc == 2 && b06j_streq(argv[1], "status"))
			return b06v1_print_status();
		if (argc == 2 && b06j_streq(argv[1], "clear")) {
			const enum x58_rs_result result = x58_rs_clear();

			return b06v1_report_result("CLEAR", result) &&
				(result != X58_RS_OK || b06v1_print_status());
		}
		if (argc == 2 && b06j_streq(argv[1], "seal")) {
			const enum x58_rs_result result = x58_rs_seal();

			return b06v1_report_result("SEAL", result) &&
				(result != X58_RS_OK || b06v1_print_status());
		}
		if (argc >= 2 && b06j_streq(argv[1], "add"))
			return b06v1_script_add(argc, argv);
		if ((argc == 2 || argc == 4) && b06j_streq(argv[1], "list"))
			return b06v1_script_list(false, argc, argv);
		if ((argc == 2 || argc == 4) && b06j_streq(argv[1], "trace"))
			return b06v1_script_list(true, argc, argv);
		if (argc == 4 && b06j_streq(argv[1], "run")) {
			enum x58_rs_result result;
			struct x58_rs_info info;
			bool auto_rollback;
			bool reported;

			if (!b06j_parse_hex(argv[2], &a) ||
			    (!b06j_streq(argv[3], "keep") &&
			     !b06j_streq(argv[3], "auto")))
				return b06j_error(
					"use: script run PROGRAM_FNV keep|auto");
			auto_rollback = b06j_streq(argv[3], "auto");
			if (!b06j_require_script(state))
				return b06j_error(
					"script locked; use: unlock SCRIPT");
			if (!b04_uart_puts("[SCRIPT] RUN PROGRAM_FNV=") ||
			    !b04_uart_put_hex(a, 8) || !b04_uart_puts(" MODE=") ||
			    !b04_uart_puts(auto_rollback ? "auto" : "keep") ||
			    !b04_uart_puts(
				"; reset/hang possible; PRE is flushed before access\r\n") ||
			    !b04_uart_wait_for(UART8250_LSR_TEMT,
				B03_UART_FLUSH_POLL_LIMIT))
				return false;
			outb(POST_B06V1_SCRIPT_RUN, CONFIG_POST_IO_PORT);
			result = x58_rs_run(a, auto_rollback, &b06v1_backend);
			x58_rs_get_info(&info);
			if (info.transaction_valid)
				b06v0_mark_vendor_state_dirty(state);
			reported = b06v1_print_result("RUN", result) &&
				b06v1_print_status();
			outb(result == X58_RS_OK ? POST_B06V1_SCRIPT_DONE :
				POST_B06V1_SCRIPT_FAILED, CONFIG_POST_IO_PORT);
			return reported;
		}
		if (argc == 3 && b06j_streq(argv[1], "rollback")) {
			enum x58_rs_result result;
			bool reported;

			if (!b06j_parse_hex(argv[2], &a))
				return b06j_error("use: script rollback TXN_FNV");
			if (!b06j_require_script(state))
				return b06j_error(
					"script locked; use: unlock SCRIPT");
			if (!b04_uart_puts(
				"[SCRIPT] ROLLBACK best-effort; hardware side effects remain possible\r\n") ||
			    !b04_uart_wait_for(UART8250_LSR_TEMT,
				B03_UART_FLUSH_POLL_LIMIT))
				return false;
			outb(POST_B06V1_ROLLBACK_RUN, CONFIG_POST_IO_PORT);
			result = x58_rs_rollback(a, &b06v1_backend);
			reported = b06v1_print_result("ROLLBACK", result) &&
				b06v1_print_status();
			outb(result == X58_RS_OK ? POST_B06V1_ROLLBACK_DONE :
				POST_B06V1_ROLLBACK_FAILED, CONFIG_POST_IO_PORT);
			return reported;
		}
		if (argc == 3 && b06j_streq(argv[1], "discard")) {
			enum x58_rs_result result;

			if (!b06j_parse_hex(argv[2], &a))
				return b06j_error("use: script discard TXN_FNV");
			if (!b06j_require_script(state))
				return b06j_error(
					"script locked; use: unlock SCRIPT");
			result = x58_rs_discard(a);
			return b06v1_report_result("DISCARD", result) &&
				(result != X58_RS_OK || b06v1_print_status());
		}
		return b06j_error("unknown script command; use: help");
	}
#endif
#if CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
#if CONFIG_X58_PRO_E_B06V4_POLICY_TELEMETRY
	if (b06j_streq(argv[0], "vinputs") && argc == 1) {
		struct b06v4_policy_inputs inputs;

		b06v4_capture_policy_inputs(&inputs);
		return b06v4_print_policy_inputs(&inputs);
	}
#endif
	if (b06j_streq(argv[0], "vinfo") && argc == 1) {
		if (!b06v0_print_runtime(state))
			return false;
		return b04_uart_puts("[VENDOR] ROMMON RUNTIME_BOOT=") &&
			b04_uart_puts(state->vendor_runtime_ready ? "PASS" : "FAIL") &&
			b04_uart_puts(" SPD_GATE=") &&
			b04_uart_puts(state->vendor_spd_valid ? "PASS" : "FAIL") &&
			b04_uart_puts(" ACKMAP=") &&
			b04_uart_put_hex(state->vendor_spd_ackmap, 2) &&
			b04_uart_puts(" DDR3MAP=") &&
			b04_uart_put_hex(state->vendor_spd_ddr3map, 2) &&
			b04_uart_puts(" SPD_FNV=") &&
			b04_uart_put_hex(state->vendor_spd_digest, 8) &&
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
			b04_uart_puts(" SPD_PROFILE_FNV=") &&
			b04_uart_put_hex(state->vendor_spd_profile_digest, 8) &&
#endif
			b04_uart_puts(" PCIEXBAR_STEP=") &&
			b04_uart_puts(state->vendor_pciexbar_ready ? "PASS" : "PENDING") &&
			b04_uart_puts(" POLICY_LOCAL=") &&
			b04_uart_puts(state->vendor_policy_ready ?
				(state->vendor_policy_modified ? "MODIFIED" : "GENERATED") :
				"ABSENT") && b04_uart_puts("\r\n");
	}
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
	if (b06j_streq(argv[0], "vprep") || b06j_streq(argv[0], "vcsi") ||
	    b06j_streq(argv[0], "vaccept") ||
	    (b06j_streq(argv[0], "vpolicy") &&
	     !(argc == 4 && b06j_streq(argv[1], "dump"))) ||
	    b06j_streq(argv[0], "vminit"))
		return b06j_error(
#if CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
			"B06VL broad/unsafe recovery ROMMON: manual vendor calls remain disabled");
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
			"B06VK recovery ROMMON: manual vendor calls remain disabled");
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
			"B06VJ recovery ROMMON: manual vendor calls remain disabled");
#else
			"B06VI recovery ROMMON: manual vendor calls remain disabled");
#endif
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
			"B06VH recovery ROMMON: manual vendor calls remain disabled");
#elif CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
			"B06VG recovery ROMMON: manual vendor calls remain disabled");
#elif CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
			"B06VF recovery ROMMON: manual vendor calls remain disabled");
#elif CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
			"B06VE recovery ROMMON: manual vendor calls remain disabled");
#elif CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
			"B06VD is terminal: manual vendor calls remain disabled");
#elif CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT
			"B06VC is terminal: manual vendor calls remain disabled");
#elif CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
			"B06VB is terminal: manual vendor calls remain disabled");
#elif CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET
			"B06VA is terminal: manual vendor calls and MINIT are disabled");
#else
			"B06V9 is terminal: manual vendor calls and MINIT are disabled");
#endif
#else
			"B06V8 is terminal: manual vendor calls and MINIT are disabled");
#endif
#endif
	if (b06j_streq(argv[0], "vprep") && argc == 2) {
		if (!b06j_parse_hex(argv[1], &a) || !state->vendor_spd_valid ||
		    a != state->vendor_spd_digest)
			return b06j_error("use after SPD review: vprep SPD_FNV");
		if (!b06v0_vendor_state_clean(state))
			return b06j_error(
				"generic write dirtied vendor state; cold reset required");
		if (!b06j_require_vendor(state))
			return b06j_error("vendor locked; use: unlock VENDOR");
		return b06v0_prepare_pciexbar(state);
	}
	if (b06j_streq(argv[0], "vcsi") && argc == 2) {
		struct x58_vendor_call_result result = { 0 };
		enum x58_vendor_status status;

		if (!b06j_parse_hex(argv[1], &a) || a > 0xff)
			return b06j_error("use: vcsi SEED_BYTE");
		if (!b06v0_vendor_state_clean(state))
			return b06j_error(
				"generic write dirtied vendor state; cold reset required");
		if (!b06j_require_vendor(state))
			return b06j_error("vendor locked; use: unlock VENDOR");
		if (!state->vendor_spd_valid || !state->vendor_pciexbar_ready)
			return b06j_error("vcsi requires accepted SPD FNV and vprep");

		status = x58_vendor_arm_csi_wrapper(a,
			X58_VENDOR_EXPERIMENT_CONFIRMATION);
		if (!b06v0_print_status("CSI_ARM", status))
			return false;
		if (status != X58_VENDOR_OK)
			return true;
		if (!b04_uart_puts(
			"[VENDOR] CALL CSI wrapper=fffc04e2 entry=fffe7000 UNKNOWN_SEED=") ||
		    !b04_uart_put_hex(a, 2) ||
		    !b04_uart_puts(
			"; may reset or HLT forever; NMI may remain disabled\r\n") ||
		    !b04_uart_wait_for(UART8250_LSR_TEMT,
					 B03_UART_FLUSH_POLL_LIMIT))
			return false;
		outb(POST_B06V0_CSI_CALL, CONFIG_POST_IO_PORT);
		status = x58_vendor_call_csi_wrapper(&result);
		outb(POST_B06V0_CSI_RETURN, CONFIG_POST_IO_PORT);
		state->vendor_policy_ready = false;
		state->vendor_policy_modified = false;
		if (!b06v0_print_status("CSI_CALL", status) ||
		    !b06v0_print_call_result("CSI", &result))
			return false;
		if (status != X58_VENDOR_OK)
			return b04_uart_puts(
				"[VENDOR] CSI call returned to ROMMON but failed runtime validation; state is not accept-able.\r\n") &&
				b06v0_print_runtime(state);
		if (!b04_uart_puts(
			"[VENDOR] CSI returned; result is NOT accepted. Use vstate then vaccept exact registers and CSI_FNV.\r\n"))
			return false;
		return b06v0_print_runtime(state);
	}
	if (b06j_streq(argv[0], "vaccept") && argc == 5) {
		enum x58_vendor_status status;

		if (!b06j_parse_hex(argv[1], &a) ||
		    !b06j_parse_hex(argv[2], &b) ||
		    !b06j_parse_hex(argv[3], &c) ||
		    !b06j_parse_hex(argv[4], &d))
			return b06j_error("use: vaccept EAX EBX ECX CSI_FNV");
		if (!b06v0_vendor_state_clean(state))
			return b06j_error(
				"generic write dirtied vendor state; cold reset required");
		if (!b06j_require_vendor(state))
			return b06j_error("vendor locked; use: unlock VENDOR");
		status = x58_vendor_accept_csi_result(a, b, c, d,
			X58_VENDOR_EXPERIMENT_CONFIRMATION);
		return b06v0_print_status("CSI_ACCEPT", status);
	}
	if (b06j_streq(argv[0], "vstate") && argc == 3) {
		if (!b06j_parse_hex(argv[1], &a) ||
		    !b06j_parse_hex(argv[2], &b))
			return b06j_error("use: vstate OFF COUNT");
		return b06v0_dump_buffer("CSI", x58_vendor_csi_state_snapshot(),
			X58_VENDOR_CSI_STATE_SIZE, a, b);
	}
	if (b06j_streq(argv[0], "vwork") && argc == 3) {
		if (!b06j_parse_hex(argv[1], &a) ||
		    !b06j_parse_hex(argv[2], &b))
			return b06j_error("use: vwork OFF COUNT");
		return b06v0_dump_buffer("WORK", x58_vendor_minit_workspace(),
			X58_VENDOR_MINIT_WORKSPACE_SIZE, a, b);
	}
	if (b06j_streq(argv[0], "vpolicy") && argc == 2 &&
	    b06j_streq(argv[1], "reset")) {
		if (!b06v0_vendor_state_clean(state))
			return b06j_error(
				"generic write dirtied vendor state; cold reset required");
		if (!b06j_require_vendor(state))
			return b06j_error("vendor locked; use: unlock VENDOR");
		return b06v0_policy_reset(state);
	}
	if (b06j_streq(argv[0], "vpolicy") && argc == 4 &&
	    b06j_streq(argv[1], "dump")) {
		if (!b06j_parse_hex(argv[2], &a) ||
		    !b06j_parse_hex(argv[3], &b) || !state->vendor_policy_ready)
			return b06j_error("use after reset: vpolicy dump OFF COUNT");
		return b06v0_dump_buffer("POLICY", state->vendor_policy,
			X58_VENDOR_MINIT_POLICY_SIZE, a, b);
	}
#if CONFIG_X58_PRO_E_B06V5_COLD_MINIT
	if (b06j_streq(argv[0], "vpolicy") && argc == 2 &&
	    b06j_streq(argv[1], "cold")) {
		if (!b06v0_vendor_state_clean(state))
			return b06j_error(
				"generic write dirtied vendor state; cold reset required");
		if (!b06j_require_vendor(state))
			return b06j_error("vendor locked; use: unlock VENDOR");
		return b06v5_policy_force_cold(state);
	}
#endif
	if (b06j_streq(argv[0], "vpolicy") && argc == 4 &&
	    b06j_streq(argv[1], "set")) {
		enum x58_vendor_status status;

		if (!b06j_parse_hex(argv[2], &a) || a >= sizeof(state->vendor_policy) ||
		    !b06j_parse_hex(argv[3], &b) || b > 0xff ||
		    !state->vendor_policy_ready)
			return b06j_error("use after reset: vpolicy set OFF BYTE");
		if (!b06v0_vendor_state_clean(state))
			return b06j_error(
				"generic write dirtied vendor state; cold reset required");
		if (!b06j_require_vendor(state))
			return b06j_error("vendor locked; use: unlock VENDOR");
		status = x58_vendor_invalidate_minit_policy();
		if (!b06v0_print_status("POLICY_INVALIDATE", status))
			return false;
		if (status != X58_VENDOR_OK)
			return true;
		state->vendor_policy[a] = b;
		state->vendor_policy_modified = true;
		return b04_uart_puts("[VENDOR] POLICY[") &&
			b04_uart_put_hex(a, 2) && b04_uart_puts("]=") &&
			b04_uart_put_hex(b, 2) && b04_uart_puts(" FNV1A=") &&
			b04_uart_put_hex(b06v0_policy_digest(state->vendor_policy), 8) &&
			b04_uart_puts("; not installed\r\n");
	}
	if (b06j_streq(argv[0], "vpolicy") && argc == 3 &&
	    b06j_streq(argv[1], "install")) {
		struct x58_vendor_runtime_info info;
		enum x58_vendor_status status;

		if (!b06j_parse_hex(argv[2], &a) || !state->vendor_policy_ready)
			return b06j_error(
				"use after review: vpolicy install POLICY_FNV");
		if (!b06v0_vendor_state_clean(state))
			return b06j_error(
				"generic write dirtied vendor state; cold reset required");
		if (!b06j_require_vendor(state))
			return b06j_error("vendor locked; use: unlock VENDOR");
		status = x58_vendor_install_confirmed_minit_policy(
			state->vendor_policy, sizeof(state->vendor_policy),
			a,
			X58_VENDOR_EXPERIMENT_CONFIRMATION);
		if (!b06v0_print_status("POLICY_INSTALL", status))
			return false;
		if (status != X58_VENDOR_OK)
			return true;
		status = x58_vendor_runtime_probe(&info);
		if (!b06v0_print_status("POLICY_READBACK", status))
			return false;
		if (status != X58_VENDOR_OK)
			return true;
		if (info.minit_policy_digest != a ||
		    info.minit_policy_current_digest != a)
			return b06j_error("installed policy FNV readback mismatch");
		return b04_uart_puts("[VENDOR] POLICY FNV1A=") &&
			b04_uart_put_hex(b06v0_policy_digest(state->vendor_policy), 8) &&
			b04_uart_puts(state->vendor_policy_modified ?
				" MODIFIED installed\r\n" :
				" REFERENCE installed\r\n");
	}
	if (b06j_streq(argv[0], "vminit") && argc == 1) {
		struct x58_vendor_call_result result = { 0 };
		const uint8_t *policy;
		const uint8_t *workspace;
		enum x58_vendor_status status;
		bool candidate;
#if CONFIG_X58_PRO_E_B06V5_COLD_MINIT
		bool full_path;
		uint32_t mc_mapper;
		uint32_t mc_common_f8;
		uint32_t ch2_dod;
		uint32_t ch2_ranks;
#endif

		if (!b06v0_vendor_state_clean(state))
			return b06j_error(
				"generic write dirtied vendor state; cold reset required");
		if (!b06v0_vendor_path_ready(state))
			return b06j_error(
				"vendor path requires accepted SPD FNV and vprep");
		if (!state->vendor_policy_ready)
			return b06j_error("vminit requires current local policy copy");
		if (!b06j_require_vendor(state))
			return b06j_error("vendor locked; use: unlock VENDOR");
		status = x58_vendor_arm_minit(X58_VENDOR_EXPERIMENT_CONFIRMATION);
		if (!b06v0_print_status("MINIT_ARM", status))
			return false;
		if (status != X58_VENDOR_OK)
			return true;
		if (!b04_uart_puts(
			"[VENDOR] CALL MINIT entry=fffc2000; reset/HLT/hang possible; no automatic DRAM access\r\n") ||
		    !b04_uart_wait_for(UART8250_LSR_TEMT,
					 B03_UART_FLUSH_POLL_LIMIT))
			return false;
		outb(POST_B06V0_MINIT_CALL, CONFIG_POST_IO_PORT);
		status = x58_vendor_call_minit(&result);
		outb(POST_B06V0_MINIT_RETURN, CONFIG_POST_IO_PORT);
		if (!b06v0_print_status("MINIT_CALL", status) ||
		    !b06v0_print_call_result("MINIT", &result))
			return false;
		if (status != X58_VENDOR_OK)
			return b04_uart_puts(
				"[VENDOR] MINIT call returned to ROMMON but failed runtime validation; retained buffers are unavailable.\r\n") &&
				b06v0_print_runtime(state);
		policy = x58_vendor_minit_policy();
		workspace = x58_vendor_minit_workspace();
		if (policy == NULL || workspace == NULL)
			return b06j_error("MINIT returned without retained buffers");
		candidate = status == X58_VENDOR_OK && result.eax == 0 &&
#if CONFIG_X58_PRO_E_B06V5_COLD_MINIT
			workspace[1] == 0 &&
#else
			workspace[1] != 1 &&
#endif
			workspace[2] == 0 && policy[0x0a] == 0;
#if CONFIG_X58_PRO_E_B06V5_COLD_MINIT
		full_path = candidate && workspace[B06V5_WORK_COMPLETE_OFFSET] == 1 &&
			!(workspace[B06V5_WORK_B3_FLAGS_OFFSET] & BIT(2));
		mc_mapper = b06j_pci_read(B06V5_MC_COMMON_DEV,
			B06V5_MC_CHANNEL_MAPPER, 4);
		mc_common_f8 = b06j_pci_read(B06K_UNCORE_COMMON_DEV, 0xf8, 4);
		ch2_dod = b06j_pci_read(B06V5_CHANNEL2_ADDR_DEV,
			B06V5_MC_DOD_DIMM0, 4);
		ch2_ranks = b06j_pci_read(B06K_CHANNEL2_DEV,
			B06K_MC_RANK_PRESENT, 4);
#endif
		if (!b04_uart_puts("[VENDOR] MINIT GATES EAX=") ||
		    !b04_uart_put_hex(result.eax, 8) ||
		    !b04_uart_puts(" WS01=") || !b04_uart_put_hex(workspace[1], 2) ||
		    !b04_uart_puts(" WS02=") || !b04_uart_put_hex(workspace[2], 2) ||
		    !b04_uart_puts(" POLICY0A=") || !b04_uart_put_hex(policy[0x0a], 2) ||
#if CONFIG_X58_PRO_E_B06V5_COLD_MINIT
		    !b04_uart_puts(" WS4F=") ||
		    !b04_uart_put_hex(workspace[B06V5_WORK_B3_FLAGS_OFFSET], 2) ||
		    !b04_uart_puts(" COMPLETE_E79=") ||
		    !b04_uart_put_hex(workspace[B06V5_WORK_COMPLETE_OFFSET], 2) ||
#endif
		    !b04_uart_puts(" RESULT=") ||
#if CONFIG_X58_PRO_E_B06V5_COLD_MINIT
		    !b04_uart_puts(full_path ?
			"FULL_PATH_RETURN_DRAM_UNTESTED" :
			(candidate &&
			 (workspace[B06V5_WORK_B3_FLAGS_OFFSET] & BIT(2)) ?
				"B3_EARLY_RETURN" :
			 ((result.eax & 0xffff) == 0xe801 ?
				"RESET_REQUEST_NOT_EXECUTED" : "REJECTED_OR_INCOMPLETE"))) ||
#else
		    !b04_uart_puts(candidate ?
			"CANDIDATE_ONLY_DRAM_UNTESTED" :
			((result.eax & 0xffff) == 0xe801 ?
				 "RESET_REQUEST_NOT_EXECUTED" : "REJECTED")) ||
#endif
		    !b04_uart_puts("\r\n"))
			return false;
#if CONFIG_X58_PRO_E_B06V5_COLD_MINIT
		if (!b04_uart_puts("[VENDOR] MINIT UNCORE MC_MAP60=") ||
		    !b04_uart_put_hex(mc_mapper, 8) ||
		    !b04_uart_puts(" MC_F8=") ||
		    !b04_uart_put_hex(mc_common_f8, 8) ||
		    !b04_uart_puts(" CH2_DOD80=") ||
		    !b04_uart_put_hex(ch2_dod, 8) ||
		    !b04_uart_puts(" CH2_RANK7C=") ||
		    !b04_uart_put_hex(ch2_ranks, 8) ||
		    !b04_uart_puts(" DRAM_ACCESSES=00\r\n"))
			return false;
#endif
		return b06v0_print_runtime(state);
	}
#endif
#if CONFIG_X58_PRO_E_B06M_BASEINIT
	if (b06j_streq(argv[0], "baseinit") && argc == 1) {
		if (!b06j_require_write(state))
			return b06j_error("write locked; use: unlock WRITE");
		if (!b06m_run_baseinit())
			return b06j_error("baseinit failed; remove AC power");
		return true;
	}
#endif
	if (b06j_streq(argv[0], "serialice") && argc == 1) {
		if (!b04_uart_puts("UNSAFE STREAM MODE: writes are immediate until reset\r\n"))
			return false;
		b06j_serialice(false);
	}
	if (b06j_streq(argv[0], "cpuid") && (argc == 2 || argc == 3)) {
		struct cpuid_result result;

		if (!b06j_parse_hex(argv[1], &a) ||
		    (argc == 3 && !b06j_parse_hex(argv[2], &b)))
			return b06j_error("use: cpuid LEAF [SUB]");
		result = cpuid_ext(a, argc == 3 ? b : 0);
		return b04_uart_puts("EAX=") && b04_uart_put_hex(result.eax, 8) &&
			b04_uart_puts(" EBX=") && b04_uart_put_hex(result.ebx, 8) &&
			b04_uart_puts(" ECX=") && b04_uart_put_hex(result.ecx, 8) &&
			b04_uart_puts(" EDX=") && b04_uart_put_hex(result.edx, 8) &&
			b04_uart_puts("\r\n");
	}
	if (b06j_streq(argv[0], "phyr") && argc == 4) {
		uint32_t raw;
		uint32_t value;

		if (!b06j_parse_hex(argv[1], &a) || a > 2 ||
		    !b06j_parse_hex(argv[2], &b) || b > UINT16_MAX ||
		    !b06j_parse_hex(argv[3], &c) ||
		    !b06k_phy_field_valid(b, c))
			return b06j_error("use: phyr CH START WIDTH");
#if CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
		/* PHY reads issue selector/command writes outside the vendor gates. */
		b06v0_mark_vendor_state_dirty(state);
#endif
		if (!b06k_phy_read(a, b, c, &raw, &value))
			return b06j_error("PHY read timeout");
		return b04_uart_puts("PHY_RAW=") && b04_uart_put_hex(raw, 8) &&
			b04_uart_puts(" VALUE=") && b04_uart_put_hex(value, 8) &&
			b04_uart_puts("\r\n");
	}
	if (b06j_streq(argv[0], "phyw") && argc == 6) {
		if (!b06j_parse_hex(argv[1], &a) || a > 2 ||
		    !b06j_parse_hex(argv[2], &b) || b > UINT16_MAX ||
		    !b06j_parse_hex(argv[3], &c) ||
		    !b06j_parse_hex(argv[4], &d) ||
		    !b06j_parse_hex(argv[5], &e) || e > 1 ||
		    !b06k_phy_field_valid(b, c))
			return b06j_error("use: phyw CH START WIDTH VALUE MODE");
		if (!b06j_require_write(state))
			return b06j_error("write locked; use: unlock WRITE");
		if (!b06k_phy_write(a, b, c, d, e))
			return b06j_error("PHY write timeout");
		return b04_uart_puts("PHY WRITE OK; LOCKED\r\n");
	}
	if (b06j_streq(argv[0], "rdtry") && argc == 2) {
		if (!b06j_parse_hex(argv[1], &a))
			return b06j_error("use: rdtry SWEEP");
		if (!b06j_require_write(state))
			return b06j_error("write locked; use: unlock WRITE");
		return b06k_run_rd_try(a);
	}
	if (b06j_streq(argv[0], "rdexact") && argc == 2) {
		uint32_t status = 0;

		if (!b06j_parse_hex(argv[1], &a) || a > 0x80)
			return b06j_error("use: rdexact SWEEP (<= 80)");
		if (!b06j_require_write(state))
			return b06j_error("write locked; use: unlock WRITE");
		if (!b06l_run_rd_exact(a, &status))
			return b06j_error("exact RD gate/timeout");
		return true;
	}
	if (b06j_streq(argv[0], "rdsweep") && argc == 1) {
		if (!b06j_require_write(state))
			return b06j_error("write locked; use: unlock WRITE");
		return b06l_run_rd_sweep();
	}
	if (b06j_streq(argv[0], "rcvtry") && argc == 2) {
		if (!b06j_parse_hex(argv[1], &a))
			return b06j_error("use: rcvtry COARSE");
		if (!b06j_require_write(state))
			return b06j_error("write locked; use: unlock WRITE");
		return b06k_run_rcven_try(a);
	}
	if (b06j_streq(argv[0], "msr") && argc == 2) {
		msr_t value;

		if (!b06j_parse_hex(argv[1], &a))
			return b06j_error("use: msr INDEX");
		value = rdmsr(a);
		return b04_uart_puts("MSR=") && b04_uart_put_hex(value.hi, 8) &&
			b04_uart_put_hex(value.lo, 8) && b04_uart_puts("\r\n");
	}
	if (b06j_streq(argv[0], "msrw") && argc == 4) {
		msr_t value;

		if (!b06j_parse_hex(argv[1], &a) ||
		    !b06j_parse_hex(argv[2], &b) ||
		    !b06j_parse_hex(argv[3], &c))
			return b06j_error("use: msrw INDEX HI LO");
		if (!b06j_require_write(state))
			return b06j_error("write locked; use: unlock WRITE");
		value.hi = b;
		value.lo = c;
		wrmsr(a, value);
		return b04_uart_puts("WRITE OK; LOCKED\r\n");
	}
	if (b06j_streq(argv[0], "io") && (argc == 2 || argc == 3)) {
		if (!b06j_parse_hex(argv[1], &a) || a > UINT16_MAX ||
		    !b06j_width(argc == 3 ? argv[2] : NULL, &width))
			return b06j_error("use: io PORT [b|w|l]");
		return b06j_print_u32("VALUE=", b06j_io_read(a, width), width * 2);
	}
	if (b06j_streq(argv[0], "iow") && (argc == 3 || argc == 4)) {
		if (!b06j_parse_hex(argv[1], &a) || a > UINT16_MAX ||
		    !b06j_parse_hex(argv[2], &b) ||
		    !b06j_width(argc == 4 ? argv[3] : NULL, &width))
			return b06j_error("use: iow PORT VALUE [b|w|l]");
		if (!b06j_require_write(state))
			return b06j_error("write locked; use: unlock WRITE");
		b06j_io_write(a, b, width);
		return b04_uart_puts("WRITE OK; LOCKED\r\n");
	}
	if ((b06j_streq(argv[0], "pci") && (argc == 5 || argc == 6)) ||
	    (b06j_streq(argv[0], "pciw") && (argc == 6 || argc == 7))) {
		const bool write = argv[0][3] == 'w';
		const unsigned int width_arg = write ? 6 : 5;

		if (!b06j_parse_hex(argv[1], &a) || a > 0xff ||
		    !b06j_parse_hex(argv[2], &b) || b > 0x1f ||
		    !b06j_parse_hex(argv[3], &c) || c > 7 ||
		    !b06j_parse_hex(argv[4], &d) || d > 0xff ||
		    (write && !b06j_parse_hex(argv[5], &e)) ||
		    !b06j_width(argc > width_arg ? argv[width_arg] : NULL, &width) ||
		    (d & (width - 1)))
			return b06j_error(write ?
				"use: pciw BUS DEV FN REG VALUE [b|w|l]" :
				"use: pci BUS DEV FN REG [b|w|l]");
		if (!write)
			return b06j_print_u32("VALUE=", b06j_pci_read(
				PCI_DEV(a, b, c), d, width), width * 2);
		if (!b06j_require_write(state))
			return b06j_error("write locked; use: unlock WRITE");
		b06j_pci_write(PCI_DEV(a, b, c), d, e, width);
		return b04_uart_puts("WRITE OK; LOCKED\r\n");
	}
	if ((b06j_streq(argv[0], "mem") && (argc == 2 || argc == 3)) ||
	    (b06j_streq(argv[0], "memw") && (argc == 3 || argc == 4))) {
		const bool write = argv[0][3] == 'w';

		if (!b06j_parse_hex(argv[1], &a) ||
		    (write && !b06j_parse_hex(argv[2], &b)) ||
		    !b06j_width(argc == (write ? 4 : 3) ? argv[argc - 1] : NULL,
				 &width) || (a & (width - 1)))
			return b06j_error(write ?
				"use: memw ADDR VALUE [b|w|l]" :
				"use: mem ADDR [b|w|l]");
		if (!write)
			return b06j_print_u32("VALUE=", b06j_mem_read(a, width),
				width * 2);
		if (!b06j_require_write(state))
			return b06j_error("write locked; use: unlock WRITE");
		b06j_mem_write(a, b, width);
		return b04_uart_puts("WRITE OK; LOCKED\r\n");
	}
	if (b06j_streq(argv[0], "dump") && argc == 3) {
		unsigned int offset;

		if (!b06j_parse_hex(argv[1], &a) ||
		    !b06j_parse_hex(argv[2], &b) || !b || b > B06J_DUMP_MAX ||
		    a > UINT32_MAX - (b - 1))
			return b06j_error("use: dump ADDR COUNT (COUNT <= 40 hex)");
		for (offset = 0; offset < b; offset++) {
			if ((offset & 0xf) == 0 &&
			    (!b04_uart_put_hex(a + offset, 8) || !b04_uart_putc(':')))
				return false;
			if (!b04_uart_putc(' ') ||
			    !b04_uart_put_hex(b06j_mem_read(a + offset, 1), 2))
				return false;
			if ((offset & 0xf) == 0xf || offset + 1 == b) {
				if (!b04_uart_puts("\r\n"))
					return false;
			}
		}
		return true;
	}
	if (b06j_streq(argv[0], "spd") && argc == 1)
		return b06j_run_spd(state);
	if (b06j_streq(argv[0], "post") && argc == 2) {
		if (!b06j_parse_hex(argv[1], &a) || a > 0xff)
			return b06j_error("use: post VALUE");
		if (!b06j_require_write(state))
			return b06j_error("write locked; use: unlock WRITE");
		outb(a, CONFIG_POST_IO_PORT);
		return b04_uart_puts("POST emitted; LOCKED\r\n");
	}
	if (b06j_streq(argv[0], "halt") && argc == 1) {
		if (!b06j_require_write(state))
			return b06j_error("write locked; use: unlock WRITE");
		(void)b04_uart_puts("HALT\r\n");
		stop_with_post(POST_B06J_READY);
	}

	return b06j_error("unknown command or wrong arguments; use: help");
}

static void __noreturn b06j_rommon(struct b06j_state *state)
{
	char *argv[B06J_MAX_ARGS];
	bool prompt = true;

#if CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
	{
		if (!b04_uart_puts(
#if CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
			"\r\nB06VL broad/unsafe path stopped; recovery ROMMON remains in CAR.\r\n") ||
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
			"\r\nB06VK canonical-Q path stopped; recovery ROMMON remains in CAR.\r\n") ||
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
			"\r\nB06VJ coupled path stopped; recovery ROMMON remains in CAR.\r\n") ||
#else
			"\r\nB06VI coupled path stopped; recovery ROMMON remains in CAR.\r\n") ||
#endif
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
			"\r\nB06VH PRIMARY/rearm path stopped; recovery ROMMON remains in CAR.\r\n") ||
#elif CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
			"\r\nB06VG coupled path stopped; recovery ROMMON remains in CAR.\r\n") ||
#elif CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
			"\r\nB06VF payload path stopped; recovery ROMMON remains in CAR.\r\n") ||
#elif CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
			"\r\nB06VE exact High-QPI automatic path stopped; recovery ROMMON remains in CAR.\r\n") ||
#elif CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
			"\r\nB06VD paired CSI2A6 High-QPI MINIT observation stopped; "
			"recovery ROMMON remains in CAR.\r\n") ||
#elif CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT
			"\r\nB06VC High-QPI A0 four-set MINIT observation stopped; "
			"recovery ROMMON remains in CAR.\r\n") ||
#elif CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
			"\r\nB06VB High-QPI MINIT observation stopped; recovery ROMMON remains in CAR.\r\n") ||
#elif CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET
			"\r\nB06VA High-QPI A0-allowlist three-pass probe stopped; recovery ROMMON remains in CAR.\r\n") ||
#else
			"\r\nB06V9 deterministic High-QPI three-pass probe stopped; recovery ROMMON remains in CAR.\r\n") ||
#endif
#elif CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
			"\r\nB06V8 High-QPI three-pass probe stopped; recovery ROMMON remains in CAR.\r\n") ||
#elif CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
			"\r\nB06V7 robust Slow-QPI automatic path stopped; recovery ROMMON remains in CAR.\r\n") ||
#elif CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF
			"\r\nB06V6 automatic path stopped; recovery ROMMON remains in CAR.\r\n") ||
#elif CONFIG_X58_PRO_E_B06V5_COLD_MINIT
			"\r\nB06V5 COLD-MINIT-GATE SCRIPT ROMMON in CAR; no DRAM; no automatic script/blob call.\r\n") ||
#elif CONFIG_X58_PRO_E_B06V4_POLICY_TELEMETRY
			"\r\nB06V4 POLICY-TELEMETRY SCRIPT ROMMON in CAR; no DRAM; no automatic script/blob call.\r\n") ||
#elif CONFIG_X58_PRO_E_B06V3_WARM_RESUME
			"\r\nB06V3 CSI WARM-RESUME SCRIPT ROMMON in CAR; no DRAM; no automatic script/blob call.\r\n") ||
#elif CONFIG_X58_PRO_E_B06V2_CSI_HEADER_FIX
			"\r\nB06V2 CORRECTED CSI-GATE SCRIPT ROMMON in CAR; no DRAM; no automatic script/blob call.\r\n") ||
#elif CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
			"\r\nB06V1 TRANSACTIONAL SCRIPT ROMMON in CAR; no DRAM; no automatic script/blob call.\r\n") ||
#else
			"\r\nB06V0 VENDOR-ASSISTED ROMMON in CAR; no DRAM; no automatic blob call.\r\n") ||
#endif
		    !b06v0_print_status("CAR_PREPARE",
			state->vendor_prepare_status) ||
		    !b04_uart_puts(
#if CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
			"Type help. Start passive with: spd, vinfo, script status\r\n"))
#else
			"Type help. Start with: spd, vinfo\r\n"))
#endif
			stop_with_post(POST_B06J_RX_ERROR);
	}
#else
	if (!b04_uart_puts("\r\nB06J ROMMON in CAR; no DRAM. Type help.\r\n"))
		stop_with_post(POST_B06J_RX_ERROR);
#endif
	outb(POST_B06J_READY, CONFIG_POST_IO_PORT);

	for (;;) {
		uint8_t value;
		enum b06j_rx_result rx;

		if (prompt) {
			const char *prompt_text = "rommon> ";

#if CONFIG_X58_PRO_E_B06V0_VENDOR_INIT
			if (state->write_armed && state->reset_armed &&
			    state->vendor_armed)
				prompt_text = "rommon[WRV]> ";
			else if (state->write_armed && state->vendor_armed)
				prompt_text = "rommon[WV]> ";
			else if (state->reset_armed && state->vendor_armed)
				prompt_text = "rommon[RV]> ";
			else if (state->write_armed && state->reset_armed)
				prompt_text = "rommon[WR]> ";
			else if (state->write_armed)
				prompt_text = "rommon[W]> ";
			else if (state->reset_armed)
				prompt_text = "rommon[R]> ";
			else if (state->vendor_armed)
				prompt_text = "rommon[V]> ";
#else
			if (state->write_armed && state->reset_armed)
				prompt_text = "rommon[WR]> ";
			else if (state->write_armed)
				prompt_text = "rommon[W]> ";
			else if (state->reset_armed)
				prompt_text = "rommon[R]> ";
#endif
#if CONFIG_X58_PRO_E_B06V1_REG_SCRIPT
			if (state->script_armed)
				prompt_text = "rommon[S]> ";
#endif
			if (!b04_uart_puts(prompt_text))
				stop_with_post(POST_B06J_RX_ERROR);
			prompt = false;
		}
		rx = b06j_getc(&value);
		if (rx == B06J_RX_IDLE)
			continue;
		if (rx == B06J_RX_FAULT) {
			outb(POST_B06J_RX_ERROR, CONFIG_POST_IO_PORT);
			if (!b04_uart_puts("\r\nERR UART receive status; line cleared\r\n"))
				stop_with_post(POST_B06J_RX_ERROR);
			state->length = 0;
			prompt = true;
			continue;
		}
		if (!state->length && value == '@') {
			/* SerialICE-QEMU sends '@' to trigger its initial prompt. */
			if (!b04_uart_putc(value))
				stop_with_post(POST_B06J_RX_ERROR);
			b06j_serialice(false);
		}
		if (!state->length && value == '*') {
			if (!b04_uart_putc(value))
				stop_with_post(POST_B06J_RX_ERROR);
			b06j_serialice(true);
		}

		if (value == '\r' || value == '\n') {
			unsigned int argc;

			if (value == '\n' && !state->length)
				continue;
			state->line[state->length] = '\0';
			if (!b04_uart_puts("\r\n"))
				stop_with_post(POST_B06J_RX_ERROR);
			argc = b06j_split(state->line, argv);
			if (argc > B06J_MAX_ARGS) {
				if (!b06j_error("too many arguments"))
					stop_with_post(POST_B06J_RX_ERROR);
			} else if (!b06j_execute(state, argc, argv)) {
				stop_with_post(POST_B06J_RX_ERROR);
			}
			state->length = 0;
			outb(POST_B06J_READY, CONFIG_POST_IO_PORT);
			prompt = true;
			continue;
		}
		if (value == 0x08 || value == 0x7f) {
			if (state->length) {
				state->length--;
				if (!b04_uart_puts("\b \b"))
					stop_with_post(POST_B06J_RX_ERROR);
			}
			continue;
		}
		if (value == 0x15) {
			while (state->length) {
				state->length--;
				if (!b04_uart_puts("\b \b"))
					stop_with_post(POST_B06J_RX_ERROR);
			}
			continue;
		}
		if (value < 0x20 || value > 0x7e)
			continue;
		if (state->length + 1 >= B06J_LINE_SIZE) {
			if (!b04_uart_putc('\a'))
				stop_with_post(POST_B06J_RX_ERROR);
			continue;
		}
		state->line[state->length++] = value;
		if (!b04_uart_putc(value))
			stop_with_post(POST_B06J_RX_ERROR);
	}
}
#endif
#endif
#endif
#endif

void mainboard_romstage_entry(void)
{
#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 3
	outb(POST_B03_ROMSTAGE_ENTRY, CONFIG_POST_IO_PORT);
	if (!uart_puts(b03_romstage_banner))
		stop_with_post(POST_B03_UART_ERROR);
	outb(POST_B03_UART_SENT, CONFIG_POST_IO_PORT);
	stop_with_post(POST_B03_SUCCESS);
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 4 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 5 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 6 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 7 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 8 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 9 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 10 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 11
	struct b04_ich10_snapshot ich10_before;
	struct b04_ich10_snapshot ich10_after;
	struct b04_smbus_snapshot smbus_before;
	struct b04_smbus_snapshot smbus_after;
#if CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
	struct b06v9_rtc_upper_bank_state rtc_upper_bank = { 0 };
#endif
#if CONFIG_X58_PRO_E_B06V3_WARM_RESUME
	enum b06v3_smbus_entry_state smbus_entry;
#endif
#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 11
	struct b06j_state rommon_state = { 0 };
#if CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF
	enum b06v6_auto_result auto_result;
	struct b06v6_i801_signature entry_signature = { 0 };
#endif
#endif
#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 5
	struct b05_probe_result probe = { 0 };
	uint8_t probe_error;
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 6
	struct b06_scan_result scan = { 0 };
	uint8_t scan_terminal;
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 7 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 9
	struct b06b_result spd = { 0 };
	uint8_t spd_terminal;
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 8
	struct b06b_result spd = { 0 };
	uint8_t spd_terminal;
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 10
	struct b06b_result spd = { 0 };
	uint8_t spd_terminal;
#endif
	uint8_t readback_error;

	outb(POST_B03_ROMSTAGE_ENTRY, CONFIG_POST_IO_PORT);
#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 5 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 6 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 7 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 8 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 9 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 10 || \
	CONFIG_X58_PRO_E_BRINGUP_STAGE == 11
	if (cpuid_eax(1) != 0x000206c2) {
#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 5
		stop_with_post(POST_B05_CPU_ERROR);
#else
		stop_with_post(POST_B06_CPU_ERROR);
#endif
	}
#endif
	outb(POST_B04_ICH10R_BEGIN, CONFIG_POST_IO_PORT);
	capture_ich10(&ich10_before);
	capture_smbus(&smbus_before);
	readback_error = ich10_preflight_error(&ich10_before);
	if (readback_error)
		stop_with_post(readback_error);
	if (smbus_before.id == UINT32_MAX)
		stop_with_post(POST_B04_SMBUS_ABSENT);
	if (smbus_before.id != PCI_ID_ICH10R_SMBUS)
		stop_with_post(POST_B04_SMBUS_ID_ERROR);
	/* Do not move a live I/O BAR; legacy stages require cold defaults. */
#if CONFIG_X58_PRO_E_B06V3_WARM_RESUME
	if (smbus_is_exact_cold_default(&smbus_before))
		smbus_entry = B06V3_SMBUS_ENTRY_COLD;
	else if (smbus_is_exact_configured(&smbus_before)) {
		smbus_entry = B06V3_SMBUS_ENTRY_CONFIGURED_AFTER_RESET;
#if CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF
		/* Capture before any SPD command overwrites the volatile signature. */
		b06v6_capture_i801_signature(&entry_signature);
#endif
	} else
		stop_with_post(POST_B04_SMBUS_CONFIG_ERROR);
#else
	if (smbus_before.command != 0 || smbus_before.hostc != 0 ||
	    smbus_before.bar4 != PCI_BASE_ADDRESS_SPACE_IO)
		stop_with_post(POST_B04_SMBUS_CONFIG_ERROR);
#endif

	/* Existing coreboot ICH10 early BAR setup; no GPIO pins are touched. */
	i82801jx_setup_bars();
	capture_ich10(&ich10_after);
	readback_error = ich10_readback_error(&ich10_before, &ich10_after);
	if (readback_error)
		stop_with_post(readback_error);
	outb(POST_B04_ICH10R_READY, CONFIG_POST_IO_PORT);
#if CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
	/* B06V9's single RCBA write is separately exact-gated and reported. */
	b06v9_enable_rtc_upper_bank(&rtc_upper_bank);
#endif

	/* Program the controller decode only; do not access SMBus status/data. */
	if (smbus_enable_iobar(CONFIG_FIXED_SMBUS_IO_BASE) < 0)
		stop_with_post(POST_B04_SMBUS_CONFIG_ERROR);
	capture_smbus(&smbus_after);
	if (!smbus_readback_valid(&smbus_after))
		stop_with_post(POST_B04_SMBUS_CONFIG_ERROR);
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
	/* Read only idle host registers, after decode and before any SPD command. */
	if (smbus_entry == B06V3_SMBUS_ENTRY_COLD) {
		b06v6_capture_i801_signature(&entry_signature);
	} else if ((b06v6_i801_signature_matches(&entry_signature,
			&b06v8_pass2_signature) ||
		    b06v6_i801_signature_matches(&entry_signature,
			&b06v8_pass3_signature)) &&
		   !b06v6_set_i801_signature(&b06v8_consumed_signature)) {
		/* Never leave a recognized automatic phase retryable after a fault. */
		b06v8_phase_guard_stop("B06V8_ENTRY_PHASE_CONSUME");
	}
#endif
	outb(POST_B04_SMBUS_READY, CONFIG_POST_IO_PORT);

	if (!report_ich10(&ich10_before, &ich10_after,
			  &smbus_before, &smbus_after))
		stop_with_post(POST_B04_UART_ERROR);
#if CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
	if (!b06v9_report_rtc_upper_bank(&rtc_upper_bank))
		stop_with_post(POST_B04_UART_ERROR);
#endif
#if CONFIG_X58_PRO_E_B06V3_WARM_RESUME
	if (!b04_uart_puts("[SMBUS] ENTRY=") ||
	    !b04_uart_puts(smbus_entry == B06V3_SMBUS_ENTRY_COLD ?
			    "COLD_DEFAULT\r\n" :
			    "CONFIGURED_AFTER_RESET\r\n"))
		stop_with_post(POST_B04_UART_ERROR);
#endif
	outb(POST_B04_UART_SENT, CONFIG_POST_IO_PORT);
#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 4
	stop_with_post(POST_B04_SUCCESS);
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 5
	probe_error = b05_probe_spd_type(&probe);
	if (probe_error) {
		(void)b05_report_probe(&probe, probe_error);
		stop_with_post(probe_error);
	}
	if (!b05_report_probe(&probe, 0))
		stop_with_post(POST_B05_UART_ERROR);
	outb(POST_B05_UART_SENT, CONFIG_POST_IO_PORT);
	stop_with_post(POST_B05_SUCCESS);
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 6
	scan_terminal = b06_scan_spd_types(&scan);
	if (!b06_report_scan(&scan, scan_terminal)) {
		/* Preserve primary hardware failures; UART failure qualifies a good scan. */
		if (scan_terminal >= POST_B06_SINGLE_BASE &&
		    scan_terminal <= POST_B06_MULTIPLE_DDR3)
			stop_with_post(POST_B06_UART_ERROR);
		stop_with_post(scan_terminal);
	}
	stop_with_post(scan_terminal);
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 7
	spd_terminal = b06b_read_base_spd(&spd);
	if (!spd_terminal)
		spd_terminal = POST_B06B_SUCCESS;
	if (!b06b_report(&spd, spd_terminal)) {
		/* A UART fault qualifies only an otherwise successful SPD result. */
		if (spd_terminal == POST_B06B_SUCCESS)
			stop_with_post(POST_B06_UART_ERROR);
		stop_with_post(spd_terminal);
	}
	if (spd_terminal == POST_B06B_SUCCESS)
		outb(POST_B06B_UART_SENT, CONFIG_POST_IO_PORT);
	stop_with_post(spd_terminal);
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 8
	spd_terminal = b06b_read_base_spd(&spd);
	if (!spd_terminal)
		spd_terminal = POST_B06C_SUCCESS;
	if (!b06c_report(&spd, spd_terminal)) {
		/* A UART fault qualifies only an otherwise successful SPD result. */
		if (spd_terminal == POST_B06C_SUCCESS)
			stop_with_post(POST_B06_UART_ERROR);
		stop_with_post(spd_terminal);
	}
	if (spd_terminal == POST_B06C_SUCCESS)
		outb(POST_B06C_UART_SENT, CONFIG_POST_IO_PORT);
	stop_with_post(spd_terminal);
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 9
	spd_terminal = b06b_read_base_spd(&spd);
	if (!spd_terminal) {
		const uint8_t encoded_header = B06H_HEADER_FIELD_TAG |
			(spd.spd[0] & B06H_HEADER_FIELD_MASK);

		/* The stage-9 reader has released I801; do no further bus access. */
		outb(POST_B06H_HEADER_MARKER, CONFIG_POST_IO_PORT);
		(void)b06b_report(&spd, encoded_header);
		stop_with_post(encoded_header);
	}
	/* A reporting fault must not hide the primary hardware failure. */
	(void)b06b_report(&spd, spd_terminal);
	stop_with_post(spd_terminal);
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 10
	spd_terminal = b06b_read_base_spd(&spd);
	if (!spd_terminal)
		spd_terminal = POST_B06I_SUCCESS;
	if (!b06c_report(&spd, spd_terminal)) {
		/* A UART fault qualifies only an otherwise successful SPD result. */
		if (spd_terminal == POST_B06I_SUCCESS)
			stop_with_post(POST_B06_UART_ERROR);
		stop_with_post(spd_terminal);
	}
	if (spd_terminal == POST_B06I_SUCCESS)
		outb(POST_B06I_UART_SENT, CONFIG_POST_IO_PORT);
	stop_with_post(spd_terminal);
#else
	b06j_state_initialize(&rommon_state);
#if CONFIG_X58_PRO_E_B06V6_AUTO_HANDOFF
#if CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
	auto_result = b06v8_high_qpi_probe(&rommon_state, smbus_entry,
		&entry_signature
#if CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
		, &rtc_upper_bank
#endif
		);
#else
	auto_result = b06v6_auto_handoff(&rommon_state, smbus_entry,
		&entry_signature);
#endif
	if (auto_result == B06V6_AUTO_UART_ERROR)
		stop_with_post(POST_B06J_RX_ERROR);
	if (auto_result == B06V6_AUTO_READY)
		return;
#endif
	b06j_rommon(&rommon_state);
#endif
#else
	outb(POST_B00_UNEXPECTED_ROMSTAGE, CONFIG_POST_IO_PORT);
	asm volatile (
		"cli\n\t"
		"1: hlt\n\t"
		"jmp 1b"
	);
	__builtin_unreachable();
#endif
}
