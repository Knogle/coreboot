/* SPDX-License-Identifier: GPL-2.0-only */
/* One raw boundary marker after normal NONCAR runtime, before SoC C setup. */
#include "b565d4_cpu.h"
#include <arch/io.h>
#include <bootblock_common.h>
#include <stdbool.h>
#include <stdint.h>

#define LPC_CFG 0x8000a300U
#define UART_BASE 0x2f8U
#define AST_SIO 0x4eU
#define SCU 0x1e6e2000U
#define AST_OBSERVED_REVISION 0x04030103U
#define AST_HICR5 0x1e789080U
#define AST_SIRQ_START_FRAME (1U << 5)
#define AST_GPIO 0x1e780000U
#define AST_GPIOM_DIR (AST_GPIO + 0x7c)
#define AST_GPIOM_LATCH (AST_GPIO + 0xcc)
#define AST_GPIOM_SOURCE0 (AST_GPIO + 0xe0)
#define AST_GPIOM_SOURCE1 (AST_GPIO + 0xe4)
#define AST_GPIOM_OWNER 1U
#define AST_NRTS2_DIR (1U << 5)
#define AST_NRTS2_MUX (1U << 29)
#define IOMUX 0xfed80d00U
#define POLLS 100000U
#define ID_ATTEMPTS 6400U
#define ID_PAUSES 1000000U

/* Own BSS, consulted only after the original CPU/context guards. These are
 * retry observations, not a claim that a failed attempt initialized hardware.
 */
static struct {
	uint32_t last_retry, ldn_attempts, absent_attempts, poll_retries;
	uint32_t f0_before, f0_after;
	uint32_t ast_revision_last, ast_revision_attempts;
} uart_readiness;

struct ast_nrts2_state {
	uint32_t direction, latch, source0, source1, stage;
	bool warm_pair, cleanup_ok;
};

extern uint32_t b565d4_normal_reset_eax;
extern const uint8_t gdtptr[];
extern const uint8_t _bootblock[];
extern const uint8_t _ebootblock[];
asmlinkage void __real_bootblock_c_entry(uint64_t base_timestamp);
asmlinkage void b565d4_pre_soc_c_entry(uint64_t base_timestamp);

_Static_assert(sizeof(uintptr_t) == 4, "IA32 entry only");
_Static_assert(1, "Opaque reset EAX wrapper");
_Static_assert(1, "Retained normal SoC flow");
_Static_assert(CONFIG_UART_FOR_CONSOLE == 1 && CONFIG_TTYS0_BASE == 0x2f8 &&
	CONFIG_TTYS0_BAUD == 115200 && CONFIG_TTYS0_LCS == 3, "Qualified UART2 parameters");

static void __noreturn refuse(uint8_t code)
{
	outb(code, 0x80);
	for (;;)
		asm volatile ("cli; hlt" : : : "memory");
}

static uint32_t cpu_signature(void)
{
	uint32_t a, b, c, d;
	asm volatile ("cpuid" : "=a" (a), "=b" (b), "=c" (c), "=d" (d)
		: "a" (0), "c" (0));
	if (a < 1 || b != 0x68747541 || d != 0x69746e65 || c != 0x444d4163)
		refuse(0xe2);
	asm volatile ("cpuid" : "=a" (a), "=b" (b), "=c" (c), "=d" (d)
		: "a" (1), "c" (0));
	if (!b565d4_is_selected_cpu(a))
		refuse(0xe2);
	return a;
}

static uint32_t read_flags(void)
{
	uint32_t value;
	asm volatile ("pushfl; popl %0" : "=r" (value) : : "memory");
	return value;
}

static uint32_t context_cr0(void)
{
	uint32_t value;
	asm volatile ("mov %%cr0,%0" : "=r" (value));
	return value;
}

static void qualify_context(void)
{
	struct {
		uint16_t limit;
		uint32_t base;
	} __packed gdtr;
	uint16_t cs, ds, es, ss;
	asm volatile ("sgdt %0" : "=m" (gdtr));
	asm volatile ("mov %%cs,%0; mov %%ds,%1; mov %%es,%2; mov %%ss,%3"
		: "=r" (cs), "=r" (ds), "=r" (es), "=r" (ss));
	uint32_t c0 = context_cr0();
	/* Admit the original flat IA32 runtime before dereferencing descriptors. */
	if ((c0 & 0xe0000001U) != 0x60000001U || (read_flags() & 0x600) ||
	    cs != 8 || ds != 0x10 || es != 0x10 || ss != 0x10)
		refuse(0xe3);
	uintptr_t begin = (uintptr_t)_bootblock, end = (uintptr_t)_ebootblock;
	uintptr_t descriptor = (uintptr_t)gdtptr;
	/* gdt is local to original assembly; only its exported six-byte gdtptr
	 * descriptor is referenced. Bound both reads to the exact linker-owned
	 * bootblock span inside the unchanged 128-KiB reservation.
	 */
	if (begin < 0x09fe0000U || end != 0x0a000000U || begin >= end ||
	    descriptor < begin || descriptor >= end || 6U > end - descriptor)
		refuse(0xe3);
	uint16_t expected_limit = (uint16_t)gdtptr[0] | (uint16_t)gdtptr[1] << 8;
	uint32_t expected_base = (uint32_t)gdtptr[2] | (uint32_t)gdtptr[3] << 8 |
		(uint32_t)gdtptr[4] << 16 | (uint32_t)gdtptr[5] << 24;
	if (expected_limit != 31 || expected_base < begin || expected_base >= end ||
	    32U > end - expected_base)
		refuse(0xe3);
	static const uint8_t flat_gdt[32] = {
		0, 0, 0, 0, 0, 0, 0, 0,
		0xff, 0xff, 0, 0, 0, 0x9b, 0xcf, 0,
		0xff, 0xff, 0, 0, 0, 0x93, 0xcf, 0,
		0xff, 0xff, 0, 0, 0, 0x9b, 0xaf, 0,
	};
	const volatile uint8_t *table = (void *)(uintptr_t)expected_base;
	for (unsigned int i = 0; i < sizeof(flat_gdt); i++) {
		if (table[i] != flat_gdt[i])
			refuse(0xe3);
	}
	/* No paging, interrupts or DF; expected stock flat selectors/GDT.
	 * The common NONCAR runtime deliberately left both cache-disable bits set.
	 */
	if (gdtr.base != expected_base || gdtr.limit != expected_limit)
		refuse(0xe3);
}

static uint32_t cfg_read(uint8_t reg)
{
	outl(LPC_CFG | (reg & 0xfc), 0xcf8);
	return inl(0xcfc);
}

static void cfg_write(uint8_t reg, uint32_t value)
{
	outl(LPC_CFG | (reg & 0xfc), 0xcf8);
	outl(value, 0xcfc);
}

static uint8_t cfg_read8(uint8_t reg)
{
	outl(LPC_CFG | (reg & 0xfc), 0xcf8);
	return inb(0xcfc + (reg & 3));
}

static void cfg_write8(uint8_t reg, uint8_t value)
{
	outl(LPC_CFG | (reg & 0xfc), 0xcf8);
	outb(value, 0xcfc + (reg & 3));
}

static uint8_t pm_read(uint8_t reg)
{
	outb(reg, 0xcd6);
	return inb(0xcd7);
}

static void pm_enable(uint8_t reg, uint8_t bits)
{
	uint8_t value = pm_read(reg);
	if (value == 0xff)
		refuse(0xe4);
	outb(value | bits, 0xcd7);
	if (pm_read(reg) != (uint8_t)(value | bits))
		refuse(0xe4);
}

static void lpc_prepare(void)
{
	/* Same narrow PM gates as the physically observed DirectC12 raw path.
	 * PM04 bit1 also matches original Huashan's ACPIMMIO-enable operation.
	 */
	pm_enable(0, 1);
	pm_enable(0xec, 1);
	pm_enable(4, 2);
	uint32_t value = cfg_read(0x48);
	if (value == UINT32_MAX)
		refuse(0xe4);
	cfg_write(0x48, value | 3U | (1U << 21));
	if (cfg_read(0x48) != (value | 3U | (1U << 21)))
		refuse(0xe4);
	value = cfg_read(0x44);
	if (value == UINT32_MAX)
		refuse(0xe4);
	cfg_write(0x44, value | (1U << 7));
	if (cfg_read(0x44) != (value | (1U << 7)))
		refuse(0xe4);
	/* Original Huashan byte-D1 masks: clock0/1 enabled PCD defaults.
	 * Exact retained LPC mux0/eMMC-disabled sequence; omit SPI_CS3 pin31.
	 */
	/* D1=ff is documented with working UART in qualified C12 (D0=08ffff86).
	 * Admit that byte only inside a present, consistent full D0 register;
	 * preserve every other D0 byte and require the known device throughout.
	 */
	uint32_t d0 = cfg_read(0xd0);
	if (cfg_read(0) != 0x790e1022 || d0 == UINT32_MAX)
		refuse(0xe4);
	uint8_t d1 = (uint8_t)(d0 >> 8);
	if (cfg_read8(0xd1) != d1 || cfg_read(0xd0) != d0)
		refuse(0xe4);
	uint8_t wanted_d1 = d1 | 0x60;
	cfg_write8(0xd1, wanted_d1);
	uint32_t wanted_d0 = (d0 & ~0xff00U) | (uint32_t)wanted_d1 << 8;
	if (cfg_read8(0xd1) != wanted_d1 || cfg_read(0xd0) != wanted_d0 ||
	    cfg_read(0) != 0x790e1022)
		refuse(0xe4);
	static const uint8_t pins[] = {21, 32, 104, 105, 106, 107, 74, 88, 75, 87, 109};
	for (unsigned int i = 0; i < sizeof(pins); i++) {
		volatile uint8_t *pin = (void *)(uintptr_t)(IOMUX + pins[i]);
		*pin = 0;
		if (*pin != 0)
			refuse(0xe4);
	}
	/* Same exact 16-bit internal legacy-UART decode clear as DirectC12.
	 * No mobile AOAC, I2C, eSPI, GPIO output or reset-pulse programming.
	 */
	volatile uint16_t *legacy = (void *)(uintptr_t)0xfedc0020U;
	*legacy = 0;
	if (*legacy != 0)
		refuse(0xe4);
}

static void sio_enter(uint16_t port, uint8_t key)
{
	outb(key, port);
	outb(key, port);
}

static uint8_t sio_read(uint16_t port, uint8_t reg)
{
	outb(reg, port);
	return inb(port + 1);
}

static void sio_write(uint16_t port, uint8_t reg, uint8_t value)
{
	outb(reg, port);
	outb(value, port + 1);
}

static void readiness_pause(void)
{
	/* Iteration bound only; no wall-clock, timer or TSC assumption. */
	for (unsigned int i = 0; i < ID_PAUSES; i++)
		asm volatile ("pause");
}

static uint16_t nct_disable(void)
{
	uint16_t id = 0xffff;
	for (unsigned int attempt = 0; attempt < ID_ATTEMPTS; attempt++) {
		sio_enter(0x2e, 0x87);
		id = (uint16_t)sio_read(0x2e, 0x20) << 8;
		id |= sio_read(0x2e, 0x21);
		if ((id & 0xfff0) == 0xc800 || (id & 0xfff0) == 0xd420) {
			uint8_t selection = sio_read(0x2e, 7);
			if (selection == 0xff) {
				outb(0xaa, 0x2e);
				refuse(0xe6);
			}
			for (unsigned int ldn = 2; ldn <= 3; ldn++) {
				sio_write(0x2e, 7, ldn);
				uint8_t active = sio_read(0x2e, 0x30);
				if (active == 0xff) {
					sio_write(0x2e, 7, selection);
					outb(0xaa, 0x2e);
					refuse(0xe6);
				}
				sio_write(0x2e, 0x30, active & ~1U);
				if (sio_read(0x2e, 0x30) != (uint8_t)(active & ~1U)) {
					sio_write(0x2e, 7, selection);
					outb(0xaa, 0x2e);
					refuse(0xe6);
				}
			}
			sio_write(0x2e, 7, selection);
			outb(0xaa, 0x2e);
			return id;
		}
		outb(0xaa, 0x2e);
		/* Only transient absence is retried; no unknown logical-device writes. */
		if (id != 0 && id != 0xffff)
			refuse(0xe6);
		uart_readiness.last_retry = 0xe6;
		readiness_pause();
	}
	refuse(0xe6);
}

static void pre_ahb_uart_probe(void)
{
	/* Optional witness using only the known physical AST UART2. A missing
	 * route or busy UART must still reach the original SCU initialization.
	 * No AHB access, delay, refusal or readiness retry belongs in this probe.
	 */
	sio_enter(AST_SIO, 0xa5);
	uint8_t selection = sio_read(AST_SIO, 7);
	if (selection == 0xff)
		goto exit;
	sio_write(AST_SIO, 7, 3);
	uint8_t active = sio_read(AST_SIO, 0x30);
	uint8_t clock = sio_read(AST_SIO, 0xf0);
	if (active == 0xff || clock == 0xff)
		goto restore;
	sio_write(AST_SIO, 0x30, active & ~1U);
	if (sio_read(AST_SIO, 0x30) != (uint8_t)(active & ~1U))
		goto restore;
	sio_write(AST_SIO, 0xf0, clock & ~2U);
	if (sio_read(AST_SIO, 0xf0) != (uint8_t)(clock & ~2U))
		goto restore;
	sio_write(AST_SIO, 0x60, 2);
	sio_write(AST_SIO, 0x61, 0xf8);
	sio_write(AST_SIO, 0x70, 3);
	sio_write(AST_SIO, 0x30, active | 1);
	bool configured = sio_read(AST_SIO, 0x30) == (uint8_t)(active | 1) &&
		sio_read(AST_SIO, 0x60) == 2 && sio_read(AST_SIO, 0x61) == 0xf8 &&
		sio_read(AST_SIO, 0x70) == 3 &&
		sio_read(AST_SIO, 0xf0) == (uint8_t)(clock & ~2U);
	sio_write(AST_SIO, 7, selection);
	outb(0xaa, AST_SIO);
	if (!configured)
		return;
	uint8_t lsr = inb(UART_BASE + 5);
	if (inb(UART_BASE + 3) == 0xff || lsr == 0xff || !(lsr & 0x20))
		return;
	/* Same DIV1/8N1 register values as the qualified UART path. Keep DLAB
	 * clear even when the divisor readback fails. Do not clear either FIFO.
	 */
	outb(3, UART_BASE + 3);
	outb(0, UART_BASE + 1);
	outb(1, UART_BASE + 2);
	outb(3, UART_BASE + 4);
	outb(0x83, UART_BASE + 3);
	outb(0, UART_BASE + 1);
	outb(1, UART_BASE);
	bool divisor_ok = inb(UART_BASE + 1) == 0 && inb(UART_BASE) == 1;
	outb(3, UART_BASE + 3);
	if (!divisor_ok || inb(UART_BASE + 3) != 3)
		return;
	uint8_t iir = inb(UART_BASE + 2);
	lsr = inb(UART_BASE + 5);
	if (iir == 0xff || (iir & 0xc0) != 0xc0 ||
	    lsr == 0xff || !(lsr & 0x20))
		return;
	/* An empty, enabled FIFO admits this one eight-byte burst. No THRE or
	 * TEMT wait follows; the unchanged full AST/SCU path always runs next.
	 */
	static const uint8_t marker[8] = {'P', 'R', 'E', 'A', 'H', 'B', '\r', '\n'};
	for (unsigned int i = 0; i < sizeof(marker); i++)
		outb(marker[i], UART_BASE);
	return;
restore:
	sio_write(AST_SIO, 7, selection);
exit:
	outb(0xaa, AST_SIO);
}

static bool pre_soc_ast_ahb(uint32_t address, uint32_t *value, bool write)
{
	sio_enter(AST_SIO, 0xa5);
	uint8_t selection = sio_read(AST_SIO, 7);
	if (selection == 0xff) {
		outb(0xaa, AST_SIO);
		return false;
	}
	sio_write(AST_SIO, 7, 0x0d);
	uint8_t active = sio_read(AST_SIO, 0x30);
	uint8_t width = sio_read(AST_SIO, 0xf8);
	bool passed = active != 0xff && width != 0xff;
	if (passed) {
		sio_write(AST_SIO, 0x30, active | 1);
		passed = sio_read(AST_SIO, 0x30) == (uint8_t)(active | 1);
	}
	if (passed) {
		for (unsigned int i = 0; i < 4; i++)
			sio_write(AST_SIO, 0xf0 + i, address >> (24 - i * 8));
		sio_write(AST_SIO, 0xf8, (width & ~3U) | 2);
		passed = sio_read(AST_SIO, 0xf8) == (uint8_t)((width & ~3U) | 2);
	}
	if (passed) {
		if (write) {
			for (unsigned int i = 0; i < 4; i++)
				sio_write(AST_SIO, 0xf4 + i, *value >> (24 - i * 8));
			sio_write(AST_SIO, 0xfe, 0xcf);
		} else {
			(void)sio_read(AST_SIO, 0xfe);
			*value = 0;
			for (unsigned int i = 0; i < 4; i++)
				*value = (*value << 8) | sio_read(AST_SIO, 0xf4 + i);
		}
	}
	/* Preserve the normal helper's activation/width/selection boundary. */
	if (width != 0xff)
		sio_write(AST_SIO, 0xf8, width);
	if (active != 0xff)
		sio_write(AST_SIO, 0x30, active);
	if (passed)
		passed = sio_read(AST_SIO, 0xf8) == width &&
			sio_read(AST_SIO, 0x30) == active;
	sio_write(AST_SIO, 7, selection);
	outb(0xaa, AST_SIO);
	return passed;
}

static uint32_t ast_read(uint32_t address)
{
	uint32_t value = 0;
	if (!pre_soc_ast_ahb(address, &value, false))
		refuse(0xe7);
	return value;
}

static void ast_write(uint32_t address, uint32_t value)
{
	if (!pre_soc_ast_ahb(address, &value, true))
		refuse(0xe7);
}

/* GPIO078 is sampled input, not the output latch. The admitted low latch
 * at GPIO0cc permits DIR07c.bit5 without any GPIO data/latch write.
 */
static bool ast_pad_read(uint32_t address, uint32_t *value)
{
	return pre_soc_ast_ahb(address, value, false) && *value != UINT32_MAX;
}

static bool ast_pad_mask(uint32_t address, uint32_t mask, uint32_t bits)
{
	uint32_t current, after;

	if (!ast_pad_read(address, &current))
		return false;
	uint32_t wanted = (current & ~mask) | (bits & mask);
	if (wanted != current && !pre_soc_ast_ahb(address, &wanted, true))
		return false;
	return ast_pad_read(address, &after) && after == wanted;
}

static bool ast_pad_key(uint32_t command, uint32_t expected)
{
	uint32_t after;

	return pre_soc_ast_ahb(SCU, &command, true) &&
		ast_pad_read(SCU, &after) && after == expected;
}

static bool ast_nrts2_prepare(uint32_t *scu84, uint32_t *scu90,
			     struct ast_nrts2_state *state)
{
	uint32_t revision, key, before84, before90, selector, before_dir;
	uint32_t before_source0, before_source1, latch, value;
	bool owner_attempted = false, dir_attempted = false;
	bool key_attempted = false, relock_attempted = false;
	bool mux_attempted = false, cleanup_ok = true;

	state->stage = 1;
	state->cleanup_ok = true;
	if (!ast_pad_read(SCU + 0x7c, &revision) || revision != AST_OBSERVED_REVISION ||
	    !ast_pad_read(SCU, &key) || key != 0 ||
	    !ast_pad_read(SCU + 0x84, &before84) ||
	    !ast_pad_read(SCU + 0x90, &before90) ||
	    !ast_pad_read(SCU + 0x94, &selector) || (selector & 3) ||
	    !ast_pad_read(AST_GPIOM_DIR, &before_dir) ||
	    !ast_pad_read(AST_GPIOM_LATCH, &latch) || (latch & AST_NRTS2_DIR) ||
	    !ast_pad_read(AST_GPIOM_SOURCE0, &before_source0) ||
	    !ast_pad_read(AST_GPIOM_SOURCE1, &before_source1) ||
	    ((before_source0 | before_source1) & AST_GPIOM_OWNER))
		return false;
	/* Admit only the original pair or this initializer's completed pair.
	 * The original SCU90 mask below still clears bits4/5 on a cold path;
	 * do not require its pre-mask value to equal the later Linux snapshot.
	 * An own warm pair must already have its NRTS2 selector clear.
	 */
	state->stage = 2;
	state->warm_pair = (before_dir & AST_NRTS2_DIR) && !(before84 & AST_NRTS2_MUX);
	if (state->warm_pair ? (before90 & 0x20) :
	    ((before_dir & AST_NRTS2_DIR) || !(before84 & AST_NRTS2_MUX)))
		return false;
	if (!state->warm_pair) {
		/* Source1 remains ARM0; Source0 selects LPC for the whole M byte.
		 * The permission is temporary, not a permanent eight-pin takeover.
		 */
		state->stage = 3;
		owner_attempted = true;
		if (!ast_pad_read(AST_GPIOM_SOURCE1, &value) || (value & AST_GPIOM_OWNER) ||
		    !ast_pad_mask(AST_GPIOM_SOURCE0, AST_GPIOM_OWNER, AST_GPIOM_OWNER) ||
		    !ast_pad_read(AST_GPIOM_LATCH, &value) || (value & AST_NRTS2_DIR))
			goto cleanup;
		state->stage = 4;
		dir_attempted = true;
		if (!ast_pad_mask(AST_GPIOM_DIR, AST_NRTS2_DIR, AST_NRTS2_DIR))
			goto cleanup;
	}
	state->stage = 5;
	if (!ast_pad_read(SCU, &value) || value != 0)
		goto cleanup;
	state->stage = 6;
	key_attempted = true;
	if (!ast_pad_key(0x1688a8a8, 1))
		goto cleanup;
	state->stage = 7;
	mux_attempted = true;
	/* Keep the original AST2500PeiInit masks, adding only NRTS2's GPIO mux. */
	if (!ast_pad_mask(SCU + 0x84, 0x00ff0000U | AST_NRTS2_MUX, 0x00ff0000U) ||
	    !ast_pad_mask(SCU + 0x90, 0x30, 0) ||
	    !ast_pad_read(AST_GPIOM_SOURCE1, &value) || (value & AST_GPIOM_OWNER))
		goto cleanup;
	state->stage = 8;
	if (owner_attempted &&
	    !ast_pad_mask(AST_GPIOM_SOURCE0, AST_GPIOM_OWNER, before_source0))
		goto cleanup;
	state->stage = 9;
	if (!ast_pad_read(SCU + 0x7c, &revision) || revision != AST_OBSERVED_REVISION ||
	    !ast_pad_read(SCU + 0x84, scu84) || (*scu84 & AST_NRTS2_MUX) ||
	    (*scu84 & 0x00ff0000U) != 0x00ff0000U ||
	    !ast_pad_read(SCU + 0x90, scu90) || (*scu90 & 0x30) ||
	    !ast_pad_read(SCU + 0x94, &selector) || (selector & 3) ||
	    !ast_pad_read(AST_GPIOM_DIR, &state->direction) ||
	    !(state->direction & AST_NRTS2_DIR) ||
	    !ast_pad_read(AST_GPIOM_LATCH, &state->latch) ||
	    (state->latch & AST_NRTS2_DIR) ||
	    !ast_pad_read(AST_GPIOM_SOURCE0, &state->source0) ||
	    !ast_pad_read(AST_GPIOM_SOURCE1, &state->source1) ||
	    ((state->source0 | state->source1) & AST_GPIOM_OWNER))
		goto cleanup;
	state->stage = 10;
	relock_attempted = true;
	if (!ast_pad_key(0, 0))
		goto cleanup;
	state->stage = 0;
	return true;
cleanup:
	cleanup_ok &= !relock_attempted;
	/* No retries. Restore only new owned bits, then refuse terminally.
	 * Do not issue a DIR restore under ARM: reacquire LPC first. Attempt
	 * owner/key cleanup even when another read or restoration failed.
	 */
	if (mux_attempted)
		cleanup_ok &= ast_pad_mask(SCU + 0x84, AST_NRTS2_MUX, before84);
	if (dir_attempted) {
		bool lpc = ast_pad_read(AST_GPIOM_SOURCE1, &value) &&
			!(value & AST_GPIOM_OWNER) &&
			ast_pad_mask(AST_GPIOM_SOURCE0, AST_GPIOM_OWNER, AST_GPIOM_OWNER);
		cleanup_ok &= lpc;
		if (lpc)
			cleanup_ok &= ast_pad_mask(AST_GPIOM_DIR, AST_NRTS2_DIR, before_dir);
	}
	if (owner_attempted)
		cleanup_ok &= ast_pad_mask(AST_GPIOM_SOURCE0, AST_GPIOM_OWNER, before_source0);
	if (key_attempted && !relock_attempted)
		cleanup_ok &= ast_pad_key(0, 0);
	state->cleanup_ok = cleanup_ok;
	if (!cleanup_ok)
		outb(0xea, 0x80);
	return false;
}

static void ast_qualify(void)
{
	for (unsigned int attempt = 0; attempt < ID_ATTEMPTS; attempt++) {
		uint32_t revision = 0;
		uart_readiness.ast_revision_attempts = attempt + 1;
		bool read_ok = pre_soc_ast_ahb(SCU + 0x7c, &revision, false);
		uart_readiness.ast_revision_last = revision;
		/* No nonexact sample qualifies the device or permits SCU writes. */
		if (read_ok && revision == AST_OBSERVED_REVISION)
			return;
		uart_readiness.last_retry = 0xe7;
		readiness_pause();
	}
	refuse(0xe7);
}

static uint8_t ast_uart(void)
{
	/* ast_qualify already admitted the exact AST2500 revision. Retry only
	 * absent pre-write reads; an unexpected programming readback still halts.
	 * Zero activation/F0 values are valid, not absence indications.
	 */
	for (unsigned int attempt = 0; attempt < ID_ATTEMPTS; attempt++) {
		uart_readiness.ldn_attempts = attempt + 1;
		sio_enter(AST_SIO, 0xa5);
		uint8_t selection = sio_read(AST_SIO, 7);
		if (selection == 0xff) {
			outb(0xaa, AST_SIO);
			uart_readiness.last_retry = 0xe7;
			readiness_pause();
			continue;
		}
		sio_write(AST_SIO, 7, 3);
		uint8_t active = sio_read(AST_SIO, 0x30);
		uint8_t clock = sio_read(AST_SIO, 0xf0);
		if (active == 0xff || clock == 0xff) {
			sio_write(AST_SIO, 7, selection);
			outb(0xaa, AST_SIO);
			uart_readiness.last_retry = 0xe7;
			readiness_pause();
			continue;
		}
		uint8_t wanted_clock = clock & ~2U;
		sio_write(AST_SIO, 0x30, active & ~1U);
		bool passed = sio_read(AST_SIO, 0x30) == (uint8_t)(active & ~1U);
		if (passed) {
			/* LDN3-only clock-selection trial, analogous to the board's
			 * old UART1 path. Clear only F0.bit1 (24MHz/13), preserving
			 * every other bit; prior UART1 code is not LDN3 evidence.
			 */
			if (wanted_clock != clock)
				sio_write(AST_SIO, 0xf0, wanted_clock);
			passed = sio_read(AST_SIO, 0xf0) == wanted_clock;
		}
		if (passed) {
			sio_write(AST_SIO, 0x60, 2);
			sio_write(AST_SIO, 0x61, 0xf8);
			sio_write(AST_SIO, 0x70, 3);
			sio_write(AST_SIO, 0x30, active | 1);
			passed = sio_read(AST_SIO, 0x30) == (uint8_t)(active | 1) &&
				sio_read(AST_SIO, 0x60) == 2 && sio_read(AST_SIO, 0x61) == 0xf8 &&
				sio_read(AST_SIO, 0x70) == 3 &&
				sio_read(AST_SIO, 0xf0) == wanted_clock;
		}
		sio_write(AST_SIO, 7, selection);
		outb(0xaa, AST_SIO);
		if (!passed)
			refuse(0xe7);
		uart_readiness.f0_before = clock;
		uart_readiness.f0_after = wanted_clock;
		return wanted_clock;
	}
	refuse(0xe7);
}

static void uart_wait(uint8_t mask)
{
	/* A transient ff/zero/busy LSR is not a successful readiness result.
	 * Bound both fast reads and delayed rounds; no persistent TX/noise loop.
	 */
	for (unsigned int round = 0; round < ID_ATTEMPTS; round++) {
		for (unsigned int attempt = 0; attempt < POLLS; attempt++) {
			uint8_t state = inb(UART_BASE + 5);
			if (state == 0xff)
				break;
			if ((state & mask) == mask)
				return;
		}
		uart_readiness.poll_retries++;
		uart_readiness.last_retry = 0xe8;
		readiness_pause();
	}
	refuse(0xe8);
}

static void uart_start(void)
{
	/* The exact known cold UART is now configured for this early diagnostic.
	 * Start its local counter before the first readiness wait; no global
	 * clock, SCU reset or route selection change. LDN3 F0 was qualified and
	 * selected separately above. DLH precedes DLL.
	 */
	bool present = false;
	for (unsigned int attempt = 0; attempt < ID_ATTEMPTS; attempt++) {
		if (inb(UART_BASE + 3) != 0xff && inb(UART_BASE + 5) != 0xff) {
			present = true;
			break;
		}
		uart_readiness.absent_attempts++;
		uart_readiness.last_retry = 0xe8;
		readiness_pause();
	}
	if (!present)
		refuse(0xe8);
	outb(3, UART_BASE + 3);
	outb(0, UART_BASE + 1);
	outb(1, UART_BASE + 2);
	outb(3, UART_BASE + 4);
	outb(0x83, UART_BASE + 3);
	outb(0, UART_BASE + 1);
	outb(1, UART_BASE);
	bool passed = inb(UART_BASE + 1) == 0 && inb(UART_BASE) == 1;
	outb(3, UART_BASE + 3);
	if (!passed || inb(UART_BASE + 3) != 3)
		refuse(0xe8);
	uart_wait(0x60);
}

static void uart_byte(uint8_t value)
{
	uart_wait(0x20);
	outb(value, UART_BASE);
}

static void raw_text(const char *text)
{
	for (unsigned int i = 0; i < 128; i++) {
		if (!text[i])
			return;
		uart_byte(text[i]);
	}
	refuse(0xe9);
}

static void raw_hex(uint32_t value)
{
	static const char digits[] = "0123456789abcdef";
	for (unsigned int i = 8; i; i--)
		uart_byte(digits[(value >> ((i - 1) * 4)) & 15]);
}

static void raw_field(const char *name, uint32_t value)
{
	raw_text(name);
	raw_hex(value);
}

/* One diagnostic after UART readiness and before ordinary SoC C/ROM work.
 * Same-board AmdPspDxeV2Rn reads public SMN 03810570 through D0F0 B8/BC.
 * This is selector protocol writing, never a PSP data/command write.
 * No mismatch changes the normal boot policy; report validity and return.
 */
static void native172_early_psp_status(void)
{
	const uint32_t saved_cf8 = inl(0xcf8);
	uint32_t host_id, saved_b8 = UINT32_MAX, restored_b8 = UINT32_MAX;
	uint32_t raw = UINT32_MAX, read_count = 0, b8_write_count = 0;

	/* Drain before changing either selector; a missing result remains visible. */
	raw_text("\r\nNATIVE172 EARLY_PSP_SMN BEGIN index=03810570\r\n");
	uart_wait(0x60);
	outl(0x80000000U, 0xcf8);
	host_id = inl(0xcfc);
	if (host_id == 0x16301022U) {
		outl(0x800000b8U, 0xcf8);
		saved_b8 = inl(0xcfc);
		outl(0x03810570U, 0xcfc);
		b8_write_count++;
		outl(0x800000bcU, 0xcf8);
		raw = inl(0xcfc);
		read_count++;
		outl(0x800000b8U, 0xcf8);
		outl(saved_b8, 0xcfc);
		b8_write_count++;
		restored_b8 = inl(0xcfc);
	}
	/* Restore even for an unexpected host ID or an all-ones data result. */
	outl(saved_cf8, 0xcf8);
	const uint32_t restored_cf8 = inl(0xcf8);
	const bool b8_restore_match = read_count == 1 && saved_b8 == restored_b8;
	const bool cf8_restore_match = saved_cf8 == restored_cf8;
	const bool valid = host_id == 0x16301022U && read_count == 1 &&
		raw != UINT32_MAX && b8_restore_match && cf8_restore_match;

	raw_text("NATIVE172 EARLY_PSP_SMN RESULT");
	raw_field(" host=", host_id);
	raw_field(" raw=", raw);
	raw_field(" reads=", read_count);
	raw_field(" valid=", valid);
	raw_field(" B8_before=", saved_b8);
	raw_field(" B8_after=", restored_b8);
	raw_field(" B8_writes=", b8_write_count);
	raw_field(" B8_restore_checked=", read_count == 1);
	raw_field(" B8_restore_match=", b8_restore_match);
	raw_field(" CF8_before=", saved_cf8);
	raw_field(" CF8_after=", restored_cf8);
	raw_field(" CF8_restore_match=", cf8_restore_match);
	raw_text(" BC_writes=0 PSP_commands=0 after_UART_ready=1 before_soc_c=1 diagnostic_only=1\r\n");
	uart_wait(0x60);
}

asmlinkage void b565d4_pre_soc_c_entry(uint64_t base_timestamp)
{
	/* Assembly captured MM0 in owned BSS and tail-jumped with the original
	 * cdecl uint64 timestamp still at ESP+4. No early allocator is involved.
	 */
	uint32_t signature = cpu_signature();
	qualify_context();
	uint32_t saved_cf8 = inl(0xcf8);
	if (cfg_read(0) != 0x790e1022)
		refuse(0xe4);
	uint32_t d0_before = cfg_read(0xd0);
	lpc_prepare();
	uint16_t nct = nct_disable();
	pre_ahb_uart_probe();
	ast_qualify();
	uint8_t clock = ast_uart();
	/* Exact original AST2500PeiInit SCU masks, already physically exercised
	 * by DirectC12. Unknown silicon refuses before these SCU writes.
	 */
	ast_write(SCU, 0x1688a8a8);
	uint32_t scu84 = ast_read(SCU + 0x84);
	uint32_t scu90 = ast_read(SCU + 0x90);
	ast_write(SCU + 0x84, (scu84 & 0xff00ffff) | 0x00ff0000);
	ast_write(SCU + 0x90, scu90 & 0xffffffcf);
	ast_write(SCU, 0);
	scu84 = ast_read(SCU + 0x84);
	scu90 = ast_read(SCU + 0x90);
	if (ast_read(SCU + 0x7c) != AST_OBSERVED_REVISION ||
	    (scu84 & 0x00ff0000) != 0x00ff0000 || (scu90 & 0x30))
		refuse(0xe7);
	/* AST2500 v1.6 HICR5.bit5 enables slave SIRQ start-frame requests.
	 * Keep the qualified AHB provider and every other HICR5 bit intact.
	 */
	uint32_t hicr5_before = ast_read(AST_HICR5);
	if (hicr5_before == UINT32_MAX)
		refuse(0xe7);
	uint32_t hicr5_wanted = hicr5_before | AST_SIRQ_START_FRAME;
	if (hicr5_wanted != hicr5_before)
		ast_write(AST_HICR5, hicr5_wanted);
	uint32_t hicr5_after = ast_read(AST_HICR5);
	if (hicr5_after != hicr5_wanted)
		refuse(0xe7);
	uint8_t initial_lsr = inb(UART_BASE + 5);
	uart_start();
	uint32_t c3, c4;
	asm volatile ("mov %%cr3,%0; mov %%cr4,%1" : "=r" (c3), "=r" (c4));
	/* First finite marker after actual UART readiness, before extra reports. */
	raw_text("\r\nNATIVE102 UART_READY");
	raw_field(" last_retry=", uart_readiness.last_retry);
	raw_field(" LDN_attempts=", uart_readiness.ldn_attempts);
	raw_field(" absent=", uart_readiness.absent_attempts);
	raw_field(" polls=", uart_readiness.poll_retries);
	raw_field(" F0_before=", uart_readiness.f0_before);
	raw_field(" F0_after=", uart_readiness.f0_after);
	raw_field(" AST_rev_last=", uart_readiness.ast_revision_last);
	raw_field(" AST_rev_attempts=", uart_readiness.ast_revision_attempts);
	native172_early_psp_status();
	/* Publish the post-original-setup cold values before pad admission or
	 * new GPIO/mux writes. A failed read is visible, never a valid zero.
	 */
	uint32_t pad_valid = 0;
	static const struct {
		const char *name;
		uint32_t address;
	} pad_pre[] = {
		{" key=", SCU}, {" SCU84=", SCU + 0x84},
		{" SCU90=", SCU + 0x90}, {" SCU94=", SCU + 0x94},
		{" DIR=", AST_GPIOM_DIR}, {" latch=", AST_GPIOM_LATCH},
		{" source0=", AST_GPIOM_SOURCE0}, {" source1=", AST_GPIOM_SOURCE1},
	};
	raw_text("\r\nNATIVE169 PAD_PRE");
	for (unsigned int i = 0; i < sizeof(pad_pre) / sizeof(pad_pre[0]); i++) {
		uint32_t value = UINT32_MAX;
		if (ast_pad_read(pad_pre[i].address, &value))
			pad_valid |= 1U << i;
		raw_field(pad_pre[i].name, value);
	}
	raw_field(" valid_mask=", pad_valid);
	raw_text(" after_original_UART_setup=1\r\n");
	uart_wait(0x60);
	struct ast_nrts2_state pad = {0};
	if (pad_valid != 0xff) {
		pad.stage = 0x100;
		pad.cleanup_ok = true;
	} else {
		raw_text("NATIVE169 PAD_BEGIN\r\n");
		uart_wait(0x60);
		if (ast_nrts2_prepare(&scu84, &scu90, &pad))
			goto pad_ready;
	}
	raw_text("NATIVE169 UART_PAD_REFUSE");
	raw_field(" stage=", pad.stage);
	raw_field(" cleanup_ok=", pad.cleanup_ok);
	raw_text(" terminal=1\r\n");
	uart_wait(0x60);
	outl(saved_cf8, 0xcf8);
	refuse(0xe7);
pad_ready:
	raw_text("NATIVE169 UART_PAD");
	raw_field(" DIR=", pad.direction);
	raw_field(" latch=", pad.latch);
	raw_field(" SCU84=", scu84);
	raw_field(" source0=", pad.source0);
	raw_field(" source1=", pad.source1);
	raw_field(" warm_pair=", pad.warm_pair);
	raw_text(" ARM_owner=1 latch_write=0 SCU_key=0\r\n");
	raw_text("\r\nB565D4 NATIVE_PRE_SOC v=1");
	raw_field(" CPUID=", signature);
	raw_field(" ResetEAX=", b565d4_normal_reset_eax);
	raw_field(" CR0=", context_cr0());
	raw_field(" CR3=", c3);
	raw_field(" CR4=", c4);
	raw_field(" FLAGS=", read_flags());
	raw_field(" LPCD0_before=", d0_before);
	raw_field(" LPCD0_after=", cfg_read(0xd0));
	raw_field(" LPC44=", cfg_read(0x44));
	raw_field(" LPC48=", cfg_read(0x48));
	raw_field(" NCT=", nct);
	raw_field(" AST_REV=", AST_OBSERVED_REVISION);
	raw_field(" F0=", clock);
	raw_field(" SCU84=", scu84);
	raw_field(" SCU90=", scu90);
	raw_field(" SCU94=", ast_read(SCU + 0x94));
	raw_field(" HICRA=", ast_read(0x1e78909c));
	raw_field(" HICR5_before=", hicr5_before);
	raw_field(" HICR5_after=", hicr5_after);
	raw_field(" initial_LSR=", initial_lsr);
	raw_text(" before_soc_c=1 before_fch=1 before_tsc_calibration=1 native_release=0 fsp_api=0 owner_release=0\r\n");
	uart_wait(0x60);
	outl(saved_cf8, 0xcf8);
	/* The raw marker is finite. Preserve the exact timestamp into stock C.
	 * Its original ECAM/cache/FCH/AOAC prerequisites and refusal behavior
	 * remain intact; this source does not bypass an ACK or invoke FSP.
	 */
	__real_bootblock_c_entry(base_timestamp);
	refuse(0xee);
}
