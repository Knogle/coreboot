/* SPDX-License-Identifier: GPL-2.0-only */
#include "native84_profile.h"
#include "native84_context.h"
#include <arch/io.h>
#include <cbmem.h>
#include <imd.h>
#include <string.h>
#include <symbols.h>

_Static_assert(sizeof(void *) == 4 && sizeof(size_t) == 4 &&
	sizeof(struct imd) == 16 && sizeof(struct native84_profile_policy) == 32 &&
	offsetof(struct native84_profile_policy, entry_path) == 8,
	"Native84 genuine IA32 ABI");

/* Root-selected HOST build fixture, not a runtime ownership/IO/execute grant. */
const volatile struct native84_profile_policy native84_profile_policy = {
	.enabled = 1,
	.epoch = UINT32_C(0x00840001),
	.entry_path = 2, /* Early native APOB; no PEI allocation. */
	.expected_cr0 = UINT32_C(0x00010011),
	.expected_cr3 = 0,
	.expected_cr4 = UINT32_C(0x00000600),
	.uart_max_polls = UINT32_C(1000000),
	.fault_entry = NATIVE84_BRIDGE_BEGIN + 0x49,
};
static struct native84_handoff stage_descriptor;
static int stage_admitted;

void native84_terminal(void)
{
	asm volatile ("cli" : : : "memory");
	for (;;)
		asm volatile ("hlt" : : : "memory");
}

static void require_cpu(void)
{
	uint32_t cr0, cr3, cr4, flags;
	uint16_t cs, ds, es, ss;

	asm volatile ("mov %%cr0,%0" : "=r" (cr0));
	asm volatile ("mov %%cr3,%0" : "=r" (cr3));
	asm volatile ("mov %%cr4,%0" : "=r" (cr4));
	asm volatile ("pushfl; popl %0" : "=r" (flags));
	asm volatile ("mov %%cs,%0" : "=r" (cs));
	asm volatile ("mov %%ds,%0" : "=r" (ds));
	asm volatile ("mov %%es,%0" : "=r" (es));
	asm volatile ("mov %%ss,%0" : "=r" (ss));
	if (native84_profile_policy.enabled != 1 ||
	    native84_profile_policy.epoch != NATIVE84_EPOCH ||
	    native84_profile_policy.entry_path != 2 ||
	    !native84_profile_policy.uart_max_polls ||
	    native84_profile_policy.uart_max_polls > 1000000 ||
	    !(cr0 & 1) || (cr0 & UINT32_C(0x80000000)) ||
	    cr0 != native84_profile_policy.expected_cr0 ||
	    cr3 != native84_profile_policy.expected_cr3 ||
	    cr4 != native84_profile_policy.expected_cr4 ||
	    (flags & ((1u << 9) | (1u << 10))) ||
	    cs != 8 || ds != 16 || es != 16 || ss != 16 ||
	    native84_profile_policy.fault_entry < NATIVE84_BRIDGE_BEGIN ||
	    native84_profile_policy.fault_entry >= NATIVE84_BRIDGE_END)
		native84_terminal();
}

size_t native84_protected(struct native84_interval *list, size_t capacity)
{
	/* Whole slots are conservative exclusions, not emitted-size assertions.
	 * The handle bank is its own 32B subspan, never the whole EARLY prefix.
	 */
	static const struct native84_interval early[] = {
		{ NATIVE84_A, NATIVE84_A + 56 },
		{ NATIVE84_CONTEXT_BEGIN, NATIVE84_CONTEXT_BEGIN + NATIVE84_IMAGE_BYTES },
		{ NATIVE84_BRIDGE_BEGIN, NATIVE84_BRIDGE_END },
		{ NATIVE84_LATE_STACK_BEGIN, NATIVE84_LATE_STACK_END },
		{ NATIVE84_EARLY_STACK_BEGIN, NATIVE84_EARLY_STACK_END },
		{ NATIVE84_BOOTBLOCK_BEGIN, NATIVE84_BOOTBLOCK_END },
		{ NATIVE84_ROMSTAGE_BEGIN, NATIVE84_ROMSTAGE_END },
		{ NATIVE84_MEDIA_BEGIN, NATIVE84_MEDIA_BEGIN + NATIVE84_MEDIA_BYTES },
	};
	size_t i, count = sizeof(early) / sizeof(early[0]);

	if (!list || capacity < count + (ENV_RAMSTAGE ? 1 : 0) +
	    (ENV_SEPARATE_ROMSTAGE ? 2 : 0))
		native84_terminal();
	for (i = 0; i < count; i++)
		list[i] = early[i];
#if ENV_SEPARATE_ROMSTAGE
	if ((uintptr_t)_program >= (uintptr_t)_eprogram ||
	    (uintptr_t)_earlyram_stack >= (uintptr_t)_eearlyram_stack)
		native84_terminal();
	list[count++] = (struct native84_interval) {
		(uintptr_t)_program, (uintptr_t)_eprogram
	};
	list[count++] = (struct native84_interval) {
		(uintptr_t)_earlyram_stack, (uintptr_t)_eearlyram_stack
	};
#endif
#if ENV_RAMSTAGE
	list[count++] = (struct native84_interval) {
		(uintptr_t)_program, (uintptr_t)_eprogram
	};
#endif
	return count;
}

static void require_descriptor(const struct native84_handoff *d)
{
	struct native84_interval protected[NATIVE84_MAX_PROTECTED];
	struct native84_validation_request request;
	struct native84_layout layout;

	/* Readable immutable object and complete current prologue/frame backing
	 * were admitted by the caller before C entry. Arithmetic is not a mapping.
	 */
	if (!d || ((uintptr_t)d & 3))
		native84_terminal();
	request.expected_epoch = NATIVE84_EPOCH;
	request.emitted_handle_bytes = sizeof(struct imd);
	request.protected = protected;
	request.protected_count = native84_protected(protected, NATIVE84_MAX_PROTECTED);
	if (native84_validate_handoff(d, &request, &layout) != NATIVE84_VALID ||
	    d->parent_base != NATIVE84_A || d->parent_bytes != NATIVE84_R ||
	    d->early_base != NATIVE84_A || d->early_bytes != NATIVE84_EARLY_BYTES ||
	    d->media_base != NATIVE84_MEDIA_BEGIN || d->media_bytes != NATIVE84_MEDIA_BYTES ||
	    d->cbmem_bottom != NATIVE84_CBMEM_BEGIN || d->cbmem_top != NATIVE84_PARENT_END ||
	    d->handle_bank_base != NATIVE84_BANK_BEGIN ||
	    d->handle_bank_bytes != NATIVE84_BANK_BYTES)
		native84_terminal();
}

void native84_stage_admit(const struct native84_handoff *descriptor)
{
	const struct native84_handoff *early = (const void *)(uintptr_t)NATIVE84_A;

	require_cpu();
	require_descriptor(descriptor);
	if (memcmp(descriptor, early, sizeof(*descriptor)))
		native84_terminal();
	if (stage_admitted) {
		if (memcmp(&stage_descriptor, descriptor, sizeof(*descriptor)))
			native84_terminal();
		return;
	}
	stage_descriptor = *descriptor;
	stage_admitted = 1;
}

void native84_stage_check(void)
{
	require_cpu();
	if (!stage_admitted)
		native84_terminal();
	require_descriptor(&stage_descriptor);
	if (memcmp(&stage_descriptor, (const void *)(uintptr_t)NATIVE84_A,
		   sizeof(stage_descriptor)))
		native84_terminal();
}

const struct native84_handoff *native84_descriptor(void)
{
	native84_stage_check();
	return &stage_descriptor;
}

/* Retain the all-256 terminal IDT through every genuine stage GDT switch.
 * No debug-register, breakpoint, AP, SMI or platform exception setup is replayed.
 */
void __wrap_exception_init(void);
void __wrap_exception_init(void)
{
	const struct native84_context_image *image =
		(const void *)(uintptr_t)NATIVE84_CONTEXT_BEGIN;
	const uint8_t *idt = image->idt_08;
	uint32_t target = native84_profile_policy.fault_entry;
	unsigned int i;

	/* assembly_entry may call this before its C car_stage_entry wrapper. */
	if (!stage_admitted)
		native84_stage_admit((const void *)(uintptr_t)NATIVE84_A);
	native84_stage_check();
	if (image->idtr_08.limit != 2047 ||
	    image->idtr_08.base != (uintptr_t)idt)
		native84_terminal();
	for (i = 0; i < 256; i++, idt += 8) {
		if (idt[0] != (uint8_t)target || idt[1] != (uint8_t)(target >> 8) ||
		    idt[2] != 8 || idt[3] || idt[4] || idt[5] != 0x8e ||
		    idt[6] != (uint8_t)(target >> 16) ||
		    idt[7] != (uint8_t)(target >> 24))
			native84_terminal();
	}
	asm volatile ("lidt %0" : : "m" (image->idtr_08) : "memory");
}
