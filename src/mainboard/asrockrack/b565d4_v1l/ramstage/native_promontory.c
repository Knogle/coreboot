/* SPDX-License-Identifier: GPL-2.0-only */
#include "b565d4_cpu.h"
#include <commonlib/bsd/compiler.h>
#include "native_promontory.h"
#include "native84_profile.h"
#include "native_apob_resources.h"
#include <amdblocks/data_fabric.h>
#include <arch/cpuid.h>
#include <cbfs.h>
#include <cbmem.h>
#include <console/console.h>
#include <console/uart.h>
#include <cpu/x86/tsc.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_ops.h>
#include <device/resource.h>
#include <static.h>
#include <stdint.h>

#define PT19_BYTES 0x1c00cU
#define PT19_PAYLOAD_BYTES 0x1c000U
#define PT19_CHECKSUM 0x00b891dcU
#define PT19_CBMEM_ID 0x50543139U
#define PT19_FILE "native42/promontory19.bin"
#define PT19_POLL_LIMIT 100000U
#define PT19_TOTAL_USECS 60000000U
#define PT19_FIFO 0x3010U
#define PT19_BUSY 0x3009U

_Static_assert(sizeof(uintptr_t) == 4 && PT19_BYTES == 12 + PT19_PAYLOAD_BYTES,
	"Selected IA32 PT19 FIFO trial");
_Static_assert(DF_MMIO_SHIFT == 16 && DF_MMIO_REG_SET_COUNT == 8 &&
	IOMS0_FABRIC_ID == 10, "Selected Cezanne posted route ABI");

struct pt19_context {
	const struct device *dev;
	uintptr_t mmio;
	uint64_t resource_end;
	uint64_t first_ticks, last_ticks, tick_budget;
	uint32_t stores;
	uint16_t command;
	unsigned int command_owned;
};

static unsigned int attempted;

static int restore_command(struct pt19_context *c)
{
	if (!c->command_owned)
		return 1;
	if (pci_read_config32(c->dev, PCI_VENDOR_ID) != 0x43ef1022U)
		return 0;
	pci_write_config16(c->dev, PCI_COMMAND, c->command);
	if (pci_read_config16(c->dev, PCI_COMMAND) != c->command)
		return 0;
	c->command_owned = 0;
	return 1;
}

static void refuse(struct pt19_context *c, const char *why, unsigned int line)
	__noreturn;
static void refuse(struct pt19_context *c, const char *why, unsigned int line)
{
	const int restored = restore_command(c);

	printk(BIOS_ERR, "NATIVE42 PT19 REFUSE line=%u %s stores=%u command_restored=%u "
	       "partial_effects_retained=1 no_retry=1\n", line, why, c->stores, restored);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	/* No further indirect writes after a failed poll, no BAR/bus cleanup,
	 * and no claim that restoring Command undoes controller/firmware state.
	 */
	native84_terminal();
}
#define REQUIRE(c, cond, why) do { if (!(cond)) refuse(c, why, __LINE__); } while (0)

static int budget(struct pt19_context *c)
{
	const uint64_t now = rdtscll();

	if (now < c->last_ticks || now - c->first_ticks >= c->tick_budget)
		return 0;
	c->last_ticks = now;
	return 1;
}

static uint8_t mmio8(struct pt19_context *c, unsigned int offset)
{
	return *(volatile uint8_t *)(c->mmio + offset);
}

static int wait_byte(struct pt19_context *c, unsigned int offset,
	uint8_t mask, uint8_t value)
{
	for (unsigned int n = 0; n < PT19_POLL_LIMIT; n++) {
		if (!budget(c))
			return 0;
		if ((mmio8(c, offset) & mask) == value)
			return 1;
	}
	return 0;
}

/* Original 09d327b9 echoes index/data writes and polls busy bit7 after each.
 * Every MMIO address is inside the renewed assigned BAR0 extent.
 */
static int write_byte(struct pt19_context *c, unsigned int offset, uint8_t value)
{
	if (!budget(c))
		return 0;
	*(volatile uint8_t *)(c->mmio + offset) = value;
	return wait_byte(c, offset, 0xff, value) && wait_byte(c, PT19_BUSY, 0x80, 0);
}

static int select_index(struct pt19_context *c, uint16_t index)
{
	return write_byte(c, 0x3000, index & 0xff) &&
		write_byte(c, 0x3001, index >> 8);
}

static int indirect_write(struct pt19_context *c, uint16_t index, uint8_t value)
{
	return select_index(c, index) && write_byte(c, 0x3004, value);
}

/* Original 09d32747 returns indirect read data from BAR+3008, whereas
 * FIFO completion uses BAR+3004. Keep the existing bounded index transport.
 */
static int indirect_read(struct pt19_context *c, uint16_t index, uint8_t *value)
{
	if (!select_index(c, index) || !budget(c))
		return 0;
	*value = mmio8(c, 0x3008);
	return 1;
}

static int policy_unlock(struct pt19_context *c, uint16_t first)
{
	return indirect_write(c, first, 0x78) &&
		indirect_write(c, first + 1, 0x56) &&
		indirect_write(c, first + 2, 0x34) &&
		indirect_write(c, first + 3, 0x12);
}

/* Reduced OEM pre-FIFO policy, not the complete Promontory PEIM.
 * Original defaults, historical first PCD HOB and PT GUID-HOB agree:
 * policy[4c..55]=ten 01 bytes, [e2..eb]=ten 00 bytes, [7]=00, [8]=0f.
 * Original 09d31a5a..09d31aa1 calls 09d30a22 then 09d30729 BEFORE upload.
 */
static void port_policy(struct pt19_context *c)
{
	const uint32_t signature = cpuid_eax(0x80000001U);
	uint8_t c51c, c51f, mode0151, mask_low, mask_high;

	/* The selected A50F00 takes Original's clear-bit0 branch at09d30f84.
	 * Bind that branch explicitly before this function's first MMIO write.
	 */
	REQUIRE(c, b565d4_is_cezanne(signature), "selected Cezanne port policy");
	printk(BIOS_INFO, "NATIVE49 PT19 OEM PRE-FIFO policy BEGIN cpuid=%08x\n",
	       signature);
	REQUIRE(c, policy_unlock(c, 0xa520) &&
		indirect_write(c, 0xb522, 0xfe) && indirect_write(c, 0xb523, 0x7f),
		"OEM 43ef port mask7ffe");
	REQUIRE(c, policy_unlock(c, 0xa51c) && indirect_read(c, 0xc51c, &c51c),
		"OEM C51C policy read");
	REQUIRE(c, indirect_write(c, 0xc51c, c51c & 0xfe),
		"OEM Cezanne C51C clear bit0");
	/* +7==0: do not set bit4. +8==0f is NOT equal to1: no C51D access. */
	REQUIRE(c, policy_unlock(c, 0xa520) && indirect_write(c, 0xc51e, 0) &&
		indirect_read(c, 0xc51f, &c51f) && indirect_write(c, 0xc51f, c51f),
		"OEM C51E zero/C51F OR00");
	/* Exact instruction09d30823 is immediate10151: host low16 index0151. */
	REQUIRE(c, indirect_read(c, 0x0151, &mode0151), "OEM 0151 policy read");
	if (!(mode0151 & 4))
		REQUIRE(c, indirect_write(c, 0x0151, mode0151 | 4),
			"OEM 0151 set bit2");
	REQUIRE(c, indirect_read(c, 0x041a, &mask_low) &&
		indirect_read(c, 0x041b, &mask_high), "OEM 041A/B observation");
	printk(BIOS_INFO, "NATIVE49 PT19 OEM PRE-FIFO policy COMPLETE "
	       "mask=7ffe C51C=%02x->%02x C51E=00 C51F=%02x->%02x "
	       "0151=%02x->%02x 041A/B=%04x temporary43ea_path=excluded\n",
	       c51c, c51c & 0xfe, c51f, c51f, mode0151, mode0151 | 4,
	       mask_low | (unsigned int)mask_high << 8);
	/* Do not clear041A/B or copy the unselected temporary43ea PCI94/9a
	 * enumeration branch at09d3089e..09d30a18. No Gen1/ASPM/BAR teardown.
	 */
}

static int wait_status_clear(struct pt19_context *c)
{
	for (unsigned int n = 0; n < PT19_POLL_LIMIT; n++) {
		if (!budget(c))
			return 0;
		if (!(*(volatile uint32_t *)(c->mmio + 0x20) & 2))
			return 1;
	}
	return 0;
}

static int wait_counter(struct pt19_context *c, uint16_t prior, int equal)
{
	for (unsigned int n = 0; n < PT19_POLL_LIMIT; n++) {
		uint16_t value;

		if (!budget(c))
			return 0;
		value = pci_read_config16(c->dev, 0xe2);

		if (value != 0xffff && !(value & 1) && (value == prior) == equal)
			return 1;
	}
	return 0;
}

static uint16_t le16(const uint8_t *p)
{
	return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}

static uint32_t le32(const uint8_t *p)
{
	return (uint32_t)le16(p) | (uint32_t)le16(p + 2) << 16;
}

static int below_root(const struct device *dev, const struct device *root)
{
	for (unsigned int n = 0; n < 16 && dev && dev->upstream; n++) {
		dev = dev->upstream->dev;
		if (dev == root)
			return 1;
	}
	return 0;
}

static const struct device *find_endpoint(struct pt19_context *c)
{
	const struct device *const root = DEV_PTR(gpp_bridge_0);
	const struct device *found = NULL;
	unsigned int count = 0;

	REQUIRE(c, root && root->enabled && root->path.type == DEVICE_PATH_PCI &&
		root->path.pci.devfn == PCI_DEVFN(2, 1) && root->upstream &&
		!root->upstream->secondary && !root->upstream->segment_group &&
		pci_read_config32(root, PCI_VENDOR_ID) == 0x16341022U,
		"current Cezanne root11 identity");
	for (const struct device *dev = all_devices; dev; dev = dev->next) {
		/* Bound the real linked device list as well as ancestry traversal. */
		REQUIRE(c, count++ < 1024, "device list bound");
		if (dev->path.type != DEVICE_PATH_PCI || !dev->enabled ||
		    dev->vendor != 0x1022 || dev->device != 0x43ef ||
		    !below_root(dev, root))
			continue;
		REQUIRE(c, !found, "multiple43ef endpoints below root11");
		found = dev;
	}
	REQUIRE(c, found && found->initialized && found->upstream &&
		!found->upstream->segment_group && found->upstream->secondary <= 127 &&
		found->class == 0x0c0330 &&
		pci_read_config32(found, PCI_VENDOR_ID) == 0x43ef1022U &&
		(pci_read_config32(found, PCI_CLASS_REVISION) >> 8) == 0x0c0330 &&
		(pci_read_config8(found, PCI_HEADER_TYPE) & 0x7f) == PCI_HEADER_TYPE_NORMAL,
		"unique initialized43ef xHCI identity/class/header");
	return found;
}

static void check_route(struct pt19_context *c, uint64_t begin, uint64_t end)
{
	const struct device *const root = DEV_PTR(gpp_bridge_0);
	const struct device *child = c->dev;
	unsigned int matches = 0;

	for (unsigned int depth = 0; ; depth++) {
		const struct device *bridge;
		uint32_t buses, window;
		uint64_t base, limit;

		REQUIRE(c, depth < 16 && child->upstream &&
			!child->upstream->segment_group, "assigned PCI route depth/segment");
		bridge = child->upstream->dev;
		REQUIRE(c, bridge && bridge->enabled && bridge->path.type == DEVICE_PATH_PCI &&
			(pci_read_config8(bridge, PCI_HEADER_TYPE) & 0x7f) == PCI_HEADER_TYPE_BRIDGE &&
			(pci_read_config16(bridge, PCI_COMMAND) &
			 (PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER)) ==
			(PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER),
			"generic Type1 route already owns MEM/MASTER");
		buses = pci_read_config32(bridge, PCI_PRIMARY_BUS);
		window = pci_read_config32(bridge, PCI_MEMORY_BASE);
		base = (uint64_t)(window & 0xfff0) << 16;
		limit = ((uint64_t)(window & 0xfff00000) | 0xfffff) + 1;
		REQUIRE(c, bridge->upstream && (buses & 0xff) == bridge->upstream->secondary &&
			((buses >> 8) & 0xff) == child->upstream->secondary &&
			((buses >> 16) & 0xff) >= child->upstream->secondary &&
			base <= begin && end <= limit, "assigned bus numbers/MEM window");
		if (bridge == root)
			break;
		child = bridge;
	}
	/* Query the complete 64-KiB APOB cover of the admitted BAR extent. */
	REQUIRE(c, native_apob_require_pci_hole(begin & ~0xffffULL,
		(end + 0xffffULL) & ~0xffffULL) == NATIVE_APOB_OK,
		"current positive APOB PCI hole");
	for (unsigned int i = 0; i < DF_MMIO_REG_SET_COUNT; i++) {
		union df_mmio_control control = {
			.raw = data_fabric_broadcast_read32(DF_MMIO_CONTROL(i)),
		};
		const uint64_t base = (uint64_t)data_fabric_broadcast_read32(
			DF_MMIO_BASE(i)) << DF_MMIO_SHIFT;
		const uint64_t limit = ((uint64_t)data_fabric_broadcast_read32(
			DF_MMIO_LIMIT(i)) + 1) << DF_MMIO_SHIFT;

		REQUIRE(c, control.raw != 0xffffffffU, "DF control readable");
		if (!control.re && !control.we)
			continue;
		REQUIRE(c, base < limit, "active DF span");
		if (base >= end || begin >= limit)
			continue;
		REQUIRE(c, control.re && control.we &&
			!control.np && control.dst_fabric_id == IOMS0_FABRIC_ID &&
			base <= begin && end <= limit && !matches++, "unique posted DF route");
	}
	REQUIRE(c, matches == 1, "one current DF route covers assigned BAR");
}

static void admit_bar(struct pt19_context *c)
{
	const struct resource *res = probe_resource(c->dev, PCI_BASE_ADDRESS_0);
	const uint32_t bar = pci_read_config32(c->dev, PCI_BASE_ADDRESS_0);
	uint64_t actual = bar & ~(uint64_t)PCI_BASE_ADDRESS_MEM_ATTR_MASK;
	const uint32_t type = bar & PCI_BASE_ADDRESS_MEM_LIMIT_MASK;
	const uint32_t flags = IORESOURCE_MEM | IORESOURCE_ASSIGNED | IORESOURCE_STORED;

	REQUIRE(c, !(bar & PCI_BASE_ADDRESS_SPACE_IO) &&
		(type == PCI_BASE_ADDRESS_MEM_LIMIT_32 || type == PCI_BASE_ADDRESS_MEM_LIMIT_64),
		"BAR0 memory type");
	if (type == PCI_BASE_ADDRESS_MEM_LIMIT_64)
		actual |= (uint64_t)pci_read_config32(c->dev, PCI_BASE_ADDRESS_1) << 32;
	REQUIRE(c, res && (res->flags & flags) == flags &&
		!(res->flags & (IORESOURCE_PREFETCH | IORESOURCE_READONLY)) &&
		res->base == actual && actual >= 0xc0000000ULL && actual < 0xf0000000ULL &&
		res->size >= PT19_FIFO + 4 && res->size <= 0xf0000000ULL - actual &&
		!(actual & 0xff), "assigned BAR0 extent/identity");
	c->mmio = actual;
	c->resource_end = actual + res->size;
	check_route(c, actual, c->resource_end);
	REQUIRE(c, pci_read_config16(c->dev, PCI_COMMAND) & PCI_COMMAND_MEMORY,
		"generic DEV_ENABLE already owns MEM decode");
}

static void *firmware_allocator(void *arg, size_t size, const union cbfs_mdata *mdata)
{
	const struct cbmem_entry *entry;
	void *allocation;

	(void)arg;
	(void)mdata;
	if (size != PT19_BYTES || cbmem_entry_find(PT19_CBMEM_ID))
		return NULL;
	allocation = cbmem_add(PT19_CBMEM_ID, size);
	entry = cbmem_entry_find(PT19_CBMEM_ID);
	if (!allocation || !entry || cbmem_entry_start(entry) != allocation ||
	    cbmem_entry_size(entry) < size || (uintptr_t)allocation < NATIVE84_CBMEM_BEGIN ||
	    (uintptr_t)allocation >= NATIVE84_PARENT_END ||
	    cbmem_entry_size(entry) > (uint64_t)NATIVE84_PARENT_END - (uintptr_t)allocation)
		return NULL;
	return allocation;
}

static const uint8_t *load_firmware(struct pt19_context *c)
{
	size_t size = 0;
	const uint8_t *blob = cbfs_alloc_exact(PT19_FILE, firmware_allocator, NULL, &size);
	uint32_t sum = 0;

	REQUIRE(c, blob && size == PT19_BYTES && le32(blob) == 0x5f54505fU &&
		le32(blob + 4) == PT19_BYTES && le32(blob + 8) == PT19_CHECKSUM,
		"exact selected PT19 header/size");
	for (unsigned int i = 12; i < PT19_BYTES; i++)
		sum += blob[i];
	REQUIRE(c, sum == PT19_CHECKSUM, "Original PT19 byte-sum");
	/* The HOST-selected CBFS input must match the recorded whole SHA256.
	 * Header and additive checksum are structural checks, not authentication.
	 * Never free this owned source entry during this boot.
	 */
	return blob + 12;
}

static void upload(struct pt19_context *c, const uint8_t *payload)
{
	uint8_t completion = 0;
	uint16_t counter = 0;

	REQUIRE(c, indirect_write(c, 0x5040, 2) && indirect_write(c, 0x5042, 1) &&
		wait_status_clear(c) && indirect_write(c, 0x500e, 1), "upload setup");
	pci_write_config8(c->dev, 0xef, 0x80);
	REQUIRE(c, pci_read_config8(c->dev, 0xef) == 0x80, "EF upload mode readback");
	pci_write_config16(c->dev, 0xe2, 0);
	REQUIRE(c, wait_counter(c, 0, 1), "initial E2 zero readback");
	for (unsigned int pass = 0; pass < 2; pass++) {
		for (unsigned int i = 0; i < 0x4000; i++) {
			const uint32_t low = le16(payload + pass * 0x10000 + 2 * i);
			const uint32_t high = pass == 0 ? le16(payload + 0x8000 + 2 * i) :
				(i < 0x2000 ? le16(payload + 0x18000 + 2 * i) : 0);

			REQUIRE(c, budget(c), "FIFO total deadline");
			*(volatile uint32_t *)(c->mmio + PT19_FIFO) = low | high << 16;
			c->stores++;
			REQUIRE(c, wait_counter(c, counter, 0), "FIFO E2 progress");
			counter += 2;
			if (!(c->stores & 0xfff))
				printk(BIOS_INFO, "NATIVE42 PT19 FIFO stores=%u prior_counter=%04x\n",
				       c->stores, counter);
		}
		if (pass == 0) {
			pci_write_config16(c->dev, 0xe2, 0x8000);
			REQUIRE(c, wait_counter(c, 0x8000, 1), "E2 bank boundary readback");
		}
	}
	/* Original low-index byte is previous returned completion value. First
	 * selection is 0x5000, NOT 0x0050. Preserve this finite feedback sequence.
	 */
	for (unsigned int n = 0; n < 256 && completion != 3; n++) {
		REQUIRE(c, indirect_write(c, 0x5040, 3) &&
			select_index(c, 0x5000 | completion), "completion index/echo/busy");
		completion = mmio8(c, 0x3004);
	}
	REQUIRE(c, completion == 3 && c->stores == 32768, "bounded completion value3");
	pci_write_config8(c->dev, 0xef, 0);
	REQUIRE(c, pci_read_config8(c->dev, 0xef) == 0 &&
		indirect_write(c, 0x500e, 0) && wait_status_clear(c), "upload final controls");
}

void native42_promontory_init(void)
{
	struct pt19_context c = { 0 };
	const uint8_t *payload;
	unsigned long mhz;
	uint32_t marker;
	uint16_t cap, link;
	const struct resource *res;

	native84_stage_check();
	REQUIRE(&c, !attempted++ && native84_profile_policy.entry_path == 2,
		"once-only EARLY path2 BSP entry");
	c.dev = find_endpoint(&c);
	marker = pci_read_config32(c.dev, 0xf0);
	printk(BIOS_INFO, "NATIVE42 PT19 %s rev=%02x cmd=%04x F0=%08x\n", dev_path(c.dev),
	       pci_read_config8(c.dev, PCI_REVISION_ID),
	       pci_read_config16(c.dev, PCI_COMMAND), marker);
	res = probe_resource(c.dev, PCI_BASE_ADDRESS_0);
	printk(BIOS_INFO, "NATIVE42 PT19 BAR0=%08x BAR1=%08x resource=%u "
	       "base=%llx size=%llx flags=%lx\n",
	       pci_read_config32(c.dev, PCI_BASE_ADDRESS_0),
	       pci_read_config32(c.dev, PCI_BASE_ADDRESS_1), !!res,
	       res ? res->base : 0, res ? res->size : 0, res ? res->flags : 0);
	if (marker == 0x55aa55aaU) {
		printk(BIOS_INFO, "NATIVE42 PT19 F0 PRESENT; UPLOAD SKIPPED; USB unobserved\n");
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		return;
	}
	REQUIRE(&c, marker != 0xffffffffU, "F0 endpoint remains accessible");
	mhz = tsc_freq_mhz();
	REQUIRE(&c, mhz && mhz <= 100000, "current TSC frequency");
	c.first_ticks = c.last_ticks = rdtscll();
	c.tick_budget = (uint64_t)mhz * PT19_TOTAL_USECS;
	admit_bar(&c);
	cap = pci_find_capability(DEV_PTR(gpp_bridge_0), PCI_CAP_ID_PCIE);
	REQUIRE(&c, cap >= 0x40 && cap <= 0xec, "root PCIe capability");
	link = pci_read_config16(DEV_PTR(gpp_bridge_0), cap + PCI_EXP_LNKSTA);
	printk(BIOS_INFO, "NATIVE42 PT19 BAR=%08x..%llx root_link=%04x "
	       "port_policy=OEM_FIXED22_PRE_FIFO Gen1_switch=excluded\n",
	       (uint32_t)c.mmio, c.resource_end, link);
	REQUIRE(&c, link & PCI_EXP_LNKSTA_DLLLA, "current root data link active");
	payload = load_firmware(&c);
	/* Fresh reads immediately before this trial's first controller effect. */
	admit_bar(&c);
	REQUIRE(&c, pci_read_config32(c.dev, 0xf0) == marker &&
		pci_read_config8(c.dev, 0xef) == 0 && budget(&c), "renewed marker/control/deadline");
	c.command = pci_read_config16(c.dev, PCI_COMMAND);
	REQUIRE(&c, c.command != 0xffff && (c.command & PCI_COMMAND_MEMORY),
		"renewed incoming Command identity");
	c.command_owned = 1;
	/* Preserve every incoming Command bit. Only add the MASTER bit required
	 * by the Original kernel, then restore the complete incoming WORD.
	 */
	pci_write_config16(c.dev, PCI_COMMAND, c.command | PCI_COMMAND_MASTER);
	REQUIRE(&c, pci_read_config16(c.dev, PCI_COMMAND) ==
		(c.command | PCI_COMMAND_MASTER), "temporary endpoint MASTER readback");
	port_policy(&c);
	upload(&c, payload);
	admit_bar(&c);
	REQUIRE(&c, restore_command(&c), "incoming Command restore/readback");
	printk(BIOS_INFO, "NATIVE42 PT19 FIFO COMPLETE stores=%u F0_after=%08x "
	       "register_progress_checked=1 firmware_content_readback=0 USB_unobserved=1\n",
	       c.stores, pci_read_config32(c.dev, 0xf0));
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}
