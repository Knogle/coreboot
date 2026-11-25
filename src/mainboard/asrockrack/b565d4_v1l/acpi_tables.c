/* SPDX-License-Identifier: GPL-2.0-only */
/* Retained PM/SCI RTC fixed-event and actual-enabled-CPU/C1 tables. */
#include "native84_profile.h"
#include <acpi/acpi.h>
#include <acpi/acpi_ivrs.h>
#include <acpi/acpigen.h>
#include <amdblocks/acpimmio.h>
#include <amdblocks/amd_pci_util.h>
#include <amdblocks/cpu.h>
#include <amdblocks/reset.h>
#include <amdblocks/vermeer_pcc.h>
#include <arch/cpu.h>
#include <arch/io.h>
#include <console/console.h>
#include <cpu/x86/lapic.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ops.h>
#include <soc/amd_pci_int_defs.h>
#include <soc/southbridge.h>
#include <string.h>
#include <types.h>
#include "setup_options.h"
#include "vermeer/cpu_power.h"
#include "vermeer/cstates.h"

#if !CONFIG(SOC_AMD_CEZANNE_NATIVE) || !CONFIG(NO_SMM)
#error "This ACPI draft is only for the retained NO_SMM profile"
#endif

unsigned long mainboard_fill_ivrs(unsigned long current, const struct device *domain)
{
	unsigned int root_device;
	uint32_t root_id;
	const uint32_t signature = cpuid(1).eax & 0x0fff0ff0;

	/* These root ports emit MSI requests with requester ID 00:00.0.
	 * Describe the common requester for each port so interrupt remapping uses
	 * distinct entries in one table, instead of delivering a different port's
	 * bandwidth event to the chipset uplink's interrupt.
	 */
	if (!domain || domain->path.type != DEVICE_PATH_DOMAIN ||
	    domain->path.domain.domain_id || !domain->downstream ||
	    domain->downstream->secondary)
		return current;
	if (signature == 0x00a50f00) {
		root_device = 2;
		root_id = 0x16341022;
	} else if (signature == 0x00870f10 || signature == 0x00a20f10) {
		root_device = 1;
		root_id = 0x14831022;
	} else {
		return current;
	}

	for (unsigned int function = 1; function <= 7; function++) {
		const struct device *port = pcidev_on_root(root_device, function);
		ivrs_ivhd_alias_t *alias;

		if (!port || !port->enabled ||
		    pci_read_config32(port, PCI_VENDOR_ID) != root_id)
			continue;
		if (!check_acpi_tables_write_limit(current, sizeof(*alias)))
			return current;

		alias = (void *)current;
		memset(alias, 0, sizeof(*alias));
		alias->type = IVHD_DEV_8_BYTE_ALIAS_SELECT;
		alias->dev_id = PCI_DEVFN(root_device, function);
		alias->source_dev_id = PCI_DEVFN(0, 0);
		printk(BIOS_INFO, "Board IVRS: root MSI requester alias %04x -> %04x\n",
		       alias->dev_id, alias->source_dev_id);
		current += sizeof(*alias);
	}

	return current;
}

static uint16_t retained_pm16(uint8_t index)
{
	const uint8_t low = pm_io_read8(index);
	const uint8_t high = pm_io_read8(index + 1);

	return low | ((uint16_t)high << 8);
}

struct retained_pm {
	uint16_t event, control, timer, gpe;
};

static void prepare_retained_pm(struct retained_pm *pm)
{
	const uint32_t decode = retained_pm16(0x04) |
		((uint32_t)retained_pm16(0x06) << 16);
	const uint16_t conf_low = retained_pm16(PM_ACPI_CONF);
	const uint16_t conf_high = retained_pm16(PM_ACPI_CONF + 2);
	const uint32_t conf = conf_low | ((uint32_t)conf_high << 16);
	const uint32_t required = PM_ACPI_DECODE_STD | PM_ACPI_GLOBAL_EN |
		PM_ACPI_RTC_EN_EN | PM_ACPI_TIMER_EN_EN;
	uint8_t pic, apic, pic_after, apic_after;
	uint16_t before, after;

	pm->event = retained_pm16(PM_EVT_BLK);
	pm->control = retained_pm16(PM1_CNT_BLK);
	pm->timer = retained_pm16(PM_TMR_BLK);
	pm->gpe = retained_pm16(PM_GPE0_BLK);
	printk(BIOS_INFO, "NATIVE91 ACPI PM decode=%08x policy=%08x "
	       "evt=%04x cnt=%04x tmr=%04x gpe=%04x\n", decode, conf,
	       pm->event, pm->control, pm->timer, pm->gpe);
	if (!(decode & (1U << 1)) || (conf & required) != required ||
	    (conf & PM_ACPI_REDUCED_HW_EN) ||
	    !((pm->event == 0x400 && pm->control == 0x404 &&
	       pm->timer == 0x408 && pm->gpe == 0x420) ||
	      (pm->event == 0x800 && pm->control == 0x804 &&
	       pm->timer == 0x808 && pm->gpe == 0x820)))
		die("NATIVE91 ACPI: unsupported retained PM decode/map\n");

	/* Genuine raw PIC/IOAPIC route bytes, selectors10h/90h atC00/C01.
	 * Root selected this finite SCI-only normalization, not the whole IRQ
	 * table hook. Preserve9/9 with no data write; otherwise write/readback9.
	 */
	pic = read_pci_int_idx(PIRQ_SCI, 0);
	apic = read_pci_int_idx(PIRQ_SCI, 1);
	if (pic != 9 || apic != 9) {
		write_pci_int_idx(PIRQ_SCI, 0, 9);
		write_pci_int_idx(PIRQ_SCI, 1, 9);
	}
	pic_after = read_pci_int_idx(PIRQ_SCI, 0);
	apic_after = read_pci_int_idx(PIRQ_SCI, 1);
	printk(BIOS_INFO, "NATIVE91 SCI routes PIC/APIC %02x/%02x -> %02x/%02x\n",
	       pic, apic, pic_after, apic_after);
	if (pic_after != 9 || apic_after != 9)
		die("NATIVE91 ACPI: SCI route9 readback failed\n");

	/* Only PM1_CNT bit0, at the discovered coherent base. No OEM SMI,
	 * sleep request, PM status W1C clear, or cold400/800 base rewrite.
	 */
	before = inw(pm->control);
	if (!(before & 1))
		outw(before | 1U, pm->control);
	after = inw(pm->control);
	printk(BIOS_INFO, "NATIVE91 SCI_EN port=%04x control=%04x -> %04x\n",
	       pm->control, before, after);
	if (after != (uint16_t)(before | 1U))
		die("NATIVE91 ACPI: SCI_EN control readback failed\n");
}

static bool prepare_cold_reset(void)
{
	const uint32_t decode_before = pm_io_read32(PM_DECODE_EN);
	const uint16_t power_before = retained_pm16(PWR_RESET_CFG);
	uint32_t decode_after;
	uint16_t power_after;

	if (decode_before == UINT32_MAX || power_before == UINT16_MAX) {
		printk(BIOS_ERR, "B565D4 CF9: inaccessible retained PM registers\n");
		return false;
	}

	/* Use the Cezanne policy for a complete power-good reset. This only
	 * prepares a future reset; neither helper writes the CF9 trigger.
	 */
	fch_enable_cf9_io();
	set_resets_to_cold();
	decode_after = pm_io_read32(PM_DECODE_EN);
	power_after = retained_pm16(PWR_RESET_CFG);
	printk(BIOS_INFO, "B565D4 CF9: decode %08x -> %08x power %04x -> %04x\n",
	       decode_before, decode_after, power_before, power_after);
	if (decode_after != (decode_before | CF9_IO_EN) ||
	    power_after != (uint16_t)(power_before | TOGGLE_ALL_PWR_GOOD)) {
		printk(BIOS_ERR, "B565D4 CF9: reset policy readback failed\n");
		return false;
	}

	return true;
}

void mainboard_fill_fadt(acpi_fadt_t *fadt)
{
	struct retained_pm pm;

	prepare_retained_pm(&pm);

	/* Called after the arch and SoC fillers: replace both legacy and GAS
	 * addresses without cold port programming. The selected SCI route and
	 * control changes are explicit and logged above.
	 */
	fadt->preferred_pm_profile = 1;
	fadt->sci_int = 9;
	fadt->pm1a_evt_blk = pm.event;
	fadt->pm1b_evt_blk = 0;
	fadt->pm1a_cnt_blk = pm.control;
	fadt->pm1b_cnt_blk = 0;
	fadt->pm2_cnt_blk = 0;
	fadt->pm_tmr_blk = pm.timer;
	fadt->gpe0_blk = pm.gpe;
	fadt->gpe1_blk = 0;
	fadt->pm1_evt_len = 4;
	fadt->pm1_cnt_len = 2;
	fadt->pm2_cnt_len = 0;
	fadt->pm_tmr_len = 4;
	fadt->gpe0_blk_len = 8;
	fadt->gpe1_blk_len = 0;
	fadt->gpe1_base = 0;
	memset(&fadt->x_pm1a_evt_blk, 0, 8 * sizeof(acpi_addr_t));
	fill_fadt_extended_pm_io(fadt);

	fadt->smi_cmd = 0;
	fadt->acpi_enable = 0;
	fadt->acpi_disable = 0;
	fadt->s4bios_req = 0;
	fadt->pstate_cnt = 0;
	fadt->cst_cnt = 0;
	fadt->p_lvl2_lat = ACPI_FADT_C2_NOT_SUPPORTED;
	fadt->p_lvl3_lat = ACPI_FADT_C3_NOT_SUPPORTED;
	fadt->day_alrm = 0x0d;
	fadt->mon_alrm = 0;
	fadt->century = 0x32;
	fadt->iapc_boot_arch = ACPI_FADT_LEGACY_DEVICES;
	/* The chassis power button uses the fixed PM1 event interface.
	 * No separate sleep button or control-method button is described.
	 * The RTC fixed event remains enabled. No S3/S4/S0ix is offered.
	 * Reset support is supplied only by the selected CF9 preparation below.
	 */
	fadt->flags = ACPI_FADT_WBINVD | ACPI_FADT_C1_SUPPORTED |
		ACPI_FADT_SLEEP_BUTTON |
		ACPI_FADT_32BIT_TIMER;
	if (CONFIG(B565D4_BOARD_SETUP) && b565d4_setup_get(B565D4_OPT_WOL))
		fadt->flags |= ACPI_FADT_PCI_EXPRESS_WAKE;
	memset(&fadt->reset_reg, 0, sizeof(fadt->reset_reg));
	fadt->reset_value = 0;
	if (CONFIG(B565D4_FADT_COLD_RESET) && prepare_cold_reset()) {
		fadt->flags |= ACPI_FADT_RESET_REGISTER;
		fadt->reset_reg.space_id = ACPI_ADDRESS_SPACE_IO;
		fadt->reset_reg.bit_width = 8;
		fadt->reset_reg.access_size = ACPI_ACCESS_SIZE_BYTE_ACCESS;
		fadt->reset_reg.addrl = RST_CNT;
		fadt->reset_value = RST_CPU | SYS_RST;
	}
	memset(&fadt->sleep_control_reg, 0, sizeof(fadt->sleep_control_reg));
	memset(&fadt->sleep_status_reg, 0, sizeof(fadt->sleep_status_reg));
}

/* The common AMD P-state writer uses its CPU index only to form _PSD domains.
 * MADT UIDs follow device-list order, which can list all primary threads before
 * their SMT siblings. Give the writer an APIC-sorted index without changing
 * those UIDs, and verify that each resulting domain is exactly one real core.
 */
static bool pstate_topology_indices(const struct device *const *cpus,
				    unsigned int count, unsigned int *indices)
{
	const unsigned int threads = get_threads_per_core();

	if (!threads || !count || count % threads)
		return false;
	for (unsigned int i = 0; i < count; i++) {
		unsigned int rank = 0;

		for (unsigned int j = 0; j < count; j++) {
			if (i != j && cpus[i]->path.apic.apic_id == cpus[j]->path.apic.apic_id)
				return false;
			rank += cpus[j]->path.apic.apic_id < cpus[i]->path.apic.apic_id;
		}
		if (cpus[i]->path.apic.thread_id != rank % threads)
			return false;
		indices[i] = rank;
	}
	for (unsigned int i = 0; i < count; i++) {
		for (unsigned int j = 0; j < count; j++) {
			const bool same_core =
				cpus[i]->path.apic.package_id == cpus[j]->path.apic.package_id &&
				cpus[i]->path.apic.core_id_within_package ==
				cpus[j]->path.apic.core_id_within_package;

			if (same_core != (indices[i] / threads == indices[j] / threads))
				return false;
		}
	}
	return true;
}

/* PCCT is generated before the CPU SSDT. Keep one common eligibility decision
 * for all CPU packages and the shared-memory descriptor. A failed probe,
 * unsupported topology or selected legacy P-state limit publishes neither.
 */
static struct {
	bool checked;
	bool eligible;
	bool published;
	unsigned int count;
	uint32_t apic_ids[CONFIG_MAX_CPUS];
	struct cppc_config config[CONFIG_MAX_CPUS];
	struct vermeer_pcc_subspace subspace;
} cppc_acpi;

static bool prepare_cppc_acpi(void)
{
	const struct device *cpus[CONFIG_MAX_CPUS];
	unsigned int ranks[CONFIG_MAX_CPUS];
	struct vermeer_pcc_state state;
	unsigned int count = 0, nodes = 0;
	const struct device *cpu;

	if (cppc_acpi.checked)
		return cppc_acpi.eligible;
	cppc_acpi.checked = true;
	if (!CONFIG(B565D4_VERMEER) || !b565d4_vermeer_cpu_power_ready() ||
	    (CONFIG(B565D4_DESKTOP_SETUP) && b565d4_setup_get(B565D4_OPT_PSTATE_LIMIT)) ||
	    !vermeer_pcc_get_state(&state))
		return false;
	if (!state.cpu_count || state.cpu_count > CONFIG_MAX_CPUS ||
	    state.cpu_count != get_cpu_count())
		return false;
	for (cpu = all_devices; cpu; cpu = cpu->next) {
		if (++nodes > 256)
			return false;
		if (!is_enabled_cpu(cpu))
			continue;
		if (!cpu->initialized || count >= state.cpu_count)
			return false;
		cpus[count++] = cpu;
	}
	if (count != state.cpu_count || !pstate_topology_indices(cpus, count, ranks) ||
	    !vermeer_pcc_subspace(state.shared_base, &cppc_acpi.subspace))
		return false;
	for (unsigned int i = 0; i < count; i++) {
		struct cppc_config *config = &cppc_acpi.config[i];

		if (!vermeer_cppc_config(state.information, ranks[i], state.lowest_mhz,
				       state.nominal_mhz, config) ||
		    config->version != CPPC_VERSION_3)
			return false;
		cppc_acpi.apic_ids[i] = cpus[i]->path.apic.apic_id;
	}
	cppc_acpi.count = count;
	cppc_acpi.eligible = true;
	return true;
}

unsigned long acpi_fill_pcct(unsigned long current)
{
	if (!CONFIG(B565D4_VERMEER) || cppc_acpi.published || !prepare_cppc_acpi() ||
	    !check_acpi_tables_write_limit(current, sizeof(cppc_acpi.subspace)))
		return current;
	memcpy((void *)current, &cppc_acpi.subspace, sizeof(cppc_acpi.subspace));
	cppc_acpi.published = true;
	printk(BIOS_NOTICE, "ACPI CPPC: PCCT base=%08x bytes=%u cpus=%u\n",
	       (unsigned int)cppc_acpi.subspace.base, VERMEER_PCC_BYTES, cppc_acpi.count);
	return current + sizeof(cppc_acpi.subspace);
}

static void write_cppc_cpu_package(const struct device *cpu)
{
	if (!CONFIG(B565D4_VERMEER) || !cppc_acpi.published)
		return;
	for (unsigned int i = 0; i < cppc_acpi.count; i++) {
		if (cppc_acpi.apic_ids[i] != cpu->path.apic.apic_id)
			continue;
		acpigen_write_CPPC_package(&cppc_acpi.config[i]);
		/* The generic method names absolute CPU0.GCPC. Each PCC CPU has its
		 * own offsets, so return this device's local package instead.
		 */
		acpigen_write_method("_CPC", 0);
		acpigen_emit_byte(RETURN_OP);
		acpigen_emit_namestring(CPPC_PACKAGE_NAME);
		acpigen_pop_len();
		return;
	}
}

/* Match acpi_create_madt_lapics(): first hardware thread of each core,
 * then successive threads, with APIC IDs ascending within each group.
 * MP completion can leave all_devices in a different order on every boot.
 * Sort only this local array; the device tree and PCC's APIC-rank map stay
 * unchanged. The SSDT UID must describe the same CPU as the generic MADT.
 */
static void sort_cpu_devices_for_acpi(const struct device **cpus, unsigned int count)
{
	for (unsigned int i = 1; i < count; i++) {
		const struct device *cpu = cpus[i];
		unsigned int j = i;

		while (j) {
			const struct device *previous = cpus[j - 1];

			if (previous->path.apic.thread_id < cpu->path.apic.thread_id ||
			    (previous->path.apic.thread_id == cpu->path.apic.thread_id &&
			     previous->path.apic.apic_id <= cpu->path.apic.apic_id))
				break;
			cpus[j] = previous;
			j--;
		}
		cpus[j] = cpu;
	}
}

/* The genuine CPU-bus operations reference generate_cpu_entries externally.
 * The future retained Makefile wraps that reference, leaving cold source intact.
 */
void __wrap_generate_cpu_entries(const struct device *device);
void __wrap_generate_cpu_entries(const struct device *device)
{
	static bool emitted;
	const struct device *cpu;
	const bool smp = native84_profile_policy.entry_path == 2;
	const unsigned int expected = smp ? get_cpu_count() : 1;
	const struct device *cpus[CONFIG_MAX_CPUS];
	unsigned int pstate_indices[CONFIG_MAX_CPUS];
	unsigned int nodes = 0, enabled = 0, bsp_count = 0;
	const acpi_cstate_t c1 = {
		.ctype = 1,
		.latency = 1,
		.power = 0,
		.resource = {
			.space_id = ACPI_ADDRESS_SPACE_FIXED,
			.bit_offset = ACPI_FFIXEDHW_CLASS_HLT,
		},
	};

	(void)device;
	for (cpu = all_devices; cpu; cpu = cpu->next) {
		if (++nodes > 256)
			die("Retained ACPI: device-list bound\n");
		if (is_enabled_cpu(cpu)) {
			if (enabled < CONFIG_MAX_CPUS)
				cpus[enabled] = cpu;
			if (cpu->path.apic.apic_id == lapicid())
				bsp_count++;
			enabled++;
		}
	}
	if (emitted || enabled != expected || bsp_count != 1 ||
	    (smp && (enabled < 1 || enabled > CONFIG_MAX_CPUS)))
		die("Retained ACPI: enabled CPU count/BSP mismatch\n");
	emitted = true;
	if (CONFIG(B565D4_BOARD_SETUP) || CONFIG(B565D4_DESKTOP_SETUP))
		acpigen_write_name_integer("\\RWA5",
			b565d4_setup_get(B565D4_OPT_RTC_WAKE));
	sort_cpu_devices_for_acpi(cpus, enabled);
	const bool desktop_topology_valid = CONFIG(B565D4_VERMEER) &&
		pstate_topology_indices(cpus, enabled, pstate_indices);
	const bool desktop_pstates = desktop_topology_valid &&
		b565d4_vermeer_cpu_power_ready();

	const bool desktop_cc6 = CONFIG(B565D4_VERMEER_CORE_C6) &&
		desktop_topology_valid && b565d4_vermeer_cc6_ready();
	const acpi_cstate_t cstates[] = {
		c1,
		{
			.ctype = 2,
			.latency = B565D4_VERMEER_C2_LATENCY,
			.power = 0,
			.resource = {
				.space_id = ACPI_ADDRESS_SPACE_IO,
				.bit_width = 8,
				.addrl = B565D4_VERMEER_C2_IO,
				.access_size = ACPI_ACCESS_SIZE_BYTE_ACCESS,
			},
		},
	};

	if (CONFIG(B565D4_VERMEER) && !desktop_topology_valid)
		printk(BIOS_WARNING, "ACPI: CPU topology does not qualify P-state domains\n");
	/* Generic MADT assigns UID0..N-1 in the same sorted CPU order.
	 * Retain C1/HLT and add the prepared desktop C2 entry when enabled.
	 * Reuse the common AMD provider for the CPU's existing
	 * hardware P-states; CPPC and deeper idle remain separately qualified.
	 */
	for (unsigned int index = 0; index < enabled; index++) {
		acpigen_write_processor_device(index);
		acpigen_write_STA(0x0f);
		if (CONFIG(B565D4_BOARD_SETUP) && b565d4_setup_cpu_power_ready())
			amd_generate_cpu_pstate_entries(index, enabled,
				b565d4_setup_get(B565D4_OPT_PSTATE_LIMIT));
		else if (desktop_pstates)
			amd_generate_cpu_pstate_entries(pstate_indices[index], enabled,
				CONFIG(B565D4_DESKTOP_SETUP) ?
				b565d4_setup_get(B565D4_OPT_PSTATE_LIMIT) : 0);
		write_cppc_cpu_package(cpus[index]);
		if (desktop_cc6) {
			acpigen_write_CST_package(cstates, ARRAY_SIZE(cstates));
			acpigen_write_CSD_package(pstate_indices[index] / get_threads_per_core(),
				get_threads_per_core(), CSD_HW_ALL, 0);
		} else {
			acpigen_write_CST_package(&c1, 1);
		}
		acpigen_write_processor_device_end();
	}
	acpigen_write_processor_package("PPKG", 0, enabled);
}
