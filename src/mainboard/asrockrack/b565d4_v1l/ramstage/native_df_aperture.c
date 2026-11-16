/* SPDX-License-Identifier: GPL-2.0-only */
#include "b565d4_cpu.h"
#include <commonlib/bsd/compiler.h>
#include "native_df_aperture.h"
#include "native84_profile.h"
#include "native_apob_resources.h"
#include <amdblocks/data_fabric.h>
#include <arch/cpu.h>
#include <console/console.h>
#include <console/uart.h>
#include <cpu/amd/mtrr.h>
#include <cpu/x86/msr.h>
#include <device/pci_def.h>

#define APERTURE_BEGIN 0xc0000000ULL
#define APERTURE_END   0xf0000000ULL

struct native_df_route {
	uint32_t base, limit, control;
};

_Static_assert(DF_MMIO_REG_SET_COUNT == 8 && DF_MMIO_REG_SET_SIZE == 4 &&
	DF_MMIO_SHIFT == 16 && IOMS0_FABRIC_ID == 10 &&
	sizeof(union df_mmio_control) == 4, "Selected Cezanne DF register ABI");
_Static_assert((APERTURE_BEGIN & 0xffff) == 0 &&
	(APERTURE_END & 0xffff) == 0 && APERTURE_BEGIN < APERTURE_END,
	"Whole 64KiB-aligned posted aperture");

/* Serialized BSP operation; no large automatic snapshot or borrowed storage. */
static struct native_df_route before[DF_MMIO_REG_SET_COUNT];
static unsigned int attempted;

static void refuse(const char *why, unsigned int line) __noreturn;
static void refuse(const char *why, unsigned int line)
{
	printk(BIOS_ERR, "NATIVE111 DF REFUSE line=%u %s\n", line, why);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	native84_terminal();
}

static struct native_df_route read_route(unsigned int index)
{
	return (struct native_df_route) {
		.base = data_fabric_broadcast_read32(DF_MMIO_BASE(index)),
		.limit = data_fabric_broadcast_read32(DF_MMIO_LIMIT(index)),
		.control = data_fabric_broadcast_read32(DF_MMIO_CONTROL(index)),
	};
}

static int equal_route(const struct native_df_route *a, const struct native_df_route *b)
{
	return a->base == b->base && a->limit == b->limit && a->control == b->control;
}

static int equal_msr(msr_t a, msr_t b)
{
	return a.lo == b.lo && a.hi == b.hi;
}

/* Read every route, including active non-posted/fixed and disabled records.
 * A failed check after stores is terminal; no register rollback is attempted.
 */
static int readback(const char *phase, unsigned int selected,
	const struct native_df_route *replacement)
{
	int matched = 1;

	for (unsigned int i = 0; i < DF_MMIO_REG_SET_COUNT; i++) {
		struct native_df_route current = read_route(i);
		const struct native_df_route *expected = i == selected ? replacement : &before[i];

		printk(BIOS_INFO, "NATIVE111 DF %s slot=%u base=%08x limit=%08x ctrl=%08x\n",
		       phase, i, current.base, current.limit, current.control);
		if (!equal_route(&current, expected))
			matched = 0;
	}
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	return matched;
}

void native107_df_posted_aperture(void)
{
	msr_t top, top2;
	struct native_df_route staged;
	union df_mmio_control enable = { .raw = 0 };
	union df_mmio_control disabled_ioms0 = { .raw = 0 };
	enum native_apob_status status;
	unsigned int selected = DF_MMIO_REG_SET_COUNT;

	native84_stage_check();
	if (native84_profile_policy.entry_path != 2)
		return;
	if (attempted)
		refuse("second attempt", __LINE__);
	attempted = 1;
	disabled_ioms0.dst_fabric_id = IOMS0_FABRIC_ID;
	top = rdmsr(TOP_MEM_MSR);
	top2 = rdmsr(TOP_MEM2_MSR);
	printk(BIOS_INFO, "NATIVE111 DF TOP_MEM=%08x:%08x TOP_MEM2=%08x:%08x\n",
	       top.hi, top.lo, top2.hi, top2.lo);
	for (unsigned int i = 0; i < DF_MMIO_REG_SET_COUNT; i++) {
		before[i] = read_route(i);
		printk(BIOS_INFO, "NATIVE111 DF before slot=%u base=%08x limit=%08x ctrl=%08x\n",
		       i, before[i].base, before[i].limit, before[i].control);
	}
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (top.hi || !top.lo || (top.lo & 0xffff) || top.lo > APERTURE_BEGIN)
		refuse("TOP_MEM intersects aperture or is unknown", __LINE__);
	status = native_apob_require_pci_hole(APERTURE_BEGIN, APERTURE_END);
	printk(BIOS_INFO, "NATIVE111 DF current APOB raw1 aperture status=%u\n", status);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (status != NATIVE_APOB_OK)
		refuse("aperture not wholly in current raw1 PCI hole", __LINE__);
	for (unsigned int i = 0; i < DF_MMIO_REG_SET_COUNT; i++) {
		const struct native_df_route *r = &before[i];
		union df_mmio_control control = { .raw = r->control };

		/* Base/limit encode address bits47..16. In particular, limitffffffff
		 * may validly end a fixed/NP route at the exclusive48-bit boundary.
		 */
		if (r->control == 0xffffffffU)
			refuse("unreadable or unknown route control", __LINE__);
		if (control.re || control.we) {
			uint64_t begin = (uint64_t)r->base << DF_MMIO_SHIFT;
			uint64_t end = ((uint64_t)r->limit + 1) << DF_MMIO_SHIFT;

			if (begin >= end)
				refuse("invalid active route", __LINE__);
			if (begin < APERTURE_END && APERTURE_BEGIN < end)
				refuse("active route overlaps selected aperture", __LINE__);
		} else if (!r->base && !r->limit &&
			   (r->control == 0 || r->control == disabled_ioms0.raw) &&
			   selected == DF_MMIO_REG_SET_COUNT) {
			/* Admit only zero-address slots with zero or known disabled IOMS0
			 * control. Other preprogrammed or unknown records remain untouched.
			 */
			selected = i;
		}
	}
	if (selected == DF_MMIO_REG_SET_COUNT)
		refuse("no eligible zero-address disabled route", __LINE__);
	if (!readback("prewrite", DF_MMIO_REG_SET_COUNT, NULL) ||
	    !equal_msr(rdmsr(TOP_MEM_MSR), top) ||
	    !equal_msr(rdmsr(TOP_MEM2_MSR), top2) ||
	    native_apob_require_pci_hole(APERTURE_BEGIN, APERTURE_END) != NATIVE_APOB_OK)
		refuse("route, TOM or owned APOB changed before stores", __LINE__);

	staged = (struct native_df_route) {
		.base = APERTURE_BEGIN >> DF_MMIO_SHIFT,
		.limit = (APERTURE_END >> DF_MMIO_SHIFT) - 1,
		.control = before[selected].control,
	};
	/* Preserve the admitted disabled control. Keep RE/WE off until both address
	 * words and every other route have been read back unchanged.
	 */
	data_fabric_broadcast_write32(DF_MMIO_BASE(selected), staged.base);
	data_fabric_broadcast_write32(DF_MMIO_LIMIT(selected), staged.limit);
	if (!readback("staged-disabled", selected, &staged) ||
	    !equal_msr(rdmsr(TOP_MEM_MSR), top) ||
	    !equal_msr(rdmsr(TOP_MEM2_MSR), top2))
		refuse("disabled-stage readback failed", __LINE__);
	enable.raw = staged.control;
	enable.re = 1;
	enable.we = 1;
	enable.dst_fabric_id = IOMS0_FABRIC_ID;
	enable.np = 0;
	staged.control = enable.raw;
	data_fabric_broadcast_write32(DF_MMIO_CONTROL(selected), staged.control);
	if (!readback("enabled", selected, &staged) ||
	    !equal_msr(rdmsr(TOP_MEM_MSR), top) ||
	    !equal_msr(rdmsr(TOP_MEM2_MSR), top2))
		refuse("enabled-route or preserved-state readback failed", __LINE__);
	printk(BIOS_INFO, "NATIVE111 DF POSTED slot=%u [c0000000,f0000000) dst=%u ctrl=%08x\n",
	       selected, (unsigned int)IOMS0_FABRIC_ID, staged.control);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

struct native_df_io_route {
	uint32_t base, limit;
};

_Static_assert(DF_IO_REG_COUNT == 8 && DF_IO_ADDR_SHIFT == 12 &&
	DF_DEV == 0x18 && DF_IO_BASE(0) == DF_REG_ID(0, 0xc0) &&
	DF_IO_LIMIT(0) == DF_REG_ID(0, 0xc4) && IOMS0_FABRIC_ID == 10 &&
	sizeof(union df_io_base) == 4 && sizeof(union df_io_limit) == 4,
	"Selected Cezanne DF I/O register ABI");

static struct native_df_io_route io_before[DF_IO_REG_COUNT];
static unsigned int io_attempted;

static struct native_df_io_route read_io_route(unsigned int index)
{
	return (struct native_df_io_route) {
		.base = data_fabric_broadcast_read32(DF_IO_BASE(index)),
		.limit = data_fabric_broadcast_read32(DF_IO_LIMIT(index)),
	};
}

static int io_readback(const char *phase, const struct native_df_io_route *slot0)
{
	int matched = 1;

	for (unsigned int i = 0; i < DF_IO_REG_COUNT; i++) {
		struct native_df_io_route current = read_io_route(i);
		const struct native_df_io_route *expected = i ? &io_before[i] : slot0;

		printk(BIOS_INFO, "NATIVE137 DF IO %s slot=%u base=%08x limit=%08x\n",
		       phase, i, current.base, current.limit);
		if (current.base != expected->base || current.limit != expected->limit)
			matched = 0;
	}
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	return matched;
}

/* Once before genuine DF resource discovery. Only the freshly observed unused
 * slot0 is selected; the other seven routes and all ports below1000 stay as-is.
 */
void native137_df_io_aperture(void)
{
	const struct native_df_io_route staged = { .base = 0x1000, .limit = 0xf00a };
	const struct native_df_io_route enabled = { .base = 0x1003, .limit = 0xf00a };
	uint32_t signature, identity;
	int admitted = 1;

	native84_stage_check();
	if (native84_profile_policy.entry_path != 2)
		return;
	if (io_attempted || !attempted)
		refuse("NATIVE137 IO repeated or before MMIO preparation", __LINE__);
	io_attempted = 1;
	signature = cpuid_eax(1);
	identity = pci_read_config32(SOC_DF_F0_DEV, PCI_VENDOR_ID);
	printk(BIOS_INFO, "NATIVE137 DF IO PREFLIGHT cpuid=%08x DF00:18.0=%08x\n",
	       signature, identity);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (!b565d4_is_cezanne(signature) || identity != 0x166a1022)
		refuse("NATIVE137 IO selected CPU/DF identity", __LINE__);

	/* One complete fresh sixteen-DWORD snapshot, before any I/O-route store. */
	for (unsigned int i = 0; i < DF_IO_REG_COUNT; i++) {
		io_before[i] = read_io_route(i);
		printk(BIOS_INFO, "NATIVE137 DF IO before slot=%u base=%08x limit=%08x\n",
		       i, io_before[i].base, io_before[i].limit);
		if (io_before[i].base != 0 || io_before[i].limit != 0x0a)
			admitted = 0;
	}
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (!admitted)
		refuse("NATIVE137 IO route snapshot differs from selected empty map", __LINE__);

	/* Address bits24..12 encode1000..ffff, destination10. Keep RE/WE off
	 * while staging both words; IE remains0 throughout this ordinary route.
	 */
	data_fabric_broadcast_write32(DF_IO_BASE(0), staged.base);
	data_fabric_broadcast_write32(DF_IO_LIMIT(0), staged.limit);
	if (!io_readback("staged-disabled", &staged))
		refuse("NATIVE137 IO staged route readback", __LINE__);
	data_fabric_broadcast_write32(DF_IO_BASE(0), enabled.base);
	if (!io_readback("enabled", &enabled))
		refuse("NATIVE137 IO enabled route readback", __LINE__);
	printk(BIOS_INFO, "NATIVE137 DF IO ENABLED [1000,10000) dst=10 base=00001003 limit=0000f00a\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}
