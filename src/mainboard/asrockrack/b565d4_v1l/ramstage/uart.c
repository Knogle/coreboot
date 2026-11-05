/* SPDX-License-Identifier: GPL-2.0-only */
#include "native84_profile.h"
#include <arch/io.h>
#include <drivers/uart/uart8250reg.h>

#if ENV_RAMSTAGE
#include <stdbool.h>

static uint32_t uart_ff_retries;
static uint32_t uart_dropped_bytes;
static uint32_t uart_dropped_flushes;
static uint32_t uart_full_wait_caps;
static uint32_t uart_recoveries;
static bool uart_degraded;
static bool uart_muted;

static void count_event(uint32_t *value)
{
	if (*value != UINT32_MAX)
		(*value)++;
}

uint32_t native197_uart_ff_retries(void);
uint32_t native198_uart_dropped_bytes(void);
uint32_t native198_uart_dropped_flushes(void);
uint32_t native198_uart_full_wait_caps(void);
uint32_t native198_uart_recoveries(void);
uint32_t native198_uart_degraded(void);
void native208_uart_mute(bool mute);

uint32_t native197_uart_ff_retries(void)
{
	return uart_ff_retries;
}

uint32_t native198_uart_dropped_bytes(void)
{
	return uart_dropped_bytes;
}

uint32_t native198_uart_dropped_flushes(void)
{
	return uart_dropped_flushes;
}

uint32_t native198_uart_full_wait_caps(void)
{
	return uart_full_wait_caps;
}

uint32_t native198_uart_recoveries(void)
{
	return uart_recoveries;
}

uint32_t native198_uart_degraded(void)
{
	return uart_degraded;
}

/* Only the admitted RAM DXIO quiet phase controls this CPU-only latch. */
void native208_uart_mute(bool mute)
{
	uart_muted = mute;
	__asm__ __volatile__("" ::: "memory");
}

/* One existing full poll cap on a healthy console. After a failure, probe
 * only once per TX/flush call until that call's valid ready mask returns.
 * These are finite operation caps, not calibrated wall-clock deadlines.
 */
static bool ready(unsigned int idx, uint8_t mask)
{
	uint32_t left;

	native84_stage_check();
	if (idx != 1)
		native84_terminal();
	if (uart_muted)
		return false;
	left = uart_degraded ? 1 : native84_profile_policy.uart_max_polls;
	while (left--) {
		uint8_t status = inb(0x2f8 + UART8250_LSR);

		if (status == 0xff) {
			count_event(&uart_ff_retries);
			continue;
		}
		if ((status & mask) == mask) {
			if (uart_degraded)
				count_event(&uart_recoveries);
			uart_degraded = false;
			return true;
		}
	}
	if (!uart_degraded)
		count_event(&uart_full_wait_caps);
	uart_degraded = true;
	return false;
}

void __wrap_uart_tx_byte(unsigned int idx, unsigned char byte);
void __wrap_uart_tx_byte(unsigned int idx, unsigned char byte)
{
	if (!ready(idx, UART8250_LSR_THRE)) {
		count_event(&uart_dropped_bytes);
		return;
	}
	outb(byte, 0x2f8 + UART8250_TBR);
}

void __wrap_uart_tx_flush(unsigned int idx);
void __wrap_uart_tx_flush(unsigned int idx)
{
	if (!ready(idx, UART8250_LSR_TEMT))
		count_event(&uart_dropped_flushes);
}
#else
/* Poll counts are finite operation caps, not calibrated wall-clock deadlines. */
static void ready(unsigned int idx, uint8_t mask)
{
	uint32_t left;

	native84_stage_check();
	if (idx != 1)
		native84_terminal();
	left = native84_profile_policy.uart_max_polls;
	while (left--) {
		uint8_t status = inb(0x2f8 + UART8250_LSR);

		if (status == 0xff)
			native84_terminal();
		if ((status & mask) == mask)
			return;
	}
	native84_terminal();
}

void __wrap_uart_tx_byte(unsigned int idx, unsigned char byte);
void __wrap_uart_tx_byte(unsigned int idx, unsigned char byte)
{
	ready(idx, UART8250_LSR_THRE);
	outb(byte, 0x2f8 + UART8250_TBR);
}

void __wrap_uart_tx_flush(unsigned int idx);
void __wrap_uart_tx_flush(unsigned int idx)
{
	ready(idx, UART8250_LSR_TEMT);
}
#endif
