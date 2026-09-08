/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * B06VF is intentionally only a payload-entry experiment.  It reports the
 * memory ranges that this port has actually tested, does not enumerate PCI,
 * and opens the legacy C0000-FFFFF shadow only after the exact B06VF handoff
 * has survived into ramstage.
 */

#include <arch/pci_io_cfg.h>
#include <bootstate.h>
#include <cbmem.h>
#include <commonlib/helpers.h>
#include <console/console.h>
#include <device/device.h>
#include <device/pci_def.h>
#include <device/pci_type.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "b06v6_handoff.h"
#include "b06vf_payload.h"

#define B06VF_SAD_DEV		PCI_DEV(0xff, 0, 1)
#define B06VF_SAD_EXPECTED_ID	0x2d818086u
#define B06VF_PAM_FIRST		0x40
#define B06VF_PAM_COUNT		7

#define B06VF_SHADOW_BASE	0x000c0000u
#define B06VF_SHADOW_SIZE	0x00040000u
#define B06VF_SHADOW_BLOCK	0x00004000u
#define B06VF_SHADOW_BLOCKS	(B06VF_SHADOW_SIZE / B06VF_SHADOW_BLOCK)
#define B06VF_SEABIOS_LOAD_BASE	0x000f8800u
#define B06VF_SEABIOS_ENTRY_WORD	0x000fecd4u
#define B06VF_SHADOW_SPECIAL_WORDS	2
#define B06VF_SHADOW_PROBE_WORDS	(B06VF_SHADOW_BLOCKS * 2 + \
					 B06VF_SHADOW_SPECIAL_WORDS)

#define POST_B06VF_RESOURCES	0x22
#define POST_B06VF_TABLES	0x23
#define POST_B06VF_HANDOFF_FAIL	0x24
#define POST_B06VF_SAD_ID_FAIL	0x25
#define POST_B06VF_PAM_FAIL	0x26
#define POST_B06VF_SHADOW_FAIL	0x27
#define POST_B06VF_SHADOW_READY	0x28
#define POST_B06VF_PAYLOAD_LOADED	0x29
#define POST_B06VF_PAYLOAD_BOOT	0x2a

_Static_assert(B06VF_SHADOW_BLOCKS == 16,
	"B06VF must probe every 16-KiB PAM half-window");
_Static_assert(B06VF_SEABIOS_LOAD_BASE >= B06VF_SHADOW_BASE &&
	B06VF_SEABIOS_LOAD_BASE < B06VF_SHADOW_BASE + B06VF_SHADOW_SIZE,
	"SeaBIOS load base must be inside the PAM probe window");
_Static_assert(B06VF_SEABIOS_ENTRY_WORD >= B06VF_SHADOW_BASE &&
	B06VF_SEABIOS_ENTRY_WORD + sizeof(uint32_t) <=
		B06VF_SHADOW_BASE + B06VF_SHADOW_SIZE,
	"SeaBIOS entry word must be inside the PAM probe window");

static const uint8_t b06vf_pam_open[B06VF_PAM_COUNT] = {
	0x30, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
};

static void b06vf_fence(void)
{
	asm volatile ("mfence" ::: "memory");
}

static const struct x58_b06v6_handoff *b06vf_verified_handoff(void)
{
	const struct cbmem_entry *entry;
	const struct x58_b06v6_handoff *handoff;
	void *cbmem_base;
	size_t cbmem_size;
	uintptr_t cbmem_start;
	uintptr_t cbmem_end;

	if (!cbmem_online())
		die_with_post_code(POST_B06VF_HANDOFF_FAIL,
			"[PAYLOAD] B06VF CBMEM is offline\n");
	entry = cbmem_entry_find(X58_B06V6_CBMEM_ID);
	if (entry == NULL || cbmem_entry_size(entry) < sizeof(*handoff) ||
	    cbmem_get_region(&cbmem_base, &cbmem_size))
		die_with_post_code(POST_B06VF_HANDOFF_FAIL,
			"[PAYLOAD] B06VF handoff entry/CBMEM region is invalid\n");
	cbmem_start = (uintptr_t)cbmem_base;
	if (cbmem_start > UINTPTR_MAX - cbmem_size)
		die_with_post_code(POST_B06VF_HANDOFF_FAIL,
			"[PAYLOAD] B06VF CBMEM region wraps\n");
	cbmem_end = cbmem_start + cbmem_size;
	if (cbmem_size < sizeof(*handoff) ||
	    cbmem_start < X58_B06V6_CBMEM_BASE ||
	    cbmem_end != X58_B06V6_CBMEM_TOP || cbmem_end <= cbmem_start)
		die_with_post_code(POST_B06VF_HANDOFF_FAIL,
			"[PAYLOAD] B06VF CBMEM region escaped tested DRAM\n");
	handoff = cbmem_entry_start(entry);
	if ((uintptr_t)handoff < X58_B06V6_CBMEM_BASE ||
	    (uintptr_t)handoff >
		X58_B06V6_CBMEM_TOP - sizeof(*handoff) ||
	    (uintptr_t)handoff < cbmem_start ||
	    (uintptr_t)handoff > cbmem_end - sizeof(*handoff) ||
	    !x58_b06v6_handoff_is_valid(handoff) ||
	    !(handoff->flags & X58_B06VF_HANDOFF_LOWMEM_SMOKED))
		die_with_post_code(POST_B06VF_HANDOFF_FAIL,
			"[PAYLOAD] B06VF exact handoff/low-memory gate failed\n");

	return handoff;
}

static bool b06vf_restore_pam(const uint8_t before[B06VF_PAM_COUNT])
{
	bool exact = true;

	for (size_t i = 0; i < B06VF_PAM_COUNT; i++)
		pci_io_write_config8(B06VF_SAD_DEV, B06VF_PAM_FIRST + i,
			before[i]);
	b06vf_fence();
	for (size_t i = 0; i < B06VF_PAM_COUNT; i++)
		exact &= pci_io_read_config8(B06VF_SAD_DEV,
			B06VF_PAM_FIRST + i) == before[i];

	return exact;
}

struct b06vf_shadow_word {
	uintptr_t address;
	uint32_t original;
	uint32_t pattern;
};

static uintptr_t b06vf_shadow_probe_address(size_t index)
{
	if (index < B06VF_SHADOW_BLOCKS * 2) {
		const size_t block = index / 2;
		const uintptr_t first = B06VF_SHADOW_BASE +
			block * B06VF_SHADOW_BLOCK;

		return (index & 1) ?
			first + B06VF_SHADOW_BLOCK - sizeof(uint32_t) : first;
	}
	if (index == B06VF_SHADOW_BLOCKS * 2)
		return B06VF_SEABIOS_LOAD_BASE;
	return B06VF_SEABIOS_ENTRY_WORD;
}

static bool b06vf_restore_shadow(
	const struct b06vf_shadow_word words[B06VF_SHADOW_PROBE_WORDS])
{
	bool exact = true;

	for (size_t i = B06VF_SHADOW_PROBE_WORDS; i > 0; i--)
		*(volatile uint32_t *)words[i - 1].address = words[i - 1].original;
	b06vf_fence();
	for (size_t i = 0; i < B06VF_SHADOW_PROBE_WORDS; i++)
		exact &= *(volatile const uint32_t *)words[i].address ==
			words[i].original;

	return exact;
}

static bool b06vf_shadow_decode_probe(void)
{
	struct b06vf_shadow_word words[B06VF_SHADOW_PROBE_WORDS];
	bool exact = true;

	/*
	 * Keep two unique endpoints per PAM half-window plus the exact fixed
	 * SeaBIOS load base and aligned word containing its entry point resident
	 * simultaneously.  This exposes aliases before SELF overwrites F-segment.
	 */
	for (size_t i = 0; i < B06VF_SHADOW_PROBE_WORDS; i++) {
		words[i].address = b06vf_shadow_probe_address(i);
		words[i].original = *(volatile const uint32_t *)words[i].address;
		words[i].pattern = 0xb06f0000u ^ (uint32_t)words[i].address ^
			((uint32_t)(i + 1) * 0x01010101u);
	}
	for (size_t i = 0; i < B06VF_SHADOW_PROBE_WORDS; i++)
		*(volatile uint32_t *)words[i].address = words[i].pattern;
	b06vf_fence();
	for (size_t i = 0; i < B06VF_SHADOW_PROBE_WORDS; i++)
		exact &= *(volatile const uint32_t *)words[i].address ==
			words[i].pattern;

	return b06vf_restore_shadow(words) && exact;
}

static void b06vf_open_payload_shadow(void *unused)
{
	uint8_t before[B06VF_PAM_COUNT];
	uint8_t after[B06VF_PAM_COUNT];
	uint32_t id;

	(void)unused;
	(void)b06vf_verified_handoff();
	id = pci_io_read_config32(B06VF_SAD_DEV, PCI_VENDOR_ID);
	if (id != B06VF_SAD_EXPECTED_ID)
		die_with_post_code(POST_B06VF_SAD_ID_FAIL,
			"[PAYLOAD] B06VF SAD ID mismatch: %08x\n", id);

	for (size_t i = 0; i < B06VF_PAM_COUNT; i++)
		before[i] = pci_io_read_config8(B06VF_SAD_DEV,
			B06VF_PAM_FIRST + i);
	printk(BIOS_NOTICE,
	       "[PAYLOAD] PAM PRE=%02x/%02x/%02x/%02x/%02x/%02x/%02x\n",
	       before[0], before[1], before[2], before[3], before[4], before[5],
	       before[6]);

	for (size_t i = 0; i < B06VF_PAM_COUNT; i++)
		pci_io_write_config8(B06VF_SAD_DEV, B06VF_PAM_FIRST + i,
			b06vf_pam_open[i]);
	b06vf_fence();
	for (size_t i = 0; i < B06VF_PAM_COUNT; i++)
		after[i] = pci_io_read_config8(B06VF_SAD_DEV,
			B06VF_PAM_FIRST + i);
	if (memcmp(after, b06vf_pam_open, sizeof(after))) {
		if (!b06vf_restore_pam(before))
			die_with_post_code(POST_B06VF_PAM_FAIL,
				"[PAYLOAD] B06VF PAM write and rollback both mismatched\n");
		die_with_post_code(POST_B06VF_PAM_FAIL,
			"[PAYLOAD] B06VF PAM readback mismatch\n");
	}
	printk(BIOS_NOTICE,
	       "[PAYLOAD] PAM POST=%02x/%02x/%02x/%02x/%02x/%02x/%02x\n",
	       after[0], after[1], after[2], after[3], after[4], after[5],
	       after[6]);

	if (!b06vf_shadow_decode_probe()) {
		if (!b06vf_restore_pam(before))
			die_with_post_code(POST_B06VF_PAM_FAIL,
				"[PAYLOAD] B06VF shadow probe failed and PAM rollback mismatched\n");
		die_with_post_code(POST_B06VF_SHADOW_FAIL,
			"[PAYLOAD] B06VF C0000-FFFFF decode/alias/restore probe failed\n");
	}
	post_code(POST_B06VF_SHADOW_READY);
	printk(BIOS_NOTICE,
	       "[PAYLOAD] C0000-FFFFF PAM decode verified; left read/write for SELF loader\n");
}

static void b06vf_read_resources(struct device *dev)
{
	/* Only ranges proven by the B06VF lowmem and inherited B06VE tests. */
	ram_range(dev, 0, 0x00000000, 0x000a0000);
	mmio_range(dev, 1, 0x000a0000, 0x00020000);
	reserved_ram_range(dev, 2, 0x000c0000, 0x00040000);
	ram_range(dev, 3, X58_B06V6_CBMEM_BASE, X58_B06V6_CBMEM_SIZE);
	ram_range(dev, 4, X58_B06V6_OBJECT_BASE, X58_B06V6_OBJECT_SIZE);
	post_code(POST_B06VF_RESOURCES);
	printk(BIOS_NOTICE,
	       "[PAYLOAD] conservative RAM map: 0-640K, 16-24MiB, 32-40MiB\n");
}

static struct device_operations b06vf_domain_ops = {
	.read_resources = b06vf_read_resources,
	.set_resources = noop_set_resources,
};

static struct device_operations b06vf_cpu_cluster_ops = {
	.read_resources = noop_read_resources,
	.set_resources = noop_set_resources,
};

void x58_b06vf_enable_dev(struct device *dev)
{
	if (dev->path.type == DEVICE_PATH_DOMAIN)
		dev->ops = &b06vf_domain_ops;
	else if (dev->path.type == DEVICE_PATH_CPU_CLUSTER)
		dev->ops = &b06vf_cpu_cluster_ops;
}

static void b06vf_tables_written(void *unused)
{
	(void)unused;
	post_code(POST_B06VF_TABLES);
	printk(BIOS_NOTICE, "[PAYLOAD] coreboot tables written\n");
}

static void b06vf_payload_loaded(void *unused)
{
	(void)unused;
	post_code(POST_B06VF_PAYLOAD_LOADED);
	printk(BIOS_NOTICE, "[PAYLOAD] SeaBIOS SELF image loaded\n");
}

static void b06vf_payload_boot(void *unused)
{
	(void)unused;
	post_code(POST_B06VF_PAYLOAD_BOOT);
	printk(BIOS_NOTICE, "[PAYLOAD] entering SeaBIOS payload\n");
}

BOOT_STATE_INIT_ENTRY(BS_WRITE_TABLES, BS_ON_EXIT, b06vf_tables_written, NULL);
BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_LOAD, BS_ON_ENTRY, b06vf_open_payload_shadow, NULL);
BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_LOAD, BS_ON_EXIT, b06vf_payload_loaded, NULL);
BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_BOOT, BS_ON_ENTRY, b06vf_payload_boot, NULL);
