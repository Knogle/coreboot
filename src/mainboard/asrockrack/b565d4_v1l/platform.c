/* SPDX-License-Identifier: GPL-2.0-only */

#include "b565d4_cpu.h"
#include <commonlib/bsd/compiler.h>
#include <acpi/acpigen_pci.h>
#include <amdblocks/acpi.h>
#include <amdblocks/data_fabric.h>
#include <amdblocks/fsp.h>
#include <amdblocks/root_complex.h>
#include <console/console.h>
#include <device/device.h>
#include <device/pci.h>
#include <halt.h>
#include <soc/cpu.h>
#include <soc/pci_devs.h>
#include <soc/southbridge.h>
#include <types.h>

#if CONFIG(SOC_AMD_CEZANNE_NATIVE)
#include "native84_profile.h"
#include "psp_def.h"
#include "legacy_shadow_fallback.h"
#include "native87_devices.h"
#include "native_apob_resources.h"
#include "native_df_aperture.h"
#include "czn_dxio.h"
#include "native_usb_boot.h"
#include "native_promontory.h"
#include "promontory_uplink.h"
#include "pcie_acs.h"
#include <amdblocks/acpimmio.h>
#include <amdblocks/amd_pci_util.h>
#include <amdblocks/ioapic.h>
#include <amdblocks/smn.h>
#include <arch/cpu.h>
#include <arch/io.h>
#include <arch/ioapic.h>
#include <bootstate.h>
#include <commonlib/helpers.h>
#include <console/uart.h>
#include <cpu/amd/mtrr.h>
#include <cpu/amd/msr.h>
#include <cpu/x86/msr.h>
#include <device/mmio.h>
#include <delay.h>
#include <pc80/mc146818rtc.h>
#include <static.h>
#include <soc/iomap.h>
#include "setup_options.h"

extern struct device_operations amd_cpu_bus_ops;

static void native_gnb_ioapic_prepare(void);
static void native166_psp_prepare(void);
static bool native166_psp_before_gnb_valid(uint32_t lo, uint32_t hi);
static void native170_sdhc_disable(void);
static void native179_biosram_ro(void);
static void native215_nbio_arbitration(void);

static void native87_enumeration_entry(void *unused)
{
	const struct native84_handoff *descriptor;
	enum native_apob_status status;

	native84_stage_check();
	/* No generic ECAM/DF/SMN access precedes this fresh read-only guard. */
	native87_check_ecam();
	b565d4_setup_init();
	b565d4_setup_power_failure();
	b565d4_setup_wake();
	if (CONFIG(B565D4_BOARD_SETUP))
		b565d4_setup_watchdog();
	if (CONFIG(B565D4_BOARD_SETUP))
		b565d4_setup_iommu();
	if (CONFIG(B565D4_PCIE_ACS) &&
	    (!CONFIG(B565D4_BOARD_SETUP) || b565d4_setup_get(B565D4_OPT_ACS)))
		b565d4_pcie_acs_prepare();
	native215_nbio_arbitration();
	native170_sdhc_disable();
	native87_retain_pnp();
	descriptor = native84_descriptor();
	status = native_apob_capture(descriptor->parent_base, descriptor->parent_bytes);
	printk(BIOS_INFO, "NATIVE99 APOB SNAPSHOT status=%u\n", status);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (status != NATIVE_APOB_OK)
		native84_terminal();
	if (native84_profile_policy.entry_path == 2)
		native_usb_boot_prepare();
	/* Publish the existing owned DF/PSP aperture before the DXIO requests.
	 * IRQ/IOAPIC preparation remains after training and runs only once.
	 */
	native107_df_posted_aperture();
	if (CONFIG(B565D4_BOARD_SETUP))
		b565d4_setup_high_mmio();
	native166_psp_prepare();
	native179_biosram_ro();
	native106_dxio_train();
	native137_df_io_aperture();
	native_gnb_ioapic_prepare();
	printk(BIOS_INFO, "NATIVE87 BS_DEV_ENUMERATE RETURN\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

static void native88_topology_refusal(unsigned int line) __noreturn;
static void native88_topology_refusal(unsigned int line)
{
	printk(BIOS_ERR, "NATIVE88 TOPOLOGY REFUSAL line=%u\n", line);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	native84_terminal();
}

/* Native215: literal same-board OEM BaseRnPei table [TE42d5, TE475b).
 * The OEM topology callback executes this section before DXIO; all115
 * records are unconditional once its handle is selected. Native scope is
 * established below, independently of the unmeasured vendor handle byte.
 * Full-write records gain no target data read or verification operation.
 */
static bool native215_nbio_started;

static void native215_nbio_arbitration(void)
{
	static const struct {
		uint32_t address, value;
	} direct[] = {
		{ 0x13b14480U, 0x00000040U },
		{ 0x13b14884U, 0x00000040U },
		{ 0x13b14c88U, 0x00000040U },
		{ 0x13b1508cU, 0x00000040U },
		{ 0x13b15490U, 0x00000040U },
		{ 0x13b14400U, 0x08080808U },
		{ 0x13b14804U, 0x08080808U },
		{ 0x13b14c08U, 0x08080808U },
		{ 0x13b1500cU, 0x08080808U },
		{ 0x13b14404U, 0x08080808U },
		{ 0x13b14808U, 0x08080808U },
		{ 0x13b14c0cU, 0x08080808U },
		{ 0x13b15010U, 0x08080808U },
		{ 0x13b14408U, 0x21212121U },
		{ 0x13b1440cU, 0x00000000U },
		{ 0x13b1480cU, 0x00000000U },
		{ 0x13b14810U, 0x00000000U },
		{ 0x13b14c10U, 0x08080808U },
		{ 0x13b14c14U, 0x08080808U },
		{ 0x13b15014U, 0x00000000U },
		{ 0x13b15018U, 0x00000000U },
		{ 0x13b14430U, 0x08080808U },
		{ 0x13b14834U, 0x08080808U },
		{ 0x13b14c38U, 0x08080808U },
		{ 0x13b1503cU, 0x08080808U },
		{ 0x13b14434U, 0x08080808U },
		{ 0x13b14838U, 0x08080808U },
		{ 0x13b14c3cU, 0x08080808U },
		{ 0x13b15040U, 0x08080808U },
		{ 0x13b14438U, 0x00000000U },
		{ 0x13b1483cU, 0x00000000U },
		{ 0x13b14c40U, 0x00000000U },
		{ 0x13b15044U, 0x00000000U },
		{ 0x13b1443cU, 0x00000000U },
		{ 0x13b14840U, 0x00000000U },
		{ 0x13b14c44U, 0x00000000U },
		{ 0x13b15048U, 0x00000000U },
		{ 0x13b14410U, 0x02020202U },
		{ 0x13b14814U, 0x02020202U },
		{ 0x13b14c18U, 0x02020202U },
		{ 0x13b1501cU, 0x02020202U },
		{ 0x13b14414U, 0x02020202U },
		{ 0x13b14818U, 0x02020202U },
		{ 0x13b14c1cU, 0x02020202U },
		{ 0x13b15020U, 0x02020202U },
		{ 0x13b14440U, 0x02020202U },
		{ 0x13b14844U, 0x02020202U },
		{ 0x13b14c48U, 0x02020202U },
		{ 0x13b1504cU, 0x02020202U },
		{ 0x13b14444U, 0x02020202U },
		{ 0x13b14848U, 0x02020202U },
		{ 0x13b14c4cU, 0x02020202U },
		{ 0x13b15050U, 0x02020202U },
		{ 0x15b00400U, 0x08080808U },
		{ 0x15b00800U, 0x08080808U },
		{ 0x15b00c00U, 0x08080808U },
		{ 0x15b01000U, 0x08080808U },
		{ 0x15b01400U, 0x08080808U },
		{ 0x15b00404U, 0x08080808U },
		{ 0x15b00804U, 0x08080808U },
		{ 0x15b00c04U, 0x08080808U },
		{ 0x15b01004U, 0x08080808U },
		{ 0x15b01404U, 0x08080808U },
		{ 0x15b00408U, 0x00000000U },
		{ 0x15b00808U, 0x00000000U },
		{ 0x15b00c08U, 0x00000000U },
		{ 0x15b0040cU, 0x00000000U },
		{ 0x15b0080cU, 0x00000000U },
		{ 0x15b00c0cU, 0x00000000U },
		{ 0x15b01008U, 0x08080808U },
		{ 0x15b0100cU, 0x08080808U },
		{ 0x15b01408U, 0x00000000U },
		{ 0x15b0140cU, 0x00000000U },
		{ 0x15b00430U, 0x08080808U },
		{ 0x15b00830U, 0x08080808U },
		{ 0x15b00c30U, 0x08080808U },
		{ 0x15b01030U, 0x08080808U },
		{ 0x15b01430U, 0x08080808U },
		{ 0x15b00434U, 0x08080808U },
		{ 0x15b00834U, 0x08080808U },
		{ 0x15b00c34U, 0x08080808U },
		{ 0x15b01034U, 0x08080808U },
		{ 0x15b01434U, 0x08080808U },
		{ 0x15b00438U, 0x00000000U },
		{ 0x15b00838U, 0x00000000U },
		{ 0x15b00c38U, 0x00000000U },
		{ 0x15b01038U, 0x00000000U },
		{ 0x15b01438U, 0x00000000U },
		{ 0x15b0043cU, 0x00000000U },
		{ 0x15b0083cU, 0x00000000U },
		{ 0x15b00c3cU, 0x00000000U },
		{ 0x15b0103cU, 0x00000000U },
		{ 0x15b0143cU, 0x00000000U },
		{ 0x15b00410U, 0x02020202U },
		{ 0x15b00810U, 0x02020202U },
		{ 0x15b00c10U, 0x02020202U },
		{ 0x15b01010U, 0x02020202U },
		{ 0x15b01410U, 0x02020202U },
		{ 0x15b00414U, 0x02020202U },
		{ 0x15b00814U, 0x02020202U },
		{ 0x15b00c14U, 0x02020202U },
		{ 0x15b01014U, 0x02020202U },
		{ 0x15b01414U, 0x02020202U },
		{ 0x15b00440U, 0x02020202U },
		{ 0x15b00840U, 0x02020202U },
		{ 0x15b00c40U, 0x02020202U },
		{ 0x15b01040U, 0x02020202U },
		{ 0x15b01440U, 0x02020202U },
		{ 0x15b00444U, 0x02020202U },
		{ 0x15b00844U, 0x02020202U },
		{ 0x15b00c44U, 0x02020202U },
		{ 0x15b01044U, 0x02020202U },
		{ 0x15b01444U, 0x02020202U },
	};
	static const struct {
		uint32_t address, clear, set;
	} rmw[] = {
		{ 0x13b14040U, 0xffffffffU, 0x00e00000U },
		{ 0x13b14044U, 0x1f7f1f7fU, 0x1030102fU },
	};
	/* Native217: exact same-OEM table [TE4811, TE49e7), end exclusive.
	 * First four rows are unconditional. The other eighteen were selected by
	 * PCD 19e/27c/99/122/1a2 == 1 in both saved OEM-derived debug runs.
	 * This is an explicit recorded policy choice, not live PCD emulation or
	 * a CCP-enable claim. Preserve repeated targets and set/clear ordering.
	 */
	static const struct {
		uint32_t address, clear, set;
	} policy22[] = {
		{ 0x13b10020U, 0x00000001U, 0x00000001U }, /* 0x4811, always */
		{ 0x13b10348U, 0x00000001U, 0x00000001U }, /* 0x481f, always */
		{ 0x15b00090U, 0x00000001U, 0x00000001U }, /* 0x482d, always */
		{ 0x13b10348U, 0x00000001U, 0x00000000U }, /* 0x483b, always */
		{ 0x13b10088U, 0xffc00000U, 0x00000000U }, /* 0x4849, PCD 19e == 1 */
		{ 0x13b1008cU, 0xffc00000U, 0x00000000U }, /* 0x4860, PCD 19e == 1 */
		{ 0x15b00000U, 0xffc00000U, 0x00000000U }, /* 0x4877, PCD 19e == 1 */
		{ 0x15b00004U, 0xe0000000U, 0x00000000U }, /* 0x488e, PCD 19e == 1 */
		{ 0x14300100U, 0xc0000000U, 0x00000000U }, /* 0x48a5, PCD 19e == 1 */
		{ 0x17400004U, 0x00010001U, 0x00010001U }, /* 0x48bc, PCD 27c == 1 */
		{ 0x17400004U, 0x0000ffc0U, 0x00003c00U }, /* 0x48d3, PCD 27c == 1 */
		{ 0x17500004U, 0x0000ffc0U, 0x00003c00U }, /* 0x48ea, PCD 27c == 1 */
		{ 0x17400404U, 0x00000010U, 0x00000010U }, /* 0x4901, PCD 27c == 1 */
		{ 0x13b10340U, 0x00000100U, 0x00000100U }, /* 0x4918, PCD 99 == 1 */
		{ 0x15b00080U, 0x00000100U, 0x00000100U }, /* 0x492f, PCD 99 == 1 */
		{ 0x13b10348U, 0x00000001U, 0x00000001U }, /* 0x4946, PCD 99 == 1 */
		{ 0x13b10348U, 0x00000001U, 0x00000000U }, /* 0x495d, PCD 99 == 1 */
		{ 0x15b00090U, 0x00000001U, 0x00000001U }, /* 0x4974, PCD 99 == 1 */
		{ 0x1013a21cU, 0x00000001U, 0x00000001U }, /* 0x498b, PCD 122 == 1 */
		{ 0x01410020U, 0x00000001U, 0x00000001U }, /* 0x49a2, PCD 1a2 == 1 */
		{ 0x01411020U, 0x00000001U, 0x00000001U }, /* 0x49b9, PCD 1a2 == 1 */
		{ 0x1013a0e0U, 0x000001ffU, 0x00000050U }, /* 0x49d0, PCD 1a2 == 1 */
	};
	const struct domain_iohc_info *iohc;
	size_t count;
	unsigned int direct_stores = 0, rmw_stores = 0;
	unsigned int policy_stores = 0;
	uint32_t sb_location;

	native84_stage_check();
	if (native84_profile_policy.entry_path != 2)
		return;
	_Static_assert(ARRAY_SIZE(direct) == 113 && ARRAY_SIZE(rmw) == 2,
		"Exact OEM NBIO arbitration section; end475b exclusive");
	iohc = get_iohc_info(&count);
	if (native215_nbio_started ||
	    !(rdmsr(IA32_APIC_BASE_MSR_INDEX).lo & BIT(8)) ||
	    !b565d4_is_cezanne(cpuid_eax(1)) ||
	    pci_s_read_config32(PCI_DEV(0, 0, 0), PCI_VENDOR_ID) != 0x16301022 ||
	    !iohc || count != 1 || iohc[0].misc_smn_base != 0x13b10000 ||
	    iohc[0].fabric_id != 10)
		native88_topology_refusal(__LINE__);
	/* Consume before the first attempted store. No automatic replay. */
	native215_nbio_started = true;
	/* Native216: OEM BaseInit copies the complete current SB_LOCATION DWORD.
	 * No constant replacement, destination readback or automatic replay.
	 */
	printk(BIOS_INFO, "NATIVE216 SB_LOCATION BEGIN source=13b1007c "
	       "destination1=14800024 destination2=13f0112c "
	       "source_reads=1 direct_stores=2 target_readback=0\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	sb_location = smn_read32(0x13b1007cU);
	smn_write32(0x14800024U, sb_location);
	smn_write32(0x13f0112cU, sb_location);
	printk(BIOS_INFO, "NATIVE216 SB_LOCATION END raw=%08x "
	       "source_reads=1 direct_stores=2 target_readback=0 "
	       "stores_attempted_not_verified=1\n", sb_location);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	printk(BIOS_INFO, "NATIVE215 NBIO ARBITRATION BEGIN direct=113 rmw=2 "
	       "section=42d5:475b end_exclusive=1 native_bus=0 native_offset=0 "
	       "target_readback=0\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	for (size_t i = 0; i < ARRAY_SIZE(direct); i++) {
		smn_write32(direct[i].address, direct[i].value);
		direct_stores++;
	}
	for (size_t i = 0; i < ARRAY_SIZE(rmw); i++) {
		const uint32_t old = smn_read32(rmw[i].address);

		smn_write32(rmw[i].address, (old & ~rmw[i].clear) | rmw[i].set);
		rmw_stores++;
	}
	printk(BIOS_INFO, "NATIVE215 NBIO ARBITRATION END direct_stores=%u "
	       "rmw_stores=%u target_reads=2 target_readback=0 "
	       "stores_attempted_not_verified=1\n", direct_stores, rmw_stores);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	_Static_assert(ARRAY_SIZE(policy22) == 22,
		"Exact OEM policy22 section; end49e7 exclusive");
	printk(BIOS_INFO, "NATIVE217 NBIO POLICY22 BEGIN records=22 rmw=22 "
	       "section=4811:49e7 end_exclusive=1 recorded_OEM_selected=1 "
	       "target_readback=0\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	for (size_t i = 0; i < ARRAY_SIZE(policy22); i++) {
		const uint32_t old = smn_read32(policy22[i].address);

		smn_write32(policy22[i].address,
			(old & ~policy22[i].clear) | policy22[i].set);
		policy_stores++;
	}
	printk(BIOS_INFO, "NATIVE217 NBIO POLICY22 END target_reads=22 "
	       "stores=%u target_readback=0 stores_attempted_not_verified=1\n",
	       policy_stores);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

/* Source87-based proposal only. Same-board BaseRnPei 09c57c66 programs
 * the IOAPIC high/disabled-low/enabled-low, feature OR14 and bus0 policy100.
 * Its table record at TE file47e7 is GP17:14300068/mask1f0037/set80023.
 * PSP policy remains unchanged; Native153 restores the vendor NBIF word below.
 */
#define NATIVE_GNB_IOAPIC_LO 0x13b102f0
#define NATIVE_GNB_IOAPIC_HI 0x13b102f4
#define NATIVE_GNB_FEATURES  0x14300000
#define NATIVE_GNB_BUS_NUM   0x13b10044
#define NATIVE_GNB_GP17_IRQ  0x14300068
#define NATIVE_GNB_GP17_MASK 0x001f0037
#define NATIVE_GNB_GP17_SET  0x00080023
#define NATIVE_NBIF_INT_LINE_ENABLE 0x1013a008
#define NATIVE_NBIF_INT_LINE_VENDOR 0x000703ff
#define NATIVE_GNB_CHILD_IRQ_MASK 0x001f0037

/* Exact same-board BaseRnPei tag1/scope1 records475b/4785/4793/47f5.
 * ADR/_PRT joins: Original GPP0->GP09, GPP3->GP11, GPP4->GP12, GP18->GP42.
 * The high field is bridge_irq/map, not the child interrupt group.
 */
static const struct {
	uint32_t reg;
	uint32_t set;
} native_gnb_child_irq[] = {
	{ 0x14300040, 0x00100000 }, /* GP09: group0/ABCD -> GSI24..27 */
	{ 0x1430004c, 0x000c0003 }, /* GP11: group3/ABCD -> GSI36..39 */
	{ 0x14300050, 0x000d0004 }, /* GP12: group4/ABCD -> GSI40..43 */
	{ 0x1430006c, 0x00090022 }, /* GP42: group2/CDAB -> GSI34,35,32,33 */
};

static bool native_gnb_ioapic_prepared;

static void native_gnb_store(uint32_t reg, uint32_t value)
{
	uint32_t after;

	smn_write32(reg, value);
	after = smn_read32(reg);
	printk(BIOS_INFO, "NATIVE_GNB SMN reg=%08x wanted=%08x after=%08x\n",
	       reg, value, after);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (after != value)
		native88_topology_refusal(__LINE__);
}

static void native_gnb_ioapic_prepare(void)
{
	const struct domain_iohc_info *iohc;
	size_t count;
	uint32_t lo, hi, features, bus, route, psp_lo, psp_hi, nbif_int_line;
	uint32_t child_route[ARRAY_SIZE(native_gnb_child_irq)];
	msr_t top;

	native84_stage_check();
	if (native84_profile_policy.entry_path != 2)
		return;
	_Static_assert(GNB_IO_APIC_ADDR == 0xfec01000 && IO_APIC_ADDR == 0xfec00000 &&
		SPI_BASE_ADDRESS == 0xfec10000 && GNB_IOAPIC_ID == 1,
		"Selected Cezanne adjacent fixed IOAPIC apertures");
	iohc = get_iohc_info(&count);
	top = rdmsr(TOP_MEM_MSR);
	if (native_gnb_ioapic_prepared || !iohc || count != 1 ||
	    iohc[0].misc_smn_base != 0x13b10000 || iohc[0].fabric_id != 10 ||
	    !b565d4_is_cezanne(cpuid_eax(1)) ||
	    pci_s_read_config32(PCI_DEV(0, 0, 0), PCI_VENDOR_ID) != 0x16301022 ||
	    pci_s_read_config32(PCI_DEV(0, 8, 1), PCI_VENDOR_ID) != 0x16351022 ||
	    pci_s_read_config32(PCI_DEV(0, 1, 1), PCI_VENDOR_ID) != 0x16331022 ||
	    pci_s_read_config32(PCI_DEV(0, 2, 1), PCI_VENDOR_ID) != 0x16341022 ||
	    pci_s_read_config32(PCI_DEV(0, 8, 2), PCI_VENDOR_ID) != 0x16351022 ||
	    top.hi || !top.lo || (top.lo & 0xffff) || top.lo > IO_APIC_ADDR)
		native88_topology_refusal(__LINE__);
	psp_lo = smn_read32(0x13b102e0);
	psp_hi = smn_read32(0x13b102e4);
	lo = smn_read32(NATIVE_GNB_IOAPIC_LO);
	hi = smn_read32(NATIVE_GNB_IOAPIC_HI);
	features = smn_read32(NATIVE_GNB_FEATURES);
	bus = smn_read32(NATIVE_GNB_BUS_NUM);
	route = smn_read32(NATIVE_GNB_GP17_IRQ);
	printk(BIOS_INFO, "NATIVE_GNB PRE lo=%08x hi=%08x features=%08x bus=%08x "
	       "GP17=%08x PSP=%08x:%08x\n", lo, hi, features, bus, route,
	       psp_hi, psp_lo);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	/* Accept only measured cold state or this explicit retained policy.
	 * Preserve existing feature bit9; do not guess a feature-control bit0.
	 */
	if (!native166_psp_before_gnb_valid(psp_lo, psp_hi) || hi ||
	    (lo != 0xfec00000 && lo != GNB_IO_APIC_ADDR &&
	     lo != (GNB_IO_APIC_ADDR | IOHC_MMIO_EN)) ||
	    (features != 0x204 && features != 0x214) ||
	    (bus != 0 && bus != 0x100) || route == 0xffffffff)
		native88_topology_refusal(__LINE__);
	/* Same-board BaseRnPei table FILE42cb: tag0/scope0 writes this exact
	 * word before the GP routing records. Do not infer per-function bits.
	 */
	nbif_int_line = smn_read32(NATIVE_NBIF_INT_LINE_ENABLE);
	printk(BIOS_INFO, "NATIVE153 NBIF interrupt-line reg=%08x before=%08x "
	       "wanted=%08x vendor_direct_word=1\n", NATIVE_NBIF_INT_LINE_ENABLE,
	       nbif_int_line, NATIVE_NBIF_INT_LINE_VENDOR);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (nbif_int_line == 0xffffffff)
		native88_topology_refusal(__LINE__);
	/* Qualify every added group before the first NBIF/route write.
	 * Preserve all unowned bits; reject absent SMN records before mutation.
	 */
	for (size_t i = 0; i < ARRAY_SIZE(native_gnb_child_irq); i++) {
		child_route[i] = smn_read32(native_gnb_child_irq[i].reg);
		printk(BIOS_INFO, "NATIVE_GNB CHILD_IRQ reg=%08x before=%08x "
		       "mask=%08x set=%08x\n", native_gnb_child_irq[i].reg,
		       child_route[i], NATIVE_GNB_CHILD_IRQ_MASK,
		       native_gnb_child_irq[i].set);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if (child_route[i] == 0xffffffff)
			native88_topology_refusal(__LINE__);
	}
	/* Exactly one vendor-word write; native_gnb_store checks/logs readback. */
	native_gnb_store(NATIVE_NBIF_INT_LINE_ENABLE, NATIVE_NBIF_INT_LINE_VENDOR);
	/* Each fixed route has a corresponding child _PRT; unchanged masks
	 * produce no SMN write. The separate root-own INTA table is untouched.
	 */
	for (size_t i = 0; i < ARRAY_SIZE(native_gnb_child_irq); i++) {
		uint32_t wanted = (child_route[i] & ~NATIVE_GNB_CHILD_IRQ_MASK) |
				  native_gnb_child_irq[i].set;

		if (child_route[i] != wanted)
			native_gnb_store(native_gnb_child_irq[i].reg, wanted);
		else
			printk(BIOS_INFO, "NATIVE_GNB CHILD_IRQ reg=%08x noop=%08x\n",
			       native_gnb_child_irq[i].reg, wanted);
	}
	/* GP17/GP41 and the Source90 NBIF vendor word retain their policy. */
	if ((route & NATIVE_GNB_GP17_MASK) != NATIVE_GNB_GP17_SET)
		native_gnb_store(NATIVE_GNB_GP17_IRQ,
			(route & ~NATIVE_GNB_GP17_MASK) | NATIVE_GNB_GP17_SET);
	if (lo != (GNB_IO_APIC_ADDR | IOHC_MMIO_EN)) {
		native_gnb_store(NATIVE_GNB_IOAPIC_HI, 0);
		native_gnb_store(NATIVE_GNB_IOAPIC_LO, GNB_IO_APIC_ADDR);
		native_gnb_store(NATIVE_GNB_IOAPIC_LO,
			GNB_IO_APIC_ADDR | IOHC_MMIO_EN);
	}
	if (features != (features | 0x14))
		native_gnb_store(NATIVE_GNB_FEATURES, features | 0x14);
	if (bus != 0x100)
		native_gnb_store(NATIVE_GNB_BUS_NUM, 0x100);
	if (smn_read32(NATIVE_GNB_IOAPIC_LO) != (GNB_IO_APIC_ADDR | IOHC_MMIO_EN) ||
	    smn_read32(NATIVE_GNB_IOAPIC_HI) ||
	    smn_read32(NATIVE_GNB_FEATURES) != (features | 0x14) ||
	    smn_read32(NATIVE_GNB_BUS_NUM) != 0x100 ||
	    smn_read32(NATIVE_GNB_GP17_IRQ) !=
		((route & ~NATIVE_GNB_GP17_MASK) | NATIVE_GNB_GP17_SET) ||
	    smn_read32(0x13b102e0) != psp_lo || smn_read32(0x13b102e4) != psp_hi)
		native88_topology_refusal(__LINE__);
	for (size_t i = 0; i < ARRAY_SIZE(native_gnb_child_irq); i++) {
		uint32_t wanted = (child_route[i] & ~NATIVE_GNB_CHILD_IRQ_MASK) |
				  native_gnb_child_irq[i].set;

		if (smn_read32(native_gnb_child_irq[i].reg) != wanted)
			native88_topology_refusal(__LINE__);
	}
	native_gnb_ioapic_prepared = true;
	printk(BIOS_INFO, "NATIVE_GNB aperture=fec01000+1000 GP17group3/CDAB; "
	       "genuine fixed-resource/domain-init/MADT next; endpoint INTx unchanged\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

/* Same-board AmdPspPeiV2 1cd9..1e55 allocates a 1MiB NONPCI window on
 * the BSP, writes base|0x101 and high, then initializes a zero PSPADDR.
 * This selected window stays inside the existing native posted aperture.
 * No PSP MMIO access, command, SMM transition or AP MSR change is performed.
 */
#define NATIVE166_PSP_BASE 0xc0100000ULL
#define NATIVE166_PSP_SIZE 0x00100000ULL
#define NATIVE166_PSP_END (NATIVE166_PSP_BASE + NATIVE166_PSP_SIZE)
#define NATIVE166_PSP_LO 0x13b102e0
#define NATIVE166_PSP_HI 0x13b102e4
#define NATIVE166_PSP_LOCK BIT(8)
#define NATIVE166_PSP_VALUE (NATIVE166_PSP_BASE | IOHC_MMIO_EN | NATIVE166_PSP_LOCK)

static bool native166_psp_prepared;
static bool native214_psp_resource_verified;
static uint64_t native166_non_pci_before[8];
static struct {
	uint32_t base, limit, control;
} native166_df_before[DF_MMIO_REG_SET_COUNT];

static bool native166_psp_overlap(uint64_t base, uint64_t size)
{
	if (base + size < base)
		native88_topology_refusal(__LINE__);
	return size && base < NATIVE166_PSP_END && NATIVE166_PSP_BASE < base + size;
}

/* Preserve the original zero-state admission before PSP publication.
 * After our earlier publication, require the complete own pair and BSP MSR;
 * a vanished or partial publication must not pass as the old cold state.
 */
static bool native166_psp_before_gnb_valid(uint32_t lo, uint32_t hi)
{
	if (!native166_psp_prepared)
		return !lo && !hi;
	return lo == NATIVE166_PSP_VALUE && !hi &&
		rdmsr(PSP_ADDR_MSR).raw == NATIVE166_PSP_BASE;
}

static void native166_psp_check_windows(bool installed)
{
	const struct non_pci_mmio_reg *regs;
	size_t count;

	regs = get_iohc_non_pci_mmio_regs(&count);
	if (!regs || count != ARRAY_SIZE(native166_non_pci_before))
		native88_topology_refusal(__LINE__);
	for (size_t i = 0; i < count; i++) {
		uint64_t wanted = native166_non_pci_before[i];

		if (regs[i].iohc_misc_offset == 0x2e0 && installed)
			wanted = NATIVE166_PSP_VALUE;
		else if (regs[i].iohc_misc_offset == 0x2f0 && installed &&
			 native_gnb_ioapic_prepared)
			/* Only the verified own late-GNB IOAPIC pair may change. */
			wanted = (uint64_t)GNB_IO_APIC_ADDR | IOHC_MMIO_EN;

		if (smn_read64(SMN_IOHC_MISC_BASE_13B1 + regs[i].iohc_misc_offset) != wanted)
			native88_topology_refusal(__LINE__);
	}
	for (unsigned int i = 0; i < DF_MMIO_REG_SET_COUNT; i++) {
		if (data_fabric_broadcast_read32(DF_MMIO_BASE(i)) != native166_df_before[i].base ||
		    data_fabric_broadcast_read32(DF_MMIO_LIMIT(i)) != native166_df_before[i].limit ||
		    data_fabric_broadcast_read32(DF_MMIO_CONTROL(i)) != native166_df_before[i].control)
			native88_topology_refusal(__LINE__);
	}
}

static void native166_psp_prepare(void)
{
	const struct non_pci_mmio_reg *regs;
	uint32_t lo, hi;
	msr_t addr, top, top2;
	size_t count;
	unsigned int psp_descriptors = 0, covering_routes = 0;

	native84_stage_check();
	if (native84_profile_policy.entry_path != 2)
		return;
	_Static_assert(NATIVE166_PSP_END == 0xc0200000 && DF_MMIO_REG_SET_COUNT == 8 &&
		(NATIVE166_PSP_BASE & (NATIVE166_PSP_SIZE - 1)) == 0 &&
		CONFIG_SOC_AMD_COMMON_BLOCK_CPU_SYNC_PSP_ADDR_MSR,
		"Selected 1MiB PSP aperture and Cezanne DF geometry");
	top = rdmsr(TOP_MEM_MSR);
	top2 = rdmsr(TOP_MEM2_MSR);
	addr = rdmsr(PSP_ADDR_MSR);
	lo = smn_read32(NATIVE166_PSP_LO);
	hi = smn_read32(NATIVE166_PSP_HI);
	printk(BIOS_INFO, "NATIVE166 PSP PRE pair=%08x:%08x PSPADDR=%08x:%08x "
	       "window=c0100000+100000\n", hi, lo, addr.hi, addr.lo);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (native166_psp_prepared ||
	    !(rdmsr(IA32_APIC_BASE_MSR_INDEX).lo & BIT(8)) || top.hi ||
	    !top.lo || top.lo > NATIVE166_PSP_BASE ||
	    native_apob_require_pci_hole(NATIVE166_PSP_BASE, NATIVE166_PSP_END) != NATIVE_APOB_OK ||
	    !((!lo && !hi && !addr.lo && !addr.hi) ||
	      (lo == NATIVE166_PSP_VALUE && !hi && addr.lo == NATIVE166_PSP_BASE && !addr.hi)))
		native88_topology_refusal(__LINE__);
	regs = get_iohc_non_pci_mmio_regs(&count);
	if (!regs || count != ARRAY_SIZE(native166_non_pci_before))
		native88_topology_refusal(__LINE__);
	for (size_t i = 0; i < count; i++) {
		uint64_t value = smn_read64(SMN_IOHC_MISC_BASE_13B1 + regs[i].iohc_misc_offset);

		native166_non_pci_before[i] = value;
		if (regs[i].iohc_misc_offset == 0x2e0) {
			if (value != ((uint64_t)hi << 32 | lo) ||
			    regs[i].mask != 0xfffffff00000ULL || regs[i].size != NATIVE166_PSP_SIZE ||
			    regs[i].res_idx != NON_PCI_RES_IDX_AUTO)
				native88_topology_refusal(__LINE__);
			psp_descriptors++;
		} else if (value == UINT64_MAX || ((value & IOHC_MMIO_EN) &&
			   native166_psp_overlap(value & regs[i].mask, regs[i].size))) {
			native88_topology_refusal(__LINE__);
		}
	}
	for (unsigned int i = 0; i < DF_MMIO_REG_SET_COUNT; i++) {
		uint64_t begin, end;
		union df_mmio_control control;

		native166_df_before[i].base = data_fabric_broadcast_read32(DF_MMIO_BASE(i));
		native166_df_before[i].limit = data_fabric_broadcast_read32(DF_MMIO_LIMIT(i));
		native166_df_before[i].control = data_fabric_broadcast_read32(DF_MMIO_CONTROL(i));
		control.raw = native166_df_before[i].control;
		if (control.raw == 0xffffffff)
			native88_topology_refusal(__LINE__);
		if (!control.re && !control.we)
			continue;
		begin = (uint64_t)native166_df_before[i].base << DF_MMIO_SHIFT;
		end = ((uint64_t)native166_df_before[i].limit + 1) << DF_MMIO_SHIFT;
		if (begin >= end)
			native88_topology_refusal(__LINE__);
		if (!native166_psp_overlap(begin, end - begin))
			continue;
		if (!control.re || !control.we || control.np ||
		    control.dst_fabric_id != IOMS0_FABRIC_ID ||
		    begin > NATIVE166_PSP_BASE || end < NATIVE166_PSP_END)
			native88_topology_refusal(__LINE__);
		covering_routes++;
	}
	if (psp_descriptors != 1 || covering_routes != 1)
		native88_topology_refusal(__LINE__);
	native166_psp_check_windows(false);
	if (rdmsr(TOP_MEM_MSR).raw != top.raw || rdmsr(TOP_MEM2_MSR).raw != top2.raw ||
	    rdmsr(PSP_ADDR_MSR).raw != addr.raw)
		native88_topology_refusal(__LINE__);
	if (!lo) {
		/* Match the same-board PEI store order. The selected high word is zero.
		 * Locking is deliberate; failures are terminal, not claimed reversible.
		 */
		smn_write32(NATIVE166_PSP_LO, NATIVE166_PSP_VALUE);
		smn_write32(NATIVE166_PSP_HI, 0);
	}
	native166_psp_check_windows(true);
	if (!addr.raw)
		wrmsr(PSP_ADDR_MSR, (msr_t) { .lo = NATIVE166_PSP_BASE, .hi = 0 });
	if (rdmsr(PSP_ADDR_MSR).raw != NATIVE166_PSP_BASE ||
	    rdmsr(TOP_MEM_MSR).raw != top.raw || rdmsr(TOP_MEM2_MSR).raw != top2.raw)
		native88_topology_refusal(__LINE__);
	native166_psp_prepared = true;
	printk(BIOS_INFO, "NATIVE166 PSP OWNED pair=00000000:c0100101 "
	       "BSP_PSPADDR=00000000:c0100000 other_nonPCI_DF_unchanged=1 "
	       "AP_sync=genuine_final_pending PSP_commands=0; RESOURCE_RESERVATION_PENDING\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

static void native166_psp_verify_resource(struct device *domain)
{
	const struct resource *owned = NULL;
	const struct device *dev;
	unsigned int matches = 0;
	const unsigned long required = IORESOURCE_MEM | IORESOURCE_FIXED |
		IORESOURCE_RESERVE | IORESOURCE_STORED | IORESOURCE_ASSIGNED;

	if (!native166_psp_prepared || domain->path.type != DEVICE_PATH_DOMAIN ||
	    domain->path.domain.domain_id || rdmsr(PSP_ADDR_MSR).raw != NATIVE166_PSP_BASE)
		native88_topology_refusal(__LINE__);
	native166_psp_check_windows(true);
	/* The existing 0x2e0 descriptor uses AUTO: discover its actual reserved
	 * resource by the admitted base/size/flags, never guess an allocator index.
	 */
	for (const struct resource *r = domain->resource_list; r; r = r->next) {
		if (r->base == NATIVE166_PSP_BASE && r->size == NATIVE166_PSP_SIZE &&
		    (r->flags & required) == required) {
			owned = r;
			matches++;
		}
	}
	if (matches != 1)
		native88_topology_refusal(__LINE__);
	for (dev = all_devices; dev; dev = dev->next) {
		for (const struct resource *r = dev->resource_list; r; r = r->next) {
			if (r == owned || (r->flags & (IORESOURCE_MEM | IORESOURCE_ASSIGNED)) !=
			    (IORESOURCE_MEM | IORESOURCE_ASSIGNED))
				continue;
			/* Domain allocation windows describe free address containers;
			 * assigned PCI bridge windows and every fixed range remain checked.
			 */
			if (dev->path.type == DEVICE_PATH_DOMAIN && !(r->flags & IORESOURCE_FIXED))
				continue;
			if (native166_psp_overlap(r->base, r->size))
				native88_topology_refusal(__LINE__);
		}
	}
	native214_psp_resource_verified = true;
	printk(BIOS_INFO, "NATIVE166 PSP RESERVED index=%lx base=%llx size=%llx flags=%lx "
	       "assigned_nonoverlap=1 AP_sync=genuine_final_pending PSP_commands=0\n", owned->index,
	       (unsigned long long)owned->base, (unsigned long long)owned->size, owned->flags);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

/* Reject an unmapped/all-ones IOAPIC before the generic count helper can
 * interpret its legacy ff fallback as 24 valid inputs. RTEs stay masked.
 */
static void native_gnb_ioapic_domain_init(struct device *domain)
{
	const struct resource *res;
	uint32_t id, version, last;

	if (native84_profile_policy.entry_path != 2) {
		amd_pci_domain_init(domain);
		return;
	}
	native84_stage_check();
	native166_psp_verify_resource(domain);
	res = probe_resource(domain, IOMMU_IOAPIC_IDX);
	if (!native_gnb_ioapic_prepared || domain->path.type != DEVICE_PATH_DOMAIN ||
	    domain->path.domain.domain_id || !res || res->base != GNB_IO_APIC_ADDR ||
	    res->size != 0x1000 ||
	    (res->flags & (IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_RESERVE)) !=
		(IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_RESERVE))
		native88_topology_refusal(__LINE__);
	/* The coreboot resource is a reservation, not proof of a DF route.
	 * First access qualifies the retained fixed-device path; no DF slot is added.
	 */
	printk(BIOS_INFO, "NATIVE_GNB FIRST MMIO PROBE fec01000 IOREGSEL then ID/version; "
	       "DF posted window excludes aperture; fixed-device decode unproven\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	write32p(GNB_IO_APIC_ADDR, 0);
	id = read32p(GNB_IO_APIC_ADDR + 0x10);
	write32p(GNB_IO_APIC_ADDR, 1);
	version = read32p(GNB_IO_APIC_ADDR + 0x10);
	last = (version >> 16) & 0xff;
	printk(BIOS_INFO, "NATIVE_GNB RESOURCE base=%llx size=%llx flags=%lx "
	       "idraw=%08x version=%08x last=%u\n",
	       (unsigned long long)res->base, (unsigned long long)res->size,
	       res->flags, id, version, last);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (id == 0xffffffff || version == 0xffffffff ||
	    !(version & 0xff) || (version & 0xff) == 0xff || last < 15 || last > 127)
		native88_topology_refusal(__LINE__);
	amd_pci_domain_init(domain);
	if (get_ioapic_id(GNB_IO_APIC_ADDR) != GNB_IOAPIC_ID)
		native88_topology_refusal(__LINE__);
	write32p(GNB_IO_APIC_ADDR, 1);
	if (read32p(GNB_IO_APIC_ADDR + 0x10) != version)
		native88_topology_refusal(__LINE__);
	for (uint32_t i = 0; i <= last; i++) {
		write32p(GNB_IO_APIC_ADDR, 0x10 + 2 * i);
		if (read32p(GNB_IO_APIC_ADDR + 0x10) != 0x10000)
			native88_topology_refusal(__LINE__);
		write32p(GNB_IO_APIC_ADDR, 0x11 + 2 * i);
		if (read32p(GNB_IO_APIC_ADDR + 0x10))
			native88_topology_refusal(__LINE__);
	}
	printk(BIOS_INFO, "NATIVE_GNB GENUINE INIT id=1 count=%u RTEs masked; "
	       "MADT will follow FCH24 with GSIbase24; delivery untested\n", last + 1);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

/* Real init_link initializes root children in order. Require the genuine CPU
 * cluster first, so genuine BSP metadata registration precedes domain IOAPIC,
 * LPC RTC/PIC/PIT/DMA/SERIRQ/HPET, and every PCI endpoint init callback.
 */
static void native88_require_bsp_first(void)
{
	const struct device *cpu;

	if (dev_root.ops != &default_dev_ops_root || dev_root.ops->init ||
	    !dev_root.downstream || dev_root.downstream != dev_root.upstream ||
	    dev_root.downstream->dev != &dev_root)
		native88_topology_refusal(__LINE__);
	cpu = dev_root.downstream->children;
	if (!cpu || cpu->path.type != DEVICE_PATH_CPU_CLUSTER || !cpu->enabled ||
	    cpu->initialized || cpu->ops != &amd_cpu_bus_ops || !cpu->ops->init ||
	    cpu->downstream || !cpu->sibling ||
	    cpu->sibling->path.type != DEVICE_PATH_DOMAIN ||
	    cpu->sibling->path.domain.domain_id != 0 || !cpu->sibling->enabled)
		native88_topology_refusal(__LINE__);
}

static void native88_resources_exit(void *unused)
{
	native84_stage_check();
	native88_require_bsp_first();
	print_resource_tree(&dev_root, BIOS_INFO, "NATIVE88 ASSIGNED");
	printk(BIOS_INFO, "NATIVE88 BS_DEV_RESOURCES RETURN; GENUINE DEV_ENABLE NEXT\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

/* Validate actual Cezanne USB functions after bus routing is rebuilt and
 * before BAR sizing/assignment. Absent functions stay normally disabled;
 * a wrong present identity is evidence of routing/selection trouble.
 */
static void native90_enumeration_exit(void *unused)
{
	struct device *const usb[2] = { DEV_PTR(xhci_0), DEV_PTR(xhci_1) };

	native84_stage_check();
	for (unsigned int i = 0; i < ARRAY_SIZE(usb); i++) {
		const struct device *dev = usb[i];

		if (!dev || dev->path.type != DEVICE_PATH_PCI || !dev->upstream ||
		    dev->upstream->dev != DEV_PTR(gpp_bridge_a) ||
		    dev->path.pci.devfn != PCI_DEVFN(0, 3 + i))
			native88_topology_refusal(__LINE__);
		if (!dev->vendor) {
			if (dev->enabled)
				native88_topology_refusal(__LINE__);
			printk(BIOS_INFO, "NATIVE90 XHCI%u ABSENT DISABLED\n", i);
			continue;
		}
		printk(BIOS_INFO, "NATIVE90 XHCI%u %s id=%04x:%04x class=%06x enabled=%u\n",
		       i, dev_path(dev), dev->vendor, dev->device, dev->class, dev->enabled);
		if (dev->vendor != 0x1022 || dev->device != 0x1639 ||
		    dev->class != 0x0c0330 || !dev->enabled)
			native88_topology_refusal(__LINE__);
		if (native84_profile_policy.entry_path == 2) {
			const uint16_t cap = pci_find_capability(dev, PCI_CAP_ID_PM);
			uint16_t before, after;

			if (i == 0) {
				const uint32_t usb_skip = smn_read32(0x03810a84U);

				printk(BIOS_INFO, "NATIVE114 USB SMN03810a84=%08x SkipController=%u SkipPHY=%u\n",
				       usb_skip, (usb_skip >> 9) & 1U, (usb_skip >> 10) & 1U);
				uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
			}
			if (cap < 0x40 || cap > 0xf8 || (cap & 3))
				native88_topology_refusal(__LINE__);
			before = pci_read_config16(dev, cap + PCI_PM_CTRL);
			printk(BIOS_INFO, "NATIVE114 XHCI%u PM cap=%02x BEFORE=%04x state=%u\n",
			       i, cap, before, before & PCI_PM_CTRL_STATE_MASK);
			uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
			if (before == 0xffff)
				native88_topology_refusal(__LINE__);
			if ((before & PCI_PM_CTRL_STATE_MASK) != PCI_PM_CTRL_POWER_STATE_D0) {
				/* Preserve PME_ENABLE and other bits; write zero to the
				 * PME_STATUS W1C bit so the pending event is not cleared.
				 */
				const uint16_t desired = before &
					~(PCI_PM_CTRL_STATE_MASK | PCI_PM_CTRL_PME_STATUS);

				pci_write_config16(dev, cap + PCI_PM_CTRL, desired);
				mdelay(10);
			}
			after = pci_read_config16(dev, cap + PCI_PM_CTRL);
			printk(BIOS_INFO, "NATIVE114 XHCI%u PM AFTER=%04x state=%u\n",
			       i, after, after & PCI_PM_CTRL_STATE_MASK);
			uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
			if ((after & PCI_PM_CTRL_STATE_MASK) != PCI_PM_CTRL_POWER_STATE_D0)
				native88_topology_refusal(__LINE__);
		}
	}
	native138_prepare_pci_resources();
	printk(BIOS_INFO, "NATIVE90 ENUM IDENTITIES CHECKED; RESOURCE ASSIGNMENT NEXT\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

/* Preserve the pre-x86 memory owners and use native ASPEED initialization
 * when graphics is requested. No additional SMM or option-ROM provider is used.
 */
_Static_assert((CONFIG(NO_GFX_INIT) || CONFIG(B565D4_NATIVE_GRAPHICS)) &&
	!CONFIG(VGA_ROM_RUN) &&
	!CONFIG(ON_DEVICE_ROM_LOAD) && !CONFIG(RUN_FSP_GOP) &&
	!CONFIG(SECURITY_CLEAR_DRAM_ON_REGULAR_BOOT) && !CONFIG(INTEL_TXT) &&
	(!CONFIG(DRIVERS_EFI_UPDATE_CAPSULES) ||
	 CONFIG(B565D4_EDK2_CAPSULE_FCH)), "Unsupported memory or graphics policy");
_Static_assert(CONFIG_DEFAULT_EBDA_LOWMEM == 0x100000 &&
	CONFIG_DEFAULT_EBDA_SEGMENT == 0xf600 && CONFIG_DEFAULT_EBDA_SIZE == 0x400 &&
	CONFIG_PC_CMOS_BASE_PORT_BANK0 == 0x70, "Selected BDA/EBDA/RTC geometry");

/* Only first witness from the separate low-memory draft: reads, no stores.
 * Static callback ordering must continue to put setup_default_ebda before this
 * trial's INIT entry callback. A mismatch is diagnostic, not a device-init veto.
 */
static unsigned int native89_lowmem_readback(void)
{
	const volatile uint8_t *bda = (const void *)(uintptr_t)0x400;
	const volatile uint8_t *ebda = (const void *)(uintptr_t)0xf6000;
	unsigned int status = 0;

	for (unsigned int i = 0; i < 0x200; i++) {
		uint8_t expected = 0;

		if (i == 0x0f)
			expected = 0xf6;
		if (i == 0x14)
			expected = 4;
		if (bda[i] != expected)
			status |= 1;
	}
	for (unsigned int i = 0; i < 0x400; i++) {
		if (ebda[i] != (i == 0 ? 1 : 0))
			status |= 2;
	}
	return status;
}

struct native89_pm_snapshot {
	uint32_t decode, policy;
	uint16_t event, control, timer, gpe;
};

static struct native89_pm_snapshot native89_pm;

/* Genuine indexed PMIO reads: bounded selector writes to CD6 and data reads
 * from CD7. No data writes, selector readback predicate, or restoration claim.
 */
static uint32_t native89_pm_read(uint8_t reg, unsigned int bytes)
{
	uint32_t result = 0;

	for (unsigned int i = 0; i < bytes; i++)
		result |= (uint32_t)pm_io_read8(reg + i) << (8 * i);
	return result;
}

static void native89_pm_admit(void)
{
	uint16_t pm1_status, pm1_enable, pm1_control;

	native89_pm.decode = native89_pm_read(0x04, 4);
	native89_pm.event = native89_pm_read(0x60, 2);
	native89_pm.control = native89_pm_read(0x62, 2);
	native89_pm.timer = native89_pm_read(0x64, 2);
	native89_pm.gpe = native89_pm_read(0x68, 2);
	native89_pm.policy = native89_pm_read(0x74, 4);
	printk(BIOS_INFO, "NATIVE90 PMIO decode04=%08x policy74=%08x "
	       "evt=%04x cnt=%04x tmr=%04x gpe=%04x\n", native89_pm.decode,
	       native89_pm.policy, native89_pm.event, native89_pm.control,
	       native89_pm.timer, native89_pm.gpe);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	/* The normal PEI cut may still use the coherent cold400 map; later
	 * vendor DXE/Linux uses800. Accept either observed tuple without
	 * programming the PM bases. A mixed tuple remains a refusal.
	 */
	if (!(native89_pm.decode & (1U << 1)) ||
	    !((native89_pm.event == 0x400 && native89_pm.control == 0x404 &&
	       native89_pm.timer == 0x408 && native89_pm.gpe == 0x420) ||
	      (native89_pm.event == 0x800 && native89_pm.control == 0x804 &&
	       native89_pm.timer == 0x808 && native89_pm.gpe == 0x820)))
		native88_topology_refusal(__LINE__);
	/* This diagnostic uses no fixed ACPIMMIO load. These are discovered
	 * ACPI port reads only; status is not acknowledged/cleared.
	 */
	pm1_status = inw(native89_pm.event);
	pm1_enable = inw(native89_pm.event + 2);
	pm1_control = inw(native89_pm.control);
	printk(BIOS_INFO, "NATIVE90 PM1 status=%04x enable=%04x control=%04x SCI_EN=%u\n",
	       pm1_status, pm1_enable, pm1_control, pm1_control & 1U);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

/* Root-selected retained PM change: global ACPI, RTC fixed-event and timer
 * enables at the already decoded map. Preserve other low bits and high24;
 * this does not enable RTC wake from sleep or replay FCH initialization.
 */
static void native93_enable_pm_global_rtc_timer(void)
{
	const uint32_t before = native89_pm_read(PM_ACPI_CONF, 4);
	const uint32_t expected = before | PM_ACPI_GLOBAL_EN | PM_ACPI_RTC_EN_EN |
		PM_ACPI_TIMER_EN_EN;
	uint32_t after;

	if (expected != before)
		pm_io_write8(PM_ACPI_CONF, (uint8_t)expected);
	after = native89_pm_read(PM_ACPI_CONF, 4);
	printk(BIOS_INFO, "NATIVE93 PM74 GLOBAL/RTC/TIMER %08x -> %08x expected=%08x\n",
	       before, after, expected);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (after != expected)
		native88_topology_refusal(__LINE__);
	native89_pm.policy = after;
}

/* Source113 local board policy trial: exact same-board FchShastaDxe
 * PCD7d == 0 sequence (RVA2da0). The enabled branch's packed PCI address
 * 00a6002c resolves to 00:14.6, not Hudson14.7 or nonPCI eMMC/AOAC28.
 * Run inside the existing BS_DEV_ENUMERATE entry callback after its fresh
 * ECAM guard; SoC init has already checked ACPIMMIO decode. A rejected byte
 * readback is logged, never retried or treated as a successful hide.
 */
static bool native170_sdhc_policy_ran;

static void native170_sdhc_byte_rmw(uintptr_t address, uint8_t keep)
{
	const uint8_t before = read8((void *)address);
	const uint8_t wanted = before & keep;
	uint8_t after;

	printk(BIOS_INFO, "NATIVE170 SDHC RMW BEGIN reg=%08x before=%02x "
	       "keep=%02x wanted=%02x\n", (unsigned int)address,
	       (unsigned int)before, (unsigned int)keep, (unsigned int)wanted);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	write8((void *)address, wanted);
	after = read8((void *)address);
	printk(BIOS_INFO, "NATIVE170 SDHC RMW END reg=%08x after=%02x "
	       "readback_match=%u non_ff_before_after=%u\n", (unsigned int)address,
	       (unsigned int)after, (unsigned int)(after == wanted),
	       (unsigned int)(before != UINT8_MAX && after != UINT8_MAX));
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

static void native170_sdhc_disable(void)
{
	uint32_t before, after;

	native84_stage_check();
	if (native84_profile_policy.entry_path != 2 || native170_sdhc_policy_ran)
		return;
	native170_sdhc_policy_ran = true;
	before = pci_s_read_config32(PCI_DEV(0, 0x14, 6), PCI_VENDOR_ID);
	printk(BIOS_INFO, "NATIVE170 SDHC POLICY PRE id=%08x; before PCI enumeration\n",
	       before);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (before != 0x79061022) {
		printk(BIOS_INFO, "NATIVE170 SDHC NO RMW unqualified_or_absent_id=%08x\n",
		       before);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		return;
	}
	/* Exactly three byte RMWs, including an already-clear bit; no retries.
	 * AOAC72 is index25 under 40+2*index, distinct from eMMC28 at78.
	 */
	native170_sdhc_byte_rmw(0xfed803d3U, 0xbf);
	native170_sdhc_byte_rmw(0xfed803e8U, 0xfe);
	native170_sdhc_byte_rmw(0xfed81e72U, 0xf7);
	after = pci_s_read_config32(PCI_DEV(0, 0x14, 6), PCI_VENDOR_ID);
	printk(BIOS_INFO, "NATIVE170 SDHC POLICY END id_before=%08x id_after=%08x "
	       "absent_observed=%u writes=3; no hide-cause claim\n", before, after,
	       (unsigned int)(after == UINT32_MAX));
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

/* One SDHC INTA route: retained entry1 requires the observed NC baseline;
 * native entry2 owns APIC17 without assuming PEI initialized its old value.
 * PIC17 and all other routes stay untouched; both old values are logged.
 * PIRQ_SD17 is the Cezanne selector; APIC mode selects97 at C00/C01.
 * Data16 decimal is GSI16 (10h), not the selector or a PCI line-register write.
 * Runtime interrupt delivery remains a later Linux/device acceptance test.
 */
static void native96_enable_sdhc_apic16(void)
{
	const struct device *const sd = pcidev_on_root(0x14, 6);
	uint32_t id, class;
	uint8_t pin, pic, apic, pic_after, apic_after;

	_Static_assert(PIRQ_SD == 0x17, "Selected Cezanne SD interrupt index");
	/* A measured hide must not fall into the old visible-device-only guard.
	 * Keep the original route path literally below whenever the ID remains.
	 */
	if (native84_profile_policy.entry_path == 2 && native170_sdhc_policy_ran) {
		id = pci_s_read_config32(PCI_DEV(0, 0x14, 6), PCI_VENDOR_ID);
		if (id == UINT32_MAX) {
			printk(BIOS_INFO, "NATIVE170 SDHC ROUTE SKIP id=%08x "
			       "software_present=%u; no SD IRQ write\n", id,
			       (unsigned int)(sd != NULL));
			uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
			return;
		}
	}
	if (!sd || !sd->enabled || sd->path.type != DEVICE_PATH_PCI ||
	    !sd->upstream || sd->upstream->secondary != 0 ||
	    sd->upstream->segment_group != 0 ||
	    sd->path.pci.devfn != PCI_DEVFN(0x14, 6))
		native88_topology_refusal(__LINE__);
	id = pci_read_config32(sd, PCI_VENDOR_ID);
	class = pci_read_config32(sd, PCI_CLASS_REVISION) >> 8;
	pin = pci_read_config8(sd, PCI_INTERRUPT_PIN);
	pic = read_pci_int_idx(PIRQ_SD, 0);
	apic = read_pci_int_idx(PIRQ_SD, 1);
	printk(BIOS_INFO, "NATIVE96 SDHC 00:14.6 id=%08x class=%06x pin=%u "
	       "FCH17/97=%02x/%02x\n", id, class, pin, pic, apic);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (id != 0x79061022 || class != 0x080501 || pin != 1 ||
	    (native84_profile_policy.entry_path != 2 &&
	     (pic != PIRQ_NC || apic != PIRQ_NC)))
		native88_topology_refusal(__LINE__);

	write_pci_int_idx(PIRQ_SD, 1, 16);
	pic_after = read_pci_int_idx(PIRQ_SD, 0);
	apic_after = read_pci_int_idx(PIRQ_SD, 1);
	printk(BIOS_INFO, "NATIVE96 SDHC FCH17/97 %02x/%02x -> %02x/%02x "
	       "root14 INTA GSI16\n", pic, apic, pic_after, apic_after);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (pic_after != pic || apic_after != 16)
		native88_topology_refusal(__LINE__);
}

static void native89_init_entry(void *unused)
{
	native84_stage_check();
	native88_require_bsp_first();
	printk(BIOS_INFO, "NATIVE89 BDA/EBDA readonly status=%u (diagnostic only)\n",
	       native89_lowmem_readback());
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	native89_pm_admit();
	native93_enable_pm_global_rtc_timer();
	native96_enable_sdhc_apic16();
	printk(BIOS_INFO, "NATIVE89 BS_DEV_INIT ENTRY; GENUINE BSP/DOMAIN/LPC INIT NEXT\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

/* One bounded raw RTC sample through genuine cmos_read: 70h selector writes
 * clear the NMI-disable bit, as in genuine cmos_init. No CMOS data writes and
 * no register C read (that would acknowledge pending RTC interrupts). UIP or
 * invalid-date data is reported, not promoted to a ticking-clock proof.
 */
static void native89_rtc_readback(void)
{
	static const uint8_t registers[6] = { 0, 2, 4, 7, 8, 9 };
	uint8_t raw[6], before, after, control, valid;

	before = cmos_read(RTC_REG_A);
	for (unsigned int i = 0; i < ARRAY_SIZE(registers); i++)
		raw[i] = cmos_read(registers[i]);
	control = cmos_read(RTC_REG_B);
	valid = cmos_read(RTC_REG_D);
	after = cmos_read(RTC_REG_A);
	printk(BIOS_INFO, "NATIVE89 RTC raw UIP=%u A=%02x/%02x B=%02x D=%02x "
	       "date=%02x-%02x-%02x time=%02x:%02x:%02x\n",
	       !!((before | after) & RTC_UIP), before, after, control, valid,
	       raw[5], raw[4], raw[3], raw[2], raw[1], raw[0]);
}

/* Original FCH initialization enables this IRQ input after PIC setup.
 * Root's same-image GRUB trial changed only PIC09 e7->f7 and passed the
 * previously failing Linux timer check. Preserve every other control bit.
 */
static void native123_enable_irq_input(void)
{
	uint8_t before;
	uint8_t after;

	before = read_pci_int_idx(PIRQ_MISC0, 0);
	if (!(before & BIT(4)))
		write_pci_int_idx(PIRQ_MISC0, 0, before | BIT(4));
	after = read_pci_int_idx(PIRQ_MISC0, 0);
	printk(BIOS_INFO, "NATIVE123 IRQ INPUT PIC09 before=%02x after=%02x\n",
	       before, after);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (after != (before | BIT(4)))
		native88_topology_refusal(__LINE__);
}

/* Visibility uses canonical DevFn indices, independently of physical remap
 * positions. Preserve every trained root, including newly populated slots.
 * Only the chipset link is mandatory; root12 is a removable GPP card.
 */
static void native124_hide_unused_ports(void)
{
	static const uint8_t expected_devfn[10] = {
		0x09, 0x0a, 0x0b, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17
	};
	static const uint32_t visibility_reg[10] = {
		0x13b31004, 0x13b31404, 0x13b31804, 0x13b33004, 0x13b33404,
		0x13b33804, 0x13b33c04, 0x13b34004, 0x13b34404, 0x13b34804
	};
	const uint32_t visibility_mask = 0x00040005;
	uint32_t visibility[10];
	uint16_t link[10] = { 0 };
	bool keep[10] = { false };
	uint16_t mapped_devfns = 0;
	unsigned int refusal_line = 0;

	printk(BIOS_INFO, "NATIVE276 ROOT PROFILE preserve_all_live=1 required=11; "
	       "ALL ROOT DIAGNOSTICS BEFORE HIDE\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	for (unsigned int i = 0; i < ARRAY_SIZE(expected_devfn); i++) {
		const uint32_t map = smn_read32(0x13b100b8 + 4 * i);
		unsigned int canonical;

		for (canonical = 0; canonical < ARRAY_SIZE(expected_devfn); canonical++)
			if ((map & 0xff) == expected_devfn[canonical])
				break;
		printk(BIOS_INFO, "NATIVE271 REMAP slot=%u map=%08x canonical=%u\n",
		       i, map, canonical);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if (canonical >= ARRAY_SIZE(expected_devfn) || (mapped_devfns & BIT(canonical))) {
			refusal_line = __LINE__;
			continue;
		}
		mapped_devfns |= BIT(canonical);
	}
	for (unsigned int i = 0; i < ARRAY_SIZE(expected_devfn); i++) {
		const pci_devfn_t bdf = (pci_devfn_t)expected_devfn[i] << 12;
		const uint32_t expected_id = i < 3 ? 0x16331022 : 0x16341022;
		const uint32_t id = pci_s_read_config32(bdf, PCI_VENDOR_ID);
		const bool required = expected_devfn[i] == 0x11;
		uint32_t link_cap;
		uint16_t cap;
		uint16_t flags;

		visibility[i] = smn_read32(visibility_reg[i]);
		printk(BIOS_INFO, "NATIVE271 ROOT%02x id=%08x required=%u reg=%08x vis=%08x\n",
		       expected_devfn[i], id, required, visibility_reg[i], visibility[i]);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if (!required && id == 0xffffffff &&
		    (visibility[i] & visibility_mask) == visibility_mask) {
			printk(BIOS_INFO, "NATIVE124 SLOT%u ALREADY HIDDEN\n", i);
			uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
			continue;
		}
		if (id != expected_id) {
			if (!refusal_line)
				refusal_line = __LINE__;
			continue;
		}
		cap = pci_s_find_capability(bdf, PCI_CAP_ID_PCIE);
		if (cap != 0x58) {
			if (!refusal_line)
				refusal_line = __LINE__;
			continue;
		}
		flags = pci_s_read_config16(bdf, cap + PCI_EXP_FLAGS);
		link_cap = pci_s_read_config32(bdf, cap + PCI_EXP_LNKCAP);
		link[i] = pci_s_read_config16(bdf, cap + PCI_EXP_LNKSTA);
		keep[i] = !!(link[i] & PCI_EXP_LNKSTA_DLLLA);
		printk(BIOS_INFO, "NATIVE124 ROOT%02x cap=%02x flags=%04x lnkcap=%08x "
		       "link=%04x width=%u active=%u training=%u\n", expected_devfn[i],
		       cap, flags, link_cap, link[i], (link[i] >> 4) & 0x3f,
		       !!(link[i] & PCI_EXP_LNKSTA_DLLLA), !!(link[i] & PCI_EXP_LNKSTA_LT));
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if ((((flags & PCI_EXP_FLAGS_TYPE) >> 4) != PCI_EXP_TYPE_ROOT_PORT ||
		     !(link_cap & PCI_EXP_LNKCAP_DLLLARC)) && !refusal_line)
			refusal_line = __LINE__;
		if (required && (!keep[i] || ((link[i] >> 4) & 0x3f) != 4))
			refusal_line = __LINE__;
		if (keep[i] && (!((link[i] >> 4) & 0x3f) ||
		    ((link[i] >> 4) & 0x3f) > ((link_cap >> 4) & 0x3f)))
			refusal_line = __LINE__;
	}
	if (refusal_line)
		native88_topology_refusal(refusal_line);
	for (unsigned int i = 0; i < ARRAY_SIZE(expected_devfn); i++) {
		const pci_devfn_t bdf = (pci_devfn_t)expected_devfn[i] << 12;
		uint32_t wanted;
		uint32_t after;
		uint32_t id;

		if (keep[i])
			continue;
		if ((visibility[i] & visibility_mask) != visibility_mask &&
		    (pci_s_read_config16(bdf, 0x58 + PCI_EXP_LNKSTA) & PCI_EXP_LNKSTA_DLLLA)) {
			keep[i] = true;
			continue;
		}
		wanted = (visibility[i] & ~visibility_mask) | visibility_mask;
		printk(BIOS_INFO, "NATIVE124 HIDE BEGIN slot=%u reg=%08x wanted=%08x\n",
		       i, visibility_reg[i], wanted);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		smn_write32(visibility_reg[i], wanted);
		after = smn_read32(visibility_reg[i]);
		id = pci_s_read_config32(bdf, PCI_VENDOR_ID);
		printk(BIOS_INFO, "NATIVE124 HIDE END slot=%u after=%08x id=%08x\n",
		       i, after, id);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if (after != wanted || id != 0xffffffff)
			native88_topology_refusal(__LINE__);
	}
	for (unsigned int i = 0; i < ARRAY_SIZE(expected_devfn); i++) {
		const pci_devfn_t bdf = (pci_devfn_t)expected_devfn[i] << 12;
		const uint32_t expected_id = i < 3 ? 0x16331022 : 0x16341022;
		uint32_t after;
		uint32_t id;
		uint16_t status;

		if (!keep[i])
			continue;
		after = smn_read32(visibility_reg[i]);
		id = pci_s_read_config32(bdf, PCI_VENDOR_ID);
		status = pci_s_read_config16(bdf, 0x58 + PCI_EXP_LNKSTA);
		printk(BIOS_INFO, "NATIVE124 KEEP slot=%u vis=%08x id=%08x link=%04x\n",
		       i, after, id, status);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if (after != visibility[i] || id != expected_id || !(status & PCI_EXP_LNKSTA_DLLLA))
			native88_topology_refusal(__LINE__);
	}
	printk(BIOS_INFO, "NATIVE124 UNUSED ROOTS HIDDEN; ACTIVE ROOTS PRESERVED\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

/* Source66: Root-selected Cezanne function policy, after PT19/root hiding.
 * Original NBIF D1F2/D1F3 disable leaves SATA D1F0/F1 alone. The seven GPU RMWs
 * are the final disabled-property footprint plus the later function enable:
 * keep function0 as a multifunction 1022:145a dummy, including USB siblings.
 * This is not a replay of the unrelated NBIO table or audio-disable pulse.
 */
static uint32_t native125_policy_id(const struct device *dev,
				    const struct device *bridge, uint8_t function)
{
	if (!dev || dev->path.type != DEVICE_PATH_PCI || !dev->upstream ||
	    dev->upstream->dev != bridge || dev->upstream->segment_group != 0 ||
	    !dev->upstream->secondary || dev->path.pci.devfn != PCI_DEVFN(0, function))
		native88_topology_refusal(__LINE__);
	return pci_read_config32(dev, PCI_VENDOR_ID);
}

static void native125_soc_function_policy(void)
{
	static const struct {
		uint32_t address, clear, set;
	} policy[] = {
		{ 0x10131000, 0x80000000, 0x80000000 },
		{ 0x10134000, 0x1000ffff, 0x1000145a },
		{ 0x10134034, 0x00ffffff, 0x00130000 },
		{ 0x10134010, 0x7ff00000, 0x00000000 },
		{ 0x1013400c, 0x02140000, 0x02000000 },
		{ 0x1013100c, 0x00000006, 0x00000000 },
		{ 0x10134008, 0x18070100, 0x00000000 },
		{ 0x1013a008, 0x00000c00, 0x00000000 },
		{ 0x10135400, 0x10000000, 0x00000000 },
		{ 0x10135600, 0x10000000, 0x00000000 },
		{ 0x10135000, 0x00000000, 0x00000000 },
		{ 0x10135200, 0x00000000, 0x00000000 },
		{ 0x10134200, 0x00000000, 0x00000000 },
	};
	static const uint8_t write_order[] = { 7, 8, 9, 0, 1, 2, 3, 4, 5, 6 };
	const struct device *const a = DEV_PTR(gpp_bridge_a);
	const struct device *const b = DEV_PTR(gpp_bridge_b);
	const struct device *const gpu = DEV_PTR(gfx);
	const struct device *const xgbe = DEV_PTR(xgbe_0);
	const struct device *const xgbe1 = DEV_PTR(xgbe_1);
	const struct device *const siblings[] = {
		DEV_PTR(xhci_0), DEV_PTR(xhci_1), DEV_PTR(sata_0),
		DEV_PTR(sata_1), DEV_PTR(gfx_hda),
	};
	const struct device *const parents[] = { a, a, b, b, a };
	static const uint8_t functions[] = { 3, 4, 0, 1, 1 };
	static const uint32_t sibling_ids[] = {
		0x16391022, 0x16391022, 0x79011022, 0x79011022, 0x16371002,
	};
	const uint32_t signature = cpuid_eax(1);
	uint32_t before[ARRAY_SIZE(policy)];
	uint32_t gpu_id, gpu_class, xgbe_id, xgbe1_id, fuse;
	uint8_t gpu_header;
	bool dummy, retained_real;

	if (native84_profile_policy.entry_path != 2 || !b565d4_is_cezanne(signature) ||
	    !a || !b || a->path.type != DEVICE_PATH_PCI ||
	    b->path.type != DEVICE_PATH_PCI || !a->enabled || !b->enabled ||
	    !a->upstream || !b->upstream || a->upstream->secondary != 0 ||
	    b->upstream->secondary != 0 || a->upstream->segment_group != 0 ||
	    b->upstream->segment_group != 0 ||
	    a->path.pci.devfn != PCI_DEVFN(8, 1) ||
	    b->path.pci.devfn != PCI_DEVFN(8, 2))
		native88_topology_refusal(__LINE__);
	fuse = smn_read32(0x0005d448);
	printk(BIOS_INFO, "NATIVE125 SOC POLICY cpuid=%08x fuse5d448=%08x\n",
	       signature, fuse);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (fuse == 0xffffffff || !(fuse & BIT(0)))
		native88_topology_refusal(__LINE__);

	/* Complete every identity/strap snapshot before either policy writes. */
	gpu_id = native125_policy_id(gpu, a, 0);
	xgbe_id = native125_policy_id(xgbe, b, 2);
	xgbe1_id = native125_policy_id(xgbe1, b, 3);
	gpu_class = pci_read_config32(gpu, PCI_CLASS_REVISION);
	gpu_header = pci_read_config8(gpu, PCI_HEADER_TYPE);
	printk(BIOS_INFO, "NATIVE125 GPU id=%08x classrev=%08x header=%02x "
	       "XGBE0/1 id=%08x/%08x\n", gpu_id, gpu_class, gpu_header,
	       xgbe_id, xgbe1_id);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (!(gpu_header & 0x80) || (gpu_header & 0x7f) != PCI_HEADER_TYPE_NORMAL)
		native88_topology_refusal(__LINE__);
	for (unsigned int i = 0; i < ARRAY_SIZE(siblings); i++) {
		const uint32_t id = native125_policy_id(siblings[i], parents[i], functions[i]);

		printk(BIOS_INFO, "NATIVE125 KEEP %s id=%08x\n", dev_path(siblings[i]), id);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if (id != sibling_ids[i])
			native88_topology_refusal(__LINE__);
	}
	for (unsigned int i = 0; i < ARRAY_SIZE(policy); i++) {
		before[i] = smn_read32(policy[i].address);
		printk(BIOS_INFO, "NATIVE125 SNAPSHOT reg=%08x value=%08x\n",
		       policy[i].address, before[i]);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	}
	dummy = gpu_id == 0x145a1022 && gpu_class == 0x130000c9;
	retained_real = gpu_id == 0x16381002 && gpu_class == 0x130000c9;
	if (dummy) {
		for (unsigned int i = 0; i < 7; i++) {
			if ((before[i] & policy[i].clear) != policy[i].set)
				native88_topology_refusal(__LINE__);
		}
	} else if (retained_real) {
		/* Exact mixed strap footprint observed on warm entry. Apply the
		 * normal RMW policy and keep the final dummy-ID guard below.
		 */
		static const uint32_t retained_real_bits[] = {
			0x80000000, 0x10001638, 0x00130000, 0, 0, 0, 0,
		};

		for (unsigned int i = 0; i < ARRAY_SIZE(retained_real_bits); i++) {
			if ((before[i] & policy[i].clear) != retained_real_bits[i])
				native88_topology_refusal(__LINE__);
		}
	} else if (gpu_id != 0x16381002 || gpu_class != 0x030000c9 ||
		   (before[0] & BIT(31)) || (before[1] & 0x1000ffff) != 0x10001638 ||
		   (before[2] & 0x00ffffff) != 0x00030000 || (before[4] & BIT(25))) {
		native88_topology_refusal(__LINE__);
	}
	if ((before[8] & 0xffff) != 0x1641 ||
	    !((xgbe_id == 0x16411022 && (before[8] & BIT(28))) ||
	      (xgbe_id == 0xffffffff && !(before[8] & BIT(28)) &&
	       !(before[7] & BIT(10)))) ||
	    (before[9] & 0xffff) != 0x1641 ||
	    !((xgbe1_id == 0x16411022 && (before[9] & BIT(28))) ||
	      (xgbe1_id == 0xffffffff && !(before[9] & BIT(28)) &&
	       !(before[7] & BIT(11)))) ||
	    (before[10] & 0x1000ffff) != 0x10007901 ||
	    (before[11] & 0x1000ffff) != 0x10007901 ||
	    (before[12] & 0x1000ffff) != 0x10001637)
		native88_topology_refusal(__LINE__);

	for (unsigned int n = 0; n < ARRAY_SIZE(write_order); n++) {
		const unsigned int i = write_order[n];
		const uint32_t expected = (before[i] & ~policy[i].clear) | policy[i].set;
		uint32_t after;

		printk(BIOS_INFO, "NATIVE125 RMW BEGIN reg=%08x before=%08x wanted=%08x\n",
		       policy[i].address, before[i], expected);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if (expected != before[i])
			smn_write32(policy[i].address, expected);
		after = smn_read32(policy[i].address);
		printk(BIOS_INFO, "NATIVE125 RMW END reg=%08x after=%08x\n",
		       policy[i].address, after);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if (after != expected)
			native88_topology_refusal(__LINE__);
	}
	for (unsigned int i = 0; i < ARRAY_SIZE(policy); i++) {
		const uint32_t expected = (before[i] & ~policy[i].clear) | policy[i].set;
		const uint32_t after = smn_read32(policy[i].address);

		printk(BIOS_INFO, "NATIVE125 FINAL reg=%08x value=%08x\n",
		       policy[i].address, after);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if (after != expected)
			native88_topology_refusal(__LINE__);
	}
	gpu_id = native125_policy_id(gpu, a, 0);
	gpu_class = pci_read_config32(gpu, PCI_CLASS_REVISION);
	xgbe_id = native125_policy_id(xgbe, b, 2);
	xgbe1_id = native125_policy_id(xgbe1, b, 3);
	printk(BIOS_INFO, "NATIVE125 FINAL GPU id=%08x classrev=%08x header=%02x "
	       "XGBE0/1 id=%08x/%08x\n", gpu_id, gpu_class,
	       pci_read_config8(gpu, PCI_HEADER_TYPE), xgbe_id, xgbe1_id);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (gpu_id != 0x145a1022 || gpu_class != 0x130000c9 ||
	    pci_read_config8(gpu, PCI_HEADER_TYPE) != gpu_header ||
	    xgbe_id != 0xffffffff || xgbe1_id != 0xffffffff)
		native88_topology_refusal(__LINE__);
	for (unsigned int i = 0; i < ARRAY_SIZE(siblings); i++) {
		const uint32_t id = native125_policy_id(siblings[i], parents[i], functions[i]);

		printk(BIOS_INFO, "NATIVE125 FINAL KEEP %s id=%08x\n", dev_path(siblings[i]), id);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if (id != sibling_ids[i])
			native88_topology_refusal(__LINE__);
	}
	printk(BIOS_INFO, "NATIVE125 GPU DUMMY/BOTH XGBE HIDDEN; USB/SATA/AUDIO PRESERVED\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

/* Original BaseRnPei's unconditional class-table entry changes only ACP's
 * subclass03->80. Accept our retained class on warm entry after the exact
 * resource preflight and genuine allocation; do not initialize ACP MMIO.
 */
static void native139_acp_class_policy(void)
{
	const struct device *const bridge = DEV_PTR(gpp_bridge_a);
	struct device *const acp = DEV_PTR(acp);
	const struct resource *bar, *window;
	const unsigned long assigned = IORESOURCE_MEM | IORESOURCE_ASSIGNED |
		IORESOURCE_STORED;
	const uint32_t signature = cpuid_eax(1);
	uint32_t id, class_revision, bar_raw, bar_type, bridge_raw;
	uint32_t bridge_base, bridge_limit, before, wanted, after, class_after;
	resource_t bar_end, window_end;
	uint8_t header;
	static bool attempted;

	if (native84_profile_policy.entry_path != 2)
		return;
	printk(BIOS_INFO, "NATIVE139 ACP CLASS PREFLIGHT cpuid=%08x\n", signature);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (attempted || !b565d4_is_cezanne(signature) || !bridge ||
	    bridge->path.type != DEVICE_PATH_PCI || !bridge->enabled ||
	    !bridge->upstream || bridge->upstream->secondary != 0 ||
	    bridge->upstream->segment_group != 0 ||
	    bridge->path.pci.devfn != PCI_DEVFN(8, 1) || !bridge->downstream ||
	    bridge->downstream->dev != bridge || !bridge->downstream->secondary ||
	    pci_read_config32(bridge, PCI_VENDOR_ID) != 0x16351022 ||
	    (pci_read_config32(bridge, PCI_CLASS_REVISION) >> 8) != 0x060400 ||
	    (pci_read_config8(bridge, PCI_HEADER_TYPE) & 0x7f) != PCI_HEADER_TYPE_BRIDGE)
		native88_topology_refusal(__LINE__);
	id = native125_policy_id(acp, bridge, 5);
	class_revision = pci_read_config32(acp, PCI_CLASS_REVISION);
	header = pci_read_config8(acp, PCI_HEADER_TYPE);
	printk(BIOS_INFO, "NATIVE139 ACP CLASS %s id=%08x classrev=%08x header=%02x\n",
	       dev_path(acp), id, class_revision, header);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (!acp->enabled || acp->hidden || acp->downstream ||
	    acp->upstream != bridge->downstream ||
	    acp->ops != &native138_resource_only_ops || id != 0x15e21022 ||
	    acp->vendor != 0x1022 || acp->device != 0x15e2 ||
	    (class_revision != 0x04030001 && class_revision != 0x04800001) ||
	    acp->class != (class_revision >> 8) ||
	    header != 0x80 || acp->hdr_type != header)
		native88_topology_refusal(__LINE__);

	bar = probe_resource(acp, PCI_BASE_ADDRESS_0);
	window = probe_resource(bridge, PCI_MEMORY_BASE);
	if (!bar || !window || (bar->flags & (IORESOURCE_TYPE_MASK | assigned)) != assigned ||
	    (window->flags & (IORESOURCE_TYPE_MASK | assigned | IORESOURCE_BRIDGE)) !=
	    (assigned | IORESOURCE_BRIDGE) ||
	    !bar->base || bar->size < 0x1424 || bar->base > 0xffffffffULL ||
	    bar->size > 0x100000000ULL - bar->base || !window->size ||
	    window->base > 0xffffffffULL ||
	    window->size > 0x100000000ULL - window->base)
		native88_topology_refusal(__LINE__);
	bar_end = bar->base + bar->size - 1;
	window_end = window->base + window->size - 1;
	bar_raw = pci_read_config32(acp, PCI_BASE_ADDRESS_0);
	bar_type = bar_raw & PCI_BASE_ADDRESS_MEM_LIMIT_MASK;
	bridge_raw = pci_read_config32(bridge, PCI_MEMORY_BASE);
	bridge_base = (bridge_raw & 0x0000fff0U) << 16;
	bridge_limit = (bridge_raw & 0xfff00000U) | 0x000fffffU;
	if ((bar_raw & PCI_BASE_ADDRESS_SPACE_IO) ||
	    (bar_type != PCI_BASE_ADDRESS_MEM_LIMIT_32 &&
	     bar_type != PCI_BASE_ADDRESS_MEM_LIMIT_64) ||
	    !!(bar->flags & IORESOURCE_PCI64) != (bar_type == PCI_BASE_ADDRESS_MEM_LIMIT_64) ||
	    (bar_type == PCI_BASE_ADDRESS_MEM_LIMIT_64 &&
	     pci_read_config32(acp, PCI_BASE_ADDRESS_1) != 0) ||
	    (bar_raw & ~PCI_BASE_ADDRESS_MEM_ATTR_MASK) != bar->base ||
	    bar->base < window->base || bar_end > window_end ||
	    bridge_base > bridge_limit || bar->base < bridge_base || bar_end > bridge_limit ||
	    !(pci_read_config16(bridge, PCI_COMMAND) & PCI_COMMAND_MEMORY) ||
	    !(pci_read_config16(acp, PCI_COMMAND) & PCI_COMMAND_MEMORY))
		native88_topology_refusal(__LINE__);
	printk(BIOS_INFO, "NATIVE139 ACP BAR=%08llx size=%08llx bridge=%08x..%08x "
	       "SMN READ BEGIN reg=10134a34\n",
	       (unsigned long long)bar->base, (unsigned long long)bar->size,
	       bridge_base, bridge_limit);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	before = smn_read32(0x10134a34);
	printk(BIOS_INFO, "NATIVE139 ACP CLASS SMN before=%08x\n", before);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if ((before & 0x00ffffffU) != (class_revision >> 8))
		native88_topology_refusal(__LINE__);
	wanted = (before & ~0x0000ff00U) | 0x00008000U;
	attempted = true;
	printk(BIOS_INFO, "NATIVE139 ACP CLASS RMW BEGIN reg=10134a34 "
	       "before=%08x wanted=%08x\n", before, wanted);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (before != wanted)
		smn_write32(0x10134a34, wanted);
	after = smn_read32(0x10134a34);
	class_after = pci_read_config32(acp, PCI_CLASS_REVISION);
	printk(BIOS_INFO, "NATIVE139 ACP CLASS RMW END reg=10134a34 "
	       "after=%08x classrev=%08x\n", after, class_after);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (after != wanted || class_after != 0x04800001)
		native88_topology_refusal(__LINE__);
	acp->class = class_after >> 8;
}

/* Same fixed ACPIMMIO provider as the qualified BIOSRAM4 RO helper.
 * Read before DXIO, after chip-init decode admission and BSP-only PSP166.
 * cpu_info/%gs is not installed yet; PSP166 already checked APIC_BASE.BSP.
 * A retained record is not proof of a fresh writer or a unique recovery cause.
 */
static void native179_biosram_ro(void)
{
	static bool sampled;
	uint8_t control;
	uint8_t bytes[4];
	uint32_t value, psp_raw;

	if (!native166_psp_prepared || sampled)
		return;
	sampled = true;
	printk(BIOS_INFO, "NATIVE202 PRE_DXIO_PSP_SMN BEGIN index=03810570 "
	       "provider=existing_ECAM BSP_witness=PSP166\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	psp_raw = smn_read32(0x03810570);
	printk(BIOS_INFO, "NATIVE202 PRE_DXIO_PSP_SMN RESULT raw=%08x "
	       "valid_nonff=%u ready31=%u recovery30=%u reads=1 "
	       "B8_selector_writes=1 BC_writes=0 CF8_accesses=0 "
	       "PSP_commands=0 diagnostic_only=1\n",
	       psp_raw, psp_raw != UINT32_MAX, !!(psp_raw & BIT(31)),
	       !!(psp_raw & BIT(30)));
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	printk(BIOS_INFO, "NATIVE179 BIOSRAM_RO BEGIN phase=preDXIO "
	       "BSP_witness=PSP166 PM04=fed80304 record=fed80578+4 "
	       "widths=8 writes=0\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	control = read8p(0xfed80304);
	printk(BIOS_INFO, "NATIVE179 BIOSRAM_RO PM04=%02x\n", control);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (control == 0xff || !(control & BIT(1))) {
		printk(BIOS_INFO, "NATIVE179 BIOSRAM_RO SKIP decode_unqualified "
		       "control_reads=1 record_reads=0 writes=0\n");
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		return;
	}
	for (unsigned int i = 0; i < ARRAY_SIZE(bytes); i++)
		bytes[i] = read8p(0xfed80578 + i);
	value = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
		((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
	printk(BIOS_INFO, "NATIVE179 BIOSRAM_RO RETURN bytes=%02x%02x%02x%02x "
	       "LE=%08x control_reads=1 record_reads=4 writes=0 freshness_claim=0\n",
	       bytes[0], bytes[1], bytes[2], bytes[3], value);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

/* Native251: the optional OS-facing PSP capability query is unnecessary
 * for the operator-selected CCP-hidden profile. Native250's earlier, complete
 * pre-x86 firmware cohort restored microcode 0xa500011 on all twelve CPUs,
 * but its mailbox value 0xc0000000 fails the later diagnostic Ready check.
 * Preserve pre-x86 PSP/ABL, owned apertures and all memory/resource checks;
 * issue no capability command and make no claim of complete PSP shutdown.
 */
static void native214_psp_caps_once(void)
{
	native84_stage_check();
	if (native84_profile_policy.entry_path != 2)
		return;
	printk(BIOS_INFO, "NATIVE251 PSP_CAPS OMIT optional_query=1 commands=0 "
	       "CCP_hidden_profile=1 pre_x86_PSP_preserved=1\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

static void native89_init_exit(void *unused)
{
	uint32_t first = 0, last = 0, delta = 0;
	unsigned int samples = 0;

	native84_stage_check();
	if (native84_profile_policy.entry_path == 2) {
		native123_enable_irq_input();
		native42_promontory_init();
		b565d4_promontory_uplink_init();
		native124_hide_unused_ports();
		native125_soc_function_policy();
		native139_acp_class_policy();
	}
	native89_rtc_readback();
	if (native89_pm.policy & PM_ACPI_TIMER_EN_EN) {
		first = inl(native89_pm.timer) & 0x00ffffff;
		for (samples = 0; samples < 4096; ) {
			samples++;
			last = inl(native89_pm.timer) & 0x00ffffff;
			delta = (last - first) & 0x00ffffff;
			if (delta)
				break;
		}
	}
	printk(BIOS_INFO, "NATIVE89 PM TIMER enabled=%u first=%06x last=%06x "
	       "delta=%u samples=%u\n", !!(native89_pm.policy & PM_ACPI_TIMER_EN_EN),
	       first, last, delta, samples);
	/* Restore the proven BSP-only placement before tables/payload. This
	 * repair is not reached through a SELF-copy mismatch or an MP retry.
	 */
	native91_log_visible_fixed_mtrrs();
	native91_repair_legacy_shadow();
	native214_psp_caps_once();
	printk(BIOS_INFO, "NATIVE98 SHADOW PREPARED; DEV_INIT EXIT RETURN\n");
	printk(BIOS_INFO, "NATIVE98 BSP ONLY, RTC/SDHC16/PAYLOAD FLOW NEXT\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

/* Source107: quarantine only the unsupported PCI crypto function after all
 * device/table/payload-load work. The owned nonPCI PSP aperture and genuine
 * AP PSPADDR sync remain intact; no PSP command or SMM transition is added.
 * Same-board EPF2 temporary clear/restore proved visibility, not this timing.
 */
#define NATIVE167_CCP_STRAP 0x10134400
#define NATIVE167_CCP_FUNC_EN (1U << 28)
#define NATIVE167_CCP_STRAP_BEFORE 0x100015df

static void __maybe_unused native167_ccp_quarantine(void *unused)
{
	static bool attempted;
	const struct device *const parent = DEV_PTR(gpp_bridge_a);
	const struct device *const crypto = DEV_PTR(crypto);
	const struct device *const peers[] = {
		DEV_PTR(gfx), DEV_PTR(gfx_hda), DEV_PTR(xhci_0),
		DEV_PTR(xhci_1), DEV_PTR(acp), DEV_PTR(hda),
	};
	static const uint8_t functions[] = { 0, 1, 3, 4, 5, 6 };
	static const uint32_t ids[] = {
		0x145a1022, 0x16371002, 0x16391022,
		0x16391022, 0x15e21022, 0x15e31022,
	};
	const struct non_pci_mmio_reg *regs;
	uint64_t windows[8];
	struct { uint32_t base, limit, control; } df[DF_MMIO_REG_SET_COUNT];
	msr_t apic, mmconf, psp;
	size_t count;
	uint32_t before, after, id, irq, parent_after;

	native84_stage_check();
	if (native84_profile_policy.entry_path != 2)
		return;
	/* This exact parent profile has no SMM handler or PSP SMI callbacks. */
	_Static_assert(CONFIG(NO_SMM), "Native167 selected non-SMM profile");
	_Static_assert(ARRAY_SIZE(peers) == ARRAY_SIZE(functions) &&
		ARRAY_SIZE(peers) == ARRAY_SIZE(ids), "Native167 peer identity set");
	if (attempted || !native166_psp_prepared || !native_gnb_ioapic_prepared ||
	    !parent || !crypto || crypto->hidden || !parent->downstream ||
	    parent->path.type != DEVICE_PATH_PCI || !parent->upstream ||
	    parent->upstream->secondary || parent->upstream->segment_group ||
	    parent->path.pci.devfn != PCI_DEVFN(8, 1) ||
	    pci_read_config32(parent, PCI_VENDOR_ID) != 0x16351022 ||
	    native125_policy_id(crypto, parent, 2) != 0x15df1022)
		native88_topology_refusal(__LINE__);
	apic = rdmsr(IA32_APIC_BASE_MSR_INDEX);
	mmconf = rdmsr(MMIO_CONF_BASE);
	psp = rdmsr(PSP_ADDR_MSR);
	if ((apic.lo & (BIT(8) | BIT(11))) != (BIT(8) | BIT(11)) ||
	    mmconf.raw != 0xf000001dULL || psp.raw != NATIVE166_PSP_BASE)
		native88_topology_refusal(__LINE__);
	for (size_t i = 0; i < ARRAY_SIZE(peers); i++) {
		id = native125_policy_id(peers[i], parent, functions[i]);
		printk(BIOS_INFO, "NATIVE167 CCP PEER PRE fn=%u id=%08x\n", functions[i], id);
		if (id != ids[i])
			native88_topology_refusal(__LINE__);
	}
	regs = get_iohc_non_pci_mmio_regs(&count);
	if (!regs || count != ARRAY_SIZE(windows))
		native88_topology_refusal(__LINE__);
	for (size_t i = 0; i < count; i++) {
		windows[i] = smn_read64(SMN_IOHC_MISC_BASE_13B1 + regs[i].iohc_misc_offset);
		if (windows[i] == UINT64_MAX ||
		    (regs[i].iohc_misc_offset == 0x2e0 && windows[i] != NATIVE166_PSP_VALUE))
			native88_topology_refusal(__LINE__);
	}
	for (unsigned int i = 0; i < DF_MMIO_REG_SET_COUNT; i++) {
		df[i].base = data_fabric_broadcast_read32(DF_MMIO_BASE(i));
		df[i].limit = data_fabric_broadcast_read32(DF_MMIO_LIMIT(i));
		df[i].control = data_fabric_broadcast_read32(DF_MMIO_CONTROL(i));
		if (df[i].control == UINT32_MAX)
			native88_topology_refusal(__LINE__);
	}
	irq = smn_read32(NATIVE_NBIF_INT_LINE_ENABLE);
	before = smn_read32(NATIVE167_CCP_STRAP);
	printk(BIOS_INFO, "NATIVE167 CCP QUARANTINE PRE strap=%08x parent=%08x "
	       "target=%08x software_hidden=%u APIC=%016llx MMCONF=%016llx PSP=%016llx\n",
	       before, pci_read_config32(parent, PCI_VENDOR_ID),
	       native125_policy_id(crypto, parent, 2), (unsigned int)crypto->hidden,
	       (unsigned long long)apic.raw, (unsigned long long)mmconf.raw,
	       (unsigned long long)psp.raw);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (before != NATIVE167_CCP_STRAP_BEFORE ||
	    smn_read32(NATIVE167_CCP_STRAP) != before)
		native88_topology_refusal(__LINE__);
	attempted = true;
	/* The only new hardware data store: preserve every bit except FUNC_EN. */
	smn_write32(NATIVE167_CCP_STRAP, before & ~NATIVE167_CCP_FUNC_EN);
	after = smn_read32(NATIVE167_CCP_STRAP);
	id = native125_policy_id(crypto, parent, 2);
	parent_after = pci_read_config32(parent, PCI_VENDOR_ID);
	printk(BIOS_INFO, "NATIVE167 CCP HIDDEN strap=%08x wanted=%08x id=%08x parent=%08x\n",
	       after, before & ~NATIVE167_CCP_FUNC_EN, id, parent_after);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (after != (before & ~NATIVE167_CCP_FUNC_EN) || id != UINT32_MAX ||
	    parent_after != 0x16351022)
		native88_topology_refusal(__LINE__);
	for (size_t i = 0; i < ARRAY_SIZE(peers); i++) {
		id = native125_policy_id(peers[i], parent, functions[i]);
		printk(BIOS_INFO, "NATIVE167 CCP PEER POST fn=%u id=%08x\n", functions[i], id);
		if (id != ids[i])
			native88_topology_refusal(__LINE__);
	}
	for (size_t i = 0; i < count; i++) {
		const uint64_t value = smn_read64(SMN_IOHC_MISC_BASE_13B1 + regs[i].iohc_misc_offset);

		printk(BIOS_INFO, "NATIVE167 CCP NONPCI reg=%08x before=%016llx after=%016llx\n",
		       SMN_IOHC_MISC_BASE_13B1 + regs[i].iohc_misc_offset,
		       (unsigned long long)windows[i], (unsigned long long)value);
		if (value != windows[i])
			native88_topology_refusal(__LINE__);
	}
	for (unsigned int i = 0; i < DF_MMIO_REG_SET_COUNT; i++) {
		if (data_fabric_broadcast_read32(DF_MMIO_BASE(i)) != df[i].base ||
		    data_fabric_broadcast_read32(DF_MMIO_LIMIT(i)) != df[i].limit ||
		    data_fabric_broadcast_read32(DF_MMIO_CONTROL(i)) != df[i].control)
			native88_topology_refusal(__LINE__);
	}
	if (rdmsr(IA32_APIC_BASE_MSR_INDEX).raw != apic.raw ||
	    rdmsr(MMIO_CONF_BASE).raw != mmconf.raw || rdmsr(PSP_ADDR_MSR).raw != psp.raw ||
	    smn_read32(NATIVE_NBIF_INT_LINE_ENABLE) != irq ||
	    smn_read32(NATIVE167_CCP_STRAP) != after ||
	    native125_policy_id(crypto, parent, 2) != UINT32_MAX)
		native88_topology_refusal(__LINE__);
	printk(BIOS_INFO, "NATIVE167 CCP QUARANTINE PASS target_absent=1 peers=6 parent=1 "
	       "nonPCI=8 DF=8 BSP_MSRS_unchanged=1 IRQword_unchanged=1 "
	       "BAR_writes=0 PSP_commands=0; PAYLOAD NEXT\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

BOOT_STATE_INIT_ENTRY(BS_DEV_ENUMERATE, BS_ON_ENTRY, native87_enumeration_entry, NULL);
BOOT_STATE_INIT_ENTRY(BS_DEV_ENUMERATE, BS_ON_EXIT, native90_enumeration_exit, NULL);
BOOT_STATE_INIT_ENTRY(BS_DEV_RESOURCES, BS_ON_EXIT, native88_resources_exit, NULL);
BOOT_STATE_INIT_ENTRY(BS_DEV_INIT, BS_ON_ENTRY, native89_init_entry, NULL);
BOOT_STATE_INIT_ENTRY(BS_DEV_INIT, BS_ON_EXIT, native89_init_exit, NULL);
/* Native248: restore the same-board proven late PCI function hiding on
 * the qualified Native222 32-GB boot path; early PSP/ABL remains unchanged.
 */
BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_BOOT, BS_ON_ENTRY, native167_ccp_quarantine, NULL);
#endif

static const char *soc_acpi_name(const struct device *dev)
{
	if (dev->path.type == DEVICE_PATH_DOMAIN)
		return "PCI0";

	if (dev->path.type != DEVICE_PATH_PCI)
		return NULL;

	printk(BIOS_WARNING, "Unknown PCI device: dev: %d, fn: %d\n",
	       PCI_SLOT(dev->path.pci.devfn), PCI_FUNC(dev->path.pci.devfn));
	return NULL;
};

static void board_pci_domain_fill_ssdt(const struct device *dev)
{
	amd_pci_domain_fill_ssdt(dev);
	if (CONFIG(B565D4_BOARD_SETUP))
		b565d4_setup_acpi_wake(dev);
}

struct device_operations cezanne_pci_domain_ops = {
	.read_resources	= amd_pci_domain_read_resources,
	.set_resources	= pci_domain_set_resources,
	.scan_bus	= amd_pci_domain_scan_bus,
#if CONFIG(SOC_AMD_CEZANNE_NATIVE)
	.init		= native_gnb_ioapic_domain_init,
#else
	.init		= amd_pci_domain_init,
#endif
	.acpi_name	= soc_acpi_name,
	.acpi_fill_ssdt	= board_pci_domain_fill_ssdt,
};

static void soc_init(void *chip_info)
{
	default_dev_ops_root.write_acpi_tables = soc_acpi_write_tables;

#if CONFIG(SOC_AMD_CEZANNE_NATIVE)
	native84_stage_check();
	if (native84_profile_policy.entry_path == 2) {
		if (!(pm_io_read8(ACPIMMIO_DECODE_REGISTER_04) &
		      PM_04_ACPIMMIO_DECODE_EN))
			native88_topology_refusal(__LINE__);
		fch_init_acpi_ports();
	}
	printk(BIOS_INFO, "NATIVE88 BS_DEV_INIT_CHIPS RETURN\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	return;
#else
	amd_fsp_silicon_init();

	data_fabric_print_mmio_conf();

	fch_init(chip_info);
#endif
}

static void soc_final(void *chip_info)
{
#if CONFIG(SOC_AMD_CEZANNE_NATIVE)
	native84_stage_check();
	printk(BIOS_INFO, "NATIVE91 SOC FINAL RETURN; NO COLD FCH FINALIZATION\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
#else
	fch_final(chip_info);
#endif
}

struct chip_operations soc_amd_cezanne_ops = {
#if CONFIG(SOC_AMD_CEZANNE)
	.name = "AMD Cezanne SoC",
#elif CONFIG(SOC_AMD_RENOIR)
	.name = "AMD Renoir SoC",
#elif CONFIG(SOC_AMD_V2000A)
	.name = "AMD V2000A SoC",
#endif
	.init = soc_init,
	.final = soc_final
};
