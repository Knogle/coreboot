/* SPDX-License-Identifier: GPL-2.0-only */
/* Entry1 BSP registration; entry2 genuine no-SMM MP and payload continuation. */
#include <commonlib/bsd/compiler.h>
#include "native84_profile.h"
#include "native84_context.h"
#include "native132_core_launch.h"
#include "b565d4_cpu.h"
#include "matisse_cpu.h"
#include "vermeer_cpu.h"
#include <amdblocks/amd_pci_util.h>
#include <amdblocks/cpu.h>
#include <amdblocks/cppc.h>
#include <amdblocks/vermeer_cpu_name.h>
#include <arch/cpu.h>
#include <console/console.h>
#include <console/uart.h>
#include <cpu/amd/microcode.h>
#include <cpu/x86/gdt.h>
#include <cpu/x86/lapic.h>
#include <cpu/x86/mp.h>
#include <device/device.h>
#include <stddef.h>
#include <string.h>
#include <symbols.h>
#include <types.h>

enum { BSP_GS_SELECTOR = 0x40, STOCK_GDT_BYTES = 0xc0 };
struct gdtr32 {
	uint16_t limit;
	uint32_t base;
} __packed;

_Static_assert(sizeof(uintptr_t) == 4 && CONFIG_MAX_CPUS == 16,
	"This draft covers the selected IA32/16-slot ramstage GDT only");
_Static_assert(sizeof(struct gdtr32) == 6 && offsetof(struct gdtr32, base) == 2,
	"IA32 SGDT operand layout");

static unsigned int bsp_registration_attempted;
extern struct cpu_info cpu_infos[CONFIG_MAX_CPUS];

/* Caller separately qualifies the loaded ramstage, current stack and parent
 * mapping/lifetime before entry. Numeric containment is not that qualification.
 */
static bool own_span(uintptr_t base, size_t bytes)
{
	const struct native84_handoff *d = native84_descriptor();
	uintptr_t first = (uintptr_t)_program, end = (uintptr_t)_eprogram;

	return first < end && first >= d->cbmem_bottom && end <= d->cbmem_top &&
		base >= first && base <= end && bytes <= end - base;
}

static uint16_t current_gs(void)
{
	uint16_t gs;

	asm volatile ("mov %%gs,%0" : "=r" (gs));
	return gs;
}

static void bsp_refusal(unsigned int line) __noreturn;
static void bsp_refusal(unsigned int line)
{
	printk(BIOS_ERR, "NATIVE88 BSP REFUSAL line=%u\n", line);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	native84_terminal();
}

static void admit_stock_gdt(struct gdtr32 *gdtr)
{
	uintptr_t first = (uintptr_t)gdt, end = (uintptr_t)gdt_end;
	static const uint8_t code[8] = { 0xff, 0xff, 0, 0, 0, 0x9b, 0xcf, 0 };
	static const uint8_t data[8] = { 0xff, 0xff, 0, 0, 0, 0x93, 0xcf, 0 };

	/* c_start, not the earlier Context2 bridge, must have loaded this GDT. */
	asm volatile ("sgdt %0" : "=m" (*gdtr) : : "memory");
	if (end <= first || end - first != STOCK_GDT_BYTES ||
	    !own_span(first, STOCK_GDT_BYTES) || gdtr->base != first ||
	    gdtr->limit != STOCK_GDT_BYTES - 1 ||
	    (uintptr_t)per_cpu_segment_descriptors != first + BSP_GS_SELECTOR ||
	    !own_span((uintptr_t)&per_cpu_segment_selector, sizeof(per_cpu_segment_selector)) ||
	    per_cpu_segment_selector != BSP_GS_SELECTOR ||
	    memcmp(gdt + GDT_CODE_SEG, code, sizeof(code)) ||
	    memcmp(gdt + GDT_DATA_SEG, data, sizeof(data)))
		bsp_refusal(__LINE__);
}

/* Generic child/root routing stays absent; Source27's explicit DSDT root
 * SDHC INTA/GSI16 row and one FCH selector write remain separate. CPU SSDT
 * uses the proven one-BSP/C1 wrapper; no AP startup or CPU-driver completion.
 */
const struct pci_routing_info *get_pci_routing_table(size_t *entries)
{
	/* Generic GNB/PIC tables remain omitted. The Source27 static root
	 * SDHC-only _PRT requires no PICM or INT[A-H] link objects.
	 */
	if (entries)
		*entries = 0;
	return NULL;
}

enum cb_err get_ccx_cppc_min_frequency(uint32_t *freq)
{
	(void)freq;
	return CB_ERR;
}

enum cb_err get_ccx_cppc_nom_frequency(uint32_t *freq)
{
	(void)freq;
	return CB_ERR;
}

/* Root selected [30000,40000) as temporary genuine SIPI scratch using
 * the established Native125/Source66 RAM contract, before tables/payload.
 * Keep the current MTRR/SMM policy: only the microcode blob is loaded here;
 * the genuine CPU driver applies it per CPU and SIPI still mirrors BSP MTRRs.
 */
/* gdt.h declares the genuine six-byte IA32 idtarg as an external byte array.
 * Reuse the selected BSP terminal IDT; never rebuild it or rerun exception_init.
 */
static void native129_publish_ap_idt(void)
{
	const struct native84_context_image *const image =
		(const void *)(uintptr_t)NATIVE84_CONTEXT_BEGIN;
	struct native84_table_pointer current, copied;

	asm volatile ("sidt %0" : "=m" (current) : : "memory");
	if (current.limit != NATIVE84_IDT_BYTES - 1 ||
	    current.base != (uintptr_t)image->idt_08 ||
	    memcmp(&current, &image->idtr_08, sizeof(current)) ||
	    !own_span((uintptr_t)idtarg, sizeof(current)))
		bsp_refusal(__LINE__);
	memcpy(idtarg, &current, sizeof(current));
	memcpy(&copied, idtarg, sizeof(copied));
	if (memcmp(&current, &copied, sizeof(current)))
		bsp_refusal(__LINE__);
	printk(BIOS_INFO, "NATIVE136 SMP IDT limit=%04x base=%08x idtarg=%p\n",
	       (unsigned int)current.limit, current.base, idtarg);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

static int native129_smp_trial_cpu_count(void)
{
	const int count = get_cpu_count();

	printk(BIOS_INFO, "NATIVE136 SMP CPUID logical_count=%d max=%d\n",
	       count, CONFIG_MAX_CPUS);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (count < 1 || count > CONFIG_MAX_CPUS)
		bsp_refusal(__LINE__);
	return count;
}

/* Validate observed MP devices, never infer APIC IDs from launch ordinals. */
static void native132_validate_mp_devices(struct bus *cpu_bus)
{
	const struct device *cpu;
	uint32_t apic_ids[CONFIG_MAX_CPUS];
	unsigned int count = 0;

	for (cpu = cpu_bus->children; cpu; cpu = cpu->sibling) {
		if (count >= CONFIG_MAX_CPUS || !is_enabled_cpu(cpu) || !cpu->initialized ||
		    cpu->path.apic.thread_id > 1)
			bsp_refusal(__LINE__);
		for (unsigned int i = 0; i < count; i++)
			if (apic_ids[i] == cpu->path.apic.apic_id)
				bsp_refusal(__LINE__);
		apic_ids[count++] = cpu->path.apic.apic_id;
		printk(BIOS_INFO, "NATIVE136 CPU APIC=%x thread=%u initialized=%u\n",
		       cpu->path.apic.apic_id, cpu->path.apic.thread_id, cpu->initialized);
	}
	if (count != get_cpu_count())
		bsp_refusal(__LINE__);
}

static const struct mp_ops native129_smp_trial_ops = {
	.get_cpu_count = native129_smp_trial_cpu_count,
};

void mp_init_cpus(DEVTREE_CONST struct bus *cpu_bus)
{
	struct gdtr32 before, after;
	uint8_t table_before[STOCK_GDT_BYTES];
	const struct cpuid_result cpu = cpuid(1);
	struct device *bsp;
	struct cpu_info *info;
	uintptr_t gs_base;
	const uint8_t *gs_descriptor;

	native84_stage_check();
	if (!own_span((uintptr_t)&bsp_registration_attempted,
			 sizeof(bsp_registration_attempted)) || bsp_registration_attempted)
		bsp_refusal(__LINE__);
	bsp_registration_attempted = 1;
	/* The caller is the retained BSP. CPUID ID0 is this finite board trial's
	 * identity check, not a generic proof that an arbitrary caller is the BSP.
	 */
	if (!b565d4_is_selected_cpu(cpu.eax) || !(rdmsr(LAPIC_BASE_MSR).lo & (1U << 8)) ||
	    !own_span((uintptr_t)cpu_bus, sizeof(*cpu_bus)) || cpu_bus->children ||
	    !own_span((uintptr_t)cpu_infos, sizeof(cpu_infos)) || current_gs() != 0)
		bsp_refusal(__LINE__);
	admit_stock_gdt(&before);
	memcpy(table_before, gdt, sizeof(table_before));
	/* Stock c_start initializes all per-CPU slots to flat, zero-base data. */
	for (unsigned int slot = BSP_GS_SELECTOR; slot < STOCK_GDT_BYTES; slot += 8) {
		static const uint8_t data[8] = { 0xff, 0xff, 0, 0, 0, 0x93, 0xcf, 0 };

		if (memcmp(table_before + slot, data, sizeof(data)))
			bsp_refusal(__LINE__);
	}
	/* Genuine MP is entered once from a validated BSP/GDT/empty CPU bus.
	 * NO_SMM and NULL SMM/microcode/pre/post callbacks remain unchanged.
	 * Microcode is loaded explicitly below and applied by the per-CPU driver;
	 * the generic cpu_initialize flight record still performs MCA checks/clears.
	 */
	if (native84_profile_policy.entry_path == 2) {
		enum cb_err result;
		const uint32_t *microcode;

		/* amd_microcode_find() calls the genuine CBFS loader and returns its mapping. */
		microcode = amd_microcode_find();
		printk(BIOS_INFO, "NATIVE149 MICROCODE LOAD pointer=%p present=%u patch=%08x "
		       "processor_rev_matched=1 align=16\n",
		       microcode, microcode != NULL, microcode ? microcode[1] : 0);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if (microcode && ((uintptr_t)microcode & 15))
			bsp_refusal(__LINE__);
		if (!microcode)
			printk(BIOS_WARNING, "B565D4: no matching CBFS microcode; retain the PSP patch\n");

		native129_publish_ap_idt();
		if (CONFIG(B565D4_MATISSE))
			b565d4_matisse_init_msrs();
		else if (CONFIG(B565D4_VERMEER)) {
			vermeer_cpu_name_prepare();
			b565d4_vermeer_init_msrs();
		}
		printk(BIOS_INFO, "NATIVE136 SMP BEGIN genuine INIT/SIPI; no SMM; "
		       "inherited BSP MTRRs; selected per-CPU microcode\n");
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		result = mp_init_with_smm(cpu_bus, &native129_smp_trial_ops);
		if (result == CB_SUCCESS) {
			if (CONFIG(B565D4_DESKTOP_NATIVE) &&
			    b565d4_desktop_finalize_mp() != CB_SUCCESS)
				bsp_refusal(__LINE__);
			native132_validate_mp_devices(cpu_bus);
			native132_restore_after_mp_success();
			/* Every enabled CPU completed initialization and released this mapping. */
			amd_free_microcode();
		}
		printk(BIOS_INFO, "NATIVE136 SMP RETURN result=%d\n", result);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if (result != CB_SUCCESS)
			bsp_refusal(__LINE__);
		/* Genuine MP registered every CPU; validation and reset restoration
		 * above completed before returning to device init/tables/payload.
		 */
		return;
	}
	/* Genuine init_bsp() enables/configures its LAPIC before device creation.
	 * Only the early-native entry adds this BSP-only setup; retained PEI's
	 * inherited LAPIC path stays unchanged. No AP INIT/SIPI or CPU driver.
	 */
	if (native84_profile_policy.entry_path == 2) {
		msr_t apic_before = rdmsr(LAPIC_BASE_MSR), apic_after;
		uint32_t id, svr;

		printk(BIOS_INFO, "NATIVE BSP LAPIC BEFORE MSR1b=%08x:%08x\n",
		       apic_before.hi, apic_before.lo);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if ((apic_before.lo & LAPIC_BASE_MSR_X2APIC_MODE) ||
		    ((apic_before.lo & LAPIC_BASE_MSR_ENABLE) &&
		     ((apic_before.hi & 0xffU) ||
		      (apic_before.lo & LAPIC_BASE_MSR_ADDR_MASK) != LAPIC_DEFAULT_BASE)))
			bsp_refusal(__LINE__);
		enable_lapic();
		setup_lapic_interrupts();
		apic_after = rdmsr(LAPIC_BASE_MSR);
		id = lapicid();
		svr = lapic_read(LAPIC_SPIV);
		printk(BIOS_INFO, "NATIVE BSP LAPIC AFTER MSR1b=%08x:%08x ID=%08x SVR=%08x\n",
		       apic_after.hi, apic_after.lo, id, svr);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if ((apic_after.lo & (LAPIC_BASE_MSR_ENABLE | LAPIC_BASE_MSR_X2APIC_MODE)) !=
			LAPIC_BASE_MSR_ENABLE || (apic_after.hi & 0xffU) ||
		    (apic_after.lo & LAPIC_BASE_MSR_ADDR_MASK) != LAPIC_DEFAULT_BASE ||
		    id != (cpu.ebx >> 24) || !(svr & LAPIC_SPIV_ENABLE))
			bsp_refusal(__LINE__);
	}
	bsp = add_cpu_device(cpu_bus, cpu.ebx >> 24, 1);
	if (!own_span((uintptr_t)bsp, sizeof(*bsp)) || bsp->upstream != cpu_bus ||
	    bsp->path.type != DEVICE_PATH_APIC || bsp->path.apic.apic_id != 0 ||
	    set_cpu_info(0, bsp) != CB_SUCCESS)
		bsp_refusal(__LINE__);

	admit_stock_gdt(&after);
	if (memcmp(&before, &after, sizeof(before)) || current_gs() != BSP_GS_SELECTOR)
		bsp_refusal(__LINE__);
	/* set_cpu_info may change only the four base-address bytes of GS slot0. */
	for (unsigned int i = 0; i < STOCK_GDT_BYTES; i++) {
		if (i == 0x42 || i == 0x43 || i == 0x44 || i == 0x47)
			continue;
		if ((uint8_t)gdt[i] != table_before[i])
			bsp_refusal(__LINE__);
	}
	gs_descriptor = (const uint8_t *)per_cpu_segment_descriptors;
	gs_base = gs_descriptor[2] | (uintptr_t)gs_descriptor[3] << 8 |
		(uintptr_t)gs_descriptor[4] << 16 | (uintptr_t)gs_descriptor[7] << 24;
	if (!own_span(gs_base, sizeof(struct per_cpu_segment_data)))
		bsp_refusal(__LINE__);
	info = cpu_info();
	if (info != &cpu_infos[0] || !own_span((uintptr_t)info, sizeof(*info)) ||
	    info->index != 0 || info->cpu != bsp)
		bsp_refusal(__LINE__);
	native84_stage_check();
	printk(BIOS_INFO, "NATIVE89 BSP REGISTERED GS40 RETURN; AP INIT NOT RUN\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	/* The genuine CPU-cluster callback returns. No AP startup or CPU driver
	 * was run; generic device init continues into tables and payload.
	 */
}
