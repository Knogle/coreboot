/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * B06VM is intentionally not a generic X58 PCI implementation.  It admits
 * only the exact standard-header functions observed on B06VL-HW-02, validates
 * every identity before BAR sizing, and allocates only from two fixed holes.
 */

#include <arch/pci_io_cfg.h>
#include <bootstate.h>
#include <cbmem.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pci_def.h>
#include <device/pci_ids.h>
#include <device/pci_type.h>
#include <device/resource.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "b06v6_handoff.h"
#include "b06vm_pci.h"

#define B06VM_BUILD_ID "X58PROE-B06VM-AUTO-PCI-VGA-IPXE-20260905"

#define B06VM_SAD_DEV		PCI_DEV(0xff, 0, 1)
#define B06VM_SAD_ID		0x2d818086u
#define B06VM_SAD_PCIEXBAR_LO	0x50
#define B06VM_SAD_PCIEXBAR_HI	0x54
#define B06VM_SAD_RULE_FIRST	0x80
#define B06VM_SAD_RULE_COUNT	8
#define B06VM_PCIEXBAR_LO	0xe0000001u

#define B06VM_X58_HOST_DEV	PCI_DEV(0, 0, 0)
#define B06VM_X58_HOST_ID	0x34058086u
#define B06VM_X58_HOST_CLASSREV	0x06000013u

#define B06VM_IOH_HM_DEV	PCI_DEV(0, 0x14, 0)
#define B06VM_IOH_HM_ID		0x342e8086u
#define B06VM_IOH_TOLM		0xd0
#define B06VM_IOH_TOHM_LO	0xd4
#define B06VM_IOH_TOHM_HI	0xd8
#define B06VM_TOLM_VALUE	0xbc000000u
#define B06VM_TOHM_LO_VALUE	0x3c000000u
#define B06VM_TOHM_HI_VALUE	0x00000001u

#define B06VM_LOW_RAM_BASE	0x00100000ULL
#define B06VM_LOW_RAM_TOP	0xc0000000ULL
#define B06VM_HIGH_RAM_BASE	0x100000000ULL
#define B06VM_HIGH_RAM_TOP	0x140000000ULL
#define B06VM_PCI_IO_BASE	0x1000u
#define B06VM_PCI_IO_TOP	0x10000u
#define B06VM_PCI_MMIO_BASE	0xc0000000ULL
#define B06VM_PCI_MMIO_TOP	0xe0000000ULL
#define B06VM_ECAM_BASE		0xe0000000ULL
#define B06VM_ECAM_TOP		0xf0000000ULL

#define B06VM_ROOT_FUNCTIONS	12
#define B06VM_DOWNSTREAM_FUNCTIONS 2
#define B06VM_EXPECTED_FUNCTIONS	(B06VM_ROOT_FUNCTIONS + \
					 B06VM_DOWNSTREAM_FUNCTIONS)
#define B06VM_MAX_LEAF_RESOURCES	96

#define POST_B06VM_PREFLIGHT	0x2b
#define POST_B06VM_ROOTS_SAFE	0x2c
#define POST_B06VM_SCAN_OK	0x2d
#define POST_B06VM_RESOURCES	0x2e
#define POST_B06VM_ALLOC_OK	0x2f
#define POST_B06VM_ENABLE_OK	0x34
#define POST_B06VM_HANDOFF_FAIL	0x36
#define POST_B06VM_PLATFORM_FAIL 0x37
#define POST_B06VM_TOPOLOGY_FAIL 0x38
#define POST_B06VM_IDENTITY_FAIL 0x39
#define POST_B06VM_COMMAND_FAIL	0x3a
#define POST_B06VM_BUS_FAIL	0x3b
#define POST_B06VM_RESOURCE_FAIL 0x3c
#define POST_B06VM_OVERLAP_FAIL	0x3d
#define POST_B06VM_ENABLE_FAIL	0x3e

_Static_assert(CONFIG(MINIMAL_PCI_SCANNING),
	"B06VM must never scan PCI functions absent from its mandatory devicetree");
_Static_assert(CONFIG(PCI_ALLOW_BUS_MASTER) &&
	CONFIG(PCI_SET_BUS_MASTER_PCI_BRIDGES),
	"B06VM requires forwarding BME on its two selected PCI bridges");
_Static_assert(!CONFIG(PCI_ALLOW_BUS_MASTER_ANY_DEVICE),
	"B06VM must not grant blanket endpoint bus mastering");
_Static_assert(X58_B06V6_OBJECT_BASE + X58_B06V6_OBJECT_SIZE <=
	B06VM_PCI_MMIO_BASE, "B06VM PCI MMIO overlaps a B06VL-smoked RAM window");
_Static_assert(B06VM_LOW_RAM_TOP == B06VM_PCI_MMIO_BASE,
	"B06VM PCI MMIO must begin exactly at the gated low-RAM top");
_Static_assert(B06VM_PCI_MMIO_TOP == B06VM_ECAM_BASE,
	"B06VM PCI MMIO must end before PCIEXBAR ECAM");
_Static_assert(B06VM_ECAM_TOP <= B06VM_HIGH_RAM_BASE,
	"B06VM PCIEXBAR ECAM must end before remapped RAM");

struct b06vm_expected_pci {
	unsigned int root_devfn;
	unsigned int devfn;
	uint32_t id;
	uint16_t class_code;
	int16_t revision;
	uint8_t header_type;
	bool downstream;
	bool bridge;
	const char *name;
};

/*
 * Root IDs/classes come directly from B06VL-HW-02.  The downstream NIC ID
 * and revision come from the live board inventory.  10de:0f00 is the exact
 * PCI ID associated with the recorded GF108 / GeForce GT 630 test adapter.
 * The capture records only the upper 16 class bits, so class_code deliberately
 * does not invent an unobserved programming-interface byte.
 */
static const struct b06vm_expected_pci b06vm_expected[] = {
	{ PCI_DEVFN(0x03, 0), PCI_DEVFN(0x03, 0), 0x340a8086u,
	  PCI_CLASS_BRIDGE_PCI, -1, PCI_HEADER_TYPE_BRIDGE, false, true,
	  "X58 PEG3" },
	{ PCI_DEVFN(0x03, 0), PCI_DEVFN(0x00, 0), 0x0f0010deu,
	  PCI_CLASS_DISPLAY_VGA, -1, PCI_HEADER_TYPE_NORMAL, true, false,
	  "GF108 VGA" },
	{ PCI_DEVFN(0x1a, 0), PCI_DEVFN(0x1a, 0), 0x3a378086u,
	  PCI_CLASS_SERIAL_USB, -1, PCI_HEADER_TYPE_NORMAL, false, false,
	  "ICH10R UHCI4" },
	{ PCI_DEVFN(0x1a, 1), PCI_DEVFN(0x1a, 1), 0x3a388086u,
	  PCI_CLASS_SERIAL_USB, -1, PCI_HEADER_TYPE_NORMAL, false, false,
	  "ICH10R UHCI5" },
	{ PCI_DEVFN(0x1a, 2), PCI_DEVFN(0x1a, 2), 0x3a398086u,
	  PCI_CLASS_SERIAL_USB, -1, PCI_HEADER_TYPE_NORMAL, false, false,
	  "ICH10R UHCI6" },
	{ PCI_DEVFN(0x1a, 7), PCI_DEVFN(0x1a, 7), 0x3a3c8086u,
	  PCI_CLASS_SERIAL_USB, -1, PCI_HEADER_TYPE_NORMAL, false, false,
	  "ICH10R EHCI2" },
	{ PCI_DEVFN(0x1c, 4), PCI_DEVFN(0x1c, 4), 0x3a488086u,
	  PCI_CLASS_BRIDGE_PCI, -1, PCI_HEADER_TYPE_BRIDGE, false, true,
	  "ICH10R RP5" },
	{ PCI_DEVFN(0x1c, 4), PCI_DEVFN(0x00, 0), 0x816810ecu,
	  PCI_CLASS_NETWORK_ETHERNET, 0x02, PCI_HEADER_TYPE_NORMAL, true, false,
	  "RTL8168" },
	{ PCI_DEVFN(0x1d, 0), PCI_DEVFN(0x1d, 0), 0x3a348086u,
	  PCI_CLASS_SERIAL_USB, -1, PCI_HEADER_TYPE_NORMAL, false, false,
	  "ICH10R UHCI1" },
	{ PCI_DEVFN(0x1d, 1), PCI_DEVFN(0x1d, 1), 0x3a358086u,
	  PCI_CLASS_SERIAL_USB, -1, PCI_HEADER_TYPE_NORMAL, false, false,
	  "ICH10R UHCI2" },
	{ PCI_DEVFN(0x1d, 2), PCI_DEVFN(0x1d, 2), 0x3a368086u,
	  PCI_CLASS_SERIAL_USB, -1, PCI_HEADER_TYPE_NORMAL, false, false,
	  "ICH10R UHCI3" },
	{ PCI_DEVFN(0x1d, 7), PCI_DEVFN(0x1d, 7), 0x3a3a8086u,
	  PCI_CLASS_SERIAL_USB, -1, PCI_HEADER_TYPE_NORMAL, false, false,
	  "ICH10R EHCI1" },
	{ PCI_DEVFN(0x1f, 2), PCI_DEVFN(0x1f, 2), 0x3a208086u,
	  PCI_CLASS_STORAGE_IDE, -1, PCI_HEADER_TYPE_NORMAL, false, false,
	  "ICH10R SATA1" },
	{ PCI_DEVFN(0x1f, 5), PCI_DEVFN(0x1f, 5), 0x3a268086u,
	  PCI_CLASS_STORAGE_IDE, -1, PCI_HEADER_TYPE_NORMAL, false, false,
	  "ICH10R SATA2" },
};

static const uint32_t b06vm_sad_rules[B06VM_SAD_RULE_COUNT] = {
	0x00000bc3u, 0x00000fc0u, 0x000013c3u, 0x000013c0u,
	0x000013c0u, 0x000013c0u, 0x000013c0u, 0x000013c0u,
};

struct b06vm_leaf_resource {
	const struct device *dev;
	const struct resource *resource;
};

static struct device *b06vm_domain;

_Static_assert(ARRAY_SIZE(b06vm_expected) == B06VM_EXPECTED_FUNCTIONS,
	"B06VM expected-function count changed");

static const struct x58_b06v6_handoff *b06vm_verified_handoff(void)
{
	const struct cbmem_entry *entry;
	const struct x58_b06v6_handoff *handoff;
	void *cbmem_base;
	size_t cbmem_size;
	uintptr_t cbmem_start;
	uintptr_t cbmem_end;

	if (!cbmem_online())
		die_with_post_code(POST_B06VM_HANDOFF_FAIL,
			"[RAMSTAGE] B06VM CBMEM is offline\n");
	entry = cbmem_entry_find(X58_B06V6_CBMEM_ID);
	if (entry == NULL || cbmem_entry_size(entry) < sizeof(*handoff) ||
	    cbmem_get_region(&cbmem_base, &cbmem_size))
		die_with_post_code(POST_B06VM_HANDOFF_FAIL,
			"[RAMSTAGE] B06VM handoff entry/CBMEM region invalid\n");

	cbmem_start = (uintptr_t)cbmem_base;
	if (cbmem_start > UINTPTR_MAX - cbmem_size)
		die_with_post_code(POST_B06VM_HANDOFF_FAIL,
			"[RAMSTAGE] B06VM CBMEM region wraps\n");
	cbmem_end = cbmem_start + cbmem_size;
	if (cbmem_start < X58_B06V6_CBMEM_BASE ||
	    cbmem_end != X58_B06V6_CBMEM_TOP || cbmem_end <= cbmem_start)
		die_with_post_code(POST_B06VM_HANDOFF_FAIL,
			"[RAMSTAGE] B06VM CBMEM escaped the B06VL-smoked window\n");

	handoff = cbmem_entry_start(entry);
	if ((uintptr_t)handoff < cbmem_start ||
	    (uintptr_t)handoff > cbmem_end - sizeof(*handoff) ||
	    !x58_b06v6_handoff_is_valid(handoff) ||
	    !(handoff->flags & X58_B06VF_HANDOFF_LOWMEM_SMOKED) ||
	    !(handoff->flags & X58_B06VL_HANDOFF_BROAD_POST_MINIT))
		die_with_post_code(POST_B06VM_HANDOFF_FAIL,
			"[RAMSTAGE] B06VM exact B06VL v9 handoff gate failed\n");

	return handoff;
}

static void b06vm_require_platform_state(const char *phase)
{
	uint32_t value;

	(void)b06vm_verified_handoff();
	if (pci_io_read_config32(B06VM_SAD_DEV, PCI_VENDOR_ID) != B06VM_SAD_ID ||
	    pci_io_read_config32(B06VM_SAD_DEV, B06VM_SAD_PCIEXBAR_LO) !=
		B06VM_PCIEXBAR_LO ||
	    pci_io_read_config32(B06VM_SAD_DEV, B06VM_SAD_PCIEXBAR_HI) != 0 ||
	    pci_io_read_config32(B06VM_X58_HOST_DEV, PCI_VENDOR_ID) !=
		B06VM_X58_HOST_ID ||
	    pci_io_read_config32(B06VM_X58_HOST_DEV, PCI_CLASS_REVISION) !=
		B06VM_X58_HOST_CLASSREV)
		die_with_post_code(POST_B06VM_PLATFORM_FAIL,
			"[RAMSTAGE] B06VM %s host/PCIEXBAR gate failed\n", phase);

	for (size_t i = 0; i < ARRAY_SIZE(b06vm_sad_rules); i++) {
		value = pci_io_read_config32(B06VM_SAD_DEV,
			B06VM_SAD_RULE_FIRST + i * sizeof(uint32_t));
		if (value != b06vm_sad_rules[i])
			die_with_post_code(POST_B06VM_PLATFORM_FAIL,
				"[RAMSTAGE] B06VM %s SAD%zu=%08x expected=%08x\n",
				phase, i, value, b06vm_sad_rules[i]);
	}

	if (pci_io_read_config32(B06VM_IOH_HM_DEV, PCI_VENDOR_ID) !=
		B06VM_IOH_HM_ID ||
	    pci_io_read_config32(B06VM_IOH_HM_DEV, B06VM_IOH_TOLM) !=
		B06VM_TOLM_VALUE ||
	    pci_io_read_config32(B06VM_IOH_HM_DEV, B06VM_IOH_TOHM_LO) !=
		B06VM_TOHM_LO_VALUE ||
	    pci_io_read_config32(B06VM_IOH_HM_DEV, B06VM_IOH_TOHM_HI) !=
		B06VM_TOHM_HI_VALUE)
		die_with_post_code(POST_B06VM_PLATFORM_FAIL,
			"[RAMSTAGE] B06VM %s exact TOLM/TOHM gate failed\n", phase);
}

static const struct b06vm_expected_pci *b06vm_expected_for_device(
	const struct device *dev)
{
	unsigned int root_devfn;
	bool downstream;

	if (b06vm_domain == NULL || dev == NULL ||
	    dev->path.type != DEVICE_PATH_PCI || dev->upstream == NULL)
		return NULL;

	if (dev->upstream == b06vm_domain->downstream) {
		root_devfn = dev->path.pci.devfn;
		downstream = false;
	} else if (dev->upstream->dev != NULL &&
		   dev->upstream->dev->path.type == DEVICE_PATH_PCI &&
		   dev->upstream->dev->upstream == b06vm_domain->downstream) {
		root_devfn = dev->upstream->dev->path.pci.devfn;
		downstream = true;
	} else {
		return NULL;
	}

	for (size_t i = 0; i < ARRAY_SIZE(b06vm_expected); i++) {
		const struct b06vm_expected_pci *expected = &b06vm_expected[i];

		if (expected->root_devfn == root_devfn &&
		    expected->devfn == dev->path.pci.devfn &&
		    expected->downstream == downstream)
			return expected;
	}

	return NULL;
}

static struct device *b06vm_device_for_expected(
	const struct b06vm_expected_pci *expected)
{
	struct device *root;

	if (b06vm_domain == NULL || b06vm_domain->downstream == NULL)
		return NULL;
	root = pcidev_path_behind(b06vm_domain->downstream,
		expected->root_devfn);
	if (!expected->downstream)
		return root;
	if (root == NULL || root->downstream == NULL)
		return NULL;
	return pcidev_path_behind(root->downstream, expected->devfn);
}

static void b06vm_require_identity(struct device *dev)
{
	const struct b06vm_expected_pci *expected =
		b06vm_expected_for_device(dev);
	const uint32_t id = ((uint32_t)dev->device << 16) | dev->vendor;
	const uint32_t classrev = pci_read_config32(dev, PCI_CLASS_REVISION);

	if (expected == NULL || !dev->enabled || !dev->mandatory ||
	    id != expected->id || (dev->class >> 8) != expected->class_code ||
	    (classrev >> 8) != dev->class ||
	    (dev->hdr_type & 0x7f) != expected->header_type ||
	    (expected->revision >= 0 &&
	     (classrev & 0xff) != (uint8_t)expected->revision))
		die_with_post_code(POST_B06VM_IDENTITY_FAIL,
			"[RAMSTAGE] B06VM rejected selected PCI function %s\n",
			expected ? expected->name : "outside allowlist");
}

static void b06vm_clear_command(struct device *dev)
{
	const uint16_t decode = PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
		PCI_COMMAND_MASTER;
	const uint16_t command = pci_read_config16(dev, PCI_COMMAND);

	pci_write_config16(dev, PCI_COMMAND, command & ~decode);
	if (pci_read_config16(dev, PCI_COMMAND) & decode)
		die_with_post_code(POST_B06VM_COMMAND_FAIL,
			"[RAMSTAGE] B06VM could not clear decode/master on %s\n",
			dev_path(dev));
	dev->command &= ~PCI_COMMAND_MASTER;
}

static void b06vm_probe_gate(struct device *dev)
{
	b06vm_require_identity(dev);
	b06vm_clear_command(dev);
}

static void b06vm_scan_bridge(struct device *dev)
{
	const struct b06vm_expected_pci *expected =
		b06vm_expected_for_device(dev);

	b06vm_require_identity(dev);
	if (expected == NULL || !expected->bridge)
		die_with_post_code(POST_B06VM_TOPOLOGY_FAIL,
			"[RAMSTAGE] B06VM refused non-allowlisted bridge scan\n");
	pci_scan_bridge(dev);
}

static void b06vm_require_static_topology(void)
{
	struct device *root;
	size_t root_count = 0;
	size_t downstream_count = 0;

	if (b06vm_domain == NULL || b06vm_domain->downstream == NULL)
		die_with_post_code(POST_B06VM_TOPOLOGY_FAIL,
			"[RAMSTAGE] B06VM domain has no static downstream bus\n");

	for (root = b06vm_domain->downstream->children; root;
	     root = root->sibling) {
		const struct b06vm_expected_pci *expected =
			b06vm_expected_for_device(root);

		root_count++;
		if (expected == NULL || expected->downstream || !root->enabled ||
		    !root->mandatory ||
		    root->ops != (expected->bridge ? &b06vm_root_port_ops :
						&b06vm_endpoint_ops))
			die_with_post_code(POST_B06VM_TOPOLOGY_FAIL,
				"[RAMSTAGE] B06VM root devicetree escaped allowlist\n");

		if (expected->bridge) {
			struct device *child;

			if (root->downstream == NULL ||
			    root->downstream->children == NULL ||
			    root->downstream->children->sibling != NULL)
				die_with_post_code(POST_B06VM_TOPOLOGY_FAIL,
					"[RAMSTAGE] B06VM bridge child shape mismatch\n");
			child = root->downstream->children;
			downstream_count++;
			if (b06vm_expected_for_device(child) == NULL ||
			    !child->enabled || !child->mandatory ||
			    child->ops != &b06vm_endpoint_ops)
				die_with_post_code(POST_B06VM_TOPOLOGY_FAIL,
					"[RAMSTAGE] B06VM bridge child escaped allowlist\n");
		} else if (root->downstream != NULL) {
			die_with_post_code(POST_B06VM_TOPOLOGY_FAIL,
				"[RAMSTAGE] B06VM endpoint unexpectedly owns a bus\n");
		}
	}

	if (root_count != B06VM_ROOT_FUNCTIONS ||
	    downstream_count != B06VM_DOWNSTREAM_FUNCTIONS)
		die_with_post_code(POST_B06VM_TOPOLOGY_FAIL,
			"[RAMSTAGE] B06VM static PCI function count mismatch\n");
}

static void b06vm_raw_root_preflight(void)
{
	const uint16_t decode = PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
		PCI_COMMAND_MASTER;

	/* Validate the complete root allowlist before the first PCI write. */
	for (size_t i = 0; i < ARRAY_SIZE(b06vm_expected); i++) {
		const struct b06vm_expected_pci *expected = &b06vm_expected[i];
		pci_devfn_t raw;
		uint32_t classrev;

		if (expected->downstream)
			continue;
		raw = PCI_DEV(0, PCI_SLOT(expected->root_devfn),
			PCI_FUNC(expected->root_devfn));
		classrev = pci_io_read_config32(raw, PCI_CLASS_REVISION);
		if (pci_io_read_config32(raw, PCI_VENDOR_ID) != expected->id ||
		    (classrev >> 16) != expected->class_code ||
		    (pci_io_read_config8(raw, PCI_HEADER_TYPE) & 0x7f) !=
			expected->header_type)
			die_with_post_code(POST_B06VM_IDENTITY_FAIL,
				"[RAMSTAGE] B06VM root preflight rejected %s\n",
				expected->name);
	}

	/* Only after every root identity passed may their decodes be quiesced. */
	for (size_t i = 0; i < ARRAY_SIZE(b06vm_expected); i++) {
		const struct b06vm_expected_pci *expected = &b06vm_expected[i];
		pci_devfn_t raw;
		uint16_t command;

		if (expected->downstream)
			continue;
		raw = PCI_DEV(0, PCI_SLOT(expected->root_devfn),
			PCI_FUNC(expected->root_devfn));
		command = pci_io_read_config16(raw, PCI_COMMAND);
		pci_io_write_config16(raw, PCI_COMMAND, command & ~decode);
		if (pci_io_read_config16(raw, PCI_COMMAND) & decode)
			die_with_post_code(POST_B06VM_COMMAND_FAIL,
				"[RAMSTAGE] B06VM root command clear failed for %s\n",
				expected->name);
	}
}

static void b06vm_require_bridge_routes(void)
{
	uint8_t secondary[2];
	size_t count = 0;

	for (size_t i = 0; i < ARRAY_SIZE(b06vm_expected); i++) {
		const struct b06vm_expected_pci *expected = &b06vm_expected[i];
		struct device *dev;
		uint32_t buses;

		if (!expected->bridge)
			continue;
		dev = b06vm_device_for_expected(expected);
		if (dev == NULL || dev->downstream == NULL || count >= 2)
			die_with_post_code(POST_B06VM_BUS_FAIL,
				"[RAMSTAGE] B06VM bridge route is missing\n");
		buses = pci_read_config32(dev, PCI_PRIMARY_BUS);
		secondary[count++] = (buses >> 8) & 0xff;
		if ((buses & 0xff) != 0 || secondary[count - 1] == 0 ||
		    secondary[count - 1] == 0xff ||
		    ((buses >> 16) & 0xff) != secondary[count - 1] ||
		    dev->downstream->secondary != secondary[count - 1] ||
		    dev->downstream->subordinate != secondary[count - 1])
			die_with_post_code(POST_B06VM_BUS_FAIL,
				"[RAMSTAGE] B06VM bridge bus-number readback failed\n");
	}
	if (count != 2 || secondary[0] == secondary[1])
		die_with_post_code(POST_B06VM_BUS_FAIL,
			"[RAMSTAGE] B06VM bridge buses are not distinct\n");
}

static void b06vm_require_enumerated_topology(void)
{
	b06vm_require_static_topology();
	for (size_t i = 0; i < ARRAY_SIZE(b06vm_expected); i++) {
		struct device *dev = b06vm_device_for_expected(&b06vm_expected[i]);

		if (dev == NULL)
			die_with_post_code(POST_B06VM_TOPOLOGY_FAIL,
				"[RAMSTAGE] B06VM selected function disappeared\n");
		b06vm_require_identity(dev);
		if (pci_read_config16(dev, PCI_COMMAND) &
		    (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER))
			die_with_post_code(POST_B06VM_COMMAND_FAIL,
				"[RAMSTAGE] B06VM selected function not quiescent\n");
	}
	b06vm_require_bridge_routes();
}

static void b06vm_domain_scan_bus(struct device *dev)
{
	if (dev != b06vm_domain)
		die_with_post_code(POST_B06VM_TOPOLOGY_FAIL,
			"[RAMSTAGE] B06VM unexpected PCI domain\n");

	post_code(POST_B06VM_PREFLIGHT);
	printk(BIOS_NOTICE, "[RAMSTAGE] %s selective PCI preflight\n",
	       B06VM_BUILD_ID);
	b06vm_require_platform_state("pre-scan");
	b06vm_require_static_topology();
	b06vm_raw_root_preflight();
	post_code(POST_B06VM_ROOTS_SAFE);

	pci_host_bridge_scan_bus(dev);
	b06vm_require_enumerated_topology();
	post_code(POST_B06VM_SCAN_OK);
	printk(BIOS_NOTICE,
	       "[RAMSTAGE] B06VM exact 12-root/2-downstream PCI scan accepted\n");
}

static void b06vm_read_resources(struct device *dev)
{
	b06vm_require_platform_state("resource-map");
	b06vm_require_enumerated_topology();

	/* Legacy VGA/option-ROM layout retained from the B06VL payload path. */
	ram_range(dev, 0, 0x00000000, 0x000a0000);
	mmio_range(dev, 1, 0x000a0000, 0x00020000);
	reserved_ram_range(dev, 2, 0x000c0000, 0x00040000);

	/* Exact gated one-DIMM map: 3 GiB low plus 1 GiB remapped above 4 GiB. */
	ram_from_to(dev, 3, B06VM_LOW_RAM_BASE, B06VM_LOW_RAM_TOP);
	ram_from_to(dev, 4, B06VM_HIGH_RAM_BASE, B06VM_HIGH_RAM_TOP);

	/* The allocator receives no subtractive or above-4G PCI window. */
	domain_io_window_from_to(dev, 5, B06VM_PCI_IO_BASE,
		B06VM_PCI_IO_TOP);
	domain_mem_window_from_to(dev, 6, B06VM_PCI_MMIO_BASE,
		B06VM_PCI_MMIO_TOP);
	mmio_from_to(dev, 7, B06VM_ECAM_BASE, B06VM_ECAM_TOP);

	post_code(POST_B06VM_RESOURCES);
	printk(BIOS_NOTICE,
	       "[RAMSTAGE] B06VM RAM=0-640K,1M-3G,4G-5G PCI_IO=1000-ffff PCI_MMIO=c0000000-dfffffff\n");
}

static void b06vm_require_domain_resource(unsigned long index, uint64_t base,
	uint64_t top, unsigned long required_flags)
{
	const struct resource *resource = probe_resource(b06vm_domain, index);
	bool bounds_exact;

	if (resource != NULL && (resource->flags & IORESOURCE_FIXED))
		bounds_exact = resource->base == base &&
			resource->size == top - base;
	else
		bounds_exact = resource != NULL && resource->base == base &&
			resource->limit == top - 1;

	if (!bounds_exact ||
	    (resource->flags & required_flags) != required_flags)
		die_with_post_code(POST_B06VM_RESOURCE_FAIL,
			"[RAMSTAGE] B06VM domain resource %lx mismatch\n", index);
}

static void b06vm_require_allocated_resource(const struct device *dev,
	const struct resource *resource)
{
	uint64_t top;
	const unsigned long type = resource->flags &
		(IORESOURCE_IO | IORESOURCE_MEM);

	if (!resource->size || !type)
		return;
	if (type == (IORESOURCE_IO | IORESOURCE_MEM) ||
	    resource->base > UINT64_MAX - resource->size)
		die_with_post_code(POST_B06VM_RESOURCE_FAIL,
			"[RAMSTAGE] B06VM malformed resource on %s\n", dev_path(dev));
	top = resource->base + resource->size;
	if (!(resource->flags & IORESOURCE_ASSIGNED) ||
	    !(resource->flags & IORESOURCE_STORED) ||
	    (resource->flags & IORESOURCE_ABOVE_4G) ||
	    resource_end(resource) != top - 1 ||
	    (type == IORESOURCE_IO &&
	     (resource->base < B06VM_PCI_IO_BASE || top > B06VM_PCI_IO_TOP)) ||
	    (type == IORESOURCE_MEM &&
	     (resource->base < B06VM_PCI_MMIO_BASE ||
	      top > B06VM_PCI_MMIO_TOP)))
		die_with_post_code(POST_B06VM_RESOURCE_FAIL,
			"[RAMSTAGE] B06VM resource escaped aperture on %s\n",
			dev_path(dev));
}

static size_t b06vm_collect_and_audit_resources(
	struct b06vm_leaf_resource leaves[B06VM_MAX_LEAF_RESOURCES])
{
	size_t leaf_count = 0;

	for (size_t i = 0; i < ARRAY_SIZE(b06vm_expected); i++) {
		const struct b06vm_expected_pci *expected = &b06vm_expected[i];
		struct device *dev = b06vm_device_for_expected(expected);
		const struct resource *resource;
		size_t usable = 0;

		if (dev == NULL)
			die_with_post_code(POST_B06VM_RESOURCE_FAIL,
				"[RAMSTAGE] B06VM resource owner missing\n");
		for (resource = dev->resource_list; resource;
		     resource = resource->next) {
			const unsigned long type = resource->flags &
				(IORESOURCE_IO | IORESOURCE_MEM);

			if (!resource->size || !type)
				continue;
			usable++;
			b06vm_require_allocated_resource(dev, resource);
			if (!(resource->flags & IORESOURCE_BRIDGE)) {
				if (leaf_count >= B06VM_MAX_LEAF_RESOURCES)
					die_with_post_code(POST_B06VM_RESOURCE_FAIL,
						"[RAMSTAGE] B06VM resource audit overflow\n");
				leaves[leaf_count++] = (struct b06vm_leaf_resource) {
					.dev = dev,
					.resource = resource,
				};
			}
		}
		if (!expected->bridge && usable == 0)
			die_with_post_code(POST_B06VM_RESOURCE_FAIL,
				"[RAMSTAGE] B06VM endpoint has no usable BAR: %s\n",
				expected->name);
	}

	return leaf_count;
}

static void b06vm_require_no_leaf_overlap(
	const struct b06vm_leaf_resource leaves[B06VM_MAX_LEAF_RESOURCES],
	size_t count)
{
	for (size_t i = 0; i < count; i++) {
		const struct resource *left = leaves[i].resource;
		const unsigned long left_type = left->flags &
			(IORESOURCE_IO | IORESOURCE_MEM);
		const uint64_t left_top = left->base + left->size;

		for (size_t j = i + 1; j < count; j++) {
			const struct resource *right = leaves[j].resource;
			const unsigned long right_type = right->flags &
				(IORESOURCE_IO | IORESOURCE_MEM);
			const uint64_t right_top = right->base + right->size;

			if (left_type == right_type && left->base < right_top &&
			    right->base < left_top)
				die_with_post_code(POST_B06VM_OVERLAP_FAIL,
					"[RAMSTAGE] B06VM overlapping leaf resources: %s / %s\n",
					dev_path(leaves[i].dev), dev_path(leaves[j].dev));
		}
	}
}

static void b06vm_resources_assigned(void *unused)
{
	struct b06vm_leaf_resource leaves[B06VM_MAX_LEAF_RESOURCES];
	size_t leaf_count;

	(void)unused;
	b06vm_require_platform_state("post-allocation");
	b06vm_require_enumerated_topology();
	b06vm_require_domain_resource(0, 0, 0x000a0000,
		IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_CACHEABLE);
	b06vm_require_domain_resource(1, 0x000a0000, 0x000c0000,
		IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_RESERVE);
	b06vm_require_domain_resource(2, 0x000c0000, 0x00100000,
		IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_RESERVE);
	b06vm_require_domain_resource(3, B06VM_LOW_RAM_BASE, B06VM_LOW_RAM_TOP,
		IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_CACHEABLE);
	b06vm_require_domain_resource(4, B06VM_HIGH_RAM_BASE, B06VM_HIGH_RAM_TOP,
		IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_CACHEABLE);
	b06vm_require_domain_resource(5, B06VM_PCI_IO_BASE, B06VM_PCI_IO_TOP,
		IORESOURCE_IO | IORESOURCE_BRIDGE);
	b06vm_require_domain_resource(6, B06VM_PCI_MMIO_BASE,
		B06VM_PCI_MMIO_TOP, IORESOURCE_MEM | IORESOURCE_BRIDGE);
	b06vm_require_domain_resource(7, B06VM_ECAM_BASE, B06VM_ECAM_TOP,
		IORESOURCE_MEM | IORESOURCE_FIXED | IORESOURCE_RESERVE);

	leaf_count = b06vm_collect_and_audit_resources(leaves);
	b06vm_require_no_leaf_overlap(leaves, leaf_count);
	post_code(POST_B06VM_ALLOC_OK);
	printk(BIOS_NOTICE,
	       "[RAMSTAGE] B06VM PCI allocation audit PASS; endpoint BME remains off\n");
}

static void b06vm_resources_enabled(void *unused)
{
	(void)unused;
	b06vm_require_platform_state("post-enable");
	for (size_t i = 0; i < ARRAY_SIZE(b06vm_expected); i++) {
		const struct b06vm_expected_pci *expected = &b06vm_expected[i];
		struct device *dev = b06vm_device_for_expected(expected);
		const uint16_t command = dev ?
			pci_read_config16(dev, PCI_COMMAND) : PCI_COMMAND_MASTER;
		const uint16_t expected_master = expected->bridge ?
			PCI_COMMAND_MASTER : 0;

		if (dev == NULL ||
		    (command & PCI_COMMAND_MASTER) != expected_master ||
		    (dev->command & PCI_COMMAND_MASTER) != expected_master ||
		    (command & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY)) !=
		    (dev->command & (PCI_COMMAND_IO | PCI_COMMAND_MEMORY)))
			die_with_post_code(POST_B06VM_ENABLE_FAIL,
				"[RAMSTAGE] B06VM decode/master enable audit failed\n");
	}
	post_code(POST_B06VM_ENABLE_OK);
	printk(BIOS_NOTICE,
	       "[RAMSTAGE] B06VM decodes enabled; BME only on two forwarding bridges\n");
}

static struct device_operations b06vm_domain_ops = {
	.read_resources = b06vm_read_resources,
	.set_resources = pci_domain_set_resources,
	.scan_bus = b06vm_domain_scan_bus,
};

static struct device_operations b06vm_cpu_cluster_ops = {
	.read_resources = noop_read_resources,
	.set_resources = noop_set_resources,
};

struct device_operations b06vm_root_port_ops = {
	.read_resources = pci_bus_read_resources,
	.set_resources = pci_dev_set_resources,
	.enable_resources = pci_bus_enable_resources,
	.scan_bus = b06vm_scan_bridge,
	.enable = b06vm_probe_gate,
};

struct device_operations b06vm_endpoint_ops = {
	.read_resources = pci_dev_read_resources,
	.set_resources = pci_dev_set_resources,
	.enable_resources = pci_dev_enable_resources,
	.enable = b06vm_probe_gate,
};

void x58_b06vm_enable_dev(struct device *dev)
{
	if (dev->path.type == DEVICE_PATH_DOMAIN) {
		if (b06vm_domain != NULL && b06vm_domain != dev)
			die_with_post_code(POST_B06VM_TOPOLOGY_FAIL,
				"[RAMSTAGE] B06VM found multiple PCI domains\n");
		b06vm_domain = dev;
		dev->ops = &b06vm_domain_ops;
	} else if (dev->path.type == DEVICE_PATH_CPU_CLUSTER) {
		dev->ops = &b06vm_cpu_cluster_ops;
	}
}

BOOT_STATE_INIT_ENTRY(BS_DEV_RESOURCES, BS_ON_EXIT,
	b06vm_resources_assigned, NULL);
BOOT_STATE_INIT_ENTRY(BS_DEV_ENABLE, BS_ON_EXIT,
	b06vm_resources_enabled, NULL);
