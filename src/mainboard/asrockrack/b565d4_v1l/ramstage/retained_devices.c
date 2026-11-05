/* SPDX-License-Identifier: GPL-2.0-only */
#include <commonlib/bsd/compiler.h>
#include "native84_profile.h"
#include "native87_devices.h"
#include <console/console.h>
#include <console/uart.h>
#include <cpu/amd/msr.h>
#include <cpu/x86/msr.h>
#include <device/device.h>
#include <device/pci.h>
#include <commonlib/helpers.h>
#include <static.h>

#if CONFIG_ECAM_MMCONF_BASE_ADDRESS != 0xf0000000 || \
	CONFIG_ECAM_MMCONF_BUS_NUMBER != 128 || CONFIG_ECAM_MMCONF_LENGTH != 0x08000000
#error "Native87 requires the selected same-board f0000000/128-bus ECAM configuration"
#endif

extern struct chip_operations superio_nuvoton_nct6796d_ops;
extern struct chip_operations superio_aspeed_ast2400_ops;

static unsigned int ecam_checked, pnp_attempted;

static void refusal(const char *reason) __noreturn;
static void refusal(const char *reason)
{
	printk(BIOS_ERR, "NATIVE87 REFUSAL %s\n", reason);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	native84_terminal();
}

void native87_check_ecam(void)
{
	msr_t mapping;

	native84_stage_check();
	if (ecam_checked)
		refusal("ECAM repeated");
	ecam_checked = 1;
	/* Same encoding as genuine enable_pci_mmconf, but never replay its WRMSR.
	 * log2(128)=7; base f0000000, enabled, reserved bits all zero.
	 * This fresh observation is required before any generic ECAM/DF/SMN use.
	 */
	mapping = rdmsr(MMIO_CONF_BASE);
	printk(BIOS_INFO, "NATIVE87 ECAM MSR %08x:%08x\n", mapping.hi, mapping.lo);
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
	if (mapping.hi || mapping.lo !=
	    (0xf0000000U | MMIO_RANGE_EN | (7U << MMIO_BUS_RANGE_SHIFT)))
		refusal("ECAM mapping");
}

/* Resource metadata only: the inherited AST decode stays unchanged. No PnP
 * configuration entry, LDN selection, enable/disable, or sibling creation.
 */
static struct chip_operations retained_pnp_chip = {
	.name = "Native87 retained PnP resources only",
};

static void retained_pnp_read_resources(struct device *dev)
{
	struct resource *io, *irq;

	if (dev->path.type != DEVICE_PATH_PNP || dev->path.pnp.port != 0x4e ||
	    dev->path.pnp.device != 3 || !dev->enabled)
		refusal("unexpected PnP resource reader");
	io = dev->resource_list;
	irq = io ? io->next : NULL;
	if (!io || !irq || irq->next || io->index != 0x60 || io->base != 0x2f8 ||
	    io->size || io->flags != (IORESOURCE_IO | IORESOURCE_FIXED | IORESOURCE_ASSIGNED) ||
	    irq->index != 0x70 || irq->base != 3 || irq->size ||
	    irq->flags != (IORESOURCE_IRQ | IORESOURCE_FIXED | IORESOURCE_ASSIGNED))
		refusal("AST fixed resource shape");
	io->size = 8;
	io->limit = 0x2ff;
	io->align = io->gran = 0;
	io->flags |= IORESOURCE_STORED;
	irq->size = 1;
	irq->limit = 3;
	irq->align = irq->gran = 0;
	irq->flags |= IORESOURCE_STORED;
}

static struct device_operations retained_pnp_ops = {
	.read_resources = retained_pnp_read_resources,
	.set_resources = noop_set_resources,
};

void native87_retain_pnp(void)
{
	struct device *nct2 = NULL, *nct3 = NULL, *ast3 = NULL, *dev;
	unsigned int nodes = 0;
	const struct resource *io, *irq;

	if (!ecam_checked || pnp_attempted)
		refusal("PnP preparation order");
	pnp_attempted = 1;
	/* Complete original static list, bounded independently of next pointers.
	 * No driver hook has run yet; reject additional PnP nodes rather than
	 * allowing generic pnp_enable_devices to manufacture enabled siblings.
	 */
	for (dev = all_devices; dev; dev = dev->next) {
		if (++nodes > 128)
			refusal("static device list");
		if (dev->path.type != DEVICE_PATH_PNP)
			continue;
		if (dev->downstream || dev->ops || !dev->upstream ||
		    !dev->upstream->dev || dev->upstream->dev->path.type != DEVICE_PATH_PCI ||
		    dev->upstream->dev->path.pci.devfn != PCI_DEVFN(0x14, 3))
			refusal("static PnP topology");
		if (dev->path.pnp.port == 0x2e && dev->path.pnp.device == 2 && !nct2)
			nct2 = dev;
		else if (dev->path.pnp.port == 0x2e && dev->path.pnp.device == 3 && !nct3)
			nct3 = dev;
		else if (dev->path.pnp.port == 0x4e && dev->path.pnp.device == 3 && !ast3)
			ast3 = dev;
		else
			refusal("unselected or duplicate PnP node");
	}
	if (!nct2 || !nct3 || !ast3 || nct2->enabled || nct3->enabled || !ast3->enabled ||
	    nct2->chip_ops != &superio_nuvoton_nct6796d_ops ||
	    nct3->chip_ops != &superio_nuvoton_nct6796d_ops ||
	    ast3->chip_ops != &superio_aspeed_ast2400_ops ||
	    nct2->upstream != ast3->upstream || nct3->upstream != ast3->upstream ||
	    nct2->resource_list || nct3->resource_list ||
	    ast3->upstream->children != nct2 || nct2->sibling != nct3 ||
	    nct3->sibling != ast3 || ast3->sibling)
		refusal("selected PnP shape");
	io = ast3->resource_list;
	irq = io ? io->next : NULL;
	if (!io || !irq || irq->next || io->index != 0x60 || io->base != 0x2f8 ||
	    io->size || io->flags != (IORESOURCE_IO | IORESOURCE_FIXED | IORESOURCE_ASSIGNED) ||
	    irq->index != 0x70 || irq->base != 3 || irq->size ||
	    irq->flags != (IORESOURCE_IRQ | IORESOURCE_FIXED | IORESOURCE_ASSIGNED))
		refusal("static AST resources");
	/* RAMstage device.h intentionally exposes mutable ops/chip_ops. Apply
	 * changes only after the complete three-node topology has been checked.
	 */
	nct2->chip_ops = nct3->chip_ops = ast3->chip_ops = &retained_pnp_chip;
	nct2->ops = nct3->ops = ast3->ops = &retained_pnp_ops;
	printk(BIOS_INFO, "NATIVE87 PNP RETAINED NCT2/3 OFF AST3 2f8/8 IRQ3\n");
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

/* Exact present-but-OFF endpoints need normal BAR allocation if their config
 * functions remain visible to the OS. These ops deliberately have no init,
 * scan, subsystem-ID, or ACPI callback: no ACP pin/wake/PME initialization.
 * Generic DEV_RESOURCES still probes/assigns real BARs and DEV_ENABLE writes
 * the normal PCI command bits; this is not a controller-initialized claim.
 */
#if !CONFIG(B565D4_DESKTOP_NATIVE)
struct device_operations native138_resource_only_ops = {
	.read_resources = pci_dev_read_resources,
	.set_resources = pci_dev_set_resources,
	.enable_resources = pci_dev_enable_resources,
};

void native138_prepare_pci_resources(void)
{
	static const struct {
		uint8_t parent, function;
		uint32_t id, class;
	} expected[] = {
		{ 0, 1, 0x16371002, 0x040300 }, /* Display audio */
		{ 0, 2, 0x15df1022, 0x108000 }, /* PSP/CCP */
		{ 0, 5, 0x15e21022, 0x040300 }, /* ACP */
		{ 0, 6, 0x15e31022, 0x040300 }, /* HDA */
		{ 1, 0, 0x16421022, 0x0d8000 }, /* Bus-C function0 */
		{ 1, 2, 0x16441022, 0x0d8000 }, /* Bus-C function2 */
	};
	struct device *const parents[] = { DEV_PTR(gpp_bridge_a), DEV_PTR(gpp_bridge_c) };
	struct device *const targets[] = {
		DEV_PTR(gfx_hda), DEV_PTR(crypto), DEV_PTR(acp), DEV_PTR(hda),
		DEV_PTR(dummy_function_c), DEV_PTR(i2s_ac97),
	};

	if (native84_profile_policy.entry_path != 2)
		return;
	if (!ecam_checked || !pnp_attempted)
		refusal("resource-only preparation order");
	/* Complete every parent/endpoint check before changing any device ops. */
	for (unsigned int i = 0; i < ARRAY_SIZE(parents); i++) {
		const struct device *bridge = parents[i];

		if (!bridge || bridge->path.type != DEVICE_PATH_PCI || !bridge->enabled ||
		    !bridge->upstream || bridge->upstream->secondary != 0 ||
		    bridge->upstream->segment_group != 0 ||
		    bridge->path.pci.devfn != PCI_DEVFN(8, i == 0 ? 1 : 3) ||
		    !bridge->downstream || bridge->downstream->dev != bridge ||
		    !bridge->downstream->secondary ||
		    pci_read_config32(bridge, PCI_VENDOR_ID) != 0x16351022)
			refusal("resource-only parent identity");
	}
	for (unsigned int i = 0; i < ARRAY_SIZE(targets); i++) {
		const struct device *dev = targets[i];
		const struct device *bridge = parents[expected[i].parent];
		uint32_t id, class;

		if (!dev || dev->path.type != DEVICE_PATH_PCI || dev->enabled ||
		    dev->hidden || dev->initialized || dev->resource_list || dev->downstream ||
		    dev->upstream != bridge->downstream ||
		    dev->path.pci.devfn != PCI_DEVFN(0, expected[i].function))
			refusal("resource-only endpoint shape");
		id = pci_read_config32(dev, PCI_VENDOR_ID);
		class = pci_read_config32(dev, PCI_CLASS_REVISION) >> 8;
		printk(BIOS_INFO, "NATIVE138 RESOURCE-ONLY PREFLIGHT %s id=%08x class=%06x\n",
		       dev_path(dev), id, class);
		uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
		if (id != expected[i].id ||
		    (class != expected[i].class &&
		     !(dev == DEV_PTR(acp) && id == 0x15e21022 && class == 0x048000)) ||
		    dev->vendor != (id & 0xffff) || dev->device != (id >> 16) ||
		    dev->class != class ||
		    (pci_read_config8(dev, PCI_HEADER_TYPE) & 0x7f) != PCI_HEADER_TYPE_NORMAL)
			refusal("resource-only endpoint identity");
	}
	for (unsigned int i = 0; i < ARRAY_SIZE(targets); i++) {
		targets[i]->ops = &native138_resource_only_ops;
		targets[i]->enabled = 1;
		printk(BIOS_INFO, "NATIVE138 RESOURCE-ONLY ENABLE %s; NO INIT CALLBACK\n",
		       dev_path(targets[i]));
	}
	uart_tx_flush(CONFIG_UART_FOR_CONSOLE);
}

#endif
