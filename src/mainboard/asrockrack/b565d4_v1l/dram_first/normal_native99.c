/* SPDX-License-Identifier: GPL-2.0-only */
#include "b565d4_cpu.h"
#include "normal_native99.h"
#include "native99_apob_select.h"
#include "native99_apob_packet.h"
#include "native84_handoff.h"
#include "native84_context.h"
#include "bridge_bytes.h"
#include "ramstage_contract.h"
#include "native84_profile.h"
#include <arch/cpu.h>
#include <boot_device.h>
#include <cbfs.h>
#include <fmap.h>
#include <amdblocks/lpc.h>
#include <amdblocks/spi.h>
#include <console/console.h>
#include <console/uart.h>
#include <cpu/amd/msr.h>
#include <cpu/x86/msr.h>
#include <halt.h>
#include <option.h>
#include <program_loading.h>
#include <device/pci_ops.h>
#include <delay.h>
#include <spi-generic.h>
#include <timer.h>
#include <symbols.h>
#include <vb2_sha.h>

#define N99_A NATIVE99_PARENT_BEGIN
#define N99_END NATIVE99_PARENT_END
#define N99_EPOCH 0x00840001U
#define N99_CONTEXT (N99_A + 0x100U)
#define N99_BRIDGE (N99_A + 0x2000U)
#define N99_BRIDGE_08 (N99_BRIDGE + 0x2eU)
#define N99_FAULT (N99_BRIDGE + 0x49U)
#define N99_ROM (N99_A + 0x30000U)
#define N99_MEDIA (N99_A + 0xd0000U)
#define N99_CBMEM (N99_A + 0x150000U)
#define NATIVE156_SPI_MEDIA_OFFSET 0x01d61000U
#define N99_MEDIA_BYTES 0x80000U
#define NATIVE156_SPI_READY_TIMEOUT_MS 3000

asmlinkage void native95_context_prepare(
	const struct native84_context_image *image);

static void __noreturn refuse(const char *why)
{
	printk(BIOS_ERR, "NATIVE99 EARLY REFUSED %s; NO RETURN/RETRY\n", why);
	uart_tx_flush(1);
	halt();
}

/* A different logical device is admitted only after the complete SPI13/SHA
 * guard returns. Bootblock keeps its original EARLY RAM boot-device provider.
 */
static unsigned int native94_medium_ready;
static const struct mem_region_device native94_verified_media =
	MEM_REGION_DEV_RO_INIT((void *)(uintptr_t)N99_MEDIA, N99_MEDIA_BYTES);

#if CONFIG(USE_OPTION_TABLE)
bool cmos_option_table_ready(void)
{
	/* console_init() asks for debug_level before the SPI medium has been
	 * loaded. Keep that lookup on its standard fallback until admission.
	 */
	return native94_medium_ready;
}
#endif

const struct region_device *__wrap_boot_device_ro(void);
void __wrap_boot_device_init(void);

const struct region_device *__wrap_boot_device_ro(void)
{
	if (!native94_medium_ready)
		refuse("CBFS medium requested before whole SPI13/SHA guard");
	return &native94_verified_media.rdev;
}

void __wrap_boot_device_init(void)
{
	if (!native94_medium_ready)
		refuse("CBFS init before whole SPI13/SHA guard");
	/* The admitted read-only RAM copy needs no ROM2/SPI configuration. */
}

static int same(uint32_t to, const uint8_t *from, uint32_t bytes)
{
	const volatile uint8_t *p = (const void *)(uintptr_t)to;
	for (uint32_t i = 0; i < bytes; i++)
		if (p[i] != from[i])
			return 0;
	return 1;
}

static void copy(uint32_t to, const uint8_t *from, uint32_t bytes)
{
	volatile uint8_t *p = (void *)(uintptr_t)to;
	for (uint32_t i = 0; i < bytes; i++)
		p[i] = from[i];
	if (!same(to, from, bytes))
		refuse("whole copy/readback");
}

static void clear(uint32_t to, uint32_t bytes)
{
	volatile uint8_t *p = (void *)(uintptr_t)to;
	for (uint32_t i = 0; i < bytes; i++)
		p[i] = 0;
	for (uint32_t i = 0; i < bytes; i++)
		if (p[i])
			refuse("BSS/context zero readback");
}

/* Read the selected physical medium into the already excluded N99 parent.
 * Only the exact LATE154 whole-medium SHA256 permits the existing ROM handoff.
 */
static void native156_spi13_load_medium(void)
{
	const pci_devfn_t lpc = PCI_DEV(0, 0x14, 3);
	const uint16_t owner_mask = SPI_MUTEX_PSP_OWNS | SPI_MUTEX_HFP_OWNS |
		SPI_MUTEX_HBIOS_OWNS | SPI_SEMAPHORE_DRIVER_LOCKED |
		SPI_SEMAPHORE_BIOS_LOCKED;
	struct spi_slave slave;
	struct stopwatch bound;
	struct vb2_sha256_context sha;
	uint8_t digest[VB2_SHA256_DIGEST_SIZE] = { 0 };
	static const uint8_t expected_sha256[VB2_SHA256_DIGEST_SIZE] =
		B565D4_MEDIA_SHA256;
	uint8_t fifo[68], opcode, tx, rx, cs, override, addr32, page;
	uint16_t enable, speed, misc;
	uint32_t restricted[2], status, completed = 0;
	int ret = 0, restore = 0, controls_same, match = 0;

	/* This proposal must not silently acquire new hardware/SMM owners. */
	_Static_assert(CONFIG(SOC_AMD_COMMON_BLOCK_SPI), "genuine FCH SPI provider");
	_Static_assert(!CONFIG(SOC_AMD_COMMON_BLOCK_SPI_SEMAPHORE),
		"no new hardware-mutex writes in this fixed trial");
	_Static_assert(!CONFIG(SOC_AMD_COMMON_BLOCK_PSP_SMI),
		"no new PSP/SMM semaphore writes in this fixed trial");
	_Static_assert(!CONFIG(SOC_AMD_COMMON_BLOCK_PSP_ROM_ARMOR3),
		"no ROM Armor/mailbox change in this fixed trial");
	_Static_assert(CONFIG(HAVE_MONOTONIC_TIMER), "existing romstage timer");
	_Static_assert(N99_MEDIA_BYTES == 0x80000U && N99_MEDIA_BYTES % 64 == 0,
		"fixed whole512KiB medium and finite64-byte transfers");
	_Static_assert(NATIVE156_SPI_MEDIA_OFFSET + N99_MEDIA_BYTES == 0x01de1000U,
		"fixed physical raw FFS body");
	_Static_assert(sizeof(expected_sha256) == 32, "fixed whole-medium SHA256");

	printk(BIOS_INFO, "NATIVE156 SPI13 MEDIUM BEGIN offset=01d61000 bytes=524288\n");
	uart_tx_flush(1);
	if (pci_s_read_config32(lpc, 0) != 0x790e1022 ||
	    pci_s_read_config32(lpc, 0xa0) != 0xfec10002)
		refuse("SPI13 fixed LPC identity/base/decode");
	for (unsigned int i = 0; i < MAX_ROM_PROTECT_RANGES; i++) {
		uint32_t range = pci_s_read_config32(lpc, ROM_PROTECT_RANGE_REG(i));
		printk(BIOS_INFO, "NATIVE156 SPI13 LPC RP%u=%08x\n", i, range);
		if (range & ROM_RANGE_RP) /* RP bit9; RANGE_UNIT bit8 is not protection. */
			refuse("SPI13 read protection; no unlock");
	}
	/* spi_init discovers current base/boot CS; it is not fch_spi_early_init. */
	spi_init();
	if (spi_get_bar() != 0xfec10000U)
		refuse("SPI13 discovered base");
	/* Warm reset can leave this read-only admission state transiently busy.
	 * Wait for the same predicate; never clear or acquire an owner. */
	stopwatch_init_msecs_expire(&bound, NATIVE156_SPI_READY_TIMEOUT_MS);
	do {
		cs = spi_read8(0x1d);
		enable = spi_read16(SPI100_ENABLE);
		misc = spi_read16(SPI_MISC_CNTRL);
		status = spi_read32(SPI_STATUS);
		if (status != UINT32_MAX && !(status & SPI_BUSY) && !(cs & 3) &&
		    (enable & SPI_USE_SPI100) && !(misc & owner_mask))
			break;
		udelay(1000);
	} while (!stopwatch_expired(&bound));
	speed = spi_read16(SPI100_SPEED_CONFIG);
	override = spi_read8(SPI_ROM2_OVERRIDE);
	addr32 = spi_read8(SPI_ROM_ADDR32_CTRL0);
	page = spi_read8(SPI_ROM_PAGE);
	restricted[0] = spi_read32(0x04);
	restricted[1] = spi_read32(0x08);
	printk(BIOS_INFO, "NATIVE156 SPI13 PREFLIGHT ctrl=%08x status=%08x "
		"CS=%02x enable=%04x speed=%04x override=%02x addr32=%02x page=%02x "
		"misc=%04x restricted=%08x/%08x\n", spi_read32(SPI_CNTRL0), status,
		cs, enable, speed, override, addr32, page, misc,
		restricted[0], restricted[1]);
	uart_tx_flush(1);
	if (status == UINT32_MAX || (status & SPI_BUSY) || (cs & 3) ||
	    !(enable & SPI_USE_SPI100) || (misc & owner_mask))
		refuse("SPI13 fresh idle/CS0/SPI100/observed owners");
	for (unsigned int i = 0; i < 8; i++)
		if (((restricted[i / 4] >> (8 * (i % 4))) & 0xff) == 0x13)
			refuse("SPI13 restricted opcode; no unlock");
	if (spi_setup_slave(0, 0, &slave))
		refuse("SPI13 genuine controller setup");
	opcode = spi_read8(0x45);
	tx = spi_read8(0x48);
	rx = spi_read8(0x4b);
	for (unsigned int i = 0; i < sizeof(fifo); i++)
		fifo[i] = spi_read8(SPI_FIFO + i);
	if (spi_claim_bus(&slave))
		refuse("SPI13 genuine controller claim");

	stopwatch_init_msecs_expire(&bound, 15000);
	for (uint32_t off = 0; off < N99_MEDIA_BYTES; off += 64) {
		uint32_t address = NATIVE156_SPI_MEDIA_OFFSET + off;
		uint8_t cmd[5] = { 0x13, address >> 24, address >> 16,
			address >> 8, address };
		/* Same command/response vectors as genuine do_spi_flash_cmd().
		 * FCH combines them into one opcode13/TX4/RX64 transaction.
		 */
		struct spi_op vectors[2] = {
			{ .dout = cmd, .bytesout = sizeof(cmd) },
			{ .din = (void *)(uintptr_t)(N99_MEDIA + off), .bytesin = 64 },
		};
		if (stopwatch_expired(&bound)) {
			ret = -1;
			break;
		}
		ret = spi_xfer_vector(&slave, vectors, 2);
		if (ret || vectors[0].status != SPI_OP_SUCCESS ||
		    vectors[1].status != SPI_OP_SUCCESS)
			break;
		completed += 64;
	}
	if (stopwatch_expired(&bound))
		ret = -1;

	/* Restore only owned command/count/FIFO bytes, never trigger/status or CS.
	 * No retry/reset if completion is still Busy or unknown.
	 */
	status = spi_read32(SPI_STATUS);
	if (status != UINT32_MAX && !(status & SPI_BUSY)) {
		spi_write8(0x45, opcode);
		spi_write8(0x48, tx);
		spi_write8(0x4b, rx);
		for (unsigned int i = 0; i < sizeof(fifo); i++)
			spi_write8(SPI_FIFO + i, fifo[i]);
		restore = spi_read8(0x45) == opcode && spi_read8(0x48) == tx &&
			spi_read8(0x4b) == rx;
		for (unsigned int i = 0; i < sizeof(fifo); i++)
			restore &= spi_read8(SPI_FIFO + i) == fifo[i];
	}
	controls_same = spi_read8(0x1d) == cs &&
		spi_read16(SPI100_ENABLE) == enable &&
		spi_read16(SPI100_SPEED_CONFIG) == speed &&
		spi_read8(SPI_ROM2_OVERRIDE) == override &&
		spi_read8(SPI_ROM_ADDR32_CTRL0) == addr32 &&
		spi_read8(SPI_ROM_PAGE) == page &&
		spi_read16(SPI_MISC_CNTRL) == misc;
	/* With the static config above and unchanged CS0 this release performs
	 * no CS, hardware-mutex or SMM semaphore write. Refuse instead of repairing
	 * an unexpected chip-select change by another agent.
	 */
	if (spi_read8(0x1d) == cs)
		spi_release_bus(&slave);
	else
		controls_same = 0;
	if (!ret && completed == N99_MEDIA_BYTES && restore && controls_same) {
		vb2_sha256_init(&sha, VB2_HASH_SHA256);
		vb2_sha256_update(&sha, (const void *)(uintptr_t)N99_MEDIA,
			N99_MEDIA_BYTES);
		vb2_sha256_finalize(&sha, digest, VB2_HASH_SHA256);
		match = 1;
		for (unsigned int i = 0; i < sizeof(digest); i++)
			match &= digest[i] == expected_sha256[i];
		printk(BIOS_INFO, "NATIVE156 SPI13 SHA256 actual=");
		for (unsigned int i = 0; i < sizeof(digest); i++)
			printk(BIOS_INFO, "%02x", digest[i]);
		printk(BIOS_INFO, "\n");
	}
	printk(BIOS_INFO, "NATIVE156 SPI13 RESULT completed_bytes=%u ret=%d status=%08x "
		"owned_restore=%d controls_same=%d whole_sha256_match=%d\n",
		completed, ret, status, restore, controls_same, match);
	uart_tx_flush(1);
	if (ret || completed != N99_MEDIA_BYTES ||
	    !restore || !controls_same || !match)
		refuse("SPI13 medium incomplete/SHA256 mismatch; no fallback/retry");
	printk(BIOS_INFO, "NATIVE156 SPI13 MEDIUM SHA256 VERIFIED "
		"bytes=524288; CONTINUE SAME ROM/PACKET\n");
	uart_tx_flush(1);
}

/* Keep the verified physicalM device/cache transition, without loading or
 * entering its now-unused second ROMstage. Profile admission precedes these
 * CBFS calls because metadata-cache lookup can inspect the external IMD bank.
 */
static void native161_prepare_verified_medium(void)
{
	const struct cbfs_boot_device *cbd;
	void *mapping;

	_Static_assert(CONFIG(NO_FMAP_CACHE) && CONFIG(NO_CBFS_MCACHE),
		"The stage transition uses a fresh read-only CBFS lookup");
	if (native94_medium_ready)
		refuse("repeated CBFS medium activation");
	native94_medium_ready = 1;
	cbd = cbfs_get_boot_device(true);
	if (!cbd || region_device_sz(&cbd->rdev) != 0x7f000)
		refuse("CBFS verified device geometry");
	mapping = rdev_mmap_full(&cbd->rdev);
	if (mapping != (void *)(uintptr_t)(N99_MEDIA + 0x1000))
		refuse("CBFS verified COREBOOT mapping");
	rdev_munmap(&cbd->rdev, mapping);
	printk(BIOS_INFO, "B565D4 verified CBFS ready; direct ramstage next\n");
}

static void context(void)
{
	volatile struct native84_context_image *image =
		(void *)(uintptr_t)N99_CONTEXT;
	/* Exact standard stock selector arrangement, no foreign72-byte read. */
	static const uint8_t standard_gdt[32] = {
		0, 0, 0, 0, 0, 0, 0, 0,
		0xff, 0xff, 0, 0, 0, 0x9b, 0xcf, 0,
		0xff, 0xff, 0, 0, 0, 0x93, 0xcf, 0,
		0xff, 0xff, 0, 0, 0, 0x9b, 0xaf, 0,
	};

	clear(N99_CONTEXT, sizeof(*image));
	copy(N99_CONTEXT + NATIVE84_OFF_GDT2, standard_gdt, sizeof(standard_gdt));
	image->gdtr2.limit = NATIVE84_GDT_BYTES - 1;
	image->gdtr2.base = N99_CONTEXT + NATIVE84_OFF_GDT2;
	image->idtr_08.limit = NATIVE84_IDT_BYTES - 1;
	image->idtr_08.base = N99_CONTEXT + NATIVE84_OFF_IDT_08;
	image->stack_10.esp = N99_A + 0xc000;
	image->stack_10.ss = 0x10;
	image->far_08.offset = N99_BRIDGE_08;
	image->far_08.selector = 8;
	image->entry = N99_ROM;
	image->descriptor = N99_A;
	for (uint32_t i = 0; i < 256; i++) {
		volatile uint8_t *g = image->idt_08 + i * 8;
		g[0] = N99_FAULT & 255;
		g[1] = (N99_FAULT >> 8) & 255;
		g[2] = 8; g[3] = 0; g[4] = 0; g[5] = 0x8e;
		g[6] = (N99_FAULT >> 16) & 255;
		g[7] = N99_FAULT >> 24;
		if (g[0] != (N99_FAULT & 255) ||
		    g[1] != ((N99_FAULT >> 8) & 255) || g[2] != 8 ||
		    g[3] || g[4] || g[5] != 0x8e ||
		    g[6] != ((N99_FAULT >> 16) & 255) || g[7] != (N99_FAULT >> 24))
			refuse("IDT gate readback");
	}
	if (image->gdtr2.limit != 87 ||
	    image->gdtr2.base != N99_CONTEXT + NATIVE84_OFF_GDT2 ||
	    image->idtr_08.limit != 2047 ||
	    image->idtr_08.base != N99_CONTEXT + NATIVE84_OFF_IDT_08 ||
	    image->stack_10.esp != N99_A + 0xc000 || image->stack_10.ss != 0x10 ||
	    image->far_08.offset != N99_BRIDGE_08 || image->far_08.selector != 8 ||
	    image->entry != N99_ROM || image->descriptor != N99_A)
		refuse("context scalar readback");
}

static void cpu_mode(uint32_t *cr0, uint32_t *cr3, uint32_t *cr4)
{
	asm volatile ("mov %%cr0,%0" : "=r" (*cr0));
	asm volatile ("mov %%cr3,%0" : "=r" (*cr3));
	asm volatile ("mov %%cr4,%0" : "=r" (*cr4));
}

static void native95_context_check(void)
{
	struct native84_table_pointer gdtr, idtr;
	uint32_t cr0, cr3, cr4, flags;
	uint16_t cs, ds, es, ss, fs, gs;
	uintptr_t sp;

	asm volatile ("sgdt %0" : "=m" (gdtr) : : "memory");
	asm volatile ("sidt %0" : "=m" (idtr) : : "memory");
	asm volatile ("pushfl; popl %0" : "=r" (flags) : : "memory");
	asm volatile ("mov %%cs,%0" : "=r" (cs));
	asm volatile ("mov %%ds,%0" : "=r" (ds));
	asm volatile ("mov %%es,%0" : "=r" (es));
	asm volatile ("mov %%ss,%0" : "=r" (ss));
	asm volatile ("mov %%fs,%0" : "=r" (fs));
	asm volatile ("mov %%gs,%0" : "=r" (gs));
	asm volatile ("mov %%esp,%0" : "=r" (sp));
	cpu_mode(&cr0, &cr3, &cr4);
	if (gdtr.limit != NATIVE84_GDT_BYTES - 1 ||
	    gdtr.base != N99_CONTEXT + NATIVE84_OFF_GDT2 ||
	    idtr.limit != NATIVE84_IDT_BYTES - 1 ||
	    idtr.base != N99_CONTEXT + NATIVE84_OFF_IDT_08 ||
	    (flags & 0x600) || cs != 8 || ds != 16 || es != 16 || ss != 16 ||
	    fs || gs || cr0 != 0x10011 || cr3 || cr4 != 0x600 ||
	    sp < (uintptr_t)_earlyram_stack || sp >= (uintptr_t)_eearlyram_stack)
		refuse("prog_run context/CPU/owned stack readback");
	printk(BIOS_INFO, "NATIVE99 PROG_RUN CONTEXT gdtr=%x/%x idtr=%x/%x esp=%lx\n",
		gdtr.base, gdtr.limit, idtr.base, idtr.limit, sp);
	printk(BIOS_INFO, "NATIVE99 PROG_RUN CPU=%x/%x/%x flags=%x "
		"selectors=%x/%x/%x/%x/%x/%x\n",
		cr0, cr3, cr4, flags, cs, ds, es, ss, fs, gs);
	uart_tx_flush(1);
}

void __noreturn native99_prepare_and_enter(const uint8_t *snapshot, uint32_t bytes)
{
	struct native99_apob_selection selection;
	struct native99_span excluded[7];
	struct b565d4_smm_state fresh_smm;
	struct b565d4_smm_regions smm_regions;
	uint32_t excluded_count = 6;
	struct native84_handoff descriptor = {
		.magic = NATIVE84_HANDOFF_MAGIC, .version = 1, .bytes = 56,
		.epoch = N99_EPOCH, .parent_base = N99_A, .parent_bytes = 0x800000,
		.early_base = N99_A, .early_bytes = 0x10000,
		.cbmem_bottom = N99_CBMEM, .cbmem_top = N99_END,
		.media_base = N99_MEDIA, .media_bytes = N99_MEDIA_BYTES,
		.handle_bank_base = N99_A + 0x40, .handle_bank_bytes = 32,
	};
	struct native99_apob_packet packet = {
		.magic = NATIVE99_PACKET_MAGIC, .version = NATIVE99_PACKET_VERSION,
		.header_bytes = NATIVE99_PACKET_HEADER_BYTES,
		.epoch = N99_EPOCH, .parent_base = N99_A, .parent_bytes = 0x800000,
		.apob_bytes = bytes, .reserved = 0,
	};
	uint32_t cr0, cr3, cr4, flags, bits;
	uint16_t cs, ds, es, ss;
	uintptr_t sp;

	/* The existing95 code/BSS/4KiB stack are caller-qualified before entry.
	 * No parent store occurs before the complete fixed-arena selection.
	 */
	printk(BIOS_INFO, "NATIVE99 EARLY ENTER snapshot_bytes=%x\n", bytes);
	uart_tx_flush(1);
	asm volatile ("pushfl; popl %0" : "=r" (flags) : : "memory");
	asm volatile ("mov %%cs,%0; mov %%ds,%1; mov %%es,%2; mov %%ss,%3"
		: "=r" (cs), "=r" (ds), "=r" (es), "=r" (ss));
	asm volatile ("mov %%esp,%0" : "=r" (sp));
	cpu_mode(&cr0, &cr3, &cr4);
	if ((flags & 0x600) || cs != 8 || ds != 16 || es != 16 || ss != 16 ||
	    cr0 != 0x11 || cr3 || cr4 != 0x200 ||
	    !b565d4_is_selected_cpu(cpuid_eax(1)) || cpuid_eax(0x80000000) < 0x80000008 ||
	    (cpuid(1).edx & ((1U << 24) | (1U << 25))) != ((1U << 24) | (1U << 25)))
		refuse("fresh CPU mode/SSE");
	if ((uintptr_t)_program >= (uintptr_t)_eprogram ||
	    (uintptr_t)snapshot < (uintptr_t)_program ||
	    (uintptr_t)snapshot > (uintptr_t)_eprogram ||
	    bytes > (uintptr_t)_eprogram - (uintptr_t)snapshot ||
	    (uintptr_t)_earlyram_stack >= (uintptr_t)_eearlyram_stack ||
	    sp < (uintptr_t)_earlyram_stack || sp >= (uintptr_t)_eearlyram_stack)
		refuse("existing own code/snapshot/stack");
	bits = cpuid_eax(0x80000008) & 255;
	/* Preserve active/locked SMM ranges; do not clear firmware protection. */
	if (!b565d4_read_smm_state(&packet.smm) ||
	    !b565d4_decode_smm_regions(packet.smm.smbase, packet.smm.smm_addr,
		packet.smm.smm_mask, packet.smm.hwcr, bits, &smm_regions))
		refuse("fresh SMM snapshot or range geometry");

	excluded[0] = (struct native99_span) { 0, 0x100000 };
	excluded[1] = (struct native99_span) { 0x09000000, 0x0b100000 };
	excluded[2] = (struct native99_span) { 0xbe000000, 0xbf000000 };
	excluded[3] = (struct native99_span) {
		(uintptr_t)_program, (uintptr_t)_eprogram };
	excluded[4] = (struct native99_span) {
		(uintptr_t)_earlyram_stack, (uintptr_t)_eearlyram_stack };
	excluded[5] = (struct native99_span) {
		smm_regions.smbase_begin, smm_regions.smbase_end };
	if (smm_regions.flags & B565D4_SMM_TSEG_VALID)
		excluded[excluded_count++] = (struct native99_span) {
			smm_regions.tseg_begin, smm_regions.tseg_end };
	if (native99_apob_select(snapshot, bytes, bits, excluded, excluded_count, &selection))
		refuse("positive Available/exclusion selection");
	if (bytes > NATIVE99_PACKET_END - NATIVE99_PACKET_BASE - sizeof(packet))
		refuse("APOB packet exceeds its reserved extent");

	printk(BIOS_INFO, "NATIVE99 POSITIVE APOB gap=%llx..%llx SELECT=%x..%x\n",
		selection.available_begin, selection.available_end, N99_A, N99_END);
	uart_tx_flush(1);

	if (!b565d4_read_smm_state(&fresh_smm) ||
	    !b565d4_same_smm_state(&packet.smm, &fresh_smm))
		refuse("SMM state changed before first parent write");
	printk(BIOS_INFO, "B565D4 SMM HANDOFF base=%llx addr=%llx mask=%llx hwcr=%llx "
		"TSEG=%llx..%llx flags=%x\n", packet.smm.smbase, packet.smm.smm_addr,
		packet.smm.smm_mask, packet.smm.hwcr, smm_regions.tseg_begin,
		smm_regions.tseg_end, smm_regions.flags);
	uart_tx_flush(1);

	/* First parent write. Every later error is terminal, never retry/OEM. */
	clear(N99_A + 0x40, 32); /* External creator/recovery IMD slots, both fresh. */
	printk(BIOS_INFO, "NATIVE156 BANK ZERO; READ PHYSICAL SPI MEDIUM\n");
	uart_tx_flush(1);
	native156_spi13_load_medium();
	printk(BIOS_INFO, "NATIVE156 VERIFIED SPI MEDIUM; COPY PACKET/CONTEXT\n");
	uart_tx_flush(1);
	copy(NATIVE99_PACKET_BASE + sizeof(packet), snapshot, bytes);
	copy(NATIVE99_PACKET_BASE, (const void *)&packet, sizeof(packet));
	copy(N99_A, (const void *)&descriptor, sizeof(descriptor));
	copy(N99_BRIDGE, native99_bridge, sizeof(native99_bridge));
	context();
	printk(BIOS_INFO, "NATIVE99 COPIES/PACKET/CONTEXT COMPLETE; CPU PRE=%x/%x/%x\n",
		cr0, cr3, cr4);
	uart_tx_flush(1);
	/* Root-selected CPU initialization only: WP and OSXMMEXCPT. */
	asm volatile ("mov %0,%%cr0" : : "r" (cr0 | 0x10000U) : "memory");
	asm volatile ("mov %0,%%cr4" : : "r" (cr4 | 0x400U) : "memory");
	cpu_mode(&cr0, &cr3, &cr4);
	printk(BIOS_INFO, "NATIVE99 CPU POST=%x/%x/%x; PREPARE DIRECT RAMSTAGE\n",
		cr0, cr3, cr4);
	uart_tx_flush(1);
	if (cr0 != 0x10011 || cr3 || cr4 != 0x600)
		refuse("explicit CPU-init readback");
	native95_context_prepare((const void *)(uintptr_t)N99_CONTEXT);
	native95_context_check();
	/* WholeM/SHA, packet/descriptor/context and CPU are now admitted. The
	 * bank was zeroed before SPI, so pre-creation CBFS metadata lookup sees
	 * a stable empty IMD handle, not an unqualified foreign allocator.
	 */
	native84_stage_admit((const void *)(uintptr_t)N99_A);
	printk(BIOS_INFO, "NATIVE161 FIRST ROM DESCRIPTOR ADMITTED; SECOND_ROM_EXEC=0\n");
	uart_tx_flush(1);
	native161_prepare_verified_medium();
	native84_romstage_main();
	refuse("direct run_ramstage returned");
}
