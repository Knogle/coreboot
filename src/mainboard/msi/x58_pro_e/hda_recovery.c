/* SPDX-License-Identifier: GPL-2.0-only */
#include <arch/io.h>
#include <console/console.h>
#include <device/azalia_device.h>
#include <device/device.h>
#include <device/mmio.h>
#include <device/pci.h>
#include <device/pci_ops.h>
#include <string.h>
#include "hda_recovery.h"

_Static_assert(CONFIG(HAVE_MONOTONIC_TIMER) &&
	!CONFIG(NO_MONOTONIC_TIMER) && CONFIG(NO_SMM),
	"ICH10 requires the full ICH10 monotonic/no-SMM recovery profile");

/* ICH10 datasheet 319973: 18.1.20, 18.1.23, 18.1.40, 18.1.43.
 * These are HDA PCI configuration offsets, not controller BAR offsets.
 * VCi ID/map are software-visible but not used by ICH10 hardware. Neither
 * VCiSTS nor VC0STS negotiation bits prove a working physical virtual channel.
 */
#define HDA_TCSEL 0x44
#define HDA_PCS 0x54
#define HDA_VC0CTL 0x114
#define HDA_VCICTL 0x120
#define HDA_TC_MASK 7u
#define HDA_POWER_MASK 3u
#define HDA_VC_ENABLE (1u << 31)
#define HDA_VCI_ID_MAP ((7u << 24) | 0xfeu)
#define HDA_BAR_SIZE 0x4000u

static bool requested;
static bool frozen;
static bool consumed;
static bool tracing;

void x58_hda_status(void)
{
	printk(BIOS_NOTICE, "[HDA] POLICY=%s FROZEN=%u CONSUMED=%u "
	       "PERSISTENCE=RAM RESET_DEFAULT=defer\n",
	       requested ? "vc0-once" : "defer", frozen, consumed);
}

bool x58_hda_command(int argc, char **argv)
{
	if (argc < 1 || strcmp(argv[0], "hda"))
		return false;
	if (argc == 1) {
		x58_hda_status();
	} else if (argc != 2 || (strcmp(argv[1], "defer") && strcmp(argv[1], "vc0-once"))) {
		printk(BIOS_NOTICE, "[HDA] hda [defer|vc0-once]; PRE-DEVICE only\n");
	} else if (frozen || consumed) {
		printk(BIOS_WARNING, "[HDA] selection frozen; next reset defaults to defer\n");
	} else {
		requested = !strcmp(argv[1], "vc0-once");
		x58_hda_status();
	}
	return true;
}

void x58_hda_freeze(void)
{
	frozen = true;
	x58_hda_status();
}

void azalia_reset_trace(const u8 *base, bool reset, const char *step, u32 value)
{
	if (!tracing)
		return;
	/* Distinct access-entry/return text is authoritative; POST d6 marks the
	 * diagnostic region. This hook performs no HDA or timer access. */
	outb(0xd6, CONFIG_POST_IO_PORT);
	printk(BIOS_NOTICE, "[HDA] %s BASE=%p RESET=%u VALUE=%08x\n",
	       step, base, reset, value);
}

static bool mainboard_ich10_hda_begin(struct device *dev)
{
	bool run = frozen && requested && !consumed;
	/* Consume before even PCI configuration reads. No persistent selection can
	 * replay the attempt after a MMIO hang and external cold recovery. */
	requested = false;
	consumed = true;
	if (!run) {
		printk(BIOS_NOTICE, "[HDA] DEFER before init writes/MMIO; "
		       "PCI function/resources retained for OS\n");
		return false;
	}
	const u32 id = pci_read_config32(dev, PCI_VENDOR_ID);
	const u16 command = pci_read_config16(dev, PCI_COMMAND);
	const u16 pcs = pci_read_config16(dev, HDA_PCS);
	const u32 bar = pci_read_config32(dev, PCI_BASE_ADDRESS_0);
	const u32 bar_high = pci_read_config32(dev, PCI_BASE_ADDRESS_1);
	const u8 tcsel = pci_read_config8(dev, HDA_TCSEL);
	const u32 vc0 = pci_read_config32(dev, HDA_VC0CTL);
	const u32 vci = pci_read_config32(dev, HDA_VCICTL);
	const struct resource *res = probe_resource(dev, PCI_BASE_ADDRESS_0);
	printk(BIOS_NOTICE, "[HDA] PRE ID=%08x CMD=%04x PCS=%04x "
	       "BAR=%08x:%08x TCSEL=%02x VC0=%08x VCI=%08x\n",
	       id, command, pcs, bar_high, bar, tcsel, vc0, vci);
	/* D3->D0 has reset side effects. Do not manufacture a D0 transition, touch
	 * unknown BARs, or remap a controller already allowed to DMA. Section
	 * 18.1.12 defines HDBAR as64-bit (low nibble4), with HDBARU atBAR1. */
	if (id != 0x3a3e8086u || !dev->upstream || dev->upstream->secondary != 0 ||
	    dev->path.pci.devfn != PCI_DEVFN(0x1b, 0) || !dev->enabled ||
	    (pcs & HDA_POWER_MASK) || !(command & PCI_COMMAND_MEMORY) ||
	    (command & PCI_COMMAND_MASTER) || !res || res->size != HDA_BAR_SIZE ||
	    !(res->flags & IORESOURCE_MEM) || !(res->flags & IORESOURCE_ASSIGNED) ||
	    res->base < 0xc0000000ULL || res->base > 0xe0000000ULL - HDA_BAR_SIZE ||
	    (bar & ~0xfu) != res->base || (bar & 0xf) != 4 || bar_high ||
	    !(vc0 & HDA_VC_ENABLE)) {
		printk(BIOS_WARNING, "[HDA] TEST_DECLINED before writes/MMIO; OS owns init\n");
		return false;
	}
	const u32 target_vci = vci & ~(HDA_VC_ENABLE | HDA_VCI_ID_MAP);
	const u32 target_vc0 = vc0 | 0xffu;
	const u8 target_tc = tcsel & ~HDA_TC_MASK;
	/* Explicit VC0-only path. Preserve unrelated/reserved bits; do not
	 * transplant X4x/PCH northbridge registers into this X58 platform. */
	printk(BIOS_NOTICE, "[HDA] VC0_CONFIG_ENTER\n");
	pci_write_config32(dev, HDA_VCICTL, target_vci);
	pci_write_config8(dev, HDA_TCSEL, target_tc);
	pci_write_config32(dev, HDA_VC0CTL, target_vc0);
	if (pci_read_config32(dev, HDA_VCICTL) != target_vci ||
	    pci_read_config8(dev, HDA_TCSEL) != target_tc ||
	    pci_read_config32(dev, HDA_VC0CTL) != target_vc0)
		die("[HDA] VC0 readback failed after write; cold recovery required\n");
	tracing = true;
	azalia_reset_trace(NULL, false, "VC0_CONFIG_RETURN", target_vc0);
	return true;
}

static void mainboard_ich10_hda_end(void)
{
	azalia_reset_trace(NULL, false, "DRIVER_RETURN", 0);
	tracing = false;
}

static u16 x58_ich1b_codec_detect(u8 *base)
{
	u16 statests;

	if (azalia_enter_reset(base) != CB_SUCCESS)
		goto no_codec;

	if (azalia_exit_reset(base) != CB_SUCCESS)
		goto no_codec;

	/* ICH10 datasheet 319973, section 18.2.8: STATESTS is a 16-bit
	 * naturally aligned register at 0x0e. Do not issue a 32-bit access. */
	azalia_reset_trace(base, false, "STATESTS_READ_ENTER", 0);
	statests = read16(base + HDA_STATESTS_REG);
	azalia_reset_trace(base, false, "STATESTS_READ_RETURN", statests);
	statests &= 0x0f;
	if (statests)
		return statests;

no_codec:
	azalia_enter_reset(base);
	printk(BIOS_DEBUG, "Azalia: No codec!\n");
	return 0;
}

void x58_ich1b_hda_init(struct device *dev)
{
	struct resource *res;
	u8 *base;
	u16 codec_mask;

	/* Must precede ESD/VC writes, BME, R/WO fields, and controller MMIO. */
	if (!mainboard_ich10_hda_begin(dev))
		return;
	azalia_reset_trace(NULL, false, "PCI_SETUP_ENTER", 0);

	/* Keep the pre-existing controller sequence. It deliberately
	 * omits the stock VC and BME writes: the board's one-shot diagnostic
	 * owns VC0 and leaves DMA engines to the OS. */
	pci_update_config32(dev, 0x134, ~0x00ff0000, 2 << 16);
	pci_update_config32(dev, 0x140, ~0x00ff0000, 2 << 16);
	pci_and_config8(dev, 0x4d, (u8)~(1 << 7));
	pci_update_config32(dev, 0x74, ~0, 0);
	azalia_reset_trace(NULL, false, "PCI_SETUP_RETURN", 0);

	res = probe_resource(dev, PCI_BASE_ADDRESS_0);
	if (!res)
		goto out;

	base = res2mmio(res, 0, 0);
	printk(BIOS_DEBUG, "Azalia: base = %p\n", base);
	codec_mask = x58_ich1b_codec_detect(base);
	if (codec_mask) {
		printk(BIOS_DEBUG, "Azalia: codec_mask = %02x\n", codec_mask);
		azalia_reset_trace(base, false, "CODECS_ENTER", codec_mask);
		azalia_codecs_init(base, codec_mask);
	}

out:
	mainboard_ich10_hda_end();
}
