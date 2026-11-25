/* SPDX-License-Identifier: GPL-2.0-only */
#include "b565d4_cpu.h"
#include "b565d4_memory.h"
#include <arch/cpu.h>
#include <arch/io.h>
#include <console/uart.h>
#include <cpu/amd/mtrr.h>
#include <cpu/x86/msr.h>
#include <halt.h>
#include <symbols.h>
#include "normal_cold_apob.h"
#include "native99_apob_select.h"
#if CONFIG(SOC_AMD_CEZANNE_NATIVE)
#include "normal_native99.h"
#endif

DECLARE_REGION(bss)

#define COLD_APOB_BASE 0x0a200000U
#define COLD_APOB_BYTES 16U
#define FLAGS_IF_DF 0x600U

struct cold_apob_gdtr {
	uint16_t limit;
	uint32_t base;
} __packed;

/* Exact public assembly symbol, not a caller-supplied GDT pointer. */
extern const struct cold_apob_gdtr gdtptr;

struct cold_apob_snapshot {
	uint32_t status, cpuid, cr0, flags;
	uint16_t cs, ds, es, ss;
	struct cold_apob_gdtr gdtr;
	uint16_t observed;
	msr_t syscfg, tom;
	uint8_t raw[COLD_APOB_BYTES];
};
static struct cold_apob_snapshot cold_apob;
_Static_assert(sizeof(uintptr_t) == 4, "IA32 native witness");
_Static_assert(sizeof(struct cold_apob_gdtr) == 6, "IA32 SGDT operand");
_Static_assert(sizeof(struct cold_apob_snapshot) == 64, "Fixed own BSS budget");
_Static_assert(CONFIG_PSP_APOB_DRAM_ADDRESS == COLD_APOB_BASE &&
	CONFIG_PSP_APOB_DRAM_RESERVED_SIZE == 0x28000, "Unchanged APOB protection");

/* Status0 means only that the fixed16B read completed. It is not an APOB
 * payload, version, NVS, FSP or memory-owner acceptance result. */
enum cold_apob_status {
	COLD_APOB_READ_COMPLETE,
	COLD_APOB_FLAGS,
	COLD_APOB_CPU,
	COLD_APOB_MODE,
	COLD_APOB_SEGMENTS,
	COLD_APOB_GDT,
	COLD_APOB_SME,
	COLD_APOB_TOM,
};
enum cold_apob_observed {
	OBS_FLAGS_SEGMENTS = 1U << 0,
	OBS_CPUID = 1U << 1,
	OBS_CR0 = 1U << 2,
	OBS_GDTR = 1U << 3,
	OBS_SYSCFG = 1U << 4,
	OBS_TOM = 1U << 5,
	OBS_FIXED16 = 1U << 6,
};

static int own_program(uintptr_t p, size_t n)
{
	uintptr_t first = (uintptr_t)_program, end = (uintptr_t)_eprogram;
	return end > first && p >= first && p <= end && n <= end - p;
}

static void text(const char *p)
{
	for (unsigned int i = 0; i < 128 && p[i]; i++)
		uart_tx_byte(1, p[i]);
}

static void hex(uint64_t value, unsigned int bytes)
{
	static const char digits[] = "0123456789abcdef";
	for (unsigned int n = bytes * 2; n; n--)
		uart_tx_byte(1, digits[(value >> ((n - 1) * 4)) & 15]);
}

static void report(void)
{
	text("B565D4 COLD_APOB CONTEXT status="); hex(cold_apob.status, 4);
	text(" observed="); hex(cold_apob.observed, 2);
	text(" cpuid="); hex(cold_apob.cpuid, 4);
	text(" cr0="); hex(cold_apob.cr0, 4);
	text(" flags="); hex(cold_apob.flags, 4);
	text(" cs="); hex(cold_apob.cs, 2);
	text(" ds="); hex(cold_apob.ds, 2);
	text(" es="); hex(cold_apob.es, 2);
	text(" ss="); hex(cold_apob.ss, 2); text("\r\n");
	text("B565D4 COLD_APOB DECODE gdtr_base="); hex(cold_apob.gdtr.base, 4);
	text(" gdtr_limit="); hex(cold_apob.gdtr.limit, 2);
	text(" syscfg="); hex(cold_apob.syscfg.raw, 8);
	text(" tom="); hex(cold_apob.tom.raw, 8); text("\r\n");
	if (cold_apob.observed & OBS_FIXED16) {
		text("B565D4 COLD_APOB HEADER base=0a200000 bytes=00000010 raw=");
		for (unsigned int i = 0; i < COLD_APOB_BYTES; i++)
			hex(cold_apob.raw[i], 1);
		text(" owner_release=0\r\n");
	}
	text("B565D4 COLD_APOB END status="); hex(cold_apob.status, 4);
	text(" source_bytes="); hex(cold_apob.observed & OBS_FIXED16 ? 16 : 0, 4);
	text(" complete="); hex(cold_apob.observed & OBS_FIXED16 ? 1 : 0, 4);
	text(" owner_release=0\r\n");
	uart_tx_flush(1);
}

#define COLD_APOB_DATA_LIMIT NATIVE99_APOB_MAX_BYTES
#define COLD_APOB_DATA_CHUNK 48U

/* Native222 memory-only baseline: the captured16B v18/f8 header supplies
 * one bounded length. Every copy/repeat/report and native handoff remains tied
 * to that same header; no new in-range header is accepted as unchanged.
 */
static const volatile uint32_t cold_data_enable = 1;

struct cold_apob_data_snapshot {
	uint32_t status, complete, bytes, crc32;
	uint8_t raw[COLD_APOB_DATA_LIMIT];
};
static struct cold_apob_data_snapshot cold_data;
_Static_assert(sizeof(struct cold_apob_data_snapshot) == 16 + COLD_APOB_DATA_LIMIT &&
	COLD_APOB_DATA_LIMIT <= CONFIG_PSP_APOB_DRAM_RESERVED_SIZE,
	"Fixed whole cold-data BSS object");
_Static_assert(CONFIG_PSP_APOB_DRAM_SIZE == 0x10000,
	"Unchanged stock 64KiB size policy");

/* A status is only a diagnostic result, never a payload read-owner or FSP
 * compatibility result. Refusal reporting never reads cold_data. */
enum cold_apob_data_status {
	COLD_DATA_COMPLETE,
	COLD_DATA_CONTEXT,
	COLD_DATA_UNBOUND,
	COLD_DATA_OWN_BSS,
	COLD_DATA_HEADER,
	COLD_DATA_SIZE,
	COLD_DATA_CHANGED,
};

static uint32_t cold_data_le32(const uint8_t *p)
{
	uint32_t value = 0;
	for (unsigned int i = 0; i < 4; i++)
		value |= (uint32_t)p[i] << (8 * i);
	return value;
}

static int cold_data_overlap(uintptr_t p, size_t n, uintptr_t q, size_t m)
{
	/* All callers first admit p+n within the current owned program; named
	 * fixed ranges and the separately admitted 64B context cannot wrap. */
	return p < q + m && q < p + n;
}

static int cold_data_owned(void)
{
	uintptr_t p = (uintptr_t)&cold_data;
	uintptr_t bss = (uintptr_t)_bss, end = (uintptr_t)_ebss;
	const size_t bytes = sizeof(cold_data);
	return end > bss && own_program(p, bytes) && p >= bss && p <= end &&
		bytes <= end - p &&
		!cold_data_overlap(p, bytes, (uintptr_t)&cold_apob, sizeof(cold_apob)) &&
		!cold_data_overlap(p, bytes, (uintptr_t)_earlyram_stack,
			REGION_SIZE(earlyram_stack)) &&
		!cold_data_overlap(p, bytes, 0x09c10000U, 0x80000U) &&
		!cold_data_overlap(p, bytes, 0x09e00000U, 0xc0000U) &&
		!cold_data_overlap(p, bytes, 0x09fe0000U, 0x20000U) &&
		!cold_data_overlap(p, bytes, COLD_APOB_BASE, 0x28000U);
}

static int cold_data_context_current(uint64_t *tom)
{
	uint32_t original_flags;
	__asm__ volatile("pushfl; popl %0; cld"
		: "=r" (original_flags) : : "memory", "cc");
	/* The parent always proved its own 64B context before reaching the
	 * terminal call. Renew the current CPU state; no foreign APOB access. */
	if (original_flags & FLAGS_IF_DF ||
	    cold_apob.status != COLD_APOB_READ_COMPLETE ||
	    cold_apob.observed != (OBS_FLAGS_SEGMENTS | OBS_CPUID | OBS_CR0 |
		OBS_GDTR | OBS_SYSCFG | OBS_TOM | OBS_FIXED16))
		return 0;
	struct cpuid_result vendor = cpuid(0);
	if (vendor.eax < 1 || vendor.ebx != 0x68747541 ||
	    vendor.edx != 0x69746e65 || vendor.ecx != 0x444d4163 ||
	    !b565d4_is_selected_cpu(cpuid_eax(1)))
		return 0;
	uint32_t c0;
	uint16_t cs, ds, es, ss;
	__asm__ volatile("mov %%cr0,%0" : "=r" (c0));
	__asm__ volatile("mov %%cs,%0; mov %%ds,%1; mov %%es,%2; mov %%ss,%3"
		: "=r" (cs), "=r" (ds), "=r" (es), "=r" (ss));
	if ((c0 & 0x80000001U) != 1 || c0 != cold_apob.cr0 ||
	    cs != 0x08 || ds != 0x10 || es != 0x10 || ss != 0x10)
		return 0;
	struct cold_apob_gdtr gdtr;
	__asm__ volatile("sgdt %0" : "=m" (gdtr));
	if (!own_program((uintptr_t)&gdtptr, sizeof(gdtptr)) ||
	    gdtr.limit != 31 || gdtr.limit != gdtptr.limit ||
	    gdtr.base != gdtptr.base || gdtr.base != cold_apob.gdtr.base ||
	    !own_program(gdtptr.base, 32))
		return 0;
	static const uint8_t flat_gdt[32] = {
		0, 0, 0, 0, 0, 0, 0, 0,
		0xff, 0xff, 0, 0, 0, 0x9b, 0xcf, 0,
		0xff, 0xff, 0, 0, 0, 0x93, 0xcf, 0,
		0xff, 0xff, 0, 0, 0, 0x9b, 0xaf, 0,
	};
	for (unsigned int i = 0; i < sizeof(flat_gdt); i++)
		if (((const volatile uint8_t *)(uintptr_t)gdtptr.base)[i] != flat_gdt[i])
			return 0;
	msr_t syscfg = rdmsr(SYSCFG_MSR), top = rdmsr(TOP_MEM_MSR);
	if (!b565d4_direct_memory_valid(syscfg.raw) || syscfg.raw != cold_apob.syscfg.raw ||
	    top.raw != cold_apob.tom.raw || (top.raw & ((1U << 23) - 1)) ||
	    top.raw > 0x100000000ULL || top.raw < COLD_APOB_BASE + COLD_APOB_BYTES)
		return 0;
	*tom = top.raw;
	return 1;
}

static int cold_data_header_matches(const uint8_t *bytes)
{
	if (cold_data_le32(bytes) != 0x424f5041U ||
	    !b565d4_apob_header_valid(cold_data_le32(bytes + 4),
				     cold_data_le32(bytes + 12),
				     cold_data_le32(bytes + 8), COLD_APOB_DATA_LIMIT))
		return 0;
	for (unsigned int i = 0; i < COLD_APOB_BYTES; i++)
		if (bytes[i] != cold_apob.raw[i])
			return 0;
	return 1;
}

static int cold_data_source_header_matches(void)
{
	/* Only after the independent data gate, exact admitted current header,
	 * full BSS and finite extent/TOM. This does not expand any access. */
	for (unsigned int i = 0; i < COLD_APOB_BYTES; i++)
		if (((const volatile uint8_t *)COLD_APOB_BASE)[i] != cold_apob.raw[i])
			return 0;
	return 1;
}

static uint32_t cold_data_crc(const uint8_t *p, uint32_t bytes)
{
	uint32_t value = ~0U;
	for (uint32_t i = 0; i < bytes; i++) {
		value ^= p[i];
		for (unsigned int j = 0; j < 8; j++)
			value = (value >> 1) ^ (0xedb88320U & -(value & 1U));
	}
	return ~value;
}

static enum cold_apob_data_status cold_data_capture(void)
{
	/* No own snapshot read/write on a failed full-object admission. The
	 * separately proved parent 64B context is the only earlier owned input. */
	if (!cold_data_owned())
		return COLD_DATA_OWN_BSS;
	cold_data.complete = cold_data.bytes = cold_data.crc32 = 0;
	cold_data.status = COLD_DATA_CONTEXT;
	uint64_t tom;
	if (!cold_data_context_current(&tom))
		return COLD_DATA_CONTEXT;
	if (!cold_data_enable)
		return COLD_DATA_UNBOUND;
	if (!cold_data_header_matches(cold_apob.raw) ||
	    cold_data_le32(cold_apob.raw) != 0x424f5041U)
		return COLD_DATA_HEADER;
	uint32_t bytes = cold_data_le32(cold_apob.raw + 8);
	if (bytes < COLD_APOB_BYTES || bytes > COLD_APOB_DATA_LIMIT ||
	    tom < (uint64_t)COLD_APOB_BASE + bytes)
		return COLD_DATA_SIZE;
	if (!cold_data_source_header_matches())
		return COLD_DATA_CHANGED;
	for (uint32_t i = 0; i < bytes; i++)
		cold_data.raw[i] = ((const volatile uint8_t *)COLD_APOB_BASE)[i];
	for (uint32_t i = 0; i < bytes; i++)
		if (((const volatile uint8_t *)COLD_APOB_BASE)[i] != cold_data.raw[i])
			return COLD_DATA_CHANGED;
	if (!cold_data_header_matches(cold_data.raw) ||
	    !cold_data_source_header_matches() || !cold_data_context_current(&tom) ||
	    tom < (uint64_t)COLD_APOB_BASE + bytes)
		return COLD_DATA_CHANGED;
	cold_data.crc32 = cold_data_crc(cold_data.raw, bytes);
	cold_data.bytes = bytes;
	cold_data.status = COLD_DATA_COMPLETE;
	cold_data.complete = 1;
	return COLD_DATA_COMPLETE;
}

static void cold_data_refusal(enum cold_apob_data_status status)
{
	/* Current scalar only; no stale or unadmitted snapshot metadata. */
	text("B565D4 COLD_APOB_DATA STATUS status="); hex(status, 4);
	text(" bytes=00000000 complete=00000000 cap="); hex(COLD_APOB_DATA_LIMIT, 4);
	text(" owner_release=0\r\n");
}

static void cold_data_report(enum cold_apob_data_status status)
{
	if (status != COLD_DATA_COMPLETE) {
		cold_data_refusal(status);
		return;
	}
	/* All output uses the same literal full BSS and finite owned prefix. */
	if (!cold_data_owned() || cold_data.status != COLD_DATA_COMPLETE ||
	    cold_data.complete != 1 || cold_data.bytes < COLD_APOB_BYTES ||
	    cold_data.bytes > COLD_APOB_DATA_LIMIT ||
	    !cold_data_header_matches(cold_data.raw) ||
	    cold_data_crc(cold_data.raw, cold_data.bytes) != cold_data.crc32) {
		cold_data_refusal(COLD_DATA_CHANGED);
		return;
	}
	const uint32_t bytes = cold_data.bytes, check = cold_data.crc32;
	text("B565D4 COLD_APOB_DATA BEGIN status=00000000 bytes="); hex(bytes, 4);
	text(" cap="); hex(COLD_APOB_DATA_LIMIT, 4);
	text(" crc="); hex(check, 4);
	text(" source_repeat_equal=00000001 owner_release=0\r\n");
	for (uint32_t offset = 0; offset < bytes; ) {
		uint32_t count = bytes - offset;
		if (count > COLD_APOB_DATA_CHUNK)
			count = COLD_APOB_DATA_CHUNK;
		if (offset > sizeof(cold_data.raw) || count > sizeof(cold_data.raw) - offset) {
			cold_data_refusal(COLD_DATA_CHANGED);
			return;
		}
		if (CONFIG(SOC_AMD_CEZANNE_NATIVE)) {
			/* Keep the same finite owned-byte read walk without serializing
			 * it. Both complete CRC checks and every source/context check
			 * below remain; only UART bulk output is omitted.
			 */
			const volatile uint8_t *raw = cold_data.raw;
			for (uint32_t i = 0; i < count; i++)
				(void)raw[offset + i];
		} else {
			text("B565D4 COLD_APOB_DATA RAW offset="); hex(offset, 4);
			text(" count="); hex(count, 4); text(" data=");
			for (uint32_t i = 0; i < count; i++)
				hex(cold_data.raw[offset + i], 1);
			text("\r\n");
		}
		offset += count;
	}
	/* No successful END after changed snapshot/context. Full-dump mode can
	 * still be reconstructed by the receiver. Native summary mode publishes
	 * target-validated bytes/CRC, not a fresh receiver-side full APOB copy.
	 */
	uint64_t tom;
	if (!cold_data_owned() || cold_data.bytes != bytes || cold_data.crc32 != check ||
	    cold_data_crc(cold_data.raw, bytes) != check ||
	    !cold_data_context_current(&tom) || tom < (uint64_t)COLD_APOB_BASE + bytes) {
		cold_data_refusal(COLD_DATA_CHANGED);
		return;
	}
	if (CONFIG(SOC_AMD_CEZANNE_NATIVE)) {
		text("B565D4 COLD_APOB_DATA SUMMARY bytes="); hex(bytes, 4);
		text(" crc="); hex(check, 4);
		text(" raw_uart_dump=00000000 source_repeat_equal=00000001 owner_release=0\r\n");
	}
	text("B565D4 COLD_APOB_DATA END status=00000000 bytes="); hex(bytes, 4);
	text(" crc="); hex(check, 4);
	text(" complete=00000001 source_repeat_equal=00000001 owner_release=0\r\n");
}

static int cold_data_snapshot_current(void)
{
	/* Whole BSS admission precedes every metadata/payload read. This is
	 * the existing owned copy, not permission to follow foreign pointers. */
	return cold_data_owned() &&
		cold_data.status == COLD_DATA_COMPLETE && cold_data.complete == 1 &&
		cold_data.bytes >= 16U && cold_data.bytes <= COLD_APOB_DATA_LIMIT &&
		cold_data_le32(cold_data.raw + 8) == cold_data.bytes &&
		cold_data_header_matches(cold_data.raw) &&
		cold_data_crc(cold_data.raw, cold_data.bytes) == cold_data.crc32;
}

/* Native95/Source26: diagnostic bus0 only. No BAR sizing, enumeration,
 * resource publication, CFC data write, bridge routing or supplier callback.
 * Preserve the proven read sequence and report its result.
 */
struct native95_pci_snapshot {
	uint32_t status, saved_cf8, restored_cf8;
	uint32_t requested, observed, complete, present;
};
static struct native95_pci_snapshot native95_pci;
_Static_assert(sizeof(struct native95_pci_snapshot) == 28,
	"Bounded Native95 own-stage BSS state");

static int native95_pci_read(uint32_t selector, uint32_t *value)
{
	native95_pci.requested = selector;
	outl(selector, 0xcf8);
	native95_pci.observed = inl(0xcf8);
	if (native95_pci.observed != selector)
		return 0;
	*value = inl(0xcfc);
	return 1;
}

static void native95_pci0_scan(void)
{
	uint64_t tom;
	uintptr_t p = (uintptr_t)&native95_pci;
	uintptr_t bss = (uintptr_t)_bss, end = (uintptr_t)_ebss;

	/* Context refusal is terminal after a diagnostic line, before PCI I/O.
	 * No current APOB hole or TOM value confers additional write ownership.
	 */
	if (!own_program(p, sizeof(native95_pci)) || p < bss || p > end ||
	    sizeof(native95_pci) > end - p || !cold_data_context_current(&tom)) {
		text("B565D4 NATIVE95 PCI0 SKIP context_or_owned_bss=0\r\n");
		return;
	}
	native95_pci.status = 1;
	native95_pci.complete = native95_pci.present = 0;
	native95_pci.saved_cf8 = inl(0xcf8);
	for (unsigned int i = 0; i < 256; i++) {
		const uint32_t selector = 0x80000000U | (i << 8);
		uint32_t id, value;

		if (!native95_pci_read(selector, &id))
			goto restore;
		/* Preserve raw absent/invalid IDs in the report. Do not follow them. */
		if (id != 0xffffffffU && id != 0) {
			if (!native95_pci_read(selector | 8, &value) ||
			    !native95_pci_read(selector | 12, &value))
				goto restore;
			if (((value >> 16) & 0x7f) == 1 &&
			    !native95_pci_read(selector | 24, &value))
				goto restore;
			native95_pci.present++;
		}
		native95_pci.complete++;
	}
	native95_pci.status = 0;
restore:
	/* Every ordinary post-save path restores the entire original selector.
	 * Report only after restoration; no UART callback executes while selected.
	 */
	outl(native95_pci.saved_cf8, 0xcf8);
	native95_pci.restored_cf8 = inl(0xcf8);
	if (native95_pci.restored_cf8 != native95_pci.saved_cf8)
		native95_pci.status = 2;
	text("B565D4 NATIVE95 PCI0 BEGIN status="); hex(native95_pci.status, 4);
	text(" complete="); hex(native95_pci.complete, 4);
	text(" present="); hex(native95_pci.present, 4);
	text(" saved_cf8="); hex(native95_pci.saved_cf8, 4);
	text(" restored_cf8="); hex(native95_pci.restored_cf8, 4);
	text(" requested="); hex(native95_pci.requested, 4);
	text(" observed="); hex(native95_pci.observed, 4); text("\r\n");
	text("B565D4 NATIVE95 END status="); hex(native95_pci.status, 4);
	text(" fsp_calls=0 cbmem_write=0 owner_release=0 native_release=0000\r\n");
}

void __noreturn b565d4_cold_apob_header_witness(void)
{
	uint32_t original_flags;
	__asm__ volatile("pushfl; popl %0; cld"
		: "=r" (original_flags) : : "memory", "cc");

	/* Genuine cbfs_prog_stage_load zeroes memlen minus loaded file bytes.
	 * That owned stage/BSS proof is a separate deployment premise. No
	 * fallback object or external scratch is used. */
	uintptr_t bss = (uintptr_t)_bss, end = (uintptr_t)_ebss;
	uintptr_t p = (uintptr_t)&cold_apob;
	if (!own_program(p, sizeof(cold_apob)) || p < bss || p > end ||
	    sizeof(cold_apob) > end - p) {
		text("B565D4 COLD_APOB REFUSED own_bss=0\r\n");
		uart_tx_flush(1);
		halt();
	}

	cold_apob.flags = original_flags;
	__asm__ volatile("mov %%cs,%0; mov %%ds,%1; mov %%es,%2; mov %%ss,%3"
		: "=r" (cold_apob.cs), "=r" (cold_apob.ds),
		  "=r" (cold_apob.es), "=r" (cold_apob.ss));
	cold_apob.observed = OBS_FLAGS_SEGMENTS;
	cold_apob.status = COLD_APOB_FLAGS;
	if (cold_apob.flags & FLAGS_IF_DF)
		goto done;
	cold_apob.status = COLD_APOB_SEGMENTS;
	if (cold_apob.cs != 0x08 || cold_apob.ds != 0x10 ||
	    cold_apob.es != 0x10 || cold_apob.ss != 0x10)
		goto done;

	struct cpuid_result vendor = cpuid(0);
	cold_apob.status = COLD_APOB_CPU;
	if (vendor.eax < 1 || vendor.ebx != 0x68747541 ||
	    vendor.edx != 0x69746e65 || vendor.ecx != 0x444d4163)
		goto done;
	cold_apob.cpuid = cpuid_eax(1);
	cold_apob.observed |= OBS_CPUID;
	if (!b565d4_is_selected_cpu(cold_apob.cpuid))
		goto done;

	__asm__ volatile("mov %%cr0,%0" : "=r" (cold_apob.cr0));
	cold_apob.observed |= OBS_CR0;
	cold_apob.status = COLD_APOB_MODE;
	if ((cold_apob.cr0 & 0x80000001U) != 1)
		goto done;

	__asm__ volatile("sgdt %0" : "=m" (cold_apob.gdtr));
	cold_apob.observed |= OBS_GDTR;
	cold_apob.status = COLD_APOB_GDT;
	if (!own_program((uintptr_t)&gdtptr, sizeof(gdtptr)) ||
	    cold_apob.gdtr.limit != 31 || cold_apob.gdtr.limit != gdtptr.limit ||
	    cold_apob.gdtr.base != gdtptr.base || !own_program(gdtptr.base, 32))
		goto done;
	/* The current table is the exact owned stock gdt_init.S table. Source
	 * assembly/segment load proves the hidden flat caches; do not repair them. */
	static const uint8_t flat_gdt[32] = {
		0, 0, 0, 0, 0, 0, 0, 0,
		0xff, 0xff, 0, 0, 0, 0x9b, 0xcf, 0,
		0xff, 0xff, 0, 0, 0, 0x93, 0xcf, 0,
		0xff, 0xff, 0, 0, 0, 0x9b, 0xaf, 0,
	};
	for (unsigned int i = 0; i < sizeof(flat_gdt); i++)
		if (((const volatile uint8_t *)(uintptr_t)gdtptr.base)[i] != flat_gdt[i])
			goto done;

	cold_apob.syscfg = rdmsr(SYSCFG_MSR);
	cold_apob.observed |= OBS_SYSCFG;
	cold_apob.status = COLD_APOB_SME;
	if (!b565d4_direct_memory_valid(cold_apob.syscfg.raw))
		goto done;
	cold_apob.tom = rdmsr(TOP_MEM_MSR);
	cold_apob.observed |= OBS_TOM;
	cold_apob.status = COLD_APOB_TOM;
	if ((cold_apob.tom.raw & ((1U << 23) - 1)) ||
	    cold_apob.tom.raw > 0x100000000ULL ||
	    cold_apob.tom.raw < COLD_APOB_BASE + COLD_APOB_BYTES)
		goto done;

	/* Exactly one fixed16B read. No header field extends this read, and no
	 * NV-cache, mailbox, SMN, CBMEM or FSP path is reachable from this witness. */
	for (unsigned int i = 0; i < COLD_APOB_BYTES; i++)
		cold_apob.raw[i] = ((const volatile uint8_t *)COLD_APOB_BASE)[i];
	cold_apob.observed |= OBS_FIXED16;
	cold_apob.status = COLD_APOB_READ_COMPLETE;
done:
	report();
	enum cold_apob_data_status data_status = cold_data_capture();
	cold_data_report(data_status);
	native95_pci0_scan();
#if CONFIG(SOC_AMD_CEZANNE_NATIVE)
	if (data_status != COLD_DATA_COMPLETE || !cold_data_snapshot_current())
		halt();
	native99_prepare_and_enter(cold_data.raw, cold_data.bytes);
#endif
	uart_tx_flush(1);
	halt();
}
