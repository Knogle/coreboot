/* SPDX-License-Identifier: GPL-2.0-only */
/* EDK2 trial: genuine LZMA SELF loader and stock coreboot payload entry. */
#include <commonlib/bsd/compiler.h>
#include "native84_profile.h"
#include "native253_payload.h"
#include <arch/mmio.h>
#include <bootmem.h>
#include <cbfs.h>
#include <cbmem.h>
#include <commonlib/bsd/compression.h>
#include <commonlib/bsd/ipchksum.h>
#include <commonlib/endian.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <console/uart.h>
#include <cpu/x86/msr.h>
#include <cpu/x86/mtrr.h>
#include <limits.h>
#include <program_loading.h>
#include <string.h>


static uint8_t *self_backup;
static unsigned int payload_attempted, payload_loaded;

static void refuse(const char *reason) __noreturn;
static void refuse(const char *reason)
{
	printk(BIOS_ERR, "NATIVE253 PAYLOAD REFUSAL %s\n", reason);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	native84_terminal();
}

static bool cbmem_span(const void *p, uint64_t bytes)
{
	const struct native84_handoff *d = native84_descriptor();
	uintptr_t first = (uintptr_t)p;
	uint64_t parent_end = (uint64_t)d->parent_base + d->parent_bytes;

	return parent_end <= 0x100000000ULL && d->cbmem_bottom >= d->parent_base &&
		d->cbmem_top <= parent_end && d->cbmem_bottom < d->cbmem_top &&
		first >= d->cbmem_bottom &&
		first < d->cbmem_top && bytes && bytes <= d->cbmem_top - first;
}

static bool exact_self(const void *mapping, size_t bytes)
{
	return bytes == SELF_BYTES &&
		!memcmp(mapping, b565d4_self_header, sizeof(b565d4_self_header));
}

static void log_mtrrs(void)
{
	static const unsigned int indices[] = {
		0xfe, 0x2ff, 0xc0010010,
		0x250, 0x258, 0x259, 0x268, 0x269, 0x26a,
		0x26b, 0x26c, 0x26d, 0x26e, 0x26f,
	};

	for (unsigned int i = 0; i < sizeof(indices) / sizeof(indices[0]); i++) {
		msr_t v = rdmsr(indices[i]);

		printk(BIOS_INFO, "NATIVE253 MTRR %08x=%08x:%08x\n",
			indices[i], v.hi, v.lo);
		if (indices[i] == 0xc0010010 && !(v.lo & (1U << 19)))
			printk(BIOS_INFO, "NATIVE253 FIXED RDWR BITS HIDDEN; NO ROUTING CHANGE\n");
	}
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

static void verify_load(const char *phase)
{
	for (size_t i = 0; i < ARRAY_SIZE(b565d4_payload_segments); i++) {
		const uint32_t crc = native253_crc32(
			(const void *)(uintptr_t)b565d4_payload_segments[i].address,
			b565d4_payload_segments[i].bytes);

		printk(BIOS_INFO, "B565D4 SELF %s segment=%zu base=%08x bytes=%u "
		       "crc32=%08x expected=%08x\n", phase, i,
		       b565d4_payload_segments[i].address,
		       b565d4_payload_segments[i].bytes, crc,
		       b565d4_payload_segments[i].crc32);
		if (crc != b565d4_payload_segments[i].crc32)
			refuse("decompressed SELF segment readback");
	}
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

/* Genuine strong override of the standard weak hook, not --wrap of a local
 * same-TU call. Non-payload behavior remains the stock allow-loading result.
 * Root selects the actual medium/SELF identity and low-memory takeover before
 * running this trial. This hook does not grant ownership by inspecting scalars.
 */
int prog_locate_hook(struct prog *prog)
{
	const struct cbmem_entry *entry;
	void *table;

	if (prog_type(prog) != PROG_PAYLOAD)
		return 0;
	if (CONFIG(EDK2_DEBUG)) {
		printk(BIOS_NOTICE, "Board payload: locating and loading SELF\n");
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	}
	native84_stage_check();
	if (payload_attempted || strcmp(prog_name(prog), CONFIG_CBFS_PREFIX "/payload"))
		refuse("attempt/name");
	payload_attempted = 1;
	if (cbmem_entry_find(SELF_BACKUP_ID))
		refuse("backup ID already present");
	table = cbmem_find(CBMEM_ID_CBTABLE);
	if (!table)
		refuse("missing coreboot table");
	self_backup = cbmem_add(SELF_BACKUP_ID, NATIVE253_SPI_BYTES);
	entry = cbmem_entry_find(SELF_BACKUP_ID);
	if (!self_backup || !entry || cbmem_entry_start(entry) != self_backup ||
	    cbmem_entry_size(entry) < NATIVE253_SPI_BYTES ||
	    !cbmem_span(self_backup, cbmem_entry_size(entry)))
		refuse("owned payload buffer allocation");
	native253_load_payload(self_backup);
	if (!exact_self(self_backup, SELF_BYTES))
		refuse("exact built SELF headers after full SPI read");
	prog->cbfs_type = CBFS_TYPE_SELF;
	/* Fresh raw observations only. Hidden attribute bits do not refuse the
 * Root-selected finite transfer, and this function never writes an MSR.
 */
	log_mtrrs();
	if (!selfload_mapped(prog, self_backup, BM_MEM_RAM) ||
	    (uintptr_t)prog_entry(prog) != SELF_ENTRY ||
	    prog_entry_arg(prog) != table)
		refuse("stock SELF load/entry/table");
	verify_load("COPIED");
	payload_loaded = 1;
	return 1; /* payload_load skips duplicate traversal; its normal out check remains. */
}

/* Physical low-memory reads, including address zero, are explicit x86
 * instructions rather than C null-pointer dereferences.
 */
static uint32_t low_physical_read32(uintptr_t address)
{
	uint32_t value;

	asm volatile ("movl (%1), %0" : "=r" (value) : "r" (address) : "memory");
	return value;
}

/* EARLY debug handles are not the later stage's BSS handles. Release only
 * their four DR7 enable pairs before handing low memory to the payload.
 */
static void native116_debug_handoff(void)
{
	uintptr_t dr0, dr1, dr2, dr3, dr6, dr7, after;

	asm volatile ("mov %%dr0, %0" : "=r" (dr0) : : "memory");
	asm volatile ("mov %%dr1, %0" : "=r" (dr1) : : "memory");
	asm volatile ("mov %%dr2, %0" : "=r" (dr2) : : "memory");
	asm volatile ("mov %%dr3, %0" : "=r" (dr3) : : "memory");
	asm volatile ("mov %%dr6, %0" : "=r" (dr6) : : "memory");
	asm volatile ("mov %%dr7, %0" : "=r" (dr7) : : "memory");
	printk(BIOS_INFO, "NATIVE116 DEBUG BEFORE DR0=%08x DR1=%08x DR2=%08x "
	       "DR3=%08x DR6=%08x DR7=%08x\n", (unsigned int)dr0, (unsigned int)dr1,
	       (unsigned int)dr2, (unsigned int)dr3, (unsigned int)dr6, (unsigned int)dr7);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	const uintptr_t disabled = dr7 & ~(uintptr_t)0xff;

	asm volatile ("mov %0, %%dr7" : : "r" (disabled) : "memory");
	asm volatile ("mov %%dr7, %0" : "=r" (after) : : "memory");
	printk(BIOS_INFO, "NATIVE116 DEBUG AFTER DR7=%08x enable_bits=%02x\n",
	       (unsigned int)after, (unsigned int)(after & 0xff));
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (after & 0xff)
		refuse("payload debug enable readback");
}

/* Read the bounded table through explicit volatile DWORD loads. Combine
 * genuine Internet checksums of each owned local word; no external stores.
 */
static uint16_t lbio_read_checksum(uintptr_t address, unsigned int bytes)
{
	uint16_t checksum = 0xffff;

	for (unsigned int offset = 0; offset < bytes; offset += 4) {
		const uint32_t word = read32((const void *)(address + offset));

		checksum = ipchksum_add(offset, checksum, ipchksum(&word, sizeof(word)));
	}
	return checksum;
}

/* Diagnostic only: no table repair, new allocation, or permission promotion.
 * This runs after SELF readback, immediately before the unchanged payload.
 */
static void native116_lbio_readback(const void *table)
{
	const struct cbmem_entry *entry = cbmem_entry_find(CBMEM_ID_CBTABLE);
	uint32_t low[10], high[6], candidate[6];
	unsigned int matches = 0;

	printk(BIOS_INFO, "NATIVE116 LBIO LOW500 READ BEGIN bytes=40\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	for (unsigned int i = 0; i < ARRAY_SIZE(low); i++)
		low[i] = low_physical_read32(0x500 + 4 * i);
	printk(BIOS_INFO, "NATIVE116 LBIO LOW500 hdr=%08x/%08x/%08x/%08x/%08x/%08x "
	       "fwd=%08x/%08x/%08x/%08x hsum=%04x fsum=%04x expected=%p\n",
	       low[0], low[1], low[2], low[3], low[4], low[5],
	       low[6], low[7], low[8], low[9], ipchksum(low, 24),
	       ipchksum(low + 6, 16), table);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (entry && cbmem_entry_start(entry) == table &&
	    cbmem_entry_size(entry) >= sizeof(high) && cbmem_entry_size(entry) <= 0x8000 &&
	    cbmem_span(table, cbmem_entry_size(entry))) {
		printk(BIOS_INFO, "NATIVE116 LBIO HIGH READ BEGIN address=%p entry_bytes=%u\n",
		       table, (unsigned int)cbmem_entry_size(entry));
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		for (unsigned int i = 0; i < ARRAY_SIZE(high); i++)
			high[i] = read32((const void *)((uintptr_t)table + 4 * i));
		printk(BIOS_INFO, "NATIVE116 LBIO HIGH hdr=%08x/%08x/%08x/%08x/%08x/%08x "
		       "hsum=%04x\n", high[0], high[1], high[2], high[3], high[4], high[5],
		       ipchksum(high, sizeof(high)));
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if (!(high[3] & 3U) && high[3] <= cbmem_entry_size(entry) - sizeof(high)) {
			const uint16_t sum = lbio_read_checksum((uintptr_t)table + sizeof(high),
				high[3]);

			printk(BIOS_INFO, "NATIVE116 LBIO HIGH DATA bytes=%u sum=%04x "
			       "expected=%08x match=%u\n", high[3], sum, high[4], sum == high[4]);
		} else {
			printk(BIOS_ERR, "NATIVE116 LBIO HIGH DATA SKIP invalid bounded length=%u\n",
			       high[3]);
		}
	} else {
		printk(BIOS_ERR, "NATIVE116 LBIO HIGH SKIP missing/out-of-bounds entry\n");
	}
	printk(BIOS_INFO, "NATIVE116 LBIO LOW SCAN BEGIN [0000,1000) stride16\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	for (uintptr_t address = 0; address < 0x1000; address += 16) {
		if (low_physical_read32(address) != 0x4f49424cU)
			continue;
		matches++;
		printk(BIOS_INFO, "NATIVE116 LBIO CANDIDATE address=%08x\n",
		       (unsigned int)address);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if (address > 0x1000 - sizeof(candidate))
			continue;
		for (unsigned int i = 0; i < ARRAY_SIZE(candidate); i++)
			candidate[i] = low_physical_read32(address + 4 * i);
		printk(BIOS_INFO, "NATIVE116 LBIO CANDIDATE hdr=%08x/%08x/%08x/%08x/%08x/%08x\n",
		       candidate[0], candidate[1], candidate[2], candidate[3], candidate[4],
		       candidate[5]);
	}
	printk(BIOS_INFO, "NATIVE116 LBIO LOW SCAN DONE signatures=256 matches=%u; "
	       "stock SELF next\n", matches);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

/* Last ordinary hook before genuine arch_prog_run/cdecl entry. Recheck after
 * intervening bootstate/finalization work without parsing/loading a second time.
 */
void platform_prog_run(struct prog *prog)
{
	void *table;

	if (prog_type(prog) != PROG_PAYLOAD)
		return;
	native84_stage_check();
	table = cbmem_find(CBMEM_ID_CBTABLE);
	if (!payload_loaded || !table || !cbmem_span(self_backup, SELF_BYTES) ||
	    (uintptr_t)prog_entry(prog) != SELF_ENTRY ||
	    prog_entry_arg(prog) != table)
		refuse("final payload state");
	verify_load("BEFORE ENTRY");
	if (native84_profile_policy.entry_path == 2) {
		native116_debug_handoff();
		native116_lbio_readback(table);
	}
	if (CONFIG(EDK2_DEBUG)) {
		printk(BIOS_NOTICE, "Board payload: entering verified SELF at %p\n",
		       prog_entry(prog));
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	}
}
