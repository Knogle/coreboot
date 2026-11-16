/* SPDX-License-Identifier: GPL-2.0-only */
/* DRAM-first reset observer and launch adapter, followed by genuine INIT/SIPI.
 * Cezanne is qualified on this board. The selected desktop branch uses the
 * desktop launch register and APOB schema and still needs physical validation.
 */
#include "native132_core_launch.h"
#include "b565d4_cpu_topology.h"
#if CONFIG(B565D4_DESKTOP_NATIVE)
#include "desktop_topology.h"
#endif
#include "b565d4_cpu.h"
#include "native84_profile.h"
#include <amdblocks/cpu.h>
#include <amdblocks/smn.h>
#include <arch/cpuid.h>
#include <console/console.h>
#include <console/uart.h>
#include <cpu/amd/mtrr.h>
#include <cpu/x86/cache.h>
#include <cpu/x86/cr.h>
#include <cpu/x86/msr.h>
#include <cpu/x86/mtrr.h>
#include <delay.h>
#include <string.h>
#include <types.h>

#define APOB_BASE 0x0a200000U
#define APOB_CAP 0x28000U
#define TOPOLOGY_BYTES B565D4_CEZANNE_TOPOLOGY_BYTES
#if CONFIG(B565D4_DESKTOP_NATIVE)
#define LAUNCH_MASK 0x30081018U
#define SMU_RESPONSE 0x03b1057cU
#define SMU_MESSAGE 0x03b10530U
#define SMU_ARG0 0x03b109c4U
#else
#define LAUNCH_MASK 0x0005a818U
#define SMU_RESPONSE 0x03b10564U
#define SMU_MESSAGE 0x03b10528U
#define SMU_ARG0 0x03b10998U
#endif
#define WAIT_STEPS 10000U
#define WAIT_US 100U

struct native132_saved_msr { uint32_t index, lo, hi; } __packed;
struct native132_ap_record {
	uint32_t done, launch_bit, reset_cs, cpuid1_eax, cpuid1_ebx;
	uint32_t topology_eax, topology_ebx, apic_base_lo, apic_base_hi;
	uint32_t cr0, cr4, slot;
};
_Static_assert(sizeof(uintptr_t) == 4 && sizeof(struct native132_saved_msr) == 12,
	"Source74 genuine IA32 saved-MSR layout");
_Static_assert(sizeof(struct native132_ap_record) == NATIVE132_RECORD_BYTES,
	"Source74 stackless ASM record layout");
_Static_assert(NATIVE132_MSR_OFFSET + NATIVE132_MSR_COUNT * 12 ==
	NATIVE132_RECORDS_OFFSET, "Source74 bounded table before parameters");
_Static_assert(CONFIG_PSP_APOB_DRAM_ADDRESS == APOB_BASE &&
	CONFIG_PSP_APOB_DRAM_RESERVED_SIZE == APOB_CAP && CONFIG_MAX_CPUS == 16,
	"Source74 selected current topology/backing");

extern const uint8_t native132_reset_image[], native132_reset_image_end[];
static uint8_t apob_copy[APOB_CAP];
static uint8_t reset_backup[NATIVE132_RESET_BYTES];
static uint8_t reset_expected[NATIVE132_RESET_BYTES];
static volatile struct native132_ap_record records[NATIVE132_RECORD_SLOTS];
static bool attempted, patched;
static unsigned int released;
static struct b565d4_cpu_topology cpu_topology;
static struct {
	uint8_t ordinal, physical_core, physical_complex, thread;
} launch_topology[NATIVE132_RECORD_SLOTS];

static uint32_t le32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
		(uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static volatile uint16_t *entry_count(void)
{
	return (void *)(NATIVE132_RESET_BASE + NATIVE132_ENTER_OFFSET);
}

static volatile uint16_t *ready_count(void)
{
	return (void *)(NATIVE132_RESET_BASE + NATIVE132_READY_OFFSET);
}

static void log_records(void)
{
	printk(BIOS_INFO, "NATIVE132 RESET entered=%u ready=%u released=%u patched=%u\n",
		patched ? *entry_count() : 0, patched ? *ready_count() : 0,
		released, patched);
	for (unsigned int i = 0; i < NATIVE132_RECORD_SLOTS; i++) {
		const volatile struct native132_ap_record *r = &records[i];
		if (!r->done)
			continue;
		printk(BIOS_INFO, "NATIVE132 AP slot=%u tag=%08x bit=%u resetCS=%x "
			"CPUID1=%08x/%08x topo=%08x/%08x APIC_BASE=%08x%08x "
			"CR0=%08x CR4=%08x\n", r->slot, r->done, r->launch_bit,
			r->reset_cs, r->cpuid1_eax, r->cpuid1_ebx,
			r->topology_eax, r->topology_ebx, r->apic_base_hi,
			r->apic_base_lo, r->cr0, r->cr4);
	}
}

static void __noreturn stop(const char *why)
{
	printk(BIOS_ERR, "NATIVE132 STOP %s; no retry/reset restoration\n", why);
	log_records();
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	native84_terminal();
}

static void copy_apob(void)
{
	const volatile uint8_t *src = (const void *)APOB_BASE;
	uint8_t head[16];
	for (size_t i = 0; i < sizeof(head); i++)
		head[i] = src[i];
	const uint32_t bytes = le32(head + 8), first = le32(head + 12);
	/* The selected supplier schema is also checked in romstage. Read only
	 * this bounded container; never follow opaque record pointers.
	 */
	if (le32(head) != 0x424f5041 ||
	    !b565d4_apob_header_valid(le32(head + 4), first, bytes, APOB_CAP))
		stop("APOB header/extent/schema");
	for (uint32_t i = 0; i < bytes; i++)
		apob_copy[i] = src[i];
	if (memcmp(head, apob_copy, sizeof(head)))
		stop("APOB header changed during copy");
	for (uint32_t i = 0; i < bytes; i++)
		if (apob_copy[i] != src[i])
			stop("APOB full repeated comparison changed");
	printk(BIOS_INFO, "NATIVE132 APOB fullcopy bytes=%x first=%x version=%u\n",
		bytes, first, le32(head + 4));
}

static uint32_t topology_mask(void)
{
	copy_apob();
	const uint32_t bytes = le32(apob_copy + 8);
	const uint8_t *top = NULL;
	uint32_t offset = le32(apob_copy + 12), n = 0, mask = 0;
	while (offset < bytes) {
		if (++n > 512 || bytes - offset < 16)
			stop("APOB record count/header");
		const uint8_t *p = apob_copy + offset;
		const uint32_t size = le32(p + 12);
		if (size < 16 || (size & 3) || size > bytes - offset)
			stop("APOB record extent");
		if (le32(p) == 3 && le32(p + 4) == 3 && !le32(p + 8)) {
			if (top || size != TOPOLOGY_BYTES)
				stop("APOB duplicate/size3/3/0");
			top = p;
		}
		offset += size;
	}
	if (offset != bytes || !top)
		stop("APOB missing3/3/0/final extent");
#if CONFIG(B565D4_DESKTOP_NATIVE)
	struct am4_desktop_topology desktop;
	const enum b565d4_cpu_family family = b565d4_cpu_family(cpuid_eax(1));

	if (!b565d4_is_selected_cpu(cpuid_eax(1)) ||
	    !am4_desktop_decode_topology(family, top, TOPOLOGY_BYTES,
					get_cpu_count(), &desktop) || desktop.ccd_count != 1 ||
	    desktop.physical_ccd[0] || desktop.logical_ccd[0])
		stop("Desktop single-CCD APOB/CPUID topology mismatch");
	/* The prepared reset observer has sixteen slots. Admit one desktop CCD
	 * before touching its mask; multiple CCDs need separately tagged resets.
	 */
	cpu_topology.threads_per_core = desktop.threads_per_core;
	cpu_topology.logical_threads = desktop.thread_count;
	cpu_topology.launch_mask = desktop.launch_mask[0];
	memset(cpu_topology.physical_core, 0xff, sizeof(cpu_topology.physical_core));
	for (unsigned int i = 0; i < desktop.thread_count; i++) {
		const struct am4_desktop_thread *thread = &desktop.threads[i];
		const unsigned int bit = thread->launch_bit;

		launch_topology[bit].ordinal = thread->ccx * 4 + thread->core;
		launch_topology[bit].physical_core = thread->physical_core;
		launch_topology[bit].physical_complex = thread->physical_ccx;
		launch_topology[bit].thread = thread->thread;
	}
#else
	if (!b565d4_decode_cezanne_topology(top, TOPOLOGY_BYTES, get_cpu_count(),
					 &cpu_topology))
		stop("APOB/CPUID Cezanne topology mismatch");
	for (unsigned int bit = 0;
	     bit < cpu_topology.threads_per_core * B565D4_CEZANNE_MAX_CORES; bit++) {
		const unsigned int core = bit / cpu_topology.threads_per_core;

		launch_topology[bit].ordinal = core;
		launch_topology[bit].physical_core = cpu_topology.physical_core[core];
		launch_topology[bit].thread = bit % cpu_topology.threads_per_core;
	}
#endif
	mask = cpu_topology.launch_mask;
	for (unsigned int bit = 0; bit < NATIVE132_RECORD_SLOTS; bit++)
		if (mask & (1U << bit))
			printk(BIOS_INFO, "NATIVE132 TOPO complex=%u ordinal=%u "
			       "physical=%u thread=%u bit=%u\n",
			       launch_topology[bit].physical_complex,
			       launch_topology[bit].ordinal,
			       launch_topology[bit].physical_core,
			       launch_topology[bit].thread, bit);
	return mask;
}

static void validate_msrs(const struct native132_saved_msr *table, size_t count)
{
	static const uint32_t first[] = { SYSCFG_MSR, TOP_MEM_MSR, TOP_MEM2_MSR,
		MTRR_IORR0_BASE, MTRR_IORR0_MASK, MTRR_IORR1_BASE, MTRR_IORR1_MASK };
	static const uint32_t fixed[] = { 0x250, 0x258, 0x259, 0x268, 0x269,
		0x26a, 0x26b, 0x26c, 0x26d, 0x26e, 0x26f };
	if (count != NATIVE132_MSR_COUNT || !table || rdmsr(MTRR_CAP_MSR).lo != 0x508)
		stop("not the current genuine36-MSR snapshot");
	for (size_t i = 0; i < 7; i++)
		if (table[i].index != first[i])
			stop("MSR prefix order");
	for (size_t i = 0; i < 11; i++)
		if (table[7 + i].index != fixed[i])
			stop("fixed MSR order");
	for (size_t i = 0; i < 16; i++)
		if (table[18 + i].index != 0x200 + i)
			stop("variable MSR order");
	const msr_t sys = rdmsr(SYSCFG_MSR);
	if (table[34].index != MTRR_DEF_TYPE_MSR || table[35].index != SYSCFG_MSR ||
	    table[35].lo != sys.lo || table[35].hi != sys.hi ||
	    table[0].lo != (sys.lo | SYSCFG_MSR_MtrrFixDramModEn) ||
	    table[0].hi != sys.hi)
		stop("DEF/SYS order/current value");
}

static bool reset_bytes_match(bool counters_mutable)
{
	const volatile uint8_t *p = (const void *)NATIVE132_RESET_BASE;
	for (size_t i = 0; i < NATIVE132_RESET_BYTES; i++) {
		if (counters_mutable && i >= NATIVE132_ENTER_OFFSET)
			continue;
		if (p[i] != reset_expected[i])
			return false;
	}
	return true;
}

static void install_reset(const struct native132_saved_msr *table)
{
	volatile uint8_t *p = (void *)NATIVE132_RESET_BASE;
	if ((uintptr_t)native132_reset_image_end - (uintptr_t)native132_reset_image !=
	    NATIVE132_RESET_BYTES)
		stop("compiled reset blob size");
	memcpy(reset_expected, native132_reset_image, sizeof(reset_expected));
	memcpy(reset_expected + NATIVE132_MSR_OFFSET, table,
		NATIVE132_MSR_COUNT * sizeof(*table));
	const uint32_t record_ptr = (uintptr_t)records;
	memcpy(reset_expected + NATIVE132_RECORDS_OFFSET, &record_ptr, sizeof(record_ptr));
	memset((void *)records, 0, sizeof(records));
	for (size_t i = 0; i < sizeof(reset_backup); i++)
		reset_backup[i] = p[i];
	for (size_t i = 0; i < sizeof(reset_backup); i++)
		if (reset_backup[i] != p[i])
			stop("reset backup not stable");
	/* No restore path after this first store until MP succeeds. The reset
	 * vector itself is published last, before the single flush/readback.
	 */
	patched = true;
	for (size_t i = 0; i < sizeof(reset_expected); i++)
		if (i < NATIVE132_VECTOR_OFFSET || i >= NATIVE132_VECTOR_OFFSET + 4)
			p[i] = reset_expected[i];
	for (size_t i = NATIVE132_VECTOR_OFFSET; i < NATIVE132_VECTOR_OFFSET + 4; i++)
		p[i] = reset_expected[i];
	wbinvd();
	if (!reset_bytes_match(false))
		stop("whole reset patch readback");
	printk(BIOS_INFO, "NATIVE132 RESET installed09fffbf0..0a000000 bytes=1040 "
		"table=36x12 GDT09ffffa0 vector09fffff0\n");
}

static uint32_t smu_message_one(void)
{
	/* Exact selected Original6c48 subset; finite timeout added. No second
	 * message/retry, and no semantic name inferred from message number1.
	 */
	printk(BIOS_INFO, "NATIVE132 SMU old_response=%08x\n", smn_read32(SMU_RESPONSE));
	smn_write32(SMU_RESPONSE, 0);
	for (unsigned int i = 0; i < 6; i++)
		smn_write32(SMU_ARG0 + i * 4, i ? 0 : 0xaa55aa55U);
	smn_write32(SMU_MESSAGE, 1);
	uint32_t response = 0;
	for (unsigned int i = 0; i < WAIT_STEPS; i++) {
		response = smn_read32(SMU_RESPONSE);
		if (response)
			break;
		udelay(WAIT_US);
	}
	printk(BIOS_INFO, "NATIVE132 SMU raw_completion=%08x", response);
	for (unsigned int i = 0; i < 6; i++)
		printk(BIOS_INFO, " arg%u=%08x", i, smn_read32(SMU_ARG0 + i * 4));
	printk(BIOS_INFO, "\n");
	if (!response || response == 0xfc)
		stop("SMU timeout/vendor-fc");
	return response;
}

void native132_release_before_init(const void *saved_msrs, size_t count)
{
	native84_stage_check();
	if (attempted)
		stop("second release attempt");
	attempted = true;
	validate_msrs(saved_msrs, count);
	uint32_t desired = topology_mask();
	const uint32_t current = smn_read32(LAUNCH_MASK);
	/* Launch masks include the BSP in the APOB model. The running desktop
	 * reference has bit0 clear in 30081018 with all twelve CPUs online.
	 * Never use that bit to decide whether to release the already running BSP.
	 */
	if (CONFIG(B565D4_DESKTOP_NATIVE))
		desired &= ~1U;
	const struct cpuid_result bsp = cpuid(1);
	printk(BIOS_INFO, "NATIVE132 mask=%08x desired=%08x BSP_CPUID1=%08x/%08x\n",
		current, desired, bsp.eax, bsp.ebx);
	if ((current & desired) == desired) {
		printk(BIOS_INFO, "NATIVE132 no missing bits; no launch/reset writes\n");
		return;
	}
	/* The selected BSP tuple is ordinal0/thread0, not every AP's ID map. */
	if ((!CONFIG(B565D4_DESKTOP_NATIVE) && !(current & 1)) || (bsp.ebx >> 24) != 0 ||
	    !(rdmsr(0x1b).lo & (1U << 8)))
		stop("unqualified BSP ordinal0/reset state");
	install_reset(saved_msrs);
	for (unsigned int bit = 1; bit < NATIVE132_RECORD_SLOTS; bit++) {
		if (!(desired & (1U << bit)) || (current & (1U << bit)))
			continue;
		if (*entry_count() != released || *ready_count() != released)
			stop("unexpected reset entries before next launch");
		uint32_t before = smn_read32(LAUNCH_MASK);
		if (before & (1U << bit))
			stop("missing launch bit changed asynchronously");
		/* Select one launch ordinal for the AP's observation record. */
		memcpy(reset_expected + NATIVE132_LAUNCH_BIT_OFFSET, &bit, sizeof(bit));
		volatile uint32_t *selected_bit = (void *)(NATIVE132_RESET_BASE +
			NATIVE132_LAUNCH_BIT_OFFSET);
		*selected_bit = bit;
		wbinvd();
		if (!reset_bytes_match(true))
			stop("reset code/table changed before launch");
		const uint32_t after = before | (1U << bit);
		printk(BIOS_INFO, "NATIVE132 launch ordinal=%u physical=%u thread=%u "
		       "mask=%08x->%08x\n", launch_topology[bit].ordinal,
			launch_topology[bit].physical_core,
			launch_topology[bit].thread, before, after);
		smn_write32(LAUNCH_MASK, after);
		if (smn_read32(LAUNCH_MASK) != after)
			stop("launch mask RMW readback");
		released++;
		smu_message_one();
		for (unsigned int i = 0; i < WAIT_STEPS; i++) {
			if (*entry_count() == released && *ready_count() == released)
				break;
			udelay(WAIT_US);
		}
		log_records();
		if (*entry_count() != released || *ready_count() != released ||
		    records[released - 1].done != NATIVE132_RECORD_DONE ||
		    records[released - 1].launch_bit != bit ||
		    !(records[released - 1].apic_base_lo & (1U << 11)))
			stop("per-thread first-reset/mirror/APIC-enable readback");
	}
	if ((smn_read32(LAUNCH_MASK) & desired) != desired || !reset_bytes_match(true))
		stop("final mask/reset body");
	printk(BIOS_INFO, "NATIVE132 first-reset ready; now genuine INIT/SIPI\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

void native132_restore_after_mp_success(void)
{
	/* Integration calls this only after genuine MP success and validation of
	 * all actual enabled CPU devices. No failed/parked-AP cleanup invokes it.
	 */
	if (!attempted)
		stop("missing release seam");
	if (!patched)
		return;
	log_records();
	if (*entry_count() != released || *ready_count() != released ||
	    !reset_bytes_match(true))
		stop("post-MP reset body/counters");
	volatile uint8_t *p = (void *)NATIVE132_RESET_BASE;
	for (size_t i = 0; i < sizeof(reset_backup); i++)
		p[i] = reset_backup[i];
	wbinvd();
	for (size_t i = 0; i < sizeof(reset_backup); i++)
		if (p[i] != reset_backup[i])
			stop("post-success reset restore readback");
	patched = false;
	printk(BIOS_INFO, "NATIVE132 restored whole reset window after MP success\n");
}
