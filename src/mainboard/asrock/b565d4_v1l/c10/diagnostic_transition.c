/* SPDX-License-Identifier: GPL-2.0-only */
/* Early C10 transition: no bootblock console/timer/FCH initializer required. */

#include <arch/io.h>
#include <stdint.h>

#define UART_BASE 0x2f8
#define STAGE_BASE 0x09d00000
#define STAGE_MAX_SIZE 0x70000

struct diagnostic_stage_descriptor {
	uint32_t magic;
	uint32_t version;
	uint32_t entry;
	uint32_t file_size;
	uint32_t memory_size;
	uint32_t crc32;
};

void __noreturn raw_enter_embedded_diagnostic_romstage(void);

static void __noreturn raw_halt(uint8_t code)
{
	outb(code, 0x80);
	for (;;)
		asm volatile ("cli; hlt");
}

static void raw_text(const char *text)
{
	while (*text) {
		unsigned int attempts;

		for (attempts = 0; attempts < 100000; attempts++) {
			const uint8_t status = inb(UART_BASE + 5);

			if (status != 0xff && (status & 0x20)) {
				outb(*text++, UART_BASE);
				break;
			}
		}
		if (attempts == 100000)
			raw_halt(0xeb);
	}
}

static void flush_raw_uart(void)
{
	for (unsigned int i = 0; i < 100000; i++) {
		const uint8_t status = inb(UART_BASE + 5);

		/* Require THRE and TEMT, avoiding open-bus status. */
		if (status != 0xff && (status & 0x60) == 0x60)
			return;
	}
	raw_halt(0xeb);
}

static uint32_t stage_crc32(const volatile uint8_t *bytes, uint32_t size)
{
	uint32_t crc = UINT32_MAX;

	while (size--) {
		crc ^= *bytes++;
		for (unsigned int bit = 0; bit < 8; bit++)
			crc = (crc >> 1) ^ (0xedb88320 & -(crc & 1));
	}
	return ~crc;
}

void __noreturn raw_enter_embedded_diagnostic_romstage(void)
{
	const volatile struct diagnostic_stage_descriptor *stage = (void *)0x09c80000;
	const uint32_t magic = stage->magic;
	const uint32_t version = stage->version;
	const uint32_t entry = stage->entry;
	const uint32_t file_size = stage->file_size;
	const uint32_t memory_size = stage->memory_size;
	const uint32_t crc = stage->crc32;

	if (magic != 0x52344d41 || version != 1 || entry != STAGE_BASE ||
	    file_size == 0 || file_size > memory_size || memory_size > STAGE_MAX_SIZE ||
	    stage_crc32((void *)STAGE_BASE, memory_size) != crc) {
		raw_text("\r\nC10 embedded romstage descriptor/checksum invalid.\r\n");
		raw_halt(0xe9);
	}
	raw_text("\r\nB565D4-V1L embedded diagnostic romstage C10; transfer 09d00000.\r\n");
	flush_raw_uart();
	outb(0xc3, 0x80);
	((void (*)(void))(uintptr_t)STAGE_BASE)();
	raw_text("\r\nC10 diagnostic romstage unexpectedly returned.\r\n");
	raw_halt(0xea);
}
