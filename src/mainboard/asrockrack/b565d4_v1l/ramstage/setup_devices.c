/* SPDX-License-Identifier: GPL-2.0-only */

#include <acpi/acpi.h>
#include <amdblocks/iommu.h>
#include <bootstate.h>
#include <console/console.h>
#include <device/device.h>
#include <device/pci.h>
#include <device/pciexp.h>
#include <static.h>
#include "setup_options.h"

#if !CONFIG(B565D4_DESKTOP_NATIVE)
void __real_amd_cpu_init(struct device *dev);
void __wrap_amd_cpu_init(struct device *dev);
void __wrap_amd_cpu_init(struct device *dev)
{
	__real_amd_cpu_init(dev);
	b565d4_setup_cpu();
}

void b565d4_setup_iommu(void)
{
	if (!CONFIG(B565D4_BOARD_SETUP) || b565d4_setup_get(B565D4_OPT_IOMMU))
		return;
	struct device *dev = DEV_PTR(iommu);
	uint32_t base = pci_read_config32(dev, IOMMU_CAP_BASE_LO);

	/* Before enumeration and before any OS-owned translation state. Leave
	 * the PCI capability visible; suppress both MMIO decode and IVRS.
	 */
	pci_write_config32(dev, IOMMU_CAP_BASE_LO, base & ~IOMMU_ENABLE);
	dev->enabled = false;
	printk(BIOS_NOTICE, "Board setup: IOMMU disabled, cap base=%08x\n",
	       pci_read_config32(dev, IOMMU_CAP_BASE_LO));
}

unsigned long __real_acpi_add_ivrs_table(unsigned long current, acpi_rsdp_t *rsdp);
unsigned long __wrap_acpi_add_ivrs_table(unsigned long current, acpi_rsdp_t *rsdp);
unsigned long __wrap_acpi_add_ivrs_table(unsigned long current, acpi_rsdp_t *rsdp)
{
	if (CONFIG(B565D4_BOARD_SETUP) && !b565d4_setup_get(B565D4_OPT_IOMMU))
		return current;
	return __real_acpi_add_ivrs_table(current, rsdp);
}

#endif

/* Classify ports by the board wiring, not an endpoint's current BDF. The
 * chipset switch can be renumbered when CPU slot topology changes.
 */
static int slot_policy(const struct device *dev)
{
	if (!dev->upstream || dev->path.type != DEVICE_PATH_PCI)
		return -1;
	if (dev->upstream->secondary == 0) {
		unsigned int fn = dev->path.pci.devfn;

		if (CONFIG(B565D4_DESKTOP_NATIVE)) {
			if (fn == PCI_DEVFN(1, 1))
				return 2; /* PCIE4, separate CPU general-purpose lanes. */
			if (fn >= PCI_DEVFN(3, 1) && fn <= PCI_DEVFN(3, 4))
				return 0; /* PCIE6, including the four split functions. */
		} else {
			if (fn == PCI_DEVFN(2, 2))
				return 2; /* PCIE4, general-purpose CPU lanes. */
			if ((fn >= PCI_DEVFN(1, 1) && fn <= PCI_DEVFN(1, 3)) ||
			    (fn >= PCI_DEVFN(2, 3) && fn <= PCI_DEVFN(2, 7)))
				return 0; /* CPU x16 slot, including split functions. */
		}
	}
	const struct device *upstream = dev->upstream->dev;

	if (upstream && upstream->vendor == 0x1022 && upstream->device == 0x43e9) {
		if (dev->path.pci.devfn == PCI_DEVFN(4, 0))
			return 1; /* PCIE3. */
		if (dev->path.pci.devfn == PCI_DEVFN(0, 0))
			return 3; /* PCIE5. */
	}
	return -1;
}

static void set_link_speed(struct device *dev, unsigned int cap, unsigned int speed)
{
	if (!speed || !cap ||
	    (pci_read_config16(dev, cap + PCI_EXP_FLAGS) & PCI_EXP_FLAGS_VERS) < 2)
		return;
	uint32_t capabilities = pci_read_config32(dev, cap + PCI_EXP_LNKCAP);
	unsigned int maximum = capabilities & 0xf;
	unsigned int requested = speed < maximum ? speed : maximum;
	uint16_t status = pci_read_config16(dev, cap + PCI_EXP_LNKSTA);

	if (!maximum)
		return;
	pci_update_config16(dev, cap + PCI_EXP_LNKCTL2,
			    (uint16_t)~PCI_EXP_LNKCTL2_TLS, requested);
	int result = 0;

	bool active = capabilities & PCI_EXP_LNKCAP_DLLLARC ?
		status & PCI_EXP_LNKSTA_DLLLA : status & PCI_EXP_LNKSTA_NLW;

	/* An empty port can retain a nominal width while DLLLA is clear.
	 * Program its future speed limit, but do not retrain a disconnected link.
	 */
	if (active && (status & PCI_EXP_LNKSTA_CLS) != requested)
		result = pciexp_retrain_link(dev, cap);
	printk(BIOS_NOTICE, "Board setup: %s speed requested=%u target=%u status=%04x rc=%d\n",
	       dev_path(dev), speed, requested,
	       pci_read_config16(dev, cap + PCI_EXP_LNKSTA), result);
}

static void set_link_aspm(struct device *dev, unsigned int cap, unsigned int policy)
{
	if (!policy || !cap || !dev->downstream)
		return; /* OS-controlled policy leaves the link unchanged. */
	unsigned int enable = policy == 2 ? PCIE_ASPM_L1 : PCIE_ASPM_NONE;
	bool endpoint_seen = false;
	struct device *child;

	/* Reuse the common capability and exit-latency checks on every function.
	 * This permits a requested slot without enabling the global ASPM tuner.
	 */
	for (child = dev->downstream->children; child; child = child->sibling) {
		unsigned int end_cap = pci_find_capability(child, PCI_CAP_ID_PCIE);

		if (end_cap) {
			endpoint_seen = true;
			enable &= pciexp_get_aspm_allowed(dev, cap, child, end_cap);
		}
	}
	if (!endpoint_seen)
		enable = PCIE_ASPM_NONE;

	/* Enable root first; disable endpoint first, as in the common tuner. */
	if (enable)
		pci_update_config16(dev, cap + PCI_EXP_LNKCTL, 0xfffc, enable);
	for (child = dev->downstream->children; child; child = child->sibling) {
		unsigned int end_cap = pci_find_capability(child, PCI_CAP_ID_PCIE);

		if (end_cap)
			pci_update_config16(child, end_cap + PCI_EXP_LNKCTL, 0xfffc, enable);
	}
	if (!enable)
		pci_update_config16(dev, cap + PCI_EXP_LNKCTL, 0xfffc, enable);
	printk(BIOS_NOTICE, "Board setup: %s ASPM policy=%u effective=%u (OS may change)\n",
	       dev_path(dev), policy, enable);
}

static void set_ari_forwarding(struct device *dev, unsigned int cap, bool before_scan)
{
	if (!cap || !dev->downstream)
		return;
	uint16_t flags = pci_read_config16(dev, cap + PCI_EXP_FLAGS);
	unsigned int type = (flags & PCI_EXP_FLAGS_TYPE) >> 4;

	if ((flags & PCI_EXP_FLAGS_VERS) < 2 ||
	    (type != PCI_EXP_TYPE_ROOT_PORT && type != PCI_EXP_TYPE_DOWNSTREAM) ||
	    !(pci_read_config32(dev, cap + PCI_EXP_DEVCAP2) & PCI_EXP_DEVCAP2_ARI))
		return;
	uint16_t before = pci_read_config16(dev, cap + PCI_EXP_DEVCTL2);
	bool requested = b565d4_setup_get(B565D4_OPT_ARI);

	/* Disable before enumeration, while no enumerated function depends on
	 * ARI routing. An enabled hierarchy can legitimately have functions >7.
	 */
	if (before_scan) {
		if (!requested && (before & PCI_EXP_DEVCTL2_ARI))
			pci_update_config16(dev, cap + PCI_EXP_DEVCTL2,
					    (uint16_t)~PCI_EXP_DEVCTL2_ARI, 0);
		return;
	}
	if (!requested)
		return;
	bool endpoint_ari = false;
	bool conventional_device = false;

	for (struct device *child = dev->downstream->children; child; child = child->sibling) {
		if (!child->enabled || child->path.type != DEVICE_PATH_PCI)
			continue;
		if (!(before & PCI_EXP_DEVCTL2_ARI) && (child->path.pci.devfn >> 3))
			conventional_device = true;
		if (!child->path.pci.devfn &&
		    pciexp_find_extended_cap(child, PCIE_EXT_CAP_ARI_ID, 0))
			endpoint_ari = true;
	}
	bool enable = endpoint_ari && !conventional_device;
	uint16_t wanted = (before & ~PCI_EXP_DEVCTL2_ARI) |
		(enable ? PCI_EXP_DEVCTL2_ARI : 0);

	/* Firmware does not enable VFs. Linux retains ownership of VF creation
	 * and ARI configuration. Never reinterpret a conventional device number
	 * as an ARI function or enable forwarding without both capabilities.
	 */
	if (wanted != before)
		pci_update_config16(dev, cap + PCI_EXP_DEVCTL2,
				    (uint16_t)~PCI_EXP_DEVCTL2_ARI, wanted & PCI_EXP_DEVCTL2_ARI);
	printk(BIOS_NOTICE, "Board setup: %s ARI requested=%u endpoint=%u forwarding=%u\n",
	       dev_path(dev), b565d4_setup_get(B565D4_OPT_ARI), endpoint_ari, enable);
}

void __real_do_pci_scan_bridge(struct device *dev,
	void (*scan_bus)(struct bus *, unsigned int, unsigned int));
void __wrap_do_pci_scan_bridge(struct device *dev,
	void (*scan_bus)(struct bus *, unsigned int, unsigned int));
void __wrap_do_pci_scan_bridge(struct device *dev,
	void (*scan_bus)(struct bus *, unsigned int, unsigned int))
{
	int slot = slot_policy(dev);
	unsigned int cap = pci_find_capability(dev, PCI_CAP_ID_PCIE);

	/* Desktop CPU links use the already bound native SMU service. Standard
	 * PCIe TLS/retrain remains appropriate for the PCH downstream slots.
	 */
	if (slot >= 0 && !(CONFIG(B565D4_DESKTOP_NATIVE) &&
			  dev->upstream->secondary == 0))
		set_link_speed(dev, cap, b565d4_setup_get(B565D4_OPT_CPU_SPEED + slot));
	set_ari_forwarding(dev, cap, true);
	__real_do_pci_scan_bridge(dev, scan_bus);
	set_ari_forwarding(dev, cap, false);
	if (slot >= 0 && !CONFIG(B565D4_DESKTOP_NATIVE))
		set_link_aspm(dev, cap, b565d4_setup_get(B565D4_OPT_CPU_ASPM + slot));
}

#if CONFIG(B565D4_DESKTOP_SETUP)
static void desktop_link_policy_exit(void *unused)
{
	/* A parent bus can tune a child bridge after its own scan callback.
	 * Apply the desktop ASPM restriction after all scan and device init,
	 * using only the states already qualified on both link ends.
	 */
	for (struct device *dev = all_devices; dev; dev = dev->next) {
		if (!dev->enabled || dev->path.type != DEVICE_PATH_PCI)
			continue;
		int slot = slot_policy(dev);

		if (slot >= 0)
			set_link_aspm(dev, pci_find_capability(dev, PCI_CAP_ID_PCIE),
				      b565d4_setup_get(B565D4_OPT_CPU_ASPM + slot));
	}
}
BOOT_STATE_INIT_ENTRY(BS_DEV_INIT, BS_ON_EXIT, desktop_link_policy_exit, NULL);
#endif
