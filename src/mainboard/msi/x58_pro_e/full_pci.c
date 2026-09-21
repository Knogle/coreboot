/* SPDX-License-Identifier: GPL-2.0-only */

#include <bootstate.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_def.h>
#include <device/pci_ids.h>
#include <device/pci_ops.h>
#include <device/resource.h>
#include <stdint.h>
#include <string.h>
#include "acpi_tables.h"
#include "pci.h"
#include "platform_acpi.h"
#include "full_pci.h"

/* The platform's proven one-DIMM map remains unchanged. These are limits,
 * not desired card BAR values. Normal coreboot allocates all PCI resources.
 */
#define FULL_IO_BASE 0x1000ULL
#define FULL_IO_TOP 0x10000ULL
#define FULL_MEM_BASE 0xc0000000ULL
#define FULL_MEM_TOP 0xe0000000ULL
#define FULL_ENDPOINTS 96
#define FULL_BRIDGES 32
#define FULL_DEPTH 8
#define FULL_LEAVES (FULL_ENDPOINTS * 7 + 64)
#define FULL_DECODE (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER)

struct full_root {
	unsigned int devfn;
	uint32_t id;
	bool conventional;
};

/* IOH identities were already gated/observed by the native path. D30F0 is
 * the ICH10 conventional PCI bridge, not a PCIe port or a PCIe switch.
 * Root-index ownership is deliberately independent of PCI_FUNC(): three
 * different IOH roots and the PCI bridge all have function number zero.
 */
static const struct full_root full_roots[] = {
	{ PCI_DEVFN(1, 0), 0x34088086, false },
	{ PCI_DEVFN(3, 0), 0x340a8086, false },
	{ PCI_DEVFN(7, 0), 0x340e8086, false },
	{ PCI_DEVFN(0x1c, 0), 0x3a408086, false },
	{ PCI_DEVFN(0x1c, 1), 0x3a428086, false },
	{ PCI_DEVFN(0x1c, 2), 0x3a448086, false },
	{ PCI_DEVFN(0x1c, 3), 0x3a468086, false },
	{ PCI_DEVFN(0x1c, 4), 0x3a488086, false },
	{ PCI_DEVFN(0x1c, 5), 0x3a4a8086, false },
	{ PCI_DEVFN(0x1e, 0), 0x244e8086, true },
};

/* Enumerated identities of the enabled ICH10 non-PCI-bridge functions.
 * SATA's initial IDE identity belongs to chipset pre-init, not this post-map
 * device-model contract. Disabled 19.0/1f.5 never become resource owners.
 */
static const struct {
	unsigned int devfn;
	uint32_t id;
} full_ich10_functions[] = {
	{ PCI_DEVFN(0x1a, 0), 0x3a378086 },
	{ PCI_DEVFN(0x1a, 1), 0x3a388086 },
	{ PCI_DEVFN(0x1a, 2), 0x3a398086 },
	{ PCI_DEVFN(0x1a, 7), 0x3a3c8086 },
	{ PCI_DEVFN(0x1b, 0), 0x3a3e8086 },
	{ PCI_DEVFN(0x1d, 0), 0x3a348086 },
	{ PCI_DEVFN(0x1d, 1), 0x3a358086 },
	{ PCI_DEVFN(0x1d, 2), 0x3a368086 },
	{ PCI_DEVFN(0x1d, 7), 0x3a3a8086 },
	{ PCI_DEVFN(0x1f, 0), 0x3a168086 },
	{ PCI_DEVFN(0x1f, 2), 0x3a228086 },
	{ PCI_DEVFN(0x1f, 3), 0x3a308086 },
	{ PCI_DEVFN(0x1f, 6), 0x3a328086 },
};

struct full_endpoint {
	struct device *dev;
	struct bus *parent;
	uint32_t id, classrev;
	uint8_t header, root, depth, child_slots, devfn;
};

static struct device *full_domain;
static struct full_endpoint full_endpoints[FULL_ENDPOINTS];
/* Snapshot use ends before recursive PCI scanning starts. Keep this whole-bus
 * admission buffer out of the inherited 8-KiB ramstage/early stack. */
static struct full_endpoint full_snapshot[256];
static size_t full_endpoint_count;
static unsigned int full_bridge_count;
static unsigned int full_scanned, full_isolated;
static bool full_preflight_ready, full_scan_started, full_seed_active;
static struct device_operations full_bridge_ops;
static unsigned int full_bridge_slots(const struct device *dev);

static int full_root_index(const struct device *dev)
{
	if (!dev || !full_domain || dev->path.type != DEVICE_PATH_PCI ||
	    dev->upstream != full_domain->downstream ||
	    pcidev_path_behind(dev->upstream, dev->path.pci.devfn) != dev)
		return -1;
	for (size_t i = 0; i < ARRAY_SIZE(full_roots); ++i)
		if (dev->path.pci.devfn == full_roots[i].devfn)
			return i;
	return -1;
}

static void full_require_root(struct device *dev)
{
	const int i = full_root_index(dev);
	if (i < 0 || !dev->enabled || !dev->mandatory ||
	    pci_read_config32(dev, PCI_VENDOR_ID) != full_roots[i].id ||
	    (pci_read_config32(dev, PCI_CLASS_REVISION) >> 16) != PCI_CLASS_BRIDGE_PCI ||
	    (pci_read_config8(dev, PCI_HEADER_TYPE) & 0x7f) != PCI_HEADER_TYPE_BRIDGE)
		die("[ICH10] root identity/topology mismatch\n");
}

void x58_full_pci_preflight(void)
{
	struct device *peg = pcidev_on_root(3, 0);
	if (full_preflight_ready || !peg || !peg->upstream || !peg->upstream->dev ||
	    peg->upstream->dev->path.type != DEVICE_PATH_DOMAIN)
		die("[ICH10] preflight domain/order mismatch\n");
	full_domain = peg->upstream->dev;
	if (full_domain->downstream->secondary || full_domain->downstream->segment_group)
		die("[ICH10] root bus must be segment zero, bus zero\n");
	size_t roots = 0, functions = 0, disabled = 0;
	for (struct device *dev = full_domain->downstream->children; dev; dev = dev->sibling) {
		if (dev->path.type != DEVICE_PATH_PCI || dev->hidden)
			die("[ICH10] unsupported static root path\n");
		for (struct device *prior = full_domain->downstream->children; prior != dev;
		     prior = prior->sibling)
			if (prior->path.pci.devfn == dev->path.pci.devfn)
				die("[ICH10] duplicate static root path\n");
		if (!dev->enabled) {
			if (dev->path.pci.devfn != PCI_DEVFN(0x19, 0) &&
			    dev->path.pci.devfn != PCI_DEVFN(0x1f, 5))
				die("[ICH10] unsupported disabled function policy\n");
			++disabled;
		} else if (full_root_index(dev) >= 0) {
			++roots;
		} else {
			bool found = false;
			for (size_t i = 0; i < ARRAY_SIZE(full_ich10_functions); ++i)
				found |= dev->path.pci.devfn == full_ich10_functions[i].devfn;
			if (!found)
				die("[ICH10] unknown enabled static function\n");
			++functions;
		}
	}
	if (roots != ARRAY_SIZE(full_roots) || functions != ARRAY_SIZE(full_ich10_functions) ||
	    disabled != 2)
		die("[ICH10] incomplete static chipset graph\n");
	/* Read all bridge identities before chipset init may change any decode. */
	for (size_t i = 0; i < ARRAY_SIZE(full_roots); ++i)
		full_require_root(pcidev_path_behind(full_domain->downstream,
			full_roots[i].devfn));
	for (size_t i = 0; i < ARRAY_SIZE(full_ich10_functions); ++i) {
		const unsigned int devfn = full_ich10_functions[i].devfn;
		struct device *dev = pcidev_path_behind(full_domain->downstream, devfn);
		/* Full chipset init selects AHCI only after this read-only admission. */
		const uint32_t id = devfn == PCI_DEVFN(0x1f, 2) ?
			0x3a208086 : full_ich10_functions[i].id;
		if (!dev || !dev->enabled || !dev->mandatory || dev->hidden ||
		    pci_read_config32(dev, PCI_VENDOR_ID) != id ||
		    pci_read_config32(dev, PCI_CLASS_REVISION) == UINT32_MAX ||
		    (pci_read_config8(dev, PCI_HEADER_TYPE) & 0x7f) != PCI_HEADER_TYPE_NORMAL)
			die("[ICH10] ICH10 pre-init identity/graph mismatch\n");
	}
	full_preflight_ready = true;
}

static void full_clear_command(struct device *dev)
{
	const uint16_t command = pci_read_config16(dev, PCI_COMMAND);
	if (command == UINT16_MAX)
		die("[ICH10] inaccessible command; no write\n");
	pci_write_config16(dev, PCI_COMMAND, command & ~FULL_DECODE);
	if (pci_read_config16(dev, PCI_COMMAND) != (command & ~FULL_DECODE))
		die("[ICH10] command quiescence failed; no retry\n");
	dev->command &= ~FULL_DECODE;
}

static bool full_absent(uint32_t id)
{
	return id == 0 || id == UINT32_MAX || id == 0xffff || id == 0xffff0000;
}

static const struct full_endpoint *full_endpoint_entry(const struct device *dev)
{
	for (size_t i = 0; i < full_endpoint_count; ++i)
		if (full_endpoints[i].dev == dev)
			return &full_endpoints[i];
	return NULL;
}

static void full_require_endpoint(struct device *dev)
{
	const struct full_endpoint *entry = full_endpoint_entry(dev);
	if (!entry || entry->root >= ARRAY_SIZE(full_roots) || !entry->depth ||
	    entry->depth > FULL_DEPTH || dev->path.pci.devfn != entry->devfn ||
	    !dev->enabled || !dev->mandatory || dev->hidden ||
	    dev->path.type != DEVICE_PATH_PCI || dev->upstream != entry->parent ||
	    (!entry->child_slots && dev->downstream) ||
	    dev->upstream->dev->downstream != dev->upstream ||
	    pcidev_path_behind(dev->upstream, dev->path.pci.devfn) != dev ||
	    dev->ops != (entry->child_slots ? &full_bridge_ops : &x58_full_endpoint_ops) ||
	    pci_read_config32(dev, PCI_VENDOR_ID) != entry->id ||
	    pci_read_config32(dev, PCI_CLASS_REVISION) != entry->classrev ||
	    pci_read_config8(dev, PCI_HEADER_TYPE) != entry->header ||
	    (((uint32_t)dev->device << 16) | dev->vendor) != entry->id ||
	    dev->class != entry->classrev >> 8 || dev->hdr_type != entry->header ||
	    (entry->child_slots && full_bridge_slots(dev) != entry->child_slots))
		die("[ICH10] endpoint identity/topology changed\n");
}

static void full_endpoint_enable(struct device *dev)
{
	full_require_endpoint(dev);
	full_clear_command(dev);
}

struct device_operations x58_full_endpoint_ops = {
	.read_resources = pci_dev_read_resources,
	.set_resources = pci_dev_set_resources,
	.enable_resources = pci_dev_enable_resources,
	.enable = full_endpoint_enable,
};

bool x58_full_pci_root_isolated(const struct device *dev)
{
	const int i = full_root_index(dev);
	return i >= 0 && (full_isolated & (1U << i));
}

bool x58_full_ich10_pci_admitted(const struct device *dev)
{
	if (!full_domain || !dev || !dev->enabled || !dev->mandatory || dev->hidden ||
	    dev->path.type != DEVICE_PATH_PCI || !dev->upstream ||
	    dev->upstream->segment_group != 0 || dev->upstream->secondary >= 0xff ||
	    pcidev_path_behind(dev->upstream, dev->path.pci.devfn) != dev)
		return false;
	const struct full_endpoint *endpoint = full_endpoint_entry(dev);
	uint32_t id = 0;
	if (endpoint) {
		if (endpoint->root >= ARRAY_SIZE(full_roots) || !endpoint->depth ||
		    endpoint->depth > FULL_DEPTH || dev->path.pci.devfn != endpoint->devfn ||
		    (full_isolated & (1U << endpoint->root)) ||
		    dev->upstream != endpoint->parent ||
		    dev->upstream->dev->downstream != dev->upstream ||
		    (endpoint->child_slots ? !dev->downstream : !!dev->downstream) ||
		    dev->ops != (endpoint->child_slots ? &full_bridge_ops : &x58_full_endpoint_ops) ||
		    dev->class != endpoint->classrev >> 8 || dev->hdr_type != endpoint->header ||
		    pci_read_config32(dev, PCI_CLASS_REVISION) != endpoint->classrev ||
		    pci_read_config8(dev, PCI_HEADER_TYPE) != endpoint->header ||
		    (endpoint->child_slots && full_bridge_slots(dev) != endpoint->child_slots))
			return false;
		/* Frozen ancestry prevents transplanting a valid node into another
		 * branch. The decreasing depth also bounds malformed/cyclic graphs. */
		const struct full_endpoint *ancestor = endpoint;
		while (ancestor->depth > 1) {
			const struct full_endpoint *parent = full_endpoint_entry(ancestor->parent->dev);
			if (!parent || !parent->child_slots || parent->root != endpoint->root ||
			    parent->depth + 1 != ancestor->depth ||
			    parent->dev->upstream != parent->parent || !parent->dev->enabled ||
			    parent->dev->path.pci.devfn != parent->devfn ||
			    parent->dev->downstream != ancestor->parent)
				return false;
			ancestor = parent;
		}
		if (ancestor->depth != 1 || full_root_index(ancestor->parent->dev) != endpoint->root)
			return false;
		id = endpoint->id;
	} else if (dev->upstream == full_domain->downstream) {
		const int root = full_root_index(dev);
		if (root >= 0) {
			if ((pci_read_config32(dev, PCI_CLASS_REVISION) >> 16) != PCI_CLASS_BRIDGE_PCI ||
			    (pci_read_config8(dev, PCI_HEADER_TYPE) & 0x7f) != PCI_HEADER_TYPE_BRIDGE)
				return false;
			id = full_roots[root].id;
		} else {
			if ((pci_read_config8(dev, PCI_HEADER_TYPE) & 0x7f) != PCI_HEADER_TYPE_NORMAL)
				return false;
			for (size_t i = 0; i < ARRAY_SIZE(full_ich10_functions); ++i)
				if (full_ich10_functions[i].devfn == dev->path.pci.devfn)
					id = full_ich10_functions[i].id;
		}
	}
	return id && pci_read_config32(dev, PCI_VENDOR_ID) == id &&
		(((uint32_t)dev->device << 16) | dev->vendor) == id;
}

static void full_verify_isolation(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(full_roots); ++i) {
		if (!(full_isolated & (1U << i)))
			continue;
		struct device *root = pcidev_path_behind(full_domain->downstream,
			full_roots[i].devfn);
		full_require_root(root);
		if (!root->downstream ||
		    ((root->command | pci_read_config16(root, PCI_COMMAND)) & FULL_DECODE))
			die("[ICH10] quarantined root forwarding reopened\n");
		for (struct device *dev = root->downstream->children; dev; dev = dev->sibling)
			if (dev->enabled)
				die("[ICH10] quarantined root has enabled child\n");
	}
}

bool x58_full_pci_intx_route(const struct device *dev, unsigned int pin,
	struct x58_full_intx_route *route)
{
	const struct full_endpoint *entry = full_endpoint_entry(dev);
	if (!route || pin < 1 || pin > 4 || !entry ||
	    !x58_full_ich10_pci_admitted(dev))
		return false;
	for (unsigned int depth = entry->depth; depth > 1; --depth) {
		pin = (pin - 1 + PCI_SLOT(dev->path.pci.devfn)) % 4 + 1;
		dev = dev->upstream->dev;
		if (!x58_full_ich10_pci_admitted(dev))
			return false;
	}
	const unsigned int root = entry->root;
	const unsigned int slot = PCI_SLOT(dev->path.pci.devfn);
	if (slot >= (full_roots[root].conventional ? 2U : 1U))
		return false;
	/* Observed vendor NPE1/3/7, P0P4..9 and P0P1 APIC _PRT packages.
	 * No extrapolation to additional physical motherboard PCI positions. */
	const unsigned int rotation = root >= 3 && root < 9 ? root - 3 : 0;
	*route = (struct x58_full_intx_route) {
		.root_devfn = full_roots[root].devfn, .slot = slot, .pin = pin,
		.gsi = 16 + (rotation + slot + pin - 1) % 4,
	};
	return true;
}

static void full_quarantine(unsigned int root, unsigned int devfn, uint32_t id)
{
	struct device *bridge = pcidev_path_behind(full_domain->downstream,
		full_roots[root].devfn);
	/* Root is quiescent before scanning. Close forwarding before disabling
	 * graph nodes; no downstream config access after isolation is assumed. */
	full_clear_command(bridge);
	full_isolated |= 1U << root;
	for (size_t i = 0; i < full_endpoint_count; ++i)
		if (full_endpoints[i].root == root)
			full_endpoints[i].dev->enabled = false;
	for (struct device *dev = bridge->downstream->children; dev; dev = dev->sibling)
		dev->enabled = false;
	full_verify_isolation();
	printk(BIOS_WARNING, "[ICH10] ADDIN_SKIPPED ROOT=%02x.%u DEVFN=%02x ID=%08x FORWARDING=OFF\n",
		PCI_SLOT(full_roots[root].devfn), PCI_FUNC(full_roots[root].devfn), devfn, id);
}

/* PCI Type1 bridge layout and PCI Express capability Device/Port Type.
 * Only transparent bridges are admitted; no subtractive/CardBus/ARI or
 * hotplug policy is inferred. Capability traversal is bounded and read-only.
 * Upstream switch ports and conventional PCI buses have 32 device numbers;
 * PCIe downstream links have only device0 (PCIe r5.0, section7.3.1). */
static unsigned int full_bridge_slots(const struct device *dev)
{
	if (!(pci_read_config16(dev, PCI_STATUS) & PCI_STATUS_CAP_LIST))
		return 32;
	unsigned int offset = pci_read_config8(dev, PCI_CAPABILITY_LIST);
	if (!offset)
		return 0;
	uint64_t seen = 0;
	unsigned int slots = 32;
	bool pcie = false;
	while (offset) {
		if (offset < 0x40 || offset > 0xfc || (offset & 3) ||
		    (seen & (1ULL << (offset / 4))))
			return 0;
		seen |= 1ULL << (offset / 4);
		const uint16_t cap = pci_read_config16(dev, offset);
		if (cap == UINT16_MAX || pci_read_config16(dev, offset) != cap)
			return 0;
		if ((cap & 0xff) == PCI_CAP_ID_PCIE) {
			if (pcie || offset > 0xe8)
				return 0;
			pcie = true;
			const uint16_t flags = pci_read_config16(dev, offset + PCI_EXP_FLAGS);
			if (!(flags & PCI_EXP_FLAGS_VERS) ||
			    pci_read_config16(dev, offset + PCI_EXP_FLAGS) != flags)
				return 0;
			switch ((flags & PCI_EXP_FLAGS_TYPE) >> 4) {
			case PCI_EXP_TYPE_UPSTREAM:
			case PCI_EXP_TYPE_PCI_BRIDGE:
				slots = 32;
				break;
			case PCI_EXP_TYPE_DOWNSTREAM:
			case PCI_EXP_TYPE_PCIE_BRIDGE:
				slots = 1;
				break;
			default:
				return 0;
			}
		}
		offset = cap >> 8;
	}
	return slots;
}

static void full_seed_bus(struct bus *bus, unsigned int min, unsigned int max)
{
	/* Conventional PCI has 32 device numbers, each with at most 8 functions.
	 * Snapshot a whole bus before node creation/BAR sizing, so an unsupported
	 * bridge can quarantine the root without partially admitting other cards.
	 */
	struct full_endpoint *snapshot = full_snapshot;
	const struct full_endpoint *parent = bus ? full_endpoint_entry(bus->dev) : NULL;
	const int root = parent ? parent->root : bus ? full_root_index(bus->dev) : -1;
	const unsigned int depth = parent ? parent->depth + 1 : 1;
	if (full_seed_active || root < 0 || bus->dev->downstream != bus || min != 0 || max < 7 ||
	    bus->secondary == 0 || bus->secondary >= 0xff || bus->segment_group ||
	    (!parent && (full_scanned & (1U << root))))
		die("[ICH10] endpoint scan boundary/order invalid\n");
	const unsigned int slots = parent ? parent->child_slots : full_roots[root].conventional ? 32 : 1;
	unsigned int present = 0, bridges = 0;
	full_seed_active = true;
	memset(snapshot, 0, sizeof(full_snapshot));
	if (!parent)
		full_scanned |= 1U << root;
	for (unsigned int slot = 0; slot < slots; ++slot) {
		unsigned int functions = 1;
		for (unsigned int fn = 0; fn < functions; ++fn) {
			const unsigned int devfn = PCI_DEVFN(slot, fn);
			struct device probe = { .upstream = bus,
				.path = { .type = DEVICE_PATH_PCI, .pci.devfn = devfn } };
			struct full_endpoint *entry = &snapshot[devfn];
			entry->id = pci_read_config32(&probe, PCI_VENDOR_ID);
			if (full_absent(entry->id)) {
				if (pci_read_config32(&probe, PCI_VENDOR_ID) != entry->id)
					die("[ICH10] unstable absent function\n");
				continue;
			}
			entry->classrev = pci_read_config32(&probe, PCI_CLASS_REVISION);
			entry->header = pci_read_config8(&probe, PCI_HEADER_TYPE);
			if (entry->classrev == UINT32_MAX ||
			    pci_read_config32(&probe, PCI_VENDOR_ID) != entry->id ||
			    pci_read_config32(&probe, PCI_CLASS_REVISION) != entry->classrev ||
			    pci_read_config8(&probe, PCI_HEADER_TYPE) != entry->header)
				die("[ICH10] unstable function; no BAR probe\n");
			const bool bridge =
				(entry->header & 0x7f) == PCI_HEADER_TYPE_BRIDGE &&
				(entry->classrev >> 8) == (PCI_CLASS_BRIDGE_PCI << 8);
			if (bridge)
				entry->child_slots = full_bridge_slots(&probe);
			if ((!parent && full_roots[root].conventional && slot > 1) ||
			    (bridge ? !entry->child_slots :
			     ((entry->header & 0x7f) != PCI_HEADER_TYPE_NORMAL ||
			      (entry->classrev >> 24) == PCI_BASE_CLASS_BRIDGE)) ||
			    depth > FULL_DEPTH || (bridge && depth == FULL_DEPTH)) {
				full_quarantine(root, devfn, entry->id);
				full_seed_active = false;
				return;
			}
			entry->root = root;
			entry->depth = depth;
			entry->parent = bus;
			entry->devfn = devfn;
			bridges += bridge;
			if (!fn && (entry->header & 0x80))
				functions = 8;
			++present;
		}
	}
	if (full_endpoint_count > FULL_ENDPOINTS || present > FULL_ENDPOINTS - full_endpoint_count ||
	    bridges > FULL_BRIDGES - full_bridge_count) {
		full_quarantine(root, 0, 0);
		full_seed_active = false;
		return;
	}
	/* Only the optional PEG VGA is statically described. Keep this property
	 * so ONBOARD_VGA_IS_PRIMARY selects it, not a discovered secondary VGA.
	 */
	for (struct device *dev = bus->children; dev; dev = dev->sibling) {
		if (parent || full_roots[root].devfn != PCI_DEVFN(3, 0) ||
		    dev->path.type != DEVICE_PATH_PCI || dev->path.pci.devfn != 0 ||
		    dev->ops != &x58_full_endpoint_ops || dev->sibling)
			die("[ICH10] unexpected static endpoint\n");
		if (full_absent(snapshot[0].id))
			dev->enabled = false;
		else if ((snapshot[0].id & 0xffff) != 0x1002 ||
			 (snapshot[0].classrev >> 16) != PCI_CLASS_DISPLAY_VGA)
			die("[ICH10] static primary VGA identity mismatch\n");
	}
	for (unsigned int devfn = 0; devfn < slots * 8; ++devfn) {
		if (full_absent(snapshot[devfn].id))
			continue;
		struct device *dev = pcidev_path_behind(bus, devfn);
		if (!dev) {
			struct device_path path = { .type = DEVICE_PATH_PCI, .pci.devfn = devfn };
			dev = alloc_dev(bus, &path);
			if (!dev)
				die("[ICH10] endpoint allocation failed\n");
		}
		dev->mandatory = true;
		dev->ops = snapshot[devfn].child_slots ? &full_bridge_ops : &x58_full_endpoint_ops;
		snapshot[devfn].dev = dev;
		full_endpoints[full_endpoint_count++] = snapshot[devfn];
		printk(BIOS_DEBUG, "[ICH10] ENDPOINT ROOT=%02x.%u BUS=%02x DEVFN=%02x ID=%08x GENERIC=1 BME=OS\n",
			PCI_SLOT(full_roots[root].devfn), PCI_FUNC(full_roots[root].devfn),
			bus->secondary, devfn, snapshot[devfn].id);
	}
	full_bridge_count += bridges;
	/* All snapshot entries have been copied; recursive scans may reuse it. */
	full_seed_active = false;
	pci_scan_bus(bus, min, MIN(max, slots * 8 - 1));
}

void x58_full_pci_scan_bridge(struct device *dev)
{
	if (!full_scan_started)
		die("[ICH10] bridge scan outside domain scan\n");
	const struct full_endpoint *entry = full_endpoint_entry(dev);
	if (entry) {
		if (full_isolated & (1U << entry->root))
			return;
		full_require_endpoint(dev);
		if (!entry->child_slots)
			die("[ICH10] endpoint used as bridge\n");
		if (dev->downstream)
			die("[ICH10] repeated add-in bridge scan\n");
		if (dev->upstream->subordinate >= 0xfe - ARRAY_SIZE(full_roots)) {
			full_quarantine(entry->root, dev->path.pci.devfn, entry->id);
			return;
		}
	} else {
		full_require_root(dev);
	}
	full_clear_command(dev);
	do_pci_scan_bridge(dev, full_seed_bus);
}

static struct device_operations full_bridge_ops = {
	.read_resources = pci_bus_read_resources,
	.set_resources = pci_dev_set_resources,
	.enable_resources = pci_bus_enable_resources,
	.enable = full_endpoint_enable,
	.scan_bus = x58_full_pci_scan_bridge,
};

struct device_operations x58_full_ioh_ops = {
	.read_resources = pci_bus_read_resources,
	.set_resources = pci_dev_set_resources,
	.enable_resources = pci_bus_enable_resources,
	.scan_bus = x58_full_pci_scan_bridge,
};

static void full_verify_routes(void)
{
	bool used[256] = { false };
	if (full_scanned != (1U << ARRAY_SIZE(full_roots)) - 1)
		die("[ICH10] incomplete bridge scan\n");
	for (size_t i = 0; i < ARRAY_SIZE(full_roots) + full_endpoint_count; ++i) {
		const struct full_endpoint *entry = i < ARRAY_SIZE(full_roots) ? NULL :
			&full_endpoints[i - ARRAY_SIZE(full_roots)];
		if (entry && (!entry->child_slots || !entry->dev->enabled))
			continue;
		struct device *dev = entry ? entry->dev :
			pcidev_path_behind(full_domain->downstream, full_roots[i].devfn);
		if (entry)
			full_require_endpoint(dev);
		else
			full_require_root(dev);
		const uint32_t buses = pci_read_config32(dev, PCI_PRIMARY_BUS);
		const unsigned int secondary = (buses >> 8) & 0xff;
		const unsigned int subordinate = (buses >> 16) & 0xff;
		if (!secondary || secondary == 0xff || used[secondary] ||
		    (buses & 0xff) != dev->upstream->secondary ||
		    subordinate < secondary || subordinate >= 0xff ||
		    !dev->downstream || dev->downstream->secondary != secondary ||
		    dev->downstream->subordinate != subordinate ||
		    (entry && (secondary <= dev->upstream->secondary ||
			       subordinate > dev->upstream->subordinate)))
			die("[ICH10] bridge route readback mismatch\n");
		used[secondary] = true;
		/* Root ranges are disjoint; nested ranges may only overlap their
		 * ancestors, never a sibling. Check all previously visited bridges. */
		for (size_t j = 0; j < i; ++j) {
			const struct full_endpoint *prior = j < ARRAY_SIZE(full_roots) ? NULL :
				&full_endpoints[j - ARRAY_SIZE(full_roots)];
			if (prior && (!prior->child_slots || !prior->dev->enabled))
				continue;
			const struct device *other = prior ? prior->dev :
				pcidev_path_behind(full_domain->downstream, full_roots[j].devfn);
			if (other->upstream == dev->upstream &&
			    secondary <= other->downstream->subordinate &&
			    other->downstream->secondary <= subordinate)
				die("[ICH10] overlapping sibling bus ranges\n");
		}
	}
	full_verify_isolation();
}

static void full_scan_domain(struct device *dev)
{
	if (!full_preflight_ready || full_scan_started || dev != full_domain)
		die("[ICH10] domain scan admission/order mismatch\n");
	x58_full_platform_verify("full-scan");
	x58_full_ioh_prepare();
	/* Standard Type1 route setup must never forward CPU uncore busff. */
	dev->downstream->max_subordinate = 0xfe;
	full_scan_started = true;
	pci_host_bridge_scan_bus(dev);
	full_verify_routes();
	for (size_t i = 0; i < full_endpoint_count; ++i) {
		if (full_isolated & (1U << full_endpoints[i].root))
			continue;
		full_require_endpoint(full_endpoints[i].dev);
		if (pci_read_config16(full_endpoints[i].dev, PCI_COMMAND) & FULL_DECODE)
			die("[ICH10] endpoint not quiescent before allocation\n");
	}
	printk(BIOS_NOTICE, "[ICH10] ENUMERATED ROOTS=%zu ENDPOINTS=%zu ISOLATED=%03x\n",
		ARRAY_SIZE(full_roots), full_endpoint_count, full_isolated);
}

static void full_read_resources(struct device *dev)
{
	x58_full_platform_verify("full-resource-map");
	full_verify_routes();
	ram_range(dev, 0, 0, 0xa0000);
	mmio_range(dev, 1, 0xa0000, 0x20000);
	reserved_ram_range(dev, 2, 0xc0000, 0x40000);
	ram_from_to(dev, 3, 0x100000, FULL_MEM_BASE);
	ram_from_to(dev, 4, 0x100000000ULL, 0x140000000ULL);
	domain_io_window_from_to(dev, 5, FULL_IO_BASE, FULL_IO_TOP);
	domain_mem_window_from_to(dev, 6, FULL_MEM_BASE, FULL_MEM_TOP);
	mmio_range(dev, 7, FULL_MEM_TOP, 0x10000000);
	/* Normal LPC owns IO0..fff, ROM and IOAPIC; SMBus owns its fixed BAR.
	 * Do not duplicate those as allocatable leaves of the host bridge.
	 */
	mmio_range(dev, 12, 0xfed1c000, 0x4000);
	mmio_range(dev, 13, 0xfee00000, 0x1000);
	mmio_range(dev, 15, 0xfed00000, 0x400);
	x58_platform_reserve(dev);
}

static void full_resource_bounds(const struct resource *res)
{
	const bool io = res->flags & IORESOURCE_IO;
	const uint64_t low = io ? FULL_IO_BASE : FULL_MEM_BASE;
	const uint64_t high = io ? FULL_IO_TOP : FULL_MEM_TOP;
	if ((res->flags & (IORESOURCE_IO | IORESOURCE_MEM)) == (IORESOURCE_IO | IORESOURCE_MEM) ||
	    (res->flags & (IORESOURCE_ASSIGNED | IORESOURCE_STORED)) !=
	    (IORESOURCE_ASSIGNED | IORESOURCE_STORED) ||
	    (res->flags & IORESOURCE_ABOVE_4G) || !res->size ||
	    res->base < low || res->base >= high || res->size > high - res->base)
		die("[ICH10] unassigned/out-of-aperture PCI resource\n");
}

static void full_resource_readback(struct device *dev, const struct resource *res)
{
	uint64_t base, end;
	if (res->flags & IORESOURCE_BRIDGE) {
		if (res->index == PCI_IO_BASE) {
			base = (pci_read_config8(dev, PCI_IO_BASE) & 0xf0) << 8;
			end = ((pci_read_config8(dev, PCI_IO_LIMIT) & 0xf0) << 8) | 0xfff;
			if ((pci_read_config8(dev, PCI_IO_BASE) & 0xf) == PCI_IO_RANGE_TYPE_32) {
				base |= (uint64_t)pci_read_config16(dev, PCI_IO_BASE_UPPER16) << 16;
				end |= (uint64_t)pci_read_config16(dev, PCI_IO_LIMIT_UPPER16) << 16;
			}
		} else if (res->index == PCI_MEMORY_BASE || res->index == PCI_PREF_MEMORY_BASE) {
			base = (uint64_t)(pci_read_config16(dev, res->index) & 0xfff0) << 16;
			end = ((uint64_t)(pci_read_config16(dev, res->index + 2) & 0xfff0) << 16) | 0xfffff;
			if (res->index == PCI_PREF_MEMORY_BASE &&
			    (pci_read_config16(dev, res->index) & 0xf) == PCI_PREF_RANGE_TYPE_64) {
				base |= (uint64_t)pci_read_config32(dev, PCI_PREF_BASE_UPPER32) << 32;
				end |= (uint64_t)pci_read_config32(dev, PCI_PREF_LIMIT_UPPER32) << 32;
			}
		} else {
			die("[ICH10] unknown bridge window\n");
		}
		if (base != res->base || end != res->base + res->size - 1)
			die("[ICH10] bridge window readback mismatch\n");
		return;
	}
	const uint32_t value = pci_read_config32(dev, res->index);
	const bool rom = res->index == ((dev->hdr_type & 0x7f) == PCI_HEADER_TYPE_BRIDGE ?
		PCI_ROM_ADDRESS1 : PCI_ROM_ADDRESS);
	base = value & (rom ? 0xfffff800U : (res->flags & IORESOURCE_IO) ? 0xfffffffcU : 0xfffffff0U);
	if (res->flags & IORESOURCE_PCI64)
		base |= (uint64_t)pci_read_config32(dev, res->index + 4) << 32;
	if (base != res->base)
		die("[ICH10] BAR readback mismatch\n");
}

static bool full_window_contains(const struct resource *window, const struct resource *leaf)
{
	return window->size && (window->flags & IORESOURCE_BRIDGE) &&
		(!(window->flags & IORESOURCE_PREFETCH) || (leaf->flags & IORESOURCE_PREFETCH)) &&
		(window->flags & (IORESOURCE_IO | IORESOURCE_MEM)) ==
		(leaf->flags & (IORESOURCE_IO | IORESOURCE_MEM)) &&
		leaf->base >= window->base && leaf->size <= window->size &&
		leaf->base - window->base <= window->size - leaf->size;
}

static bool full_resources_overlap(const struct resource *left, const struct resource *right)
{
	return (left->flags & (IORESOURCE_IO | IORESOURCE_MEM)) ==
		(right->flags & (IORESOURCE_IO | IORESOURCE_MEM)) &&
		left->base < right->base + right->size && right->base < left->base + left->size;
}

static bool full_descends_from(const struct device *dev, const struct device *ancestor)
{
	for (unsigned int depth = 0; dev && depth <= FULL_DEPTH; ++depth) {
		dev = dev->upstream ? dev->upstream->dev : NULL;
		if (dev == ancestor)
			return true;
	}
	return false;
}

static void full_resources_assigned(void *unused)
{
	static const struct resource *leaves[FULL_LEAVES];
	static const struct device *leaf_owners[FULL_LEAVES];
	static const struct resource *windows[(ARRAY_SIZE(full_roots) + FULL_BRIDGES) * 3];
	static const struct device *window_owners[(ARRAY_SIZE(full_roots) + FULL_BRIDGES) * 3];
	size_t count = 0, window_count = 0;
	(void)unused;
	full_verify_routes();
	x58_full_platform_verify("full-resources-assigned");
	x58_platform_verify_reservations(full_domain);
	for (struct device *dev = all_devices; dev; dev = dev->next) {
		if (dev->path.type != DEVICE_PATH_PCI || !dev->enabled)
			continue;
		if (!x58_full_ich10_pci_admitted(dev))
			die("[ICH10] unadmitted PCI resource owner\n");
		const struct full_endpoint *endpoint = full_endpoint_entry(dev);
		if (endpoint)
			full_require_endpoint(dev);
		for (const struct resource *res = dev->resource_list; res; res = res->next) {
			if (!res->size || !(res->flags & (IORESOURCE_IO | IORESOURCE_MEM)) ||
			    (res->flags & (IORESOURCE_FIXED | IORESOURCE_SUBTRACTIVE)))
				continue;
			full_resource_bounds(res);
			full_resource_readback(dev, res);
			if (endpoint) {
				bool contained = false;
				for (const struct resource *win = dev->upstream->dev->resource_list; win; win = win->next)
					contained |= full_window_contains(win, res);
				if (!contained)
					die("[ICH10] endpoint BAR outside parent bridge window\n");
			}
			if (res->flags & IORESOURCE_BRIDGE) {
				if (window_count == ARRAY_SIZE(windows))
					die("[ICH10] bridge window capacity exceeded\n");
				for (size_t i = 0; i < window_count; ++i)
					if (full_resources_overlap(windows[i], res) &&
					    !full_descends_from(dev, window_owners[i]) &&
					    !full_descends_from(window_owners[i], dev))
						die("[ICH10] overlapping sibling bridge windows\n");
				windows[window_count] = res;
				window_owners[window_count++] = dev;
				continue;
			}
			if (count == ARRAY_SIZE(leaves))
				die("[ICH10] resource audit capacity exceeded\n");
			for (size_t i = 0; i < count; ++i)
				if (full_resources_overlap(leaves[i], res))
					die("[ICH10] overlapping PCI leaves\n");
			leaf_owners[count] = dev;
			leaves[count++] = res;
			printk(BIOS_DEBUG, "[ICH10] RESOURCE %s BAR=%lx BASE=%llx SIZE=%llx\n",
				dev_path(dev), res->index, (unsigned long long)res->base,
				(unsigned long long)res->size);
		}
	}
	for (size_t i = 0; i < window_count; ++i)
		for (size_t j = 0; j < count; ++j)
			if (full_resources_overlap(windows[i], leaves[j]) &&
			    !full_descends_from(leaf_owners[j], window_owners[i]))
				die("[ICH10] bridge window covers unrelated PCI BAR\n");
	printk(BIOS_NOTICE, "[ICH10] RESOURCES_READY LEAVES=%zu READBACK=PASS PARENT_WINDOWS=PASS\n", count);
}

static void full_handoff(void *unused)
{
	(void)unused;
	full_verify_routes();
	for (size_t i = 0; i < full_endpoint_count; ++i) {
		struct device *dev = full_endpoints[i].dev;
		if (full_isolated & (1U << full_endpoints[i].root))
			continue;
		full_require_endpoint(dev);
		/* Bridge BME forwards downstream transactions under the normal
		 * bridge owner; endpoint DMA remains exclusively an OS decision. */
		if ((!full_endpoints[i].child_slots && (dev->command & PCI_COMMAND_MASTER)) ||
		    (pci_read_config16(dev, PCI_COMMAND) & FULL_DECODE) != (dev->command & FULL_DECODE))
			die("[ICH10] endpoint decode/BME handoff mismatch\n");
	}
	printk(BIOS_NOTICE, "[ICH10] PCI_HANDOFF ENDPOINTS=%zu BME=OS ISOLATED=%03x\n",
		full_endpoint_count, full_isolated);
}

static struct device_operations full_domain_ops = {
	.read_resources = full_read_resources,
	.set_resources = pci_domain_set_resources,
	.scan_bus = full_scan_domain,
	.acpi_fill_ssdt = x58_platform_fill_ssdt,
	.write_acpi_tables = x58_acpi_platform_write_acpi_tables,
};

static void x58_full_pci_admit_domain(struct device *dev)
{
	if (dev->path.type != DEVICE_PATH_DOMAIN || (full_domain && full_domain != dev))
		die("[ICH10] unexpected domain owner\n");
	full_domain = dev;
}

void x58_full_pci_enable_dev(struct device *dev)
{
	x58_full_pci_admit_domain(dev);
	dev->ops = &full_domain_ops;
}

BOOT_STATE_INIT_ENTRY(BS_DEV_RESOURCES, BS_ON_EXIT, full_resources_assigned, NULL);
BOOT_STATE_INIT_ENTRY(BS_DEV_INIT, BS_ON_EXIT, full_handoff, NULL);
