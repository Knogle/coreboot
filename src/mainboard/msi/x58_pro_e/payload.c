/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * X58_MEMORY_HANDOFF is intentionally only a payload-entry path.  It reports the
 * memory ranges that this port has actually tested, does not enumerate PCI,
 * and opens the legacy C0000-FFFFF shadow only after the exact X58_MEMORY_HANDOFF handoff
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

#include "raminit_handoff.h"
#include "payload.h"

#define X58_MEMORY_HANDOFF_SAD_DEV		PCI_DEV(0xff, 0, 1)
#define X58_MEMORY_HANDOFF_SAD_EXPECTED_ID	0x2d818086u
#define X58_MEMORY_HANDOFF_PAM_FIRST		0x40
#define X58_MEMORY_HANDOFF_PAM_COUNT		7

#define X58_MEMORY_HANDOFF_SHADOW_BASE	0x000c0000u
#define X58_MEMORY_HANDOFF_SHADOW_SIZE	0x00040000u
#define X58_MEMORY_HANDOFF_SHADOW_BLOCK	0x00004000u
#define X58_MEMORY_HANDOFF_SHADOW_BLOCKS	(X58_MEMORY_HANDOFF_SHADOW_SIZE / X58_MEMORY_HANDOFF_SHADOW_BLOCK)
#define X58_MEMORY_HANDOFF_SEABIOS_LOAD_BASE	0x000f8800u
#define X58_MEMORY_HANDOFF_SEABIOS_ENTRY_WORD	0x000fecd4u
#define X58_MEMORY_HANDOFF_SHADOW_SPECIAL_WORDS	2
#define X58_MEMORY_HANDOFF_SHADOW_PROBE_WORDS	(X58_MEMORY_HANDOFF_SHADOW_BLOCKS * 2 + \
					 X58_MEMORY_HANDOFF_SHADOW_SPECIAL_WORDS)

#define POST_X58_MEMORY_HANDOFF_RESOURCES	0x22
#define POST_X58_MEMORY_HANDOFF_TABLES	0x23
#define POST_X58_MEMORY_HANDOFF_FAIL	0x24
#define POST_X58_MEMORY_HANDOFF_SAD_ID_FAIL	0x25
#define POST_X58_MEMORY_HANDOFF_PAM_FAIL	0x26
#define POST_X58_MEMORY_HANDOFF_SHADOW_FAIL	0x27
#define POST_X58_MEMORY_HANDOFF_SHADOW_READY	0x28
#define POST_X58_MEMORY_HANDOFF_PAYLOAD_LOADED	0x29
#define POST_X58_MEMORY_HANDOFF_PAYLOAD_BOOT	0x2a

_Static_assert(X58_MEMORY_HANDOFF_SHADOW_BLOCKS == 16,
	"X58_MEMORY_HANDOFF must probe every 16-KiB PAM half-window");
_Static_assert(X58_MEMORY_HANDOFF_SEABIOS_LOAD_BASE >= X58_MEMORY_HANDOFF_SHADOW_BASE &&
	X58_MEMORY_HANDOFF_SEABIOS_LOAD_BASE < X58_MEMORY_HANDOFF_SHADOW_BASE + X58_MEMORY_HANDOFF_SHADOW_SIZE,
	"SeaBIOS load base must be inside the PAM probe window");
_Static_assert(X58_MEMORY_HANDOFF_SEABIOS_ENTRY_WORD >= X58_MEMORY_HANDOFF_SHADOW_BASE &&
	X58_MEMORY_HANDOFF_SEABIOS_ENTRY_WORD + sizeof(uint32_t) <=
		X58_MEMORY_HANDOFF_SHADOW_BASE + X58_MEMORY_HANDOFF_SHADOW_SIZE,
	"SeaBIOS entry word must be inside the PAM probe window");

static const uint8_t x58_memory_handoff_pam_open[X58_MEMORY_HANDOFF_PAM_COUNT] = {
	0x30, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
};

static void x58_memory_handoff_fence(void)
{
	asm volatile ("mfence" ::: "memory");
}

static const struct x58_raminit_handoff *x58_memory_handoff_verified_handoff(void)
{
	const struct cbmem_entry *entry;
	const struct x58_raminit_handoff *handoff;
	void *cbmem_base;
	size_t cbmem_size;
	uintptr_t cbmem_start;
	uintptr_t cbmem_end;

	if (!cbmem_online())
		die_with_post_code(POST_X58_MEMORY_HANDOFF_FAIL,
			"[PAYLOAD] X58_MEMORY_HANDOFF CBMEM is offline\n");
	entry = cbmem_entry_find(X58_RAMINIT_CBMEM_ID);
	if (entry == NULL || cbmem_entry_size(entry) < sizeof(*handoff) ||
	    cbmem_get_region(&cbmem_base, &cbmem_size))
		die_with_post_code(POST_X58_MEMORY_HANDOFF_FAIL,
			"[PAYLOAD] X58_MEMORY_HANDOFF handoff entry/CBMEM region is invalid\n");
	cbmem_start = (uintptr_t)cbmem_base;
	if (cbmem_start > UINTPTR_MAX - cbmem_size)
		die_with_post_code(POST_X58_MEMORY_HANDOFF_FAIL,
			"[PAYLOAD] X58_MEMORY_HANDOFF CBMEM region wraps\n");
	cbmem_end = cbmem_start + cbmem_size;
	if (cbmem_size < sizeof(*handoff) ||
	    cbmem_start < X58_RAMINIT_CBMEM_BASE ||
	    cbmem_end != X58_RAMINIT_CBMEM_TOP || cbmem_end <= cbmem_start)
		die_with_post_code(POST_X58_MEMORY_HANDOFF_FAIL,
			"[PAYLOAD] X58_MEMORY_HANDOFF CBMEM region escaped tested DRAM\n");
	handoff = cbmem_entry_start(entry);
	if ((uintptr_t)handoff < X58_RAMINIT_CBMEM_BASE ||
	    (uintptr_t)handoff >
		X58_RAMINIT_CBMEM_TOP - sizeof(*handoff) ||
	    (uintptr_t)handoff < cbmem_start ||
	    (uintptr_t)handoff > cbmem_end - sizeof(*handoff) ||
	    !x58_raminit_handoff_is_valid(handoff) ||
	    !(handoff->flags & X58_MEMORY_HANDOFF_LOWMEM_SMOKED))
		die_with_post_code(POST_X58_MEMORY_HANDOFF_FAIL,
			"[PAYLOAD] X58_MEMORY_HANDOFF exact handoff/low-memory gate failed\n");

	return handoff;
}

static bool x58_memory_handoff_restore_pam(const uint8_t before[X58_MEMORY_HANDOFF_PAM_COUNT])
{
	bool exact = true;

	for (size_t i = 0; i < X58_MEMORY_HANDOFF_PAM_COUNT; i++)
		pci_io_write_config8(X58_MEMORY_HANDOFF_SAD_DEV, X58_MEMORY_HANDOFF_PAM_FIRST + i,
			before[i]);
	x58_memory_handoff_fence();
	for (size_t i = 0; i < X58_MEMORY_HANDOFF_PAM_COUNT; i++)
		exact &= pci_io_read_config8(X58_MEMORY_HANDOFF_SAD_DEV,
			X58_MEMORY_HANDOFF_PAM_FIRST + i) == before[i];

	return exact;
}

struct x58_memory_handoff_shadow_word {
	uintptr_t address;
	uint32_t original;
	uint32_t pattern;
};

static uintptr_t x58_memory_handoff_shadow_probe_address(size_t index)
{
	if (index < X58_MEMORY_HANDOFF_SHADOW_BLOCKS * 2) {
		const size_t block = index / 2;
		const uintptr_t first = X58_MEMORY_HANDOFF_SHADOW_BASE +
			block * X58_MEMORY_HANDOFF_SHADOW_BLOCK;

		return (index & 1) ?
			first + X58_MEMORY_HANDOFF_SHADOW_BLOCK - sizeof(uint32_t) : first;
	}
	if (index == X58_MEMORY_HANDOFF_SHADOW_BLOCKS * 2)
		return X58_MEMORY_HANDOFF_SEABIOS_LOAD_BASE;
	return X58_MEMORY_HANDOFF_SEABIOS_ENTRY_WORD;
}

static bool x58_memory_handoff_restore_shadow(
	const struct x58_memory_handoff_shadow_word words[X58_MEMORY_HANDOFF_SHADOW_PROBE_WORDS])
{
	bool exact = true;

	for (size_t i = X58_MEMORY_HANDOFF_SHADOW_PROBE_WORDS; i > 0; i--)
		*(volatile uint32_t *)words[i - 1].address = words[i - 1].original;
	x58_memory_handoff_fence();
	for (size_t i = 0; i < X58_MEMORY_HANDOFF_SHADOW_PROBE_WORDS; i++)
		exact &= *(volatile const uint32_t *)words[i].address ==
			words[i].original;

	return exact;
}

static bool x58_memory_handoff_shadow_decode_probe(void)
{
	struct x58_memory_handoff_shadow_word words[X58_MEMORY_HANDOFF_SHADOW_PROBE_WORDS];
	bool exact = true;

	/*
	 * Keep two unique endpoints per PAM half-window plus the exact fixed
	 * SeaBIOS load base and aligned word containing its entry point resident
	 * simultaneously.  This exposes aliases before SELF overwrites F-segment.
	 */
	for (size_t i = 0; i < X58_MEMORY_HANDOFF_SHADOW_PROBE_WORDS; i++) {
		words[i].address = x58_memory_handoff_shadow_probe_address(i);
		words[i].original = *(volatile const uint32_t *)words[i].address;
		words[i].pattern = 0x7522f000u ^ (uint32_t)words[i].address ^
			((uint32_t)(i + 1) * 0x01010101u);
	}
	for (size_t i = 0; i < X58_MEMORY_HANDOFF_SHADOW_PROBE_WORDS; i++)
		*(volatile uint32_t *)words[i].address = words[i].pattern;
	x58_memory_handoff_fence();
	for (size_t i = 0; i < X58_MEMORY_HANDOFF_SHADOW_PROBE_WORDS; i++)
		exact &= *(volatile const uint32_t *)words[i].address ==
			words[i].pattern;

	return x58_memory_handoff_restore_shadow(words) && exact;
}

static void x58_memory_handoff_open_payload_shadow(void *unused)
{
	uint8_t before[X58_MEMORY_HANDOFF_PAM_COUNT];
	uint8_t after[X58_MEMORY_HANDOFF_PAM_COUNT];
	uint32_t id;

	(void)unused;
	(void)x58_memory_handoff_verified_handoff();
	id = pci_io_read_config32(X58_MEMORY_HANDOFF_SAD_DEV, PCI_VENDOR_ID);
	if (id != X58_MEMORY_HANDOFF_SAD_EXPECTED_ID)
		die_with_post_code(POST_X58_MEMORY_HANDOFF_SAD_ID_FAIL,
			"[PAYLOAD] X58_MEMORY_HANDOFF SAD ID mismatch: %08x\n", id);

	for (size_t i = 0; i < X58_MEMORY_HANDOFF_PAM_COUNT; i++)
		before[i] = pci_io_read_config8(X58_MEMORY_HANDOFF_SAD_DEV,
			X58_MEMORY_HANDOFF_PAM_FIRST + i);
	printk(BIOS_DEBUG,
	       "[PAYLOAD] PAM PRE=%02x/%02x/%02x/%02x/%02x/%02x/%02x\n",
	       before[0], before[1], before[2], before[3], before[4], before[5],
	       before[6]);

	for (size_t i = 0; i < X58_MEMORY_HANDOFF_PAM_COUNT; i++)
		pci_io_write_config8(X58_MEMORY_HANDOFF_SAD_DEV, X58_MEMORY_HANDOFF_PAM_FIRST + i,
			x58_memory_handoff_pam_open[i]);
	x58_memory_handoff_fence();
	for (size_t i = 0; i < X58_MEMORY_HANDOFF_PAM_COUNT; i++)
		after[i] = pci_io_read_config8(X58_MEMORY_HANDOFF_SAD_DEV,
			X58_MEMORY_HANDOFF_PAM_FIRST + i);
	if (memcmp(after, x58_memory_handoff_pam_open, sizeof(after))) {
		if (!x58_memory_handoff_restore_pam(before))
			die_with_post_code(POST_X58_MEMORY_HANDOFF_PAM_FAIL,
				"[PAYLOAD] X58_MEMORY_HANDOFF PAM write and rollback both mismatched\n");
		die_with_post_code(POST_X58_MEMORY_HANDOFF_PAM_FAIL,
			"[PAYLOAD] X58_MEMORY_HANDOFF PAM readback mismatch\n");
	}
	printk(BIOS_DEBUG,
	       "[PAYLOAD] PAM POST=%02x/%02x/%02x/%02x/%02x/%02x/%02x\n",
	       after[0], after[1], after[2], after[3], after[4], after[5],
	       after[6]);

	if (!x58_memory_handoff_shadow_decode_probe()) {
		if (!x58_memory_handoff_restore_pam(before))
			die_with_post_code(POST_X58_MEMORY_HANDOFF_PAM_FAIL,
				"[PAYLOAD] X58_MEMORY_HANDOFF shadow probe failed and PAM rollback mismatched\n");
		die_with_post_code(POST_X58_MEMORY_HANDOFF_SHADOW_FAIL,
			"[PAYLOAD] X58_MEMORY_HANDOFF C0000-FFFFF decode/alias/restore probe failed\n");
	}
	post_code(POST_X58_MEMORY_HANDOFF_SHADOW_READY);
	printk(BIOS_NOTICE,
	       "[PAYLOAD] C0000-FFFFF PAM decode verified; left read/write for SELF loader\n");
}

static void x58_memory_handoff_read_resources(struct device *dev)
{
	/* Only ranges proven by the X58_MEMORY_HANDOFF lowmem and inherited X58_MEMORY_RESULT tests. */
	ram_range(dev, 0, 0x00000000, 0x000a0000);
	mmio_range(dev, 1, 0x000a0000, 0x00020000);
	reserved_ram_range(dev, 2, 0x000c0000, 0x00040000);
	ram_range(dev, 3, X58_RAMINIT_CBMEM_BASE, X58_RAMINIT_CBMEM_SIZE);
	ram_range(dev, 4, X58_RAMINIT_OBJECT_BASE, X58_RAMINIT_OBJECT_SIZE);
	post_code(POST_X58_MEMORY_HANDOFF_RESOURCES);
	printk(BIOS_NOTICE,
	       "[PAYLOAD] conservative RAM map: 0-640K, 16-24MiB, 32-40MiB\n");
}

static struct device_operations x58_memory_handoff_domain_ops = {
	.read_resources = x58_memory_handoff_read_resources,
	.set_resources = noop_set_resources,
};

static struct device_operations x58_memory_handoff_cpu_cluster_ops = {
	.read_resources = noop_read_resources,
	.set_resources = noop_set_resources,
};

void x58_memory_handoff_enable_dev(struct device *dev)
{
	if (dev->path.type == DEVICE_PATH_DOMAIN)
		dev->ops = &x58_memory_handoff_domain_ops;
	else if (dev->path.type == DEVICE_PATH_CPU_CLUSTER)
		dev->ops = &x58_memory_handoff_cpu_cluster_ops;
}

static void x58_memory_handoff_tables_written(void *unused)
{
	(void)unused;
	post_code(POST_X58_MEMORY_HANDOFF_TABLES);
	printk(BIOS_NOTICE, "[PAYLOAD] coreboot tables written\n");
}

static void x58_memory_handoff_payload_loaded(void *unused)
{
	(void)unused;
	post_code(POST_X58_MEMORY_HANDOFF_PAYLOAD_LOADED);
	printk(BIOS_NOTICE, "[PAYLOAD] SeaBIOS SELF image loaded\n");
}

static void x58_memory_handoff_payload_boot(void *unused)
{
	(void)unused;
	post_code(POST_X58_MEMORY_HANDOFF_PAYLOAD_BOOT);
	printk(BIOS_NOTICE, "[PAYLOAD] entering SeaBIOS payload\n");
}

BOOT_STATE_INIT_ENTRY(BS_WRITE_TABLES, BS_ON_EXIT, x58_memory_handoff_tables_written, NULL);
BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_LOAD, BS_ON_ENTRY, x58_memory_handoff_open_payload_shadow, NULL);
BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_LOAD, BS_ON_EXIT, x58_memory_handoff_payload_loaded, NULL);
BOOT_STATE_INIT_ENTRY(BS_PAYLOAD_BOOT, BS_ON_ENTRY, x58_memory_handoff_payload_boot, NULL);
