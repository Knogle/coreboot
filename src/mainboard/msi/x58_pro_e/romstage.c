/* SPDX-License-Identifier: GPL-2.0-only */

#include "cold_cookie_policy.h"
#include "../../../cpu/intel/model_206cx/mca_diagnostic.h"

#include <arch/cpuid.h>
#include <arch/io.h>
#include <arch/pci_io_cfg.h>
#include <arch/romstage.h>
#include <arch/symbols.h>
#include <commonlib/bsd/compiler.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <device/pci_def.h>
#include <device/pci_type.h>
#include <device/dram/common.h>
#include <device/dram/ddr3.h>
#include <device/smbus_host.h>
#include <drivers/uart/uart8250reg.h>
#include <stdbool.h>
#include <cpu/x86/msr.h>
#include <cpu/intel/microcode.h>
#include <southbridge/intel/common/lpc_def.h>
#include <southbridge/intel/common/pmutil.h>
#include <southbridge/intel/i82801jx/i82801jx.h>
#include "vendor_init.h"
#include "csi_state_policy.h"
#include "spd_topology.h"
#include "rommon_script.h"
#include "raminit_handoff.h"
_Static_assert(X58_SPD_PROFILE_SIZE == SPD_SIZE_MAX_DDR3 &&
	X58_SPD_PROFILE_SERIAL_OFFSET == SPD_DDR3_SERIAL_NUM &&
	X58_SPD_PROFILE_SERIAL_SIZE == SPD_DDR3_SERIAL_LEN,
	"SPD profile normalization must match DDR3 serial layout");

#define POST_B00_UNEXPECTED_ROMSTAGE	0xee
#define POST_X58_EARLY_UART_ROMSTAGE_ENTRY		0xd5
#define POST_X58_EARLY_UART_SENT		0xd6
#define POST_X58_EARLY_UART_SUCCESS		0xde
#define POST_X58_EARLY_UART_ERROR		0xed

#define POST_X58_ROMSTAGE_ICH10R_BEGIN		0xc4
#define POST_X58_ROMSTAGE_ICH10R_READY		0xc5
#define POST_X58_ROMSTAGE_SMBUS_READY		0xc6
#define POST_X58_ROMSTAGE_UART_SENT		0xd7
#define POST_X58_ROMSTAGE_SUCCESS		0xdd
#define POST_X58_ROMSTAGE_RCBA_ERROR		0xf0
#define POST_X58_ROMSTAGE_SMBUS_ABSENT		0xf1
#define POST_X58_ROMSTAGE_SMBUS_ID_ERROR		0xf2
#define POST_X58_ROMSTAGE_SMBUS_CONFIG_ERROR	0xf3
#define POST_X58_ROMSTAGE_PMBASE_ERROR		0xf4
#define POST_X58_ROMSTAGE_GPIOBASE_ERROR		0xf5
#define POST_X58_ROMSTAGE_LPC_INVARIANT_ERROR	0xf6
#define POST_X58_ROMSTAGE_UART_ERROR		0xf7

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

#define POST_X58_PLATFORM_SINGLE_BASE		0x41
#define POST_X58_PLATFORM_MULTIPLE_DDR3		0x49
#define POST_X58_PLATFORM_NO_RESPONSE		0x4a
#define POST_X58_PLATFORM_BAD_TYPE		0x4b
#define POST_X58_PLATFORM_CLOCK_ERROR		0x4c
#define POST_X58_PLATFORM_DATA_ERROR		0x4d
#define POST_X58_PLATFORM_BOTH_PINS_ERROR	0x4e
#define POST_X58_PLATFORM_HOST_STATE_ERROR	0x4f
#define POST_X58_PLATFORM_TIMEOUT			0x50
#define POST_X58_PLATFORM_BUS_ERROR		0x51
#define POST_X58_PLATFORM_TRANSACTION_ERROR	0x52
#define POST_X58_PLATFORM_UART_ERROR		0x53
#define POST_X58_PLATFORM_PIN_PHASE		0x54
#define POST_X58_PLATFORM_CLOCK_RELEASED		0x55
#define POST_X58_PLATFORM_PINS_IDLE		0x56
#define POST_X58_PLATFORM_CPU_ERROR		0x57
#define POST_X58_PLATFORM_PROBE_BASE		0x80
#define POST_X58_PLATFORM_DEV_ERR_BASE		0x88
#define POST_X58_PLATFORM_DDR3_BASE		0x90
#define POST_X58_PLATFORM_OTHER_TYPE_BASE	0x98

#define POST_X58_SPD_READ_READ_BEGIN		0x58
#define POST_X58_SPD_READ_TYPE_OK		0x59
#define POST_X58_SPD_READ_BLOCK_DONE_BASE	0x5a
#define POST_X58_SPD_READ_CRC_OK		0x62
#define POST_X58_SPD_READ_UART_SENT		0x63
#define POST_X58_SPD_READ_SUCCESS		0x64
#define POST_X58_SPD_READ_DEV_ERROR		0x65
#define POST_X58_SPD_READ_NOT_DDR3		0x66
#define POST_X58_SPD_READ_CRC_ERROR		0x67

#define POST_X58_SPD_TIMING_HEADER_OK		0x68
#define POST_X58_SPD_TIMING_UPPER_PASS1_BEGIN	0x69
#define POST_X58_SPD_TIMING_PASS1_BLOCK_BASE	0x6a
#define POST_X58_SPD_TIMING_UPPER_PASS2_BEGIN	0x6d
#define POST_X58_SPD_TIMING_PASS2_BLOCK_BASE	0x6e
#define POST_X58_SPD_TIMING_UPPER_MATCH		0x71
#define POST_X58_SPD_TIMING_DECODE_OK		0x72
#define POST_X58_SPD_TIMING_POLICY_OK		0x73
#define POST_X58_SPD_TIMING_UART_SENT		0x74
#define POST_X58_SPD_TIMING_SUCCESS		0x75
#define POST_X58_SPD_TIMING_HEADER_ERROR		0x76
#define POST_X58_SPD_TIMING_UPPER_MISMATCH	0x77
#define POST_X58_SPD_TIMING_DECODE_ERROR		0x78
#define POST_X58_SPD_TIMING_POLICY_ERROR		0x79

#define POST_X58_SCRIPT_HEADER_MARKER		0x7a
#define X58_SCRIPT_HEADER_FIELD_MASK		0x7f
#define X58_SCRIPT_HEADER_FIELD_TAG		0x80

#define POST_X58_PLATFORMI_HEADER_OK		0xa0
#define POST_X58_PLATFORMI_UPPER_PASS1_BEGIN	0xa1
#define POST_X58_PLATFORMI_PASS1_BLOCK_BASE	0xa2
#define POST_X58_PLATFORMI_UPPER_PASS2_BEGIN	0xaa
#define POST_X58_PLATFORMI_PASS2_BLOCK_BASE	0xab
#define POST_X58_PLATFORMI_UPPER_MATCH		0xb3
#define POST_X58_PLATFORMI_DECODE_OK		0xb4
#define POST_X58_PLATFORMI_POLICY_OK		0xb5
#define POST_X58_PLATFORMI_UART_SENT		0xb6
#define POST_X58_PLATFORMI_SUCCESS		0xb7
#define POST_X58_PLATFORMI_HEADER_ERROR		0xb8
#define POST_X58_PLATFORMI_UPPER_MISMATCH	0xb9
#define POST_X58_PLATFORMI_DECODE_ERROR		0xba
#define POST_X58_PLATFORMI_POLICY_ERROR		0xbb

#define POST_X58_ROMMON_READY			0xbc
#define POST_X58_ROMMON_COMMAND		0xbd
#define POST_X58_ROMMON_RX_ERROR		0xbe
#define POST_X58_ROMMON_COMMAND_ERROR		0xbf
#define POST_X58_ROMMON_SERIALICE		0xc7
#define POST_X58_VENDOR_PATH_RESET_INIT		0xcc
#define POST_X58_VENDOR_PATH_RESET_WARM		0xcd
#define POST_X58_VENDOR_PATH_RESET_FULL		0xce
#define POST_X58_VENDOR_ENTRY_PCIEXBAR_READY	0xd0
#define POST_X58_VENDOR_ENTRY_CSI_CALL		0xd1
#define POST_X58_VENDOR_ENTRY_CSI_RETURN		0xd2
#define POST_X58_VENDOR_ENTRY_MINIT_CALL		0xd3
#define POST_X58_VENDOR_ENTRY_MINIT_RETURN		0xd4
#define POST_X58_RAMINIT_AUTO_BEGIN		0x02
#define POST_X58_RAMINIT_PLATFORM_READY	0x03
#define POST_X58_RAMINIT_CSI_PASS1_ARMED	0x04
#define POST_X58_RAMINIT_CSI_PASS2_ARMED	0x05
#define POST_X58_RAMINIT_CSI_ACCEPTED		0x06
#define POST_X58_RAMINIT_POLICY_INSTALLED	0x07
#define POST_X58_RAMINIT_MINIT_ACCEPTED	0x08
#define POST_X58_RAMINIT_GUARD_STOP		0x1f
#define POST_X58_RAMINIT_AUTO_FALLBACK	0x20
#define POST_X58_INIT_PHASE_PASS2_ACCEPTED	0xca
#define POST_X58_INIT_PHASE_OUTER_RESET_ARMED	0xcb
#define POST_X58_INIT_PHASE_CSI_PASS3_ARMED	0xcf
#define POST_X58_INIT_PHASE_CSI_PASS3_RETURN	0xd9
#define POST_X58_INIT_PHASE_TERMINAL		0xda
#define POST_X58_INIT_PHASE_IOH_SYRE		0xfe
#define POST_X58_RTC_POLICY_RTC_PRECHECK		0x10
#define POST_X58_RTC_POLICY_RTC_ENABLED		0x11
#define POST_X58_RTC_POLICY_PROFILE_READY	0x12
#define POST_X58_VENDOR_TRACE_HIGH_PROFILE_READY	0x13
#define POST_X58_VENDOR_ENTRY_ERROR			0xdf
#define POST_X58_VENDOR_SCRIPT_SCRIPT_RUN		0xe5
#define POST_X58_VENDOR_SCRIPT_SCRIPT_FAILED	0xe6
#define POST_X58_VENDOR_SCRIPT_SCRIPT_DONE		0xe7
#define POST_X58_VENDOR_SCRIPT_ROLLBACK_RUN		0xe8
#define POST_X58_VENDOR_SCRIPT_ROLLBACK_DONE	0xe9
#define POST_X58_VENDOR_SCRIPT_ROLLBACK_FAILED	0xea

#define X58_EARLY_UART_BASE			0x3f8
#define X58_EARLY_UART_TX_POLL_LIMIT		100000
#define X58_EARLY_UART_FLUSH_POLL_LIMIT	1000000

#define PCI_VENDOR_ID_INTEL		0x8086
#define PCI_DEVICE_ID_ICH10R_SMBUS	0x3a30
#define PCI_ID_ICH10R_SMBUS \
	((PCI_DEVICE_ID_ICH10R_SMBUS << 16) | PCI_VENDOR_ID_INTEL)
#define ICH10R_LPC_DEV			PCI_DEV(0, 0x1f, 0)
#define ICH10R_SMBUS_DEV		PCI_DEV(0, 0x1f, 3)

#define X58_ROMSTAGE_LPC_IO_DEC			0x0010
#define X58_ROMSTAGE_LPC_EN			(CNF2_LPC_EN | COMA_LPC_EN)

#define X58_ROMSTAGE_RCBA_BASE_MASK		0xffffc000U
#define X58_ROMSTAGE_PMBASE_MASK			0xfffcU
#define X58_ROMSTAGE_GPIOBASE_MASK		0xfffeU
#define X58_RTC_POLICY_ICH10_RC			0x3400
#define X58_RTC_POLICY_ICH10_RC_U128E		BIT(2)


struct x58_romstage_ich10_snapshot {
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

struct x58_romstage_smbus_snapshot {
	uint32_t id;
	uint32_t bar4;
	uint16_t command;
	uint8_t hostc;
};

struct x58_rtc_policy_rtc_upper_bank_state {
	uint32_t before;
	uint32_t after;
	bool enabled;
};

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

static void capture_ich10(struct x58_romstage_ich10_snapshot *snapshot)
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

static void capture_smbus(struct x58_romstage_smbus_snapshot *snapshot)
{
	snapshot->id = pci_io_read_config32(ICH10R_SMBUS_DEV, PCI_VENDOR_ID);
	snapshot->bar4 = pci_io_read_config32(ICH10R_SMBUS_DEV, SMB_BASE);
	snapshot->command =
		pci_io_read_config16(ICH10R_SMBUS_DEV, PCI_COMMAND);
	snapshot->hostc = pci_io_read_config8(ICH10R_SMBUS_DEV, HOSTC);
}

static uint8_t ich10_preflight_error(const struct x58_romstage_ich10_snapshot *before)
{
	/* Never relocate a window that an earlier reset phase left decoding. */
	if ((before->rcba & 1) &&
	    (before->rcba & X58_ROMSTAGE_RCBA_BASE_MASK) != CONFIG_FIXED_RCBA_MMIO_BASE)
		return POST_X58_ROMSTAGE_RCBA_ERROR;
	if ((before->acpi_cntl & 0x80) &&
	    (before->pmbase & X58_ROMSTAGE_PMBASE_MASK) != DEFAULT_PMBASE)
		return POST_X58_ROMSTAGE_PMBASE_ERROR;
	if ((before->gpio_cntl & 0x10) &&
	    (before->gpiobase & X58_ROMSTAGE_GPIOBASE_MASK) != DEFAULT_GPIOBASE)
		return POST_X58_ROMSTAGE_GPIOBASE_ERROR;

	return 0;
}

static uint8_t ich10_readback_error(const struct x58_romstage_ich10_snapshot *before,
				    const struct x58_romstage_ich10_snapshot *after)
{
	size_t i;

	if ((after->rcba & X58_ROMSTAGE_RCBA_BASE_MASK) != CONFIG_FIXED_RCBA_MMIO_BASE ||
	    !(after->rcba & 1))
		return POST_X58_ROMSTAGE_RCBA_ERROR;
	if ((after->pmbase & X58_ROMSTAGE_PMBASE_MASK) != DEFAULT_PMBASE ||
	    !(after->pmbase & 1) ||
	    after->acpi_cntl != 0x80)
		return POST_X58_ROMSTAGE_PMBASE_ERROR;
	if ((after->gpiobase & X58_ROMSTAGE_GPIOBASE_MASK) != DEFAULT_GPIOBASE ||
	    after->gpio_cntl != (before->gpio_cntl | 0x10))
		return POST_X58_ROMSTAGE_GPIOBASE_ERROR;
	if (after->serirq_cntl != before->serirq_cntl ||
	    after->lpc_io_dec != X58_ROMSTAGE_LPC_IO_DEC ||
	    after->lpc_en != X58_ROMSTAGE_LPC_EN)
		return POST_X58_ROMSTAGE_LPC_INVARIANT_ERROR;

	/* This board intentionally has no generic LPC windows in X58_ROMSTAGE. */
	for (i = 0; i < ARRAY_SIZE(after->generic); i++) {
		if (after->generic[i] != before->generic[i] ||
		    (after->generic[i] & LPC_LGIR_EN))
			return POST_X58_ROMSTAGE_LPC_INVARIANT_ERROR;
	}

	return 0;
}

static bool smbus_readback_valid(const struct x58_romstage_smbus_snapshot *after)
{
	return after->id == PCI_ID_ICH10R_SMBUS &&
		(after->bar4 & ~PCI_BASE_ADDRESS_IO_ATTR_MASK) ==
			CONFIG_FIXED_SMBUS_IO_BASE &&
		(after->bar4 & PCI_BASE_ADDRESS_SPACE_IO) &&
		after->hostc == HST_EN && after->command == PCI_COMMAND_IO;
}

enum x58_vendor_smbus_entry_state {
	X58_VENDOR_SMBUS_ENTRY_COLD,
	X58_VENDOR_SMBUS_ENTRY_CONFIGURED_AFTER_RESET,
};

static bool smbus_is_exact_cold_default(const struct x58_romstage_smbus_snapshot *before)
{
	return before->bar4 == PCI_BASE_ADDRESS_SPACE_IO &&
		before->hostc == 0 && before->command == 0;
}

static bool smbus_is_exact_configured(const struct x58_romstage_smbus_snapshot *before)
{
	return before->bar4 ==
		(CONFIG_FIXED_SMBUS_IO_BASE | PCI_BASE_ADDRESS_SPACE_IO) &&
		before->hostc == HST_EN && before->command == PCI_COMMAND_IO;
}

static bool x58_romstage_uart_wait_for(uint8_t mask, unsigned int limit)
{
	while (limit--) {
		if ((inb(X58_EARLY_UART_BASE + UART8250_LSR) & mask) == mask)
			return true;
	}

	return false;
}

static bool x58_romstage_uart_putc(uint8_t value)
{
	if (!x58_romstage_uart_wait_for(UART8250_LSR_THRE, X58_EARLY_UART_TX_POLL_LIMIT))
		return false;
	outb(value, X58_EARLY_UART_BASE + UART8250_TBR);
	return true;
}

static bool x58_romstage_uart_puts(const char *string)
{
	while (*string) {
		if (!x58_romstage_uart_putc(*string++))
			return false;
	}

	return true;
}

static bool x58_romstage_uart_detailed(void)
{
	return CONFIG_DEFAULT_CONSOLE_LOGLEVEL >= BIOS_DEBUG;
}

static bool x58_romstage_uart_put_hex(uint32_t value, unsigned int digits)
{
	static const char hex[] = "0123456789abcdef";

	while (digits--) {
		const unsigned int shift = digits * 4;

		if (!x58_romstage_uart_putc(hex[(value >> shift) & 0xf]))
			return false;
	}

	return true;
}

static void x58_rtc_policy_enable_rtc_upper_bank(
	struct x58_rtc_policy_rtc_upper_bank_state *state)
{
	outb(POST_X58_RTC_POLICY_RTC_PRECHECK, CONFIG_POST_IO_PORT);
	state->before = RCBA32(X58_RTC_POLICY_ICH10_RC);
	state->after = state->before;
	state->enabled = false;

	/*
	 * ICH10 Datasheet 319973-003 sections 10.1.73 and 13.6: RC bit 2
	 * selects the upper 128-byte RTC bank at ports 0x72/0x73.  Accept only
	 * the two states observed on the target and vendor-booted reference.
	 */
	if (state->before != 0 && state->before != X58_RTC_POLICY_ICH10_RC_U128E)
		return;
	RCBA32(X58_RTC_POLICY_ICH10_RC) = state->before | X58_RTC_POLICY_ICH10_RC_U128E;
	state->after = RCBA32(X58_RTC_POLICY_ICH10_RC);
	if (state->after != X58_RTC_POLICY_ICH10_RC_U128E)
		return;

	state->enabled = true;
	outb(POST_X58_RTC_POLICY_RTC_ENABLED, CONFIG_POST_IO_PORT);
}

static bool x58_rtc_policy_report_rtc_upper_bank(
	const struct x58_rtc_policy_rtc_upper_bank_state *state)
{
	return x58_romstage_uart_puts("\r\n[ICH10] RTC_RC PRE=") &&
		x58_romstage_uart_put_hex(state->before, 8) &&
		x58_romstage_uart_puts(" POST=") && x58_romstage_uart_put_hex(state->after, 8) &&
		x58_romstage_uart_puts(" U128E=") &&
		x58_romstage_uart_puts(state->enabled ? "PASS" : "FAIL") &&
		x58_romstage_uart_puts(
			"; exact gate before vendor code; no RTC data read/write\r\n");
}

static bool report_ich10(const struct x58_romstage_ich10_snapshot *before,
			 const struct x58_romstage_ich10_snapshot *after,
			 const struct x58_romstage_smbus_snapshot *smbus_before,
			 const struct x58_romstage_smbus_snapshot *smbus_after)
{
	if (!x58_romstage_uart_detailed())
		return true;

	if (!x58_romstage_uart_puts("\r\n[") ||
	    !x58_romstage_uart_puts(CONFIG_MAINBOARD_PART_NUMBER) ||
	    !x58_romstage_uart_puts("]\r\n[ICH10] upstream i82801jx early BAR core\r\n"
			   "[ICH10] PRE  RCBA="))
		return false;
	if (!x58_romstage_uart_put_hex(before->rcba, 8) ||
	    !x58_romstage_uart_puts(" PMBASE=") || !x58_romstage_uart_put_hex(before->pmbase, 8) ||
	    !x58_romstage_uart_puts(" ACPI=") || !x58_romstage_uart_put_hex(before->acpi_cntl, 2) ||
	    !x58_romstage_uart_puts(" GPIOBASE=") || !x58_romstage_uart_put_hex(before->gpiobase, 8) ||
	    !x58_romstage_uart_puts(" GPIOCNTL=") || !x58_romstage_uart_put_hex(before->gpio_cntl, 2) ||
	    !x58_romstage_uart_puts(" SERIRQ=") || !x58_romstage_uart_put_hex(before->serirq_cntl, 2) ||
	    !x58_romstage_uart_puts(" LPCIO=") || !x58_romstage_uart_put_hex(before->lpc_io_dec, 4) ||
	    !x58_romstage_uart_puts(" LPCEN=") || !x58_romstage_uart_put_hex(before->lpc_en, 4))
		return false;
	if (!x58_romstage_uart_puts("\r\n[ICH10] POST RCBA=") ||
	    !x58_romstage_uart_put_hex(after->rcba, 8) ||
	    !x58_romstage_uart_puts(" PMBASE=") || !x58_romstage_uart_put_hex(after->pmbase, 8) ||
	    !x58_romstage_uart_puts(" ACPI=") || !x58_romstage_uart_put_hex(after->acpi_cntl, 2) ||
	    !x58_romstage_uart_puts(" GPIOBASE=") || !x58_romstage_uart_put_hex(after->gpiobase, 8) ||
	    !x58_romstage_uart_puts(" GPIOCNTL=") || !x58_romstage_uart_put_hex(after->gpio_cntl, 2) ||
	    !x58_romstage_uart_puts(" SERIRQ=") || !x58_romstage_uart_put_hex(after->serirq_cntl, 2) ||
	    !x58_romstage_uart_puts(" LPCIO=") || !x58_romstage_uart_put_hex(after->lpc_io_dec, 4) ||
	    !x58_romstage_uart_puts(" LPCEN=") || !x58_romstage_uart_put_hex(after->lpc_en, 4))
		return false;
	if (!x58_romstage_uart_puts("\r\n[SMBUS] ID=8086:3a30 PRE BAR4=") ||
	    !x58_romstage_uart_put_hex(smbus_before->bar4, 8) ||
	    !x58_romstage_uart_puts(" HOSTC=") || !x58_romstage_uart_put_hex(smbus_before->hostc, 2) ||
	    !x58_romstage_uart_puts(" CMD=") || !x58_romstage_uart_put_hex(smbus_before->command, 4) ||
	    !x58_romstage_uart_puts(" POST BAR4=") || !x58_romstage_uart_put_hex(smbus_after->bar4, 8) ||
	    !x58_romstage_uart_puts(" HOSTC=") || !x58_romstage_uart_put_hex(smbus_after->hostc, 2) ||
	    !x58_romstage_uart_puts(" CMD=") || !x58_romstage_uart_put_hex(smbus_after->command, 4))
		return false;

	return x58_romstage_uart_puts("\r\n[ROMMON] controller ready; interactive CAR monitor follows\r\n") &&
		x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
				  X58_EARLY_UART_FLUSH_POLL_LIMIT);
}

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

#define X58_SPD_READ_I801_POLL_LIMIT		1000000U
#define X58_SPD_READ_PIN_POLL_LIMIT		100000U
#define X58_SPD_READ_SPD_ADDRESS		0x54
#define X58_SPD_READ_SPD_SIZE			128
#define X58_SPD_READ_SPD_MEMORY_TYPE_OFFSET	0x02
#define X58_SPD_READ_SPD_DDR3_MEMORY_TYPE	0x0b

#define X58_PLATFORM_SPD_BUFFER_SIZE		SPD_SIZE_MAX_DDR3
#define X58_SPD_TIMING_SPD_USED_SIZE		SPD_SIZE_MAX_DDR3
#define POST_X58_SPD_CRC_HEADER_OK		POST_X58_PLATFORMI_HEADER_OK
#define POST_X58_SPD_CRC_UPPER_PASS1_BEGIN	POST_X58_PLATFORMI_UPPER_PASS1_BEGIN
#define POST_X58_SPD_CRC_PASS1_BLOCK_BASE	POST_X58_PLATFORMI_PASS1_BLOCK_BASE
#define POST_X58_SPD_CRC_UPPER_PASS2_BEGIN	POST_X58_PLATFORMI_UPPER_PASS2_BEGIN
#define POST_X58_SPD_CRC_PASS2_BLOCK_BASE	POST_X58_PLATFORMI_PASS2_BLOCK_BASE
#define POST_X58_SPD_CRC_UPPER_MATCH		POST_X58_PLATFORMI_UPPER_MATCH
#define POST_X58_SPD_CRC_DECODE_OK		POST_X58_PLATFORMI_DECODE_OK
#define POST_X58_SPD_CRC_POLICY_OK		POST_X58_PLATFORMI_POLICY_OK
#define POST_X58_SPD_CRC_HEADER_ERROR		POST_X58_PLATFORMI_HEADER_ERROR
#define POST_X58_SPD_CRC_UPPER_MISMATCH	POST_X58_PLATFORMI_UPPER_MISMATCH
#define POST_X58_SPD_CRC_DECODE_ERROR		POST_X58_PLATFORMI_DECODE_ERROR
#define POST_X58_SPD_CRC_POLICY_ERROR		POST_X58_PLATFORMI_POLICY_ERROR
#define X58_SPD_CRC_UPPER_CRC_LABEL		"[SPD] UNPROTECTED_UPPER128_CRC16="
#define X58_SPD_CRC_USED_CRC_LABEL		" USED256_CRC16="
#define X58_SPD_TIMING_SPD_UPPER_OFFSET		128
#define X58_SPD_TIMING_SPD_UPPER_SIZE		(X58_SPD_TIMING_SPD_USED_SIZE - X58_SPD_TIMING_SPD_UPPER_OFFSET)
#define X58_SPD_TIMING_TARGET_TCK			TCK_400MHZ
#define X58_SPD_TIMING_MIN_CAS			4
#define X58_SPD_TIMING_MAX_CAS			18
#define X58_SPD_TIMING_MIN_TRRD_CYCLES		4
#define X58_SPD_TIMING_MIN_TWTR_CYCLES		4
#define X58_SPD_TIMING_MIN_TRTP_CYCLES		4

struct x58_spd_timing_policy {
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

struct x58_spd_read_result {
	uint8_t spd[X58_PLATFORM_SPD_BUFFER_SIZE];
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
	struct dimm_attr_ddr3_st dimm;
	struct x58_spd_timing_policy policy;
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
};

static uint8_t x58_spd_read_pin_error(uint8_t pin)
{
	const bool clock_error = !(pin & I801_PIN_CLK_CTL) ||
		!(pin & I801_PIN_CLK_STATUS);
	const bool data_error = !(pin & I801_PIN_DATA_STATUS);

	if (clock_error && data_error)
		return POST_X58_PLATFORM_BOTH_PINS_ERROR;
	if (clock_error)
		return POST_X58_PLATFORM_CLOCK_ERROR;
	if (data_error)
		return POST_X58_PLATFORM_DATA_ERROR;

	return 0;
}

static void x58_spd_read_release_host(uint8_t status)
{
	const uintptr_t base = CONFIG_FIXED_SMBUS_IO_BASE;

	/* INUSE releases ownership; completion flags are write-one-to-clear. */
	outb(I801_HSTSTAT_INUSE | (status & I801_HSTSTAT_FLAGS),
	     base + I801_HSTSTAT);
}

static bool x58_spd_read_control_idle(uint8_t control)
{
	if (control == 0)
		return true;
	/* I801 retains the completed byte-data protocol bits while idle. */
	if (control == I801_BYTE_DATA)
		return true;
	return false;
}

static uint8_t x58_spd_timing_read_byte(struct x58_spd_read_result *result, uint8_t offset,
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
	terminal = x58_spd_read_pin_error(result->pin_before);
	if (terminal) {
		x58_spd_read_release_host(0);
		return terminal;
	}

	outb(I801_BYTE_DATA, base + I801_HSTCTL);
	outb((X58_SPD_READ_SPD_ADDRESS << 1) | 1, base + I801_XMITADD);
	outb(offset, base + I801_HSTCMD);
	outb(0, base + I801_HSTDAT0);
	outb(0, base + I801_HSTDAT1);
	if (inb(base + I801_HSTCTL) != I801_BYTE_DATA ||
	    inb(base + I801_XMITADD) != ((X58_SPD_READ_SPD_ADDRESS << 1) | 1) ||
	    inb(base + I801_HSTCMD) != offset ||
	    inb(base + I801_HSTDAT0) != 0 ||
	    inb(base + I801_HSTDAT1) != 0) {
		x58_spd_read_release_host(0);
		return POST_X58_PLATFORM_HOST_STATE_ERROR;
	}

	outb(I801_BYTE_DATA | I801_HSTCTL_START, base + I801_HSTCTL);
	for (loops = X58_SPD_READ_I801_POLL_LIMIT; loops; loops--) {
		result->last_status = inb(base + I801_HSTSTAT);
		result->last_status_valid = true;
		if (!(result->last_status & I801_HSTSTAT_HOST_BUSY) &&
		    (result->last_status & I801_HSTSTAT_TERMINAL))
			break;
	}
	result->last_polls = loops ? X58_SPD_READ_I801_POLL_LIMIT - loops + 1 :
		X58_SPD_READ_I801_POLL_LIMIT;
	result->total_polls += result->last_polls;
	if (result->last_polls > result->maximum_polls)
		result->maximum_polls = result->last_polls;
	if (!loops)
		/* Do not touch any I801 register after an unresolved timeout. */
		return POST_X58_PLATFORM_TIMEOUT;

	masked_status = result->last_status & I801_HSTSTAT_RESULT;
	if (masked_status == I801_HSTSTAT_INTR)
		*value = inb(base + I801_HSTDAT0);
	result->pin_after = inb(base + I801_PIN_CTL);
	result->pin_after_valid = true;
	/* Clear this command's completion flags but retain semaphore ownership. */
	outb(result->last_status & I801_HSTSTAT_FLAGS,
	     base + I801_HSTSTAT);

	terminal = x58_spd_read_pin_error(result->pin_after);
	if (terminal) {
		x58_spd_read_release_host(0);
		return terminal;
	}
	if (masked_status != I801_HSTSTAT_INTR) {
		x58_spd_read_release_host(0);
		if (masked_status == I801_HSTSTAT_DEV_ERR)
			return POST_X58_SPD_READ_DEV_ERROR;
		if (masked_status == I801_HSTSTAT_BUS_ERR)
			return POST_X58_PLATFORM_BUS_ERROR;
		return POST_X58_PLATFORM_TRANSACTION_ERROR;
	}

	return 0;
}

static bool x58_spd_timing_decoder_input_valid(const uint8_t *spd)
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

static bool x58_spd_timing_to_cycles(uint32_t timing, uint16_t tck,
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

static bool x58_spd_timing_derive_policy(struct x58_spd_read_result *result)
{
	const struct dimm_attr_ddr3_st *dimm = &result->dimm;
	struct x58_spd_timing_policy *policy = &result->policy;
	unsigned int cas;
	uint16_t taa_cycles;

	/* Keep the first writable path constrained to the observed module. */
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
	    dimm->tCK > X58_SPD_TIMING_TARGET_TCK)
		return false;

	if (!x58_spd_timing_to_cycles(dimm->tAA, X58_SPD_TIMING_TARGET_TCK, &taa_cycles))
		return false;
	cas = MAX(X58_SPD_TIMING_MIN_CAS, taa_cycles);
	for (; cas <= X58_SPD_TIMING_MAX_CAS; cas++) {
		if (dimm->cas_supported & BIT(cas - X58_SPD_TIMING_MIN_CAS))
			break;
	}
	if (cas > X58_SPD_TIMING_MAX_CAS || cas * X58_SPD_TIMING_TARGET_TCK >= 20 * 256)
		return false;

	policy->tck = X58_SPD_TIMING_TARGET_TCK;
	policy->cas = cas;
	if (!x58_spd_timing_to_cycles(dimm->tRCD, policy->tck, &policy->trcd) ||
	    !x58_spd_timing_to_cycles(dimm->tRP, policy->tck, &policy->trp) ||
	    !x58_spd_timing_to_cycles(dimm->tRAS, policy->tck, &policy->tras) ||
	    !x58_spd_timing_to_cycles(dimm->tRC, policy->tck, &policy->trc) ||
	    !x58_spd_timing_to_cycles(dimm->tRFC, policy->tck, &policy->trfc) ||
	    !x58_spd_timing_to_cycles(dimm->tWR, policy->tck, &policy->twr) ||
	    !x58_spd_timing_to_cycles(dimm->tRRD, policy->tck, &policy->trrd) ||
	    !x58_spd_timing_to_cycles(dimm->tWTR, policy->tck, &policy->twtr) ||
	    !x58_spd_timing_to_cycles(dimm->tRTP, policy->tck, &policy->trtp) ||
	    !x58_spd_timing_to_cycles(dimm->tFAW, policy->tck, &policy->tfaw))
		return false;

	/* DDR3 also specifies cycle-count floors beyond the absolute-time SPD. */
	policy->trrd = MAX(policy->trrd, X58_SPD_TIMING_MIN_TRRD_CYCLES);
	policy->twtr = MAX(policy->twtr, X58_SPD_TIMING_MIN_TWTR_CYCLES);
	policy->trtp = MAX(policy->trtp, X58_SPD_TIMING_MIN_TRTP_CYCLES);

	return true;
}

static uint8_t x58_spd_timing_read_upper_and_decode(struct x58_spd_read_result *result)
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
	if (result->declared_used != X58_SPD_TIMING_SPD_USED_SIZE ||
	    result->declared_total != SPD_SIZE_MAX_DDR3) {
		x58_spd_read_release_host(0);
		return POST_X58_SPD_CRC_HEADER_ERROR;
	}
	result->header_valid = true;
	outb(POST_X58_SPD_CRC_HEADER_OK, CONFIG_POST_IO_PORT);
	outb(POST_X58_SPD_CRC_UPPER_PASS1_BEGIN, CONFIG_POST_IO_PORT);

	for (offset = X58_SPD_TIMING_SPD_UPPER_OFFSET; offset < X58_SPD_TIMING_SPD_USED_SIZE;
	     offset++) {
		terminal = x58_spd_timing_read_byte(result, offset, &result->spd[offset]);
		if (terminal)
			return terminal;
		result->unique_completed = offset + 1;
		if ((offset & 0xf) == 0xf)
			outb(POST_X58_SPD_CRC_PASS1_BLOCK_BASE +
			     ((offset - X58_SPD_TIMING_SPD_UPPER_OFFSET) >> 4),
			     CONFIG_POST_IO_PORT);
	}

	outb(POST_X58_SPD_CRC_UPPER_PASS2_BEGIN, CONFIG_POST_IO_PORT);
	for (offset = X58_SPD_TIMING_SPD_UPPER_OFFSET; offset < X58_SPD_TIMING_SPD_USED_SIZE;
	     offset++) {
		uint8_t verification;

		terminal = x58_spd_timing_read_byte(result, offset, &verification);
		if (terminal)
			return terminal;
		result->verification_completed =
			offset - X58_SPD_TIMING_SPD_UPPER_OFFSET + 1;
		if (verification != result->spd[offset]) {
			result->mismatch_offset = offset;
			result->mismatch_first = result->spd[offset];
			result->mismatch_second = verification;
			result->mismatch_valid = true;
			x58_spd_read_release_host(0);
			return POST_X58_SPD_CRC_UPPER_MISMATCH;
		}
		if ((offset & 0xf) == 0xf)
			outb(POST_X58_SPD_CRC_PASS2_BLOCK_BASE +
			     ((offset - X58_SPD_TIMING_SPD_UPPER_OFFSET) >> 4),
			     CONFIG_POST_IO_PORT);
	}

	/* All SMBus work is complete before any semantic decoding. */
	x58_spd_read_release_host(0);
	result->upper_match = true;
	outb(POST_X58_SPD_CRC_UPPER_MATCH, CONFIG_POST_IO_PORT);
	result->upper_crc = ddr_crc16(&result->spd[X58_SPD_TIMING_SPD_UPPER_OFFSET],
				      X58_SPD_TIMING_SPD_UPPER_SIZE);
	result->used_crc = ddr_crc16(result->spd, X58_SPD_TIMING_SPD_USED_SIZE);
	result->fingerprint_valid = true;

	/* The decoder has no length argument and is not fail-fast on all fields. */
	if (!x58_spd_timing_decoder_input_valid(result->spd))
		return POST_X58_SPD_CRC_DECODE_ERROR;
	result->decode_attempted = true;
	result->decode_status = spd_decode_ddr3(&result->dimm, result->spd);
	if (result->decode_status != SPD_STATUS_OK)
		return POST_X58_SPD_CRC_DECODE_ERROR;
	result->decode_valid = true;
	outb(POST_X58_SPD_CRC_DECODE_OK, CONFIG_POST_IO_PORT);

	if (!x58_spd_timing_derive_policy(result))
		return POST_X58_SPD_CRC_POLICY_ERROR;
	result->policy_valid = true;
	outb(POST_X58_SPD_CRC_POLICY_OK, CONFIG_POST_IO_PORT);

	return 0;
}

static uint8_t x58_spd_read_base_spd(struct x58_spd_read_result *result)
{
	const uintptr_t base = CONFIG_FIXED_SMBUS_IO_BASE;
	unsigned int offset;
	uint8_t terminal;

	/* The first HSTSTAT read acquires the I801 software semaphore. */
	result->initial_status = inb(base + I801_HSTSTAT);
	if (result->initial_status & I801_HSTSTAT_INUSE)
		return POST_X58_PLATFORM_HOST_STATE_ERROR;
	if (result->initial_status & I801_HSTSTAT_HOST_BUSY) {
		/* This read acquired a previously free semaphore; give it back. */
		x58_spd_read_release_host(0);
		return POST_X58_PLATFORM_HOST_STATE_ERROR;
	}

	result->initial_control = inb(base + I801_HSTCTL);
	result->initial_control_valid = true;
	if ((result->initial_status & I801_HSTSTAT_FLAGS) ||
	    !x58_spd_read_control_idle(result->initial_control)) {
		x58_spd_read_release_host(0);
		return POST_X58_PLATFORM_HOST_STATE_ERROR;
	}

	outb(POST_X58_PLATFORM_PIN_PHASE, CONFIG_POST_IO_PORT);
	result->initial_pin = inb(base + I801_PIN_CTL);
	result->initial_pin_valid = true;
	/* Release SMBCLK; bits 1:0 are read-only levels and reserved bits are zero. */
	outb(I801_PIN_CLK_CTL, base + I801_PIN_CTL);
	outb(POST_X58_PLATFORM_CLOCK_RELEASED, CONFIG_POST_IO_PORT);
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
	} while (result->pin_polls < X58_SPD_READ_PIN_POLL_LIMIT);
	terminal = x58_spd_read_pin_error(result->enabled_pin);
	if (terminal) {
		x58_spd_read_release_host(0);
		return terminal;
	}
	outb(POST_X58_PLATFORM_PINS_IDLE, CONFIG_POST_IO_PORT);
	outb(POST_X58_SPD_READ_READ_BEGIN, CONFIG_POST_IO_PORT);

	/* Keep one semaphore across exactly 128 one-shot read-byte-data commands. */
	for (offset = 0; offset < X58_SPD_READ_SPD_SIZE; offset++) {
		uint32_t loops;
		uint8_t masked_status;

		result->last_offset = offset;
		result->last_offset_valid = true;
		result->last_status_valid = false;
		result->pin_after_valid = false;
		result->last_polls = 0;
		result->pin_before = inb(base + I801_PIN_CTL);
		result->pin_before_valid = true;
		terminal = x58_spd_read_pin_error(result->pin_before);
		if (terminal) {
			x58_spd_read_release_host(0);
			return terminal;
		}

		outb(I801_BYTE_DATA, base + I801_HSTCTL);
		outb((X58_SPD_READ_SPD_ADDRESS << 1) | 1, base + I801_XMITADD);
		outb(offset, base + I801_HSTCMD);
		outb(0, base + I801_HSTDAT0);
		outb(0, base + I801_HSTDAT1);
		if (inb(base + I801_HSTCTL) != I801_BYTE_DATA ||
		    inb(base + I801_XMITADD) != ((X58_SPD_READ_SPD_ADDRESS << 1) | 1) ||
		    inb(base + I801_HSTCMD) != offset ||
		    inb(base + I801_HSTDAT0) != 0 ||
		    inb(base + I801_HSTDAT1) != 0) {
			x58_spd_read_release_host(0);
			return POST_X58_PLATFORM_HOST_STATE_ERROR;
		}

		outb(I801_BYTE_DATA | I801_HSTCTL_START, base + I801_HSTCTL);
		for (loops = X58_SPD_READ_I801_POLL_LIMIT; loops; loops--) {
			result->last_status = inb(base + I801_HSTSTAT);
			result->last_status_valid = true;
			if (!(result->last_status & I801_HSTSTAT_HOST_BUSY) &&
			    (result->last_status & I801_HSTSTAT_TERMINAL))
				break;
		}
		result->last_polls = loops ? X58_SPD_READ_I801_POLL_LIMIT - loops + 1 :
			X58_SPD_READ_I801_POLL_LIMIT;
		result->total_polls += result->last_polls;
		if (result->last_polls > result->maximum_polls)
			result->maximum_polls = result->last_polls;
		if (!loops)
			/* Do not touch any I801 register after an unresolved timeout. */
			return POST_X58_PLATFORM_TIMEOUT;

		masked_status = result->last_status & I801_HSTSTAT_RESULT;
		if (masked_status == I801_HSTSTAT_INTR)
			result->spd[offset] = inb(base + I801_HSTDAT0);
		result->pin_after = inb(base + I801_PIN_CTL);
		result->pin_after_valid = true;
		/* Clear this command's completion flags but retain semaphore ownership. */
		outb(result->last_status & I801_HSTSTAT_FLAGS,
		     base + I801_HSTSTAT);

		terminal = x58_spd_read_pin_error(result->pin_after);
		if (terminal) {
			x58_spd_read_release_host(0);
			return terminal;
		}
		if (masked_status != I801_HSTSTAT_INTR) {
			x58_spd_read_release_host(0);
			if (masked_status == I801_HSTSTAT_DEV_ERR)
				return POST_X58_SPD_READ_DEV_ERROR;
			if (masked_status == I801_HSTSTAT_BUS_ERR)
				return POST_X58_PLATFORM_BUS_ERROR;
			return POST_X58_PLATFORM_TRANSACTION_ERROR;
		}

		result->completed = offset + 1;
		if (offset == X58_SPD_READ_SPD_MEMORY_TYPE_OFFSET) {
			if (result->spd[offset] != X58_SPD_READ_SPD_DDR3_MEMORY_TYPE) {
				x58_spd_read_release_host(0);
				return POST_X58_SPD_READ_NOT_DDR3;
			}
			outb(POST_X58_SPD_READ_TYPE_OK, CONFIG_POST_IO_PORT);
		}
		if ((offset & 0xf) == 0xf)
			outb(POST_X58_SPD_READ_BLOCK_DONE_BASE + (offset >> 4),
			     CONFIG_POST_IO_PORT);
	}

	/* The last completed transaction already supplied the final pin sample. */
	result->unique_completed = result->completed;
	result->crc_coverage = (result->spd[0] & BIT(7)) ? 117 : 126;
	result->crc_calculated = ddr_crc16(result->spd, result->crc_coverage);
	result->crc_stored = result->spd[126] |
		((uint16_t)result->spd[127] << 8);
	result->crc_valid = true;
	if (result->crc_calculated != result->crc_stored) {
		x58_spd_read_release_host(0);
		return POST_X58_SPD_READ_CRC_ERROR;
	}

	outb(POST_X58_SPD_READ_CRC_OK, CONFIG_POST_IO_PORT);
	return x58_spd_timing_read_upper_and_decode(result);
}

static bool x58_spd_read_report_optional_byte(bool valid, uint8_t value)
{
	if (!valid)
		return x58_romstage_uart_puts("NA");

	return x58_romstage_uart_put_hex(value, 2);
}


static bool x58_spd_timing_report_part_number(const struct x58_spd_read_result *result)
{
	unsigned int offset;

	if (!x58_romstage_uart_puts("[SPD] PART="))
		return false;
	for (offset = SPD_DDR3_PART_NUM;
	     offset < SPD_DDR3_PART_NUM + SPD_DDR3_PART_LEN; offset++) {
		uint8_t value = result->spd[offset];

		if (value < 0x20 || value > 0x7e)
			value = '.';
		if (!x58_romstage_uart_putc(value))
			return false;
	}

	return x58_romstage_uart_puts("\r\n");
}

static bool x58_spd_timing_report(const struct x58_spd_read_result *result, uint8_t terminal)
{
	const unsigned int unique = result->unique_completed ?
		result->unique_completed : result->completed;
	unsigned int offset;

	if (!x58_romstage_uart_puts("[SMBUS] PRE STS=") ||
	    !x58_romstage_uart_put_hex(result->initial_status, 2) ||
	    !x58_romstage_uart_puts(" CTL=") ||
	    !x58_spd_read_report_optional_byte(result->initial_control_valid,
				       result->initial_control) ||
	    !x58_romstage_uart_puts(" PIN_PRE=") ||
	    !x58_spd_read_report_optional_byte(result->initial_pin_valid,
				       result->initial_pin) ||
	    !x58_romstage_uart_puts(" PIN_ENABLED=") ||
	    !x58_spd_read_report_optional_byte(result->enabled_pin_valid,
				       result->enabled_pin) ||
	    !x58_romstage_uart_puts(" PIN_POLLS=") ||
	    !x58_romstage_uart_put_hex(result->pin_polls, 8) ||
	    !x58_romstage_uart_puts("\r\n[SPD] A=54 UNIQUE=") ||
	    !x58_romstage_uart_put_hex(unique, 4) ||
	    !x58_romstage_uart_puts(" VERIFY=") ||
	    !x58_romstage_uart_put_hex(result->verification_completed, 4) ||
	    !x58_romstage_uart_puts(" LAST_OFF=") ||
	    !x58_spd_read_report_optional_byte(result->last_offset_valid,
				       result->last_offset) ||
	    !x58_romstage_uart_puts(" STS=") ||
	    !x58_spd_read_report_optional_byte(result->last_status_valid,
				       result->last_status) ||
	    !x58_romstage_uart_puts(" POLLS=") ||
	    !x58_romstage_uart_put_hex(result->last_polls, 8) ||
	    !x58_romstage_uart_puts(" TOTAL=") ||
	    !x58_romstage_uart_put_hex(result->total_polls, 8) ||
	    !x58_romstage_uart_puts(" MAX=") ||
	    !x58_romstage_uart_put_hex(result->maximum_polls, 8) ||
	    !x58_romstage_uart_puts(" PIN0=") ||
	    !x58_spd_read_report_optional_byte(result->pin_before_valid,
				       result->pin_before) ||
	    !x58_romstage_uart_puts(" PIN1=") ||
	    !x58_spd_read_report_optional_byte(result->pin_after_valid,
				       result->pin_after) ||
	    !x58_romstage_uart_puts("\r\n"))
		return false;

	if (result->completed >= 3 &&
	    (!x58_romstage_uart_puts("[SPD] B0=") ||
	     !x58_romstage_uart_put_hex(result->spd[0], 2) ||
	     !x58_romstage_uart_puts(" REV=") ||
	     !x58_romstage_uart_put_hex(result->spd[1], 2) ||
	     !x58_romstage_uart_puts(" TYPE=") ||
	     !x58_romstage_uart_put_hex(result->spd[2], 2) ||
	     !x58_romstage_uart_puts(" USED=") ||
	     !x58_romstage_uart_put_hex(result->declared_used, 4) ||
	     !x58_romstage_uart_puts(" TOTAL=") ||
	     !x58_romstage_uart_put_hex(result->declared_total, 4) ||
	     !x58_romstage_uart_puts(result->header_valid ? " HEADER_OK\r\n" :
						   " HEADER_BAD\r\n")))
		return false;

	for (offset = 0; offset < unique; offset++) {
		if ((offset & 0xf) == 0 &&
		    (!x58_romstage_uart_puts("[SPD] ") ||
		     !x58_romstage_uart_put_hex(offset, 2) || !x58_romstage_uart_putc(':')))
			return false;
		if (!x58_romstage_uart_put_hex(result->spd[offset], 2))
			return false;
		if ((offset & 0xf) == 0xf || offset + 1 == unique) {
			if (!x58_romstage_uart_puts("\r\n"))
				return false;
		}
	}

	if (unique >= SPD_DDR3_PART_NUM + SPD_DDR3_PART_LEN &&
	    !x58_spd_timing_report_part_number(result))
		return false;
	if (result->crc_valid &&
	    (!x58_romstage_uart_puts("[SPD] BASE_CRC COVER_HEX=") ||
	     !x58_romstage_uart_put_hex(result->crc_coverage, 2) ||
	     !x58_romstage_uart_puts(" CALC=") ||
	     !x58_romstage_uart_put_hex(result->crc_calculated, 4) ||
	     !x58_romstage_uart_puts(" STORED=") ||
	     !x58_romstage_uart_put_hex(result->crc_stored, 4) ||
	     !x58_romstage_uart_puts(result->crc_calculated == result->crc_stored ?
			      " OK\r\n" : " MISMATCH\r\n")))
		return false;
	if (result->fingerprint_valid &&
	    (!x58_romstage_uart_puts(X58_SPD_CRC_UPPER_CRC_LABEL) ||
	     !x58_romstage_uart_put_hex(result->upper_crc, 4) ||
	     !x58_romstage_uart_puts(X58_SPD_CRC_USED_CRC_LABEL) ||
	     !x58_romstage_uart_put_hex(result->used_crc, 4) ||
	     !x58_romstage_uart_puts(" DOUBLE_READ_MATCH\r\n")))
		return false;
	if (result->mismatch_valid &&
	    (!x58_romstage_uart_puts("[SPD] VERIFY_MISMATCH OFF=") ||
	     !x58_romstage_uart_put_hex(result->mismatch_offset, 2) ||
	     !x58_romstage_uart_puts(" FIRST=") ||
	     !x58_romstage_uart_put_hex(result->mismatch_first, 2) ||
	     !x58_romstage_uart_puts(" SECOND=") ||
	     !x58_romstage_uart_put_hex(result->mismatch_second, 2) ||
	     !x58_romstage_uart_puts("\r\n")))
		return false;

	if (result->decode_attempted &&
	    (!x58_romstage_uart_puts("[SPD] DECODE_STATUS=") ||
	     !x58_romstage_uart_put_hex(result->decode_status, 2) ||
	     !x58_romstage_uart_puts("\r\n")))
		return false;
	if (result->decode_valid &&
	    (!x58_romstage_uart_puts("[SPD] SIZE_MIB=") ||
	     !x58_romstage_uart_put_hex(result->dimm.size_mb, 8) ||
	     !x58_romstage_uart_puts(" RANKS=") ||
	     !x58_romstage_uart_put_hex(result->dimm.ranks, 2) ||
	     !x58_romstage_uart_puts(" WIDTH=") ||
	     !x58_romstage_uart_put_hex(result->dimm.width, 2) ||
	     !x58_romstage_uart_puts(" ROW=") ||
	     !x58_romstage_uart_put_hex(result->dimm.row_bits, 2) ||
	     !x58_romstage_uart_puts(" COL=") ||
	     !x58_romstage_uart_put_hex(result->dimm.col_bits, 2) ||
	     !x58_romstage_uart_puts(" FLAGS=") ||
	     !x58_romstage_uart_put_hex(result->dimm.flags.raw, 8) ||
	     !x58_romstage_uart_puts(" CASMAP=") ||
	     !x58_romstage_uart_put_hex(result->dimm.cas_supported, 4) ||
	     !x58_romstage_uart_puts(" TCK=") ||
	     !x58_romstage_uart_put_hex(result->dimm.tCK, 8) ||
	     !x58_romstage_uart_puts(" TAA=") ||
	     !x58_romstage_uart_put_hex(result->dimm.tAA, 8) ||
	     !x58_romstage_uart_puts("\r\n")))
		return false;

	if (result->policy_valid &&
	    (!x58_romstage_uart_puts("[SPD] DDR3-800 JEDEC_CYCLE_CANDIDATE TCK=") ||
	     !x58_romstage_uart_put_hex(result->policy.tck, 4) ||
	     !x58_romstage_uart_puts(" CL=") ||
	     !x58_romstage_uart_put_hex(result->policy.cas, 2) ||
	     !x58_romstage_uart_puts(" RCD=") ||
	     !x58_romstage_uart_put_hex(result->policy.trcd, 2) ||
	     !x58_romstage_uart_puts(" RP=") ||
	     !x58_romstage_uart_put_hex(result->policy.trp, 2) ||
	     !x58_romstage_uart_puts(" RAS=") ||
	     !x58_romstage_uart_put_hex(result->policy.tras, 2) ||
	     !x58_romstage_uart_puts(" RC=") ||
	     !x58_romstage_uart_put_hex(result->policy.trc, 2) ||
	     !x58_romstage_uart_puts(" RFC=") ||
	     !x58_romstage_uart_put_hex(result->policy.trfc, 2) ||
	     !x58_romstage_uart_puts(" WR=") ||
	     !x58_romstage_uart_put_hex(result->policy.twr, 2) ||
	     !x58_romstage_uart_puts(" RRD=") ||
	     !x58_romstage_uart_put_hex(result->policy.trrd, 2) ||
	     !x58_romstage_uart_puts(" WTR=") ||
	     !x58_romstage_uart_put_hex(result->policy.twtr, 2) ||
	     !x58_romstage_uart_puts(" RTP=") ||
	     !x58_romstage_uart_put_hex(result->policy.trtp, 2) ||
	     !x58_romstage_uart_puts(" FAW=") ||
	     !x58_romstage_uart_put_hex(result->policy.tfaw, 2) ||
	     !x58_romstage_uart_puts("\r\n[SPD] candidate is not an X58 register encoding\r\n")))
		return false;

	if (!x58_romstage_uart_puts("[SPD] full declared 256 bytes read twice; returning to ROMMON\r\n"
			    "RESULT=") ||
	    !x58_romstage_uart_put_hex(terminal, 2) ||
	    !x58_romstage_uart_puts("\r\n"))
		return false;
	if (terminal == POST_X58_PLATFORM_TIMEOUT &&
	    !x58_romstage_uart_puts("[SMBUS] timeout not recovered; remove AC power\r\n"))
		return false;

	return x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
				 X58_EARLY_UART_FLUSH_POLL_LIMIT);
}

#define X58_ROMMON_LINE_SIZE		128
#define X58_ROMMON_MAX_ARGS		10
#define X58_ROMMON_RX_POLL_LIMIT	1000000U
#define X58_ROMMON_DUMP_MAX		64

#define X58_VENDOR_PATH_RST_CNT_PORT	0xcf9

#define X58_VENDOR_ENTRY_SAD_DEV		PCI_DEV(0xff, 0, 1)
#define X58_VENDOR_ENTRY_SAD_ID		0x2d818086U
#define X58_VENDOR_ENTRY_SAD_PCIEXBAR_LO	0x50
#define X58_VENDOR_ENTRY_SAD_PCIEXBAR_HI	0x54
#define X58_VENDOR_ENTRY_PCIEXBAR_VALUE	0xe0000001U
#define X58_VENDOR_ENTRY_IOH_MMCONFIG_ID	0x34058086U
#define X58_VENDOR_ENTRY_TARGET_SPD_HEADER	0x93
#define X58_VENDOR_ENTRY_TARGET_SPD_CRC	0xec4f
#define X58_VENDOR_ENTRY_VENDOR_DUMP_MAX	X58_VENDOR_MINIT_POLICY_SIZE
#define X58_VENDOR_ENTRY_SPD_FIRST_ADDRESS	0x50
#define X58_VENDOR_ENTRY_SPD_ADDRESS_COUNT	8
#define X58_VENDOR_ENTRY_TARGET_SPD_BITMAP	BIT(X58_SPD_READ_SPD_ADDRESS - X58_VENDOR_ENTRY_SPD_FIRST_ADDRESS)

_Static_assert(X58_VENDOR_ENTRY_SPD_FIRST_ADDRESS == 0x50 &&
	X58_VENDOR_ENTRY_TARGET_SPD_BITMAP == X58_SPD_TARGET_MAP,
	"SPD topology policy must match the physical scan bitmap");

static const uint8_t x58_vendor_entry_target_part[SPD_DDR3_PART_LEN] = {
	'B', 'L', 'S', '4', 'G', '3', 'D', '1', '6', '0', '9', 'D', 'S',
	'1', 'S', '0', '0', '.',
};

#define X58_MEMORY_CONTROLLER_UNCORE_COMMON_DEV	PCI_DEV(0xff, 3, 4)
#define X58_MEMORY_CONTROLLER_CHANNEL2_DEV	PCI_DEV(0xff, 6, 0)
#define X58_MEMORY_CONTROLLER_PHY_SELECT_REG	0x5c
#define X58_MEMORY_CONTROLLER_PHY_COMMAND_REG	0xf8
#define X58_MEMORY_CONTROLLER_PHY_DATA_REG	0xfc
#define X58_MEMORY_CONTROLLER_PHY_CHANNEL_SHIFT	25
#define X58_MEMORY_CONTROLLER_PHY_CHANNEL_MASK	(3U << X58_MEMORY_CONTROLLER_PHY_CHANNEL_SHIFT)
#define X58_MEMORY_CONTROLLER_PHY_WRITE_BUSY	BIT(30)
#define X58_MEMORY_CONTROLLER_PHY_READ_BUSY	BIT(31)
#define X58_MEMORY_CONTROLLER_PHY_ANY_BUSY	(X58_MEMORY_CONTROLLER_PHY_WRITE_BUSY | X58_MEMORY_CONTROLLER_PHY_READ_BUSY)
#define X58_MEMORY_CONTROLLER_PHY_CHAIN_BASE	0x162a
#define X58_MEMORY_CONTROLLER_PHY_LAST_FIELD_BIT	0x1603
#define X58_MEMORY_CONTROLLER_PHY_POLL_LIMIT	100000U
#define X58_MEMORY_CONTROLLER_TRAIN_POLL_LIMIT	100000U

#define X58_MEMORY_CONTROLLER_MC_INIT_CMD		0x54
#define X58_MEMORY_CONTROLLER_MC_INIT_STATUS	0x5c
#define X58_MEMORY_CONTROLLER_MC_DDR3_CMD		0x60
#define X58_MEMORY_CONTROLLER_MC_MRS_VALUE_0_1	0x70
#define X58_MEMORY_CONTROLLER_MC_MRS_VALUE_2	0x74
#define X58_MEMORY_CONTROLLER_MC_BASE_TIMING	0x78
#define X58_MEMORY_CONTROLLER_MC_RANK_PRESENT	0x7c
#define X58_MEMORY_CONTROLLER_MC_ODT_PARAMS2	0xa0
#define X58_MEMORY_CONTROLLER_MC_TRAIN_ACTION	BIT(0)
#define X58_MEMORY_CONTROLLER_MC_INIT_COMPLETE	BIT(8)

#define X58_MEMORY_TRAINING_QPI_LINK0_PHY_DEV	PCI_DEV(0xff, 2, 1)
#define X58_MEMORY_TRAINING_QPI_PH_PIS		0x80
#define X58_MEMORY_TRAINING_QPI_SLOW_PH_PIS	0x030f0f03
#define X58_MEMORY_TRAINING_MRS_VALID		BIT(23)
#define X58_MEMORY_TRAINING_DDR3_RANK_SHIFT	20
#define X58_MEMORY_TRAINING_DDR3_MRS_BA_SHIFT	16
#define X58_MEMORY_TRAINING_INIT_RANK_SHIFT	5
#define X58_MEMORY_TRAINING_ASSERT_CKE		BIT(17)
#define X58_MEMORY_TRAINING_DO_ZQCL		BIT(15)
#define X58_MEMORY_TRAINING_IGNORE_RX		BIT(9)
#define X58_MEMORY_TRAINING_STOP_ON_FAIL	BIT(8)
#define X58_MEMORY_TRAINING_ZQCL_COMPLETE	BIT(7)
#define X58_MEMORY_TRAINING_RANK_PRESENT	0x03
#define X58_MEMORY_TRAINING_MR0		0x1528
#define X58_MEMORY_TRAINING_MR1		0x0806
#define X58_MEMORY_TRAINING_MR2		0x0000
#define X58_MEMORY_TRAINING_RD_INIT_PARAMS	0x063f4031
#define X58_MEMORY_TRAINING_BASE_TIMING		0x00000643


struct x58_memory_controller_phy_seed {
	uint16_t start;
	uint8_t width;
	uint16_t value;
	uint8_t mode;
};

/* MSI MINITDLL V8.14B8 descriptor tables, channel 2, rank 0, non-ECC. */
static const struct x58_memory_controller_phy_seed x58_memory_controller_rd_pulse[] = {
	{ 0x15fd, 7, 2, 1 }, { 0x13ae, 7, 2, 1 },
	{ 0x115f, 7, 2, 1 }, { 0x0f10, 7, 2, 1 },
	{ 0x0936, 7, 2, 1 }, { 0x06e7, 7, 2, 1 },
	{ 0x0498, 7, 2, 1 }, { 0x0249, 7, 2, 1 },
	{ 0x0cb7, 7, 2, 1 },
};

static const struct x58_memory_controller_phy_seed x58_memory_controller_rd_lane_seed[] = {
	{ 0x13f9, 12, 0, 0 }, { 0x1405, 11, 0, 0 },
	{ 0x11aa, 12, 0, 0 }, { 0x11b6, 11, 0, 0 },
	{ 0x0f5b, 12, 0, 0 }, { 0x0f67, 11, 0, 0 },
	{ 0x0d0c, 12, 0, 0 }, { 0x0d18, 11, 0, 0 },
	{ 0x0732, 12, 0, 0 }, { 0x073e, 11, 0, 0 },
	{ 0x04e3, 12, 0, 0 }, { 0x04ef, 11, 0, 0 },
	{ 0x0294, 12, 0, 0 }, { 0x02a0, 11, 0, 0 },
	{ 0x0045, 12, 0, 0 }, { 0x0051, 11, 0, 0 },
};

static const struct x58_memory_controller_phy_seed x58_memory_controller_rcven_lane_seed[] = {
	{ 0x13bd, 9, 0, 0 }, { 0x13c6, 11, 0x15e, 0 },
	{ 0x116e, 9, 0, 0 }, { 0x1177, 11, 0x15e, 0 },
	{ 0x0f1f, 9, 0, 0 }, { 0x0f28, 11, 0x15e, 0 },
	{ 0x0cd0, 9, 0, 0 }, { 0x0cd9, 11, 0x15e, 0 },
	{ 0x06f6, 9, 0, 0 }, { 0x06ff, 11, 0x15e, 0 },
	{ 0x04a7, 9, 0, 0 }, { 0x04b0, 11, 0x15e, 0 },
	{ 0x0258, 9, 0, 0 }, { 0x0261, 11, 0x15e, 0 },
	{ 0x0009, 9, 0, 0 }, { 0x0012, 11, 0x15e, 0 },
};


struct x58_rommon_state {
	char line[X58_ROMMON_LINE_SIZE];
	unsigned int length;
	bool write_armed;
	bool reset_armed;
	bool smbus_timed_out;
	bool script_armed;
	uint8_t vendor_policy[X58_VENDOR_MINIT_POLICY_SIZE];
	bool vendor_armed;
	bool vendor_runtime_ready;
	enum x58_vendor_status vendor_prepare_status;
	bool vendor_spd_valid;
	uint32_t vendor_spd_digest;
	uint32_t vendor_spd_profile_digest;
	uint8_t vendor_spd_ackmap;
	uint8_t vendor_spd_ddr3map;
	uint8_t vendor_spd_topology_status;
	bool vendor_pciexbar_ready;
	bool vendor_policy_ready;
	bool vendor_policy_modified;
	bool vendor_external_write;
};

static void x58_rommon_state_initialize(struct x58_rommon_state *state)
{
	(void)state;
	x58_rs_initialize();
	state->vendor_prepare_status = x58_vendor_runtime_prepare();
	state->vendor_runtime_ready =
		state->vendor_prepare_status == X58_VENDOR_OK;
}

enum x58_rommon_rx_result {
	X58_ROMMON_RX_IDLE,
	X58_ROMMON_RX_BYTE,
	X58_ROMMON_RX_FAULT,
};

static enum x58_rommon_rx_result x58_rommon_getc(uint8_t *value)
{
	unsigned int polls;

	for (polls = 0; polls < X58_ROMMON_RX_POLL_LIMIT; polls++) {
		const uint8_t status = inb(X58_EARLY_UART_BASE + UART8250_LSR);

		if (status & (UART8250_LSR_OE | UART8250_LSR_PE |
			      UART8250_LSR_FE | UART8250_LSR_BI)) {
			if (status & UART8250_LSR_DR)
				(void)inb(X58_EARLY_UART_BASE + UART8250_RBR);
			return X58_ROMMON_RX_FAULT;
		}
		if (status & UART8250_LSR_DR) {
			*value = inb(X58_EARLY_UART_BASE + UART8250_RBR);
			return X58_ROMMON_RX_BYTE;
		}
	}

	return X58_ROMMON_RX_IDLE;
}

static bool x58_rommon_streq(const char *left, const char *right)
{
	while (*left && *left == *right) {
		left++;
		right++;
	}

	return *left == *right;
}

static bool x58_rommon_parse_hex(const char *text, uint32_t *value)
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

static unsigned int x58_rommon_split(char *line, char **argv)
{
	unsigned int argc = 0;

	while (*line) {
		while (*line == ' ' || *line == '\t')
			*line++ = '\0';
		if (!*line)
			break;
		if (argc == X58_ROMMON_MAX_ARGS)
			return X58_ROMMON_MAX_ARGS + 1;
		argv[argc++] = line;
		while (*line && *line != ' ' && *line != '\t')
			line++;
	}

	return argc;
}

static bool x58_rommon_width(const char *text, unsigned int *width)
{
	if (!text || x58_rommon_streq(text, "l"))
		*width = 4;
	else if (x58_rommon_streq(text, "w"))
		*width = 2;
	else if (x58_rommon_streq(text, "b"))
		*width = 1;
	else
		return false;

	return true;
}

static bool x58_rommon_error(const char *message)
{
	outb(POST_X58_ROMMON_COMMAND_ERROR, CONFIG_POST_IO_PORT);
	return x58_romstage_uart_puts("ERR ") && x58_romstage_uart_puts(message) &&
		x58_romstage_uart_puts("\r\n");
}

static void x58_vendor_entry_mark_vendor_state_dirty(struct x58_rommon_state *state)
{
	state->vendor_external_write = true;
	state->vendor_spd_valid = false;
	state->vendor_spd_digest = 0;
	state->vendor_spd_profile_digest = 0;
	state->vendor_pciexbar_ready = false;
	state->vendor_policy_ready = false;
	state->vendor_policy_modified = false;
}

static bool x58_rommon_require_write(struct x58_rommon_state *state)
{
	if (!state->write_armed)
		return false;
	state->write_armed = false;
	/* A generic hardware write invalidates every vendor-call precondition. */
	x58_vendor_entry_mark_vendor_state_dirty(state);

	return true;
}

static bool x58_rommon_require_reset(struct x58_rommon_state *state)
{
	if (!state->reset_armed)
		return false;
	state->reset_armed = false;

	return true;
}

static bool x58_rommon_require_script(struct x58_rommon_state *state)
{
	if (!state->script_armed)
		return false;
	state->script_armed = false;
	return true;
}

static bool x58_rommon_require_vendor(struct x58_rommon_state *state)
{
	if (!state->vendor_armed)
		return false;
	state->vendor_armed = false;

	return true;
}

static bool x58_vendor_entry_vendor_state_clean(const struct x58_rommon_state *state)
{
	return !state->vendor_external_write;
}

static bool x58_vendor_entry_vendor_path_ready(const struct x58_rommon_state *state)
{
	return state->vendor_spd_valid && state->vendor_pciexbar_ready;
}

static bool x58_rommon_print_u32(const char *label, uint32_t value,
			   unsigned int digits)
{
	return x58_romstage_uart_puts(label) && x58_romstage_uart_put_hex(value, digits) &&
		x58_romstage_uart_puts("\r\n");
}


static uint32_t x58_rommon_io_read(uint16_t port, unsigned int width)
{
	if (width == 1)
		return inb(port);
	if (width == 2)
		return inw(port);
	return inl(port);
}

static void x58_rommon_io_write(uint16_t port, uint32_t value, unsigned int width)
{
	if (width == 1)
		outb(value, port);
	else if (width == 2)
		outw(value, port);
	else
		outl(value, port);
}

static uint32_t x58_rommon_pci_read(pci_devfn_t dev, uint16_t reg,
			      unsigned int width)
{
	if (width == 1)
		return pci_io_read_config8(dev, reg);
	if (width == 2)
		return pci_io_read_config16(dev, reg);
	return pci_io_read_config32(dev, reg);
}

static void x58_rommon_pci_write(pci_devfn_t dev, uint16_t reg, uint32_t value,
			   unsigned int width)
{
	if (width == 1)
		pci_io_write_config8(dev, reg, value);
	else if (width == 2)
		pci_io_write_config16(dev, reg, value);
	else
		pci_io_write_config32(dev, reg, value);
}

static bool x58_memory_controller_phy_wait(uint32_t busy_mask)
{
	unsigned int polls;

	for (polls = 0; polls < X58_MEMORY_CONTROLLER_PHY_POLL_LIMIT; polls++) {
		if (!(x58_rommon_pci_read(X58_MEMORY_CONTROLLER_UNCORE_COMMON_DEV,
				     X58_MEMORY_CONTROLLER_PHY_COMMAND_REG, 4) & busy_mask))
			return true;
	}

	return false;
}

static bool x58_memory_controller_phy_select(unsigned int channel)
{
	uint32_t value;

	if (channel > 2 || !x58_memory_controller_phy_wait(X58_MEMORY_CONTROLLER_PHY_ANY_BUSY))
		return false;
	value = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_UNCORE_COMMON_DEV,
			       X58_MEMORY_CONTROLLER_PHY_SELECT_REG, 4);
	value &= ~X58_MEMORY_CONTROLLER_PHY_CHANNEL_MASK;
	value |= channel << X58_MEMORY_CONTROLLER_PHY_CHANNEL_SHIFT;
	x58_rommon_pci_write(X58_MEMORY_CONTROLLER_UNCORE_COMMON_DEV, X58_MEMORY_CONTROLLER_PHY_SELECT_REG, value, 4);

	return true;
}

static bool x58_memory_controller_phy_field_valid(uint16_t start, unsigned int width)
{
	return width >= 2 && width <= 30 &&
		start <= X58_MEMORY_CONTROLLER_PHY_LAST_FIELD_BIT &&
		width - 1 <= X58_MEMORY_CONTROLLER_PHY_LAST_FIELD_BIT - start;
}

static uint32_t x58_memory_controller_phy_value_mask(unsigned int width)
{
	return (1U << (width - 2)) - 1;
}

static bool x58_memory_controller_phy_write(unsigned int channel, uint16_t start,
			   unsigned int width, uint32_t value,
			   unsigned int mode)
{
	uint32_t payload;
	uint16_t end;

	if (!x58_memory_controller_phy_field_valid(start, width) || mode > 1 ||
	    !x58_memory_controller_phy_select(channel))
		return false;
	end = start + width - 1;
	payload = value & x58_memory_controller_phy_value_mask(width);
	payload |= (mode + 2) << (width - 2);
	x58_rommon_pci_write(X58_MEMORY_CONTROLLER_UNCORE_COMMON_DEV, X58_MEMORY_CONTROLLER_PHY_DATA_REG, payload, 4);
	x58_rommon_pci_write(X58_MEMORY_CONTROLLER_UNCORE_COMMON_DEV, X58_MEMORY_CONTROLLER_PHY_COMMAND_REG,
			  X58_MEMORY_CONTROLLER_PHY_WRITE_BUSY | end, 4);

	return x58_memory_controller_phy_wait(X58_MEMORY_CONTROLLER_PHY_WRITE_BUSY);
}

static bool x58_memory_controller_phy_read(unsigned int channel, uint16_t start,
			  unsigned int width, uint32_t *raw,
			  uint32_t *value)
{
	uint16_t end;

	if (!x58_memory_controller_phy_field_valid(start, width) ||
	    !x58_memory_controller_phy_select(channel))
		return false;
	end = start + width - 1;
	x58_rommon_pci_write(X58_MEMORY_CONTROLLER_UNCORE_COMMON_DEV, X58_MEMORY_CONTROLLER_PHY_COMMAND_REG,
			  X58_MEMORY_CONTROLLER_PHY_READ_BUSY | (X58_MEMORY_CONTROLLER_PHY_CHAIN_BASE - end), 4);
	if (!x58_memory_controller_phy_wait(X58_MEMORY_CONTROLLER_PHY_READ_BUSY))
		return false;
	*raw = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_UNCORE_COMMON_DEV, X58_MEMORY_CONTROLLER_PHY_DATA_REG, 4);
	*value = *raw & x58_memory_controller_phy_value_mask(width);

	return true;
}

static bool x58_memory_controller_phy_apply(const struct x58_memory_controller_phy_seed *seed,
			   unsigned int count)
{
	unsigned int index;

	for (index = 0; index < count; index++) {
		if (!x58_memory_controller_phy_write(2, seed[index].start, seed[index].width,
				    seed[index].value, seed[index].mode))
			return false;
	}

	return true;
}

static bool x58_memory_controller_train(uint32_t command, uint32_t *status)
{
	unsigned int polls;

	x58_rommon_pci_write(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_INIT_CMD, 0x20600, 4);
	x58_rommon_pci_write(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_INIT_CMD, command, 4);
	for (polls = 0; polls < X58_MEMORY_CONTROLLER_TRAIN_POLL_LIMIT; polls++) {
		const uint32_t action = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV,
						      X58_MEMORY_CONTROLLER_MC_INIT_CMD, 4);

		*status = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV,
					 X58_MEMORY_CONTROLLER_MC_INIT_STATUS, 4);
		if (!(action & X58_MEMORY_CONTROLLER_MC_TRAIN_ACTION) &&
		    (*status & X58_MEMORY_CONTROLLER_MC_INIT_COMPLETE))
			return true;
	}

	return false;
}

static bool x58_memory_training_issue_mrs(unsigned int rank, unsigned int bank,
			   uint16_t value)
{
	uint32_t command;

	command = X58_MEMORY_TRAINING_MRS_VALID | (rank << X58_MEMORY_TRAINING_DDR3_RANK_SHIFT) |
		(bank << X58_MEMORY_TRAINING_DDR3_MRS_BA_SHIFT) | value;
	x58_rommon_pci_write(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_DDR3_CMD, command, 4);
	/*
	 * MSI fffcc43a writes and delays.  Bit 23 remains set on both the live
	 * target and a successful vendor-initialized capture; it encodes the
	 * direct command and is not a completion bit to poll clear.
	 */
	/* X58_MEMORY_TRAINING/X58_MEMORY_BASELINE compatibility path; X58_MEMORY_SEQUENCE replaces this unmeasured gap. */
	(void)x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_DDR3_CMD, 4);

	return true;
}

static bool x58_memory_training_issue_mode_update(void)
{
	unsigned int polls;

	x58_rommon_pci_write(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_INIT_CMD, 0x30200, 4);
	for (polls = 0; polls < X58_MEMORY_CONTROLLER_TRAIN_POLL_LIMIT; polls++) {
		if (x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_INIT_STATUS, 4) &
		    BIT(9))
			return true;
	}

	return false;
}

static bool x58_memory_training_issue_zqcl(unsigned int rank)
{
	unsigned int polls;
	uint32_t command = X58_MEMORY_TRAINING_ASSERT_CKE | X58_MEMORY_TRAINING_DO_ZQCL | X58_MEMORY_TRAINING_IGNORE_RX |
		(rank << X58_MEMORY_TRAINING_INIT_RANK_SHIFT);

	x58_rommon_pci_write(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_INIT_CMD, command, 4);
	for (polls = 0; polls < X58_MEMORY_CONTROLLER_TRAIN_POLL_LIMIT; polls++) {
		if (x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_INIT_STATUS, 4) &
		    X58_MEMORY_TRAINING_ZQCL_COMPLETE) {
			x58_rommon_pci_write(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_INIT_CMD,
					  X58_MEMORY_TRAINING_ASSERT_CKE | X58_MEMORY_TRAINING_IGNORE_RX |
					  X58_MEMORY_TRAINING_STOP_ON_FAIL, 4);
			return true;
		}
	}

	return false;
}

static bool x58_memory_training_prepare_rd_point(void)
{
	unsigned int rank;

	if ((x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_RANK_PRESENT, 1) & 0xff) !=
	    X58_MEMORY_TRAINING_RANK_PRESENT ||
	    x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_MRS_VALUE_0_1, 4) !=
		((X58_MEMORY_TRAINING_MR1 << 16) | X58_MEMORY_TRAINING_MR0) ||
	    (x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_MRS_VALUE_2, 2) & 0xffff) !=
		X58_MEMORY_TRAINING_MR2 ||
	    x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_BASE_TIMING, 4) !=
		X58_MEMORY_TRAINING_BASE_TIMING ||
	    x58_rommon_pci_read(X58_MEMORY_TRAINING_QPI_LINK0_PHY_DEV, X58_MEMORY_TRAINING_QPI_PH_PIS, 4) !=
		X58_MEMORY_TRAINING_QPI_SLOW_PH_PIS)
		return false;

	/* fffc5f0b + fffc8065: temporary training gate and FIFO reset. */
	x58_rommon_pci_write(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_INIT_CMD, 0x200, 4);
	x58_rommon_pci_write(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_INIT_STATUS, 1, 1);
	x58_rommon_pci_write(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, 0x58, X58_MEMORY_TRAINING_RD_INIT_PARAMS, 4);
	x58_rommon_pci_write(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, 0x50, 1, 4);
	(void)x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, 0x50, 4);
	x58_rommon_pci_write(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_INIT_CMD, 0x20600, 4);
	(void)x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_INIT_CMD, 4);

	/*
	 * Intel DX58SO ffe37125 asserts CKE and issues MRS2/MRS3/MRS1/MRS0
	 * without the MSI-only bank-4 command.  MSI fffd2bbd derives that
	 * command's low word from a runtime DIMM table; no constant value has
	 * been reconstructed for this configuration, so do not invent one.
	 */
	x58_rommon_pci_write(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_INIT_CMD,
			  X58_MEMORY_TRAINING_ASSERT_CKE | X58_MEMORY_TRAINING_IGNORE_RX, 4);
	(void)x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_INIT_CMD, 4);
	for (rank = 0; rank < 2; rank++) {
		if (!x58_memory_training_issue_mrs(rank, 2, X58_MEMORY_TRAINING_MR2) ||
		    !x58_memory_training_issue_mrs(rank, 3, 0) ||
		    !x58_memory_training_issue_mrs(rank, 1, X58_MEMORY_TRAINING_MR1) ||
		    !x58_memory_training_issue_mrs(rank, 0, X58_MEMORY_TRAINING_MR0) ||
		    !x58_memory_training_issue_mode_update())
			return false;
	}
	for (rank = 0; rank < 2; rank++) {
		if (!x58_memory_training_issue_zqcl(rank))
			return false;
	}

	return true;
}

static bool x58_memory_training_train_rd(uint32_t *status)
{
	unsigned int polls;

	/* MSI fffd5fc5 issues 0x26b01 directly after fffd2bbd. */
	x58_rommon_pci_write(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_INIT_CMD, 0x26b01, 4);
	for (polls = 0; polls < X58_MEMORY_CONTROLLER_TRAIN_POLL_LIMIT; polls++) {
		const uint32_t action = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV,
						      X58_MEMORY_CONTROLLER_MC_INIT_CMD, 4);

		*status = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV,
					 X58_MEMORY_CONTROLLER_MC_INIT_STATUS, 4);
		if (!(action & X58_MEMORY_CONTROLLER_MC_TRAIN_ACTION) &&
		    (*status & X58_MEMORY_CONTROLLER_MC_INIT_COMPLETE))
			return true;
	}

	return false;
}


static bool x58_memory_controller_report_lane_pair(const char *prefix, unsigned int lane,
				  const struct x58_memory_controller_phy_seed *pair)
{
	uint32_t raw;
	uint32_t first;
	uint32_t second;

	if (!x58_memory_controller_phy_read(2, pair[0].start, pair[0].width, &raw, &first) ||
	    !x58_memory_controller_phy_read(2, pair[1].start, pair[1].width, &raw, &second))
		return false;

	return x58_romstage_uart_puts(prefix) && x58_romstage_uart_put_hex(lane, 1) &&
		x58_romstage_uart_puts(" A=") && x58_romstage_uart_put_hex(first, 4) &&
		x58_romstage_uart_puts(" B=") && x58_romstage_uart_put_hex(second, 4) &&
		x58_romstage_uart_puts("\r\n");
}

static bool x58_memory_controller_run_rd_try(uint32_t sweep)
{
	struct x58_memory_controller_phy_seed global = { 0x09c7, 9, sweep, 1 };
	uint32_t status = 0;
	unsigned int lane;

	if (sweep > 0x80)
		return x58_rommon_error("rdtry SWEEP must be <= 80");
	if (!x58_memory_controller_phy_apply(x58_memory_controller_rd_pulse, ARRAY_SIZE(x58_memory_controller_rd_pulse)) ||
	    !x58_memory_controller_phy_apply(&global, 1) ||
	    !x58_memory_controller_phy_apply(x58_memory_controller_rd_lane_seed,
			    ARRAY_SIZE(x58_memory_controller_rd_lane_seed)))
		return x58_rommon_error("RD PHY write timeout");
	if (!x58_memory_controller_train(0x26b01, &status))
		return x58_rommon_error("RD training timeout");
	if (!x58_rommon_print_u32("RD_STATUS=", status, 8))
		return false;
	for (lane = 0; lane < 8; lane++) {
		if (!x58_memory_controller_report_lane_pair("RD", lane,
					   &x58_memory_controller_rd_lane_seed[lane * 2]))
			return x58_rommon_error("RD PHY read timeout");
	}

	return true;
}

static bool x58_memory_training_set_rd_pulses(uint32_t value)
{
	unsigned int index;

	for (index = 0; index < ARRAY_SIZE(x58_memory_controller_rd_pulse); index++) {
		if (!x58_memory_controller_phy_write(2, x58_memory_controller_rd_pulse[index].start,
				    x58_memory_controller_rd_pulse[index].width, value,
				    x58_memory_controller_rd_pulse[index].mode))
			return false;
	}

	return true;
}

static bool x58_memory_training_seed_rd_point(uint32_t sweep)
{
	struct x58_memory_controller_phy_seed global = { 0x09c7, 9, sweep, 1 };

	return sweep <= 0x80 &&
		x58_memory_controller_phy_apply(&global, 1) &&
		x58_memory_controller_phy_apply(x58_memory_controller_rd_lane_seed,
				 ARRAY_SIZE(x58_memory_controller_rd_lane_seed));
}

static bool x58_memory_training_report_rd_point(uint32_t sweep, uint32_t status)
{
	unsigned int lane;

	if (!x58_romstage_uart_puts("RD_SWEEP=") || !x58_romstage_uart_put_hex(sweep, 2) ||
	    !x58_romstage_uart_puts(" STATUS=") || !x58_romstage_uart_put_hex(status, 8))
		return false;
	for (lane = 0; lane < 8; lane++) {
		const struct x58_memory_controller_phy_seed *pair =
			&x58_memory_controller_rd_lane_seed[lane * 2];
		uint32_t raw;
		uint32_t first;
		uint32_t second;

		if (!x58_memory_controller_phy_read(2, pair[0].start, pair[0].width,
				   &raw, &first) ||
		    !x58_memory_controller_phy_read(2, pair[1].start, pair[1].width,
				   &raw, &second) ||
		    !x58_romstage_uart_puts(" L") || !x58_romstage_uart_put_hex(lane, 1) ||
		    !x58_romstage_uart_putc('=') || !x58_romstage_uart_put_hex(first, 3) ||
		    !x58_romstage_uart_putc('/') || !x58_romstage_uart_put_hex(second, 3))
			return false;
	}

	return x58_romstage_uart_puts("\r\n");
}

static bool x58_memory_training_run_rd_point(uint32_t sweep, uint32_t *status)
{
	if (!x58_memory_training_seed_rd_point(sweep))
		return false;
	if (!x58_memory_training_prepare_rd_point())
		return false;
	if (!x58_memory_training_train_rd(status))
		return false;

	return x58_memory_training_report_rd_point(sweep, *status);
}

static bool x58_memory_training_run_rd_exact(uint32_t sweep, uint32_t *status)
{
	bool result;

	if (!x58_memory_training_set_rd_pulses(2))
		return false;
	result = x58_memory_training_run_rd_point(sweep, status);
	if (!x58_memory_training_set_rd_pulses(0))
		return false;

	return result;
}

static bool x58_memory_training_run_rd_sweep(void)
{
	uint32_t sweep;
	bool result = false;

	/* MSI fffd82bf holds these nine fields across the complete loop. */
	if (!x58_memory_training_set_rd_pulses(2))
		return false;

	for (sweep = 0; sweep <= 0x80; sweep += 2) {
		uint32_t status = 0;

		if (!x58_memory_training_run_rd_point(sweep, &status))
			break;
		if (status & BIT(3)) {
			result = x58_romstage_uart_puts("RD_SWEEP_PASS=") &&
				x58_romstage_uart_put_hex(sweep, 2) &&
				x58_romstage_uart_puts("\r\n");
			break;
		}
	}
	if (!x58_memory_training_set_rd_pulses(0))
		return x58_rommon_error("RD pulse clear timeout");
	if (sweep <= 0x80)
		return result || x58_rommon_error("exact RD sweep gate/timeout");

	return x58_romstage_uart_puts("RD_SWEEP_NO_PASS\r\n");
}

static bool x58_memory_controller_run_rcven_try(uint32_t coarse)
{
	struct x58_memory_controller_phy_seed global = { 0x09fa, 8, coarse, 1 };
	uint32_t status = 0;
	unsigned int lane;

	if (coarse > 0x3f)
		return x58_rommon_error("rcvtry COARSE must be <= 3f");
	/* MSI and DX58SO both write 0x0100 to the low word before RCVEN. */
	x58_rommon_pci_write(X58_MEMORY_CONTROLLER_CHANNEL2_DEV, X58_MEMORY_CONTROLLER_MC_ODT_PARAMS2, 0x100, 2);
	if (!x58_memory_controller_phy_apply(&global, 1) ||
	    !x58_memory_controller_phy_apply(x58_memory_controller_rcven_lane_seed,
			    ARRAY_SIZE(x58_memory_controller_rcven_lane_seed)))
		return x58_rommon_error("RCVEN PHY write timeout");
	if (!x58_memory_controller_train(0x27301, &status))
		return x58_rommon_error("RCVEN training timeout");
	if (!x58_rommon_print_u32("RCV_STATUS=", status, 8))
		return false;
	for (lane = 0; lane < 8; lane++) {
		if (!x58_memory_controller_report_lane_pair("RCV", lane,
					   &x58_memory_controller_rcven_lane_seed[lane * 2]))
			return x58_rommon_error("RCVEN PHY read timeout");
	}

	return true;
}

static uint32_t x58_rommon_mem_read(uint32_t address, unsigned int width)
{
	if (width == 1)
		return *(volatile uint8_t *)(uintptr_t)address;
	if (width == 2)
		return *(volatile uint16_t *)(uintptr_t)address;
	return *(volatile uint32_t *)(uintptr_t)address;
}

static void x58_rommon_mem_write(uint32_t address, uint32_t value,
			   unsigned int width)
{
	if (width == 1)
		*(volatile uint8_t *)(uintptr_t)address = value;
	else if (width == 2)
		*(volatile uint16_t *)(uintptr_t)address = value;
	else
		*(volatile uint32_t *)(uintptr_t)address = value;
}

static bool x58_vendor_script_parse_value(char *text, struct x58_rs_value *value)
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
		return x58_rommon_parse_hex(text, &value->lo);
	*separator = '\0';
	result = x58_rommon_parse_hex(text, &value->hi) &&
		x58_rommon_parse_hex(separator + 1, &value->lo);
	*separator = ':';
	return result;
}

static bool x58_vendor_script_parse_space(const char *text, enum x58_rs_space *space)
{
	if (x58_rommon_streq(text, "io"))
		*space = X58_RS_SPACE_IO;
	else if (x58_rommon_streq(text, "pci"))
		*space = X58_RS_SPACE_PCI;
	else if (x58_rommon_streq(text, "mem"))
		*space = X58_RS_SPACE_MEM;
	else if (x58_rommon_streq(text, "msr"))
		*space = X58_RS_SPACE_MSR;
	else
		return false;
	return true;
}

static bool x58_vendor_script_parse_width(const char *text, enum x58_rs_space space,
	unsigned int *width)
{
	if (space == X58_RS_SPACE_MSR) {
		if (!x58_rommon_streq(text, "q"))
			return false;
		*width = 8;
		return true;
	}
	return x58_rommon_width(text, width);
}

static bool x58_vendor_script_parse_kind(const char *text, enum x58_rs_kind *kind)
{
	if (x58_rommon_streq(text, "read"))
		*kind = X58_RS_OP_READ;
	else if (x58_rommon_streq(text, "write"))
		*kind = X58_RS_OP_WRITE;
	else if (x58_rommon_streq(text, "mask"))
		*kind = X58_RS_OP_MASK;
	else if (x58_rommon_streq(text, "poll"))
		*kind = X58_RS_OP_POLL;
	else if (x58_rommon_streq(text, "delay"))
		*kind = X58_RS_OP_DELAY;
	else if (x58_rommon_streq(text, "assert"))
		*kind = X58_RS_OP_ASSERT;
	else
		return false;
	return true;
}

static bool x58_vendor_script_parse_reversibility(const char *text, uint8_t *flags)
{
	if (x58_rommon_streq(text, "rev"))
		*flags = X58_RS_FLAG_REVERSIBLE;
	else if (x58_rommon_streq(text, "nr"))
		*flags = 0;
	else
		return false;
	return true;
}

static bool x58_vendor_script_print_value(const struct x58_rs_value *value,
	unsigned int width)
{
	if (width == 8)
		return x58_romstage_uart_put_hex(value->hi, 8) &&
			x58_romstage_uart_putc(':') &&
			x58_romstage_uart_put_hex(value->lo, 8);
	return x58_romstage_uart_put_hex(value->lo, width * 2);
}

static bool x58_vendor_script_print_op(const char *prefix, size_t index,
	const struct x58_rs_op *op)
{
	if (!x58_romstage_uart_puts(prefix) || !x58_romstage_uart_puts("I=") ||
	    !x58_romstage_uart_put_hex(index, 2) || !x58_romstage_uart_puts(" OP=") ||
	    !x58_romstage_uart_puts(x58_rs_kind_name(op->kind)))
		return false;
	if (op->kind == X58_RS_OP_DELAY)
		return x58_romstage_uart_puts(" ITER=") &&
			x58_romstage_uart_put_hex(op->limit, 8) && x58_romstage_uart_puts("\r\n");
	if (!x58_romstage_uart_puts(" SPACE=") ||
	    !x58_romstage_uart_puts(x58_rs_space_name(op->space)) ||
	    !x58_romstage_uart_puts(" TARGET=") || !x58_romstage_uart_put_hex(op->target, 8) ||
	    !x58_romstage_uart_puts(" WIDTH=") ||
	    !x58_romstage_uart_putc(op->width == 8 ? 'q' :
		(op->width == 4 ? 'l' : (op->width == 2 ? 'w' : 'b'))))
		return false;
	if (op->kind == X58_RS_OP_WRITE || op->kind == X58_RS_OP_MASK) {
		if (!x58_romstage_uart_puts(" REV=") ||
		    !x58_romstage_uart_put_hex(!!(op->flags & X58_RS_FLAG_REVERSIBLE), 2))
			return false;
	}
	if (op->kind == X58_RS_OP_MASK || op->kind == X58_RS_OP_POLL ||
	    op->kind == X58_RS_OP_ASSERT) {
		if (!x58_romstage_uart_puts(op->kind == X58_RS_OP_MASK ? " CLEAR=" :
				   " MASK=") ||
		    !x58_vendor_script_print_value(&op->mask, op->width))
			return false;
	}
	if (op->kind != X58_RS_OP_READ) {
		if (!x58_romstage_uart_puts(op->kind == X58_RS_OP_MASK ? " SET=" :
				   " VALUE=") ||
		    !x58_vendor_script_print_value(&op->value, op->width))
			return false;
	}
	if (op->kind == X58_RS_OP_POLL &&
	    (!x58_romstage_uart_puts(" LIMIT=") || !x58_romstage_uart_put_hex(op->limit, 8)))
		return false;
	return x58_romstage_uart_puts("\r\n");
}

static bool x58_vendor_script_print_trace(const char *prefix, size_t index,
	const struct x58_rs_trace *trace)
{
	const unsigned int width = trace->op.width ? trace->op.width : 4;

	if (!x58_romstage_uart_puts(prefix) || !x58_romstage_uart_puts("I=") ||
	    !x58_romstage_uart_put_hex(index, 2) || !x58_romstage_uart_puts(" STATUS=") ||
	    !x58_romstage_uart_puts(x58_rs_trace_name(trace->status)) ||
	    !x58_romstage_uart_puts(" BEFORE=") ||
	    !x58_vendor_script_print_value(&trace->before, width) ||
	    !x58_romstage_uart_puts(" OBS=") ||
	    !x58_vendor_script_print_value(&trace->observed, width) ||
	    !x58_romstage_uart_puts(" ITER=") || !x58_romstage_uart_put_hex(trace->iterations, 8) ||
	    !x58_romstage_uart_puts(" WROTE=") ||
	    !x58_romstage_uart_put_hex(trace->write_performed, 2) ||
	    !x58_romstage_uart_puts(" RB=") ||
	    !x58_romstage_uart_puts(x58_rs_trace_name(trace->rollback_status)))
		return false;
	if (trace->rollback_status != X58_RS_TRACE_EMPTY &&
	    (!x58_romstage_uart_puts(" RB_OBS=") ||
	     !x58_vendor_script_print_value(&trace->rollback_observed, trace->op.width)))
		return false;
	return x58_romstage_uart_puts("\r\n");
}

static bool x58_vendor_script_backend_read(void *context, const struct x58_rs_op *op,
	struct x58_rs_value *value)
{
	(void)context;
	value->hi = 0;
	if (op->space == X58_RS_SPACE_IO) {
		value->lo = x58_rommon_io_read(op->target, op->width);
	} else if (op->space == X58_RS_SPACE_PCI) {
		value->lo = x58_rommon_pci_read(PCI_DEV(op->target >> 24,
			(op->target >> 16) & 0xff, (op->target >> 8) & 0xff),
			op->target & 0xff, op->width);
	} else if (op->space == X58_RS_SPACE_MEM) {
		value->lo = x58_rommon_mem_read(op->target, op->width);
	} else if (op->space == X58_RS_SPACE_MSR) {
		const msr_t msr = rdmsr(op->target);

		value->lo = msr.lo;
		value->hi = msr.hi;
	} else {
		return false;
	}
	return true;
}

static bool x58_vendor_script_backend_write(void *context, const struct x58_rs_op *op,
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
		x58_rommon_io_write(op->target, value->lo, op->width);
	} else if (op->space == X58_RS_SPACE_PCI) {
		x58_rommon_pci_write(PCI_DEV(op->target >> 24,
			(op->target >> 16) & 0xff, (op->target >> 8) & 0xff),
			op->target & 0xff, value->lo, op->width);
	} else if (op->space == X58_RS_SPACE_MEM) {
		x58_rommon_mem_write(op->target, value->lo, op->width);
	} else if (op->space == X58_RS_SPACE_MSR) {
		const msr_t msr = { .lo = value->lo, .hi = value->hi };

		wrmsr(op->target, msr);
	} else {
		return false;
	}
	return true;
}

static void x58_vendor_script_backend_delay(void *context, uint32_t iterations)
{
	(void)context;
	while (iterations--)
		asm volatile("pause" ::: "memory");
}

static bool x58_vendor_script_backend_event(void *context, enum x58_rs_event event,
	size_t index, const struct x58_rs_trace *trace)
{
	bool result;

	(void)context;
	if (event == X58_RS_EVENT_PRE) {
		outb(POST_X58_VENDOR_SCRIPT_SCRIPT_RUN, CONFIG_POST_IO_PORT);
		result = x58_vendor_script_print_op("[SCRIPT] PRE ", index, &trace->op);
	} else if (event == X58_RS_EVENT_POST) {
		result = x58_vendor_script_print_trace("[SCRIPT] POST ", index, trace);
	} else if (event == X58_RS_EVENT_ROLLBACK_PRE) {
		outb(POST_X58_VENDOR_SCRIPT_ROLLBACK_RUN, CONFIG_POST_IO_PORT);
		result = x58_vendor_script_print_op("[SCRIPT] ROLLBACK_PRE ", index,
			&trace->op);
	} else {
		result = x58_vendor_script_print_trace("[SCRIPT] ROLLBACK_POST ", index,
			trace);
	}
	return result && x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
		X58_EARLY_UART_FLUSH_POLL_LIMIT);
}

static const struct x58_rs_backend x58_vendor_script_backend = {
	.context = NULL,
	.read = x58_vendor_script_backend_read,
	.write = x58_vendor_script_backend_write,
	.delay = x58_vendor_script_backend_delay,
	.event = x58_vendor_script_backend_event,
};

static bool x58_vendor_script_print_result(const char *label, enum x58_rs_result result)
{
	return x58_romstage_uart_puts("[SCRIPT] ") && x58_romstage_uart_puts(label) &&
		x58_romstage_uart_puts("=") && x58_romstage_uart_puts(x58_rs_result_name(result)) &&
		x58_romstage_uart_puts("\r\n");
}

static bool x58_vendor_script_report_result(const char *label, enum x58_rs_result result)
{
	if (result != X58_RS_OK)
		outb(POST_X58_ROMMON_COMMAND_ERROR, CONFIG_POST_IO_PORT);
	return x58_vendor_script_print_result(label, result);
}

static bool x58_vendor_script_print_status(void)
{
	struct x58_rs_info info;

	x58_rs_get_info(&info);
	return x58_romstage_uart_puts("[SCRIPT] FORMAT=") &&
		x58_romstage_uart_put_hex(X58_RS_FORMAT_VERSION, 2) &&
		x58_romstage_uart_puts(" OPS=") &&
		x58_romstage_uart_put_hex(info.op_count, 2) &&
		x58_romstage_uart_puts(" PROGRAM_FNV=") &&
		x58_romstage_uart_put_hex(info.program_digest, 8) &&
		x58_romstage_uart_puts(" SEALED=") &&
		x58_romstage_uart_put_hex(info.program_sealed, 2) &&
		x58_romstage_uart_puts(" TXN_VALID=") &&
		x58_romstage_uart_put_hex(info.transaction_valid, 2) &&
		x58_romstage_uart_puts(" TRACE=") && x58_romstage_uart_put_hex(info.trace_count, 2) &&
		x58_romstage_uart_puts(" TXN_FNV=") &&
		x58_romstage_uart_put_hex(info.transaction_digest, 8) &&
		x58_romstage_uart_puts(" LAST=") && x58_romstage_uart_puts(x58_rs_result_name(
			info.last_result)) &&
		x58_romstage_uart_puts(" MUT=") && x58_romstage_uart_put_hex(info.mutations, 2) &&
		x58_romstage_uart_puts(" NONREV=") &&
		x58_romstage_uart_put_hex(info.nonreversible_mutations, 2) &&
		x58_romstage_uart_puts(" RB_AVAILABLE=") &&
		x58_romstage_uart_put_hex(info.rollback_available, 2) &&
		x58_romstage_uart_puts(" RB_RESULT=") &&
		x58_romstage_uart_puts(x58_rs_result_name(info.rollback_result)) &&
		x58_romstage_uart_puts(" RB_ATTEMPTED=") &&
		x58_romstage_uart_put_hex(info.rollback_attempted, 2) &&
		x58_romstage_uart_puts(" RB_TRIES=") &&
		x58_romstage_uart_put_hex(info.rollback_attempts, 8) &&
		x58_romstage_uart_puts(" ROLLED_BACK=") &&
		x58_romstage_uart_put_hex(info.rolled_back, 2) && x58_romstage_uart_puts("\r\n");
}

static bool x58_vendor_script_add(unsigned int argc, char **argv)
{
	struct x58_rs_op op = { 0 };
	enum x58_rs_kind kind;
	enum x58_rs_space space;
	unsigned int width;
	enum x58_rs_result result;

	if (argc < 4 || !x58_vendor_script_parse_kind(argv[2], &kind))
		return x58_rommon_error("script add read|write|mask|poll|delay|assert ...");
	op.kind = kind;
	if (kind == X58_RS_OP_DELAY) {
		if (argc != 4 || !x58_rommon_parse_hex(argv[3], &op.limit))
			return x58_rommon_error("use: script add delay ITER");
		op.space = X58_RS_SPACE_NONE;
	} else {
		if (argc < 6 || !x58_vendor_script_parse_space(argv[3], &space) ||
		    !x58_rommon_parse_hex(argv[4], &op.target) ||
		    !x58_vendor_script_parse_width(argv[5], space, &width))
			return x58_rommon_error("script target: SPACE TARGET b|w|l; MSR uses q");
		op.space = space;
		op.width = width;
		if (kind == X58_RS_OP_READ && argc != 6)
			return x58_rommon_error("use: script add read SPACE TARGET WIDTH");
		if (kind == X58_RS_OP_WRITE &&
		    (argc != 8 || !x58_vendor_script_parse_value(argv[6], &op.value) ||
		     !x58_vendor_script_parse_reversibility(argv[7], &op.flags)))
			return x58_rommon_error(
				"use: script add write SPACE TARGET WIDTH VALUE rev|nr");
		if (kind == X58_RS_OP_MASK &&
		    (argc != 9 || !x58_vendor_script_parse_value(argv[6], &op.mask) ||
		     !x58_vendor_script_parse_value(argv[7], &op.value) ||
		     !x58_vendor_script_parse_reversibility(argv[8], &op.flags)))
			return x58_rommon_error(
				"use: script add mask SPACE TARGET WIDTH CLEAR SET rev|nr");
		if (kind == X58_RS_OP_POLL &&
		    (argc != 9 || !x58_vendor_script_parse_value(argv[6], &op.mask) ||
		     !x58_vendor_script_parse_value(argv[7], &op.value) ||
		     !x58_rommon_parse_hex(argv[8], &op.limit)))
			return x58_rommon_error(
				"use: script add poll SPACE TARGET WIDTH MASK EXPECT LIMIT");
		if (kind == X58_RS_OP_ASSERT &&
		    (argc != 8 || !x58_vendor_script_parse_value(argv[6], &op.mask) ||
		     !x58_vendor_script_parse_value(argv[7], &op.value)))
			return x58_rommon_error(
				"use: script add assert SPACE TARGET WIDTH MASK EXPECT");
	}
	result = x58_rs_add(&op);
	if (!x58_vendor_script_report_result("ADD", result))
		return false;
	return result != X58_RS_OK || x58_vendor_script_print_status();
}

static bool x58_vendor_script_list(bool trace_log, unsigned int argc, char **argv)
{
	struct x58_rs_info info;
	uint32_t start = 0;
	uint32_t count;

	x58_rs_get_info(&info);
	count = trace_log ? info.trace_count : info.op_count;
	if (argc == 4) {
		if (!x58_rommon_parse_hex(argv[2], &start) ||
		    !x58_rommon_parse_hex(argv[3], &count))
			return x58_rommon_error("use: script list|trace [START COUNT]");
	} else if (argc != 2) {
		return x58_rommon_error("use: script list|trace [START COUNT]");
	}
	if (!count || start >= (trace_log ? info.trace_count : info.op_count) ||
	    count > (trace_log ? info.trace_count : info.op_count) - start)
		return x58_rommon_error("script list/trace range");
	for (size_t i = start; i < start + count; i++) {
		if (trace_log) {
			struct x58_rs_trace trace;

			if (!x58_rs_get_trace(i, &trace) ||
			    !x58_vendor_script_print_op("[SCRIPT] TRACE_OP ", i, &trace.op) ||
			    !x58_vendor_script_print_trace("[SCRIPT] TRACE ", i, &trace))
				return false;
		} else {
			struct x58_rs_op op;

			if (!x58_rs_get_op(i, &op) ||
			    !x58_vendor_script_print_op("[SCRIPT] LIST ", i, &op))
				return false;
		}
	}
	return true;
}

static void x58_vendor_entry_policy_write16(uint8_t *policy, unsigned int offset,
				 uint16_t value)
{
	policy[offset] = value;
	policy[offset + 1] = value >> 8;
}

static void x58_vendor_entry_policy_write32(uint8_t *policy, unsigned int offset,
				 uint32_t value)
{
	policy[offset] = value;
	policy[offset + 1] = value >> 8;
	policy[offset + 2] = value >> 16;
	policy[offset + 3] = value >> 24;
}

static uint32_t x58_vendor_entry_policy_read32(const uint8_t *policy, unsigned int offset)
{
	return (uint32_t)policy[offset] |
		((uint32_t)policy[offset + 1] << 8) |
		((uint32_t)policy[offset + 2] << 16) |
		((uint32_t)policy[offset + 3] << 24);
}

static uint32_t x58_vendor_entry_buffer_digest(const uint8_t *buffer, size_t size)
{
	uint32_t digest = 2166136261U;
	unsigned int offset;

	for (offset = 0; offset < size; offset++) {
		digest ^= buffer[offset];
		digest *= 16777619U;
	}

	return digest;
}

static uint32_t x58_vendor_entry_policy_digest(const uint8_t *policy)
{
	return x58_vendor_entry_buffer_digest(policy, X58_VENDOR_MINIT_POLICY_SIZE);
}

static uint8_t x58_vendor_entry_cmos_direct(uint8_t selector)
{
	outb(selector, 0x72);
	return inb(0x73);
}

#define X58_VENDOR_CMOS_DIAGNOSTIC_SELECTOR	0x0e
#define X58_VENDOR_CMOS_DIAGNOSTIC_ERROR_MASK	0xc0

struct x58_vendor_cmos_policy_inputs {
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

static void x58_vendor_cmos_capture_policy_inputs(struct x58_vendor_cmos_policy_inputs *inputs)
{
	const uint8_t saved_standard_selector = inb(0x70);
	const uint8_t saved_extended_selector = inb(0x72);

	/* The original wrapper disables NMI before reading CMOS diagnostic 0x0e. */
	outb(X58_VENDOR_CMOS_DIAGNOSTIC_SELECTOR | BIT(7), 0x70);
	(void)inb(0x61);
	inputs->diagnostic_0e = inb(0x71);
	inputs->extended_81 = x58_vendor_entry_cmos_direct(0x81);
	inputs->extended_82 = x58_vendor_entry_cmos_direct(0x82);
	inputs->extended_88 = x58_vendor_entry_cmos_direct(0x88);
	inputs->extended_89 = x58_vendor_entry_cmos_direct(0x89);
	inputs->extended_8e = x58_vendor_entry_cmos_direct(0x8e);
	inputs->extended_ca = x58_vendor_entry_cmos_direct(0xca);
	inputs->extended_f1 = x58_vendor_entry_cmos_direct(0xf1);
	inputs->extended_f5 = x58_vendor_entry_cmos_direct(0xf5);
	/* The MSI wrapper reads only the low byte before extracting bits 6:4. */
	inputs->alt_gp_smi_en_low = inb(DEFAULT_PMBASE + ALT_GP_SMI_EN);

	/* Telemetry restores both selector/NMI states and never writes CMOS data. */
	outb(saved_extended_selector, 0x72);
	outb(saved_standard_selector, 0x70);
}

static bool x58_vendor_cmos_print_policy_inputs(const struct x58_vendor_cmos_policy_inputs *inputs)
{
	const bool diagnostic_valid =
		!(inputs->diagnostic_0e & X58_VENDOR_CMOS_DIAGNOSTIC_ERROR_MASK);
	const uint8_t effective_8e =
		inputs->extended_8e > 0x30 ? 0x30 : inputs->extended_8e;

	return x58_romstage_uart_puts("[VENDOR] INPUT CMOS_DIAG_0E=") &&
		x58_romstage_uart_put_hex(inputs->diagnostic_0e, 2) &&
		x58_romstage_uart_puts(" VALID=") && x58_romstage_uart_put_hex(diagnostic_valid, 2) &&
		x58_romstage_uart_puts(" EXT81/82/88/89=") &&
		x58_romstage_uart_put_hex(inputs->extended_81, 2) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(inputs->extended_82, 2) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(inputs->extended_88, 2) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(inputs->extended_89, 2) &&
		x58_romstage_uart_puts("\r\n[VENDOR] INPUT EXT8E=") &&
		x58_romstage_uart_put_hex(inputs->extended_8e, 2) &&
		x58_romstage_uart_puts(" EFFECTIVE8E=") && x58_romstage_uart_put_hex(effective_8e, 2) &&
		x58_romstage_uart_puts(" EXTCA/F1/F5=") &&
		x58_romstage_uart_put_hex(inputs->extended_ca, 2) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(inputs->extended_f1, 2) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(inputs->extended_f5, 2) &&
		x58_romstage_uart_puts(" ALT_GP_SMI_EN_LOW=") &&
		x58_romstage_uart_put_hex(inputs->alt_gp_smi_en_low, 2) &&
		x58_romstage_uart_puts(" FIELD_6_4=") &&
		x58_romstage_uart_put_hex((inputs->alt_gp_smi_en_low >> 4) & 7, 2) &&
		x58_romstage_uart_puts(" CMOS_DATA_WRITES=00\r\n");
}

static uint8_t x58_vendor_entry_cmos_descriptor(uint16_t descriptor)
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

static void x58_vendor_entry_policy_setup_fields(uint8_t *policy)
{
	uint8_t value;

	if (x58_vendor_entry_cmos_direct(0xf5) & 3)
		policy[0xc3] = x58_vendor_entry_cmos_descriptor(0x2546);
	policy[0xc5] |= x58_vendor_entry_cmos_descriptor(0x1624);
	value = x58_vendor_entry_cmos_descriptor(0x2540);
	if (value == 0) {
		const uint8_t fallback = x58_vendor_entry_cmos_direct(0xf1);

		if (fallback & BIT(1))
			value = 2;
		if (fallback & BIT(2))
			value = 3;
	}
	policy[0xc6] = value;
	value = x58_vendor_entry_cmos_descriptor(0x443c);
	policy[0xc7] = value ? value + 4 : 0;
	value = x58_vendor_entry_cmos_descriptor(0x4450);
	policy[0xc8] = value ? value + 2 : 0;
	value = x58_vendor_entry_cmos_descriptor(0x4454);
	policy[0xc9] = value ? value + 2 : 0;
	value = x58_vendor_entry_cmos_descriptor(0x5388);
	policy[0xca] = value ? value + 8 : 0;
	value = x58_vendor_entry_cmos_descriptor(0x7348);
	x58_vendor_entry_policy_write16(policy, 0xcb, value ? value + 0x0e : 0);
	policy[0xcd] = x58_vendor_entry_cmos_descriptor(0x4458);
	policy[0xce] = x58_vendor_entry_cmos_descriptor(0x445c);
	policy[0xcf] = x58_vendor_entry_cmos_descriptor(0x34f0);
	policy[0xd0] = x58_vendor_entry_cmos_descriptor(0x4460);
	policy[0xd1] = x58_vendor_entry_cmos_descriptor(0x2542);
	policy[0xd2] = x58_vendor_entry_cmos_descriptor(0x4464);
	policy[0xd3] = x58_vendor_entry_cmos_descriptor(0x4468);
	policy[0xd4] = x58_vendor_entry_cmos_descriptor(0x5390);
	policy[0xd5] = x58_vendor_entry_cmos_descriptor(0x5398);
	policy[0xd6] = x58_vendor_entry_cmos_descriptor(0x53a0);
	policy[0xd7] = x58_vendor_entry_cmos_descriptor(0x53a8);
	policy[0xd8] = x58_vendor_entry_cmos_descriptor(0x446c);
	policy[0xd9] = x58_vendor_entry_cmos_descriptor(0x4480);
	policy[0xda] = x58_vendor_entry_cmos_descriptor(0x2544);
	policy[0xdb] = x58_vendor_entry_cmos_descriptor(0x4484);
	policy[0xdc] = x58_vendor_entry_cmos_descriptor(0x4488);
	policy[0xdd] = x58_vendor_entry_cmos_descriptor(0x62c2);
}

static bool x58_vendor_entry_spd_topology_is_supported(const struct x58_rommon_state *state)
{
	return x58_spd_topology_supported(state->vendor_spd_topology_status,
		state->vendor_spd_ackmap, state->vendor_spd_ddr3map,
		1);
}

static bool x58_vendor_entry_spd_matches_target(const struct x58_spd_read_result *result,
				      uint8_t terminal)
{
	unsigned int offset;

	if (terminal != POST_X58_PLATFORMI_SUCCESS || !result->header_valid ||
	    !result->upper_match || !result->decode_valid ||
	    !result->policy_valid || result->spd[0] != X58_VENDOR_ENTRY_TARGET_SPD_HEADER ||
	    result->crc_calculated != X58_VENDOR_ENTRY_TARGET_SPD_CRC ||
	    result->crc_stored != X58_VENDOR_ENTRY_TARGET_SPD_CRC ||
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
		    x58_vendor_entry_target_part[offset])
			return false;
	}

	return true;
}

static uint8_t x58_vendor_entry_scan_spd_topology(uint8_t *ackmap, uint8_t *ddr3map)
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
		return POST_X58_PLATFORM_HOST_STATE_ERROR;
	if (status & I801_HSTSTAT_HOST_BUSY) {
		x58_spd_read_release_host(0);
		return POST_X58_PLATFORM_HOST_STATE_ERROR;
	}
	if ((status & I801_HSTSTAT_FLAGS) ||
	    !x58_spd_read_control_idle(inb(base + I801_HSTCTL))) {
		x58_spd_read_release_host(0);
		return POST_X58_PLATFORM_HOST_STATE_ERROR;
	}
	terminal = x58_spd_read_pin_error(inb(base + I801_PIN_CTL));
	if (terminal) {
		x58_spd_read_release_host(0);
		return terminal;
	}

	for (index = 0; index < X58_VENDOR_ENTRY_SPD_ADDRESS_COUNT; index++) {
		const uint8_t address = X58_VENDOR_ENTRY_SPD_FIRST_ADDRESS + index;
		uint32_t loops;
		uint8_t masked_status;
		uint8_t value = 0;

		outb(I801_BYTE_DATA, base + I801_HSTCTL);
		outb((address << 1) | 1, base + I801_XMITADD);
		outb(X58_SPD_READ_SPD_MEMORY_TYPE_OFFSET, base + I801_HSTCMD);
		outb(0, base + I801_HSTDAT0);
		outb(0, base + I801_HSTDAT1);
		if (inb(base + I801_HSTCTL) != I801_BYTE_DATA ||
		    inb(base + I801_XMITADD) != ((address << 1) | 1) ||
		    inb(base + I801_HSTCMD) != X58_SPD_READ_SPD_MEMORY_TYPE_OFFSET ||
		    inb(base + I801_HSTDAT0) != 0 ||
		    inb(base + I801_HSTDAT1) != 0) {
			x58_spd_read_release_host(0);
			return POST_X58_PLATFORM_HOST_STATE_ERROR;
		}

		outb(I801_BYTE_DATA | I801_HSTCTL_START, base + I801_HSTCTL);
		for (loops = X58_SPD_READ_I801_POLL_LIMIT; loops; loops--) {
			status = inb(base + I801_HSTSTAT);
			if (!(status & I801_HSTSTAT_HOST_BUSY) &&
			    (status & I801_HSTSTAT_TERMINAL))
				break;
		}
		if (!loops)
			/* Preserve the unresolved controller state for diagnosis. */
			return POST_X58_PLATFORM_TIMEOUT;

		masked_status = status & I801_HSTSTAT_RESULT;
		if (masked_status == I801_HSTSTAT_INTR) {
			value = inb(base + I801_HSTDAT0);
			*ackmap |= BIT(index);
			if (value == X58_SPD_READ_SPD_DDR3_MEMORY_TYPE)
				*ddr3map |= BIT(index);
		}
		terminal = x58_spd_read_pin_error(inb(base + I801_PIN_CTL));
		/* Clear completion flags while retaining semaphore ownership. */
		outb(status & I801_HSTSTAT_FLAGS, base + I801_HSTSTAT);
		if (terminal) {
			x58_spd_read_release_host(0);
			return terminal;
		}
		if (masked_status == I801_HSTSTAT_DEV_ERR)
			continue;
		if (masked_status == I801_HSTSTAT_BUS_ERR) {
			x58_spd_read_release_host(0);
			return POST_X58_PLATFORM_BUS_ERROR;
		}
		if (masked_status != I801_HSTSTAT_INTR) {
			x58_spd_read_release_host(0);
			return POST_X58_PLATFORM_TRANSACTION_ERROR;
		}
	}

	terminal = x58_spd_read_pin_error(inb(base + I801_PIN_CTL));
	x58_spd_read_release_host(0);
	return terminal;
}

static bool x58_vendor_entry_emit_status(const char *operation,
			      enum x58_vendor_status status)
{
	return x58_romstage_uart_puts("[VENDOR] ") && x58_romstage_uart_puts(operation) &&
		x58_romstage_uart_puts(" STATUS=") &&
		x58_romstage_uart_puts(x58_vendor_status_name(status)) &&
		x58_romstage_uart_puts(" CODE=") && x58_romstage_uart_put_hex(status, 2) &&
		x58_romstage_uart_puts("\r\n");
}

static bool x58_vendor_entry_print_status(const char *operation,
			       enum x58_vendor_status status)
{
	if (status != X58_VENDOR_OK)
		outb(POST_X58_VENDOR_ENTRY_ERROR, CONFIG_POST_IO_PORT);

	return x58_vendor_entry_emit_status(operation, status);
}

static bool x58_vendor_entry_print_call_result(const char *name,
				    const struct x58_vendor_call_result *result)
{
	if (!x58_romstage_uart_detailed())
		return true;

	return x58_romstage_uart_puts("[VENDOR] ") && x58_romstage_uart_puts(name) &&
		x58_romstage_uart_puts(" RETURN EAX=") && x58_romstage_uart_put_hex(result->eax, 8) &&
		x58_romstage_uart_puts(" EBX=") && x58_romstage_uart_put_hex(result->ebx, 8) &&
		x58_romstage_uart_puts(" ECX=") && x58_romstage_uart_put_hex(result->ecx, 8) &&
		x58_romstage_uart_puts(" EDX=") && x58_romstage_uart_put_hex(result->edx, 8) &&
		x58_romstage_uart_puts(" EDI=") && x58_romstage_uart_put_hex(result->edi, 8) &&
		x58_romstage_uart_puts(" EFLAGS=") && x58_romstage_uart_put_hex(result->eflags, 8) &&
		x58_romstage_uart_puts(" VESP=") &&
		x58_romstage_uart_put_hex(result->vendor_esp_after_return, 8) &&
		x58_romstage_uart_puts("\r\n");
}

static bool x58_vendor_entry_print_runtime(const struct x58_rommon_state *state)
{
	struct x58_vendor_runtime_info info;
	const enum x58_vendor_status status =
		x58_vendor_runtime_probe(&info);

	if (!x58_vendor_entry_emit_status("PROBE", status))
		return false;
	if (!x58_romstage_uart_detailed())
		return true;

	if (!x58_romstage_uart_puts("[VENDOR] SIG WRAPPER=") ||
	    !x58_romstage_uart_put_hex(info.wrapper_signature_valid, 2) ||
	    !x58_romstage_uart_puts(" CSI=") ||
	    !x58_romstage_uart_put_hex(info.csi_signature_valid, 2) ||
	    !x58_romstage_uart_puts(" MINIT=") ||
	    !x58_romstage_uart_put_hex(info.minit_signature_valid, 2) ||
	    !x58_romstage_uart_puts(" CANARY=") ||
	    !x58_romstage_uart_put_hex(info.canaries_valid, 2) ||
	    !x58_romstage_uart_puts("\r\n[VENDOR] CPU CPUID1=") ||
	    !x58_romstage_uart_put_hex(info.cpuid_1_eax, 8) ||
	    !x58_romstage_uart_puts(" UCODE=") ||
	    !x58_romstage_uart_put_hex(info.microcode_revision, 8) ||
	    !x58_romstage_uart_puts(" APIC_BASE=") ||
	    !x58_romstage_uart_put_hex(info.apic_base_high, 8) || !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(info.apic_base_low, 8) ||
	    !x58_romstage_uart_puts(" CR0=") || !x58_romstage_uart_put_hex(info.cr0, 8) ||
	    !x58_romstage_uart_puts(" CR4=") || !x58_romstage_uart_put_hex(info.cr4, 8) ||
	    !x58_romstage_uart_puts(" EFLAGS=") ||
	    !x58_romstage_uart_put_hex(info.eflags_before_call, 8) ||
	    !x58_romstage_uart_puts(" CS/DS/ES/SS=") ||
	    !x58_romstage_uart_put_hex(info.cs_selector, 4) || !x58_romstage_uart_putc('/') ||
	    !x58_romstage_uart_put_hex(info.ds_selector, 4) || !x58_romstage_uart_putc('/') ||
	    !x58_romstage_uart_put_hex(info.es_selector, 4) || !x58_romstage_uart_putc('/') ||
	    !x58_romstage_uart_put_hex(info.ss_selector, 4) ||
	    !x58_romstage_uart_puts("\r\n[VENDOR] CAR=") ||
	    !x58_romstage_uart_put_hex(info.car_scratch_begin, 8) ||
	    !x58_romstage_uart_putc('-') || !x58_romstage_uart_put_hex(info.car_scratch_end, 8) ||
	    !x58_romstage_uart_puts(" STACK=") ||
	    !x58_romstage_uart_put_hex(info.vendor_stack_low, 8) ||
	    !x58_romstage_uart_putc('-') || x58_romstage_uart_put_hex(info.vendor_stack_top, 8) == false ||
	    !x58_romstage_uart_puts(" SIZE=") ||
	    !x58_romstage_uart_put_hex(info.vendor_stack_size, 8) ||
	    !x58_romstage_uart_puts(" HIGHWATER=") ||
	    !x58_romstage_uart_put_hex(info.vendor_stack_high_water, 8) ||
	    !x58_romstage_uart_puts("\r\n[VENDOR] SAD_ID=") ||
	    !x58_romstage_uart_put_hex(info.uncore_sad_id, 8) ||
	    !x58_romstage_uart_puts(" PCIEXBAR=") ||
	    !x58_romstage_uart_put_hex(info.pciexbar_high, 8) ||
	    !x58_romstage_uart_putc(':') || !x58_romstage_uart_put_hex(info.pciexbar_low, 8) ||
	    !x58_romstage_uart_puts(" X58_ID=") ||
	    !x58_romstage_uart_put_hex(info.x58_hostbridge_id, 8) ||
	    !x58_romstage_uart_puts(" CLASSREV=") ||
	    !x58_romstage_uart_put_hex(info.x58_hostbridge_class_revision, 8) ||
	    !x58_romstage_uart_puts("\r\n[VENDOR] QPI80=") ||
	    !x58_romstage_uart_put_hex(info.qpi_phy_observed_80, 8) ||
	    !x58_romstage_uart_puts(" MC50=") ||
	    !x58_romstage_uart_put_hex(info.memory_clock_observed_50, 8) ||
	    !x58_romstage_uart_puts(" MC54=") ||
	    !x58_romstage_uart_put_hex(info.memory_clock_observed_54, 8) ||
	    !x58_romstage_uart_puts("\r\n[VENDOR] PHASE CSI_ARMED=") ||
	    !x58_romstage_uart_put_hex(info.csi_wrapper_armed, 2) ||
	    !x58_romstage_uart_puts(" CSI_ATTEMPTED=") ||
	    !x58_romstage_uart_put_hex(info.csi_call_attempted, 2) ||
	    !x58_romstage_uart_puts(" CSI_RETURNED=") ||
	    !x58_romstage_uart_put_hex(info.csi_returned, 2) ||
	    !x58_romstage_uart_puts(" CSI_ACCEPTED=") ||
	    !x58_romstage_uart_put_hex(info.csi_result_accepted, 2) ||
	    !x58_romstage_uart_puts(" HIGH_MINIT_AUTH=") ||
	    !x58_romstage_uart_put_hex(info.x58_minit_high_qpi_minit_authorized, 2) ||
	    !x58_romstage_uart_puts(" POLICY=") ||
	    !x58_romstage_uart_put_hex(info.minit_policy_confirmed, 2) ||
	    !x58_romstage_uart_puts(" MINIT_ARMED=") ||
	    !x58_romstage_uart_put_hex(info.minit_armed, 2) ||
	    !x58_romstage_uart_puts(" MINIT_ATTEMPTED=") ||
	    !x58_romstage_uart_put_hex(info.minit_call_attempted, 2) ||
	    !x58_romstage_uart_puts(" MINIT_RETURNED=") ||
	    !x58_romstage_uart_put_hex(info.minit_returned, 2) ||
	    !x58_romstage_uart_puts(" SEED=") ||
	    !x58_romstage_uart_put_hex(info.vendor_status_seed, 2) ||
	    !x58_romstage_uart_puts(" CSI_FNV=") ||
	    !x58_romstage_uart_put_hex(info.csi_state_digest, 8) ||
	    !x58_romstage_uart_puts(" POLICY_IN_FNV=") ||
	    !x58_romstage_uart_put_hex(info.minit_policy_digest, 8) ||
	    !x58_romstage_uart_puts(" POLICY_NOW_FNV=") ||
	    !x58_romstage_uart_put_hex(info.minit_policy_current_digest, 8) ||
	    !x58_romstage_uart_puts(" WORK_FNV=") ||
	    !x58_romstage_uart_put_hex(info.minit_workspace_digest, 8) ||
	    !x58_romstage_uart_puts(" ROMMON_DIRTY=") ||
	    !x58_romstage_uart_put_hex(state->vendor_external_write, 2) ||
	    !x58_romstage_uart_puts("\r\n"))
		return false;

	if (info.csi_call_attempted || info.minit_call_attempted)
		return x58_vendor_entry_print_call_result("LAST", &info.last_call);

	return true;
}

static bool x58_vendor_entry_dump_buffer(const char *name, const uint8_t *buffer,
			      unsigned int size, unsigned int start,
			      unsigned int count)
{
	unsigned int offset;

	if (buffer == NULL || start >= size || count == 0 ||
	    count > X58_VENDOR_ENTRY_VENDOR_DUMP_MAX || count > size - start)
		return x58_rommon_error("vendor dump range");

	for (offset = 0; offset < count; offset++) {
		if ((offset & 0xf) == 0 &&
		    (!x58_romstage_uart_puts("[VENDOR] ") || !x58_romstage_uart_puts(name) ||
		     !x58_romstage_uart_putc(' ') ||
		     !x58_romstage_uart_put_hex(start + offset, 4) ||
		     !x58_romstage_uart_putc(':')))
			return false;
		if (!x58_romstage_uart_put_hex(buffer[start + offset], 2))
			return false;
		if ((offset & 0xf) == 0xf || offset + 1 == count) {
			if (!x58_romstage_uart_puts("\r\n"))
				return false;
		}
	}

	return true;
}

static bool x58_vendor_entry_policy_reset(struct x58_rommon_state *state)
{
	struct x58_vendor_runtime_info info;
	enum x58_vendor_status status;
	uint32_t policy_flags;
	uint8_t policy[X58_VENDOR_MINIT_POLICY_SIZE];
	uint8_t combined;
	uint8_t csi_result;
	struct x58_vendor_cmos_policy_inputs inputs;
	bool diagnostic_valid;
	uint8_t effective_8e;
	unsigned int offset;

	state->vendor_policy_ready = false;
	state->vendor_policy_modified = false;
	status = x58_vendor_invalidate_minit_policy();
	if (status != X58_VENDOR_OK)
		return x58_vendor_entry_print_status("POLICY_INVALIDATE", status);

	status = x58_vendor_runtime_probe(&info);
	if (status != X58_VENDOR_OK) {
		if (!x58_vendor_entry_print_status("POLICY_PROBE", status))
			return false;
		return true;
	}
	if (!info.csi_returned || !info.csi_result_accepted)
		return x58_rommon_error("vpolicy reset requires accepted CSI result");
	if (info.last_call.eax > UINT8_MAX)
		return x58_rommon_error("CSI EAX does not fit policy status byte");

	for (offset = 0; offset < sizeof(policy); offset++)
		policy[offset] = 0;

	csi_result = info.last_call.eax;
	if (csi_result == 3)
		csi_result = 4;
	combined = info.vendor_status_seed | csi_result;
	x58_vendor_cmos_capture_policy_inputs(&inputs);
	if (!x58_vendor_cmos_print_policy_inputs(&inputs))
		return false;
	diagnostic_valid =
		!(inputs.diagnostic_0e & X58_VENDOR_CMOS_DIAGNOSTIC_ERROR_MASK);
	effective_8e = inputs.extended_8e > 0x30 ? 0x30 : inputs.extended_8e;
	if (diagnostic_valid && ((inputs.alt_gp_smi_en_low >> 4) & 7) != 1)
		return x58_rommon_error(
			"valid-CMOS policy is reconstructed only for ALT_GP_SMI_EN[6:4]=1");

	policy[0x00] = 0xff;
	policy[0x01] = diagnostic_valid ? inputs.extended_81 & 3 : 0;
	policy[0x02] = -effective_8e & 0x3f;
	policy[0x03] = 0x01;
	/* The original wrapper leaves this zero when CMOS diagnostic 0x0e is bad. */
	policy[0x04] = diagnostic_valid ? 0x01 : 0x00;
	policy[0x05] = diagnostic_valid && (inputs.extended_ca & 0xc0) ? 0x03 : 0x06;
	policy[0x06] = 0x04;
	policy[0x07] = 0x02;
	policy[0x08] = 0x02;
	policy[0x0a] = combined;
	policy[0x0b] = 0x01;
	policy[0x0c] = 0x12;
	policy[0x0d] = 0x09;
	x58_vendor_entry_policy_write16(policy, 0x10, 0x1388);
	x58_vendor_entry_policy_write16(policy, 0x14, 0x1000);
	x58_vendor_entry_policy_write32(policy, 0x20, 0xe0000000);
	policy_flags = 0x01024486 | (combined ? 0x00040000 : 0);
	if (diagnostic_valid) {
		policy_flags &= ~0x6;
		policy_flags |= inputs.extended_82 & 0x6;
		if (!(inputs.extended_ca & 0xc0))
			policy_flags &= ~BIT(14);
	}
	x58_vendor_entry_policy_write32(policy, 0x24, policy_flags);
	policy[0x28] = 0x01;
	policy[0x29] = 0x06;
	policy[0x2a] = 0x46;
	policy[0x2b] = 0x14;
	x58_vendor_entry_policy_write16(policy, 0x2c, 0x05dc);
	x58_vendor_entry_policy_write16(policy, 0x30, 0x0190);
	policy[0x32] = 0x46;
	policy[0x33] = 0x14;
	x58_vendor_entry_policy_write16(policy, 0x34, 0x05dc);
	x58_vendor_entry_policy_write16(policy, 0x38, 0x0190);
	x58_vendor_entry_policy_write32(policy, 0x46, 0);
	policy[0x4e] = 0x01;
	policy[0x50] = 0x40;
	x58_vendor_entry_policy_write16(policy, 0x51, 0x0003);
	policy[0x53] = 0x5c;
	policy[0x55] = 0x01;
	policy[0x73] = 0x01;
	policy[0x91] = 0x01;
	if (diagnostic_valid) {
		/* Still inferred from the sole captured field-1 vendor profile. */
		x58_vendor_entry_policy_write16(policy, 0xbc, 0x0085);
		x58_vendor_entry_policy_setup_fields(policy);
	}
	/* The original CSI wrapper's returned CMOS/NMI state is port 0x70 = 0x8e. */
	outb(0x8e, 0x70);
	policy[0xde] = 0x00;
	for (offset = 0; offset < sizeof(policy); offset++)
		state->vendor_policy[offset] = policy[offset];
	state->vendor_policy_ready = true;

	return x58_romstage_uart_puts("[VENDOR] policy template=") &&
		x58_romstage_uart_puts(diagnostic_valid ?
			"VALID_CMOS_FIELD1_INFERRED" : "INVALID_CMOS_MSI_DEFAULT") &&
		x58_romstage_uart_puts(" EXT8E_CLAMPED_IN_POLICY=") &&
		x58_romstage_uart_put_hex(inputs.extended_8e > 0x30, 2) &&
		x58_romstage_uart_puts(" SEED_OR_CSI=") &&
		x58_romstage_uart_put_hex(combined, 2) && x58_romstage_uart_puts(" FNV1A=") &&
		x58_romstage_uart_put_hex(x58_vendor_entry_policy_digest(state->vendor_policy), 8) &&
		x58_romstage_uart_puts(
			"; inspect/edit before install; CMOS70=8e NMI_DISABLED\r\n");
}

#define X58_VENDOR_POLICY_STATUS_OFFSET	0x0a
#define X58_VENDOR_POLICY_FLAGS_OFFSET	0x24
#define X58_VENDOR_POLICY_B3_SKIP		BIT(18)
#define X58_VENDOR_POLICY_CMOS_CANDIDATE_FLAGS	0x01060480
#define X58_VENDOR_POLICY_CMOS_CANDIDATE_FNV	0x2e0ccce9
#define X58_VENDOR_POLICY_COLD_FNV		0x3c0f3a0b
#define X58_VENDOR_POLICY_WORK_B3_FLAGS_OFFSET	0x4f
#define X58_VENDOR_POLICY_WORK_COMPLETE_OFFSET	0xe79
#define X58_VENDOR_POLICY_MC_COMMON_DEV		PCI_DEV(0xff, 3, 0)
#define X58_VENDOR_POLICY_CHANNEL2_ADDR_DEV		PCI_DEV(0xff, 6, 1)
#define X58_VENDOR_POLICY_MC_CHANNEL_MAPPER		0x60
#define X58_VENDOR_POLICY_MC_DOD_DIMM0		0x48

static bool x58_vendor_policy_force_cold(struct x58_rommon_state *state)
{
	struct x58_vendor_runtime_info info;
	enum x58_vendor_status status;
	uint32_t digest;
	uint32_t flags;

	if (!state->vendor_policy_ready || !state->vendor_policy_modified)
		return x58_rommon_error(
			"vpolicy cold requires the reviewed X58_VENDOR_CMOS candidate policy");
	status = x58_vendor_runtime_probe(&info);
	if (status != X58_VENDOR_OK)
		return x58_vendor_entry_print_status("COLD_POLICY_PROBE", status);
	if (!info.csi_returned || !info.csi_result_accepted ||
	    info.last_call.eax != 2 || info.last_call.ebx != 2 ||
	    info.last_call.ecx != 0x106)
		return x58_rommon_error("cold override requires accepted CSI tuple 2/2/106");

	digest = x58_vendor_entry_policy_digest(state->vendor_policy);
	flags = x58_vendor_entry_policy_read32(state->vendor_policy,
		X58_VENDOR_POLICY_FLAGS_OFFSET);
	if (digest != X58_VENDOR_POLICY_CMOS_CANDIDATE_FNV ||
	    state->vendor_policy[X58_VENDOR_POLICY_STATUS_OFFSET] != 2 ||
	    flags != X58_VENDOR_POLICY_CMOS_CANDIDATE_FLAGS)
		return x58_rommon_error(
			"policy must exactly match X58_VENDOR_CMOS returning candidate FNV 2e0ccce9");

	status = x58_vendor_invalidate_minit_policy();
	if (!x58_vendor_entry_print_status("POLICY_INVALIDATE", status))
		return false;
	if (status != X58_VENDOR_OK)
		return true;

	state->vendor_policy[X58_VENDOR_POLICY_STATUS_OFFSET] = 0;
	flags &= ~X58_VENDOR_POLICY_B3_SKIP;
	x58_vendor_entry_policy_write32(state->vendor_policy, X58_VENDOR_POLICY_FLAGS_OFFSET,
		flags);
	if (x58_vendor_entry_policy_digest(state->vendor_policy) != X58_VENDOR_POLICY_COLD_FNV)
		return x58_rommon_error("cold policy digest invariant failed");
	state->vendor_policy_modified = true;

	return x58_romstage_uart_puts(
		"[VENDOR] POLICY override=COLD_FORCE_UNVERIFIED STATUS=00 FLAGS=") &&
		x58_romstage_uart_put_hex(flags, 8) && x58_romstage_uart_puts(" FNV1A=") &&
		x58_romstage_uart_put_hex(x58_vendor_entry_policy_digest(state->vendor_policy), 8) &&
		x58_romstage_uart_puts(
			"; B3 skip cleared; inspect/dump/install separately; no call made\r\n");
}

static bool x58_vendor_entry_prepare_pciexbar(struct x58_rommon_state *state)
{
	const uint32_t sad_id = pci_io_read_config32(X58_VENDOR_ENTRY_SAD_DEV, 0);
	const uint32_t qpi_state = x58_rommon_pci_read(X58_MEMORY_TRAINING_QPI_LINK0_PHY_DEV,
		X58_MEMORY_TRAINING_QPI_PH_PIS, 4);
	uint32_t low = pci_io_read_config32(X58_VENDOR_ENTRY_SAD_DEV,
		X58_VENDOR_ENTRY_SAD_PCIEXBAR_LO);
	const uint32_t high = pci_io_read_config32(X58_VENDOR_ENTRY_SAD_DEV,
		X58_VENDOR_ENTRY_SAD_PCIEXBAR_HI);
	uint32_t x58_id;
	uint32_t class_revision;
	struct x58_vendor_runtime_info info;
	enum x58_vendor_status status;

	state->vendor_pciexbar_ready = false;
	if (!state->vendor_spd_valid)
		return x58_rommon_error("run spd and pass exact X58_VENDOR_ENTRY DIMM gate first");
	status = x58_vendor_preflight_pciexbar();
	if (!x58_vendor_entry_print_status("PCIEXBAR_PREFLIGHT", status))
		return false;
	if (status != X58_VENDOR_OK)
		return true;
	if (sad_id != X58_VENDOR_ENTRY_SAD_ID || high != 0 ||
	    (low != 0 && low != X58_VENDOR_ENTRY_PCIEXBAR_VALUE))
		return x58_rommon_error("SAD/PCIEXBAR pre-state rejected");
	if ((qpi_state != X58_MEMORY_TRAINING_QPI_SLOW_PH_PIS &&
	     qpi_state != 0x070f0f03) ||
	    x58_rommon_pci_read(X58_MEMORY_CONTROLLER_UNCORE_COMMON_DEV, 0x50, 4) != 0x0a000006 ||
	    x58_rommon_pci_read(X58_MEMORY_CONTROLLER_UNCORE_COMMON_DEV, 0x54, 4) != 0x00000006)
		return x58_rommon_error("Slow-QPI/ratio-6 pre-state rejected");

	if (!x58_romstage_uart_puts("[VENDOR] PCIEXBAR PRE ID=") ||
	    !x58_romstage_uart_put_hex(sad_id, 8) || !x58_romstage_uart_puts(" HI=") ||
	    !x58_romstage_uart_put_hex(high, 8) || !x58_romstage_uart_puts(" LO=") ||
	    !x58_romstage_uart_put_hex(low, 8) || !x58_romstage_uart_puts("\r\n"))
		return false;

	if (low == 0) {
		pci_io_write_config32(X58_VENDOR_ENTRY_SAD_DEV, X58_VENDOR_ENTRY_SAD_PCIEXBAR_HI, 0);
		pci_io_write_config32(X58_VENDOR_ENTRY_SAD_DEV, X58_VENDOR_ENTRY_SAD_PCIEXBAR_LO,
			X58_VENDOR_ENTRY_PCIEXBAR_VALUE);
	}
	low = pci_io_read_config32(X58_VENDOR_ENTRY_SAD_DEV, X58_VENDOR_ENTRY_SAD_PCIEXBAR_LO);
	if (low != X58_VENDOR_ENTRY_PCIEXBAR_VALUE ||
	    pci_io_read_config32(X58_VENDOR_ENTRY_SAD_DEV, X58_VENDOR_ENTRY_SAD_PCIEXBAR_HI) != 0)
		return x58_rommon_error("PCIEXBAR readback rejected");

	x58_id = x58_rommon_mem_read(0xe0000000, 4);
	class_revision = x58_rommon_mem_read(0xe0000008, 4);
	if (x58_id != X58_VENDOR_ENTRY_IOH_MMCONFIG_ID ||
	    (class_revision & 0xffffff00) != 0x06000000)
		return x58_rommon_error("X58 MMCONFIG identity rejected");

	status = x58_vendor_runtime_probe(&info);
	if (status == X58_VENDOR_OK && qpi_state == 0x070f0f03 &&
	    !info.x58_init_phase_high_qpi_csi_profile)
		status = X58_VENDOR_ERR_PLATFORM_STATE;
	state->vendor_pciexbar_ready = status == X58_VENDOR_OK;
	if (!x58_romstage_uart_puts("[VENDOR] PCIEXBAR POST LO=") ||
	    !x58_romstage_uart_put_hex(low, 8) || !x58_romstage_uart_puts(" X58_ID=") ||
	    !x58_romstage_uart_put_hex(x58_id, 8) || !x58_romstage_uart_puts(" CLASSREV=") ||
	    !x58_romstage_uart_put_hex(class_revision, 8) || !x58_romstage_uart_puts("\r\n") ||
	    !x58_vendor_entry_print_status("PCIEXBAR", status))
		return false;
	if (status != X58_VENDOR_OK)
		return true;

	outb(POST_X58_VENDOR_ENTRY_PCIEXBAR_READY, CONFIG_POST_IO_PORT);
	return true;
}

static bool x58_vendor_path_report_reset_registers(void)
{
	const uint32_t pmbase_reg =
		pci_io_read_config32(ICH10R_LPC_DEV, D31F0_PMBASE);
	const uint8_t acpi_cntl =
		pci_io_read_config8(ICH10R_LPC_DEV, D31F0_ACPI_CNTL);
	const uint16_t pmbase = pmbase_reg & X58_ROMSTAGE_PMBASE_MASK;
	const uint16_t gen_pmcon_1 =
		pci_io_read_config16(ICH10R_LPC_DEV, D31F0_GEN_PMCON_1);
	const uint16_t gen_pmcon_2 =
		pci_io_read_config16(ICH10R_LPC_DEV, D31F0_GEN_PMCON_2);
	const uint16_t gen_pmcon_3 =
		pci_io_read_config16(ICH10R_LPC_DEV, D31F0_GEN_PMCON_3);
	const uint32_t etr3 =
		pci_io_read_config32(ICH10R_LPC_DEV, D31F0_ETR3);
	const uint8_t rst_cnt = inb(X58_VENDOR_PATH_RST_CNT_PORT);
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
	if (!x58_romstage_uart_puts("[RESET] RAW D31F0 PMBASE=") ||
	    !x58_romstage_uart_put_hex(pmbase_reg, 8) ||
	    !x58_romstage_uart_puts(" ACPI_CNTL=") ||
	    !x58_romstage_uart_put_hex(acpi_cntl, 2) ||
	    !x58_romstage_uart_puts(" GEN_PMCON_1=") ||
	    !x58_romstage_uart_put_hex(gen_pmcon_1, 4) ||
	    !x58_romstage_uart_puts(" GEN_PMCON_2=") ||
	    !x58_romstage_uart_put_hex(gen_pmcon_2, 4) ||
	    !x58_romstage_uart_puts(" GEN_PMCON_3=") ||
	    !x58_romstage_uart_put_hex(gen_pmcon_3, 4) ||
	    !x58_romstage_uart_puts(" ETR3=") ||
	    !x58_romstage_uart_put_hex(etr3, 8) ||
	    !x58_romstage_uart_puts(" RST_CNT=") ||
	    !x58_romstage_uart_put_hex(rst_cnt, 2) ||
	    !x58_romstage_uart_puts("\r\n"))
		return false;

	if (!pm_io_available)
		return x58_romstage_uart_puts(
			"[RESET] RAW PM I/O unavailable: ACPI decode disabled or base zero\r\n"
			"[RESET] no cause decoded; no status bits cleared\r\n");

	return x58_romstage_uart_puts("[RESET] RAW PMIO BASE=") &&
		x58_romstage_uart_put_hex(pmbase, 4) &&
		x58_romstage_uart_puts(" PM1_STS=") &&
		x58_romstage_uart_put_hex(pm1_sts, 4) &&
		x58_romstage_uart_puts(" PM1_CNT=") &&
		x58_romstage_uart_put_hex(pm1_cnt, 8) &&
		x58_romstage_uart_puts(" SMI_STS=") &&
		x58_romstage_uart_put_hex(smi_sts, 8) &&
		x58_romstage_uart_puts(" GPE0_STS[31:0]=") &&
		x58_romstage_uart_put_hex(gpe0_sts_lo, 8) &&
		x58_romstage_uart_puts(" GPE0_STS[63:32]=") &&
		x58_romstage_uart_put_hex(gpe0_sts_hi, 8) &&
		x58_romstage_uart_puts(
			"\r\n[RESET] no cause decoded; no status bits cleared\r\n");
}

static void __noreturn x58_vendor_path_cf9_reset(const char *message, uint8_t post,
				      uint8_t first, uint8_t second)
{
	/*
	 * CAR is the only writable memory at this point.  In particular, do not
	 * call the generic reset path: it cleans the cache before touching CF9.
	 */
	(void)x58_romstage_uart_puts(message);
	(void)x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
				 X58_EARLY_UART_FLUSH_POLL_LIMIT);
	outb(post, CONFIG_POST_IO_PORT);
	asm volatile ("cli" ::: "memory");
	outb(first, X58_VENDOR_PATH_RST_CNT_PORT);
	outb(second, X58_VENDOR_PATH_RST_CNT_PORT);

	/* Retain the requested reset's POST code if the chipset does not act. */
	for (;;)
		asm volatile ("hlt");
}

static bool x58_rommon_si_getc(uint8_t *value)
{
	for (;;) {
		const enum x58_rommon_rx_result rx = x58_rommon_getc(value);

		if (rx == X58_ROMMON_RX_IDLE)
			continue;
		if (rx == X58_ROMMON_RX_FAULT)
			return false;
		return x58_romstage_uart_putc(*value);
	}
}

static bool x58_rommon_si_hex(unsigned int digits, uint32_t *value)
{
	uint32_t parsed = 0;

	while (digits--) {
		uint8_t byte;
		uint8_t digit;

		if (!x58_rommon_si_getc(&byte))
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

static bool x58_rommon_si_expect(uint8_t expected)
{
	uint8_t value;

	return x58_rommon_si_getc(&value) && value == expected;
}

static bool x58_rommon_si_width(uint8_t width, unsigned int *bytes)
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

static bool x58_rommon_si_command(void)
{
	uint8_t first;
	uint8_t second;
	uint8_t width_byte;
	uint32_t address;
	uint32_t value;
	uint32_t key;
	unsigned int width;

	if (!x58_rommon_si_getc(&first) || !x58_rommon_si_getc(&second))
		return false;

	if (first == 'm' && second == 'b')
		return x58_romstage_uart_puts("\r\nMSI X58 Pro-E (MS-7522)         ");
	if (first == 'v' && second == 'i')
		return x58_romstage_uart_puts("\nSerialICE v1.5 X58_ROMMON-X58 (2026-08-31)\n");
	if (first == 'c' && second == 'i') {
		struct cpuid_result result;

		if (!x58_rommon_si_hex(8, &address) || !x58_rommon_si_expect('.') ||
		    !x58_rommon_si_hex(8, &value))
			return false;
		result = cpuid_ext(address, value);
		return x58_romstage_uart_puts("\r\n") &&
			x58_romstage_uart_put_hex(result.eax, 8) && x58_romstage_uart_putc('.') &&
			x58_romstage_uart_put_hex(result.ebx, 8) && x58_romstage_uart_putc('.') &&
			x58_romstage_uart_put_hex(result.ecx, 8) && x58_romstage_uart_putc('.') &&
			x58_romstage_uart_put_hex(result.edx, 8);
	}
	if ((first == 'r' || first == 'w') && second == 'm') {
		if (!x58_rommon_si_hex(8, &address) || !x58_rommon_si_expect('.') ||
		    !x58_rommon_si_getc(&width_byte) ||
		    !x58_rommon_si_width(width_byte, &width))
			return false;
		if (first == 'r')
			return x58_romstage_uart_puts("\r\n") &&
				x58_romstage_uart_put_hex(x58_rommon_mem_read(address, width),
						 width * 2);
		if (!x58_rommon_si_expect('=') || !x58_rommon_si_hex(width * 2, &value))
			return false;
		x58_rommon_mem_write(address, value, width);
		return true;
	}
	if ((first == 'r' || first == 'w') && second == 'i') {
		if (!x58_rommon_si_hex(4, &address) || !x58_rommon_si_expect('.') ||
		    !x58_rommon_si_getc(&width_byte) ||
		    !x58_rommon_si_width(width_byte, &width))
			return false;
		if (first == 'r')
			return x58_romstage_uart_puts("\r\n") &&
				x58_romstage_uart_put_hex(x58_rommon_io_read(address, width),
						 width * 2);
		if (!x58_rommon_si_expect('=') || !x58_rommon_si_hex(width * 2, &value))
			return false;
		x58_rommon_io_write(address, value, width);
		return true;
	}
	if ((first == 'r' || first == 'w') && second == 'c') {
		msr_t msr;

		if (!x58_rommon_si_hex(8, &address) || !x58_rommon_si_expect('.') ||
		    !x58_rommon_si_hex(8, &key))
			return false;
		/* SerialICE carries its historical EDI key; RDMSR/WRMSR do not use it. */
		(void)key;
		if (first == 'r') {
			msr = rdmsr(address);
			return x58_romstage_uart_puts("\r\n") &&
				x58_romstage_uart_put_hex(msr.hi, 8) && x58_romstage_uart_putc('.') &&
				x58_romstage_uart_put_hex(msr.lo, 8);
		}
		if (!x58_rommon_si_expect('=') || !x58_rommon_si_hex(8, &msr.hi) ||
		    !x58_rommon_si_expect('.') || !x58_rommon_si_hex(8, &msr.lo))
			return false;
		wrmsr(address, msr);
		return true;
	}

	return false;
}

static void __noreturn x58_rommon_serialice(bool command_started)
{
	outb(POST_X58_ROMMON_SERIALICE, CONFIG_POST_IO_PORT);
	if (!command_started &&
	    !x58_romstage_uart_puts("\nSerialICE v1.5 X58_ROMMON-X58 (2026-08-31)\n"))
		stop_with_post(POST_X58_ROMMON_RX_ERROR);

	for (;;) {
		uint8_t value;

		if (!command_started) {
			if (!x58_romstage_uart_puts("\r\n> ") || !x58_rommon_si_getc(&value))
				stop_with_post(POST_X58_ROMMON_RX_ERROR);
			if (value != '*')
				continue;
		}
		if (!x58_rommon_si_command()) {
			outb(POST_X58_ROMMON_COMMAND_ERROR, CONFIG_POST_IO_PORT);
			if (!x58_romstage_uart_puts("ERROR\r\n"))
				stop_with_post(POST_X58_ROMMON_RX_ERROR);
		}
		command_started = false;
	}
}

static bool x58_rommon_help(void)
{
	return x58_romstage_uart_puts(
		"Read:  cpuid LEAF [SUB] | msr INDEX | io PORT [b|w|l]\r\n"
		"       pci BUS DEV FN REG [b|w|l] | mem ADDR [b|w|l]\r\n"
		"       dump ADDR COUNT | phyr CH START WIDTH | spd | resetcause\r\n"
		"       id | help\r\n"
		"Write: unlock WRITE arms exactly one of:\r\n"
		"       msrw INDEX HI LO | iow PORT VALUE [b|w|l]\r\n"
		"       pciw BUS DEV FN REG VALUE [b|w|l]\r\n"
		"       memw ADDR VALUE [b|w|l] | phyw CH START WIDTH VALUE MODE\r\n"
		"       rdtry SWEEP | rdexact SWEEP | rdsweep | rcvtry COARSE\r\n"
		"       post VALUE | halt\r\n"
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
		"Vendor read: vinfo | vstate OFF COUNT | vwork OFF COUNT\r\n"
		"       vinputs (CMOS banks plus ALT_GP_SMI_EN; no CMOS data write)\r\n"
		"       vpolicy dump OFF COUNT\r\n"
		"X58_MEMORY_RESULT locks manual vendor mutation/calls; automatic handoff is exact-gated.\r\n"
		"Reset: unlock RESET arms exactly one reset or auto-recovery command.\r\n"
		"       reset init|warm|full\r\n"
		"Auto recovery: unlock RESET; autoguard clear; then remove AC power.\r\n"
		"lock cancels every arm. All numbers and delay counts are hexadecimal.\r\n"
		"PHY: CH 0..2; MODE 0..1; RD/RCV profiles are CH2 rank0 only.\r\n"
		"rdexact/rdsweep require ranks=03, MR=0806:1528/0000, Slow QPI.\r\n"
		"Reset uses direct CF9 writes and does not flush CAR; full is not AC-off.\r\n"
		"serialice enters permanent v1.5; leading * command or @ QEMU handshake.\r\n"
		"WARNING: invalid MSR/MMIO/I/O/PCI access may hang or reset hardware.\r\n");
}

static bool x58_rommon_run_spd(struct x58_rommon_state *state)
{
	struct x58_spd_read_result result = { 0 };
	uint8_t terminal;

	state->vendor_spd_valid = false;
	state->vendor_spd_digest = 0;
	state->vendor_spd_profile_digest = 0;
	state->vendor_pciexbar_ready = false;
	state->vendor_policy_ready = false;
	state->vendor_policy_modified = false;
	if (state->smbus_timed_out)
		return x58_rommon_error("SMBus timeout latched; remove AC power");
	terminal = x58_spd_read_base_spd(&result);
	if (!terminal)
		terminal = POST_X58_PLATFORMI_SUCCESS;
	if (terminal == POST_X58_PLATFORM_TIMEOUT)
		state->smbus_timed_out = true;
	if ((x58_romstage_uart_detailed() || terminal != POST_X58_PLATFORMI_SUCCESS) &&
	    !x58_spd_timing_report(&result, terminal))
		return false;
	state->vendor_spd_topology_status = terminal;
	state->vendor_spd_ackmap = 0;
	state->vendor_spd_ddr3map = 0;
	if (terminal == POST_X58_PLATFORMI_SUCCESS)
		state->vendor_spd_topology_status = x58_vendor_entry_scan_spd_topology(
			&state->vendor_spd_ackmap, &state->vendor_spd_ddr3map);
	if (state->vendor_spd_topology_status == POST_X58_PLATFORM_TIMEOUT)
		state->smbus_timed_out = true;
	state->vendor_spd_valid = x58_vendor_entry_spd_matches_target(&result, terminal) &&
		x58_vendor_entry_spd_topology_is_supported(state);
	if (state->vendor_spd_valid) {
		state->vendor_spd_digest = x58_vendor_entry_buffer_digest(result.spd,
			sizeof(result.spd));
		state->vendor_spd_profile_digest = x58_spd_profile_digest(result.spd,
			sizeof(result.spd));
	}
	if (!x58_romstage_uart_puts("[VENDOR] SPD_TOPOLOGY ACKMAP=") ||
	    !x58_romstage_uart_put_hex(state->vendor_spd_ackmap, 2) ||
	    !x58_romstage_uart_puts(" DDR3MAP=") ||
	    !x58_romstage_uart_put_hex(state->vendor_spd_ddr3map, 2) ||
	    !x58_romstage_uart_puts(" RESULT=") ||
	    !x58_romstage_uart_put_hex(state->vendor_spd_topology_status, 2) ||
	    !x58_romstage_uart_puts(" EXACT_ONE_54=") ||
	    !x58_romstage_uart_puts((state->vendor_spd_topology_status == 0 &&
		state->vendor_spd_ackmap == X58_VENDOR_ENTRY_TARGET_SPD_BITMAP &&
		state->vendor_spd_ddr3map == X58_VENDOR_ENTRY_TARGET_SPD_BITMAP) ?
			"PASS\r\n" : "FAIL\r\n") ||
	    !x58_romstage_uart_puts("[SPD] TOPOLOGY_POLICY EXTRA_50=") ||
	    !x58_romstage_uart_puts("ALLOW") ||
	    !x58_romstage_uart_puts(" ADMISSION=") ||
	    !x58_romstage_uart_puts(x58_vendor_entry_spd_topology_is_supported(state) ?
		"PASS\r\n" : "FAIL\r\n") ||
	    !x58_romstage_uart_puts("[VENDOR] SPD54_TARGET_GATE=") ||
	    !x58_romstage_uart_puts(state->vendor_spd_valid ? "PASS" : "FAIL") ||
	    !x58_romstage_uart_puts(" FULL256_FNV1A=") ||
	    !x58_romstage_uart_put_hex(state->vendor_spd_digest, 8) ||
	    !x58_romstage_uart_puts(
		" expected=BLS4G3D1609DS1S00. hdr93 crcEC4F 4GiB 2Rx8 CL5\r\n"))
		return false;
	if (!x58_romstage_uart_puts("[SPD] PROFILE_FNV1A=") ||
	    !x58_romstage_uart_put_hex(state->vendor_spd_profile_digest, 8) ||
	    !x58_romstage_uart_puts(" SERIAL_122_125=IGNORED COMPAT_GATE=") ||
	    !x58_romstage_uart_puts((state->vendor_spd_valid &&
		x58_raminit_spd_digest_is_exact(state->vendor_spd_digest,
			state->vendor_spd_profile_digest)) ? "PASS\r\n" : "FAIL\r\n"))
		return false;
	outb(terminal, CONFIG_POST_IO_PORT);

	return true;
}

static bool x58_raminit_spd_is_exact(const struct x58_rommon_state *state)
{
	return state->vendor_spd_valid &&
		x58_raminit_spd_digest_is_exact(state->vendor_spd_digest,
			state->vendor_spd_profile_digest);
}

/*
 * Every value below is an exact observation from X58_VENDOR_CMOS/X58_VENDOR_POLICY on the single
 * supported lab configuration.  They are gates, not generalized register
 * definitions or native X58 policy.
 */
#define X58_RAMINIT_POLICY_BASE_FNV		0x36981039
#define X58_RAMINIT_POLICY_COLD_FLAGS		0x01020480
#define X58_RAMINIT_CMOS_INDEX_PORT		0x70
#define X58_RAMINIT_CMOS_DATA_PORT		0x71
#define X58_RAMINIT_CMOS_INDEX_READBACK_PORT	0x74
#define X58_RAMINIT_CMOS_DIAGNOSTIC_SELECTOR	0x0e
#define X58_RAMINIT_CMOS_DIAGNOSTIC_OBSERVED	0x6c
#define X58_RAMINIT_CMOS_GUARD_IN_PROGRESS	0xec
#define X58_INIT_PHASE_CMOS_COLD_AUTHORIZATION	0x2c
#define X58_INIT_PHASE_CMOS_PHASE_FAILED		0xed
#define X58_MINIT_CSI_OBSERVATION_FNV	0x8b38506a
#define X58_MINIT_POLICY_BASE_FNV		0xbc4268bf
#define X58_MINIT_CSI_CONTEXT_DELTA		0x1b9
#define X58_CSI_CANONICAL_CSI_DYNAMIC_OFFSET	0x02a6
#define X58_CSI_CANONICAL_CSI_RAW_08_FNV		0x03e3d24e
#define X58_CSI_CANONICAL_CSI_RAW_0C_FNV		0x8b38506a
#define X58_CSI_CANONICAL_CSI_CANONICAL_FNV		0x908dabb6
_Static_assert(X58_CSI_CANONICAL_CSI_DYNAMIC_OFFSET < X58_VENDOR_CSI_STATE_SIZE,
	"X58_CSI_CANONICAL CSI canonical offset is outside the state buffer");
struct x58_raminit_policy_byte {
	uint8_t offset;
	uint8_t value;
};

struct x58_raminit_i801_signature {
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
static const struct x58_raminit_i801_signature x58_raminit_first_pass_signature = {
	.control = I801_BYTE_DATA,
	.command = 0xa5,
	.xmit_address = 0x3c,
	.data0 = 0x5a,
	.data1 = 0xc3,
};

static const struct x58_raminit_i801_signature x58_raminit_in_progress_signature = {
	.control = I801_BYTE_DATA,
	.command = 0x5a,
	.xmit_address = 0xc3,
	.data0 = 0xa5,
	.data1 = 0x3c,
};

static const struct x58_raminit_i801_signature x58_raminit_clear_signature = {
	.control = I801_BYTE_DATA,
};

/*
 * X58_INIT_PHASE deliberately does not reuse a X58_RAMINIT/V7 signature.  Each command/
 * address and data pair is complemented, START stays clear, and the complete
 * five-byte tuple is checked.  Register retention is proven for CSI's first
 * reset; retention of these exact values across the outer IOH SYRE reset is
 * the hardware hypothesis tested by X58_INIT_PHASE.
 */
static const struct x58_raminit_i801_signature x58_init_phase_pass2_signature = {
	.control = I801_BYTE_DATA,
	.command = 0x31,
	.xmit_address = 0xce,
	.data0 = 0x68,
	.data1 = 0x97,
};

static const struct x58_raminit_i801_signature x58_init_phase_consumed_signature = {
	.control = I801_BYTE_DATA,
	.command = 0x42,
	.xmit_address = 0xbd,
	.data0 = 0x7a,
	.data1 = 0x85,
};

static const struct x58_raminit_i801_signature x58_init_phase_pass3_signature = {
	.control = I801_BYTE_DATA,
	.command = 0x53,
	.xmit_address = 0xac,
	.data0 = 0x6b,
	.data1 = 0x94,
};

/*
 * Dedicated fail-closed phase tag committed immediately before POST d3.
 * Both payload pairs are complements and cannot alias an ordinary SPD read.
 */
static const struct x58_raminit_i801_signature x58_minit_in_progress_signature = {
	.control = I801_BYTE_DATA,
	.command = 0x64,
	.xmit_address = 0x9b,
	.data0 = 0x5c,
	.data1 = 0xa3,
};
/* Exact tuple left by both controlled X58_CSI_PROFILE PRIMARY MINIT returns. */
static const struct x58_raminit_i801_signature x58_workspace_state_minit_return_signature = {
	.control = I801_BYTE_DATA,
	.command = 0x00,
	.xmit_address = 0x5d,
	.data0 = 0x01,
	.data1 = 0xa3,
};

/* Neutral offset names are intentional: their semantics remain unproven. */
static const struct x58_raminit_policy_byte x58_raminit_candidate_edits[] = {
	{ 0x04, 0x01 },
	{ 0x08, 0x01 },
	{ 0x24, 0x80 },
	{ 0x25, 0x04 },
	{ 0xbc, 0x85 },
	{ 0xd9, 0x0a },
	{ 0xdb, 0x05 },
};

#define X58_CSI_STATE_CSI_DYNAMIC_OFFSET	0x02a6

struct x58_csi_state_zero_range {
	uint16_t first;
	uint16_t last;
};

/*
 * These are byte ranges observed to vary between two successful Slow-QPI
 * MINIT returns.  Their meanings are not yet established.  Canonicalization
 * affects only the digest input; the vendor buffers themselves stay intact.
 */
static const struct x58_csi_state_zero_range x58_csi_state_workspace_dynamic_ranges[] = {
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

/*
 * Separate High-QPI canonicalization for the two successful X58_MEMORY_RESULT hardware
 * returns promoted by X58_MEMORY_HANDOFF.  It preserves every Slow-QPI range above and
 * adds only the five newly observed variable bytes.  The raw workspace is
 * never modified.
 */
static const struct x58_csi_state_zero_range x58_memory_handoff_workspace_dynamic_ranges[] = {
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

_Static_assert(X58_CSI_STATE_CSI_DYNAMIC_OFFSET < X58_VENDOR_CSI_STATE_SIZE,
	"X58_CSI_STATE CSI canonical offset is outside the state buffer");
_Static_assert(0x252e < X58_VENDOR_MINIT_WORKSPACE_SIZE,
	"X58_CSI_STATE workspace canonical range is outside the workspace");
_Static_assert(0x2461 < X58_VENDOR_MINIT_WORKSPACE_SIZE,
	"X58_MEMORY_HANDOFF workspace canonical range is outside the workspace");

static uint32_t x58_csi_state_canonical_digest(const uint8_t *buffer, size_t size,
	const struct x58_csi_state_zero_range *ranges, size_t range_count)
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

static uint32_t x58_csi_state_csi_canonical_digest(const uint8_t *csi_state)
{
	static const struct x58_csi_state_zero_range dynamic_byte = {
		.first = X58_CSI_STATE_CSI_DYNAMIC_OFFSET,
		.last = X58_CSI_STATE_CSI_DYNAMIC_OFFSET,
	};

	return x58_csi_state_canonical_digest(csi_state, X58_VENDOR_CSI_STATE_SIZE,
		&dynamic_byte, 1);
}

static uint32_t x58_csi_state_workspace_canonical_digest(const uint8_t *workspace)
{
	return x58_csi_state_canonical_digest(workspace,
		X58_VENDOR_MINIT_WORKSPACE_SIZE,
		x58_csi_state_workspace_dynamic_ranges,
		ARRAY_SIZE(x58_csi_state_workspace_dynamic_ranges));
}

static uint32_t x58_memory_handoff_workspace_canonical_digest(const uint8_t *workspace)
{
	return x58_csi_state_canonical_digest(workspace,
		X58_VENDOR_MINIT_WORKSPACE_SIZE,
		x58_memory_handoff_workspace_dynamic_ranges,
		ARRAY_SIZE(x58_memory_handoff_workspace_dynamic_ranges));
}

static bool x58_memory_handoff_workspace_raw_pattern_exact(const uint8_t *workspace,
	uint32_t raw_digest)
{
	if (workspace == NULL)
		return false;

	/* Couple each admitted raw FNV to its exact observed byte tuple. */
	if (raw_digest == X58_MEMORY_HANDOFF_EXPECTED_WORKSPACE_RAW_9429_FNV)
		return workspace[0x2459] == 0x00 &&
			workspace[0x245a] == 0x00 &&
			workspace[0x245b] == 0x00 &&
			workspace[0x245c] == 0x00 &&
			workspace[0x2461] == 0xfe;
	if (raw_digest == X58_MEMORY_HANDOFF_EXPECTED_WORKSPACE_RAW_B634_FNV)
		return workspace[0x2459] == 0xff &&
			workspace[0x245a] == 0xff &&
			workspace[0x245b] == 0xff &&
			workspace[0x245c] == 0xff &&
			workspace[0x2461] == 0xff;

	return false;
}

static bool x58_workspace_state_workspace_pattern_exact(const uint8_t *workspace,
	uint32_t raw_digest, uint32_t canonical_digest)
{
	if (workspace == NULL ||
	    !x58_workspace_state_workspace_digest_pair_exact(raw_digest,
		canonical_digest))
		return false;

	/* Preserve both already admitted X58_MEMORY_HANDOFF exact raw/pattern pairs. */
	if (raw_digest == X58_MEMORY_HANDOFF_EXPECTED_WORKSPACE_RAW_9429_FNV ||
	    raw_digest == X58_MEMORY_HANDOFF_EXPECTED_WORKSPACE_RAW_B634_FNV)
		return x58_memory_handoff_workspace_raw_pattern_exact(workspace, raw_digest);

	/* X58_CSI_PROFILE G3-04 PRIMARY capture P.  Neutral offsets are intentional. */
	if (raw_digest == X58_WORKSPACE_STATE_EXPECTED_WORKSPACE_RAW_EB15_FNV)
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

	/* PRIMARY workspace capture identified by its exact digest. */
	if (raw_digest == X58_WORKSPACE_STATE_EXPECTED_WORKSPACE_RAW_5FB6_FNV)
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

static bool x58_profile_policy_deferred_primary_workspace_pattern_exact(const uint8_t *workspace,
	uint32_t raw_digest, uint32_t canonical_digest)
{
	if (workspace == NULL ||
	    raw_digest != X58_PROFILE_POLICY_EXPECTED_WORKSPACE_RAW_50F6_FNV ||
	    canonical_digest != X58_PROFILE_POLICY_EXPECTED_WORKSPACE_CANONICAL_92C7_FNV)
		return false;

	/* Deferred workspace capture. Neutral offsets are intentional. */
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

static bool x58_deferred_profile_workspace_pattern_exact(const uint8_t *workspace,
	uint32_t raw_digest, uint32_t canonical_digest)
{
	if (workspace == NULL || canonical_digest !=
		X58_DEFERRED_PROFILE_EXPECTED_WORKSPACE_CANONICAL_0B16_FNV)
		return false;
	/* X58_PROFILE_CLASS preserves the raw digest as telemetry inside the v8 handoff. */
	(void)raw_digest;

	/* Second deferred workspace capture. Neutral offsets are intentional. */
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

static bool x58_profile_policy_deferred_workspace_pattern_exact(const uint8_t *workspace,
	uint32_t raw_digest, uint32_t canonical_digest)
{
	if (x58_profile_policy_deferred_primary_workspace_pattern_exact(workspace, raw_digest,
		canonical_digest))
		return true;
	return x58_deferred_profile_workspace_pattern_exact(workspace, raw_digest,
		canonical_digest);
}

static bool x58_profile_policy_workspace_pattern_exact(const uint8_t *workspace,
	uint32_t raw_digest, uint32_t canonical_digest)
{
	return x58_workspace_state_workspace_pattern_exact(workspace, raw_digest,
		canonical_digest) ||
		x58_profile_policy_deferred_workspace_pattern_exact(workspace, raw_digest,
			canonical_digest);
}

static bool x58_csi_state_csi_dynamic_byte_is_known(uint8_t value)
{
	return value == 0x08 || value == 0x0c;
}

enum x58_raminit_auto_result {
	X58_RAMINIT_AUTO_READY,
	X58_RAMINIT_AUTO_FALLBACK,
	X58_RAMINIT_AUTO_COLD_CONTINUE,
	X58_RAMINIT_AUTO_UART_ERROR,
};

static void x58_raminit_capture_i801_signature(
	struct x58_raminit_i801_signature *signature)
{
	const uintptr_t base = CONFIG_FIXED_SMBUS_IO_BASE;

	/* HSTSTAT is deliberately not read here: it owns the SW semaphore. */
	signature->control = inb(base + I801_HSTCTL);
	signature->command = inb(base + I801_HSTCMD);
	signature->xmit_address = inb(base + I801_XMITADD);
	signature->data0 = inb(base + I801_HSTDAT0);
	signature->data1 = inb(base + I801_HSTDAT1);
}

static bool x58_raminit_i801_signature_matches(
	const struct x58_raminit_i801_signature *observed,
	const struct x58_raminit_i801_signature *expected)
{
	return observed->control == expected->control &&
		observed->command == expected->command &&
		observed->xmit_address == expected->xmit_address &&
		observed->data0 == expected->data0 &&
		observed->data1 == expected->data1;
}

static bool x58_raminit_i801_signature_is_zero(
	const struct x58_raminit_i801_signature *signature)
{
	return !(signature->control | signature->command |
		signature->xmit_address | signature->data0 | signature->data1);
}

static bool x58_init_phase_i801_signature_is_incomplete_phase(
	const struct x58_raminit_i801_signature *signature)
{
	return x58_raminit_i801_signature_matches(signature, &x58_init_phase_pass2_signature) ||
		x58_raminit_i801_signature_matches(signature, &x58_init_phase_consumed_signature) ||
		x58_raminit_i801_signature_matches(signature, &x58_init_phase_pass3_signature)
		|| x58_raminit_i801_signature_matches(signature,
			&x58_minit_in_progress_signature)
		;
}

static uint8_t x58_raminit_read_cmos_diagnostic(void)
{
	/* 0x74 reads back the RTC index. Intel 319973-003 contradicts itself on
	 * bit7 (13.6.1: always zero; 13.7.2: all bits readable). The existing
	 * sequence is retained, but original NMI-mask preservation is unproven;
	 * do not reuse this as a qualified diagnostic NMI-state accessor.
	 */
	const uint8_t saved_selector = inb(X58_RAMINIT_CMOS_INDEX_READBACK_PORT);
	uint8_t value;

	/* Match the MSI wrapper: select standard 0x0e while NMI is disabled. */
	outb(X58_RAMINIT_CMOS_DIAGNOSTIC_SELECTOR | BIT(7), X58_RAMINIT_CMOS_INDEX_PORT);
	(void)inb(0x61);
	value = inb(X58_RAMINIT_CMOS_DATA_PORT);
	outb(saved_selector, X58_RAMINIT_CMOS_INDEX_PORT);

	return value;
}

static bool x58_raminit_write_cmos_diagnostic(uint8_t value)
{
	/* See the index/NMI readback caveat in x58_raminit_read_cmos_diagnostic(). */
	const uint8_t saved_selector = inb(X58_RAMINIT_CMOS_INDEX_READBACK_PORT);
	uint8_t readback;

	/*
	 * X58_RAMINIT/V7 observed 0x6c; the separately selected X58_INIT_PHASE High-QPI trial
	 * uses the live opt-in value 0x2c.  The in-progress value 0xec has bits
	 * 7:6 set. Restore the read-back selector byte after the verified write;
	 * preservation of its original NMI bit is not established by this read.
	 */
	outb(X58_RAMINIT_CMOS_DIAGNOSTIC_SELECTOR | BIT(7), X58_RAMINIT_CMOS_INDEX_PORT);
	(void)inb(0x61);
	outb(value, X58_RAMINIT_CMOS_DATA_PORT);
	readback = inb(X58_RAMINIT_CMOS_DATA_PORT);
	outb(saved_selector, X58_RAMINIT_CMOS_INDEX_PORT);

	return readback == value;
}

static bool x58_raminit_set_i801_signature(
	const struct x58_raminit_i801_signature *signature)
{
	const uintptr_t base = CONFIG_FIXED_SMBUS_IO_BASE;
	const uint8_t status = inb(base + I801_HSTSTAT);
	struct x58_raminit_i801_signature readback;

	/* The first status read owns the semaphore only when INUSE was clear. */
	if (status & I801_HSTSTAT_INUSE)
		return false;
	if ((status & (I801_HSTSTAT_HOST_BUSY | I801_HSTSTAT_FLAGS)) ||
	    !x58_spd_read_control_idle(inb(base + I801_HSTCTL))) {
		x58_spd_read_release_host(0);
		return false;
	}

	/* These host registers have no bus side effect while START stays clear. */
	outb(signature->control, base + I801_HSTCTL);
	outb(signature->command, base + I801_HSTCMD);
	outb(signature->xmit_address, base + I801_XMITADD);
	outb(signature->data0, base + I801_HSTDAT0);
	outb(signature->data1, base + I801_HSTDAT1);
	x58_raminit_capture_i801_signature(&readback);
	if (!x58_raminit_i801_signature_matches(&readback, signature)) {
		x58_spd_read_release_host(0);
		return false;
	}
	x58_spd_read_release_host(0);
	x58_raminit_capture_i801_signature(&readback);

	return x58_raminit_i801_signature_matches(&readback, signature);
}

bool x58_profile_policy_rearm_deferred_guard_after_postmem(void)
{
	struct x58_raminit_i801_signature before = { 0 };
	struct x58_raminit_i801_signature after = { 0 };
	uint8_t cmos;
	bool exact;

	/* The deferred profiles retain MINIT's return tuple through all DRAM tests. */
	x58_raminit_capture_i801_signature(&before);
	cmos = x58_raminit_read_cmos_diagnostic();
	if (!x58_raminit_i801_signature_matches(&before,
		&x58_workspace_state_minit_return_signature) ||
	    cmos != X58_RAMINIT_CMOS_GUARD_IN_PROGRESS)
		return false;
	if (!x58_raminit_set_i801_signature(&x58_minit_in_progress_signature))
		return false;
	x58_raminit_capture_i801_signature(&after);
	exact = x58_raminit_i801_signature_matches(&after,
		&x58_minit_in_progress_signature);
	cmos = x58_raminit_read_cmos_diagnostic();
	if (!x58_romstage_uart_puts(
		"[RAMINIT] X58_MEMORY_PROFILE BROAD POSTMEM_REARM I801_PRE=") ||
	    !x58_romstage_uart_put_hex(before.control, 2) || !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(before.command, 2) || !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(before.xmit_address, 2) || !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(before.data0, 2) || !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(before.data1, 2) ||
	    !x58_romstage_uart_puts(" I801_POST=") ||
	    !x58_romstage_uart_put_hex(after.control, 2) || !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(after.command, 2) || !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(after.xmit_address, 2) || !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(after.data0, 2) || !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(after.data1, 2) ||
	    !x58_romstage_uart_puts(" REARMED_EXACT=") ||
	    !x58_romstage_uart_put_hex(exact, 2) || !x58_romstage_uart_puts(" CMOS0E=") ||
	    !x58_romstage_uart_put_hex(cmos, 2) ||
	    !x58_romstage_uart_puts(" AFTER_FULL_POSTMEM=01\r\n"))
		return false;

	return exact && cmos == X58_RAMINIT_CMOS_GUARD_IN_PROGRESS;
}

bool x58_memory_result_finalize_persistent_guard(void)
{
	struct x58_raminit_i801_signature i801;

	/* Refuse to clear anything unless the exact MINIT guard is still armed. */
	x58_raminit_capture_i801_signature(&i801);
	if (!x58_raminit_i801_signature_matches(&i801,
		&x58_minit_in_progress_signature) ||
	    x58_raminit_read_cmos_diagnostic() != X58_RAMINIT_CMOS_GUARD_IN_PROGRESS)
		return false;

	/* Clear I801 first, then restore the explicit High-QPI cold cookie. */
	if (!x58_raminit_set_i801_signature(&x58_raminit_clear_signature))
		return false;
	if (!x58_raminit_write_cmos_diagnostic(X58_INIT_PHASE_CMOS_COLD_AUTHORIZATION))
		return false;

	/* Both setters already completed an immediate exact readback. */
	return true;
}

static void __noreturn x58_raminit_guard_stop(const char *reason)
{
	outb(POST_X58_RAMINIT_GUARD_STOP, CONFIG_POST_IO_PORT);
	(void)x58_romstage_uart_puts("[RAMINIT] AUTO_GUARD_STOP=");
	(void)x58_romstage_uart_puts(reason);
	(void)x58_romstage_uart_puts(
		"; persistent guard retained; reboot to ROMMON, unlock RESET; "
		"autoguard clear; then remove AC power\r\n");
	(void)x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
		X58_EARLY_UART_FLUSH_POLL_LIMIT);
	stop_with_post(POST_X58_RAMINIT_GUARD_STOP);
}

static void __noreturn x58_init_phase_guard_stop(const char *reason)
{
	/*
	 * An I801 transition or the outer SYRE sequence can fail after the old
	 * phase still looks valid.  Poison the independent CMOS authorization so
	 * no warm reset can promote that stale signature to the next pass.
	 */
	const bool fail_lock_set = x58_raminit_write_cmos_diagnostic(
		X58_INIT_PHASE_CMOS_PHASE_FAILED);

	outb(POST_X58_RAMINIT_GUARD_STOP, CONFIG_POST_IO_PORT);
	(void)x58_romstage_uart_puts("[QPI] X58_MEMORY_RESULT_PHASE_GUARD_STOP=");
	(void)x58_romstage_uart_puts(reason);
	(void)x58_romstage_uart_puts(fail_lock_set ?
		"; CMOS0E=ed fail-lock set; " :
		"; CMOS fail-lock WRITE FAILED; ");
	(void)x58_romstage_uart_puts(
		"DO NOT WARM RESET; remove AC before recovery boot; clear autoguard in ROMMON; remove AC again before retry\r\n");
	(void)x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
		X58_EARLY_UART_FLUSH_POLL_LIMIT);
	stop_with_post(POST_X58_RAMINIT_GUARD_STOP);
}

static enum x58_raminit_auto_result x58_raminit_fallback(const char *reason)
{
	outb(POST_X58_RAMINIT_AUTO_FALLBACK, CONFIG_POST_IO_PORT);
	if (!x58_romstage_uart_puts("[RAMINIT] AUTO_FALLBACK=") ||
	    !x58_romstage_uart_puts(reason) ||
	    !x58_romstage_uart_puts("; normal profile halts without CAR ROMMON\r\n") ||
	    !x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
		X58_EARLY_UART_FLUSH_POLL_LIMIT))
		return X58_RAMINIT_AUTO_UART_ERROR;

	return X58_RAMINIT_AUTO_FALLBACK;
}

/*
 * The normal profile deliberately has no CAR command surface.  It must never
 * turn a rejected pre-RAM handoff into an implicit MINIT retry or a generic
 * register console.  The preceding fallback already records the first reason;
 * this marker makes the terminal policy unambiguous in a UART capture.
 */
static void __noreturn x58_normal_boot_fallback(void)
{
	(void)x58_romstage_uart_puts("[X58-NORMAL] EARLY_HANDOFF_REJECTED; "
		"external recovery required\r\n");
	(void)x58_romstage_uart_wait_for(UART8250_LSR_TEMT, X58_EARLY_UART_FLUSH_POLL_LIMIT);
	stop_with_post(POST_X58_RAMINIT_AUTO_FALLBACK);
}

#define X58_INIT_PHASE_CPU_QPI_LINK_DEV		PCI_DEV(0xff, 2, 0)
#define X58_INIT_PHASE_CPU_QPI_PHY_DEV		PCI_DEV(0xff, 2, 1)
#define X58_INIT_PHASE_IOH_QPI_LINK_DEV		PCI_DEV(0, 0x10, 0)
#define X58_INIT_PHASE_IOH_SR_DEV		PCI_DEV(0, 0x14, 1)
#define X58_INIT_PHASE_IOH_SYRE_DEV		PCI_DEV(0, 0x14, 2)
#define X58_INIT_PHASE_IOH_QPI0_ECAM		0xe0068000u
#define X58_INIT_PHASE_IOH_SYRE_REQUEST		BIT(10)
#define X58_INIT_PHASE_PH_PIS_HIGH_MASK		0x041f1f01u
#define X58_INIT_PHASE_PH_PIS_HIGH_VALUE	0x040f0f01u

enum x58_init_phase {
	X58_INIT_PHASE_PASS1 = 1,
	X58_INIT_PHASE_PASS2,
	X58_INIT_PHASE_PASS3,
};

struct x58_init_phase_qpi_tuple {
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


static void x58_init_phase_capture_qpi_tuple(struct x58_init_phase_qpi_tuple *tuple)
{
	tuple->cpu_50 = x58_rommon_pci_read(X58_INIT_PHASE_CPU_QPI_PHY_DEV, 0x50, 4);
	tuple->cpu_54 = x58_rommon_pci_read(X58_INIT_PHASE_CPU_QPI_PHY_DEV, 0x54, 4);
	tuple->cpu_6c = x58_rommon_pci_read(X58_INIT_PHASE_CPU_QPI_PHY_DEV, 0x6c, 4);
	tuple->cpu_80 = x58_rommon_pci_read(X58_INIT_PHASE_CPU_QPI_PHY_DEV, 0x80, 4);
	tuple->cpu_94 = x58_rommon_pci_read(X58_INIT_PHASE_CPU_QPI_PHY_DEV, 0x94, 4);
	tuple->cpu_9c = x58_rommon_pci_read(X58_INIT_PHASE_CPU_QPI_PHY_DEV, 0x9c, 4);
	tuple->cpu_a0 = x58_rommon_pci_read(X58_INIT_PHASE_CPU_QPI_PHY_DEV, 0xa0, 4);
	tuple->cpu_a4 = x58_rommon_pci_read(X58_INIT_PHASE_CPU_QPI_PHY_DEV, 0xa4, 4);
	tuple->cpu_link_50 = x58_rommon_pci_read(X58_INIT_PHASE_CPU_QPI_LINK_DEV, 0x50, 4);
	tuple->cpu_link_58 = x58_rommon_pci_read(X58_INIT_PHASE_CPU_QPI_LINK_DEV, 0x58, 4);
	/* PCIEXBAR is exact-gated before these read-only extended accesses. */
	tuple->ioh_82c = x58_rommon_mem_read(X58_INIT_PHASE_IOH_QPI0_ECAM + 0x82c, 4);
	tuple->ioh_840 = x58_rommon_mem_read(X58_INIT_PHASE_IOH_QPI0_ECAM + 0x840, 4);
	tuple->ioh_854 = x58_rommon_mem_read(X58_INIT_PHASE_IOH_QPI0_ECAM + 0x854, 4);
	tuple->ioh_85c = x58_rommon_mem_read(X58_INIT_PHASE_IOH_QPI0_ECAM + 0x85c, 4);
	tuple->ioh_864 = x58_rommon_mem_read(X58_INIT_PHASE_IOH_QPI0_ECAM + 0x864, 4);
	tuple->ioh_link_c8 = x58_rommon_pci_read(X58_INIT_PHASE_IOH_QPI_LINK_DEV, 0xc8, 4);
	tuple->ioh_sr0_7c = x58_rommon_pci_read(X58_INIT_PHASE_IOH_SR_DEV, 0x7c, 4);
	tuple->ioh_sr1_80 = x58_rommon_pci_read(X58_INIT_PHASE_IOH_SR_DEV, 0x80, 4);
	tuple->ioh_syre_cc = x58_rommon_pci_read(X58_INIT_PHASE_IOH_SYRE_DEV, 0xcc, 4);
}


static bool x58_csi_profile_capture_stable_pre_a0(uint32_t *saved_pre_a0)
{
	const uint32_t pre_a0_first =
		x58_rommon_pci_read(X58_INIT_PHASE_CPU_QPI_PHY_DEV, 0xa0, 4);
	const uint32_t pre_a0_second =
		x58_rommon_pci_read(X58_INIT_PHASE_CPU_QPI_PHY_DEV, 0xa0, 4);
	const bool normal_log_only =
		x58_vendor_normal_stable_pre_minit_a0(pre_a0_first);
	const bool historical_match =
		x58_vendor_csi_profile_high_qpi_cpu_a0_exact(pre_a0_first);
	const bool admitted = saved_pre_a0 != NULL &&
		pre_a0_first == pre_a0_second &&
		(historical_match || normal_log_only);

	if (!x58_romstage_uart_puts("[QPI] X58_CSI_PROFILE PRE_A0 FIRST/SECOND=") ||
	    !x58_romstage_uart_put_hex(pre_a0_first, 8) || !x58_romstage_uart_putc('/') ||
	    !x58_romstage_uart_put_hex(pre_a0_second, 8) ||
	    !x58_romstage_uart_puts(
		" X58_MEMORY_PROFILE_BASE_SET=00017000|00017200|00017400|00017600|00017800|00017a00|00017c00 HISTORICAL_MATCH=") ||
	    !x58_romstage_uart_put_hex(historical_match, 2) ||
	    !x58_romstage_uart_puts(" NORMAL_A0_POLICY=") ||
	    !x58_romstage_uart_puts(normal_log_only ? "LOG_ONLY" : "EXACT") ||
	    !x58_romstage_uart_puts(" STABLE=") || !x58_romstage_uart_put_hex(admitted, 2) ||
	    !x58_romstage_uart_puts("\r\n"))
		return false;
	if (saved_pre_a0 == NULL || pre_a0_first != pre_a0_second)
		return false;
	if (!admitted) {
		return false;
	}

	*saved_pre_a0 = pre_a0_first;
	return true;
}

static bool x58_init_phase_print_qpi_tuple(const char *label,
	const struct x58_init_phase_qpi_tuple *tuple)
{
	if (!x58_romstage_uart_detailed())
		return true;

	return x58_romstage_uart_puts("[QPI] ") && x58_romstage_uart_puts(label) &&
		x58_romstage_uart_puts(" CPU ff:02.1 50/54/6c/80=") &&
		x58_romstage_uart_put_hex(tuple->cpu_50, 8) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(tuple->cpu_54, 8) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(tuple->cpu_6c, 8) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(tuple->cpu_80, 8) &&
		x58_romstage_uart_puts(" 94/9c/a0/a4=") &&
		x58_romstage_uart_put_hex(tuple->cpu_94, 8) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(tuple->cpu_9c, 8) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(tuple->cpu_a0, 8) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(tuple->cpu_a4, 8) && x58_romstage_uart_puts("\r\n") &&
		x58_romstage_uart_puts("[QPI] ") && x58_romstage_uart_puts(label) &&
		x58_romstage_uart_puts(" IOH 00:0d.0 82c/840/854/85c/864=") &&
		x58_romstage_uart_put_hex(tuple->ioh_82c, 8) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(tuple->ioh_840, 8) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(tuple->ioh_854, 8) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(tuple->ioh_85c, 8) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(tuple->ioh_864, 8) && x58_romstage_uart_puts("\r\n") &&
		x58_romstage_uart_puts("[QPI] ") && x58_romstage_uart_puts(label) &&
		x58_romstage_uart_puts(" LINK CPU50/58=") &&
		x58_romstage_uart_put_hex(tuple->cpu_link_50, 8) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(tuple->cpu_link_58, 8) &&
		x58_romstage_uart_puts(" IOH_C8=") &&
		x58_romstage_uart_put_hex(tuple->ioh_link_c8, 8) &&
		x58_romstage_uart_puts(" SR0/SR1=") &&
		x58_romstage_uart_put_hex(tuple->ioh_sr0_7c, 8) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(tuple->ioh_sr1_80, 8) &&
		x58_romstage_uart_puts(" SYRE_CC=") &&
		x58_romstage_uart_put_hex(tuple->ioh_syre_cc, 8) && x58_romstage_uart_puts("\r\n");
}

static bool x58_init_phase_pre_outer_reset_tuple_exact(
	const struct x58_init_phase_qpi_tuple *tuple)
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

static bool x58_init_phase_cold_slow_qpi_cpu_tuple_exact(
	const struct x58_init_phase_qpi_tuple *tuple)
{
	/*
	 * This is the CPU-side Slow-QPI reset tuple observed in two independent
	 * verified G3 captures.  It is deliberately not the later pre-outer-reset
	 * tuple: CPU 54/6c/94/a4 have not reached those programmed values yet.
	 * Keep the reset subset separate for the ICH-reset-with-persistent-marker
	 * case because classification runs before PCIEXBAR is admitted, so
	 * extended IOH reads cannot be gates here.
	 */
	return tuple->cpu_50 == 0x160c0110 &&
		tuple->cpu_54 == 0x00000010 &&
		tuple->cpu_6c == 0x0000a020 &&
		tuple->cpu_80 == 0x030f0f03 &&
		tuple->cpu_94 == 0x00000102 &&
		tuple->cpu_9c == 0x00000502 &&
		tuple->cpu_a0 == 0x00000c00 &&
		tuple->cpu_a4 == 0x001d2c03;
}

static bool x58_init_phase_slow_qpi_entry_tuple_exact(
	const struct x58_init_phase_qpi_tuple *tuple, enum x58_init_phase phase)
{
	/*
	 * Strict composite Slow-QPI hypothesis assembled from immutable X58_RAMINIT
	 * observations.  The individual values are evidence-backed, but every
	 * field was not captured jointly at both AC-cold and pass-2 entry.  Fail
	 * closed on any mismatch.  SYRE is the only phase-dependent predicate.
	 */
	return (phase == X58_INIT_PHASE_PASS1 || phase == X58_INIT_PHASE_PASS2) &&
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
		tuple->ioh_syre_cc == (phase == X58_INIT_PHASE_PASS1 ?
			0x00000200 : 0x00000600);
}

static bool x58_init_phase_high_qpi_tuple_exact(const struct x58_init_phase_qpi_tuple *tuple)
{
	return tuple->cpu_50 == 0x160c0112 &&
		tuple->cpu_54 == 0x00000012 &&
		tuple->cpu_6c == 0x0040a0a0 &&
		tuple->cpu_80 == 0x070f0f03 &&
		tuple->cpu_94 == 0x00010202 &&
		tuple->cpu_9c == 0x00000502 &&
		 (x58_vendor_csi_profile_high_qpi_cpu_a0_exact(tuple->cpu_a0)
		 || x58_vendor_normal_stable_pre_minit_a0(tuple->cpu_a0)
		) &&
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
		(tuple->cpu_80 & X58_INIT_PHASE_PH_PIS_HIGH_MASK) ==
			X58_INIT_PHASE_PH_PIS_HIGH_VALUE &&
		(tuple->ioh_840 & X58_INIT_PHASE_PH_PIS_HIGH_MASK) ==
			X58_INIT_PHASE_PH_PIS_HIGH_VALUE;
}

struct x58_minit_stage_fingerprints {
	uint32_t cpu_context_80;
	uint32_t cpu_stage_d0;
	uint32_t ioh_stage_9c;
	uint32_t memory_clock_50;
	uint32_t memory_clock_54;
};

static void x58_minit_capture_stage_fingerprints(
	struct x58_minit_stage_fingerprints *stage)
{
	stage->cpu_context_80 = x58_rommon_pci_read(X58_INIT_PHASE_CPU_QPI_LINK_DEV, 0x80, 4);
	stage->cpu_stage_d0 = x58_rommon_pci_read(X58_INIT_PHASE_CPU_QPI_LINK_DEV, 0xd0, 4);
	stage->ioh_stage_9c = x58_rommon_pci_read(X58_INIT_PHASE_IOH_SR_DEV, 0x9c, 4);
	stage->memory_clock_50 = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_UNCORE_COMMON_DEV, 0x50, 4);
	stage->memory_clock_54 = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_UNCORE_COMMON_DEV, 0x54, 4);
}

static bool x58_minit_print_stage_fingerprints(const char *label,
	const struct x58_minit_stage_fingerprints *stage,
	const struct x58_vendor_call_result *csi_result)
{
	const uint32_t expected_context =
		(csi_result->edx + X58_MINIT_CSI_CONTEXT_DELTA) & 0xffff;

	if (!x58_romstage_uart_detailed())
		return true;

	return x58_romstage_uart_puts("[QPI] ") && x58_romstage_uart_puts(label) &&
		x58_romstage_uart_puts(" STAGE_TELEMETRY CPU ff:02.0 80/d0=") &&
		x58_romstage_uart_put_hex(stage->cpu_context_80, 8) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(stage->cpu_stage_d0, 8) &&
		x58_romstage_uart_puts(" PTR_LOW16_EXPECT=") &&
		x58_romstage_uart_put_hex(expected_context, 4) &&
		x58_romstage_uart_puts(" PTR_MATCH=") &&
		x58_romstage_uart_put_hex(stage->cpu_context_80 == expected_context, 2) &&
		x58_romstage_uart_puts(" IOH 00:14.1 9c=") &&
		x58_romstage_uart_put_hex(stage->ioh_stage_9c, 8) &&
		x58_romstage_uart_puts(" MC50/54=") &&
		x58_romstage_uart_put_hex(stage->memory_clock_50, 8) && x58_romstage_uart_putc('/') &&
		x58_romstage_uart_put_hex(stage->memory_clock_54, 8) &&
		x58_romstage_uart_puts("; CPU80/D0 READ_ONLY_NOT_GATES\r\n");
}



static bool x58_profile_policy_post_minit_common_endpoint_exact(
	const struct x58_init_phase_qpi_tuple *tuple,
	const struct x58_minit_stage_fingerprints *stage)
{
	/* A0 and CPU 9c are coupled by x58_profile_policy_profile_for_tuple(). */
	return tuple != NULL && stage != NULL &&
		tuple->cpu_50 == 0x160c0112 &&
		tuple->cpu_54 == 0x00000012 &&
		tuple->cpu_6c == 0x0040a0a8 &&
		tuple->cpu_80 == X58_MEMORY_RESULT_EXPECTED_QPI_STATUS &&
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
			X58_MEMORY_RESULT_EXPECTED_POST_MINIT_IOH_STAGE_9C &&
		stage->memory_clock_50 == 0x0a000006 &&
		stage->memory_clock_54 == 0x00000006;
}

static enum x58_raminit_auto_result x58_init_phase_promote_completed_warm_restart(
	struct x58_rommon_state *state, bool cold_fallback_allowed)
{
	struct x58_init_phase_qpi_tuple first;
	struct x58_init_phase_qpi_tuple second;
	struct x58_minit_stage_fingerprints first_stage;
	struct x58_minit_stage_fingerprints second_stage;
	const uint32_t mc_mapper = x58_rommon_pci_read(X58_VENDOR_POLICY_MC_COMMON_DEV,
		X58_VENDOR_POLICY_MC_CHANNEL_MAPPER, 4);
	const uint32_t mc_common_f8 = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_UNCORE_COMMON_DEV, 0xf8, 4);
	const uint32_t ch2_dod = x58_rommon_pci_read(X58_VENDOR_POLICY_CHANNEL2_ADDR_DEV,
		X58_VENDOR_POLICY_MC_DOD_DIMM0, 4);
	const uint32_t ch2_ranks = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV,
		X58_MEMORY_CONTROLLER_MC_RANK_PRESENT, 4);
	const uint32_t ch2_status = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV,
		X58_MEMORY_CONTROLLER_MC_INIT_STATUS, 4);
	bool identity_exact;
	bool endpoint_exact;
	bool cold_entry_exact;

	x58_init_phase_capture_qpi_tuple(&first);
	x58_minit_capture_stage_fingerprints(&first_stage);
	x58_init_phase_capture_qpi_tuple(&second);
	x58_minit_capture_stage_fingerprints(&second_stage);

	if (!x58_romstage_uart_puts("[QPI] WARM_REUSE=VALIDATE_LIVE_TRAINED_STATE; CSI/MINIT=SKIPPED; CPU_50/54/6C/80/94/9C/A0/A4=") ||
	    !x58_romstage_uart_put_hex(first.cpu_50, 8) || !x58_romstage_uart_putc('/') ||
	    !x58_romstage_uart_put_hex(first.cpu_54, 8) || !x58_romstage_uart_putc('/') ||
	    !x58_romstage_uart_put_hex(first.cpu_6c, 8) || !x58_romstage_uart_putc('/') ||
	    !x58_romstage_uart_put_hex(first.cpu_80, 8) || !x58_romstage_uart_putc('/') ||
	    !x58_romstage_uart_put_hex(first.cpu_94, 8) || !x58_romstage_uart_putc('/') ||
	    !x58_romstage_uart_put_hex(first.cpu_9c, 8) || !x58_romstage_uart_putc('/') ||
	    !x58_romstage_uart_put_hex(first.cpu_a0, 8) || !x58_romstage_uart_putc('/') ||
	    !x58_romstage_uart_put_hex(first.cpu_a4, 8) ||
	    !x58_romstage_uart_puts(" MC_MAP60/F8=") || !x58_romstage_uart_put_hex(mc_mapper, 8) ||
	    !x58_romstage_uart_putc('/') || !x58_romstage_uart_put_hex(mc_common_f8, 8) ||
	    !x58_romstage_uart_puts(" CH2_DOD/RANKS/STATUS=") ||
	    !x58_romstage_uart_put_hex(ch2_dod, 8) || !x58_romstage_uart_putc('/') ||
	    !x58_romstage_uart_put_hex(ch2_ranks, 8) || !x58_romstage_uart_putc('/') ||
	    !x58_romstage_uart_put_hex(ch2_status, 8) || !x58_romstage_uart_puts("\r\n"))
		return X58_RAMINIT_AUTO_UART_ERROR;

	identity_exact = state->vendor_spd_valid && x58_raminit_spd_is_exact(state) &&
		x58_vendor_entry_spd_topology_is_supported(state) &&
		cpuid_eax(1) == X58_RAMINIT_EXPECTED_CPUID &&
		get_current_microcode_rev() == X58_RAMINIT_EXPECTED_UCODE;
	if (!identity_exact)
		return x58_raminit_fallback("X58_INIT_PHASE_WARM_REUSE_LIVE_STATE_GATE");

	/* Only endpoint fields used by the cold post-MINIT contract are gates.
	 * CPU context 80/d0 remains telemetry because it contains call-local data.
	 */
	endpoint_exact = x58_profile_policy_post_minit_common_endpoint_exact(&first,
			&first_stage) &&
		x58_profile_policy_post_minit_common_endpoint_exact(&second, &second_stage) &&
		first.cpu_a0 == second.cpu_a0 && first.cpu_9c == second.cpu_9c &&
		first_stage.ioh_stage_9c == second_stage.ioh_stage_9c &&
		first_stage.memory_clock_50 == second_stage.memory_clock_50 &&
		first_stage.memory_clock_54 == second_stage.memory_clock_54 &&
		x58_memory_profile_handoff_normal_cpu_a0_candidate(first.cpu_a0) &&
		(first.cpu_9c == 0x00a00502u || first.cpu_9c == 0x00b00502u) &&
		mc_mapper == X58_RAMINIT_EXPECTED_MC_MAPPER &&
		mc_common_f8 == X58_RAMINIT_EXPECTED_MC_COMMON_F8 &&
		ch2_dod == X58_RAMINIT_EXPECTED_CH2_DOD &&
		ch2_ranks == X58_RAMINIT_EXPECTED_CH2_RANKS &&
		ch2_status == X58_RAMINIT_EXPECTED_CH2_STATUS;

	/*
	 * A battery-backed completed marker survives a real G3 interval. The
	 * 2026-09-20 60-second G3 capture showed the exact cold CPU-side tuple and
	 * zeroed IMC fields, whereas the preceding Linux CF9 reset retained the
	 * trained A0/9C pair 00017400/00b00502. Admit cold MINIT only on the
	 * double-sampled reset tuple plus wholly unconfigured IMC state. Do not
	 * use extended IOH fields here: PCIEXBAR has not yet been admitted.
	 */
	cold_entry_exact =
		x58_init_phase_cold_slow_qpi_cpu_tuple_exact(&first) &&
		x58_init_phase_cold_slow_qpi_cpu_tuple_exact(&second) &&
		mc_mapper == 0 && mc_common_f8 == 0 && ch2_dod == 0 &&
		ch2_ranks == 0 && ch2_status == 0;
	if (!endpoint_exact && cold_fallback_allowed && cold_entry_exact) {
		if (!x58_romstage_uart_puts("[QPI] WARM_REUSE_LIVE_STATE=COLD_RESET_TUPLE; "
			"PERSISTENT_MARKER_STALE; COLD_MINIT=CONTINUE\r\n"))
			return X58_RAMINIT_AUTO_UART_ERROR;
		return X58_RAMINIT_AUTO_COLD_CONTINUE;
	}

	/* A full ICH reset erases I801 state even when the CPU memory controller
	 * remains trained, while CMOS survives a genuine G3 cold start.  The
	 * retained marker therefore requests classification; it never authorizes
	 * a boot.  At this boundary there are exactly two admissible states:
	 * a validated trained endpoint above, or the exact cold reset tuple above.
	 * Unknown, mixed and merely "not known trained" states remain terminal.
	 */
	if (!endpoint_exact)
		return x58_raminit_fallback("X58_INIT_PHASE_WARM_REUSE_LIVE_STATE_GATE");

	/* Reuse the existing crash-consistent producer/finalizer transaction. */
	if (!x58_raminit_write_cmos_diagnostic(X58_RAMINIT_CMOS_GUARD_IN_PROGRESS) ||
	    !x58_raminit_set_i801_signature(&x58_minit_in_progress_signature))
		x58_init_phase_guard_stop("X58_INIT_PHASE_WARM_REUSE_GUARD_ARM");

	{
		const struct x58_raminit_result handoff = {
			.cpuid = X58_RAMINIT_EXPECTED_CPUID,
			.microcode_revision = X58_RAMINIT_EXPECTED_UCODE,
			.spd_fnv = state->vendor_spd_digest,
			.spd_profile_fnv = state->vendor_spd_profile_digest,
			.csi_state_fnv = 0,
			.policy_fnv = 0,
			.workspace_fnv = 0,
			.csi_state_canonical_fnv = 0,
			.workspace_canonical_fnv = 0,
			.minit_eax = X58_RAMINIT_WARM_REUSE_MARKER,
			.mc_mapper = mc_mapper,
			.mc_common_f8 = mc_common_f8,
			.ch2_dod = ch2_dod,
			.ch2_ranks = ch2_ranks,
			.ch2_status = ch2_status,
			.qpi_status = first.cpu_80,
			.post_minit_ioh_stage_9c = first_stage.ioh_stage_9c,
			.profile_id = X58_MEMORY_PROFILE_WARM_REUSE,
			.saved_pre_a0 = first.cpu_a0,
			.post_minit_cpu_a0 = first.cpu_a0,
			.post_minit_cpu_9c = first.cpu_9c,
		};

		if (!x58_raminit_result_is_exact(&handoff))
			x58_init_phase_guard_stop("X58_INIT_PHASE_WARM_REUSE_RESULT_GATE");
		x58_raminit_handoff_record_success(&handoff);
	}

	if (!x58_romstage_uart_puts("[RAMINIT] WARM_REUSE_AUTO_HANDOFF=READY; live endpoint stable; reserved-memory/CBMEM validation follows\r\n") ||
	    !x58_romstage_uart_wait_for(UART8250_LSR_TEMT, X58_EARLY_UART_FLUSH_POLL_LIMIT))
		return X58_RAMINIT_AUTO_UART_ERROR;
	outb(POST_X58_RAMINIT_MINIT_ACCEPTED, CONFIG_POST_IO_PORT);
	return X58_RAMINIT_AUTO_READY;
}

static uint32_t x58_csi_canonical_csi_canonical_digest(const uint8_t *state)
{
	uint32_t digest = 2166136261u;

	/* Canonicalize only the digest stream; never modify the raw CSI state. */
	for (size_t offset = 0; offset < X58_VENDOR_CSI_STATE_SIZE; offset++) {
		const uint8_t value = offset == X58_CSI_CANONICAL_CSI_DYNAMIC_OFFSET ?
			0 : state[offset];

		digest ^= value;
		digest *= 16777619u;
	}

	return digest;
}

static bool x58_csi_canonical_select_expected_csi_raw_digest(const uint8_t *state,
	uint32_t observed_raw_digest, uint32_t canonical_digest,
	uint32_t *expected_raw_digest)
{
	if (state == NULL || expected_raw_digest == NULL ||
	    canonical_digest != X58_CSI_CANONICAL_CSI_CANONICAL_FNV)
		return false;

	/* Select only a constant from an exact byte/raw-digest pair. */
	if (state[X58_CSI_CANONICAL_CSI_DYNAMIC_OFFSET] == 0x08 &&
	    observed_raw_digest == X58_CSI_CANONICAL_CSI_RAW_08_FNV) {
		*expected_raw_digest = X58_CSI_CANONICAL_CSI_RAW_08_FNV;
		return true;
	}
	if (state[X58_CSI_CANONICAL_CSI_DYNAMIC_OFFSET] == 0x0c &&
	    observed_raw_digest == X58_CSI_CANONICAL_CSI_RAW_0C_FNV) {
		*expected_raw_digest = X58_CSI_CANONICAL_CSI_RAW_0C_FNV;
		return true;
	}

	return false;
}

static bool x58_minit_csi_state_exact(const uint8_t *state)
{
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
}

enum x58_csi_profile_csi_profile {
	X58_CSI_PROFILE_CSI_REJECTED,
	X58_CSI_PROFILE_CSI_PRIMARY,
	X58_CSI_PROFILE_CSI_OBSERVATION,
	X58_MEMORY_PROFILE_CSI_BROAD_HARD_GATED,
};

static enum x58_csi_profile_csi_profile x58_csi_profile_select_csi_profile(
	uint32_t saved_pre_a0, const struct x58_init_phase_qpi_tuple *tuple,
	const uint8_t *state, uint32_t raw_digest)
{
	bool pair_08;
	bool pair_0c;
	bool pre_a0_admitted;

	pre_a0_admitted = x58_vendor_memory_profile_high_qpi_cpu_a0_candidate(saved_pre_a0) ||
		x58_vendor_normal_stable_pre_minit_a0(saved_pre_a0);
	if (tuple == NULL || state == NULL || !pre_a0_admitted ||
	    !x58_minit_csi_state_exact(state) ||
	    x58_csi_canonical_csi_canonical_digest(state) != X58_CSI_CANONICAL_CSI_CANONICAL_FNV)
		return X58_CSI_PROFILE_CSI_REJECTED;

	pair_08 = state[X58_CSI_CANONICAL_CSI_DYNAMIC_OFFSET] == 0x08 &&
		raw_digest == X58_CSI_CANONICAL_CSI_RAW_08_FNV;
	pair_0c = state[X58_CSI_CANONICAL_CSI_DYNAMIC_OFFSET] == 0x0c &&
		raw_digest == X58_CSI_CANONICAL_CSI_RAW_0C_FNV;
	if (!pair_08 && !pair_0c)
		return X58_CSI_PROFILE_CSI_REJECTED;

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
		return X58_CSI_PROFILE_CSI_REJECTED;

	/*
	 * The historical broad profile couples a stable seven-value A0 candidate;
	 * the non-interactive normal profile adds only its separately observed
	 * 0x00016e00 value.  Both require one of the two observed CPU-9c values and
	 * an exact CSI pair.  They authorize one MINIT call only; they are not a
	 * RAM-success claim.
	 */
	if (pre_a0_admitted &&
	    tuple->cpu_a0 == saved_pre_a0 &&
	    (tuple->cpu_9c == 0x00a00502u || tuple->cpu_9c == 0x00b00502u) &&
	    (pair_08 || pair_0c))
		return X58_MEMORY_PROFILE_CSI_BROAD_HARD_GATED;
	return X58_CSI_PROFILE_CSI_REJECTED;
}

static bool x58_profile_policy_o_pre_minit_candidate_exact(uint32_t saved_pre_a0,
	const struct x58_init_phase_qpi_tuple *tuple, const uint8_t *state,
	uint32_t raw_digest)
{
	/* This authorizes only the attempt; the shared full truth table promotes. */
	return tuple != NULL && state != NULL &&
		saved_pre_a0 == 0x00017c00 &&
		tuple->cpu_a0 == 0x00017c00 &&
		tuple->cpu_9c == 0x00a00502 &&
		state[X58_CSI_CANONICAL_CSI_DYNAMIC_OFFSET] ==
			X58_PROFILE_POLICY_EXPECTED_CSI_DYNAMIC_DEFERRED &&
		raw_digest == X58_PROFILE_POLICY_EXPECTED_CSI_RAW_8B38_FNV &&
		x58_csi_canonical_csi_canonical_digest(state) ==
			X58_MEMORY_RESULT_EXPECTED_CSI_CANONICAL_FNV;
}

static bool x58_csi_profile_post_csi_stage_exact(
	const struct x58_minit_stage_fingerprints *stage)
{
	return stage->ioh_stage_9c == 0xea000000 &&
		stage->memory_clock_50 == 0x0a000006 &&
		stage->memory_clock_54 == 0x00000006;
}

static bool x58_init_phase_print_csi_state(const uint8_t *state)
{
	if (!x58_romstage_uart_detailed())
		return true;

	for (unsigned int offset = 0; offset < X58_VENDOR_CSI_STATE_SIZE;
	     offset++) {
		if ((offset & 0xf) == 0 &&
		    (!x58_romstage_uart_puts("[QPI] CSI[") ||
		     !x58_romstage_uart_put_hex(offset, 4) || !x58_romstage_uart_puts("]=")))
			return false;
		if (!x58_romstage_uart_put_hex(state[offset], 2))
			return false;
		if ((offset & 0xf) == 0xf ||
		    offset + 1 == X58_VENDOR_CSI_STATE_SIZE) {
			if (!x58_romstage_uart_puts("\r\n"))
				return false;
		} else if (!x58_romstage_uart_putc(' ')) {
			return false;
		}
	}

	return true;
}

static bool x58_init_phase_result_is_self_consistent(
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

static bool x58_minit_print_complete_buffer(const char *name,
	const uint8_t *buffer, size_t size)
{
	if (!x58_romstage_uart_detailed())
		return true;

	if (buffer == NULL)
		return x58_romstage_uart_puts("[RAMINIT] BUFFER_UNAVAILABLE\r\n");

	for (size_t offset = 0; offset < size; offset++) {
		if ((offset & 0xf) == 0 &&
		    (!x58_romstage_uart_puts("[RAMINIT] ") || !x58_romstage_uart_puts(name) ||
		     !x58_romstage_uart_putc('[') || !x58_romstage_uart_put_hex(offset, 4) ||
		     !x58_romstage_uart_puts("]=")))
			return false;
		if (!x58_romstage_uart_put_hex(buffer[offset], 2))
			return false;
		if ((offset & 0xf) == 0xf || offset + 1 == size) {
			if (!x58_romstage_uart_puts("\r\n"))
				return false;
		} else if (!x58_romstage_uart_putc(' ')) {
			return false;
		}
	}

	return true;
}

static bool x58_minit_result_is_self_consistent(
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

static enum x58_raminit_auto_result x58_memory_result_promote_post_minit(
	struct x58_rommon_state *state, enum x58_vendor_status minit_call_status,
	const struct x58_vendor_call_result *csi_result,
	const struct x58_vendor_call_result *minit_result,
	const uint8_t *final_csi_state, const uint8_t *policy,
	const uint8_t *workspace, bool post_minit_i801_exact,
	uint8_t post_minit_cmos
	, uint32_t saved_pre_a0,
	const struct x58_init_phase_qpi_tuple *pre_minit_tuple
	)
{
	struct x58_vendor_runtime_info info = { 0 };
	struct x58_init_phase_qpi_tuple post_minit_tuple;
	struct x58_minit_stage_fingerprints post_minit_stage;
	const enum x58_vendor_status probe_status =
		x58_vendor_memory_result_post_minit_probe(&info);
	uint32_t workspace_fnv = 0;
	uint32_t workspace_canonical_fnv = 0;
	bool workspace_pattern_exact = false;
	bool probe_status_admitted;
	uint32_t final_csi_raw_fnv = 0;
	uint32_t final_csi_canonical_fnv = 0;
	enum x58_profile_policy_profile_id profile_id = X58_PROFILE_POLICY_PROFILE_INVALID;
	uint32_t mc_mapper;
	uint32_t mc_common_f8;
	uint32_t ch2_dod;
	uint32_t ch2_ranks;
	uint32_t ch2_status;

	if (workspace != NULL) {
		workspace_fnv = x58_vendor_entry_buffer_digest(workspace,
			X58_VENDOR_MINIT_WORKSPACE_SIZE);
		workspace_canonical_fnv =
			x58_memory_handoff_workspace_canonical_digest(workspace);
		workspace_pattern_exact = x58_profile_policy_workspace_pattern_exact(workspace,
			workspace_fnv, workspace_canonical_fnv);
	}
	/*
	 * The unchanged vendor X58_MEMORY_RESULT platform gate can report PLATFORM_STATE for a
	 * later build's otherwise exact workspace.  Older builds admit that status
	 * only through their workspace contract.  X58_MEMORY_PROFILE instead admits the status
	 * here because every non-workspace platform field is independently hard-gated
	 * below; it still rejects every other probe status.
	 */
	probe_status_admitted = probe_status == X58_VENDOR_OK ||
		(probe_status == X58_VENDOR_ERR_PLATFORM_STATE &&
		 /* Every platform field is independently hard-gated below. */
		 true);
	x58_init_phase_capture_qpi_tuple(&post_minit_tuple);
	x58_minit_capture_stage_fingerprints(&post_minit_stage);
	if (final_csi_state != NULL) {
		final_csi_raw_fnv = x58_vendor_entry_buffer_digest(final_csi_state,
			X58_VENDOR_CSI_STATE_SIZE);
		final_csi_canonical_fnv =
			x58_csi_canonical_csi_canonical_digest(final_csi_state);
	}
	profile_id = x58_profile_policy_profile_for_tuple(saved_pre_a0,
		post_minit_tuple.cpu_a0, post_minit_tuple.cpu_9c,
		final_csi_state == NULL ? 0xff :
			final_csi_state[X58_CSI_CANONICAL_CSI_DYNAMIC_OFFSET],
		final_csi_raw_fnv, final_csi_canonical_fnv, workspace_fnv,
		workspace_canonical_fnv);
	if (!x58_vendor_entry_print_runtime(state) ||
	    !x58_romstage_uart_puts(
		"[RAMINIT] X58_MEMORY_PROFILE BROAD RETURN POLICY_IN/NOW=") ||
	    !x58_romstage_uart_put_hex(info.minit_policy_digest, 8) ||
	    !x58_romstage_uart_putc('/') ||
	    !x58_romstage_uart_put_hex(info.minit_policy_current_digest, 8) ||
	    !x58_romstage_uart_puts(" WORK_FNV=") ||
	    !x58_romstage_uart_put_hex(workspace_fnv, 8) ||
	    !x58_romstage_uart_puts(" DRAM_ACCESSES=00\r\n") ||
	    !x58_init_phase_print_qpi_tuple("POST_MINIT", &post_minit_tuple) ||
	    !x58_minit_print_stage_fingerprints("POST_MINIT", &post_minit_stage,
		csi_result) ||
	    !x58_vendor_entry_emit_status(
		"X58_MEMORY_PROFILE_BROAD_POST_MINIT_PROBE",
		probe_status))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (!x58_romstage_uart_puts(
		"[RAMINIT] X58_MEMORY_PROFILE WORK_CANON_TELEMETRY=") ||
	    !x58_romstage_uart_put_hex(workspace_canonical_fnv, 8) ||
	    !x58_romstage_uart_puts(" WORK[2459..245c/2461]=") ||
	    !x58_romstage_uart_put_hex(workspace == NULL ? 0 : workspace[0x2459], 2) ||
	    !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(workspace == NULL ? 0 : workspace[0x245a], 2) ||
	    !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(workspace == NULL ? 0 : workspace[0x245b], 2) ||
	    !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(workspace == NULL ? 0 : workspace[0x245c], 2) ||
	    !x58_romstage_uart_putc('/') ||
	    !x58_romstage_uart_put_hex(workspace == NULL ? 0 : workspace[0x2461], 2) ||
	    !x58_romstage_uart_puts(" WORKSPACE_PATTERN_TELEMETRY=") ||
	    !x58_romstage_uart_put_hex(workspace_pattern_exact, 2) ||
	    !x58_romstage_uart_puts(" PROBE_STATUS_ADMITTED=") ||
	    !x58_romstage_uart_put_hex(probe_status_admitted, 2) ||
	    !x58_romstage_uart_puts(" WORKSPACE_HASH_MODE=RAW_AND_CANONICAL_TELEMETRY_ONLY") ||
	    !x58_romstage_uart_puts("\r\n"))
		return X58_RAMINIT_AUTO_UART_ERROR;

	mc_mapper = x58_rommon_pci_read(X58_VENDOR_POLICY_MC_COMMON_DEV,
		X58_VENDOR_POLICY_MC_CHANNEL_MAPPER, 4);
	mc_common_f8 = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_UNCORE_COMMON_DEV, 0xf8, 4);
	ch2_dod = x58_rommon_pci_read(X58_VENDOR_POLICY_CHANNEL2_ADDR_DEV,
		X58_VENDOR_POLICY_MC_DOD_DIMM0, 4);
	ch2_ranks = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV,
		X58_MEMORY_CONTROLLER_MC_RANK_PRESENT, 4);
	ch2_status = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV,
		X58_MEMORY_CONTROLLER_MC_INIT_STATUS, 4);
	if (!x58_romstage_uart_puts(
		"[RAMINIT] X58_MEMORY_PROFILE BROAD WORK_RAW_TELEMETRY=") ||
	    !x58_romstage_uart_put_hex(workspace_fnv, 8) ||
	    !x58_romstage_uart_puts(" MC_MAP60=") || !x58_romstage_uart_put_hex(mc_mapper, 8) ||
	    !x58_romstage_uart_puts(" MC_F8=") || !x58_romstage_uart_put_hex(mc_common_f8, 8) ||
	    !x58_romstage_uart_puts(" CH2_DOD=") || !x58_romstage_uart_put_hex(ch2_dod, 8) ||
	    !x58_romstage_uart_puts(" CH2_RANKS=") || !x58_romstage_uart_put_hex(ch2_ranks, 8) ||
	    !x58_romstage_uart_puts(" CH2_STATUS=") || !x58_romstage_uart_put_hex(ch2_status, 8) ||
	    !x58_romstage_uart_puts(" IOH_STAGE9C=") ||
	    !x58_romstage_uart_put_hex(post_minit_stage.ioh_stage_9c, 8) ||
	    !x58_romstage_uart_puts("\r\n"))
		return X58_RAMINIT_AUTO_UART_ERROR;

	/*
	 * The profile policy admits complete rows from the shared truth table.
	 * X58_MEMORY_PROFILE instead selects its version-9 broad CPU/CSI hard class and treats
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
	    !info.csi_result_accepted || !info.x58_init_phase_high_qpi_csi_profile ||
	    !info.x58_minit_high_qpi_minit_authorized ||
	    !info.minit_policy_confirmed || info.minit_armed ||
	    !info.minit_call_attempted || !info.minit_returned ||
	    info.vendor_status_seed != 0 ||
	    info.cpuid_1_eax != X58_RAMINIT_EXPECTED_CPUID ||
	    info.microcode_revision != X58_RAMINIT_EXPECTED_UCODE ||
	    info.uncore_sad_id != 0x2d818086 ||
	    info.pciexbar_low != 0xe0000001 || info.pciexbar_high != 0 ||
	    info.x58_hostbridge_id != 0x34058086 ||
	    info.x58_hostbridge_class_revision != 0x06000013 ||
	    info.qpi_phy_observed_80 != X58_MEMORY_RESULT_EXPECTED_QPI_STATUS ||
	    info.memory_clock_observed_50 != 0x0a000006 ||
	    info.memory_clock_observed_54 != 0x00000006 ||
	    !x58_minit_csi_state_exact(final_csi_state) ||
	    info.csi_state_digest != final_csi_raw_fnv ||
	    x58_vendor_entry_buffer_digest(final_csi_state,
		X58_VENDOR_CSI_STATE_SIZE) != final_csi_raw_fnv ||
	    x58_csi_canonical_csi_canonical_digest(final_csi_state) !=
		final_csi_canonical_fnv ||
	    info.minit_policy_digest != X58_RAMINIT_EXPECTED_POLICY_FNV ||
	    info.minit_policy_current_digest !=
		X58_RAMINIT_EXPECTED_POLICY_FNV ||
	    x58_vendor_entry_policy_digest(policy) != X58_RAMINIT_EXPECTED_POLICY_FNV ||
	    info.minit_workspace_digest != workspace_fnv ||
	    profile_id != X58_MEMORY_PROFILE_BROAD ||
	    !x58_profile_policy_profile_tuple_exact(profile_id, saved_pre_a0,
		post_minit_tuple.cpu_a0, post_minit_tuple.cpu_9c,
		final_csi_raw_fnv, final_csi_canonical_fnv, workspace_fnv,
		workspace_canonical_fnv) ||
	    pre_minit_tuple->cpu_a0 != saved_pre_a0 ||
	    pre_minit_tuple->cpu_a0 != post_minit_tuple.cpu_a0 ||
	    pre_minit_tuple->cpu_9c != post_minit_tuple.cpu_9c ||
	    minit_result->eax != 0 || workspace[1] != 0 || workspace[2] != 0 ||
	    workspace[X58_VENDOR_POLICY_WORK_B3_FLAGS_OFFSET] != 0x02 ||
	    workspace[X58_VENDOR_POLICY_WORK_COMPLETE_OFFSET] != 0x01 ||
	    policy[X58_VENDOR_POLICY_STATUS_OFFSET] != 0 ||
	    !x58_minit_result_is_self_consistent(minit_result, &info) ||
	    !post_minit_i801_exact ||
	    post_minit_cmos != X58_RAMINIT_CMOS_GUARD_IN_PROGRESS ||
	    !x58_profile_policy_post_minit_common_endpoint_exact(&post_minit_tuple,
		&post_minit_stage) ||
	    mc_mapper != X58_RAMINIT_EXPECTED_MC_MAPPER ||
	    mc_common_f8 != X58_RAMINIT_EXPECTED_MC_COMMON_F8 ||
	    ch2_dod != X58_RAMINIT_EXPECTED_CH2_DOD ||
	    ch2_ranks != X58_RAMINIT_EXPECTED_CH2_RANKS ||
	    ch2_status != X58_RAMINIT_EXPECTED_CH2_STATUS ||
	    !x58_raminit_spd_is_exact(state))
		return x58_raminit_fallback(
			"X58_MEMORY_PROFILE_BROAD_POST_MINIT_HARD_GATE");

	if (!x58_romstage_uart_puts(
		"[RAMINIT] X58_MEMORY_PROFILE BROAD_UNSAFE HARD_RETURN_GATE=PASS; WORKSPACE_HASHES=TELEMETRY_ONLY; DRAM_NOT_YET_PROVEN; bounded UC tests follow\r\n") ||
	    !x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
		X58_EARLY_UART_FLUSH_POLL_LIMIT))
		return X58_RAMINIT_AUTO_UART_ERROR;

	/*
	 * MINIT owns these otherwise idle I801 host registers and reproducibly
	 * leaves the exact return tuple checked by the caller.  Rearm only after
	 * every PRIMARY workspace, CPU, IMC, IOH, QPI, ABI, and CMOS gate above
	 * has passed.  The post-memory finalizer therefore still has a unique
	 * in-progress marker to consume after all destructive DRAM readbacks.
	 */
	if (x58_profile_policy_profile_is_primary(profile_id))
	{
		struct x58_raminit_i801_signature rearmed_i801 = { 0 };
		uint8_t rearmed_cmos;
		bool rearmed_exact;

		if (!x58_raminit_set_i801_signature(
			&x58_minit_in_progress_signature))
			x58_init_phase_guard_stop("X58_WORKSPACE_STATE_POST_MINIT_I801_REARM_WRITE");
		x58_raminit_capture_i801_signature(&rearmed_i801);
		rearmed_exact = x58_raminit_i801_signature_matches(&rearmed_i801,
			&x58_minit_in_progress_signature);
		rearmed_cmos = x58_raminit_read_cmos_diagnostic();
		if (!x58_romstage_uart_puts(
			"[RAMINIT] X58_WORKSPACE_STATE POST_MINIT_REARM I801=") ||
		    !x58_romstage_uart_put_hex(rearmed_i801.control, 2) ||
		    !x58_romstage_uart_putc(':') ||
		    !x58_romstage_uart_put_hex(rearmed_i801.command, 2) ||
		    !x58_romstage_uart_putc(':') ||
		    !x58_romstage_uart_put_hex(rearmed_i801.xmit_address, 2) ||
		    !x58_romstage_uart_putc(':') ||
		    !x58_romstage_uart_put_hex(rearmed_i801.data0, 2) ||
		    !x58_romstage_uart_putc(':') ||
		    !x58_romstage_uart_put_hex(rearmed_i801.data1, 2) ||
		    !x58_romstage_uart_puts(" REARMED_EXACT=") ||
		    !x58_romstage_uart_put_hex(rearmed_exact, 2) ||
		    !x58_romstage_uart_puts(" CMOS0E=") ||
		    !x58_romstage_uart_put_hex(rearmed_cmos, 2) ||
		    !x58_romstage_uart_puts("\r\n"))
			return X58_RAMINIT_AUTO_UART_ERROR;
		if (!rearmed_exact ||
		    rearmed_cmos != X58_RAMINIT_CMOS_GUARD_IN_PROGRESS)
			x58_init_phase_guard_stop("X58_WORKSPACE_STATE_POST_MINIT_I801_REARM_READBACK");
	}

	{
		const struct x58_raminit_result handoff = {
			.cpuid = info.cpuid_1_eax,
			.microcode_revision = info.microcode_revision,
			.spd_fnv = state->vendor_spd_digest,
			.spd_profile_fnv = state->vendor_spd_profile_digest,
			.csi_state_fnv = info.csi_state_digest,
			.policy_fnv = info.minit_policy_digest,
			.workspace_fnv = info.minit_workspace_digest,
			.csi_state_canonical_fnv = final_csi_canonical_fnv,
			.workspace_canonical_fnv = workspace_canonical_fnv,
			.minit_eax = minit_result->eax,
			.mc_mapper = mc_mapper,
			.mc_common_f8 = mc_common_f8,
			.ch2_dod = ch2_dod,
			.ch2_ranks = ch2_ranks,
			.ch2_status = ch2_status,
			.qpi_status = post_minit_tuple.cpu_80,
			.post_minit_ioh_stage_9c = post_minit_stage.ioh_stage_9c,
			.profile_id = profile_id,
			.saved_pre_a0 = saved_pre_a0,
			.post_minit_cpu_a0 = post_minit_tuple.cpu_a0,
			.post_minit_cpu_9c = post_minit_tuple.cpu_9c,
		};

		x58_raminit_handoff_record_success(&handoff);
	}
	if (!x58_romstage_uart_puts(
		"[RAMINIT] X58_MEMORY_PROFILE_BROAD_AUTO_HANDOFF=READY PROFILE=") ||
	    !x58_romstage_uart_put_hex(profile_id, 2) ||
	    !x58_romstage_uart_puts(x58_profile_policy_profile_requires_deferred_rearm(profile_id) ?
		"; BROAD I801 rearm deferred until full v10 postmem; provisional payload attempt\r\n" :
		"; inherited PRIMARY I801 rearm complete; provisional payload attempt\r\n") ||
	    !x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
		X58_EARLY_UART_FLUSH_POLL_LIMIT))
		return X58_RAMINIT_AUTO_UART_ERROR;
	outb(POST_X58_RAMINIT_MINIT_ACCEPTED, CONFIG_POST_IO_PORT);

	return X58_RAMINIT_AUTO_READY;
}

static enum x58_raminit_auto_result x58_minit_high_qpi_minit_observe(
	struct x58_rommon_state *state,
	const struct x58_vendor_call_result *csi_result,
	const struct x58_vendor_runtime_info *csi_info,
	const uint8_t *csi_state, uint32_t csi_raw_fnv,
	const struct x58_init_phase_qpi_tuple *csi_tuple,
	enum x58_vendor_status csi_call_status,
	enum x58_vendor_status csi_probe_status
	, uint32_t saved_pre_a0
	)
{
	struct x58_vendor_call_result minit_result = { 0 };
	struct x58_vendor_runtime_info info = { 0 };
	struct x58_raminit_i801_signature i801 = { 0 };
	struct x58_init_phase_qpi_tuple post_minit_tuple;
	struct x58_minit_stage_fingerprints csi_stage;
	struct x58_minit_stage_fingerprints post_minit_stage;
	const uint8_t *final_csi_state;
	const uint8_t *policy;
	const uint8_t *workspace;
	enum x58_vendor_status status;
	uint32_t policy_base_fnv;
	uint32_t policy_final_fnv;
	uint32_t workspace_fnv = 0;
	uint32_t csi_canonical_fnv = 0;
	uint32_t expected_csi_raw_fnv = 0;
	bool csi_digest_pair_exact = false;
	enum x58_csi_profile_csi_profile csi_profile = X58_CSI_PROFILE_CSI_REJECTED;
	bool x58_profile_policy_o_candidate = false;
	uint8_t post_minit_cmos;
	bool post_minit_i801_exact;
	bool pre_minit_i801_consumed;
	bool full_path_observed;

	x58_minit_capture_stage_fingerprints(&csi_stage);
	if (!x58_minit_print_stage_fingerprints("PASS3_RETURN", &csi_stage,
		csi_result))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (csi_state != NULL) {
		csi_canonical_fnv = x58_csi_canonical_csi_canonical_digest(csi_state);
		csi_digest_pair_exact = x58_csi_canonical_select_expected_csi_raw_digest(
			csi_state, csi_raw_fnv, csi_canonical_fnv,
			&expected_csi_raw_fnv);
	}
	csi_profile = x58_csi_profile_select_csi_profile(saved_pre_a0, csi_tuple,
		csi_state, csi_raw_fnv);
	x58_profile_policy_o_candidate = x58_profile_policy_o_pre_minit_candidate_exact(saved_pre_a0, csi_tuple,
			csi_state, csi_raw_fnv);
	if (!x58_romstage_uart_puts("[QPI] X58_CSI_CANONICAL CSI2A6=") ||
	    !(csi_state != NULL ?
		x58_romstage_uart_put_hex(csi_state[X58_CSI_CANONICAL_CSI_DYNAMIC_OFFSET], 2) :
		x58_romstage_uart_puts("NA")) ||
	    !x58_romstage_uart_puts(" RAW_FNV=") ||
	    !x58_romstage_uart_put_hex(csi_raw_fnv, 8) ||
	    !x58_romstage_uart_puts(" CANONICAL_2A6_ZERO_FNV=") ||
	    !x58_romstage_uart_put_hex(csi_canonical_fnv, 8) ||
	    !x58_romstage_uart_puts(" EXPECT=908dabb6 PAIRED=") ||
	    !x58_romstage_uart_put_hex(csi_digest_pair_exact, 2) ||
	    !x58_romstage_uart_puts(" RAW_BUFFER_MUTATED=00\r\n"))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (csi_call_status != X58_VENDOR_OK ||
	    csi_probe_status != X58_VENDOR_OK || csi_state == NULL ||
	    !csi_info->prepared || !csi_info->canaries_valid ||
	    !csi_info->wrapper_signature_valid ||
	    !csi_info->csi_signature_valid ||
	    !csi_info->minit_signature_valid ||
	    !csi_info->csi_call_attempted || !csi_info->csi_returned ||
	    csi_info->csi_result_accepted ||
	    csi_info->vendor_status_seed != 0 ||
	    csi_result->eax != 0 || csi_result->ebx != 0 ||
	    csi_result->ecx != 0x11 ||
	    csi_info->csi_state_digest != csi_raw_fnv ||
	    !csi_digest_pair_exact ||
	    !x58_minit_csi_state_exact(csi_state) ||
	    !x58_init_phase_result_is_self_consistent(csi_result, csi_info) ||
	    !csi_info->x58_csi_profile_pre_csi_cpu_a0_valid ||
	    csi_info->x58_csi_profile_pre_csi_cpu_a0 != saved_pre_a0 ||
	    !x58_csi_state_admit(csi_profile != X58_CSI_PROFILE_CSI_REJECTED,
		"ROMSTAGE_POST_CSI_PROFILE") ||
	    !x58_csi_state_admit(x58_csi_profile_post_csi_stage_exact(&csi_stage),
		"ROMSTAGE_POST_CSI_STAGE"))
		return x58_raminit_fallback("X58_CSI_PROFILE_COUPLED_HIGH_CSI_GATE");

	if (!x58_romstage_uart_puts(
		"[QPI] X58_MEMORY_PROFILE BROAD_UNSAFE_PROFILE=") ||
	    !x58_romstage_uart_puts(csi_profile == X58_MEMORY_PROFILE_CSI_BROAD_HARD_GATED ?
		"BROAD_HARD_GATED" : "REJECTED") ||
	    !x58_romstage_uart_puts(" PRE_A0=") ||
	    !x58_romstage_uart_put_hex(saved_pre_a0, 8) ||
	    !x58_romstage_uart_puts(" POST_A0/9C=") ||
	    !x58_romstage_uart_put_hex(csi_tuple->cpu_a0, 8) || !x58_romstage_uart_putc('/') ||
	    !x58_romstage_uart_put_hex(csi_tuple->cpu_9c, 8) ||
	    !x58_romstage_uart_puts("\r\n"))
		return X58_RAMINIT_AUTO_UART_ERROR;

	/* Keep the rejected classification and all raw values in the log. The
	 * warning-only admission does not turn it into a measured profile match.
	 */
	if (!x58_romstage_uart_puts("[CSI-POLICY] MODE=LOG_ONLY ABI_INTEGRITY=REQUIRED POST_MINIT_GATES=STRICT\r\n"))
		return X58_RAMINIT_AUTO_UART_ERROR;
	status = x58_vendor_accept_csi_result(0, 0, 0x11,
		expected_csi_raw_fnv,
		X58_VENDOR_WRITE_CONFIRMATION);
	if (!x58_vendor_entry_emit_status("X58_MINIT_HIGH_CSI_OBSERVATION_ACCEPT", status))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK)
		return x58_raminit_fallback("X58_MINIT_HIGH_CSI_OBSERVATION_ACCEPT");
	status = x58_vendor_minit_authorize_high_qpi_minit(
		X58_VENDOR_WRITE_CONFIRMATION);
	if (!x58_vendor_entry_emit_status("X58_MINIT_HIGH_QPI_MINIT_AUTHORIZE", status))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK)
		return x58_raminit_fallback("X58_MINIT_HIGH_QPI_MINIT_AUTHORIZE");
	status = x58_vendor_runtime_probe(&info);
	if (status != X58_VENDOR_OK || !info.csi_result_accepted ||
	    !info.x58_minit_high_qpi_minit_authorized ||
	    info.csi_state_digest != expected_csi_raw_fnv)
		return x58_raminit_fallback("X58_MINIT_HIGH_CSI_AUTH_READBACK");
	if (!x58_romstage_uart_puts(
		"[QPI] HIGH_CSI_OBSERVATION=ACCEPTED_FOR_ONE_MINIT_CALL; not semantic CSI success\r\n"))
		return X58_RAMINIT_AUTO_UART_ERROR;

	/*
	 * CSI EAX=0 plus diagnostic-invalid CMOS already constructs a cold-policy
	 * base.  Apply the same seven reviewed edits directly; do not call the
	 * X58_VENDOR_POLICY tuple-2/2/106 cold-conversion helper.
	 */
	if (!x58_vendor_entry_policy_reset(state))
		return X58_RAMINIT_AUTO_UART_ERROR;
	policy_base_fnv = x58_vendor_entry_policy_digest(state->vendor_policy);
	if (!state->vendor_policy_ready || state->vendor_policy_modified ||
	    policy_base_fnv != X58_MINIT_POLICY_BASE_FNV)
		return x58_raminit_fallback("X58_MINIT_POLICY_BASE_GATE");
	for (size_t index = 0; index < ARRAY_SIZE(x58_raminit_candidate_edits); index++)
		state->vendor_policy[x58_raminit_candidate_edits[index].offset] =
			x58_raminit_candidate_edits[index].value;
	state->vendor_policy_modified = true;
	policy_final_fnv = x58_vendor_entry_policy_digest(state->vendor_policy);
	if (policy_final_fnv != X58_RAMINIT_EXPECTED_POLICY_FNV ||
	    state->vendor_policy[X58_VENDOR_POLICY_STATUS_OFFSET] != 0 ||
	    x58_vendor_entry_policy_read32(state->vendor_policy,
		X58_VENDOR_POLICY_FLAGS_OFFSET) != X58_RAMINIT_POLICY_COLD_FLAGS)
		return x58_raminit_fallback("X58_MINIT_POLICY_FINAL_GATE");
	if (!x58_romstage_uart_puts("[RAMINIT] X58_MINIT POLICY BASE_FNV=") ||
	    !x58_romstage_uart_put_hex(policy_base_fnv, 8) ||
	    !x58_romstage_uart_puts(" SEVEN_EDIT_FNV=") ||
	    !x58_romstage_uart_put_hex(policy_final_fnv, 8) ||
	    !x58_romstage_uart_puts(" FORCE_COLD_HELPER=00\r\n"))
		return X58_RAMINIT_AUTO_UART_ERROR;

	status = x58_vendor_install_confirmed_minit_policy(
		state->vendor_policy, sizeof(state->vendor_policy),
		X58_RAMINIT_EXPECTED_POLICY_FNV,
		X58_VENDOR_WRITE_CONFIRMATION);
	if (!x58_vendor_entry_emit_status("X58_MINIT_POLICY_INSTALL", status))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK)
		return x58_raminit_fallback("X58_MINIT_POLICY_INSTALL");
	status = x58_vendor_runtime_probe(&info);
	if (status != X58_VENDOR_OK || !info.minit_policy_confirmed ||
	    info.minit_policy_digest != X58_RAMINIT_EXPECTED_POLICY_FNV ||
	    info.minit_policy_current_digest != X58_RAMINIT_EXPECTED_POLICY_FNV)
		return x58_raminit_fallback("X58_MINIT_POLICY_INSTALL_READBACK");

	status = x58_vendor_arm_minit(X58_VENDOR_WRITE_CONFIRMATION);
	if (!x58_vendor_entry_emit_status("X58_MINIT_ARM", status))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK)
		return x58_raminit_fallback("X58_MINIT_ARM");
	x58_raminit_capture_i801_signature(&i801);
	pre_minit_i801_consumed = x58_raminit_i801_signature_matches(&i801,
		&x58_init_phase_consumed_signature);
	if (!x58_romstage_uart_puts("[RAMINIT] PRE_MINIT_I801=") ||
	    !x58_romstage_uart_put_hex(i801.control, 2) || !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(i801.command, 2) || !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(i801.xmit_address, 2) || !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(i801.data0, 2) || !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(i801.data1, 2) ||
	    !x58_romstage_uart_puts(" EXPECT_CONSUMED_08:42:bd:7a:85_MATCH=") ||
	    !x58_romstage_uart_put_hex(pre_minit_i801_consumed, 2) ||
	    !x58_romstage_uart_puts(" CMOS0E=") ||
	    !x58_romstage_uart_put_hex(x58_raminit_read_cmos_diagnostic(), 2) ||
	    !x58_romstage_uart_puts("\r\n"))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (!pre_minit_i801_consumed ||
	    x58_raminit_read_cmos_diagnostic() != X58_RAMINIT_CMOS_GUARD_IN_PROGRESS)
		return x58_raminit_fallback("X58_MINIT_PRE_MINIT_PERSISTENCE_GATE");
	if (!x58_romstage_uart_puts(
		"[RAMINIT] X58_MEMORY_PROFILE BROAD_UNSAFE ONE_MINIT_CALL=AUTHORIZED; not trained; I801=08:64:9b:5c:a3 CMOS0E=ec; POST=d3/d4; reset/HLT/hang possible\r\n") ||
	    !x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
		X58_EARLY_UART_FLUSH_POLL_LIMIT))
		return X58_RAMINIT_AUTO_UART_ERROR;

	/* No fallible serial operation may separate this phase commit from d3. */
	if (!x58_raminit_set_i801_signature(&x58_minit_in_progress_signature))
		x58_init_phase_guard_stop("X58_MINIT_SIGNATURE_WRITE");
	if (x58_raminit_read_cmos_diagnostic() != X58_RAMINIT_CMOS_GUARD_IN_PROGRESS)
		x58_init_phase_guard_stop("X58_MINIT_CMOS_GUARD_LOST_BEFORE_MINIT");
	outb(POST_X58_VENDOR_ENTRY_MINIT_CALL, CONFIG_POST_IO_PORT);
	status = x58_vendor_call_minit(&minit_result);
	outb(POST_X58_VENDOR_ENTRY_MINIT_RETURN, CONFIG_POST_IO_PORT);

	/*
	 * Capture the host-register state immediately.  X58_WORKSPACE_STATE separately accepts
	 * only MINIT's measured return tuple.  Persistent CMOS 0xec still rejects
	 * reset continuation; X58_MEMORY_PROFILE defers rearm until all post-memory tests pass.
	 */
	x58_raminit_capture_i801_signature(&i801);
	post_minit_i801_exact = x58_raminit_i801_signature_matches(&i801,
		&x58_workspace_state_minit_return_signature);
	post_minit_cmos = x58_raminit_read_cmos_diagnostic();
	if (!x58_vendor_entry_emit_status("X58_MINIT_CALL", status) ||
	    !x58_vendor_entry_print_call_result("X58_MINIT", &minit_result) ||
	    !x58_romstage_uart_puts("[RAMINIT] POST_MINIT_I801_RAW=") ||
	    !x58_romstage_uart_put_hex(i801.control, 2) || !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(i801.command, 2) || !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(i801.xmit_address, 2) || !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(i801.data0, 2) || !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(i801.data1, 2) ||
	    !x58_romstage_uart_puts(" EXPECT_RETURN_08:00:5d:01:a3_MATCH=") ||
	    !x58_romstage_uart_put_hex(post_minit_i801_exact, 2) ||
	    !x58_romstage_uart_puts(" CMOS0E=") ||
	    !x58_romstage_uart_put_hex(post_minit_cmos, 2) ||
	    !x58_romstage_uart_puts(
		" AUTO_CLEAR=00 REARM_AFTER_FULL_POSTMEM=01 BROAD_UNSAFE=01\r\n"))
		return X58_RAMINIT_AUTO_UART_ERROR;

	final_csi_state = x58_vendor_csi_state_snapshot();
	policy = x58_vendor_minit_policy();
	workspace = x58_vendor_minit_workspace();
	if (csi_profile == X58_CSI_PROFILE_CSI_OBSERVATION
	    && csi_profile != X58_MEMORY_PROFILE_CSI_BROAD_HARD_GATED
	    && !x58_profile_policy_o_candidate
	    ) {
		(void)x58_vendor_runtime_probe(&info);
		if (workspace != NULL)
			workspace_fnv = x58_vendor_entry_buffer_digest(workspace,
				X58_VENDOR_MINIT_WORKSPACE_SIZE);
		x58_init_phase_capture_qpi_tuple(&post_minit_tuple);
		x58_minit_capture_stage_fingerprints(&post_minit_stage);
		if (!x58_vendor_entry_print_runtime(state) ||
		    !x58_romstage_uart_puts(
			"[RAMINIT] X58_CSI_PROFILE OBSERVATION RETURN POLICY_IN/NOW=") ||
		    !x58_romstage_uart_put_hex(info.minit_policy_digest, 8) ||
		    !x58_romstage_uart_putc('/') ||
		    !x58_romstage_uart_put_hex(info.minit_policy_current_digest, 8) ||
		    !x58_romstage_uart_puts(" WORK_FNV=") ||
		    !x58_romstage_uart_put_hex(workspace_fnv, 8) ||
		    !x58_romstage_uart_puts(" DRAM_ACCESSES=00\r\n") ||
		    !x58_init_phase_print_qpi_tuple("POST_MINIT", &post_minit_tuple) ||
		    !x58_minit_print_stage_fingerprints("POST_MINIT",
			&post_minit_stage, csi_result))
			return X58_RAMINIT_AUTO_UART_ERROR;

		full_path_observed = status == X58_VENDOR_OK &&
			workspace != NULL && policy != NULL &&
			final_csi_state != NULL && info.canaries_valid &&
			info.csi_result_accepted &&
			info.x58_minit_high_qpi_minit_authorized &&
			info.minit_policy_confirmed && info.minit_call_attempted &&
			info.minit_returned &&
			info.minit_policy_digest == X58_RAMINIT_EXPECTED_POLICY_FNV &&
			info.minit_policy_current_digest ==
				X58_RAMINIT_EXPECTED_POLICY_FNV &&
			minit_result.eax == 0 && workspace[1] == 0 &&
			workspace[2] == 0 &&
			workspace[X58_VENDOR_POLICY_WORK_B3_FLAGS_OFFSET] == 0x02 &&
			workspace[X58_VENDOR_POLICY_WORK_COMPLETE_OFFSET] == 0x01 &&
			policy[X58_VENDOR_POLICY_STATUS_OFFSET] == 0 &&
			x58_minit_result_is_self_consistent(&minit_result, &info);
		if (!x58_romstage_uart_puts(
			"[RAMINIT] X58_CSI_PROFILE MINIT_OBSERVATION RESULT=") ||
		    !x58_romstage_uart_puts(full_path_observed ?
			"FULL_PATH_RETURN_DRAM_UNTESTED" :
			"RETURNED_UNKNOWN_OR_INCOMPLETE_DRAM_UNTESTED") ||
		    !x58_romstage_uart_puts(
			"; terminal profile; no success gate/handoff\r\n") ||
		    !x58_minit_print_complete_buffer("CSI", final_csi_state,
			X58_VENDOR_CSI_STATE_SIZE) ||
		    !x58_minit_print_complete_buffer("WORK", workspace,
			X58_VENDOR_MINIT_WORKSPACE_SIZE) ||
		    !x58_romstage_uart_puts(
			"[RAMINIT] X58_CSI_PROFILE_OBSERVATION_TERMINAL; guard retained; no ordinary DRAM, postcar, ramstage or payload\r\n") ||
		    !x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
			X58_EARLY_UART_FLUSH_POLL_LIMIT))
			return X58_RAMINIT_AUTO_UART_ERROR;
		outb(POST_X58_INIT_PHASE_TERMINAL, CONFIG_POST_IO_PORT);

		return x58_raminit_fallback("X58_CSI_PROFILE_OBSERVATION_MINIT_TERMINAL");
	}
	if (!x58_csi_state_admit(!(csi_profile != X58_CSI_PROFILE_CSI_PRIMARY
	    && csi_profile != X58_MEMORY_PROFILE_CSI_BROAD_HARD_GATED
	    && !x58_profile_policy_o_candidate
	    ), "ROMSTAGE_CSI_PROFILE_PROMOTION"))
		return x58_raminit_fallback("X58_CSI_PROFILE_PRIMARY_PROFILE_REQUIRED");
	/* A0 may change during CSI. Retain/report that original transition above,
	 * but compare MINIT's result against its actual input, not pre-CSI A0.
	 * All post-MINIT endpoint/range/map checks still run unmodified.
	 */
	if (!x58_romstage_uart_puts("[CSI-POLICY] MINIT_A0_BASELINE=POST_CSI PRE_CSI=") ||
	    !x58_romstage_uart_put_hex(saved_pre_a0, 8) || !x58_romstage_uart_puts(" POST_CSI=") ||
	    !x58_romstage_uart_put_hex(csi_tuple->cpu_a0, 8) || !x58_romstage_uart_puts("\r\n"))
		return X58_RAMINIT_AUTO_UART_ERROR;
	return x58_memory_result_promote_post_minit(state, status, csi_result,
		&minit_result, final_csi_state, policy, workspace,
		post_minit_i801_exact, post_minit_cmos
		, csi_tuple->cpu_a0,
		csi_tuple
		);
	(void)x58_vendor_runtime_probe(&info);
	if (workspace != NULL)
		workspace_fnv = x58_vendor_entry_buffer_digest(workspace,
			X58_VENDOR_MINIT_WORKSPACE_SIZE);
	x58_init_phase_capture_qpi_tuple(&post_minit_tuple);
	x58_minit_capture_stage_fingerprints(&post_minit_stage);
	if (!x58_vendor_entry_print_runtime(state) ||
	    !x58_romstage_uart_puts("[RAMINIT] X58_MINIT RETURN POLICY_IN/NOW=") ||
	    !x58_romstage_uart_put_hex(info.minit_policy_digest, 8) ||
	    !x58_romstage_uart_putc('/') ||
	    !x58_romstage_uart_put_hex(info.minit_policy_current_digest, 8) ||
	    !x58_romstage_uart_puts(" WORK_FNV=") ||
	    !x58_romstage_uart_put_hex(workspace_fnv, 8) ||
	    !x58_romstage_uart_puts(" DRAM_ACCESSES=00\r\n") ||
	    !x58_init_phase_print_qpi_tuple("POST_MINIT", &post_minit_tuple) ||
	    !x58_minit_print_stage_fingerprints("POST_MINIT", &post_minit_stage,
		csi_result))
		return X58_RAMINIT_AUTO_UART_ERROR;

	full_path_observed = status == X58_VENDOR_OK &&
		workspace != NULL && policy != NULL && final_csi_state != NULL &&
		info.canaries_valid && info.csi_result_accepted &&
		info.x58_minit_high_qpi_minit_authorized &&
		info.minit_policy_confirmed && info.minit_call_attempted &&
		info.minit_returned &&
		info.minit_policy_digest == X58_RAMINIT_EXPECTED_POLICY_FNV &&
		info.minit_policy_current_digest == X58_RAMINIT_EXPECTED_POLICY_FNV &&
		minit_result.eax == 0 && workspace[1] == 0 && workspace[2] == 0 &&
		workspace[X58_VENDOR_POLICY_WORK_B3_FLAGS_OFFSET] == 0x02 &&
		workspace[X58_VENDOR_POLICY_WORK_COMPLETE_OFFSET] == 0x01 &&
		policy[X58_VENDOR_POLICY_STATUS_OFFSET] == 0 &&
		x58_minit_result_is_self_consistent(&minit_result, &info);
	if (!x58_romstage_uart_puts("[RAMINIT] X58_MINIT MINIT_OBSERVATION RESULT=") ||
	    !x58_romstage_uart_puts(full_path_observed ?
		"FULL_PATH_RETURN_DRAM_UNTESTED" :
		"RETURNED_UNKNOWN_OR_INCOMPLETE_DRAM_UNTESTED") ||
	    !x58_romstage_uart_puts("; observational only; no success gate/handoff\r\n") ||
	    !x58_minit_print_complete_buffer("CSI", final_csi_state,
		X58_VENDOR_CSI_STATE_SIZE) ||
	    !x58_minit_print_complete_buffer("WORK", workspace,
		X58_VENDOR_MINIT_WORKSPACE_SIZE) ||
	    !x58_romstage_uart_puts(
		"[RAMINIT] X58_MINIT_TERMINAL; guard retained; no ordinary DRAM, postcar or ramstage\r\n") ||
	    !x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
		X58_EARLY_UART_FLUSH_POLL_LIMIT))
		return X58_RAMINIT_AUTO_UART_ERROR;
	outb(POST_X58_INIT_PHASE_TERMINAL, CONFIG_POST_IO_PORT);

	return x58_raminit_fallback("X58_MINIT_OBSERVATION_TERMINAL");
}

static void __noreturn x58_init_phase_issue_outer_syre_reset(void)
{
	uint32_t syre = x58_rommon_pci_read(X58_INIT_PHASE_IOH_SYRE_DEV, 0xcc, 4);

	/*
	 * Exact MSI caller sequence at fffc1330..fffc137c; never use CF9 here.
	 * The immutable live trace entered with bit 10 set.  Every mutation is
	 * bounded by an exact last-moment readback; ambiguity is a terminal stop.
	 */
	if (syre != 0x00000600)
		x58_init_phase_guard_stop("X58_INIT_PHASE_SYRE_PRE_EDGE_GATE");
	x58_rommon_pci_write(X58_INIT_PHASE_IOH_SYRE_DEV, 0xcc,
		syre & ~X58_INIT_PHASE_IOH_SYRE_REQUEST, 4);
	syre = x58_rommon_pci_read(X58_INIT_PHASE_IOH_SYRE_DEV, 0xcc, 4);
	if (syre != 0x00000200)
		x58_init_phase_guard_stop("X58_INIT_PHASE_SYRE_CLEAR_READBACK");
	x58_rommon_pci_write(X58_INIT_PHASE_IOH_SR_DEV, 0x7c, 0, 4);
	x58_rommon_pci_write(X58_INIT_PHASE_IOH_SR_DEV, 0x80, 0, 4);
	if (x58_rommon_pci_read(X58_INIT_PHASE_IOH_SR_DEV, 0x7c, 4) != 0 ||
	    x58_rommon_pci_read(X58_INIT_PHASE_IOH_SR_DEV, 0x80, 4) != 0)
		x58_init_phase_guard_stop("X58_INIT_PHASE_SR_CLEAR_READBACK");
	syre = x58_rommon_pci_read(X58_INIT_PHASE_IOH_SYRE_DEV, 0xcc, 4);
	if (syre != 0x00000200)
		x58_init_phase_guard_stop("X58_INIT_PHASE_SYRE_FINAL_PRE_EDGE_GATE");
	outb(POST_X58_INIT_PHASE_OUTER_RESET_ARMED, CONFIG_POST_IO_PORT);
	outb(POST_X58_INIT_PHASE_IOH_SYRE, CONFIG_POST_IO_PORT);
	x58_rommon_pci_write(X58_INIT_PHASE_IOH_SYRE_DEV, 0xcc,
		syre | X58_INIT_PHASE_IOH_SYRE_REQUEST, 4);

	/* A successful request resets the CPU; never mutate persistence afterward. */
	asm volatile("cli" ::: "memory");
	for (;;)
		asm volatile("hlt");
}

static enum x58_raminit_auto_result x58_init_phase_high_qpi_probe(
	struct x58_rommon_state *state, enum x58_vendor_smbus_entry_state smbus_entry,
	const struct x58_raminit_i801_signature *entry_signature
	, const struct x58_rtc_policy_rtc_upper_bank_state *rtc_state
	)
{
	struct x58_vendor_call_result result = { 0 };
	struct x58_vendor_runtime_info info = { 0 };
	struct x58_init_phase_qpi_tuple tuple;
	const struct x58_raminit_i801_signature *next_signature;
	const uint8_t *csi_state;
	enum x58_vendor_status call_status;
	enum x58_vendor_status probe_status;
	enum x58_init_phase phase;
	uint8_t cmos_guard = x58_raminit_read_cmos_diagnostic();
	uint32_t csi_raw_fnv = 0;
	bool completed_warm_restart = false;
	uint32_t saved_pre_a0 = 0;

	outb(POST_X58_RAMINIT_AUTO_BEGIN, CONFIG_POST_IO_PORT);
	if (!x58_romstage_uart_puts(
		"[QPI] vendor-assisted seven-A0 admission; "
		"opaque workspace telemetry; local hash-pinned modules; one-DIMM profile\r\n") ||
	    !x58_romstage_uart_puts("[QPI] ENTRY=") ||
	    !x58_romstage_uart_puts(smbus_entry == X58_VENDOR_SMBUS_ENTRY_COLD ?
		"COLD_DEFAULT" : "CONFIGURED_AFTER_RESET") ||
	    !x58_romstage_uart_puts(" I801_SIG=") ||
	    !x58_romstage_uart_put_hex(entry_signature->control, 2) ||
	    !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(entry_signature->command, 2) ||
	    !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(entry_signature->xmit_address, 2) ||
	    !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(entry_signature->data0, 2) ||
	    !x58_romstage_uart_putc(':') ||
	    !x58_romstage_uart_put_hex(entry_signature->data1, 2) ||
	    !x58_romstage_uart_puts(" CMOS0E=") || !x58_romstage_uart_put_hex(cmos_guard, 2) ||
	    !x58_romstage_uart_puts("\r\n") ||
	    !x58_vendor_entry_emit_status("CAR_PREPARE", state->vendor_prepare_status))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (!state->vendor_runtime_ready)
		return x58_raminit_fallback("X58_INIT_PHASE_CAR_PREPARE");
	if (rtc_state == NULL || !rtc_state->enabled)
		return x58_raminit_fallback("X58_RTC_POLICY_RTC_UPPER_BANK_GATE");

	if (smbus_entry == X58_VENDOR_SMBUS_ENTRY_COLD) {
		if (x58_init_phase_i801_signature_is_incomplete_phase(entry_signature))
			return x58_raminit_fallback("X58_INIT_PHASE_COLD_RETAINED_PHASE_SIGNATURE");
		/* A genuine cold controller state is the admission signal.  CMOS 0e
		 * is retained battery-backed history and is not a boot permission bit.
		 * Accept only reset-zero or our completed clear tuple; arbitrary host
		 * register contents still indicate an unknown/inconsistent entry.
		 */
		if (!x58_raminit_i801_signature_is_zero(entry_signature) &&
		    !x58_raminit_i801_signature_matches(entry_signature,
			&x58_raminit_clear_signature))
			return x58_raminit_fallback("X58_INIT_PHASE_COLD_UNKNOWN_I801_SIGNATURE");
		if (cmos_guard == X58_INIT_PHASE_CMOS_COLD_AUTHORIZATION) {
			if (!x58_romstage_uart_puts("[QPI] RESET_CLASS=ICH_RESET_WITH_COMPLETED_MARKER; "
				"I801_VOLATILE_STATE_LOST; LIVE_QPI_IMC_VALIDATION_REQUIRED\r\n"))
				return X58_RAMINIT_AUTO_UART_ERROR;
			completed_warm_restart = true;
		} else if (!x58_romstage_uart_puts("[QPI] COLD_ADMISSION=CONTROLLER_RESET_STATE; CMOS0E=") ||
			   !x58_romstage_uart_put_hex(cmos_guard, 2) ||
			   !x58_romstage_uart_puts(" COOKIE=TELEMETRY_ONLY; incomplete phase markers remain fatal\r\n")) {
			return X58_RAMINIT_AUTO_UART_ERROR;
		}
		phase = X58_INIT_PHASE_PASS1;
	} else {
		if (cmos_guard == X58_INIT_PHASE_CMOS_COLD_AUTHORIZATION &&
		    x58_raminit_i801_signature_matches(entry_signature,
			&x58_raminit_clear_signature)) {
			if (!x58_romstage_uart_puts("[QPI] RESET_CLASS=COMPLETED_WARM_RESTART; "
				"previous handoff finalized cleanly; cold-policy MINIT is not a warm retrain path\r\n"))
				return X58_RAMINIT_AUTO_UART_ERROR;
			completed_warm_restart = true;
			phase = X58_INIT_PHASE_PASS1; /* not consumed by the reuse path */
		} else {
			if (cmos_guard != X58_RAMINIT_CMOS_GUARD_IN_PROGRESS)
				return x58_raminit_fallback("X58_INIT_PHASE_SIGNATURE_WITHOUT_CMOS_GUARD");
			if (x58_raminit_i801_signature_matches(entry_signature,
				&x58_init_phase_pass2_signature))
				phase = X58_INIT_PHASE_PASS2;
			else if (x58_raminit_i801_signature_matches(entry_signature,
				&x58_init_phase_pass3_signature))
				phase = X58_INIT_PHASE_PASS3;
			else if (x58_raminit_i801_signature_matches(entry_signature,
				&x58_init_phase_consumed_signature))
				return x58_raminit_fallback("X58_INIT_PHASE_ONE_SHOT_CONSUMED");
			else if (x58_raminit_i801_signature_matches(entry_signature,
				&x58_minit_in_progress_signature))
				return x58_raminit_fallback("X58_MINIT_RESET_LOOP_GUARD");
			else
				return x58_raminit_fallback("X58_INIT_PHASE_UNKNOWN_PHASE_SIGNATURE");
		}
	}

	if (!completed_warm_restart) {
		if (!x58_romstage_uart_puts("[QPI] PHASE=") ||
		    !x58_romstage_uart_put_hex(phase, 2) || !x58_romstage_uart_puts("\r\n"))
			return X58_RAMINIT_AUTO_UART_ERROR;
		if (phase == X58_INIT_PHASE_PASS3) {
			if (!x58_csi_profile_capture_stable_pre_a0(&saved_pre_a0))
				return x58_raminit_fallback("X58_CSI_PROFILE_PRE_A0_STABILITY_GATE");
			call_status = x58_vendor_init_phase_select_high_qpi_csi(
				saved_pre_a0,
				X58_VENDOR_WRITE_CONFIRMATION);
			if (!x58_vendor_entry_emit_status(
				"X58_CSI_PROFILE_COUPLED_HIGH_CSI_PROFILE",
				call_status))
				return X58_RAMINIT_AUTO_UART_ERROR;
			if (call_status != X58_VENDOR_OK)
				return x58_raminit_fallback(
					"X58_CSI_PROFILE_COUPLED_HIGH_CSI_PROFILE_GATE"
					);
			outb(POST_X58_VENDOR_TRACE_HIGH_PROFILE_READY, CONFIG_POST_IO_PORT);
		}
	}
	if (!x58_rommon_run_spd(state))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (!state->vendor_spd_valid ||
	    !x58_raminit_spd_is_exact(state) ||
	    !x58_vendor_entry_spd_topology_is_supported(state))
		return x58_raminit_fallback("X58_INIT_PHASE_SPD_EXACT_GATE");

	if (completed_warm_restart) {
		const enum x58_raminit_auto_result warm_result =
			x58_init_phase_promote_completed_warm_restart(state,
				smbus_entry == X58_VENDOR_SMBUS_ENTRY_COLD);

		if (warm_result != X58_RAMINIT_AUTO_COLD_CONTINUE)
			return warm_result;
		completed_warm_restart = false;
		phase = X58_INIT_PHASE_PASS1;
		if (!x58_romstage_uart_puts("[QPI] PHASE=01; retained completion marker "
			"did not describe a live trained endpoint\r\n"))
			return X58_RAMINIT_AUTO_UART_ERROR;
	}

	if (!x58_vendor_entry_prepare_pciexbar(state))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (!state->vendor_pciexbar_ready)
		return x58_raminit_fallback("X58_INIT_PHASE_PCIEXBAR_PLATFORM_GATE");
	if (x58_rommon_mem_read(0xe0000008u, 4) != 0x06000013u)
		return x58_raminit_fallback("X58_INIT_PHASE_REVISION_GATE");
	if (!x58_romstage_uart_puts(
		"[QPI] X58_CSI_PROFILE=STATE06/07:01/01 PAIRS:08/03e3d24e|0c/8b38506a; "
		"RTC_INPUT_DEPENDENCY=NONE U128E=PASS\r\n"))
		return X58_RAMINIT_AUTO_UART_ERROR;
	outb(POST_X58_RTC_POLICY_PROFILE_READY, CONFIG_POST_IO_PORT);
	outb(POST_X58_RAMINIT_PLATFORM_READY, CONFIG_POST_IO_PORT);
	x58_init_phase_capture_qpi_tuple(&tuple);
	if (!x58_init_phase_print_qpi_tuple("ENTRY", &tuple))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (phase == X58_INIT_PHASE_PASS3 && tuple.cpu_a0 != saved_pre_a0)
		return x58_raminit_fallback("X58_CSI_PROFILE_PRE_CSI_A0_DRIFT");
	if (phase == X58_INIT_PHASE_PASS3 && !x58_init_phase_high_qpi_tuple_exact(&tuple))
		return x58_raminit_fallback("X58_INIT_PHASE_HIGH_QPI_ENTRY_GATE");
	if (phase != X58_INIT_PHASE_PASS3 &&
	    !x58_init_phase_slow_qpi_entry_tuple_exact(&tuple, phase))
		return x58_raminit_fallback("X58_INIT_PHASE_SLOW_QPI_ENTRY_GATE");

	call_status = x58_vendor_arm_csi_wrapper(0,
		X58_VENDOR_WRITE_CONFIRMATION);
	if (!x58_vendor_entry_emit_status("X58_INIT_PHASE_CSI_ARM", call_status))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (call_status != X58_VENDOR_OK)
		return x58_raminit_fallback("X58_INIT_PHASE_CSI_ARM");

	next_signature = phase == X58_INIT_PHASE_PASS1 ?
		&x58_init_phase_pass2_signature : &x58_init_phase_consumed_signature;
	if (!x58_romstage_uart_puts(phase == X58_INIT_PHASE_PASS1 ?
		"[QPI] CSI_PASS=1; expecting CSI-internal SYRE reset; no caller reset\r\n" :
		(phase == X58_INIT_PHASE_PASS2 ?
		 "[QPI] CSI_PASS=2; one-shot consumed before call; exact return required\r\n" :
		 "[QPI] CSI_PASS=3; one-shot consumed; return tuple intentionally unconstrained\r\n")) ||
	    !x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
		X58_EARLY_UART_FLUSH_POLL_LIMIT))
		return X58_RAMINIT_AUTO_UART_ERROR;
	/* No fallible return may separate persistent phase commit from the call. */
	if (!x58_raminit_write_cmos_diagnostic(X58_RAMINIT_CMOS_GUARD_IN_PROGRESS))
		x58_init_phase_guard_stop("X58_INIT_PHASE_CMOS_GUARD_WRITE");
	if (!x58_raminit_set_i801_signature(next_signature))
		x58_init_phase_guard_stop("X58_INIT_PHASE_I801_PHASE_WRITE");
	outb(phase == X58_INIT_PHASE_PASS1 ? POST_X58_RAMINIT_CSI_PASS1_ARMED :
		(phase == X58_INIT_PHASE_PASS2 ? POST_X58_RAMINIT_CSI_PASS2_ARMED :
		 POST_X58_INIT_PHASE_CSI_PASS3_ARMED), CONFIG_POST_IO_PORT);
	outb(POST_X58_VENDOR_ENTRY_CSI_CALL, CONFIG_POST_IO_PORT);
	call_status = x58_vendor_call_csi_wrapper(&result);
	outb(phase == X58_INIT_PHASE_PASS3 ? POST_X58_INIT_PHASE_CSI_PASS3_RETURN :
		POST_X58_VENDOR_ENTRY_CSI_RETURN, CONFIG_POST_IO_PORT);
	state->vendor_policy_ready = false;
	state->vendor_policy_modified = false;

	/* A pass-one return must first consume the retained pass-two signature. */
	if (phase == X58_INIT_PHASE_PASS1 &&
	    !x58_raminit_set_i801_signature(&x58_init_phase_consumed_signature))
		x58_init_phase_guard_stop("X58_INIT_PHASE_PASS1_RETURN_CONSUME");
	if (x58_raminit_read_cmos_diagnostic() != X58_RAMINIT_CMOS_GUARD_IN_PROGRESS)
		x58_init_phase_guard_stop("X58_INIT_PHASE_CMOS_GUARD_LOST_AFTER_CSI");
	if (!x58_vendor_entry_emit_status("X58_INIT_PHASE_CSI_CALL", call_status) ||
	    !x58_vendor_entry_print_call_result("X58_INIT_PHASE_CSI", &result))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (phase == X58_INIT_PHASE_PASS1)
		return x58_raminit_fallback("X58_INIT_PHASE_CSI_PASS1_RETURNED");

	probe_status = x58_vendor_runtime_probe(&info);
	csi_state = x58_vendor_csi_state_snapshot();
	if (!x58_vendor_entry_emit_status("X58_INIT_PHASE_CSI_PROBE", probe_status))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (csi_state != NULL)
		csi_raw_fnv = x58_vendor_entry_buffer_digest(csi_state,
			X58_VENDOR_CSI_STATE_SIZE);
	x58_init_phase_capture_qpi_tuple(&tuple);
	if (!x58_romstage_uart_puts("[QPI] CSI_RAW_FNV=") ||
	    !x58_romstage_uart_put_hex(csi_raw_fnv, 8) ||
	    !x58_romstage_uart_puts(" STATE06/07/2a6/2ef/301=") ||
	    !(csi_state != NULL ?
		(x58_romstage_uart_put_hex(csi_state[0x06], 2) && x58_romstage_uart_putc('/') &&
		 x58_romstage_uart_put_hex(csi_state[0x07], 2) && x58_romstage_uart_putc('/') &&
		 x58_romstage_uart_put_hex(csi_state[0x2a6], 2) && x58_romstage_uart_putc('/') &&
		 x58_romstage_uart_put_hex(csi_state[0x2ef], 2) && x58_romstage_uart_putc('/') &&
		 x58_romstage_uart_put_hex(csi_state[0x301], 2)) :
		x58_romstage_uart_puts("NA")) ||
	    !x58_romstage_uart_puts("\r\n") ||
	    !x58_romstage_uart_puts("[QPI] CSI STATE1c/1d 70/71 cd/ce 121/122=") ||
	    !(csi_state != NULL ?
		(x58_romstage_uart_put_hex(csi_state[0x1c], 2) && x58_romstage_uart_putc('/') &&
		 x58_romstage_uart_put_hex(csi_state[0x1d], 2) && x58_romstage_uart_putc(' ') &&
		 x58_romstage_uart_put_hex(csi_state[0x70], 2) && x58_romstage_uart_putc('/') &&
		 x58_romstage_uart_put_hex(csi_state[0x71], 2) && x58_romstage_uart_putc(' ') &&
		 x58_romstage_uart_put_hex(csi_state[0xcd], 2) && x58_romstage_uart_putc('/') &&
		 x58_romstage_uart_put_hex(csi_state[0xce], 2) && x58_romstage_uart_putc(' ') &&
		 x58_romstage_uart_put_hex(csi_state[0x121], 2) && x58_romstage_uart_putc('/') &&
		 x58_romstage_uart_put_hex(csi_state[0x122], 2)) :
		x58_romstage_uart_puts("NA")) ||
	    !x58_romstage_uart_puts("\r\n") ||
	    !x58_init_phase_print_qpi_tuple(phase == X58_INIT_PHASE_PASS2 ?
		"PASS2_RETURN" : "PASS3_RETURN", &tuple))
		return X58_RAMINIT_AUTO_UART_ERROR;

	if (phase == X58_INIT_PHASE_PASS2) {
		if (call_status != X58_VENDOR_OK ||
		    probe_status != X58_VENDOR_OK || csi_state == NULL ||
		    !info.prepared || !info.canaries_valid ||
		    !info.wrapper_signature_valid || !info.csi_signature_valid ||
		    !info.csi_call_attempted || !info.csi_returned ||
		    info.csi_result_accepted || info.vendor_status_seed != 0 ||
		    result.eax != 1 || result.ebx != 0 || result.ecx != 0x2a6 ||
		    info.csi_state_digest != csi_raw_fnv ||
		    csi_state[0x06] != 1 || csi_state[0x07] != 1 ||
		    csi_state[0x1c] != 0 || csi_state[0x1d] != 1 ||
		    csi_state[0x70] != 0 || csi_state[0x71] != 1 ||
		    csi_state[0xcd] != 0 || csi_state[0xce] != 1 ||
		    csi_state[0x121] != 0 || csi_state[0x122] != 1 ||
		    csi_state[0x2ef] != 1 || csi_state[0x301] != 0 ||
		    !x58_init_phase_result_is_self_consistent(&result, &info))
			return x58_raminit_fallback("X58_INIT_PHASE_CSI_PASS2_ABI_GATE");
		if (!x58_init_phase_pre_outer_reset_tuple_exact(&tuple))
			return x58_raminit_fallback("X58_INIT_PHASE_PRE_OUTER_RESET_TUPLE_GATE");
		outb(POST_X58_INIT_PHASE_PASS2_ACCEPTED, CONFIG_POST_IO_PORT);
		if (!x58_romstage_uart_puts(
			"[QPI] PASS2_ACCEPTED EAX/EBX/ECX=1/0/2a6; S3 commit follows; issuing exact IOH SYRE edge once\r\n") ||
		    !x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
			X58_EARLY_UART_FLUSH_POLL_LIMIT))
			return X58_RAMINIT_AUTO_UART_ERROR;
		/* Commit S3 only after all serial work, immediately before SYRE. */
		if (!x58_raminit_set_i801_signature(&x58_init_phase_pass3_signature))
			x58_init_phase_guard_stop("X58_INIT_PHASE_PASS3_SIGNATURE_WRITE");
		x58_init_phase_issue_outer_syre_reset();
	}

	/*
	 * Pass-three output is observational: no return-register value, raw state
	 * digest, or resulting endpoint tuple is promoted to a success meaning.
	 */
	if (!x58_romstage_uart_puts("[QPI] PASS3_RETURNED CALL_STATUS=") ||
	    !x58_romstage_uart_puts(x58_vendor_status_name(call_status)) ||
	    !x58_romstage_uart_puts(" PROBE_STATUS=") ||
	    !x58_romstage_uart_puts(x58_vendor_status_name(probe_status)) ||
	    !x58_romstage_uart_puts(" CANARIES=") ||
	    !x58_romstage_uart_put_hex(info.canaries_valid, 2) ||
	    !x58_romstage_uart_puts(" RESULT_SELF_CONSISTENT=") ||
	    !x58_romstage_uart_put_hex(x58_init_phase_result_is_self_consistent(&result, &info), 2) ||
	    !x58_romstage_uart_puts("\r\n"))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (csi_state != NULL) {
		if (!x58_init_phase_print_csi_state(csi_state))
			return X58_RAMINIT_AUTO_UART_ERROR;
	} else if (!x58_romstage_uart_puts(
		"[QPI] CSI_SNAPSHOT=UNAVAILABLE; call/runtime validation did not retain it\r\n")) {
		return X58_RAMINIT_AUTO_UART_ERROR;
	}
	return x58_minit_high_qpi_minit_observe(state, &result, &info, csi_state,
		csi_raw_fnv, &tuple, call_status, probe_status
		, saved_pre_a0
		);
}

static enum x58_raminit_auto_result __maybe_unused x58_raminit_auto_handoff(
	struct x58_rommon_state *state, enum x58_vendor_smbus_entry_state smbus_entry,
	const struct x58_raminit_i801_signature *entry_signature)
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
	uint32_t csi_raw_fnv;
	uint32_t csi_canonical_fnv;
	uint32_t final_csi_raw_fnv;
	uint32_t final_csi_canonical_fnv;
	uint32_t workspace_raw_fnv;
	uint32_t workspace_canonical_fnv;
	uint8_t csi_dynamic_byte;
	uint8_t final_csi_dynamic_byte;
	unsigned int index;

	outb(POST_X58_RAMINIT_AUTO_BEGIN, CONFIG_POST_IO_PORT);
	cmos_guard = x58_raminit_read_cmos_diagnostic();
	if (!x58_romstage_uart_puts(
		"[RAMINIT] legacy Slow-QPI helper path; local vendor-assisted policy\r\n") ||
	    !x58_romstage_uart_puts("[RAMINIT] ENTRY=") ||
	    !x58_romstage_uart_puts(smbus_entry == X58_VENDOR_SMBUS_ENTRY_COLD ?
		"COLD_DEFAULT" : "CONFIGURED_AFTER_RESET") ||
	    !x58_romstage_uart_puts(" I801_SIG=") ||
	    !(smbus_entry == X58_VENDOR_SMBUS_ENTRY_COLD ?
		x58_romstage_uart_puts("NA") :
		(x58_romstage_uart_put_hex(entry_signature->control, 2) &&
		 x58_romstage_uart_putc(':') &&
		 x58_romstage_uart_put_hex(entry_signature->command, 2) &&
		 x58_romstage_uart_putc(':') &&
		 x58_romstage_uart_put_hex(entry_signature->xmit_address, 2) &&
		 x58_romstage_uart_putc(':') &&
		 x58_romstage_uart_put_hex(entry_signature->data0, 2) &&
		 x58_romstage_uart_putc(':') &&
		 x58_romstage_uart_put_hex(entry_signature->data1, 2))) ||
	    !x58_romstage_uart_puts(" CMOS0E=") ||
	    !x58_romstage_uart_put_hex(cmos_guard, 2) ||
	    !x58_romstage_uart_puts("\r\n") ||
	    !x58_vendor_entry_emit_status("CAR_PREPARE", state->vendor_prepare_status))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (!state->vendor_runtime_ready)
		return x58_raminit_fallback("CAR_PREPARE");

	if (smbus_entry == X58_VENDOR_SMBUS_ENTRY_COLD) {
		if (cmos_guard == X58_RAMINIT_CMOS_GUARD_IN_PROGRESS)
			return x58_raminit_fallback("PERSISTENT_RESET_LOOP_GUARD");
		if (cmos_guard != X58_RAMINIT_CMOS_DIAGNOSTIC_OBSERVED)
			return x58_raminit_fallback("CMOS_DIAGNOSTIC_GATE");
		second_pass = false;
	} else if (x58_raminit_i801_signature_matches(entry_signature,
		&x58_raminit_first_pass_signature)) {
		if (cmos_guard != X58_RAMINIT_CMOS_GUARD_IN_PROGRESS)
			return x58_raminit_fallback("CSI_SIGNATURE_WITHOUT_CMOS_GUARD");
		second_pass = true;
	} else if (x58_raminit_i801_signature_matches(entry_signature,
		&x58_raminit_in_progress_signature)) {
		return x58_raminit_fallback("RESET_LOOP_GUARD");
	} else {
		return x58_raminit_fallback("CONFIGURED_WITHOUT_CSI_COOKIE");
	}

	if (!x58_rommon_run_spd(state))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (!state->vendor_spd_valid ||
	    !x58_raminit_spd_is_exact(state) ||
	    !x58_vendor_entry_spd_topology_is_supported(state))
		return x58_raminit_fallback("SPD_EXACT_GATE");

	if (!x58_vendor_entry_prepare_pciexbar(state))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (!state->vendor_pciexbar_ready)
		return x58_raminit_fallback("PCIEXBAR_PLATFORM_GATE");
	outb(POST_X58_RAMINIT_PLATFORM_READY, CONFIG_POST_IO_PORT);

	status = x58_vendor_arm_csi_wrapper(0,
		X58_VENDOR_WRITE_CONFIRMATION);
	if (!x58_vendor_entry_emit_status("AUTO_CSI_ARM", status))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK)
		return x58_raminit_fallback("CSI_ARM");

	if (!x58_romstage_uart_puts(second_pass ?
		"[RAMINIT] CSI_PASS=2; complemented guard blocks another automatic call\r\n" :
		"[RAMINIT] CSI_PASS=1; complete I801 signature must survive the expected reset\r\n") ||
	    !x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
		X58_EARLY_UART_FLUSH_POLL_LIMIT))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (!x58_raminit_write_cmos_diagnostic(X58_RAMINIT_CMOS_GUARD_IN_PROGRESS))
		x58_raminit_guard_stop("CMOS_GUARD_WRITE");
	if (!x58_raminit_set_i801_signature(second_pass ?
		&x58_raminit_in_progress_signature : &x58_raminit_first_pass_signature))
		x58_raminit_guard_stop("I801_SIGNATURE_WRITE");

	outb(second_pass ? POST_X58_RAMINIT_CSI_PASS2_ARMED :
		POST_X58_RAMINIT_CSI_PASS1_ARMED, CONFIG_POST_IO_PORT);
	outb(POST_X58_VENDOR_ENTRY_CSI_CALL, CONFIG_POST_IO_PORT);
	status = x58_vendor_call_csi_wrapper(&result);
	outb(POST_X58_VENDOR_ENTRY_CSI_RETURN, CONFIG_POST_IO_PORT);
	state->vendor_policy_ready = false;
	state->vendor_policy_modified = false;
	/* Consume pass 1 before any fallible UART work after an unexpected return. */
	if (!second_pass &&
	    !x58_raminit_set_i801_signature(&x58_raminit_in_progress_signature))
		x58_raminit_guard_stop("CSI_PASS1_RETURN_GUARD_WRITE");
	if (x58_raminit_read_cmos_diagnostic() != X58_RAMINIT_CMOS_GUARD_IN_PROGRESS)
		x58_raminit_guard_stop("CMOS_GUARD_LOST_AFTER_CSI");
	if (!x58_vendor_entry_emit_status("AUTO_CSI_CALL", status) ||
	    !x58_vendor_entry_print_call_result("AUTO_CSI", &result))
		return X58_RAMINIT_AUTO_UART_ERROR;

	/* A first-pass return is outside the only hardware-observed sequence. */
	if (!second_pass)
		return x58_raminit_fallback("CSI_PASS1_RETURNED");
	if (status != X58_VENDOR_OK)
		return x58_raminit_fallback("CSI_PASS2_CALL");

	status = x58_vendor_runtime_probe(&info);
	if (!x58_vendor_entry_emit_status("AUTO_CSI_PROBE", status))
		return X58_RAMINIT_AUTO_UART_ERROR;
	csi_state = x58_vendor_csi_state_snapshot();
	if (status != X58_VENDOR_OK || csi_state == NULL ||
	    !info.csi_returned || info.csi_result_accepted ||
	    info.last_call.eax != 2 || info.last_call.ebx != 2 ||
	    info.last_call.ecx != 0x106 || info.vendor_status_seed != 0)
		return x58_raminit_fallback("CSI_ABI_GATE");
	csi_raw_fnv = x58_vendor_entry_buffer_digest(csi_state,
		X58_VENDOR_CSI_STATE_SIZE);
	csi_canonical_fnv = x58_csi_state_csi_canonical_digest(csi_state);
	csi_dynamic_byte = csi_state[X58_CSI_STATE_CSI_DYNAMIC_OFFSET];
	if (!x58_romstage_uart_puts("[RAMINIT] CSI RAW_FNV=") ||
	    !x58_romstage_uart_put_hex(csi_raw_fnv, 8) ||
	    !x58_romstage_uart_puts(" BYTE2A6=") ||
	    !x58_romstage_uart_put_hex(csi_dynamic_byte, 2) ||
	    !x58_romstage_uart_puts(" CANON_FNV=") ||
	    !x58_romstage_uart_put_hex(csi_canonical_fnv, 8) ||
	    !x58_romstage_uart_puts("\r\n"))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (info.csi_state_digest != csi_raw_fnv ||
	    !x58_csi_state_csi_dynamic_byte_is_known(csi_dynamic_byte) ||
	    csi_canonical_fnv != X58_CSI_STATE_EXPECTED_CSI_CANONICAL_FNV)
		return x58_raminit_fallback("CSI_CANONICAL_GATE");

	status = x58_vendor_accept_csi_result(2, 2, 0x106,
		csi_raw_fnv,
		X58_VENDOR_WRITE_CONFIRMATION);
	if (!x58_vendor_entry_emit_status("AUTO_CSI_ACCEPT", status))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK)
		return x58_raminit_fallback("CSI_ACCEPT");
	status = x58_vendor_runtime_probe(&info);
	csi_state = x58_vendor_csi_state_snapshot();
	if (status != X58_VENDOR_OK || csi_state == NULL ||
	    !info.csi_result_accepted ||
	    info.csi_state_digest != csi_raw_fnv ||
	    x58_vendor_entry_buffer_digest(csi_state, X58_VENDOR_CSI_STATE_SIZE) !=
		csi_raw_fnv ||
	    csi_state[X58_CSI_STATE_CSI_DYNAMIC_OFFSET] != csi_dynamic_byte ||
	    x58_csi_state_csi_canonical_digest(csi_state) != csi_canonical_fnv ||
	    csi_canonical_fnv != X58_CSI_STATE_EXPECTED_CSI_CANONICAL_FNV)
		return x58_raminit_fallback("CSI_ACCEPT_READBACK");
	outb(POST_X58_RAMINIT_CSI_ACCEPTED, CONFIG_POST_IO_PORT);

	if (!x58_vendor_entry_policy_reset(state))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (!state->vendor_policy_ready || state->vendor_policy_modified ||
	    x58_vendor_entry_policy_digest(state->vendor_policy) != X58_RAMINIT_POLICY_BASE_FNV)
		return x58_raminit_fallback("POLICY_BASE_GATE");
	for (index = 0; index < ARRAY_SIZE(x58_raminit_candidate_edits); index++)
		state->vendor_policy[x58_raminit_candidate_edits[index].offset] =
			x58_raminit_candidate_edits[index].value;
	state->vendor_policy_modified = true;
	if (x58_vendor_entry_policy_digest(state->vendor_policy) !=
		X58_VENDOR_POLICY_CMOS_CANDIDATE_FNV ||
	    state->vendor_policy[X58_VENDOR_POLICY_STATUS_OFFSET] != 2 ||
	    x58_vendor_entry_policy_read32(state->vendor_policy,
		X58_VENDOR_POLICY_FLAGS_OFFSET) !=
		X58_VENDOR_POLICY_CMOS_CANDIDATE_FLAGS)
		return x58_raminit_fallback("POLICY_CANDIDATE_GATE");

	if (!x58_vendor_policy_force_cold(state))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (!state->vendor_policy_ready || !state->vendor_policy_modified ||
	    x58_vendor_entry_policy_digest(state->vendor_policy) != X58_VENDOR_POLICY_COLD_FNV ||
	    state->vendor_policy[X58_VENDOR_POLICY_STATUS_OFFSET] != 0 ||
	    x58_vendor_entry_policy_read32(state->vendor_policy,
		X58_VENDOR_POLICY_FLAGS_OFFSET) != X58_RAMINIT_POLICY_COLD_FLAGS)
		return x58_raminit_fallback("POLICY_COLD_GATE");

	status = x58_vendor_install_confirmed_minit_policy(
		state->vendor_policy, sizeof(state->vendor_policy),
		X58_RAMINIT_EXPECTED_POLICY_FNV,
		X58_VENDOR_WRITE_CONFIRMATION);
	if (!x58_vendor_entry_emit_status("AUTO_POLICY_INSTALL", status))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK)
		return x58_raminit_fallback("POLICY_INSTALL");
	status = x58_vendor_runtime_probe(&info);
	if (status != X58_VENDOR_OK || !info.minit_policy_confirmed ||
	    info.minit_policy_digest != X58_RAMINIT_EXPECTED_POLICY_FNV ||
	    info.minit_policy_current_digest != X58_RAMINIT_EXPECTED_POLICY_FNV)
		return x58_raminit_fallback("POLICY_INSTALL_READBACK");
	outb(POST_X58_RAMINIT_POLICY_INSTALLED, CONFIG_POST_IO_PORT);

	status = x58_vendor_arm_minit(X58_VENDOR_WRITE_CONFIRMATION);
	if (!x58_vendor_entry_emit_status("AUTO_MINIT_ARM", status))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK)
		return x58_raminit_fallback("MINIT_ARM");
	if (!x58_romstage_uart_puts(
		"[RAMINIT] calling exact cold MINIT; reset, HLT or hang remains possible\r\n") ||
	    !x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
		X58_EARLY_UART_FLUSH_POLL_LIMIT))
		return X58_RAMINIT_AUTO_UART_ERROR;
	/* Keep the loop guard armed until every returned MINIT gate has passed. */
	if (!x58_raminit_write_cmos_diagnostic(X58_RAMINIT_CMOS_GUARD_IN_PROGRESS))
		x58_raminit_guard_stop("CMOS_MINIT_GUARD_WRITE");
	if (!x58_raminit_set_i801_signature(&x58_raminit_in_progress_signature))
		x58_raminit_guard_stop("I801_MINIT_GUARD_WRITE");
	outb(POST_X58_VENDOR_ENTRY_MINIT_CALL, CONFIG_POST_IO_PORT);
	status = x58_vendor_call_minit(&result);
	outb(POST_X58_VENDOR_ENTRY_MINIT_RETURN, CONFIG_POST_IO_PORT);
	if (x58_raminit_read_cmos_diagnostic() != X58_RAMINIT_CMOS_GUARD_IN_PROGRESS)
		x58_raminit_guard_stop("CMOS_GUARD_LOST_AFTER_MINIT");
	if (!x58_vendor_entry_emit_status("AUTO_MINIT_CALL", status) ||
	    !x58_vendor_entry_print_call_result("AUTO_MINIT", &result))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK)
		return x58_raminit_fallback("MINIT_CALL");

	csi_state = x58_vendor_csi_state_snapshot();
	policy = x58_vendor_minit_policy();
	workspace = x58_vendor_minit_workspace();
	status = x58_vendor_runtime_probe(&info);
	if (!x58_vendor_entry_emit_status("AUTO_MINIT_PROBE", status))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (status != X58_VENDOR_OK || csi_state == NULL || policy == NULL ||
	    workspace == NULL || !info.prepared || !info.canaries_valid ||
	    !info.wrapper_signature_valid || !info.csi_signature_valid ||
	    !info.minit_signature_valid || info.csi_wrapper_armed ||
	    !info.csi_call_attempted || !info.csi_returned ||
	    !info.csi_result_accepted || !info.minit_policy_confirmed ||
	    info.minit_armed || !info.minit_call_attempted ||
	    !info.minit_returned || info.vendor_status_seed != 0 ||
	    result.eax != 0 ||
	    workspace[1] != 0 || workspace[2] != 0 ||
	    workspace[X58_VENDOR_POLICY_WORK_B3_FLAGS_OFFSET] != 0x02 ||
	    workspace[X58_VENDOR_POLICY_WORK_COMPLETE_OFFSET] != 0x01 ||
	    policy[X58_VENDOR_POLICY_STATUS_OFFSET] != 0)
		return x58_raminit_fallback("MINIT_RUNTIME_GATE");
	final_csi_raw_fnv = x58_vendor_entry_buffer_digest(csi_state,
		X58_VENDOR_CSI_STATE_SIZE);
	final_csi_canonical_fnv = x58_csi_state_csi_canonical_digest(csi_state);
	final_csi_dynamic_byte = csi_state[X58_CSI_STATE_CSI_DYNAMIC_OFFSET];
	workspace_raw_fnv = x58_vendor_entry_buffer_digest(workspace,
		X58_VENDOR_MINIT_WORKSPACE_SIZE);
	workspace_canonical_fnv = x58_csi_state_workspace_canonical_digest(workspace);
	if (!x58_romstage_uart_puts("[RAMINIT] FINAL CSI_RAW=") ||
	    !x58_romstage_uart_put_hex(final_csi_raw_fnv, 8) ||
	    !x58_romstage_uart_puts(" CSI_BYTE2A6=") ||
	    !x58_romstage_uart_put_hex(final_csi_dynamic_byte, 2) ||
	    !x58_romstage_uart_puts(" CSI_CANON=") ||
	    !x58_romstage_uart_put_hex(final_csi_canonical_fnv, 8) ||
	    !x58_romstage_uart_puts(" WORK_RAW=") ||
	    !x58_romstage_uart_put_hex(workspace_raw_fnv, 8) ||
	    !x58_romstage_uart_puts(" WORK_CANON=") ||
	    !x58_romstage_uart_put_hex(workspace_canonical_fnv, 8) ||
	    !x58_romstage_uart_puts("\r\n"))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (info.csi_state_digest != final_csi_raw_fnv ||
	    final_csi_raw_fnv != csi_raw_fnv ||
	    final_csi_dynamic_byte != csi_dynamic_byte ||
	    !x58_csi_state_csi_dynamic_byte_is_known(final_csi_dynamic_byte) ||
	    final_csi_canonical_fnv != csi_canonical_fnv ||
	    final_csi_canonical_fnv !=
		X58_CSI_STATE_EXPECTED_CSI_CANONICAL_FNV ||
	    info.minit_policy_digest != X58_RAMINIT_EXPECTED_POLICY_FNV ||
	    info.minit_policy_current_digest !=
		X58_RAMINIT_EXPECTED_POLICY_FNV ||
	    x58_vendor_entry_policy_digest(policy) != X58_RAMINIT_EXPECTED_POLICY_FNV ||
	    info.minit_workspace_digest != workspace_raw_fnv ||
	    workspace_canonical_fnv !=
		X58_CSI_STATE_EXPECTED_WORKSPACE_CANONICAL_FNV)
		return x58_raminit_fallback("MINIT_CANONICAL_BUFFER_GATE");
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
		return x58_raminit_fallback("MINIT_CALL_STATE_GATE");

	mc_mapper = x58_rommon_pci_read(X58_VENDOR_POLICY_MC_COMMON_DEV,
		X58_VENDOR_POLICY_MC_CHANNEL_MAPPER, 4);
	mc_common_f8 = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_UNCORE_COMMON_DEV, 0xf8, 4);
	ch2_dod = x58_rommon_pci_read(X58_VENDOR_POLICY_CHANNEL2_ADDR_DEV,
		X58_VENDOR_POLICY_MC_DOD_DIMM0, 4);
	ch2_ranks = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV,
		X58_MEMORY_CONTROLLER_MC_RANK_PRESENT, 4);
	ch2_status = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV,
		X58_MEMORY_CONTROLLER_MC_INIT_STATUS, 4);
	qpi_status = x58_rommon_pci_read(X58_MEMORY_TRAINING_QPI_LINK0_PHY_DEV,
		X58_MEMORY_TRAINING_QPI_PH_PIS, 4);
	if (!x58_romstage_uart_puts(
		"[RAMINIT] ROBUST WORK_RAW=") ||
	    !x58_romstage_uart_put_hex(workspace_raw_fnv, 8) ||
	    !x58_romstage_uart_puts(" WORK_CANON=") ||
	    !x58_romstage_uart_put_hex(workspace_canonical_fnv, 8) ||
	    !x58_romstage_uart_puts(" MC_MAP60=") || !x58_romstage_uart_put_hex(mc_mapper, 8) ||
	    !x58_romstage_uart_puts(" MC_F8=") || !x58_romstage_uart_put_hex(mc_common_f8, 8) ||
	    !x58_romstage_uart_puts(" CH2_DOD=") || !x58_romstage_uart_put_hex(ch2_dod, 8) ||
	    !x58_romstage_uart_puts(" CH2_RANKS=") || !x58_romstage_uart_put_hex(ch2_ranks, 8) ||
	    !x58_romstage_uart_puts(" CH2_STATUS=") || !x58_romstage_uart_put_hex(ch2_status, 8) ||
	    !x58_romstage_uart_puts(" QPI80=") || !x58_romstage_uart_put_hex(qpi_status, 8) ||
	    !x58_romstage_uart_puts("\r\n"))
		return X58_RAMINIT_AUTO_UART_ERROR;
	if (info.cpuid_1_eax != X58_RAMINIT_EXPECTED_CPUID ||
	    info.microcode_revision != X58_RAMINIT_EXPECTED_UCODE ||
	    mc_mapper != X58_RAMINIT_EXPECTED_MC_MAPPER ||
	    mc_common_f8 != X58_RAMINIT_EXPECTED_MC_COMMON_F8 ||
	    ch2_dod != X58_RAMINIT_EXPECTED_CH2_DOD ||
	    ch2_ranks != X58_RAMINIT_EXPECTED_CH2_RANKS ||
	    ch2_status != X58_RAMINIT_EXPECTED_CH2_STATUS ||
	    qpi_status != X58_RAMINIT_EXPECTED_QPI_STATUS)
		return x58_raminit_fallback("MINIT_UNCORE_GATE");
	if (!x58_raminit_set_i801_signature(&x58_raminit_clear_signature))
		x58_raminit_guard_stop("I801_GUARD_CLEAR");
	if (!x58_raminit_write_cmos_diagnostic(X58_RAMINIT_CMOS_DIAGNOSTIC_OBSERVED))
		x58_raminit_guard_stop("CMOS_GUARD_CLEAR");
	{
		const struct x58_raminit_result handoff = {
			.cpuid = info.cpuid_1_eax,
			.microcode_revision = info.microcode_revision,
			.spd_fnv = state->vendor_spd_digest,
			.spd_profile_fnv = state->vendor_spd_profile_digest,
			.csi_state_fnv = info.csi_state_digest,
			.policy_fnv = info.minit_policy_digest,
			.workspace_fnv = info.minit_workspace_digest,
			.csi_state_canonical_fnv = final_csi_canonical_fnv,
			.workspace_canonical_fnv = workspace_canonical_fnv,
			.minit_eax = result.eax,
			.mc_mapper = mc_mapper,
			.mc_common_f8 = mc_common_f8,
			.ch2_dod = ch2_dod,
			.ch2_ranks = ch2_ranks,
			.ch2_status = ch2_status,
			.qpi_status = qpi_status,
		};

		x58_raminit_handoff_record_success(&handoff);
	}
	if (!x58_romstage_uart_puts(
		"[RAMINIT] AUTO_HANDOFF=READY; robust Slow-QPI canonical state; returning to coreboot\r\n") ||
	    !x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
		X58_EARLY_UART_FLUSH_POLL_LIMIT))
		return X58_RAMINIT_AUTO_UART_ERROR;
	outb(POST_X58_RAMINIT_MINIT_ACCEPTED, CONFIG_POST_IO_PORT);

	return X58_RAMINIT_AUTO_READY;
}

static bool x58_rommon_execute(struct x58_rommon_state *state, unsigned int argc,
			 char **argv)
{
	uint32_t a, b, c, d, e;
	unsigned int width;

	if (!argc)
		return true;
	outb(POST_X58_ROMMON_COMMAND, CONFIG_POST_IO_PORT);

	if (x58_rommon_streq(argv[0], "help") && argc == 1)
		return x58_rommon_help();
	if (x58_rommon_streq(argv[0], "id") && argc == 1)
		return x58_romstage_uart_puts(
			CONFIG_MAINBOARD_PART_NUMBER " CPUID=") &&
			x58_romstage_uart_put_hex(cpuid_eax(1), 8) &&
			x58_romstage_uart_puts(" UART=03f8 115200 8N1\r\n");
	if (x58_rommon_streq(argv[0], "unlock") && argc == 2) {
		if (x58_rommon_streq(argv[1], "WRITE")) {
			state->write_armed = true;
			state->script_armed = false;
			return x58_romstage_uart_puts(
				"ARMED for one write; verify target and width\r\n");
		}
		if (x58_rommon_streq(argv[1], "RESET")) {
			state->reset_armed = true;
			state->script_armed = false;
			return x58_romstage_uart_puts(
				"[RESET] ARMED for one reset command\r\n");
		}
		if (x58_rommon_streq(argv[1], "SCRIPT")) {
			state->write_armed = false;
			state->reset_armed = false;
			state->vendor_armed = false;
			state->script_armed = true;
			return x58_romstage_uart_puts(
				"[SCRIPT] ARMED for one run, rollback, or discard\r\n");
		}
		if (x58_rommon_streq(argv[1], "VENDOR")) {
			state->vendor_armed = true;
			state->script_armed = false;
			return x58_romstage_uart_puts(
				"[VENDOR] ARMED for one vendor operation\r\n");
		}
		return x58_rommon_error("use: unlock WRITE|RESET|VENDOR|SCRIPT");
	}
	if (x58_rommon_streq(argv[0], "lock") && argc == 1) {
		state->write_armed = false;
		state->reset_armed = false;
		state->script_armed = false;
		state->vendor_armed = false;
		return x58_romstage_uart_puts("LOCKED\r\n");
	}
	if (x58_rommon_streq(argv[0], "resetcause") && argc == 1)
		return x58_vendor_path_report_reset_registers();
	if (x58_rommon_streq(argv[0], "autoguard") && argc == 2 &&
	    x58_rommon_streq(argv[1], "clear")) {
		if (!x58_rommon_require_reset(state))
			return x58_rommon_error(
				"autoguard clear locked; use: unlock RESET");
		const uint8_t current_guard = x58_raminit_read_cmos_diagnostic();

		if (current_guard != X58_RAMINIT_CMOS_GUARD_IN_PROGRESS &&
		    current_guard != X58_INIT_PHASE_CMOS_PHASE_FAILED)
			return x58_rommon_error(
				"CMOS auto guard is neither 0xec nor X58_INIT_PHASE fail-lock 0xed");
		if (!x58_raminit_write_cmos_diagnostic(
			X58_INIT_PHASE_CMOS_COLD_AUTHORIZATION))
			return x58_rommon_error(
				"CMOS auto guard clear/readback failed");

		return x58_romstage_uart_puts(
			"[RAMINIT] persistent auto guard 0xec/0xed->0x2c cleared; "
			"remove AC power before retry\r\n");
	}
	if (x58_rommon_streq(argv[0], "reset") && argc == 2) {
		if (!x58_rommon_streq(argv[1], "init") &&
		    !x58_rommon_streq(argv[1], "warm") &&
		    !x58_rommon_streq(argv[1], "full"))
			return x58_rommon_error("use: reset init|warm|full");
		if (!x58_rommon_require_reset(state))
			return x58_rommon_error("reset locked; use: unlock RESET");
		if (x58_rommon_streq(argv[1], "init"))
			x58_vendor_path_cf9_reset(
				"[RESET] request=init CF9=00->04; no cache flush\r\n",
				POST_X58_VENDOR_PATH_RESET_INIT, 0x00, 0x04);
		if (x58_rommon_streq(argv[1], "warm"))
			x58_vendor_path_cf9_reset(
				"[RESET] request=warm CF9=02->06; no cache flush\r\n",
				POST_X58_VENDOR_PATH_RESET_WARM, 0x02, 0x06);
		x58_vendor_path_cf9_reset(
			"[RESET] request=full CF9=0a->0e; no cache flush\r\n",
			POST_X58_VENDOR_PATH_RESET_FULL, 0x0a, 0x0e);
	}
	if (x58_rommon_streq(argv[0], "script")) {
		if (argc == 2 && x58_rommon_streq(argv[1], "status"))
			return x58_vendor_script_print_status();
		if (argc == 2 && x58_rommon_streq(argv[1], "clear")) {
			const enum x58_rs_result result = x58_rs_clear();

			return x58_vendor_script_report_result("CLEAR", result) &&
				(result != X58_RS_OK || x58_vendor_script_print_status());
		}
		if (argc == 2 && x58_rommon_streq(argv[1], "seal")) {
			const enum x58_rs_result result = x58_rs_seal();

			return x58_vendor_script_report_result("SEAL", result) &&
				(result != X58_RS_OK || x58_vendor_script_print_status());
		}
		if (argc >= 2 && x58_rommon_streq(argv[1], "add"))
			return x58_vendor_script_add(argc, argv);
		if ((argc == 2 || argc == 4) && x58_rommon_streq(argv[1], "list"))
			return x58_vendor_script_list(false, argc, argv);
		if ((argc == 2 || argc == 4) && x58_rommon_streq(argv[1], "trace"))
			return x58_vendor_script_list(true, argc, argv);
		if (argc == 4 && x58_rommon_streq(argv[1], "run")) {
			enum x58_rs_result result;
			struct x58_rs_info info;
			bool auto_rollback;
			bool reported;

			if (!x58_rommon_parse_hex(argv[2], &a) ||
			    (!x58_rommon_streq(argv[3], "keep") &&
			     !x58_rommon_streq(argv[3], "auto")))
				return x58_rommon_error(
					"use: script run PROGRAM_FNV keep|auto");
			auto_rollback = x58_rommon_streq(argv[3], "auto");
			if (!x58_rommon_require_script(state))
				return x58_rommon_error(
					"script locked; use: unlock SCRIPT");
			if (!x58_romstage_uart_puts("[SCRIPT] RUN PROGRAM_FNV=") ||
			    !x58_romstage_uart_put_hex(a, 8) || !x58_romstage_uart_puts(" MODE=") ||
			    !x58_romstage_uart_puts(auto_rollback ? "auto" : "keep") ||
			    !x58_romstage_uart_puts(
				"; reset/hang possible; PRE is flushed before access\r\n") ||
			    !x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
				X58_EARLY_UART_FLUSH_POLL_LIMIT))
				return false;
			outb(POST_X58_VENDOR_SCRIPT_SCRIPT_RUN, CONFIG_POST_IO_PORT);
			result = x58_rs_run(a, auto_rollback, &x58_vendor_script_backend);
			x58_rs_get_info(&info);
			if (info.transaction_valid)
				x58_vendor_entry_mark_vendor_state_dirty(state);
			reported = x58_vendor_script_print_result("RUN", result) &&
				x58_vendor_script_print_status();
			outb(result == X58_RS_OK ? POST_X58_VENDOR_SCRIPT_SCRIPT_DONE :
				POST_X58_VENDOR_SCRIPT_SCRIPT_FAILED, CONFIG_POST_IO_PORT);
			return reported;
		}
		if (argc == 3 && x58_rommon_streq(argv[1], "rollback")) {
			enum x58_rs_result result;
			bool reported;

			if (!x58_rommon_parse_hex(argv[2], &a))
				return x58_rommon_error("use: script rollback TXN_FNV");
			if (!x58_rommon_require_script(state))
				return x58_rommon_error(
					"script locked; use: unlock SCRIPT");
			if (!x58_romstage_uart_puts(
				"[SCRIPT] ROLLBACK best-effort; hardware side effects remain possible\r\n") ||
			    !x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
				X58_EARLY_UART_FLUSH_POLL_LIMIT))
				return false;
			outb(POST_X58_VENDOR_SCRIPT_ROLLBACK_RUN, CONFIG_POST_IO_PORT);
			result = x58_rs_rollback(a, &x58_vendor_script_backend);
			reported = x58_vendor_script_print_result("ROLLBACK", result) &&
				x58_vendor_script_print_status();
			outb(result == X58_RS_OK ? POST_X58_VENDOR_SCRIPT_ROLLBACK_DONE :
				POST_X58_VENDOR_SCRIPT_ROLLBACK_FAILED, CONFIG_POST_IO_PORT);
			return reported;
		}
		if (argc == 3 && x58_rommon_streq(argv[1], "discard")) {
			enum x58_rs_result result;

			if (!x58_rommon_parse_hex(argv[2], &a))
				return x58_rommon_error("use: script discard TXN_FNV");
			if (!x58_rommon_require_script(state))
				return x58_rommon_error(
					"script locked; use: unlock SCRIPT");
			result = x58_rs_discard(a);
			return x58_vendor_script_report_result("DISCARD", result) &&
				(result != X58_RS_OK || x58_vendor_script_print_status());
		}
		return x58_rommon_error("unknown script command; use: help");
	}
	if (x58_rommon_streq(argv[0], "vinputs") && argc == 1) {
		struct x58_vendor_cmos_policy_inputs inputs;

		x58_vendor_cmos_capture_policy_inputs(&inputs);
		return x58_vendor_cmos_print_policy_inputs(&inputs);
	}
	if (x58_rommon_streq(argv[0], "vinfo") && argc == 1) {
		if (!x58_vendor_entry_print_runtime(state))
			return false;
		return x58_romstage_uart_puts("[VENDOR] ROMMON RUNTIME_BOOT=") &&
			x58_romstage_uart_puts(state->vendor_runtime_ready ? "PASS" : "FAIL") &&
			x58_romstage_uart_puts(" SPD_GATE=") &&
			x58_romstage_uart_puts(state->vendor_spd_valid ? "PASS" : "FAIL") &&
			x58_romstage_uart_puts(" ACKMAP=") &&
			x58_romstage_uart_put_hex(state->vendor_spd_ackmap, 2) &&
			x58_romstage_uart_puts(" DDR3MAP=") &&
			x58_romstage_uart_put_hex(state->vendor_spd_ddr3map, 2) &&
			x58_romstage_uart_puts(" SPD_FNV=") &&
			x58_romstage_uart_put_hex(state->vendor_spd_digest, 8) &&
			x58_romstage_uart_puts(" SPD_PROFILE_FNV=") &&
			x58_romstage_uart_put_hex(state->vendor_spd_profile_digest, 8) &&
			x58_romstage_uart_puts(" PCIEXBAR_STEP=") &&
			x58_romstage_uart_puts(state->vendor_pciexbar_ready ? "PASS" : "PENDING") &&
			x58_romstage_uart_puts(" POLICY_LOCAL=") &&
			x58_romstage_uart_puts(state->vendor_policy_ready ?
				(state->vendor_policy_modified ? "MODIFIED" : "GENERATED") :
				"ABSENT") && x58_romstage_uart_puts("\r\n");
	}
	if (x58_rommon_streq(argv[0], "vprep") || x58_rommon_streq(argv[0], "vcsi") ||
	    x58_rommon_streq(argv[0], "vaccept") ||
	    (x58_rommon_streq(argv[0], "vpolicy") &&
	     !(argc == 4 && x58_rommon_streq(argv[1], "dump"))) ||
	    x58_rommon_streq(argv[0], "vminit"))
		return x58_rommon_error(
			"X58_MEMORY_PROFILE broad/unsafe recovery ROMMON: manual vendor calls remain disabled");
	if (x58_rommon_streq(argv[0], "vprep") && argc == 2) {
		if (!x58_rommon_parse_hex(argv[1], &a) || !state->vendor_spd_valid ||
		    a != state->vendor_spd_digest)
			return x58_rommon_error("use after SPD review: vprep SPD_FNV");
		if (!x58_vendor_entry_vendor_state_clean(state))
			return x58_rommon_error(
				"generic write dirtied vendor state; cold reset required");
		if (!x58_rommon_require_vendor(state))
			return x58_rommon_error("vendor locked; use: unlock VENDOR");
		return x58_vendor_entry_prepare_pciexbar(state);
	}
	if (x58_rommon_streq(argv[0], "vcsi") && argc == 2) {
		struct x58_vendor_call_result result = { 0 };
		enum x58_vendor_status status;

		if (!x58_rommon_parse_hex(argv[1], &a) || a > 0xff)
			return x58_rommon_error("use: vcsi SEED_BYTE");
		if (!x58_vendor_entry_vendor_state_clean(state))
			return x58_rommon_error(
				"generic write dirtied vendor state; cold reset required");
		if (!x58_rommon_require_vendor(state))
			return x58_rommon_error("vendor locked; use: unlock VENDOR");
		if (!state->vendor_spd_valid || !state->vendor_pciexbar_ready)
			return x58_rommon_error("vcsi requires accepted SPD FNV and vprep");

		status = x58_vendor_arm_csi_wrapper(a,
			X58_VENDOR_WRITE_CONFIRMATION);
		if (!x58_vendor_entry_print_status("CSI_ARM", status))
			return false;
		if (status != X58_VENDOR_OK)
			return true;
		if (!x58_romstage_uart_puts(
			"[VENDOR] CALL CSI wrapper=fffc04e2 entry=fffe7000 UNKNOWN_SEED=") ||
		    !x58_romstage_uart_put_hex(a, 2) ||
		    !x58_romstage_uart_puts(
			"; may reset or HLT forever; NMI may remain disabled\r\n") ||
		    !x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
					 X58_EARLY_UART_FLUSH_POLL_LIMIT))
			return false;
		outb(POST_X58_VENDOR_ENTRY_CSI_CALL, CONFIG_POST_IO_PORT);
		status = x58_vendor_call_csi_wrapper(&result);
		outb(POST_X58_VENDOR_ENTRY_CSI_RETURN, CONFIG_POST_IO_PORT);
		state->vendor_policy_ready = false;
		state->vendor_policy_modified = false;
		if (!x58_vendor_entry_print_status("CSI_CALL", status) ||
		    !x58_vendor_entry_print_call_result("CSI", &result))
			return false;
		if (status != X58_VENDOR_OK)
			return x58_romstage_uart_puts(
				"[VENDOR] CSI call returned to ROMMON but failed runtime validation; state is not accept-able.\r\n") &&
				x58_vendor_entry_print_runtime(state);
		if (!x58_romstage_uart_puts(
			"[VENDOR] CSI returned; result is NOT accepted. Use vstate then vaccept exact registers and CSI_FNV.\r\n"))
			return false;
		return x58_vendor_entry_print_runtime(state);
	}
	if (x58_rommon_streq(argv[0], "vaccept") && argc == 5) {
		enum x58_vendor_status status;

		if (!x58_rommon_parse_hex(argv[1], &a) ||
		    !x58_rommon_parse_hex(argv[2], &b) ||
		    !x58_rommon_parse_hex(argv[3], &c) ||
		    !x58_rommon_parse_hex(argv[4], &d))
			return x58_rommon_error("use: vaccept EAX EBX ECX CSI_FNV");
		if (!x58_vendor_entry_vendor_state_clean(state))
			return x58_rommon_error(
				"generic write dirtied vendor state; cold reset required");
		if (!x58_rommon_require_vendor(state))
			return x58_rommon_error("vendor locked; use: unlock VENDOR");
		status = x58_vendor_accept_csi_result(a, b, c, d,
			X58_VENDOR_WRITE_CONFIRMATION);
		return x58_vendor_entry_print_status("CSI_ACCEPT", status);
	}
	if (x58_rommon_streq(argv[0], "vstate") && argc == 3) {
		if (!x58_rommon_parse_hex(argv[1], &a) ||
		    !x58_rommon_parse_hex(argv[2], &b))
			return x58_rommon_error("use: vstate OFF COUNT");
		return x58_vendor_entry_dump_buffer("CSI", x58_vendor_csi_state_snapshot(),
			X58_VENDOR_CSI_STATE_SIZE, a, b);
	}
	if (x58_rommon_streq(argv[0], "vwork") && argc == 3) {
		if (!x58_rommon_parse_hex(argv[1], &a) ||
		    !x58_rommon_parse_hex(argv[2], &b))
			return x58_rommon_error("use: vwork OFF COUNT");
		return x58_vendor_entry_dump_buffer("WORK", x58_vendor_minit_workspace(),
			X58_VENDOR_MINIT_WORKSPACE_SIZE, a, b);
	}
	if (x58_rommon_streq(argv[0], "vpolicy") && argc == 2 &&
	    x58_rommon_streq(argv[1], "reset")) {
		if (!x58_vendor_entry_vendor_state_clean(state))
			return x58_rommon_error(
				"generic write dirtied vendor state; cold reset required");
		if (!x58_rommon_require_vendor(state))
			return x58_rommon_error("vendor locked; use: unlock VENDOR");
		return x58_vendor_entry_policy_reset(state);
	}
	if (x58_rommon_streq(argv[0], "vpolicy") && argc == 4 &&
	    x58_rommon_streq(argv[1], "dump")) {
		if (!x58_rommon_parse_hex(argv[2], &a) ||
		    !x58_rommon_parse_hex(argv[3], &b) || !state->vendor_policy_ready)
			return x58_rommon_error("use after reset: vpolicy dump OFF COUNT");
		return x58_vendor_entry_dump_buffer("POLICY", state->vendor_policy,
			X58_VENDOR_MINIT_POLICY_SIZE, a, b);
	}
	if (x58_rommon_streq(argv[0], "vpolicy") && argc == 2 &&
	    x58_rommon_streq(argv[1], "cold")) {
		if (!x58_vendor_entry_vendor_state_clean(state))
			return x58_rommon_error(
				"generic write dirtied vendor state; cold reset required");
		if (!x58_rommon_require_vendor(state))
			return x58_rommon_error("vendor locked; use: unlock VENDOR");
		return x58_vendor_policy_force_cold(state);
	}
	if (x58_rommon_streq(argv[0], "vpolicy") && argc == 4 &&
	    x58_rommon_streq(argv[1], "set")) {
		enum x58_vendor_status status;

		if (!x58_rommon_parse_hex(argv[2], &a) || a >= sizeof(state->vendor_policy) ||
		    !x58_rommon_parse_hex(argv[3], &b) || b > 0xff ||
		    !state->vendor_policy_ready)
			return x58_rommon_error("use after reset: vpolicy set OFF BYTE");
		if (!x58_vendor_entry_vendor_state_clean(state))
			return x58_rommon_error(
				"generic write dirtied vendor state; cold reset required");
		if (!x58_rommon_require_vendor(state))
			return x58_rommon_error("vendor locked; use: unlock VENDOR");
		status = x58_vendor_invalidate_minit_policy();
		if (!x58_vendor_entry_print_status("POLICY_INVALIDATE", status))
			return false;
		if (status != X58_VENDOR_OK)
			return true;
		state->vendor_policy[a] = b;
		state->vendor_policy_modified = true;
		return x58_romstage_uart_puts("[VENDOR] POLICY[") &&
			x58_romstage_uart_put_hex(a, 2) && x58_romstage_uart_puts("]=") &&
			x58_romstage_uart_put_hex(b, 2) && x58_romstage_uart_puts(" FNV1A=") &&
			x58_romstage_uart_put_hex(x58_vendor_entry_policy_digest(state->vendor_policy), 8) &&
			x58_romstage_uart_puts("; not installed\r\n");
	}
	if (x58_rommon_streq(argv[0], "vpolicy") && argc == 3 &&
	    x58_rommon_streq(argv[1], "install")) {
		struct x58_vendor_runtime_info info;
		enum x58_vendor_status status;

		if (!x58_rommon_parse_hex(argv[2], &a) || !state->vendor_policy_ready)
			return x58_rommon_error(
				"use after review: vpolicy install POLICY_FNV");
		if (!x58_vendor_entry_vendor_state_clean(state))
			return x58_rommon_error(
				"generic write dirtied vendor state; cold reset required");
		if (!x58_rommon_require_vendor(state))
			return x58_rommon_error("vendor locked; use: unlock VENDOR");
		status = x58_vendor_install_confirmed_minit_policy(
			state->vendor_policy, sizeof(state->vendor_policy),
			a,
			X58_VENDOR_WRITE_CONFIRMATION);
		if (!x58_vendor_entry_print_status("POLICY_INSTALL", status))
			return false;
		if (status != X58_VENDOR_OK)
			return true;
		status = x58_vendor_runtime_probe(&info);
		if (!x58_vendor_entry_print_status("POLICY_READBACK", status))
			return false;
		if (status != X58_VENDOR_OK)
			return true;
		if (info.minit_policy_digest != a ||
		    info.minit_policy_current_digest != a)
			return x58_rommon_error("installed policy FNV readback mismatch");
		return x58_romstage_uart_puts("[VENDOR] POLICY FNV1A=") &&
			x58_romstage_uart_put_hex(x58_vendor_entry_policy_digest(state->vendor_policy), 8) &&
			x58_romstage_uart_puts(state->vendor_policy_modified ?
				" MODIFIED installed\r\n" :
				" REFERENCE installed\r\n");
	}
	if (x58_rommon_streq(argv[0], "vminit") && argc == 1) {
		struct x58_vendor_call_result result = { 0 };
		const uint8_t *policy;
		const uint8_t *workspace;
		enum x58_vendor_status status;
		bool candidate;
		bool full_path;
		uint32_t mc_mapper;
		uint32_t mc_common_f8;
		uint32_t ch2_dod;
		uint32_t ch2_ranks;

		if (!x58_vendor_entry_vendor_state_clean(state))
			return x58_rommon_error(
				"generic write dirtied vendor state; cold reset required");
		if (!x58_vendor_entry_vendor_path_ready(state))
			return x58_rommon_error(
				"vendor path requires accepted SPD FNV and vprep");
		if (!state->vendor_policy_ready)
			return x58_rommon_error("vminit requires current local policy copy");
		if (!x58_rommon_require_vendor(state))
			return x58_rommon_error("vendor locked; use: unlock VENDOR");
		status = x58_vendor_arm_minit(X58_VENDOR_WRITE_CONFIRMATION);
		if (!x58_vendor_entry_print_status("MINIT_ARM", status))
			return false;
		if (status != X58_VENDOR_OK)
			return true;
		if (!x58_romstage_uart_puts(
			"[VENDOR] CALL MINIT entry=fffc2000; reset/HLT/hang possible; no automatic DRAM access\r\n") ||
		    !x58_romstage_uart_wait_for(UART8250_LSR_TEMT,
					 X58_EARLY_UART_FLUSH_POLL_LIMIT))
			return false;
		outb(POST_X58_VENDOR_ENTRY_MINIT_CALL, CONFIG_POST_IO_PORT);
		status = x58_vendor_call_minit(&result);
		outb(POST_X58_VENDOR_ENTRY_MINIT_RETURN, CONFIG_POST_IO_PORT);
		if (!x58_vendor_entry_print_status("MINIT_CALL", status) ||
		    !x58_vendor_entry_print_call_result("MINIT", &result))
			return false;
		if (status != X58_VENDOR_OK)
			return x58_romstage_uart_puts(
				"[VENDOR] MINIT call returned to ROMMON but failed runtime validation; retained buffers are unavailable.\r\n") &&
				x58_vendor_entry_print_runtime(state);
		policy = x58_vendor_minit_policy();
		workspace = x58_vendor_minit_workspace();
		if (policy == NULL || workspace == NULL)
			return x58_rommon_error("MINIT returned without retained buffers");
		candidate = status == X58_VENDOR_OK && result.eax == 0 &&
			workspace[1] == 0 &&
			workspace[2] == 0 && policy[0x0a] == 0;
		full_path = candidate && workspace[X58_VENDOR_POLICY_WORK_COMPLETE_OFFSET] == 1 &&
			!(workspace[X58_VENDOR_POLICY_WORK_B3_FLAGS_OFFSET] & BIT(2));
		mc_mapper = x58_rommon_pci_read(X58_VENDOR_POLICY_MC_COMMON_DEV,
			X58_VENDOR_POLICY_MC_CHANNEL_MAPPER, 4);
		mc_common_f8 = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_UNCORE_COMMON_DEV, 0xf8, 4);
		ch2_dod = x58_rommon_pci_read(X58_VENDOR_POLICY_CHANNEL2_ADDR_DEV,
			X58_VENDOR_POLICY_MC_DOD_DIMM0, 4);
		ch2_ranks = x58_rommon_pci_read(X58_MEMORY_CONTROLLER_CHANNEL2_DEV,
			X58_MEMORY_CONTROLLER_MC_RANK_PRESENT, 4);
		if (!x58_romstage_uart_puts("[VENDOR] MINIT GATES EAX=") ||
		    !x58_romstage_uart_put_hex(result.eax, 8) ||
		    !x58_romstage_uart_puts(" WS01=") || !x58_romstage_uart_put_hex(workspace[1], 2) ||
		    !x58_romstage_uart_puts(" WS02=") || !x58_romstage_uart_put_hex(workspace[2], 2) ||
		    !x58_romstage_uart_puts(" POLICY0A=") || !x58_romstage_uart_put_hex(policy[0x0a], 2) ||
		    !x58_romstage_uart_puts(" WS4F=") ||
		    !x58_romstage_uart_put_hex(workspace[X58_VENDOR_POLICY_WORK_B3_FLAGS_OFFSET], 2) ||
		    !x58_romstage_uart_puts(" COMPLETE_E79=") ||
		    !x58_romstage_uart_put_hex(workspace[X58_VENDOR_POLICY_WORK_COMPLETE_OFFSET], 2) ||
		    !x58_romstage_uart_puts(" RESULT=") ||
		    !x58_romstage_uart_puts(full_path ?
			"FULL_PATH_RETURN_DRAM_UNTESTED" :
			(candidate &&
			 (workspace[X58_VENDOR_POLICY_WORK_B3_FLAGS_OFFSET] & BIT(2)) ?
				"EARLY_RETURN" :
			 ((result.eax & 0xffff) == 0xe801 ?
				"RESET_REQUEST_NOT_EXECUTED" : "REJECTED_OR_INCOMPLETE"))) ||
		    !x58_romstage_uart_puts("\r\n"))
			return false;
		if (!x58_romstage_uart_puts("[VENDOR] MINIT UNCORE MC_MAP60=") ||
		    !x58_romstage_uart_put_hex(mc_mapper, 8) ||
		    !x58_romstage_uart_puts(" MC_F8=") ||
		    !x58_romstage_uart_put_hex(mc_common_f8, 8) ||
		    !x58_romstage_uart_puts(" CH2_DOD80=") ||
		    !x58_romstage_uart_put_hex(ch2_dod, 8) ||
		    !x58_romstage_uart_puts(" CH2_RANK7C=") ||
		    !x58_romstage_uart_put_hex(ch2_ranks, 8) ||
		    !x58_romstage_uart_puts(" DRAM_ACCESSES=00\r\n"))
			return false;
		return x58_vendor_entry_print_runtime(state);
	}
	if (x58_rommon_streq(argv[0], "serialice") && argc == 1) {
		if (!x58_romstage_uart_puts("UNSAFE STREAM MODE: writes are immediate until reset\r\n"))
			return false;
		x58_rommon_serialice(false);
	}
	if (x58_rommon_streq(argv[0], "cpuid") && (argc == 2 || argc == 3)) {
		struct cpuid_result result;

		if (!x58_rommon_parse_hex(argv[1], &a) ||
		    (argc == 3 && !x58_rommon_parse_hex(argv[2], &b)))
			return x58_rommon_error("use: cpuid LEAF [SUB]");
		result = cpuid_ext(a, argc == 3 ? b : 0);
		return x58_romstage_uart_puts("EAX=") && x58_romstage_uart_put_hex(result.eax, 8) &&
			x58_romstage_uart_puts(" EBX=") && x58_romstage_uart_put_hex(result.ebx, 8) &&
			x58_romstage_uart_puts(" ECX=") && x58_romstage_uart_put_hex(result.ecx, 8) &&
			x58_romstage_uart_puts(" EDX=") && x58_romstage_uart_put_hex(result.edx, 8) &&
			x58_romstage_uart_puts("\r\n");
	}
	if (x58_rommon_streq(argv[0], "phyr") && argc == 4) {
		uint32_t raw;
		uint32_t value;

		if (!x58_rommon_parse_hex(argv[1], &a) || a > 2 ||
		    !x58_rommon_parse_hex(argv[2], &b) || b > UINT16_MAX ||
		    !x58_rommon_parse_hex(argv[3], &c) ||
		    !x58_memory_controller_phy_field_valid(b, c))
			return x58_rommon_error("use: phyr CH START WIDTH");
		/* PHY reads issue selector/command writes outside the vendor gates. */
		x58_vendor_entry_mark_vendor_state_dirty(state);
		if (!x58_memory_controller_phy_read(a, b, c, &raw, &value))
			return x58_rommon_error("PHY read timeout");
		return x58_romstage_uart_puts("PHY_RAW=") && x58_romstage_uart_put_hex(raw, 8) &&
			x58_romstage_uart_puts(" VALUE=") && x58_romstage_uart_put_hex(value, 8) &&
			x58_romstage_uart_puts("\r\n");
	}
	if (x58_rommon_streq(argv[0], "phyw") && argc == 6) {
		if (!x58_rommon_parse_hex(argv[1], &a) || a > 2 ||
		    !x58_rommon_parse_hex(argv[2], &b) || b > UINT16_MAX ||
		    !x58_rommon_parse_hex(argv[3], &c) ||
		    !x58_rommon_parse_hex(argv[4], &d) ||
		    !x58_rommon_parse_hex(argv[5], &e) || e > 1 ||
		    !x58_memory_controller_phy_field_valid(b, c))
			return x58_rommon_error("use: phyw CH START WIDTH VALUE MODE");
		if (!x58_rommon_require_write(state))
			return x58_rommon_error("write locked; use: unlock WRITE");
		if (!x58_memory_controller_phy_write(a, b, c, d, e))
			return x58_rommon_error("PHY write timeout");
		return x58_romstage_uart_puts("PHY WRITE OK; LOCKED\r\n");
	}
	if (x58_rommon_streq(argv[0], "rdtry") && argc == 2) {
		if (!x58_rommon_parse_hex(argv[1], &a))
			return x58_rommon_error("use: rdtry SWEEP");
		if (!x58_rommon_require_write(state))
			return x58_rommon_error("write locked; use: unlock WRITE");
		return x58_memory_controller_run_rd_try(a);
	}
	if (x58_rommon_streq(argv[0], "rdexact") && argc == 2) {
		uint32_t status = 0;

		if (!x58_rommon_parse_hex(argv[1], &a) || a > 0x80)
			return x58_rommon_error("use: rdexact SWEEP (<= 80)");
		if (!x58_rommon_require_write(state))
			return x58_rommon_error("write locked; use: unlock WRITE");
		if (!x58_memory_training_run_rd_exact(a, &status))
			return x58_rommon_error("exact RD gate/timeout");
		return true;
	}
	if (x58_rommon_streq(argv[0], "rdsweep") && argc == 1) {
		if (!x58_rommon_require_write(state))
			return x58_rommon_error("write locked; use: unlock WRITE");
		return x58_memory_training_run_rd_sweep();
	}
	if (x58_rommon_streq(argv[0], "rcvtry") && argc == 2) {
		if (!x58_rommon_parse_hex(argv[1], &a))
			return x58_rommon_error("use: rcvtry COARSE");
		if (!x58_rommon_require_write(state))
			return x58_rommon_error("write locked; use: unlock WRITE");
		return x58_memory_controller_run_rcven_try(a);
	}
	if (x58_rommon_streq(argv[0], "msr") && argc == 2) {
		msr_t value;

		if (!x58_rommon_parse_hex(argv[1], &a))
			return x58_rommon_error("use: msr INDEX");
		value = rdmsr(a);
		return x58_romstage_uart_puts("MSR=") && x58_romstage_uart_put_hex(value.hi, 8) &&
			x58_romstage_uart_put_hex(value.lo, 8) && x58_romstage_uart_puts("\r\n");
	}
	if (x58_rommon_streq(argv[0], "msrw") && argc == 4) {
		msr_t value;

		if (!x58_rommon_parse_hex(argv[1], &a) ||
		    !x58_rommon_parse_hex(argv[2], &b) ||
		    !x58_rommon_parse_hex(argv[3], &c))
			return x58_rommon_error("use: msrw INDEX HI LO");
		if (!x58_rommon_require_write(state))
			return x58_rommon_error("write locked; use: unlock WRITE");
		value.hi = b;
		value.lo = c;
		wrmsr(a, value);
		return x58_romstage_uart_puts("WRITE OK; LOCKED\r\n");
	}
	if (x58_rommon_streq(argv[0], "io") && (argc == 2 || argc == 3)) {
		if (!x58_rommon_parse_hex(argv[1], &a) || a > UINT16_MAX ||
		    !x58_rommon_width(argc == 3 ? argv[2] : NULL, &width))
			return x58_rommon_error("use: io PORT [b|w|l]");
		return x58_rommon_print_u32("VALUE=", x58_rommon_io_read(a, width), width * 2);
	}
	if (x58_rommon_streq(argv[0], "iow") && (argc == 3 || argc == 4)) {
		if (!x58_rommon_parse_hex(argv[1], &a) || a > UINT16_MAX ||
		    !x58_rommon_parse_hex(argv[2], &b) ||
		    !x58_rommon_width(argc == 4 ? argv[3] : NULL, &width))
			return x58_rommon_error("use: iow PORT VALUE [b|w|l]");
		if (!x58_rommon_require_write(state))
			return x58_rommon_error("write locked; use: unlock WRITE");
		x58_rommon_io_write(a, b, width);
		return x58_romstage_uart_puts("WRITE OK; LOCKED\r\n");
	}
	if ((x58_rommon_streq(argv[0], "pci") && (argc == 5 || argc == 6)) ||
	    (x58_rommon_streq(argv[0], "pciw") && (argc == 6 || argc == 7))) {
		const bool write = argv[0][3] == 'w';
		const unsigned int width_arg = write ? 6 : 5;

		if (!x58_rommon_parse_hex(argv[1], &a) || a > 0xff ||
		    !x58_rommon_parse_hex(argv[2], &b) || b > 0x1f ||
		    !x58_rommon_parse_hex(argv[3], &c) || c > 7 ||
		    !x58_rommon_parse_hex(argv[4], &d) || d > 0xff ||
		    (write && !x58_rommon_parse_hex(argv[5], &e)) ||
		    !x58_rommon_width(argc > width_arg ? argv[width_arg] : NULL, &width) ||
		    (d & (width - 1)))
			return x58_rommon_error(write ?
				"use: pciw BUS DEV FN REG VALUE [b|w|l]" :
				"use: pci BUS DEV FN REG [b|w|l]");
		if (!write)
			return x58_rommon_print_u32("VALUE=", x58_rommon_pci_read(
				PCI_DEV(a, b, c), d, width), width * 2);
		if (!x58_rommon_require_write(state))
			return x58_rommon_error("write locked; use: unlock WRITE");
		x58_rommon_pci_write(PCI_DEV(a, b, c), d, e, width);
		return x58_romstage_uart_puts("WRITE OK; LOCKED\r\n");
	}
	if ((x58_rommon_streq(argv[0], "mem") && (argc == 2 || argc == 3)) ||
	    (x58_rommon_streq(argv[0], "memw") && (argc == 3 || argc == 4))) {
		const bool write = argv[0][3] == 'w';

		if (!x58_rommon_parse_hex(argv[1], &a) ||
		    (write && !x58_rommon_parse_hex(argv[2], &b)) ||
		    !x58_rommon_width(argc == (write ? 4 : 3) ? argv[argc - 1] : NULL,
				 &width) || (a & (width - 1)))
			return x58_rommon_error(write ?
				"use: memw ADDR VALUE [b|w|l]" :
				"use: mem ADDR [b|w|l]");
		if (!write)
			return x58_rommon_print_u32("VALUE=", x58_rommon_mem_read(a, width),
				width * 2);
		if (!x58_rommon_require_write(state))
			return x58_rommon_error("write locked; use: unlock WRITE");
		x58_rommon_mem_write(a, b, width);
		return x58_romstage_uart_puts("WRITE OK; LOCKED\r\n");
	}
	if (x58_rommon_streq(argv[0], "dump") && argc == 3) {
		unsigned int offset;

		if (!x58_rommon_parse_hex(argv[1], &a) ||
		    !x58_rommon_parse_hex(argv[2], &b) || !b || b > X58_ROMMON_DUMP_MAX ||
		    a > UINT32_MAX - (b - 1))
			return x58_rommon_error("use: dump ADDR COUNT (COUNT <= 40 hex)");
		for (offset = 0; offset < b; offset++) {
			if ((offset & 0xf) == 0 &&
			    (!x58_romstage_uart_put_hex(a + offset, 8) || !x58_romstage_uart_putc(':')))
				return false;
			if (!x58_romstage_uart_putc(' ') ||
			    !x58_romstage_uart_put_hex(x58_rommon_mem_read(a + offset, 1), 2))
				return false;
			if ((offset & 0xf) == 0xf || offset + 1 == b) {
				if (!x58_romstage_uart_puts("\r\n"))
					return false;
			}
		}
		return true;
	}
	if (x58_rommon_streq(argv[0], "spd") && argc == 1)
		return x58_rommon_run_spd(state);
	if (x58_rommon_streq(argv[0], "post") && argc == 2) {
		if (!x58_rommon_parse_hex(argv[1], &a) || a > 0xff)
			return x58_rommon_error("use: post VALUE");
		if (!x58_rommon_require_write(state))
			return x58_rommon_error("write locked; use: unlock WRITE");
		outb(a, CONFIG_POST_IO_PORT);
		return x58_romstage_uart_puts("POST emitted; LOCKED\r\n");
	}
	if (x58_rommon_streq(argv[0], "halt") && argc == 1) {
		if (!x58_rommon_require_write(state))
			return x58_rommon_error("write locked; use: unlock WRITE");
		(void)x58_romstage_uart_puts("HALT\r\n");
		stop_with_post(POST_X58_ROMMON_READY);
	}

	return x58_rommon_error("unknown command or wrong arguments; use: help");
}

/* The standalone normal profile deliberately has no CAR monitor entry. */
static void __noreturn __maybe_unused x58_rommon(struct x58_rommon_state *state)
{
	char *argv[X58_ROMMON_MAX_ARGS];
	bool prompt = true;

	{
		if (!x58_romstage_uart_puts(
			"\r\nX58_MEMORY_PROFILE broad/unsafe path stopped; recovery ROMMON remains in CAR.\r\n") ||
		    !x58_vendor_entry_print_status("CAR_PREPARE",
			state->vendor_prepare_status) ||
		    !x58_romstage_uart_puts(
			"Type help. Start passive with: spd, vinfo, script status\r\n"))
			stop_with_post(POST_X58_ROMMON_RX_ERROR);
	}
	outb(POST_X58_ROMMON_READY, CONFIG_POST_IO_PORT);

	for (;;) {
		uint8_t value;
		enum x58_rommon_rx_result rx;

		if (prompt) {
			const char *prompt_text = "rommon> ";

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
			if (state->script_armed)
				prompt_text = "rommon[S]> ";
			if (!x58_romstage_uart_puts(prompt_text))
				stop_with_post(POST_X58_ROMMON_RX_ERROR);
			prompt = false;
		}
		rx = x58_rommon_getc(&value);
		if (rx == X58_ROMMON_RX_IDLE)
			continue;
		if (rx == X58_ROMMON_RX_FAULT) {
			outb(POST_X58_ROMMON_RX_ERROR, CONFIG_POST_IO_PORT);
			if (!x58_romstage_uart_puts("\r\nERR UART receive status; line cleared\r\n"))
				stop_with_post(POST_X58_ROMMON_RX_ERROR);
			state->length = 0;
			prompt = true;
			continue;
		}
		if (!state->length && value == '@') {
			/* SerialICE-QEMU sends '@' to trigger its initial prompt. */
			if (!x58_romstage_uart_putc(value))
				stop_with_post(POST_X58_ROMMON_RX_ERROR);
			x58_rommon_serialice(false);
		}
		if (!state->length && value == '*') {
			if (!x58_romstage_uart_putc(value))
				stop_with_post(POST_X58_ROMMON_RX_ERROR);
			x58_rommon_serialice(true);
		}

		if (value == '\r' || value == '\n') {
			unsigned int argc;

			if (value == '\n' && !state->length)
				continue;
			state->line[state->length] = '\0';
			if (!x58_romstage_uart_puts("\r\n"))
				stop_with_post(POST_X58_ROMMON_RX_ERROR);
			argc = x58_rommon_split(state->line, argv);
			if (argc > X58_ROMMON_MAX_ARGS) {
				if (!x58_rommon_error("too many arguments"))
					stop_with_post(POST_X58_ROMMON_RX_ERROR);
			} else if (!x58_rommon_execute(state, argc, argv)) {
				stop_with_post(POST_X58_ROMMON_RX_ERROR);
			}
			state->length = 0;
			outb(POST_X58_ROMMON_READY, CONFIG_POST_IO_PORT);
			prompt = true;
			continue;
		}
		if (value == 0x08 || value == 0x7f) {
			if (state->length) {
				state->length--;
				if (!x58_romstage_uart_puts("\b \b"))
					stop_with_post(POST_X58_ROMMON_RX_ERROR);
			}
			continue;
		}
		if (value == 0x15) {
			while (state->length) {
				state->length--;
				if (!x58_romstage_uart_puts("\b \b"))
					stop_with_post(POST_X58_ROMMON_RX_ERROR);
			}
			continue;
		}
		if (value < 0x20 || value > 0x7e)
			continue;
		if (state->length + 1 >= X58_ROMMON_LINE_SIZE) {
			if (!x58_romstage_uart_putc('\a'))
				stop_with_post(POST_X58_ROMMON_RX_ERROR);
			continue;
		}
		state->line[state->length++] = value;
		if (!x58_romstage_uart_putc(value))
			stop_with_post(POST_X58_ROMMON_RX_ERROR);
	}
}

void mainboard_romstage_entry(void)
{
	struct x58_romstage_ich10_snapshot ich10_before;
	struct x58_romstage_ich10_snapshot ich10_after;
	struct x58_romstage_smbus_snapshot smbus_before;
	struct x58_romstage_smbus_snapshot smbus_after;
	struct x58_rtc_policy_rtc_upper_bank_state rtc_upper_bank = { 0 };
	enum x58_vendor_smbus_entry_state smbus_entry;
	struct x58_rommon_state rommon_state = { 0 };
	enum x58_raminit_auto_result auto_result;
	struct x58_raminit_i801_signature entry_signature = { 0 };
	uint8_t readback_error;

	outb(POST_X58_EARLY_UART_ROMSTAGE_ENTRY, CONFIG_POST_IO_PORT);
	if (cpuid_eax(1) != 0x000206c2) {
		stop_with_post(POST_X58_PLATFORM_CPU_ERROR);
	}
	printk(BIOS_NOTICE, "[PLATFORM] EXTRA_MCA_SNAPSHOTS=SKIPPED CPU_MCA_SAFETY=REQUIRED\n");
	outb(POST_X58_ROMSTAGE_ICH10R_BEGIN, CONFIG_POST_IO_PORT);
	capture_ich10(&ich10_before);
	capture_smbus(&smbus_before);
	readback_error = ich10_preflight_error(&ich10_before);
	if (readback_error)
		stop_with_post(readback_error);
	if (smbus_before.id == UINT32_MAX)
		stop_with_post(POST_X58_ROMSTAGE_SMBUS_ABSENT);
	if (smbus_before.id != PCI_ID_ICH10R_SMBUS)
		stop_with_post(POST_X58_ROMSTAGE_SMBUS_ID_ERROR);
	/* Do not move a live I/O BAR; legacy stages require cold defaults. */
	if (smbus_is_exact_cold_default(&smbus_before))
		smbus_entry = X58_VENDOR_SMBUS_ENTRY_COLD;
	else if (smbus_is_exact_configured(&smbus_before)) {
		smbus_entry = X58_VENDOR_SMBUS_ENTRY_CONFIGURED_AFTER_RESET;
		/* Capture before any SPD command overwrites the volatile signature. */
		x58_raminit_capture_i801_signature(&entry_signature);
	} else
		stop_with_post(POST_X58_ROMSTAGE_SMBUS_CONFIG_ERROR);

	/* Existing coreboot ICH10 early BAR setup; no GPIO pins are touched. */
	i82801jx_setup_bars();
	capture_ich10(&ich10_after);
	readback_error = ich10_readback_error(&ich10_before, &ich10_after);
	if (readback_error)
		stop_with_post(readback_error);
	outb(POST_X58_ROMSTAGE_ICH10R_READY, CONFIG_POST_IO_PORT);
	/* X58_RTC_POLICY's single RCBA write is separately exact-gated and reported. */
	x58_rtc_policy_enable_rtc_upper_bank(&rtc_upper_bank);

	/* Program the controller decode only; do not access SMBus status/data. */
	if (smbus_enable_iobar(CONFIG_FIXED_SMBUS_IO_BASE) < 0)
		stop_with_post(POST_X58_ROMSTAGE_SMBUS_CONFIG_ERROR);
	capture_smbus(&smbus_after);
	if (!smbus_readback_valid(&smbus_after))
		stop_with_post(POST_X58_ROMSTAGE_SMBUS_CONFIG_ERROR);
	/* Read only idle host registers, after decode and before any SPD command. */
	if (smbus_entry == X58_VENDOR_SMBUS_ENTRY_COLD) {
		x58_raminit_capture_i801_signature(&entry_signature);
	} else if ((x58_raminit_i801_signature_matches(&entry_signature,
			&x58_init_phase_pass2_signature) ||
		    x58_raminit_i801_signature_matches(&entry_signature,
			&x58_init_phase_pass3_signature)) &&
		   !x58_raminit_set_i801_signature(&x58_init_phase_consumed_signature)) {
		/* Never leave a recognized automatic phase retryable after a fault. */
		x58_init_phase_guard_stop("X58_INIT_PHASE_ENTRY_PHASE_CONSUME");
	}
	outb(POST_X58_ROMSTAGE_SMBUS_READY, CONFIG_POST_IO_PORT);

	if (!report_ich10(&ich10_before, &ich10_after,
			  &smbus_before, &smbus_after))
		stop_with_post(POST_X58_ROMSTAGE_UART_ERROR);
	if (!x58_rtc_policy_report_rtc_upper_bank(&rtc_upper_bank))
		stop_with_post(POST_X58_ROMSTAGE_UART_ERROR);
	if (!x58_romstage_uart_puts("[SMBUS] ENTRY=") ||
	    !x58_romstage_uart_puts(smbus_entry == X58_VENDOR_SMBUS_ENTRY_COLD ?
			    "COLD_DEFAULT\r\n" :
			    "CONFIGURED_AFTER_RESET\r\n"))
		stop_with_post(POST_X58_ROMSTAGE_UART_ERROR);
	outb(POST_X58_ROMSTAGE_UART_SENT, CONFIG_POST_IO_PORT);
	x58_rommon_state_initialize(&rommon_state);
	auto_result = x58_init_phase_high_qpi_probe(&rommon_state, smbus_entry,
		&entry_signature
		, &rtc_upper_bank
		);
	if (auto_result == X58_RAMINIT_AUTO_UART_ERROR)
		stop_with_post(POST_X58_ROMMON_RX_ERROR);
	if (auto_result == X58_RAMINIT_AUTO_READY)
		return;
	x58_normal_boot_fallback();
}
