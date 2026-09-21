/* SPDX-License-Identifier: GPL-2.0-only */

/* PCIE_ENUM: bounded direct endpoints on the three non-onboard ICH10 links.
 * Included after the shared identity/command helpers in pci.c.
 * Keep minimal scanning globally: never probe arbitrary bus0/uncore functions.
 * PCIe downstream links expose device0 (pci_bus_only_one_child documents the
 * standard rule). Switches/bridges, hotplug and other IOH links are separate.
 */
#define X58_PCI_DYNAMIC_MAX 24
struct x58_pci_dynamic_entry {
	struct device *dev;
	uint32_t id, classrev;
	uint8_t header;
};
static struct x58_pci_dynamic_entry x58_pci_dynamic[X58_PCI_DYNAMIC_MAX];
static size_t x58_pci_dynamic_count;
static unsigned int x58_pci_dynamic_scanned;
static struct device_operations x58_pci_dynamic_ops;
static unsigned int x58_pci_dynamic_quarantined;
static struct device *x58_pci_quarantined_roots[6];

static bool x58_pci_dynamic_root(const struct device *root)
{
	return root && x58_pci_domain && x58_pci_domain->downstream &&
		root->upstream == x58_pci_domain->downstream &&
		root->path.type == DEVICE_PATH_PCI &&
		pcidev_path_behind(root->upstream, root->path.pci.devfn) == root &&
		(root->path.pci.devfn == PCI_DEVFN(0x1c, 0) ||
		 root->path.pci.devfn == PCI_DEVFN(0x1c, 2) ||
		 root->path.pci.devfn == PCI_DEVFN(0x1c, 3));
}

static const struct x58_pci_dynamic_entry *x58_pci_dynamic_entry(const struct device *dev)
{
	for (size_t i = 0; i < x58_pci_dynamic_count; i++)
		if (x58_pci_dynamic[i].dev == dev)
			return &x58_pci_dynamic[i];
	return NULL;
}

static void x58_pci_dynamic_require(struct device *dev)
{
	const struct x58_pci_dynamic_entry *entry = x58_pci_dynamic_entry(dev);
	if (!entry || !dev->enabled || !dev->mandatory || dev->hidden ||
	    dev->path.type != DEVICE_PATH_PCI ||
	    !dev->upstream || !x58_pci_dynamic_root(dev->upstream->dev) ||
	    dev->upstream->dev->downstream != dev->upstream ||
	    PCI_SLOT(dev->path.pci.devfn) != 0 || dev->downstream ||
	    pcidev_path_behind(dev->upstream, dev->path.pci.devfn) != dev ||
	    dev->ops != &x58_pci_dynamic_ops ||
	    (((uint32_t)dev->device << 16) | dev->vendor) != entry->id ||
	    dev->class != entry->classrev >> 8 || dev->hdr_type != entry->header ||
	    pci_read_config32(dev, PCI_VENDOR_ID) != entry->id ||
	    pci_read_config32(dev, PCI_CLASS_REVISION) != entry->classrev ||
	    pci_read_config8(dev, PCI_HEADER_TYPE) != entry->header)
		die("[PCIE_ENUM] dynamic endpoint identity/topology changed\n");
}

static void x58_pci_dynamic_probe_gate(struct device *dev)
{
	x58_pci_dynamic_require(dev);
	x58_pci_clear_command(dev);
}

static struct device_operations x58_pci_dynamic_ops = {
	.read_resources = pci_dev_read_resources,
	.set_resources = pci_dev_set_resources,
	.enable_resources = pci_dev_enable_resources,
	.enable = x58_pci_dynamic_probe_gate,
};

static bool x58_pci_dynamic_id_absent(uint32_t id)
{
	/* Same absent responses as the coreboot PCI probe. */
	return id == UINT32_MAX || id == 0 || id == 0x0000ffff || id == 0xffff0000;
}

static void x58_pci_dynamic_verify_isolation(void)
{
	for (unsigned int p = 0; p < ARRAY_SIZE(x58_pci_quarantined_roots); p++) {
		struct device *root = x58_pci_quarantined_roots[p];
		if (!(x58_pci_dynamic_quarantined & (1U << p))) {
			if (root)
				die("[PCIE_ISOLATION] unexpected isolated root record\n");
			continue;
		}
		if (!x58_pci_dynamic_root(root) || !root->enabled || !root->downstream ||
		    root->downstream->dev != root || root->downstream->children ||
		    PCI_FUNC(root->path.pci.devfn) != p ||
		    ((root->command | pci_read_config16(root, PCI_COMMAND)) &
		     (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER)))
			die("[PCIE_ISOLATION] add-in link isolation lost\n");
	}
}

static void x58_pci_dynamic_isolate(struct bus *bus, unsigned int fn,
	const struct x58_pci_dynamic_entry *entry)
{
	struct device *root = bus->dev;
	const unsigned int p = PCI_FUNC(root->path.pci.devfn);
	if (!x58_pci_dynamic_root(root) || bus->children ||
	    root->downstream != bus || x58_pci_quarantined_roots[p])
		die("[PCIE_ISOLATION] invalid pre-allocation isolation boundary\n");
	/* ICH10 datasheet 319973-003 section20.1.3, PCICMD bits0..2:
	 * standard Type-1 IO/MEM/BME forwarding gates, using the
	 * existing board command helper. Never size unsupported BARs, touch
	 * the card's vendor registers, hide the port or guess link-control bits.
	 * A later OS may enumerate/re-enable the visible link independently.
	 */
	x58_pci_clear_command(root);
	root->command &= ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
	x58_pci_quarantined_roots[p] = root;
	x58_pci_dynamic_quarantined |= 1U << p;
	x58_pci_dynamic_verify_isolation();
	printk(BIOS_WARNING, "[PCIE_ISOLATION] ADDIN_SKIPPED ROOT=00:1c.%u FN=%u ID=%08x CLASSREV=%08x HDR=%02x REASON=UNSUPPORTED_HEADER_OR_BRIDGE FORWARDING=OFF BOOT=CONTINUE\n",
		p, fn, entry->id, entry->classrev, entry->header);
}

static void x58_pci_dynamic_seed(struct bus *bus, unsigned int min_devfn,
	unsigned int max_devfn)
{
	struct x58_pci_dynamic_entry snapshots[8] = { 0 };
	unsigned int functions = 1;
	if (!bus || !x58_pci_dynamic_root(bus->dev))
		return;
	const unsigned int bit = 1U << PCI_FUNC(bus->dev->path.pci.devfn);
	if (bus->dev->downstream != bus ||
	    (x58_pci_dynamic_scanned & bit) || bus->children || min_devfn != 0 ||
	    max_devfn < 7 || bus->secondary == 0 || bus->secondary >= 0xff)
		die("[PCIE_ENUM] dynamic scan boundary/order invalid\n");
	x58_pci_dynamic_scanned |= bit;
	/* Snapshot all exposed functions before creating any node or sizing BARs.
	 * Preset generic ops bypass driver-specific discovery/init side effects.
	 */
	for (unsigned int fn = 0; fn < functions; fn++) {
		struct device probe = { .upstream = bus,
			.path = { .type = DEVICE_PATH_PCI, .pci.devfn = PCI_DEVFN(0, fn) } };
		struct x58_pci_dynamic_entry *entry = &snapshots[fn];
		entry->id = pci_read_config32(&probe, PCI_VENDOR_ID);
		if (x58_pci_dynamic_id_absent(entry->id)) {
			if (pci_read_config32(&probe, PCI_VENDOR_ID) != entry->id)
				die("[PCIE_ENUM] unstable absent endpoint; no hotplug admission\n");
			continue;
		}
		entry->classrev = pci_read_config32(&probe, PCI_CLASS_REVISION);
		entry->header = pci_read_config8(&probe, PCI_HEADER_TYPE);
		if (entry->classrev == UINT32_MAX ||
		    pci_read_config32(&probe, PCI_VENDOR_ID) != entry->id ||
		    pci_read_config32(&probe, PCI_CLASS_REVISION) != entry->classrev ||
		    pci_read_config8(&probe, PCI_HEADER_TYPE) != entry->header)
			die("[PCIE_ENUM] unsupported bridge or unstable direct endpoint; remove card for recovery\n");
		if ((entry->header & 0x7f) != PCI_HEADER_TYPE_NORMAL ||
		    (entry->classrev >> 24) == PCI_BASE_CLASS_BRIDGE) {
			x58_pci_dynamic_isolate(bus, fn, entry);
			return;
		}
		if (fn == 0 && (entry->header & 0x80))
			functions = 8;
	}
	for (unsigned int fn = 0; fn < functions; fn++) {
		if (x58_pci_dynamic_id_absent(snapshots[fn].id))
			continue;
		if (x58_pci_dynamic_count == ARRAY_SIZE(x58_pci_dynamic))
			die("[PCIE_ENUM] dynamic endpoint count limit\n");
		struct device_path path = { .type = DEVICE_PATH_PCI, .pci.devfn = PCI_DEVFN(0, fn) };
		struct device *dev = alloc_dev(bus, &path);
		if (!dev)
			die("[PCIE_ENUM] dynamic endpoint allocation failed\n");
		dev->mandatory = true;
		dev->ops = &x58_pci_dynamic_ops;
		snapshots[fn].dev = dev;
		x58_pci_dynamic[x58_pci_dynamic_count++] = snapshots[fn];
		printk(BIOS_NOTICE, "[PCIE_ENUM] ADDIN ROOT=00:1c.%u FN=%u ID=%08x CLASSREV=%08x HDR=%02x DRIVER=GENERIC BME=OS\n",
			PCI_FUNC(bus->dev->path.pci.devfn), fn, snapshots[fn].id,
			snapshots[fn].classrev, snapshots[fn].header);
	}
}

static void x58_pci_dynamic_quiescent(void)
{
	x58_pci_dynamic_verify_isolation();
	if (x58_pci_dynamic_scanned != ((1U << 0) | (1U << 2) | (1U << 3)))
		die("[PCIE_ENUM] dynamic root scan incomplete\n");
	for (size_t i = 0; i < x58_pci_dynamic_count; i++) {
		struct device *dev = x58_pci_dynamic[i].dev;
		x58_pci_dynamic_require(dev);
		if (pci_read_config16(dev, PCI_COMMAND) &
		    (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER))
			die("[PCIE_ENUM] dynamic endpoint not quiescent before allocation\n");
	}
}
