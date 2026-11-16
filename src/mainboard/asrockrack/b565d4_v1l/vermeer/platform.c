/* SPDX-License-Identifier: GPL-2.0-only */

#include "platform.h"
#include "b565d4_dxio.h"
#include "b565d4_init_policy.h"
#include "dxio_service.h"
#include "nbio_base_plan.h"
#include "nbio_memory.h"
#include "usb_init.h"
#include "native84_profile.h"
#include "native87_devices.h"
#include "native_apob_resources.h"
#include "native_promontory.h"
#include "promontory_uplink.h"
#include "legacy_shadow_fallback.h"
#include "board_config.h"
#include "setup_options.h"
#include "b565d4_cpu.h"
#include "b565d4_memory.h"
#include "czn_dxio.h"
#include <acpi/acpi.h>
#include <amdblocks/acpi.h>
#include <amdblocks/acpimmio.h>
#include <amdblocks/amd_pci_util.h>
#include <amdblocks/root_complex.h>
#include <amdblocks/smn.h>
#include <arch/io.h>
#include <bootstate.h>
#include <cbmem.h>
#include <console/console.h>
#include <console/uart.h>
#include <cpu/amd/msr.h>
#include <cpu/amd/mtrr.h>
#include <cpu/x86/msr.h>
#include <delay.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ops.h>
#include <halt.h>
#include <soc/southbridge.h>
#include <static.h>

#define DESKTOP_IOHC 0x13e10000U
#define DXIO_BUFFER_ID 0x56445849U
#define DXIO_BUFFER_BYTES 4096U

static bool session_active;
static bool reset_preflight_done;
static struct stopwatch reset_budget;
static uint8_t *dxio_buffer;
static struct matisse_dxio_configured_step base_plan[MATISSE_NBIO_BASE_MAX_STEPS];
static struct matisse_dxio_configured_state base_state;
static struct b565d4_matisse_dxio_state dxio_state;
static struct matisse_usb_init_state usb_state;

static bool desktop_identity(void)
{
	return b565d4_is_selected_cpu(cpuid_eax(1)) &&
		pci_s_read_config32(PCI_DEV(0, 0, 0), PCI_VENDOR_ID) == 0x14801022 &&
		rdmsr(MMIO_CONF_BASE).raw == 0xf000001dULL;
}

static bool session_access(void *unused)
{
	return session_active && desktop_identity();
}

/* The reset register ABI belongs to this physical board. Reuse its existing
 * GPIO/ASF provider, with desktop ownership and a bounded owner lifetime.
 */
int native106_dxio_budget_ok(void)
{
	return session_access(NULL) && !stopwatch_expired(&reset_budget);
}

int native106_dxio_delay(unsigned int usec)
{
	if (usec > 1000000U)
		return 0;
	for (unsigned int done = 0; done < usec;) {
		unsigned int step = usec - done > 10 ? 10 : usec - done;

		if (!native106_dxio_budget_ok())
			return 0;
		udelay(step);
		done += step;
	}
	return native106_dxio_budget_ok();
}

void native106_dxio_io(const char *label, uint32_t address,
	uint32_t before, uint32_t after)
{
	printk(BIOS_INFO, B565D4_DESKTOP_NAME " RESET IO %s address=%08x "
	       "before=%08x after=%08x\n", label, address, before, after);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

static enum cb_err reset_port(unsigned int group, unsigned int level, void *unused)
{
	if (!native106_dxio_budget_ok())
		return CB_ERR;
	if (!reset_preflight_done) {
		if (!native106_dxio_reset_preflight())
			return CB_ERR;
		reset_preflight_done = true;
	}
	return native106_dxio_reset(group, level) ? CB_SUCCESS : CB_ERR;
}

static enum cb_err base_admit(const struct matisse_dxio_configured_step *plan,
			     size_t count, void *unused)
{
	return plan == base_plan && count == MATISSE_NBIO_BASE_MAX_STEPS &&
		session_access(NULL) ? CB_SUCCESS : CB_ERR;
}

static bool base_access(uint32_t address, bool write, void *unused)
{
	return session_access(NULL);
}

static enum cb_err read_smn(uint32_t address, uint32_t *value, void *unused)
{
	if (!value || !session_access(NULL))
		return CB_ERR;
	*value = smn_read32(address);
	return CB_SUCCESS;
}

static enum cb_err write_smn(uint32_t address, uint32_t value, void *unused)
{
	if (!session_access(NULL))
		return CB_ERR;
	smn_write32(address, value);
	return CB_SUCCESS;
}

static bool pci_address(uint32_t address, unsigned int bytes)
{
	/* Native AGESA-style address: bus23:20, device19:15, function14:12,
	 * register11:0. There is one 128-bus segment and no allocation yet.
	 */
	return session_access(NULL) && !(address & 0xf8000000U) &&
		(bytes == 1 || bytes == 2 || bytes == 4) && !(address & (bytes - 1)) &&
		(address & 0xfff) <= 0x1000 - bytes;
}

static enum cb_err read_pci(uint32_t address, unsigned int bytes, uint32_t *value,
			   void *unused)
{
	if (!value || !pci_address(address, bytes))
		return CB_ERR;
	const uintptr_t physical = CONFIG_ECAM_MMCONF_BASE_ADDRESS + address;

	if (bytes == 1)
		*value = *(volatile uint8_t *)physical;
	else if (bytes == 2)
		*value = *(volatile uint16_t *)physical;
	else
		*value = *(volatile uint32_t *)physical;
	return CB_SUCCESS;
}

static enum cb_err write_pci(uint32_t address, unsigned int bytes, uint32_t value,
			    void *unused)
{
	if (!pci_address(address, bytes))
		return CB_ERR;
	const uintptr_t physical = CONFIG_ECAM_MMCONF_BASE_ADDRESS + address;

	if (bytes == 1)
		*(volatile uint8_t *)physical = value;
	else if (bytes == 2)
		*(volatile uint16_t *)physical = value;
	else
		*(volatile uint32_t *)physical = value;
	return CB_SUCCESS;
}

static enum cb_err admit_dxio(const struct b565d4_matisse_dxio_input *input,
			     const struct b565d4_matisse_dxio_state *state, void *unused)
{
	const struct native84_handoff *arena = native84_descriptor();
	const struct cbmem_entry *entry = cbmem_entry_find(DXIO_BUFFER_ID);

	/* Romstage admitted this complete arena against the current supplier
	 * map and SMM ranges before its first store. The bounded CBMEM owner now
	 * supplies the DMA mapping; no reference-machine address is used.
	 */
	return session_access(NULL) && state == &dxio_state && entry &&
		cbmem_entry_start(entry) == dxio_buffer &&
		cbmem_entry_size(entry) >= DXIO_BUFFER_BYTES &&
		(uintptr_t)dxio_buffer >= arena->cbmem_bottom &&
		(uintptr_t)dxio_buffer < arena->cbmem_top &&
		cbmem_entry_size(entry) <= arena->cbmem_top - (uintptr_t)dxio_buffer &&
		input->platform == dxio_buffer && input->platform_capacity == DXIO_BUFFER_BYTES &&
		input->platform_physical == (uintptr_t)dxio_buffer &&
		b565d4_direct_memory_valid(rdmsr(SYSCFG_MSR).raw) ? CB_SUCCESS : CB_ERR;
}

static void report_event(const struct matisse_dxio_poll_result *event, unsigned int phase,
			 void *unused)
{
	printk(BIOS_DEBUG, B565D4_DESKTOP_NAME " DXIO phase=%u kind=%u "
	       "raw=%08x/%08x/%08x/%08x/%08x/%08x\n", phase, event->kind,
	       event->raw[0], event->raw[1], event->raw[2], event->raw[3],
	       event->raw[4], event->raw[5]);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

static void nbio_memory(void)
{
	struct matisse_nbio_memory before = {
		.root_tom = pci_read_config32(DEV_PTR(gnb), MATISSE_NBIO_ROOT_TOM),
		.tom2_low = smn_read32(MATISSE_NBIO_TOM2_LOW),
		.tom2_high = smn_read32(MATISSE_NBIO_TOM2_HIGH),
		.tom3 = smn_read32(MATISSE_NBIO_TOM3),
		.control = smn_read32(MATISSE_NBIO_TOM_CONTROL),
	}, after;

	if (!matisse_plan_nbio_memory(&before, rdmsr(TOP_MEM_MSR).raw,
				     rdmsr(TOP_MEM2_MSR).raw, rdmsr(SYSCFG_MSR).raw, &after))
		die(B565D4_DESKTOP_NAME " NBIO memory contract invalid");
	pci_write_config32(DEV_PTR(gnb), MATISSE_NBIO_ROOT_TOM, after.root_tom);
	smn_write32(MATISSE_NBIO_TOM2_LOW, after.tom2_low);
	smn_write32(MATISSE_NBIO_TOM2_HIGH, after.tom2_high);
	smn_write32(MATISSE_NBIO_TOM3, after.tom3);
	smn_write32(MATISSE_NBIO_TOM_CONTROL, after.control);
	if (pci_read_config32(DEV_PTR(gnb), MATISSE_NBIO_ROOT_TOM) != after.root_tom ||
	    smn_read32(MATISSE_NBIO_TOM2_LOW) != after.tom2_low ||
	    smn_read32(MATISSE_NBIO_TOM2_HIGH) != after.tom2_high ||
	    smn_read32(MATISSE_NBIO_TOM3) != after.tom3 ||
	    smn_read32(MATISSE_NBIO_TOM_CONTROL) != after.control)
		die(B565D4_DESKTOP_NAME " NBIO memory readback mismatch");
}

static void set_cpu_link_speeds(void)
{
	const unsigned int ceiling = CONFIG_B565D4_DESKTOP_PCIE_MAX_SPEED;
	const unsigned int gfx_speed = CONFIG(B565D4_DESKTOP_SETUP) ?
		b565d4_setup_get(B565D4_OPT_CPU_SPEED) : 0;
	const unsigned int gpp_speed = CONFIG(B565D4_DESKTOP_SETUP) ?
		b565d4_setup_get(B565D4_OPT_PCIE4_SPEED) : 0;
	const struct matisse_dxio_result *result = &dxio_state.events.result;

	if (ceiling > 4 || !result->count || result->count > MATISSE_NBIF_MAP_SLOTS)
		return;
	for (unsigned int i = 0; i < result->count; i++) {
		const struct matisse_dxio_engine_result *port = &result->engines[i];
		struct smu_payload reply = { 0 };

		/* The third OEM policy is the PCH uplink, in both slot modes. */
		if (!port->enabled || port->policy_index >= dxio_state.port_count ||
		    port->policy_index == 2 || port->engine_index > 15)
			continue;
		const unsigned int speed = b565d4_matisse_cpu_slot_speed(port->policy_index,
				dxio_state.port_count, ceiling, gfx_speed, gpp_speed);

		if (!speed)
			continue;
		if (!session_access(NULL))
			return;
		printk(BIOS_NOTICE, B565D4_DESKTOP_NAME " PCIe SMU speed bdf=%08x "
		       "engine=%u policy=%u target=Gen%u\n", port->bdf,
		       port->engine_index, port->policy_index, speed);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		const enum cb_err status = matisse_dxio_set_link_speed(port->engine_index,
								    speed, &reply);

		printk(BIOS_NOTICE, B565D4_DESKTOP_NAME " PCIe SMU speed result=%d "
		       "raw=%08x/%08x/%08x/%08x/%08x/%08x\n", status,
		       reply.msg[0], reply.msg[1], reply.msg[2], reply.msg[3],
		       reply.msg[4], reply.msg[5]);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if (status != CB_SUCCESS || matisse_dxio_status(reply.msg[0]) != 3)
			return;
		mdelay(100);
	}
}

static bool prepare_pch_speed_policy(const struct matisse_dxio_engine_result *pch,
				     unsigned int speed)
{
	const uint32_t address = 0x111402d8U + MATISSE_NBIO_SMN_OFFSET;
	uint32_t before, wanted, after;

	if (!pch)
		return false;
	if (!CONFIG(B565D4_VERMEER) || speed != 3 || pch->encoding != 0)
		return true;
	/*
	 * Original reconfiguration maps policy flags27[1:0] to this field.
	 * Refresh the explicit Gen3 policy on the measured PCH port 0 after
	 * chipset firmware, before the existing SMU rate-change request.
	 */
	before = smn_read32(address);
	if (before == UINT32_MAX)
		return false;
	wanted = (before & 0xfffffcffU) | (3U << 8);
	if (wanted != before)
		smn_write32(address, wanted);
	after = smn_read32(address);
	printk(BIOS_DEBUG, B565D4_DESKTOP_NAME
	       " PCH policy field %08x=%08x->%08x wanted=%08x\n",
	       address, before, after, wanted);
	return (after & 0x300U) == (wanted & 0x300U);
}

bool b565d4_desktop_pch_set_speed(void)
{
	const unsigned int speed = CONFIG_B565D4_DESKTOP_PCH_MAX_SPEED;
	const struct matisse_dxio_result *result = &dxio_state.events.result;
	const struct matisse_dxio_engine_result *pch = NULL;
	struct smu_payload reply = { 0 };

	if (!speed || speed > 3 || session_active || !desktop_identity() ||
	    dxio_state.phase != B565D4_MATISSE_DXIO_FINISHED ||
	    !result->count || result->count > MATISSE_NBIF_MAP_SLOTS)
		return false;
	for (unsigned int i = 0; i < result->count; i++) {
		const struct matisse_dxio_engine_result *port = &result->engines[i];

		if (port->policy_index != 2 || !port->enabled)
			continue;
		if (pch || port->policy_index >= dxio_state.port_count ||
		    port->engine_index > 15 || port->bdf != 0x0000a000)
			return false;
		pch = port;
	}
	if (!pch || pci_read_config32(DEV_PTR(gpp_bridge_0), PCI_VENDOR_ID) != 0x14831022)
		return false;
	if (!prepare_pch_speed_policy(pch, speed))
		return false;
	printk(BIOS_NOTICE, B565D4_DESKTOP_NAME " PCH SMU speed after firmware "
	       "bdf=%08x engine=%u target=Gen%u\n", pch->bdf, pch->engine_index, speed);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	session_active = true;
	const enum cb_err status = matisse_dxio_set_link_speed(pch->engine_index, speed, &reply);

	session_active = false;
	printk(BIOS_NOTICE, B565D4_DESKTOP_NAME " PCH SMU speed result=%d "
	       "raw=%08x/%08x/%08x/%08x/%08x/%08x\n", status,
	       reply.msg[0], reply.msg[1], reply.msg[2], reply.msg[3], reply.msg[4], reply.msg[5]);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (status != CB_SUCCESS || matisse_dxio_status(reply.msg[0]) != 3)
		return false;
	mdelay(100);
	return true;
}

static void enumeration_entry(void *unused)
{
	const struct native84_handoff *arena;
	const struct matisse_dxio_configured_hooks hooks = {
		.admit = base_admit, .may_access = base_access,
	};
	const struct b565d4_matisse_dxio_io io = {
		.admit = admit_dxio, .may_access = session_access,
		.smn_read = read_smn, .smn_write = write_smn,
		.pci_read = read_pci, .pci_write = write_pci, .report = report_event,
		.reset = reset_port,
	};

	native84_stage_check();
	native87_check_ecam();
	if (session_active || !desktop_identity())
		die(B565D4_DESKTOP_NAME " enumeration identity/order mismatch");
	native87_retain_pnp();
	arena = native84_descriptor();
	if (native_apob_capture(arena->parent_base, arena->parent_bytes) != NATIVE_APOB_OK)
		die(B565D4_DESKTOP_NAME " APOB CBMEM capture failed");
	if (CONFIG(B565D4_DESKTOP_SETUP))
		b565d4_setup_init();
	b565d4_desktop_fabric_prepare();
	session_active = true;
	stopwatch_init_msecs_expire(&reset_budget, 120000);
	size_t count = matisse_build_nbio_base_plan(MATISSE_NBIO_BASE_INITIAL_AM4_DEFAULTS,
						 base_plan, ARRAY_SIZE(base_plan));
	if (count != MATISSE_NBIO_BASE_MAX_STEPS ||
	    matisse_run_dxio_configured(base_plan, count, &base_state, &hooks) != CB_SUCCESS)
		die(B565D4_DESKTOP_NAME " NBIO base program failed");
	nbio_memory();
	b565d4_desktop_iohc_prepare();
	if (CONFIG(B565D4_DESKTOP_SETUP)) {
		b565d4_setup_power_failure();
		b565d4_setup_wake();
		b565d4_setup_iommu();
	}
	/* USB's desktop mailbox and PHY targets are separate from Cezanne's
	 * 49-message program. Both programs retain their original ordering.
	 */
	if (matisse_usb_run_policy(&b565d4_matisse_usb_policy, &usb_state) != CB_SUCCESS)
		die(B565D4_DESKTOP_NAME " USB firmware requests failed");
	dxio_buffer = cbmem_add(DXIO_BUFFER_ID, DXIO_BUFFER_BYTES);
	if (!dxio_buffer || ((uintptr_t)dxio_buffer & 3))
		die(B565D4_DESKTOP_NAME " DXIO buffer unavailable");
	struct b565d4_matisse_dxio_input input = {
		.slot_mode = b565d4_pcie_topology() == CZN_PCIE_FOUR_X4 ?
			B565D4_MATISSE_PCIE_FOUR_X4 : B565D4_MATISSE_PCIE_X16,
		.max_cpu_link_speed = CONFIG_B565D4_DESKTOP_PCIE_MAX_SPEED,
		.cpu_slot_speed = CONFIG(B565D4_DESKTOP_SETUP) ?
			b565d4_setup_get(B565D4_OPT_CPU_SPEED) : 0,
		.cpu_gpp_speed = CONFIG(B565D4_DESKTOP_SETUP) ?
			b565d4_setup_get(B565D4_OPT_PCIE4_SPEED) : 0,
		.reference_link_feature = CONFIG(B565D4_VERMEER),
		.reference_capability_features = CONFIG(B565D4_VERMEER),
		.acs = CONFIG(B565D4_DESKTOP_PCIE_ACS) &&
			(!CONFIG(B565D4_DESKTOP_SETUP) || b565d4_setup_get(B565D4_OPT_ACS)),
		.platform = dxio_buffer, .platform_capacity = DXIO_BUFFER_BYTES,
		.platform_physical = (uintptr_t)dxio_buffer,
	};

	printk(BIOS_NOTICE, B565D4_DESKTOP_NAME " DXIO CPU port generation policy=%u\n",
	       input.max_cpu_link_speed);
	if (b565d4_matisse_run_dxio(&input, &dxio_state, &io) != CB_SUCCESS)
		die(B565D4_DESKTOP_NAME " DXIO training failed");
	set_cpu_link_speeds();
	session_active = false;
	b565d4_matisse_disable_sdhc();
	printk(BIOS_NOTICE, B565D4_DESKTOP_NAME " native NBIO/USB/DXIO returned; normal PCI enumeration\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

static const struct domain_iohc_info iohc[] = {
	{ .fabric_id = 4, .misc_smn_base = DESKTOP_IOHC },
};

const struct domain_iohc_info *get_iohc_info(size_t *count)
{
	*count = ARRAY_SIZE(iohc);
	return iohc;
}

static const struct non_pci_mmio_reg non_pci[] = {
	{ 0x2e0, 0xfffffff00000ULL, 1 * MiB, NON_PCI_RES_IDX_AUTO },
	{ 0x2e8, 0xfffffff00000ULL, 1 * MiB, NON_PCI_RES_IDX_AUTO },
	{ 0x2f0, 0xffffffffff00ULL, 4 * KiB, 0x20000120 },
};

const struct non_pci_mmio_reg *get_iohc_non_pci_mmio_regs(size_t *count)
{
	*count = ARRAY_SIZE(non_pci);
	return non_pci;
}

static const char *root_name(const struct device *dev)
{
	return "GNB";
}

struct device_operations cezanne_root_complex_operations = {
	.read_resources = noop_read_resources,
	.set_resources = noop_set_resources,
	.enable_resources = pci_dev_enable_resources,
	.acpi_name = root_name,
};

static void soc_init(void *unused)
{
	native84_stage_check();
	default_dev_ops_root.write_acpi_tables = soc_acpi_write_tables;
	fch_init_acpi_ports();
}

struct chip_operations soc_amd_cezanne_ops = {
	.name = "AMD " B565D4_DESKTOP_NAME " desktop native bring-up",
	.init = soc_init,
};

/* Native315 temporary physical diagnosis: read the actual Promontory
 * downstream port before/after upload and retraining. No PCI data writes;
 * each CF8 selector is restored after a returned I/O access.
 */
static void native315_probe_pch(const char *phase, bool io_probe)
{
	const struct device *root = DEV_PTR(gpp_bridge_0);
	const struct device *upstream = NULL, *port = NULL;

	for (const struct device *dev = all_devices; dev; dev = dev->next) {
		if (dev->vendor == 0x1022 && dev->device == 0x43e9 &&
		    dev->upstream && dev->upstream->dev == root)
			upstream = dev;
	}
	if (upstream) {
		for (const struct device *dev = all_devices; dev; dev = dev->next) {
			if (dev->vendor == 0x1022 && dev->device == 0x43ea &&
			    dev->path.type == DEVICE_PATH_PCI && !dev->path.pci.devfn &&
			    dev->upstream && dev->upstream->dev == upstream)
				port = dev;
		}
	}
	if (!root || !upstream || !port) {
		printk(BIOS_NOTICE, "N315 PCH PROBE %s no matching discovered path\n", phase);
		return;
	}
	printk(BIOS_NOTICE, "N315 PCH PROBE %s root=%08x upstream=%08x port=%08x\n",
	       phase, pcidev_bdf(root), pcidev_bdf(upstream), pcidev_bdf(port));
	printk(BIOS_NOTICE, "N315 PCH ROUTE BEGIN %s\n", phase);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	const uint32_t root_bus = pci_read_config32(root, PCI_PRIMARY_BUS);
	const uint32_t upstream_bus = pci_read_config32(upstream, PCI_PRIMARY_BUS);

	printk(BIOS_NOTICE, "N315 PCH ROUTE END %s root=%08x upstream=%08x\n",
	       phase, root_bus, upstream_bus);
	printk(BIOS_NOTICE, "N315 PCH ECAM32 BEGIN %s\n", phase);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	const uint32_t ecam = pci_read_config32(port, PCI_VENDOR_ID);

	printk(BIOS_NOTICE, "N315 PCH ECAM32 END %s value=%08x\n", phase, ecam);
	if (!io_probe)
		return;
	const uint32_t saved = inl(0xcf8);

	printk(BIOS_NOTICE, "N315 PCH CF8_32 BEGIN %s saved=%08x\n", phase, saved);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	uint32_t value = pci_io_read_config32(pcidev_bdf(port), PCI_VENDOR_ID);

	outl(saved, 0xcf8);
	printk(BIOS_NOTICE, "N315 PCH CF8_32 END %s value=%08x restored=%08x\n",
	       phase, value, inl(0xcf8));
	printk(BIOS_NOTICE, "N315 PCH CF8_16 BEGIN %s\n", phase);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	value = pci_io_read_config16(pcidev_bdf(port), PCI_VENDOR_ID);
	outl(saved, 0xcf8);
	printk(BIOS_NOTICE, "N315 PCH CF8_16 END %s value=%08x restored=%08x\n",
	       phase, value, inl(0xcf8));
}

static void __attribute__((noinline)) matisse_enable_legacy_irq_input(void)
{
	/* The board's existing native FCH path enables this input after PIC
	 * setup. Use the common indexed FCH API and preserve all other bits.
	 */
	if (pci_s_read_config32(PCI_DEV(0, 0x14, 3), PCI_VENDOR_ID) != 0x790e1022)
		die(B565D4_DESKTOP_NAME " legacy IRQ input: FCH LPC identity mismatch");
	const uint8_t before = read_pci_int_idx(PIRQ_MISC0, 0);
	const uint8_t wanted = before | (1U << 4);

	if (before != wanted)
		write_pci_int_idx(PIRQ_MISC0, 0, wanted);
	const uint8_t after = read_pci_int_idx(PIRQ_MISC0, 0);

	printk(BIOS_NOTICE, B565D4_DESKTOP_NAME " legacy IRQ input: PIC09=%02x -> %02x\n",
	       before, after);
	if (after != wanted)
		die(B565D4_DESKTOP_NAME " legacy IRQ input readback failed");
}

static void devices_initialized(void *unused)
{
	native84_stage_check();
	native315_probe_pch("before_upload", false);
	native42_promontory_init();
	native315_probe_pch("after_upload", false);
	/* Complete the resetting speed transition before assigning resources. */
	printk(BIOS_NOTICE, B565D4_DESKTOP_NAME
	       " Promontory uplink initialized before resources\n");
	native315_probe_pch("after_retrain", true);
	matisse_enable_legacy_irq_input();
	native91_log_visible_fixed_mtrrs();
	native91_repair_legacy_shadow();
	/* Both desktop backends use the original instance-3 NBIF port pool. */
	b565d4_matisse_hide_unused_ports();
}

/* Conservative first-boot power policy: C1 only, no Cezanne SMU47 feature
 * activation and no unsupported CPPC advertisement. Desktop power services
 * need their own firmware/version-specific qualification.
 */
bool b565d4_setup_cpu_power_ready(void)
{
	return false;
}

BOOT_STATE_INIT_ENTRY(BS_DEV_ENUMERATE, BS_ON_ENTRY, enumeration_entry, NULL);
BOOT_STATE_INIT_ENTRY(BS_DEV_ENUMERATE, BS_ON_EXIT, b565d4_matisse_promontory_early, NULL);
BOOT_STATE_INIT_ENTRY(BS_DEV_INIT, BS_ON_EXIT, devices_initialized, NULL);
BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_BOOT, BS_ON_ENTRY, b565d4_desktop_ccp_hide, NULL);
