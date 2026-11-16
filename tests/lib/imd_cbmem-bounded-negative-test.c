/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Numeric preflight checks without a test-owned CBMEM arena. The production
 * validator must reject each case before using the supplied address.
 */
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <commonlib/bsd/helpers.h>
#include <cbmem.h>
#include <imd.h>
#include <bootmem.h>
#include <boot/coreboot_tables.h>

/* The coreboot stdio/stdlib headers intentionally omit host test I/O. */
extern int printf(const char *fmt, ...);
extern __noreturn void exit(int status);
extern int cbmem_initialized;

static uintptr_t lower;
static uintptr_t upper;
static struct cbmem_imd_handle_storage bank;
static int bank_calls;
static int hook_calls;
static int ramcheck_calls;
static int checks;
static int die_calls;
static int expect_die_active;
static char die_message[128];
static jmp_buf die_jump;

#define CHECK(cond)                                                    \
	do {                                                           \
		if (!(cond)) {                                         \
			printf("FAIL line %d: %s\n", __LINE__, #cond); \
			exit(2);                                       \
		}                                                      \
		checks++;                                              \
	} while (0)

void die(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vsnprintf(die_message, sizeof(die_message), fmt, args);
	va_end(args);
	die_calls++;
	if (expect_die_active)
		longjmp(die_jump, 1);
	printf("unexpected halt: %s\n", die_message);
	exit(3);
}

int printk(int level, const char *fmt, ...)
{
	(void)level;
	(void)fmt;
	return 0;
}

void mock_assert(const int result, const char *const expression, const char *const file,
		 const int line)
{
	(void)result;
	printf("unexpected assertion %s:%d %s\n", file, line, expression);
	exit(4);
}

uintptr_t cbmem_top_chipset(void)
{
	return upper;
}

uintptr_t cbmem_bottom_chipset(void)
{
	return lower;
}

struct cbmem_imd_handle_storage cbmem_imd_handle_storage_chipset(void)
{
	bank_calls++;
	return bank;
}

void quick_ram_check_or_die(uintptr_t dst)
{
	(void)dst;
	ramcheck_calls++;
}

void cbmem_run_init_hooks(int is_recovery)
{
	(void)is_recovery;
	hook_calls++;
}

static void empty_now(void)
{
	cbmem_initialize_empty();
}

#if TEST_EXTERNAL
static void find_now(void)
{
	(void)cbmem_find(0xa001);
}
#endif

static void expect_rejection(void (*fn)(void), const char *message)
{
	int before = die_calls;
	die_message[0] = '\0';
	expect_die_active = 1;
	if (setjmp(die_jump) == 0) {
		fn();
		printf("FAIL expected die: %s\n", message);
		exit(5);
	}
	expect_die_active = 0;
	CHECK(die_calls == before + 1);
	CHECK(strstr(die_message, message) != NULL);
	CHECK(hook_calls == 0);
	CHECK(ramcheck_calls == 0);
	CHECK(cbmem_initialized == 0);
}

static void invalid_arena(uintptr_t lo, uintptr_t hi)
{
	int before = bank_calls;
	lower = lo;
	upper = hi;
	_cbmem_top_ptr = hi;
	expect_rejection(empty_now, "Invalid bounded CBMEM arena");
	CHECK(bank_calls == before);
}

#if TEST_EXTERNAL
static void invalid_bank(struct cbmem_imd_handle_storage candidate, const char *message)
{
	int before = bank_calls;
	bank = candidate;
	expect_rejection(find_now, message);
	CHECK(bank_calls == before + 1);
}
#endif

int main(int argc, char **argv)
{
	static const struct {
		uintptr_t lower;
		uintptr_t upper;
	} bad_arenas[] = {
		{0,          0x140000                 },
		{0x100001,   0x140000                 },
		{0x100000,   0x140001                 },
		{0x100000,   0x100000                 },
		{0x140000,   0x100000                 },
		{0x100000,   0x101000                 },
		{0x7ffff000, 0x80001000               },
#if __SIZEOF_POINTER__ > 4
		{0x100000,   (uintptr_t)UINT32_MAX + 1},
#endif
	};
#if TEST_EXTERNAL
	const struct cbmem_imd_handle_storage bad_banks[] = {
		{(struct imd *)(uintptr_t)0x200000,   0,               0x1000},
		{(struct imd *)(uintptr_t)0x200000,   0x200000,        0     },
		{(struct imd *)(uintptr_t)0x200000,   UINTPTR_MAX - 8, 32    },
		{(struct imd *)(uintptr_t)0x100000,   0x100000,        0x1000},
		{(struct imd *)(uintptr_t)0x200001,   0x200000,        0x1000},
		{(struct imd *)(uintptr_t)0x200ff8,   0x200000,        0x1000},
		{(struct imd *)(uintptr_t)0x1ffff0,   0x200000,        0x1000},
		{(struct imd *)(uintptr_t)0x7ffff000, 0x7ffff000,      0x2000},
#if __SIZEOF_POINTER__ > 4
		{(struct imd *)(uintptr_t)0xfffff000, 0xfffff000,      0x2000},
#endif
	};
#endif
	size_t case_count = ARRAY_SIZE(bad_arenas);
	unsigned int index = 0;
	const char *arg;
#if TEST_EXTERNAL
	case_count += ARRAY_SIZE(bad_banks);
#endif
	if (argc == 2 && !strcmp(argv[1], "--count")) {
		printf("%zu\n", case_count);
		return 0;
	}
	if (argc == 1) {
		size_t i;
		const size_t arena_cases = ENV_CREATES_CBMEM ? 1 : ARRAY_SIZE(bad_arenas);
		for (i = 0; i < arena_cases; i++)
			invalid_arena(bad_arenas[i].lower, bad_arenas[i].upper);
#if TEST_EXTERNAL
		lower = 0x100000;
		upper = 0x140000;
		_cbmem_top_ptr = upper;
		for (i = 0; i < ARRAY_SIZE(bad_banks); i++)
			invalid_bank(bad_banks[i],
				     i < 3 ? "Invalid CBMEM handle bank" :
					     "Invalid external CBMEM handle storage");
#endif
		printf("PASS all checks=%d rejected=%d bank_calls=%d\n", checks, die_calls,
		       bank_calls);
		return 0;
	}
	if (argc != 2 || !argv[1][0])
		return 6;
	for (arg = argv[1]; *arg; arg++) {
		unsigned int digit;
		if (*arg < '0' || *arg > '9')
			return 6;
		digit = *arg - '0';
		if (index > (UINT_MAX - digit) / 10)
			return 6;
		index = index * 10 + digit;
	}
	if (index < ARRAY_SIZE(bad_arenas)) {
		invalid_arena(bad_arenas[index].lower, bad_arenas[index].upper);
#if TEST_EXTERNAL
	} else if (index - ARRAY_SIZE(bad_arenas) < ARRAY_SIZE(bad_banks)) {
		lower = 0x100000;
		upper = 0x140000;
		_cbmem_top_ptr = upper;
		invalid_bank(bad_banks[index - ARRAY_SIZE(bad_arenas)],
			     index - ARRAY_SIZE(bad_arenas) < 3 ?
				     "Invalid CBMEM handle bank" :
				     "Invalid external CBMEM handle storage");
#endif
	} else {
		return 6;
	}
	printf("PASS case=%u checks=%d rejected=%d bank_calls=%d\n", index, checks, die_calls,
	       bank_calls);
	return 0;
}
