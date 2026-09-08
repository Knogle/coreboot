/* SPDX-License-Identifier: GPL-2.0-only */

#include "development_id.h"

#include <arch/cpuid.h>
#include <arch/io.h>
#include <arch/pci_io_cfg.h>
#include <bootstate.h>
#include <cbmem.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <console/uart.h>
#include <cpu/x86/cache.h>
#include <cpu/x86/msr.h>
#include <cpu/x86/mtrr.h>
#include <device/mmio.h>
#include <device/pci_def.h>
#include <device/pci_type.h>
#include <drivers/uart/uart8250reg.h>
#include <southbridge/intel/common/pmutil.h>
#include <southbridge/intel/i82801jx/i82801jx.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "b06v6_handoff.h"
#if CONFIG_X58_PRO_E_B06WF_USB_LAB
#include "b06wf_usb_lab.h"
#endif
#include "ram_loader.h"

#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
#define B06V6_RAMMON_BUILD_ID X58_DEVELOPMENT_BUILD_ID
#elif CONFIG_X58_PRO_E_B06WK_ACPI_REPAIR
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06WK-ACPI-REPAIR-20260908"
#elif CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06WJ-ACPI-PLATFORM-20260907"
#elif CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06WI-VENDOR-IRQ-ACPI-20260907"
#elif CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06WH-AUTO-USB-SEABIOS-20260907"
#elif CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06WG-AUTO-USB-SEABIOS-20260907"
#elif CONFIG_X58_PRO_E_B06WF_USB_LAB
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06WF-LATE-USB-LAB-20260907"
#elif CONFIG_X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06WE-ACPI-SAD-BDF-FIX-20260907"
#elif CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06WD-SEABIOS-INPUT-20260906"
#elif CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06WC-INTEGRATED-PLATFORM-20260906"
#elif CONFIG_X58_PRO_E_B06WB_TCO_HALT
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06WB-ICH10-TCO-HALT-20260906"
#elif CONFIG_X58_PRO_E_B06WA_HPET_DECODE
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06WA-ICH10-HPET-DECODE-20260906"
#elif CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VZ-FIXED-RESOURCES-20260906"
#elif CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VY-ICH10-IOAPIC-MASK-20260906"
#elif CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VX-AHCI-USBTRACE-20260906"
#elif CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VW-ICH10-AHCI-MMIO-20260906"
#elif CONFIG_X58_PRO_E_B06VV_ICH10_PCS_SCLK
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VV-ICH10-PCS-SCLK-20260906"
#elif CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VU-ICH10-AHCI-ROUTE-20260906"
#elif CONFIG_X58_PRO_E_B06VQ_ICHBASE1
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VQ-ICHBASE1-20260906"
#elif CONFIG_X58_PRO_E_B06VQ_PLATRO1
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VQ-PLATRO1-20260906"
#elif CONFIG_X58_PRO_E_B06VQ_USB_TRACE1
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VQ-USBTRACE1-20260906"
#elif CONFIG_X58_PRO_E_B06VT_ICH10_SATA_CLOCK
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VT-ICH10-SATA-CLOCK-20260906"
#elif CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VS-ICH10-AHCI-PORTS-20260906"
#elif CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VR-ICH10-AHCI-MAP-20260906"
#elif CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VQ-ICH10-EHCI-INIT-20260906"
#elif CONFIG_X58_PRO_E_B06VP_LAPIC_EXTINT
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VP-LAPIC-EXTINT-PROBE-20260906"
#elif CONFIG_X58_PRO_E_B06VO_IOHBUSNO_ROUTE
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VO-IOHBUSNO-ROUTE-PROBE-20260906"
#elif CONFIG_X58_PRO_E_B06VN_IOU0_PCIE_PHYS_VBIOS
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VN-IOU0-HD5450-PHYSVBIOS-20260906"
#elif CONFIG_X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VM-AUTO-PCI-VGA-IPXE-20260905"
#elif CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VL-BROAD-HARD-GATE-SEABIOS-20260905"
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VK-Q-CANONICAL-CLASS-SEABIOS-20260905"
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VJ-COUPLED-PROFILE-Q-SEABIOS-20260905"
#elif CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VI-COUPLED-PROFILE-O-SEABIOS-20260905"
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VH-PRIMARY-WORKSPACE-REARM-SEABIOS-20260905"
#elif CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VG-COUPLED-MINIT-SEABIOS-20260905"
#elif CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VF-SEABIOS-ENTRY-PROBE-20260905"
#elif CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VE-HIGHQPI-AUTO-RAMSTAGE-ROMMON-20260905"
#elif CONFIG_X58_PRO_E_B06VD_HIGH_QPI_CSI2A6_MINIT
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VD-UNEXPECTED-RAMSTAGE-20260905"
#elif CONFIG_X58_PRO_E_B06VC_HIGH_QPI_A0_4SET_MINIT
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VC-UNEXPECTED-RAMSTAGE-20260904"
#elif CONFIG_X58_PRO_E_B06VB_HIGH_QPI_MINIT_OBSERVE
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VB-UNEXPECTED-RAMSTAGE-20260904"
#elif CONFIG_X58_PRO_E_B06VA_HIGH_QPI_A0_SET
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06VA-UNEXPECTED-RAMSTAGE-20260904"
#elif CONFIG_X58_PRO_E_B06V9_DETERMINISTIC_QPI_3PASS
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06V9-UNEXPECTED-RAMSTAGE-20260904"
#elif CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06V8-UNEXPECTED-RAMSTAGE-20260904"
#elif CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06V7-ROBUST-SLOWQPI-RAMSTAGE-ROMMON-20260904"
#else
#define B06V6_RAMMON_BUILD_ID \
	"X58PROE-B06V6-DRAM-ROMMON-XRL1-20260904"
#endif

#define POST_B06V6_RAMMON_ENTRY		0x0e
#define POST_B06V6_RAMMON_READY		0x0f
#define POST_B06V6_RAMLOAD_ACTIVE	0x33
#define POST_B06V6_RAMLOAD_COMPLETE	0x35
#define POST_B06V6_RAMMON_HANDOFF_FAIL	0x1a
#define POST_B06V6_RAMLOAD_FAILED	0x1b
#define POST_B06V6_RESET_WARM		0x1c
#define POST_B06V6_RESET_FULL		0x1d
#define POST_B06V6_HALT			0x1e
#define POST_B06VF_RAMMON_RETURN	0x21

#define B06V6_LINE_SIZE		128
#define B06V6_ARGV_SIZE		8
#define B06V6_DUMP_MAX		0x100
#define B06V6_UART_TX_POLLS	100000u
#define B06V6_UART_LINE_POLLS	1000000u
#define B06V6_UART_DRAIN_IDLE_POLLS	100000u
#define B06V6_UART_DRAIN_MAX_BYTES	256u
#define B06V6_HEADER_BYTE_POLLS	30000000u
#define B06V6_PAYLOAD_BYTE_POLLS	5000000u
#define B06V6_PM_TIMER_MASK	0x00ffffffu
#define B06V6_PM_TIMER_SAMPLES	4096u
#define B06V6_RST_CNT_PORT	0x0cf9

#define ICH10R_LPC_DEV		PCI_DEV(0, 0x1f, 0)
#define ICH10R_RP5_DEV		PCI_DEV(0, 0x1c, 4)
#define ICH10R_LPC_EXPECTED_ID	0x3a168086u
#define ICH10R_RP5_EXPECTED_ID	0x3a488086u
#define RTL8168_EXPECTED_ID	0x816810ecu

#define B06V6_POSTCAR_MTRR0_BASE_LO	0x01000006u
#define B06V6_POSTCAR_MTRR0_MASK_LO	0xff800800u
#define B06V6_POSTCAR_MTRR1_BASE_LO	0xfffc0005u
#define B06V6_POSTCAR_MTRR1_MASK_LO	0xfffc0800u
#define B06V6_MTRR_HIGH_MASK		0x000000ffu
#define B06V6_EXPECTED_MTRR_CAP		0x00000d0au
#define B06V6_EXPECTED_MTRR_DEF		MTRR_DEF_TYPE_EN

#define B06VM_POSTCAR_MTRR0_BASE_LO	0x00000006u
#define B06VM_POSTCAR_MTRR0_MASK_LO	0x80000800u
#define B06VM_POSTCAR_MTRR1_BASE_LO	0x80000006u
#define B06VM_POSTCAR_MTRR1_MASK_LO	0xc0000800u
#define B06VM_POSTCAR_MTRR2_BASE_LO	0x000a0000u
#define B06VM_POSTCAR_MTRR2_MASK_LO	0xfffe0800u
#define B06VM_POSTCAR_MTRR3_BASE_LO	B06V6_POSTCAR_MTRR1_BASE_LO
#define B06VM_POSTCAR_MTRR3_MASK_LO	B06V6_POSTCAR_MTRR1_MASK_LO

#if CONFIG_X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE
static const uint32_t b06v6_expected_mtrr_base_lo[] = {
	B06VM_POSTCAR_MTRR0_BASE_LO,
	B06VM_POSTCAR_MTRR1_BASE_LO,
	B06VM_POSTCAR_MTRR2_BASE_LO,
	B06VM_POSTCAR_MTRR3_BASE_LO,
};

static const uint32_t b06v6_expected_mtrr_mask_lo[] = {
	B06VM_POSTCAR_MTRR0_MASK_LO,
	B06VM_POSTCAR_MTRR1_MASK_LO,
	B06VM_POSTCAR_MTRR2_MASK_LO,
	B06VM_POSTCAR_MTRR3_MASK_LO,
};
#else
static const uint32_t b06v6_expected_mtrr_base_lo[] = {
	B06V6_POSTCAR_MTRR0_BASE_LO,
	B06V6_POSTCAR_MTRR1_BASE_LO,
};

static const uint32_t b06v6_expected_mtrr_mask_lo[] = {
	B06V6_POSTCAR_MTRR0_MASK_LO,
	B06V6_POSTCAR_MTRR1_MASK_LO,
};
#endif

_Static_assert(ARRAY_SIZE(b06v6_expected_mtrr_base_lo) ==
	ARRAY_SIZE(b06v6_expected_mtrr_mask_lo),
	"postcar MTRR base/mask tables must remain paired");

_Static_assert(CONFIG_UART_FOR_CONSOLE == 0,
	"B06V6 binary transport requires COM1");
_Static_assert(CONFIG_TTYS0_BASE == 0x3f8,
	"B06V6 binary transport requires I/O base 0x3f8");
_Static_assert(CONFIG_TTYS0_BAUD == 115200,
	"B06V6 binary transport requires 115200 baud");
_Static_assert(CONFIG_TTYS0_LCS == UART8250_LCR_WLS_8,
	"B06V6 binary transport requires 8N1");

struct b06v6_rammon_state {
	struct x58_rl_state loader;
	const struct x58_b06v6_handoff *handoff;
	bool reset_armed;
};

static struct b06v6_rammon_state rammon;

static uintptr_t b06v6_uart_base(void)
{
	return uart_platform_base(get_uart_for_console());
}

static bool b06v6_uart_putc(uint8_t value)
{
	const uintptr_t base = b06v6_uart_base();

	for (uint32_t poll = 0; poll < B06V6_UART_TX_POLLS; poll++) {
		if (inb(base + UART8250_LSR) & UART8250_LSR_THRE) {
			outb(value, base + UART8250_TBR);
			return true;
		}
	}

	return false;
}

static bool b06v6_uart_flush(void)
{
	const uintptr_t base = b06v6_uart_base();

	for (uint32_t poll = 0; poll < B06V6_UART_TX_POLLS * 16; poll++) {
		if (inb(base + UART8250_LSR) & UART8250_LSR_TEMT)
			return true;
	}

	return false;
}

/*
 * A terminal may encode Enter as CRLF (or even CRCRLF).  read_line() returns
 * on the first terminator, so consume the bounded command tail before XRL1
 * emits READY.  The host waits for READY before sending the binary header.
 */
static bool b06v6_uart_drain_command_tail(size_t *discarded)
{
	const uintptr_t base = b06v6_uart_base();
	uint32_t idle_polls = 0;
	size_t count = 0;

	while (idle_polls < B06V6_UART_DRAIN_IDLE_POLLS) {
		const uint8_t lsr = inb(base + UART8250_LSR);

		if (lsr & UART8250_LSR_DR) {
			(void)inb(base + UART8250_RBR);
			if (++count > B06V6_UART_DRAIN_MAX_BYTES)
				return false;
			idle_polls = 0;
			continue;
		}
		if (lsr & (UART8250_LSR_OE | UART8250_LSR_PE |
			   UART8250_LSR_FE | UART8250_LSR_BI))
			return false;
		idle_polls++;
	}
	if (discarded != NULL)
		*discarded = count;
	return true;
}

static enum x58_rl_io_result b06v6_uart_receive(void *context,
	uint8_t *value, uint32_t poll_limit)
{
	const uintptr_t base = b06v6_uart_base();

	(void)context;
	if (value == NULL || poll_limit == 0)
		return X58_RL_IO_TIMEOUT;
	for (uint32_t poll = 0; poll < poll_limit; poll++) {
		const uint8_t lsr = inb(base + UART8250_LSR);

		if (lsr & (UART8250_LSR_OE | UART8250_LSR_PE |
			   UART8250_LSR_FE | UART8250_LSR_BI)) {
			if (lsr & UART8250_LSR_DR)
				(void)inb(base + UART8250_RBR);
			return X58_RL_IO_FAULT;
		}
		if (lsr & UART8250_LSR_DR) {
			*value = inb(base + UART8250_RBR);
			return X58_RL_IO_BYTE;
		}
	}

	return X58_RL_IO_TIMEOUT;
}

static bool b06v6_uart_transmit(void *context, uint8_t value)
{
	(void)context;
	return b06v6_uart_putc(value);
}

static bool b06v6_object_span_valid(uint32_t address, size_t size)
{
	uint32_t offset;

	if (address < X58_B06V6_OBJECT_BASE ||
	    size > X58_B06V6_OBJECT_SIZE)
		return false;
	offset = address - X58_B06V6_OBJECT_BASE;
	return offset <= X58_B06V6_OBJECT_SIZE &&
		size <= X58_B06V6_OBJECT_SIZE - offset;
}

static bool b06v6_object_write(void *context, uint32_t address,
	const uint8_t *data, size_t size)
{
	volatile uint8_t *destination;

	(void)context;
	if (data == NULL || !b06v6_object_span_valid(address, size))
		return false;
	destination = (volatile uint8_t *)(uintptr_t)address;
	for (size_t i = 0; i < size; i++)
		destination[i] = data[i];
	asm volatile ("mfence" ::: "memory");
	return true;
}

static bool b06v6_object_read(void *context, uint32_t address,
	uint8_t *data, size_t size)
{
	const volatile uint8_t *source;

	(void)context;
	if (data == NULL || !b06v6_object_span_valid(address, size))
		return false;
	source = (const volatile uint8_t *)(uintptr_t)address;
	for (size_t i = 0; i < size; i++)
		data[i] = source[i];
	return true;
}

static const struct x58_rl_policy b06v6_loader_policy = {
	.staging_base = X58_B06V6_OBJECT_BASE,
	.staging_size = X58_B06V6_OBJECT_SIZE,
	.max_object_size = X58_B06V6_OBJECT_MAX_SIZE,
	.require_readback_verify = true,
};

static const struct x58_rl_sink b06v6_loader_sink = {
	.context = NULL,
	.write = b06v6_object_write,
	.read = b06v6_object_read,
};

static const struct x58_rl_serial_io b06v6_loader_serial = {
	.context = NULL,
	.receive = b06v6_uart_receive,
	.transmit = b06v6_uart_transmit,
};

static const struct x58_rl_serial_limits b06v6_loader_limits = {
	.header_byte_poll_limit = B06V6_HEADER_BYTE_POLLS,
	.payload_byte_poll_limit = B06V6_PAYLOAD_BYTE_POLLS,
};

static void __noreturn b06v6_rammon_stop(uint8_t post, const char *reason)
{
	printk(BIOS_EMERG, "[RAMSTAGE] FAIL %s POST=%02x\n", reason, post);
	(void)b06v6_uart_flush();
	outb(post, CONFIG_POST_IO_PORT);
	asm volatile ("cli" ::: "memory");
	for (;;)
		asm volatile ("hlt");
}

static bool b06v6_parse_hex_u32(const char *text, uint32_t *value)
{
	uint32_t parsed = 0;
	unsigned int digits = 0;

	if (text == NULL || value == NULL)
		return false;
	if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
		text += 2;
	while (*text != '\0') {
		uint8_t digit;

		if (*text >= '0' && *text <= '9')
			digit = *text - '0';
		else if (*text >= 'a' && *text <= 'f')
			digit = *text - 'a' + 10;
		else if (*text >= 'A' && *text <= 'F')
			digit = *text - 'A' + 10;
		else
			return false;
		if (digits == 8)
			return false;
		parsed = (parsed << 4) | digit;
		digits++;
		text++;
	}
	if (digits == 0)
		return false;
	*value = parsed;
	return true;
}

static int b06v6_tokenize(char *line, char *argv[B06V6_ARGV_SIZE])
{
	int argc = 0;

	while (*line != '\0') {
		while (*line == ' ' || *line == '\t')
			line++;
		if (*line == '\0')
			break;
		if (argc == B06V6_ARGV_SIZE)
			return -1;
		argv[argc++] = line;
		while (*line != '\0' && *line != ' ' && *line != '\t')
			line++;
		if (*line != '\0')
			*line++ = '\0';
	}

	return argc;
}

static bool b06v6_read_line(char line[B06V6_LINE_SIZE])
{
	size_t used = 0;

	for (;;) {
		uint8_t value;
		enum x58_rl_io_result result;

		result = b06v6_uart_receive(NULL, &value, B06V6_UART_LINE_POLLS);
		if (result == X58_RL_IO_TIMEOUT)
			continue;
		if (result != X58_RL_IO_BYTE) {
			printk(BIOS_ERR, "\n[RAMMON] UART framing/overrun fault\n");
			return false;
		}
		if (value == '\r' || value == '\n') {
			if (!b06v6_uart_putc('\r') || !b06v6_uart_putc('\n'))
				return false;
			line[used] = '\0';
			return true;
		}
		if (value == '\b' || value == 0x7f) {
			if (used != 0) {
				used--;
				if (!b06v6_uart_putc('\b') ||
				    !b06v6_uart_putc(' ') ||
				    !b06v6_uart_putc('\b'))
					return false;
			}
			continue;
		}
		if (value < 0x20 || value > 0x7e)
			continue;
		if (used + 1 >= B06V6_LINE_SIZE) {
			printk(BIOS_ERR, "\n[RAMMON] line too long; discarded\n");
			line[0] = '\0';
			return true;
		}
		line[used++] = value;
		if (!b06v6_uart_putc(value))
			return false;
	}
}

static bool b06v6_parse_width(const char *text, unsigned int *width)
{
	uint32_t parsed;

	if (!b06v6_parse_hex_u32(text, &parsed) ||
	    (parsed != 1 && parsed != 2 && parsed != 4))
		return false;
	*width = parsed;
	return true;
}

static uint32_t b06v6_mem_read(uint32_t address, unsigned int width)
{
	if (width == 1)
		return read8p(address);
	if (width == 2)
		return read16p(address);
	return read32p(address);
}

static uint32_t b06v6_io_read(uint16_t port, unsigned int width)
{
	if (width == 1)
		return inb(port);
	if (width == 2)
		return inw(port);
	return inl(port);
}

static uint32_t b06v6_pci_read(pci_devfn_t dev, uint16_t reg,
	unsigned int width)
{
	if (width == 1)
		return pci_io_read_config8(dev, reg);
	if (width == 2)
		return pci_io_read_config16(dev, reg);
	return pci_io_read_config32(dev, reg);
}

static void b06v6_print_help(void)
{
	printk(BIOS_INFO,
	       "Commands (all numbers hexadecimal):\n"
	       "  id | help | handoff | cpuid LEAF [SUBLEAF] | mtrr\n"
	       "  obj status | obj clear | obj crc | obj dump OFFSET COUNT\n"
	       "  ramload                 enter binary XRL1 upload mode\n"
	       "  timer                   passive ACPI PM-timer movement probe\n"
	       "  netprobe                passive RP5/RTL8168 PCI snapshot\n"
	       "  pci BUS DEV FN REG WIDTH  read PCI config (WIDTH 1/2/4)\n"
	       "  io PORT WIDTH           read port I/O (WIDTH 1/2/4)\n"
	       "  mem ADDRESS WIDTH       read linear/MMIO (WIDTH 1/2/4)\n"
	       "  msr INDEX               read MSR (unsafe for invalid indices)\n"
	       "  unlock RESET | lock | reset warm|full | halt\n"
	       "Upload is data-only here: no arm/exec command is connected.\n"
#if CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	       "TFTP/SSH/NIC writes/DMA are not implemented in B06VL.\n");
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
	       "TFTP/SSH/NIC writes/DMA are not implemented in B06VK.\n");
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
	       "TFTP/SSH/NIC writes/DMA are not implemented in B06VJ.\n");
#elif CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	       "TFTP/SSH/NIC writes/DMA are not implemented in B06VI.\n");
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
	       "TFTP/SSH/NIC writes/DMA are not implemented in B06VH.\n");
#elif CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	       "TFTP/SSH/NIC writes/DMA are not implemented in B06VG.\n");
#elif CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
	       "TFTP/SSH/NIC writes/DMA are not implemented in B06VF.\n");
#elif CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
	       "TFTP/SSH/NIC writes/DMA are not implemented in B06VE.\n");
#elif CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
	       "TFTP/SSH/NIC writes/DMA are not implemented in B06V7.\n");
#else
	       "TFTP/SSH/NIC writes/DMA are not implemented in B06V6.\n");
#endif

#if CONFIG_X58_PRO_E_B06WF_USB_LAB
	b06wf_usb_lab_help();
#endif
}

static void b06v6_print_handoff(void)
{
	const struct x58_b06v6_handoff *handoff = rammon.handoff;

	printk(BIOS_INFO,
	       "[HANDOFF] ptr=%p valid=%u magic=%08x ver=%u size=%u flags=%08x digest=%08x\n",
	       handoff, x58_b06v6_handoff_is_valid(handoff), handoff->magic,
	       handoff->version, handoff->structure_size, handoff->flags,
	       handoff->digest);
	printk(BIOS_INFO,
	       "[HANDOFF] CPUID=%08x UCODE=%08x SPD=%08x CSI=%08x POLICY=%08x WS=%08x EAX=%08x\n",
	       handoff->raminit.cpuid, handoff->raminit.microcode_revision,
	       handoff->raminit.spd_fnv, handoff->raminit.csi_state_fnv,
	       handoff->raminit.policy_fnv, handoff->raminit.workspace_fnv,
	       handoff->raminit.minit_eax);
#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
	printk(BIOS_INFO, "[HANDOFF] SPD_RAW=%08x SPD_PROFILE=%08x SERIAL_122_125=IGNORED\n",
	       handoff->raminit.spd_fnv, handoff->raminit.spd_profile_fnv);
#endif
#if CONFIG_X58_PRO_E_B06V7_ROBUST_SLOWQPI
	printk(BIOS_INFO,
	       "[HANDOFF] CSI_CANON=%08x WS_CANON=%08x\n",
	       handoff->raminit.csi_state_canonical_fnv,
	       handoff->raminit.workspace_canonical_fnv);
#endif
#if CONFIG_X58_PRO_E_B06VE_HIGH_QPI_AUTO_HANDOFF
	printk(BIOS_INFO, "[HANDOFF] POST_MINIT_IOH_STAGE9C=%08x\n",
	       handoff->raminit.post_minit_ioh_stage_9c);
#endif
#if CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	printk(BIOS_INFO,
	       "[HANDOFF] PROFILE=%u PRE_A0=%08x POST_A0=%08x POST_9C=%08x\n",
	       handoff->raminit.profile_id, handoff->raminit.saved_pre_a0,
	       handoff->raminit.post_minit_cpu_a0,
	       handoff->raminit.post_minit_cpu_9c);
#endif
	printk(BIOS_INFO,
	       "[HANDOFF] MC=%08x F8=%08x DOD=%08x RANKS=%08x STATUS=%08x QPI=%08x\n",
	       handoff->raminit.mc_mapper, handoff->raminit.mc_common_f8,
	       handoff->raminit.ch2_dod, handoff->raminit.ch2_ranks,
	       handoff->raminit.ch2_status, handoff->raminit.qpi_status);
	printk(BIOS_INFO,
	       "[HANDOFF] MTRRCAP=%08x DEF=%08x CBMEM=%08x..%08x OBJECT=%08x+%08x MAX=%08x\n",
	       handoff->mtrr_cap, handoff->mtrr_def_type,
	       handoff->cbmem_base, handoff->cbmem_top, handoff->object_base,
	       handoff->object_size, handoff->object_max_size);
}

static void b06v6_print_object(void)
{
	const struct x58_rl_state *state = &rammon.loader;

	printk(BIOS_INFO,
	       "[OBJECT] loaded=%u receiving=%u exec_armed=%u result=%s bytes=%08x\n",
	       state->loaded, state->receiving, state->execute_armed,
	       x58_rl_result_name(state->last_result), state->received);
	if (state->loaded)
		printk(BIOS_INFO,
		       "[OBJECT] id=%08x dst=%08x len=%08x flags=%04x entry=%08x crc=%08x\n",
		       state->header.object_id, state->header.destination,
		       state->header.length, state->header.flags,
		       state->header.entry_offset, state->header.payload_crc32);
}

static void b06v6_dump_object(uint32_t offset, uint32_t count)
{
	const struct x58_rl_state *state = &rammon.loader;
	uint8_t bytes[16];
	uint32_t consumed = 0;

	if (!state->loaded) {
		printk(BIOS_ERR, "[OBJECT] no complete object\n");
		return;
	}
	if (count == 0 || count > B06V6_DUMP_MAX ||
	    offset > state->header.length ||
	    count > state->header.length - offset) {
		printk(BIOS_ERR,
		       "[OBJECT] dump range rejected (max count %x)\n",
		       B06V6_DUMP_MAX);
		return;
	}
	while (consumed < count) {
		size_t chunk = count - consumed;

		if (chunk > sizeof(bytes))
			chunk = sizeof(bytes);
		if (!b06v6_object_read(NULL,
			state->header.destination + offset + consumed,
			bytes, chunk)) {
			printk(BIOS_ERR, "[OBJECT] sink read failed\n");
			return;
		}
		printk(BIOS_INFO, "%08x:",
		       state->header.destination + offset + consumed);
		for (size_t i = 0; i < chunk; i++)
			printk(BIOS_INFO, " %02x", bytes[i]);
		printk(BIOS_INFO, "\n");
		consumed += chunk;
	}
}

static void b06v6_report_timer(void)
{
	const uint32_t id = pci_io_read_config32(ICH10R_LPC_DEV, PCI_VENDOR_ID);
	const uint32_t pmbase_reg =
		pci_io_read_config32(ICH10R_LPC_DEV, D31F0_PMBASE);
	const uint8_t acpi_cntl =
		pci_io_read_config8(ICH10R_LPC_DEV, D31F0_ACPI_CNTL);
	const uint16_t pmbase = pmbase_reg & 0xfffc;
	uint32_t first = 0;
	uint32_t last = 0;
	uint32_t changes = 0;

	if (id != ICH10R_LPC_EXPECTED_ID || pmbase == 0 ||
	    !(acpi_cntl & 0x80)) {
		printk(BIOS_ERR,
		       "[TIMER] unavailable LPC=%08x PMBASE=%08x ACPI=%02x\n",
		       id, pmbase_reg, acpi_cntl);
		return;
	}
	first = inl(pmbase + PM1_TMR) & B06V6_PM_TIMER_MASK;
	last = first;
	for (uint32_t i = 0; i < B06V6_PM_TIMER_SAMPLES; i++) {
		const uint32_t sample =
			inl(pmbase + PM1_TMR) & B06V6_PM_TIMER_MASK;

		if (sample != last)
			changes++;
		last = sample;
	}
	printk(BIOS_INFO,
	       "[TIMER] passive PMBASE=%04x first=%06x last=%06x delta=%06x changes=%08x samples=%08x\n",
	       pmbase, first, last, (last - first) & B06V6_PM_TIMER_MASK,
	       changes, B06V6_PM_TIMER_SAMPLES);
}

static void b06v6_passive_netprobe(void)
{
	const uint32_t rp_id =
		pci_io_read_config32(ICH10R_RP5_DEV, PCI_VENDOR_ID);
	const uint32_t rp_class =
		pci_io_read_config32(ICH10R_RP5_DEV, PCI_CLASS_REVISION);
	const uint16_t rp_command =
		pci_io_read_config16(ICH10R_RP5_DEV, PCI_COMMAND);
	const uint32_t buses = pci_io_read_config32(ICH10R_RP5_DEV, 0x18);
	const uint8_t secondary = (buses >> 8) & 0xff;

	printk(BIOS_INFO,
	       "[NET] PASSIVE ONLY RP5 00:1c.4 ID=%08x expected=%u CLASSREV=%08x CMD=%04x BUSES=%08x\n",
	       rp_id, rp_id == ICH10R_RP5_EXPECTED_ID, rp_class,
	       rp_command, buses);
	if (secondary == 0 || secondary == 0xff) {
		printk(BIOS_ERR,
		       "[NET] no usable secondary bus; NIC not touched\n");
		return;
	}
	{
		const pci_devfn_t nic = PCI_DEV(secondary, 0, 0);
		const uint32_t nic_id =
			pci_io_read_config32(nic, PCI_VENDOR_ID);
		const uint32_t nic_class =
			pci_io_read_config32(nic, PCI_CLASS_REVISION);
		const uint16_t nic_command =
			pci_io_read_config16(nic, PCI_COMMAND);
		const uint32_t bar0 = pci_io_read_config32(nic, PCI_BASE_ADDRESS_0);
		const uint32_t bar2 = pci_io_read_config32(nic,
			PCI_BASE_ADDRESS_0 + 8);
		const uint32_t bar4 = pci_io_read_config32(nic,
			PCI_BASE_ADDRESS_0 + 16);
		const uint32_t subsystem = pci_io_read_config32(nic,
			PCI_SUBSYSTEM_VENDOR_ID);

		printk(BIOS_INFO,
		       "[NET] %02x:00.0 ID=%08x expected=%u CLASSREV=%08x CMD=%04x SUBSYS=%08x BAR0=%08x BAR2=%08x BAR4=%08x\n",
		       secondary, nic_id, nic_id == RTL8168_EXPECTED_ID,
		       nic_class, nic_command, subsystem, bar0, bar2, bar4);
		printk(BIOS_INFO,
		       "[NET] no config/BAR/MMIO/reset/PHY/DMA write performed; TFTP transport remains gated\n");
	}
}

static void b06v6_print_mtrrs(void)
{
	const msr_t cap = rdmsr(MTRR_CAP_MSR);
	const msr_t def = rdmsr(MTRR_DEF_TYPE_MSR);
	const unsigned int count = cap.lo & MTRR_CAP_VCNT;

	printk(BIOS_INFO, "[MTRR] CAP=%08x:%08x DEF=%08x:%08x VCNT=%u\n",
	       cap.hi, cap.lo, def.hi, def.lo, count);
	for (unsigned int i = 0; i < count; i++) {
		const msr_t base = rdmsr(MTRR_PHYS_BASE(i));
		const msr_t mask = rdmsr(MTRR_PHYS_MASK(i));

		printk(BIOS_INFO,
		       "[MTRR] %02u BASE=%08x:%08x MASK=%08x:%08x\n",
		       i, base.hi, base.lo, mask.hi, mask.lo);
	}
}

static bool b06v6_postcar_mtrrs_are_exact(void)
{
	const msr_t cap = rdmsr(MTRR_CAP_MSR);
	const msr_t def = rdmsr(MTRR_DEF_TYPE_MSR);
	bool exact = cap.hi == 0 && cap.lo == B06V6_EXPECTED_MTRR_CAP &&
		def.hi == 0 && def.lo == B06V6_EXPECTED_MTRR_DEF;
	const unsigned int expected = ARRAY_SIZE(b06v6_expected_mtrr_base_lo);
	const unsigned int count = cap.lo & MTRR_CAP_VCNT;

	if (count < expected)
		exact = false;
	for (unsigned int i = 0; i < expected && i < count; i++) {
		const msr_t base = rdmsr(MTRR_PHYS_BASE(i));
		const msr_t mask = rdmsr(MTRR_PHYS_MASK(i));

		if (base.hi != 0 ||
		    base.lo != b06v6_expected_mtrr_base_lo[i] ||
		    mask.hi != B06V6_MTRR_HIGH_MASK ||
		    mask.lo != b06v6_expected_mtrr_mask_lo[i])
			exact = false;
	}
	for (unsigned int i = expected; i < count; i++) {
		if (rdmsr(MTRR_PHYS_MASK(i)).lo & MTRR_PHYS_MASK_VALID)
			exact = false;
	}
	if (!exact) {
		printk(BIOS_ERR,
		       "[MTRR] postcar mismatch CAP=%08x:%08x DEF=%08x:%08x expected=%u\n",
		       cap.hi, cap.lo, def.hi, def.lo, expected);
		b06v6_print_mtrrs();
	}

	return exact;
}

static void __noreturn b06v6_cf9_reset(bool full)
{
	const uint8_t post = full ? POST_B06V6_RESET_FULL :
		POST_B06V6_RESET_WARM;
	const uint8_t first = full ? 0x0a : 0x02;
	const uint8_t second = full ? 0x0e : 0x06;

	rammon.reset_armed = false;
	printk(BIOS_NOTICE,
	       "[RESET] request=%s CF9=%02x->%02x; WB cache writeback first\n",
	       full ? "full" : "warm", first, second);
	(void)b06v6_uart_flush();
	outb(post, CONFIG_POST_IO_PORT);
	wbinvd();
	asm volatile ("cli" ::: "memory");
	outb(first, B06V6_RST_CNT_PORT);
	outb(second, B06V6_RST_CNT_PORT);
	for (;;)
		asm volatile ("hlt");
}

static void b06v6_ramload(void)
{
	enum x58_rl_result result;
	size_t discarded;

	outb(POST_B06V6_RAMLOAD_ACTIVE, CONFIG_POST_IO_PORT);
	printk(BIOS_INFO,
	       "[RAMLOAD] binary mode XRL1; max=%08x window=%08x..%08x readback=required exec=disabled\n",
	       X58_B06V6_OBJECT_MAX_SIZE, X58_B06V6_OBJECT_BASE,
	       X58_B06V6_OBJECT_BASE + X58_B06V6_OBJECT_SIZE - 1);
	if (!b06v6_uart_flush()) {
		x58_rl_abort(&rammon.loader, X58_RL_ERR_TX);
		outb(POST_B06V6_RAMLOAD_FAILED, CONFIG_POST_IO_PORT);
		return;
	}
	if (!b06v6_uart_drain_command_tail(&discarded)) {
		x58_rl_abort(&rammon.loader, X58_RL_ERR_RX_FAULT);
		outb(POST_B06V6_RAMLOAD_FAILED, CONFIG_POST_IO_PORT);
		printk(BIOS_ERR,
		       "[RAMLOAD] command-tail drain failed; binary mode not entered\n");
		return;
	}
	if (discarded != 0) {
		printk(BIOS_INFO,
		       "[RAMLOAD] discarded %zu trailing command byte(s) before READY\n",
		       discarded);
		if (!b06v6_uart_flush()) {
			x58_rl_abort(&rammon.loader, X58_RL_ERR_TX);
			outb(POST_B06V6_RAMLOAD_FAILED, CONFIG_POST_IO_PORT);
			return;
		}
	}
	result = x58_rl_receive_serial(&rammon.loader, &b06v6_loader_policy,
		&b06v6_loader_sink, &b06v6_loader_serial,
		&b06v6_loader_limits);
	if (result == X58_RL_OK) {
		outb(POST_B06V6_RAMLOAD_COMPLETE, CONFIG_POST_IO_PORT);
		printk(BIOS_INFO,
		       "\n[RAMLOAD] complete id=%08x dst=%08x len=%08x crc=%08x; object is not executable\n",
		       rammon.loader.header.object_id,
		       rammon.loader.header.destination,
		       rammon.loader.header.length,
		       rammon.loader.header.payload_crc32);
	} else {
		outb(POST_B06V6_RAMLOAD_FAILED, CONFIG_POST_IO_PORT);
		printk(BIOS_ERR, "\n[RAMLOAD] failed=%s accepted=%08x\n",
		       x58_rl_result_name(result), rammon.loader.received);
	}
}

static void b06v6_command(int argc, char *argv[B06V6_ARGV_SIZE])
{
	uint32_t a, b, c, d;
	unsigned int width;

	if (argc == 0)
		return;
	if (!strcmp(argv[0], "help") && argc == 1) {
		b06v6_print_help();
		return;
	}
	if (!strcmp(argv[0], "id") && argc == 1) {
		printk(BIOS_INFO, "%s\n", B06V6_RAMMON_BUILD_ID);
		return;
	}
	if (!strcmp(argv[0], "handoff") && argc == 1) {
		b06v6_print_handoff();
		return;
	}
	if (!strcmp(argv[0], "cpuid") && (argc == 2 || argc == 3) &&
	    b06v6_parse_hex_u32(argv[1], &a) &&
	    (argc == 2 || b06v6_parse_hex_u32(argv[2], &b))) {
		const struct cpuid_result result = cpuid_ext(a, argc == 3 ? b : 0);

		printk(BIOS_INFO,
		       "CPUID %08x:%08x EAX=%08x EBX=%08x ECX=%08x EDX=%08x\n",
		       a, argc == 3 ? b : 0, result.eax, result.ebx,
		       result.ecx, result.edx);
		return;
	}
	if (!strcmp(argv[0], "mtrr") && argc == 1) {
		b06v6_print_mtrrs();
		return;
	}
	if (!strcmp(argv[0], "obj") && argc == 2 &&
	    !strcmp(argv[1], "status")) {
		b06v6_print_object();
		return;
	}
	if (!strcmp(argv[0], "obj") && argc == 2 &&
	    !strcmp(argv[1], "clear")) {
		x58_rl_initialize(&rammon.loader);
		printk(BIOS_INFO,
		       "[OBJECT] metadata invalidated; scratch bytes not erased\n");
		return;
	}
	if (!strcmp(argv[0], "obj") && argc == 2 &&
	    !strcmp(argv[1], "crc")) {
		if (!rammon.loader.loaded) {
			printk(BIOS_ERR, "[OBJECT] no complete object\n");
			return;
		}
		printk(BIOS_INFO, "[OBJECT] readback crc=%08x expected=%08x\n",
		       x58_rl_crc32((const void *)(uintptr_t)
			       rammon.loader.header.destination,
			       rammon.loader.header.length),
		       rammon.loader.header.payload_crc32);
		return;
	}
	if (!strcmp(argv[0], "obj") && argc == 4 &&
	    !strcmp(argv[1], "dump") &&
	    b06v6_parse_hex_u32(argv[2], &a) &&
	    b06v6_parse_hex_u32(argv[3], &b)) {
		b06v6_dump_object(a, b);
		return;
	}
	if (!strcmp(argv[0], "ramload") && argc == 1) {
		b06v6_ramload();
		return;
	}
	if (!strcmp(argv[0], "timer") && argc == 1) {
		b06v6_report_timer();
		return;
	}
	if (!strcmp(argv[0], "netprobe") && argc == 1) {
		b06v6_passive_netprobe();
		return;
	}
	if (!strcmp(argv[0], "pci") && argc == 6 &&
	    b06v6_parse_hex_u32(argv[1], &a) &&
	    b06v6_parse_hex_u32(argv[2], &b) &&
	    b06v6_parse_hex_u32(argv[3], &c) &&
	    b06v6_parse_hex_u32(argv[4], &d) &&
	    b06v6_parse_width(argv[5], &width) && a <= 0xff && b <= 0x1f &&
	    c <= 7 && d <= (CONFIG(PCI_IO_CFG_EXT) ? 0xfff : 0xff) &&
	    width - 1 <= (CONFIG(PCI_IO_CFG_EXT) ? 0xfff : 0xff) - d &&
	    !(d & (width - 1))) {
		printk(BIOS_INFO, "PCI %02x:%02x.%x %03x/%u = %08x\n",
		       a, b, c, d, width,
		       b06v6_pci_read(PCI_DEV(a, b, c), d, width));
		return;
	}
	if (!strcmp(argv[0], "io") && argc == 3 &&
	    b06v6_parse_hex_u32(argv[1], &a) &&
	    b06v6_parse_width(argv[2], &width) &&
	    a <= 0xffff && width <= 0x10000 - a) {
		printk(BIOS_INFO, "IO %04x/%u = %08x\n", a, width,
		       b06v6_io_read(a, width));
		return;
	}
	if (!strcmp(argv[0], "mem") && argc == 3 &&
	    b06v6_parse_hex_u32(argv[1], &a) &&
	    b06v6_parse_width(argv[2], &width) &&
	    a <= UINT32_MAX - (width - 1) && !(a & (width - 1))) {
		printk(BIOS_INFO, "MEM %08x/%u = %08x\n", a, width,
		       b06v6_mem_read(a, width));
		return;
	}
	if (!strcmp(argv[0], "msr") && argc == 2 &&
	    b06v6_parse_hex_u32(argv[1], &a)) {
		const msr_t result = rdmsr(a);

		printk(BIOS_INFO, "MSR %08x = %08x:%08x\n",
		       a, result.hi, result.lo);
		return;
	}
	if (!strcmp(argv[0], "unlock") && argc == 2 &&
	    !strcmp(argv[1], "RESET")) {
		rammon.reset_armed = true;
		printk(BIOS_NOTICE, "[RESET] armed for one reset command\n");
		return;
	}
	if (!strcmp(argv[0], "lock") && argc == 1) {
		rammon.reset_armed = false;
		x58_rl_cancel_execute(&rammon.loader);
		printk(BIOS_INFO, "LOCKED\n");
		return;
	}
	if (!strcmp(argv[0], "reset") && argc == 2 &&
	    (!strcmp(argv[1], "warm") || !strcmp(argv[1], "full"))) {
		if (!rammon.reset_armed) {
			printk(BIOS_ERR,
			       "[RESET] locked; first use: unlock RESET\n");
			return;
		}
		b06v6_cf9_reset(!strcmp(argv[1], "full"));
	}
	if (!strcmp(argv[0], "halt") && argc == 1) {
		outb(POST_B06V6_HALT, CONFIG_POST_IO_PORT);
		printk(BIOS_NOTICE, "[RAMMON] halted; external reset required\n");
		(void)b06v6_uart_flush();
		asm volatile ("cli" ::: "memory");
		for (;;)
			asm volatile ("hlt");
	}
	printk(BIOS_ERR, "ERR syntax/range; use help\n");
}

static void b06v6_rammon(void *unused)
{
	const struct cbmem_entry *entry;
	void *cbmem_base;
	size_t cbmem_size;
	uintptr_t cbmem_start;
	uintptr_t cbmem_end;
	char line[B06V6_LINE_SIZE];
	char *argv[B06V6_ARGV_SIZE];

	(void)unused;
	outb(POST_B06V6_RAMMON_ENTRY, CONFIG_POST_IO_PORT);
	if (!cbmem_online())
		b06v6_rammon_stop(POST_B06V6_RAMMON_HANDOFF_FAIL,
			"CBMEM_OFFLINE");
	entry = cbmem_entry_find(X58_B06V6_CBMEM_ID);
	if (entry == NULL || cbmem_entry_size(entry) < sizeof(*rammon.handoff) ||
	    cbmem_get_region(&cbmem_base, &cbmem_size))
		b06v6_rammon_stop(POST_B06V6_RAMMON_HANDOFF_FAIL,
			"HANDOFF_ENTRY");
	cbmem_start = (uintptr_t)cbmem_base;
	if (cbmem_start > UINTPTR_MAX - cbmem_size)
		b06v6_rammon_stop(POST_B06V6_RAMMON_HANDOFF_FAIL,
			"CBMEM_WRAP");
	cbmem_end = cbmem_start + cbmem_size;
	if (cbmem_size < sizeof(*rammon.handoff) ||
	    cbmem_start < X58_B06V6_CBMEM_BASE ||
	    cbmem_end != X58_B06V6_CBMEM_TOP || cbmem_end <= cbmem_start)
		b06v6_rammon_stop(POST_B06V6_RAMMON_HANDOFF_FAIL,
			"CBMEM_REGION");
	rammon.handoff = cbmem_entry_start(entry);
	if ((uintptr_t)rammon.handoff < X58_B06V6_CBMEM_BASE ||
	    (uintptr_t)rammon.handoff >
		X58_B06V6_CBMEM_TOP - sizeof(*rammon.handoff) ||
	    (uintptr_t)rammon.handoff < cbmem_start ||
	    (uintptr_t)rammon.handoff > cbmem_end - sizeof(*rammon.handoff) ||
	    !x58_b06v6_handoff_is_valid(rammon.handoff))
		b06v6_rammon_stop(POST_B06V6_RAMMON_HANDOFF_FAIL,
			"HANDOFF_CONTENT");
	if (b06v6_uart_base() != 0x3f8 || get_uart_baudrate() != 115200 ||
	    inb(b06v6_uart_base() + UART8250_LCR) != UART8250_LCR_WLS_8)
		b06v6_rammon_stop(POST_B06V6_RAMMON_HANDOFF_FAIL,
			"UART_NOT_115200_8N1");
	if (!b06v6_postcar_mtrrs_are_exact())
		b06v6_rammon_stop(POST_B06V6_RAMMON_HANDOFF_FAIL,
			"POSTCAR_MTRR_STATE");
	x58_rl_initialize(&rammon.loader);
	rammon.reset_armed = false;
	outb(POST_B06V6_RAMMON_READY, CONFIG_POST_IO_PORT);
#if CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
	printk(BIOS_NOTICE,
	       "\n[RAMSTAGE] %s\n"
	       "[RAMSTAGE] exact handoff valid; CBMEM is WB, payload path remains fail-closed\n",
	       B06V6_RAMMON_BUILD_ID);
#else
	printk(BIOS_NOTICE,
	       "\n[RAMSTAGE] %s\n"
	       "[RAMSTAGE] exact handoff valid; CBMEM is WB, object window remains UC\n"
	       "[RAMSTAGE] serial object transport ready at 115200 8N1; type help\n",
	       B06V6_RAMMON_BUILD_ID);
	b06v6_print_object();
#endif
	(void)b06v6_uart_flush();

#if CONFIG_X58_PRO_E_B06VF_SEABIOS_ENTRY
	post_code(POST_B06VF_RAMMON_RETURN);
	printk(BIOS_NOTICE,
		#if CONFIG_X58_PRO_E_SPD_SERIAL_INDEPENDENT
	       "[PAYLOAD] serial-independent SPD v10 handoff accepted; inherited platform/payload path follows\n");
		#elif CONFIG_X58_PRO_E_B06WK_ACPI_REPAIR
	       "[PAYLOAD] B06WK ACPI repair experiment; inherited USB/IRQ "
	       "path and repaired native OS descriptions follow\n");
		#elif CONFIG_X58_PRO_E_B06WJ_ACPI_PLATFORM
	       "[PAYLOAD] B06WJ ACPI platform experiment; inherited USB/IRQ "
	       "path and CPU/LPC/HPET tables follow\n");
		#elif CONFIG_X58_PRO_E_B06WI_VENDOR_IRQ_ACPI
	       "[PAYLOAD] B06WI experimental IRQ/ACPI route accepted; "
	       "automatic GPIO57 USB release and final payload gate follow\n");
		#elif CONFIG_X58_PRO_E_B06WH_QUIET_USB_SEABIOS
	       "[PAYLOAD] B06WH experimental ASSUMED_STABLE fast-postmem v9 handoff accepted; "
	       "automatic GPIO57 USB release and final payload gate follow\n");
		#elif CONFIG_X58_PRO_E_B06WG_AUTO_USB_SEABIOS
	       "[PAYLOAD] B06WG experimental ASSUMED_STABLE fast-postmem v9 handoff accepted; "
	       "automatic GPIO57 USB release and final payload gate follow\n");
		#elif CONFIG_X58_PRO_E_B06WF_USB_LAB
	       "[PAYLOAD] B06WF exact v9 handoff accepted; late USB diagnostic lab "
	       "runs before ACPI and inherited B06WE payload path\n");
		#elif CONFIG_X58_PRO_E_B06WE_ACPI_SAD_BDF_FIX
	       "[PAYLOAD] B06WE exact v9 handoff accepted; corrected SAD/PCIEXBAR "
	       "ACPI gate follows inherited B06WD platform init\n");
		#elif CONFIG_X58_PRO_E_B06WD_SEABIOS_INPUT
	       "[PAYLOAD] B06WD exact v9 handoff accepted; SATA policy corrected, "
	       "late KBC/PS2 and inherited USB trace enabled\n");
		#elif CONFIG_X58_PRO_E_B06WC_INTEGRATED_PLATFORM
	       "[PAYLOAD] B06WC integrated v9 handoff accepted; PIC/ELCR, "
	       "quiet ACPI mode/tables, USB admission and diagnostic IRQ probe included\n");
		#elif CONFIG_X58_PRO_E_B06WB_TCO_HALT
	       "[PAYLOAD] B06WB exact v9 handoff accepted; early exact TCO halt "
	       "precedes the inherited B06WA platform path\n");
		#elif CONFIG_X58_PRO_E_B06WA_HPET_DECODE
	       "[PAYLOAD] B06WA exact v9 handoff accepted; B06VZ resources precede "
	       "quiescent ICH10 HPET decode/resource metadata\n");
		#elif CONFIG_X58_PRO_E_B06VZ_FIXED_RESOURCES
	       "[PAYLOAD] B06VZ exact v9 handoff accepted; B06VY IOAPIC mask "
	       "precedes fixed resource-only platform metadata\n");
	#elif CONFIG_X58_PRO_E_B06VY_IOAPIC_MASK
	       "[PAYLOAD] B06VY exact v9 handoff accepted; B06VX AHCI/USB trace "
	       "precedes deterministic masked IOAPIC initialization\n");
	#elif CONFIG_X58_PRO_E_B06VX_AHCI_USB_TRACE
	       "[PAYLOAD] B06VX exact v9 handoff accepted; B06VW AHCI handoff "
	       "precedes bounded deferred SeaBIOS USB tracing\n");
	#elif CONFIG_X58_PRO_E_B06VW_ICH10_AHCI_MMIO
	       "[PAYLOAD] B06VW exact v9 handoff accepted; B06VU route and B06VV "
	       "PCS/SCLK precede minimal post-allocation AHCI MMIO handoff\n");
	#elif CONFIG_X58_PRO_E_B06VV_ICH10_PCS_SCLK
	       "[PAYLOAD] B06VV exact v9 handoff accepted; B06VU corrected route "
	       "precedes post-allocation PCS/SCLK programming\n");
	#elif CONFIG_X58_PRO_E_B06VU_ICH10_AHCI_ROUTE
	       "[PAYLOAD] B06VU exact v9 handoff accepted; corrected ICH10 AHCI route "
	       "deferred to baseline-first selective PCI preflight\n");
	#elif CONFIG_X58_PRO_E_B06VQ_ICHBASE1
	       "[PAYLOAD] B06VQ-ICHBASE1 exact v9 handoff accepted; retaining B06VQ hardware policy; "
	       "six-field ICH10 baseline deferred to selective PCI preflight\n");
	#elif CONFIG_X58_PRO_E_B06VQ_PLATRO1
	       "[PAYLOAD] B06VQ-PLATRO1 exact v9 handoff accepted; retaining B06VQ hardware policy; "
	       "read-only platform census deferred until the PCI enable audit passes\n");
	#elif CONFIG_X58_PRO_E_B06VQ_USB_TRACE1
	       "[PAYLOAD] B06VQ-USBTRACE1 exact v9 handoff accepted; retaining B06VQ USB setup; "
	       "deferred SeaBIOS USB trace and 1000-ms attach interval selected\n");
	#elif CONFIG_X58_PRO_E_B06VT_ICH10_SATA_CLOCK
	       "[PAYLOAD] B06VT exact v9 handoff accepted; retaining B06VS AHCI port policy; "
	       "required SATA clock-field programming is deferred until the allocation audit passes\n");
#elif CONFIG_X58_PRO_E_B06VS_ICH10_AHCI_PORTS
	       "[PAYLOAD] B06VS exact v9 handoff accepted; retaining B06VR AHCI mode; "
	       "ICH10 AHCI port enablement is deferred until the allocation audit passes\n");
#elif CONFIG_X58_PRO_E_B06VR_ICH10_AHCI_MAP
	       "[PAYLOAD] B06VR exact v9 handoff accepted; retaining B06VQ USB setup; "
	       "ICH10 AHCI MAP selection is deferred to selective PCI preflight\n");
#elif CONFIG_X58_PRO_E_B06VQ_ICH10_EHCI_INIT
	       "[PAYLOAD] B06VQ exact v9 handoff accepted; retaining B06VP IRQ setup; "
	       "ICH10 EHCI initialization is deferred to selective PCI preflight\n");
#elif CONFIG_X58_PRO_E_B06VP_LAPIC_EXTINT
	       "[PAYLOAD] B06VP exact v9 handoff accepted; retaining B06VO routing; "
	       "LAPIC virtual-wire setup is deferred to CPU-cluster init\n");
#elif CONFIG_X58_PRO_E_B06VO_IOHBUSNO_ROUTE
	       "[PAYLOAD] B06VO exact v9 handoff accepted; repairing IOH config routing before selective PCI enumeration\n");
#elif CONFIG_X58_PRO_E_B06VN_IOU0_PCIE_PHYS_VBIOS
	       "[PAYLOAD] B06VN exact v9 handoff accepted; starting IOU0 before selective PCI enumeration\n");
#elif CONFIG_X58_PRO_E_B06VM_AUTO_PCI_VGA_IPXE
	       "[PAYLOAD] B06VM exact v9 handoff accepted; continuing to selective PCI enumeration\n");
#elif CONFIG_X58_PRO_E_B06VL_BROAD_HARD_GATE_SEABIOS
	       "[PAYLOAD] B06VL broad hard-gated v9 handoff accepted; leaving DRAM ROMMON callback\n");
#elif CONFIG_X58_PRO_E_B06VK_Q_CANONICAL_CLASS_SEABIOS
	       "[PAYLOAD] B06VK profile-Q canonical-class v8 handoff accepted; leaving DRAM ROMMON callback\n");
#elif CONFIG_X58_PRO_E_B06VJ_COUPLED_PROFILE_Q_SEABIOS
	       "[PAYLOAD] B06VJ coupled profile v7 handoff accepted; leaving DRAM ROMMON callback\n");
#elif CONFIG_X58_PRO_E_B06VI_COUPLED_PROFILE_O_SEABIOS
	       "[PAYLOAD] B06VI coupled profile v6 handoff accepted; leaving DRAM ROMMON callback\n");
#elif CONFIG_X58_PRO_E_B06VH_PRIMARY_WORKSPACE_REARM_SEABIOS
	       "[PAYLOAD] B06VH PRIMARY v5 handoff accepted; leaving DRAM ROMMON callback\n");
#elif CONFIG_X58_PRO_E_B06VG_COUPLED_MINIT_SEABIOS
	       "[PAYLOAD] B06VG PRIMARY handoff accepted; leaving DRAM ROMMON callback\n");
#else
	       "[PAYLOAD] B06VF handoff accepted; leaving DRAM ROMMON callback\n");
#endif
	(void)b06v6_uart_flush();
	return;
#endif

	for (;;) {
		int argc;

		printk(BIOS_INFO, "x58-dram> ");
		(void)b06v6_uart_flush();
		if (!b06v6_read_line(line))
			continue;
		argc = b06v6_tokenize(line, argv);
		if (argc < 0) {
			printk(BIOS_ERR, "ERR too many arguments\n");
			continue;
		}
		b06v6_command(argc, argv);
	}
}

BOOT_STATE_INIT_ENTRY(BS_PRE_DEVICE, BS_ON_ENTRY, b06v6_rammon, NULL);

#if CONFIG_X58_PRO_E_B06WF_USB_LAB
/*
 * This second monitor is intentionally late.  BS_WRITE_TABLES/ON_ENTRY is
 * after dev_initialize()/dev_finalize() and before write_tables(), so the
 * operator observes final coreboot USB resources without entering ACPI or the
 * payload.  The early handoff monitor above still returns normally.
 */
static void b06wf_late_usb_monitor(void *unused)
{
	char line[B06V6_LINE_SIZE];
	char *argv[B06V6_ARGV_SIZE];

	(void)unused;
	b06wf_usb_lab_begin();
	(void)b06v6_uart_flush();
	for (;;) {
		int argc;

		printk(BIOS_INFO, "x58-usb> ");
		(void)b06v6_uart_flush();
		if (!b06v6_read_line(line))
			continue;
		argc = b06v6_tokenize(line, argv);
		if (argc < 0) {
			printk(BIOS_ERR, "ERR too many arguments\n");
			continue;
		}
		if (argc == 1 && !strcmp(argv[0], "continue")) {
			if (b06wf_usb_lab_can_continue())
				return;
			continue;
		}
		if (argc == 1 && !strcmp(argv[0], "lock")) {
			b06wf_usb_lab_lock();
			b06v6_command(argc, argv);
			continue;
		}
		if (b06wf_usb_lab_command(argc, argv))
			continue;
		b06v6_command(argc, argv);
	}
}

BOOT_STATE_INIT_ENTRY(BS_WRITE_TABLES, BS_ON_ENTRY,
	b06wf_late_usb_monitor, NULL);
#endif
