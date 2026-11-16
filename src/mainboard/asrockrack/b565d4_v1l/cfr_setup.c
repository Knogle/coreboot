/* SPDX-License-Identifier: GPL-2.0-only */

#include <boot/coreboot_tables.h>
#include <amdblocks/cpu.h>
#include <arch/cpu.h>
#include <cpu/cpu.h>
#include <cpu/x86/name.h>
#include <console/console.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/resource.h>
#include <drivers/option/cfr_frontend.h>
#include <stdio.h>
#include "setup_options.h"

static const struct sm_enum_value speed_values[] = {
	{ "Auto", 0 }, { "Gen1", 1 }, { "Gen2", 2 }, { "Gen3", 3 }, SM_ENUM_VALUE_END,
};

static const struct sm_enum_value aspm_values[] = {
	{ "Automatic / OS control", 0 }, { "Disabled in firmware", 1 },
	{ "L1 when qualified", 2 }, SM_ENUM_VALUE_END,
};

#define SPEED_OPTION(symbol, name, label) \
	static const struct sm_object symbol = SM_DECLARE_ENUM({ \
		.opt_name = name, .ui_name = label, .default_value = 0, \
		.ui_helptext = "Limit the link speed before device initialization. " \
			"A slower endpoint may negotiate below this limit. Apply with a cold boot.", \
		.values = speed_values, \
	})

#define ASPM_OPTION(symbol, name, label) \
	static const struct sm_object symbol = SM_DECLARE_ENUM({ \
		.opt_name = name, .ui_name = label, .default_value = 0, \
		.ui_helptext = "Initial firmware policy for this slot. Only capability- and " \
			"latency-qualified states are used. The operating system retains control.", \
		.values = aspm_values, \
	})

SPEED_OPTION(cpu_speed, "pcie_cpu_speed", "CPU x16 slot maximum speed");
SPEED_OPTION(pcie3_speed, "pcie3_speed", "PCIE3 maximum speed");
SPEED_OPTION(pcie4_speed, "pcie4_speed", "PCIE4 maximum speed");
SPEED_OPTION(pcie5_speed, "pcie5_speed", "PCIE5 maximum speed");
ASPM_OPTION(cpu_aspm, "pcie_cpu_aspm", "CPU x16 slot ASPM");
ASPM_OPTION(pcie3_aspm, "pcie3_aspm", "PCIE3 ASPM");
ASPM_OPTION(pcie4_aspm, "pcie4_aspm", "PCIE4 ASPM");
ASPM_OPTION(pcie5_aspm, "pcie5_aspm", "PCIE5 ASPM");

static const struct sm_object svm = SM_DECLARE_BOOL({
	.opt_name = "svm", .ui_name = "AMD SVM virtualization", .default_value = true,
	.ui_helptext = "Allow the operating system to use hardware virtualization on all CPUs. "
			"Changing this setting requires a cold boot.",
});

static const struct sm_object iommu = SM_DECLARE_BOOL({
	.opt_name = "iommu", .ui_name = "AMD IOMMU", .default_value = true,
	.ui_helptext = "Enable DMA and interrupt remapping support for device assignment. "
			"Disabling omits IVRS and IOMMU MMIO decode. Requires a cold boot.",
});

static const struct sm_object acs = SM_DECLARE_BOOL({
	.opt_name = "pcie_acs", .ui_name = "Native PCIe access control services", .default_value = true,
	.ui_helptext = "Request native ACS isolation from the host bridges. "
			"The operating system determines IOMMU groups from actual capabilities. "
			"Changing this setting requires a cold boot.",
});

static const struct sm_object ari = SM_DECLARE_BOOL({
	.opt_name = "pcie_ari", .ui_name = "PCIe ARI forwarding", .default_value = true,
	.ui_helptext = "Prepare ARI forwarding only when the port and its function-zero "
		"endpoint support ARI. Conventional device numbers are preserved. "
		"The operating system still owns SR-IOV VF creation and may change ARI. "
		"Apply with a cold boot; a capable card is required to use this feature.",
});

static const struct sm_object power_failure = SM_DECLARE_ENUM({
	.opt_name = "power_failure", .ui_name = "Restore after AC power loss", .default_value = 1,
	.ui_helptext = "Apply the selected recovery policy during the next boot. "
			"Previous follows the last hardware power state.",
	.values = (const struct sm_enum_value[]) {
		{ "Stay off", 0 }, { "Power on", 1 }, { "Previous state", 2 }, SM_ENUM_VALUE_END,
	},
});

static const struct sm_object defaults = SM_DECLARE_BOOL({
	.opt_name = "restore_defaults", .ui_name = "Restore board defaults on next boot",
	.default_value = false,
	.ui_helptext = "Restore the board's CMOS settings at the next cold boot. "
			"This request clears itself. The clock and EFI boot entries are preserved.",
});

static const struct sm_object serial_console = SM_DECLARE_BOOL({
	.opt_name = "serial_console", .ui_name = "Payload serial console", .default_value = true,
	.ui_helptext = "Use the serial terminal for payload menus and legacy console output. "
		"Early coreboot recovery diagnostics and the operating system console "
		"are independent. Apply at the next payload start.",
});

static const struct sm_object pxe = SM_DECLARE_BOOL({
	.opt_name = "pxe", .ui_name = "PXE / iPXE option ROMs", .default_value = true,
	.ui_helptext = "Allow network option ROM dispatch and automatic iPXE boot. "
		"Network devices remain available to the operating system. "
		"Apply at the next payload start.",
});

static const struct sm_object sas_rom = SM_DECLARE_BOOL({
	.opt_name = "sas_option_rom", .ui_name = "LSI storage option ROMs", .default_value = false,
	.ui_helptext = "Allow option ROM dispatch for LSI storage controllers. "
		"The controller and operating system driver remain available when disabled. "
		"Apply at the next payload start.",
});

static const struct sm_object boot_policy = SM_DECLARE_ENUM({
	.opt_name = "boot_policy", .ui_name = "Payload boot preference", .default_value = 0,
	.ui_helptext = "EDK2 changes legacy priority only when this setting changes; "
		"other EFI boot entries retain their relative order. UEFI only disables "
		"automatic CSM selection; explicit diagnostic applications remain available. "
		"SeaBIOS provides legacy boot only.",
	.values = (const struct sm_enum_value[]) {
		{ "UEFI preferred", 0 }, { "UEFI automatic boot only", 1 },
		{ "Legacy preferred", 2 }, SM_ENUM_VALUE_END,
	},
});

static const struct sm_object boot_timeout = SM_DECLARE_NUMBER({
	.opt_name = "boot_timeout", .ui_name = "Boot menu timeout (seconds)", .default_value = 5,
	.min = 0, .max = 60, .step = 1,
	.ui_helptext = "Wait up to this many seconds before automatic boot. In EDK2, "
		"changes synchronize with the standard EFI Timeout variable; an existing "
		"OS preference is adopted when this setting is first enabled.",
});

static const struct sm_object wol = SM_DECLARE_BOOL({
	.opt_name = "wake_on_lan", .ui_name = "Allow PCIe / Wake-on-LAN wake",
	.default_value = true,
	.ui_helptext = "Permit PCIe power-management wake requests. The operating system "
		"must enable Wake-on-LAN on a capable network device before shutdown. "
		"Standby power is required. Apply with a cold boot.",
});

static const struct sm_object above_4g = SM_DECLARE_BOOL({
	.opt_name = "above_4g", .ui_name = "Above 4G PCI memory allocation",
	.default_value = false,
	.ui_helptext = "Provide a high PCI memory aperture for movable prefetchable "
		"64-bit BARs, including capable SR-IOV devices. Other BARs remain below "
		"4 GiB. Apply with a cold boot.",
});

static const struct sm_object rtc_wake = SM_DECLARE_BOOL({
	.opt_name = "rtc_wake", .ui_name = "Allow RTC alarm wake", .default_value = true,
	.ui_helptext = "Permit the RTC alarm to wake the system. The operating system "
		"must arm an alarm before shutdown. Standby power is required. "
		"Apply with a cold boot.",
});

#if CONFIG(B565D4_VERMEER_CORE_C6)
static const struct sm_object core_c6 = SM_DECLARE_BOOL({
	.opt_name = "cpu_core_c6", .ui_name = "CPU core C6 idle", .default_value = true,
	.ui_helptext = "Allow individual CPU cores to enter C6 while idle. "
		"Disabled retains C1. Apply with a cold boot.",
});
#endif

static const struct sm_object watchdog = SM_DECLARE_ENUM({
	.opt_name = "watchdog", .ui_name = "FCH hardware watchdog", .default_value = 0,
	.ui_helptext = "Disabled stops the firmware timer. OS owned prepares the stopped "
		"timer for the standard operating-system driver. The 60-second guard "
		"protects coreboot device initialization and payload loading, then stops "
		"before entering the payload. It does not time out setup menus or the OS. "
		"The operating system must configure its own watchdog policy.",
	.values = (const struct sm_enum_value[]) {
		{ "Disabled in firmware", 0 }, { "OS owned", 1 },
		{ "coreboot guard (60 seconds)", 2 }, SM_ENUM_VALUE_END,
	},
});

static const struct sm_object performance_limit = SM_DECLARE_ENUM({
	.opt_name = "cpu_performance_limit", .ui_name = "ACPI CPU performance limit",
	.default_value = 0,
	.ui_helptext = "Expose the CPU's existing hardware P-states through standard ACPI. "
		"Full OS control allows all enabled states. P1/P2 limit the highest "
		"performance state offered to the OS; a CPU with fewer states uses its "
		"lowest enabled state. This does not reprogram voltage, power limits, "
		"boost, CPPC or DRAM training. Apply with a cold boot.",
	.values = (const struct sm_enum_value[]) {
		{ "Full OS control", 0 }, { "P1 and lower", 1 },
		{ "P2 and lower", 2 }, SM_ENUM_VALUE_END,
	},
});

static char status_text[2048];

static void count_ram(void *arg, struct device *dev, struct resource *res)
{
	uint64_t *bytes = arg;

	*bytes += res->size;
}

static void status_ctor(struct sm_object *object)
{
	char brand[49] = { 0 };
	uint64_t ram = 0;
	size_t used;

	if (CONFIG(EDK2_DEBUG))
		printk(BIOS_NOTICE, "Board setup: building platform status\n");
	fill_processor_name(brand);
	if (!brand[0])
		snprintf(brand, sizeof(brand), "CPUID %08x (brand not supplied)", cpuid_eax(1));
	search_global_resources(IORESOURCE_MEM | IORESOURCE_CACHEABLE | IORESOURCE_RESERVE,
				IORESOURCE_MEM | IORESOURCE_CACHEABLE, count_ram, &ram);
	used = snprintf(status_text, sizeof(status_text), "%s; %d logical CPUs. "
			"Selected boot policies: SVM %s, IOMMU %s, native ACS %s. "
		"RAM resources: %llu MiB; DIMM/ECC status unavailable. "
		"High PCI MMIO %s. Current PCIe links:\n",
			brand, get_cpu_count(), b565d4_setup_get(B565D4_OPT_SVM) ? "on" : "off",
			b565d4_setup_get(B565D4_OPT_IOMMU) ? "on" : "off",
			b565d4_setup_get(B565D4_OPT_ACS) ? "on" : "off",
			(unsigned long long)(ram >> 20),
			(CONFIG(B565D4_BOARD_SETUP) && b565d4_setup_high_mmio_ready()) ?
			"available" : "not configured by this setup");
	for (struct device *dev = all_devices; dev && used < sizeof(status_text) - 1;
	     dev = dev->next) {
		if (dev->path.type != DEVICE_PATH_PCI || !dev->enabled)
			continue;
		unsigned int cap = pci_find_capability(dev, PCI_CAP_ID_PCIE);

		if (!cap)
			continue;
		uint16_t link = pci_read_config16(dev, cap + PCI_EXP_LNKSTA);
		uint32_t capabilities = pci_read_config32(dev, cap + PCI_EXP_LNKCAP);

		if (!(link & PCI_EXP_LNKSTA_NLW))
			continue;
		if ((capabilities & PCI_EXP_LNKCAP_DLLLARC) && !(link & PCI_EXP_LNKSTA_DLLLA))
			continue;
		int count = snprintf(status_text + used, sizeof(status_text) - used,
				     "%s: Gen%u x%u\n", dev_path(dev), link & 0xf,
				     (link & PCI_EXP_LNKSTA_NLW) >> 4);
		if (count < 0 || (size_t)count >= sizeof(status_text) - used)
			break;
		used += count;
	}
	/* A comment is not selectable in every frontend. Keep its actual values
	 * in the primary text instead of an unreachable help-only field.
	 */
	object->sm_comment.ui_name = status_text;
	object->sm_comment.ui_helptext = "Resources and policies observed during this boot.";
	if (CONFIG(EDK2_DEBUG))
		printk(BIOS_NOTICE, "Board setup: platform status complete\n");
}

static const struct sm_object status = {
	.kind = SM_OBJ_COMMENT, .ctor = status_ctor,
	.sm_comment = { .ui_name = "Platform status at this boot" },
};

static struct sm_obj_form virtualization = {
	.ui_name = "Virtualization",
	.obj_list = (const struct sm_object *[]) { &svm, &iommu, &acs, &ari, NULL },
};

static struct sm_obj_form links = {
	.ui_name = "PCIe link policy",
	.obj_list = (const struct sm_object *[]) {
		&cpu_speed, &pcie3_speed, &pcie4_speed, &pcie5_speed,
		&cpu_aspm, &pcie3_aspm, &pcie4_aspm, &pcie5_aspm, &above_4g, NULL,
	},
};

static struct sm_obj_form system = {
	.ui_name = "System and recovery",
	.obj_list = (const struct sm_object *[]) {
		&power_failure, &wol, &rtc_wake, &watchdog, &performance_limit, &serial_console,
		&defaults, &status, NULL,
	},
};

static struct sm_obj_form boot = {
	.ui_name = "Payload and boot policy",
	.obj_list = (const struct sm_object *[]) {
		&boot_policy, &boot_timeout, &pxe, &sas_rom, NULL,
	},
};

/* Desktop controls are listed only when their native consumers exist.
 * In particular, do not advertise the Cezanne DF, power or slot adapters.
 */
static struct sm_obj_form desktop_virtualization = {
	.ui_name = "Virtualization",
	.obj_list = (const struct sm_object *[]) { &svm, &iommu, &acs, &ari, NULL },
};

static struct sm_obj_form desktop_links = {
	.ui_name = "PCIe link policy",
	.obj_list = (const struct sm_object *[]) {
		&cpu_speed, &pcie3_speed, &pcie4_speed, &pcie5_speed,
		&cpu_aspm, &pcie3_aspm, &pcie4_aspm, &pcie5_aspm, NULL,
	},
};

static struct sm_obj_form desktop_system = {
	.ui_name = "System and recovery",
	.obj_list = (const struct sm_object *[]) {
		&power_failure, &wol, &rtc_wake, &performance_limit,
#if CONFIG(B565D4_VERMEER_CORE_C6)
		&core_c6,
#endif
		&serial_console, &defaults, NULL,
	},
};

void b565d4_setup_cfr(struct lb_cfr *root, struct sm_obj_form *pcie);
void b565d4_setup_cfr(struct lb_cfr *root, struct sm_obj_form *pcie)
{
	struct sm_obj_form *forms[] = { pcie, &links, &virtualization, &boot, &system, NULL };

	struct sm_obj_form *desktop_forms[] = {
		pcie, &desktop_links, &desktop_virtualization, &boot, &desktop_system, NULL,
	};

	cfr_write_setup_menu(root, CONFIG(B565D4_DESKTOP_SETUP) ? desktop_forms : forms);
}
