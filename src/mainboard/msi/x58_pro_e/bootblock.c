/* SPDX-License-Identifier: GPL-2.0-only */

#include "development_id.h"

#include <arch/io.h>
#include <arch/pci_io_cfg.h>
#include <bootblock_common.h>
#include <commonlib/bsd/compiler.h>
#include <commonlib/helpers.h>
#include <console/uart.h>
#include <device/pci_def.h>
#include <device/pci_type.h>
#include <device/pnp.h>
#include <device/pnp_ops.h>
#include <device/pnp_type.h>
#include <drivers/uart/uart8250reg.h>
#include <stdbool.h>
#include <southbridge/intel/common/lpc_def.h>
#include <superio/fintek/common/fintek.h>

#define POST_B00_C_ENVIRONMENT	0xc0
#define POST_B01_ICH10R_BEGIN	0xc1
#define POST_B02_LPC_BEGIN	0xc2
#define POST_B01_ICH10R_FOUND	0xd0
#define POST_B02_FINTEK_BEGIN	0xd1
#define POST_B02_FINTEK_FOUND	0xd2
#define POST_B02_UART_ENABLED	0xd3
#define POST_B02_UART_SENT	0xd4
#define POST_B03_ROMSTAGE_HANDOFF 0xc3
#define POST_B00_STOP		0xdf
#define POST_B01_NO_PCI_DEVICE	0xe1
#define POST_B01_DEVICE_ERROR	0xe2
#define POST_B02_FINTEK_ID_ERROR	0xe3
#define POST_B02_FINTEK_VID_ERROR 0xe4
#define POST_B02_LPC_ERROR	0xe5
#define POST_B02_UART_LDN_ERROR	0xe6
#define POST_B02_UART_REG_ERROR	0xe7
#define POST_B02_UART_TX_ERROR	0xed

#define PCI_VENDOR_ID_INTEL	0x8086
#define PCI_DEVICE_ID_ICH10R	0x3a16
#define PCI_ID_ICH10R_LPC	((PCI_DEVICE_ID_ICH10R << 16) | PCI_VENDOR_ID_INTEL)
#define ICH10R_LPC_DEV		PCI_DEV(0, 0x1f, 0)

#define B02_LPC_IO_DEC		0x0010
#define B02_LPC_EN		(CNF2_LPC_EN | COMA_LPC_EN)

#define FINTEK_CONFIG_PORT	0x4e
#define FINTEK_UART_LDN		0x01
#define FINTEK_DEVICE_ID0_REG	0x20
#define FINTEK_DEVICE_ID1_REG	0x21
#define FINTEK_VENDOR_ID0_REG	0x23
#define FINTEK_VENDOR_ID1_REG	0x24
#define FINTEK_DEVICE_ID0	0x05
#define FINTEK_DEVICE_ID1	0x41
#define FINTEK_VENDOR_ID0	0x19
#define FINTEK_VENDOR_ID1	0x34
#define FINTEK_UART_BASE		0x3f8

#define UART_TX_POLL_LIMIT	100000
#define UART_FLUSH_POLL_LIMIT	1000000

struct b02_lpc_snapshot {
	uint16_t io_dec;
	uint16_t enables;
	uint32_t generic[4];
};

/* Kept in the bootblock so every experimental chip is identifiable. */
#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 0
static const char b00_build_id[] =
	"X58PROE-B00-RESET-CAR-POST-20260830";
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 1
static const char b01_build_id[] =
	"X58PROE-B01-READ-ICH10R-ID-20260830";
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 2
static const char b02_build_id[] =
	"X58PROE-B02-LPC-FINTEK-UART-20260830";
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 3
static const char b03_build_id[] =
	"X58PROE-B03-XIP-ROMSTAGE-20260830";
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 4
static const char b04_build_id[] =
	"X58PROE-B04-ICH10R-EARLY-CORE-20260831";
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 5
static const char b05_build_id[] =
	"X58PROE-B05-SINGLE-SPD-PROBE-20260831";
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 6
static const char b06_build_id[] =
	"X58PROE-B06A-SPD-ADDRESS-DIAG-20260831";
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 7
static const char b06b_build_id[] =
	"X58PROE-B06B-SPD54-BASE128-CRC-20260831";
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 8
static const char b06c_build_id[] =
	"X58PROE-B06C-SPD54-USED176-DECODE-20260831";
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 9
static const char b06h_build_id[] =
	"X58PROE-B06H-SPD54-HEADER-TELEMETRY-20260831";
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 10
static const char b06i_build_id[] =
	"X58PROE-B06I-SPD54-USED256-DECODE-20260831";
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 11
static const char b06j_build_id[] =
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
#else
#error "Unsupported MSI X58 Pro-E bring-up stage"
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

#if CONFIG_X58_PRO_E_BRINGUP_STAGE >= 1
static void probe_ich10r_lpc(void)
{
	uint32_t id;

	outb(POST_B01_ICH10R_BEGIN, CONFIG_POST_IO_PORT);
	id = pci_io_read_config32(ICH10R_LPC_DEV, PCI_VENDOR_ID);

	if (id == UINT32_MAX)
		stop_with_post(POST_B01_NO_PCI_DEVICE);
	if (id != PCI_ID_ICH10R_LPC)
		stop_with_post(POST_B01_DEVICE_ERROR);

	outb(POST_B01_ICH10R_FOUND, CONFIG_POST_IO_PORT);
}
#endif

#if CONFIG_X58_PRO_E_BRINGUP_STAGE >= 2
static void configure_narrow_lpc(struct b02_lpc_snapshot *before)
{
	static const uint16_t generic_regs[] = {
		LPC_GEN1_DEC, LPC_GEN2_DEC, LPC_GEN3_DEC, LPC_GEN4_DEC,
	};
	size_t i;

	outb(POST_B02_LPC_BEGIN, CONFIG_POST_IO_PORT);

	before->io_dec = pci_io_read_config16(ICH10R_LPC_DEV, LPC_IO_DEC);
	before->enables = pci_io_read_config16(ICH10R_LPC_DEV, LPC_EN);
	for (i = 0; i < ARRAY_SIZE(generic_regs); i++)
		before->generic[i] =
			pci_io_read_config32(ICH10R_LPC_DEV, generic_regs[i]);
	for (i = 0; i < ARRAY_SIZE(before->generic); i++) {
		if (before->generic[i] & LPC_LGIR_EN)
			stop_with_post(POST_B02_LPC_ERROR);
	}

	/*
	 * ICH10R EDS LPC_IO_DEC/LPC_EN: COMA selector 000 is 0x3f8;
	 * enable only COMA and the second Super-I/O configuration range.
	 */
	pci_io_write_config16(ICH10R_LPC_DEV, LPC_IO_DEC, B02_LPC_IO_DEC);
	if (pci_io_read_config16(ICH10R_LPC_DEV, LPC_IO_DEC) != B02_LPC_IO_DEC)
		stop_with_post(POST_B02_LPC_ERROR);

	pci_io_write_config16(ICH10R_LPC_DEV, LPC_EN, B02_LPC_EN);
	if (pci_io_read_config16(ICH10R_LPC_DEV, LPC_EN) != B02_LPC_EN)
		stop_with_post(POST_B02_LPC_ERROR);
}

static void identify_fintek(void)
{
	const pnp_devfn_t uart = PNP_DEV(FINTEK_CONFIG_PORT, FINTEK_UART_LDN);
	uint8_t device_id0;
	uint8_t device_id1;
	uint8_t vendor_id0;
	uint8_t vendor_id1;

	outb(POST_B02_FINTEK_BEGIN, CONFIG_POST_IO_PORT);
	pnp_enter_conf_state(uart);
	device_id0 = pnp_read_config(uart, FINTEK_DEVICE_ID0_REG);
	device_id1 = pnp_read_config(uart, FINTEK_DEVICE_ID1_REG);
	vendor_id0 = pnp_read_config(uart, FINTEK_VENDOR_ID0_REG);
	vendor_id1 = pnp_read_config(uart, FINTEK_VENDOR_ID1_REG);
	pnp_exit_conf_state(uart);

	if (device_id0 != FINTEK_DEVICE_ID0 ||
	    device_id1 != FINTEK_DEVICE_ID1)
		stop_with_post(POST_B02_FINTEK_ID_ERROR);
	if (vendor_id0 != FINTEK_VENDOR_ID0 ||
	    vendor_id1 != FINTEK_VENDOR_ID1)
		stop_with_post(POST_B02_FINTEK_VID_ERROR);

	outb(POST_B02_FINTEK_FOUND, CONFIG_POST_IO_PORT);
}

static void configure_fintek_uart(void)
{
	const pnp_devfn_t uart = PNP_DEV(FINTEK_CONFIG_PORT, FINTEK_UART_LDN);
	uint8_t enabled;
	uint16_t iobase;

	/* Common Fintek code touches only LDN 1 enable and IO0 registers. */
	fintek_enable_serial(uart, FINTEK_UART_BASE);

	pnp_enter_conf_state(uart);
	pnp_set_logical_device(uart);
	enabled = pnp_read_config(uart, PNP_IDX_EN);
	iobase = pnp_read_iobase(uart, PNP_IDX_IO0);
	pnp_exit_conf_state(uart);

	if (enabled != 1 || iobase != FINTEK_UART_BASE)
		stop_with_post(POST_B02_UART_LDN_ERROR);

	outb(POST_B02_UART_ENABLED, CONFIG_POST_IO_PORT);
}

static bool uart_wait_for(uint8_t mask, unsigned int limit);

static void test_uart_registers(void)
{
	uint8_t original = inb(FINTEK_UART_BASE + UART8250_SCR);
	bool passed;

	outb(0x5a, FINTEK_UART_BASE + UART8250_SCR);
	passed = inb(FINTEK_UART_BASE + UART8250_SCR) == 0x5a;
	outb(0xa5, FINTEK_UART_BASE + UART8250_SCR);
	passed &= inb(FINTEK_UART_BASE + UART8250_SCR) == 0xa5;
	outb(original, FINTEK_UART_BASE + UART8250_SCR);

	if (!passed)
		stop_with_post(POST_B02_UART_REG_ERROR);

	/* Internal loopback proves the UART block without relying on JCOM1. */
	outb(UART8250_FCR_FIFO_EN | UART8250_FCR_CLEAR_RCVR |
	     UART8250_FCR_CLEAR_XMIT, FINTEK_UART_BASE + UART8250_FCR);
	outb(UART8250_MCR_DTR | UART8250_MCR_RTS | UART8250_MCR_LOOP,
	     FINTEK_UART_BASE + UART8250_MCR);
	passed = uart_wait_for(UART8250_LSR_THRE, UART_TX_POLL_LIMIT);
	if (passed) {
		outb(0x5a, FINTEK_UART_BASE + UART8250_TBR);
		passed = uart_wait_for(UART8250_LSR_DR, UART_TX_POLL_LIMIT);
	}
	if (passed)
		passed = inb(FINTEK_UART_BASE + UART8250_RBR) == 0x5a;
	outb(UART8250_MCR_DTR | UART8250_MCR_RTS,
	     FINTEK_UART_BASE + UART8250_MCR);
	outb(UART8250_FCR_FIFO_EN | UART8250_FCR_CLEAR_RCVR |
	     UART8250_FCR_CLEAR_XMIT, FINTEK_UART_BASE + UART8250_FCR);

	if (!passed)
		stop_with_post(POST_B02_UART_REG_ERROR);
}

static bool uart_wait_for(uint8_t mask, unsigned int limit)
{
	while (limit--) {
		if ((inb(FINTEK_UART_BASE + UART8250_LSR) & mask) == mask)
			return true;
	}
	return false;
}

static bool uart_putc(uint8_t value)
{
	if (!uart_wait_for(UART8250_LSR_THRE, UART_TX_POLL_LIMIT))
		return false;
	outb(value, FINTEK_UART_BASE + UART8250_TBR);
	return true;
}

static bool uart_puts(const char *string)
{
	while (*string) {
		if (!uart_putc(*string++))
			return false;
	}
	return true;
}

static bool uart_put_hex(uint32_t value, unsigned int digits)
{
	static const char hex[] = "0123456789abcdef";

	while (digits--) {
		unsigned int shift = digits * 4;

		if (!uart_putc(hex[(value >> shift) & 0xf]))
			return false;
	}
	return true;
}

#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 2
static bool send_b02_banner(const struct b02_lpc_snapshot *before)
{
	size_t i;

	if (!uart_puts("\r\n[") || !uart_puts(b02_build_id) ||
	    !uart_puts("]\r\nICH10R=8086:3a16\r\nLPC_PRE IO="))
		return false;
	if (!uart_put_hex(before->io_dec, 4) || !uart_puts(" EN=") ||
	    !uart_put_hex(before->enables, 4))
		return false;

	for (i = 0; i < ARRAY_SIZE(before->generic); i++) {
		if (!uart_puts(" G") || !uart_put_hex(i + 1, 1) ||
		    !uart_putc('=') || !uart_put_hex(before->generic[i], 8))
			return false;
	}

	return uart_puts("\r\nLPC_POST IO=0010 EN=2001\r\n"
			 "FINTEK DID=4105 VID=3419 LDN1=03f8\r\n"
			 "UART=03f8 115200 8N1 CAR\r\nPOST=df HALT\r\n") &&
		uart_wait_for(UART8250_LSR_TEMT, UART_FLUSH_POLL_LIMIT);
}
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 3
static bool send_b03_bootblock_banner(const struct b02_lpc_snapshot *before)
{
	size_t i;

	if (!uart_puts("\r\n[") || !uart_puts(b03_build_id) ||
	    !uart_puts("]\r\nICH10R=8086:3a16\r\nLPC_PRE IO="))
		return false;
	if (!uart_put_hex(before->io_dec, 4) || !uart_puts(" EN=") ||
	    !uart_put_hex(before->enables, 4))
		return false;

	for (i = 0; i < ARRAY_SIZE(before->generic); i++) {
		if (!uart_puts(" G") || !uart_put_hex(i + 1, 1) ||
		    !uart_putc('=') || !uart_put_hex(before->generic[i], 8))
			return false;
	}

	return uart_puts("\r\nLPC_POST IO=0010 EN=2001\r\n"
			 "FINTEK DID=4105 VID=3419 LDN1=03f8\r\n"
			 "UART=03f8 115200 8N1 CAR\r\n"
			 "POST=c3 CBFS_ROMSTAGE_HANDOFF\r\n") &&
		uart_wait_for(UART8250_LSR_TEMT, UART_FLUSH_POLL_LIMIT);
}
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 4
static bool send_b04_bootblock_banner(const struct b02_lpc_snapshot *before)
{
	size_t i;

	if (!uart_puts("\r\n[") || !uart_puts(b04_build_id) ||
	    !uart_puts("]\r\nICH10R=8086:3a16\r\nLPC_PRE IO="))
		return false;
	if (!uart_put_hex(before->io_dec, 4) || !uart_puts(" EN=") ||
	    !uart_put_hex(before->enables, 4))
		return false;

	for (i = 0; i < ARRAY_SIZE(before->generic); i++) {
		if (!uart_puts(" G") || !uart_put_hex(i + 1, 1) ||
		    !uart_putc('=') || !uart_put_hex(before->generic[i], 8))
			return false;
	}

	return uart_puts("\r\nLPC_POST IO=0010 EN=2001\r\n"
			 "FINTEK DID=4105 VID=3419 LDN1=03f8\r\n"
			 "UART=03f8 115200 8N1 CAR\r\n"
			 "POST=c3 CBFS_ROMSTAGE_HANDOFF\r\n") &&
		uart_wait_for(UART8250_LSR_TEMT, UART_FLUSH_POLL_LIMIT);
}
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 5
static bool send_b05_bootblock_banner(const struct b02_lpc_snapshot *before)
{
	size_t i;

	if (!uart_puts("\r\n[") || !uart_puts(b05_build_id) ||
	    !uart_puts("]\r\nICH10R=8086:3a16\r\nLPC_PRE IO="))
		return false;
	if (!uart_put_hex(before->io_dec, 4) || !uart_puts(" EN=") ||
	    !uart_put_hex(before->enables, 4))
		return false;

	for (i = 0; i < ARRAY_SIZE(before->generic); i++) {
		if (!uart_puts(" G") || !uart_put_hex(i + 1, 1) ||
		    !uart_putc('=') || !uart_put_hex(before->generic[i], 8))
			return false;
	}

	return uart_puts("\r\nLPC_POST IO=0010 EN=2001\r\n"
			 "FINTEK DID=4105 VID=3419 LDN1=03f8\r\n"
			 "UART=03f8 115200 8N1 CAR\r\n"
			 "POST=c3 CBFS_ROMSTAGE_HANDOFF\r\n") &&
		uart_wait_for(UART8250_LSR_TEMT, UART_FLUSH_POLL_LIMIT);
}
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 6
static bool send_b06_bootblock_banner(const struct b02_lpc_snapshot *before)
{
	size_t i;

	if (!uart_puts("\r\n[") || !uart_puts(b06_build_id) ||
	    !uart_puts("]\r\nICH10R=8086:3a16\r\nLPC_PRE IO="))
		return false;
	if (!uart_put_hex(before->io_dec, 4) || !uart_puts(" EN=") ||
	    !uart_put_hex(before->enables, 4))
		return false;

	for (i = 0; i < ARRAY_SIZE(before->generic); i++) {
		if (!uart_puts(" G") || !uart_put_hex(i + 1, 1) ||
		    !uart_putc('=') || !uart_put_hex(before->generic[i], 8))
			return false;
	}

	return uart_puts("\r\nLPC_POST IO=0010 EN=2001\r\n"
			 "FINTEK DID=4105 VID=3419 LDN1=03f8\r\n"
			 "UART=03f8 115200 8N1 CAR\r\n"
			 "POST=c3 CBFS_ROMSTAGE_HANDOFF\r\n") &&
		uart_wait_for(UART8250_LSR_TEMT, UART_FLUSH_POLL_LIMIT);
}
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 7
static bool send_b06b_bootblock_banner(const struct b02_lpc_snapshot *before)
{
	size_t i;

	if (!uart_puts("\r\n[") || !uart_puts(b06b_build_id) ||
	    !uart_puts("]\r\nICH10R=8086:3a16\r\nLPC_PRE IO="))
		return false;
	if (!uart_put_hex(before->io_dec, 4) || !uart_puts(" EN=") ||
	    !uart_put_hex(before->enables, 4))
		return false;

	for (i = 0; i < ARRAY_SIZE(before->generic); i++) {
		if (!uart_puts(" G") || !uart_put_hex(i + 1, 1) ||
		    !uart_putc('=') || !uart_put_hex(before->generic[i], 8))
			return false;
	}

	return uart_puts("\r\nLPC_POST IO=0010 EN=2001\r\n"
			 "FINTEK DID=4105 VID=3419 LDN1=03f8\r\n"
			 "UART=03f8 115200 8N1 CAR\r\n"
			 "POST=c3 CBFS_ROMSTAGE_HANDOFF\r\n") &&
		uart_wait_for(UART8250_LSR_TEMT, UART_FLUSH_POLL_LIMIT);
}
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 8
static bool send_b06c_bootblock_banner(const struct b02_lpc_snapshot *before)
{
	size_t i;

	if (!uart_puts("\r\n[") || !uart_puts(b06c_build_id) ||
	    !uart_puts("]\r\nICH10R=8086:3a16\r\nLPC_PRE IO="))
		return false;
	if (!uart_put_hex(before->io_dec, 4) || !uart_puts(" EN=") ||
	    !uart_put_hex(before->enables, 4))
		return false;

	for (i = 0; i < ARRAY_SIZE(before->generic); i++) {
		if (!uart_puts(" G") || !uart_put_hex(i + 1, 1) ||
		    !uart_putc('=') || !uart_put_hex(before->generic[i], 8))
			return false;
	}

	return uart_puts("\r\nLPC_POST IO=0010 EN=2001\r\n"
			 "FINTEK DID=4105 VID=3419 LDN1=03f8\r\n"
			 "UART=03f8 115200 8N1 CAR\r\n"
			 "POST=c3 CBFS_ROMSTAGE_HANDOFF\r\n") &&
		uart_wait_for(UART8250_LSR_TEMT, UART_FLUSH_POLL_LIMIT);
}
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 9
static bool send_b06h_bootblock_banner(const struct b02_lpc_snapshot *before)
{
	size_t i;

	if (!uart_puts("\r\n[") || !uart_puts(b06h_build_id) ||
	    !uart_puts("]\r\nICH10R=8086:3a16\r\nLPC_PRE IO="))
		return false;
	if (!uart_put_hex(before->io_dec, 4) || !uart_puts(" EN=") ||
	    !uart_put_hex(before->enables, 4))
		return false;

	for (i = 0; i < ARRAY_SIZE(before->generic); i++) {
		if (!uart_puts(" G") || !uart_put_hex(i + 1, 1) ||
		    !uart_putc('=') || !uart_put_hex(before->generic[i], 8))
			return false;
	}

	return uart_puts("\r\nLPC_POST IO=0010 EN=2001\r\n"
			 "FINTEK DID=4105 VID=3419 LDN1=03f8\r\n"
			 "UART=03f8 115200 8N1 CAR\r\n"
			 "POST=c3 CBFS_ROMSTAGE_HANDOFF\r\n") &&
		uart_wait_for(UART8250_LSR_TEMT, UART_FLUSH_POLL_LIMIT);
}
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 10
static bool send_b06i_bootblock_banner(const struct b02_lpc_snapshot *before)
{
	size_t i;

	if (!uart_puts("\r\n[") || !uart_puts(b06i_build_id) ||
	    !uart_puts("]\r\nICH10R=8086:3a16\r\nLPC_PRE IO="))
		return false;
	if (!uart_put_hex(before->io_dec, 4) || !uart_puts(" EN=") ||
	    !uart_put_hex(before->enables, 4))
		return false;

	for (i = 0; i < ARRAY_SIZE(before->generic); i++) {
		if (!uart_puts(" G") || !uart_put_hex(i + 1, 1) ||
		    !uart_putc('=') || !uart_put_hex(before->generic[i], 8))
			return false;
	}

	return uart_puts("\r\nLPC_POST IO=0010 EN=2001\r\n"
			 "FINTEK DID=4105 VID=3419 LDN1=03f8\r\n"
			 "UART=03f8 115200 8N1 CAR\r\n"
			 "POST=c3 CBFS_ROMSTAGE_HANDOFF\r\n") &&
		uart_wait_for(UART8250_LSR_TEMT, UART_FLUSH_POLL_LIMIT);
}
#else
static bool send_b06j_bootblock_banner(const struct b02_lpc_snapshot *before)
{
	size_t i;

	if (!uart_puts("\r\n[") || !uart_puts(b06j_build_id) ||
	    !uart_puts("]\r\nICH10R=8086:3a16\r\nLPC_PRE IO="))
		return false;
	if (!uart_put_hex(before->io_dec, 4) || !uart_puts(" EN=") ||
	    !uart_put_hex(before->enables, 4))
		return false;

	for (i = 0; i < ARRAY_SIZE(before->generic); i++) {
		if (!uart_puts(" G") || !uart_put_hex(i + 1, 1) ||
		    !uart_putc('=') || !uart_put_hex(before->generic[i], 8))
			return false;
	}

	return uart_puts("\r\nLPC_POST IO=0010 EN=2001\r\n"
			 "FINTEK DID=4105 VID=3419 LDN1=03f8\r\n"
			 "UART=03f8 115200 8N1 CAR\r\n"
			 "POST=c3 CBFS_ROMSTAGE_HANDOFF\r\n") &&
		uart_wait_for(UART8250_LSR_TEMT, UART_FLUSH_POLL_LIMIT);
}
#endif

static void bring_up_b02_uart(const struct b02_lpc_snapshot *before)
{
	configure_fintek_uart();
	uart_init(CONFIG_UART_FOR_CONSOLE);
	test_uart_registers();

#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 2
	if (!send_b02_banner(before))
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 3
	if (!send_b03_bootblock_banner(before))
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 4
	if (!send_b04_bootblock_banner(before))
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 5
	if (!send_b05_bootblock_banner(before))
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 6
	if (!send_b06_bootblock_banner(before))
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 7
	if (!send_b06b_bootblock_banner(before))
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 8
	if (!send_b06c_bootblock_banner(before))
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 9
	if (!send_b06h_bootblock_banner(before))
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 10
	if (!send_b06i_bootblock_banner(before))
#else
	if (!send_b06j_bootblock_banner(before))
#endif
		stop_with_post(POST_B02_UART_TX_ERROR);

	outb(POST_B02_UART_SENT, CONFIG_POST_IO_PORT);
}
#endif

void bootblock_mainboard_early_init(void)
{
	/* Keep an address reference so --gc-sections cannot discard the ID. */
#if CONFIG_X58_PRO_E_BRINGUP_STAGE == 0
	asm volatile ("" : : "r" (b00_build_id) : "memory");
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 1
	asm volatile ("" : : "r" (b01_build_id) : "memory");
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 2
	asm volatile ("" : : "r" (b02_build_id) : "memory");
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 3
	asm volatile ("" : : "r" (b03_build_id) : "memory");
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 4
	asm volatile ("" : : "r" (b04_build_id) : "memory");
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 5
	asm volatile ("" : : "r" (b05_build_id) : "memory");
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 6
	asm volatile ("" : : "r" (b06_build_id) : "memory");
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 7
	asm volatile ("" : : "r" (b06b_build_id) : "memory");
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 8
	asm volatile ("" : : "r" (b06c_build_id) : "memory");
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 9
	asm volatile ("" : : "r" (b06h_build_id) : "memory");
#elif CONFIG_X58_PRO_E_BRINGUP_STAGE == 10
	asm volatile ("" : : "r" (b06i_build_id) : "memory");
#else
	asm volatile ("" : : "r" (b06j_build_id) : "memory");
#endif

	outb(POST_B00_C_ENVIRONMENT, CONFIG_POST_IO_PORT);

#if CONFIG_X58_PRO_E_BRINGUP_STAGE >= 1
	probe_ich10r_lpc();
#endif

#if CONFIG_X58_PRO_E_BRINGUP_STAGE >= 2
	struct b02_lpc_snapshot before;

	configure_narrow_lpc(&before);
	identify_fintek();
	bring_up_b02_uart(&before);
#endif

#if CONFIG_X58_PRO_E_BRINGUP_STAGE < 3
	/* No DRAM, QPI, X58, SPI-write, romstage or payload activity. */
	stop_with_post(POST_B00_STOP);
#endif
}

#if CONFIG_X58_PRO_E_BRINGUP_STAGE >= 3
void bootblock_mainboard_init(void)
{
	/* The normal bootblock calls run_romstage() immediately after this hook. */
	outb(POST_B03_ROMSTAGE_HANDOFF, CONFIG_POST_IO_PORT);
}
#endif
