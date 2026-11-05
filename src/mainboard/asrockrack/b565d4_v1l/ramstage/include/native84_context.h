/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NATIVE84_CONTEXT_H
#define NATIVE84_CONTEXT_H

/* Shared C/assembler layout. All actual addresses and entry identities are unbound. */
#define NATIVE84_OLD_GDT_BYTES 72
#define NATIVE84_GDT_BYTES 88
#define NATIVE84_IDT_BYTES 2048
#define NATIVE84_IMAGE_BYTES 4328
#define NATIVE84_MAX_PROTECTED 32
#define NATIVE84_SEL_X 0x48
#define NATIVE84_SEL_Y 0x50
#define NATIVE84_SEL_CODE 0x08
#define NATIVE84_SEL_DATA 0x10

#define NATIVE84_OFF_GDTR1 0
#define NATIVE84_OFF_GDTR2 6
#define NATIVE84_OFF_IDTR_X 12
#define NATIVE84_OFF_IDTR_08 18
#define NATIVE84_OFF_STACK_Y 24
#define NATIVE84_OFF_STACK_10 30
#define NATIVE84_OFF_FAR_X 36
#define NATIVE84_OFF_FAR_08 42
#define NATIVE84_OFF_ENTRY 48
#define NATIVE84_OFF_DESCRIPTOR 52
#define NATIVE84_OFF_GDT1 56
#define NATIVE84_OFF_GDT2 144
#define NATIVE84_OFF_IDT_X 232
#define NATIVE84_OFF_IDT_08 2280

#ifndef __ASSEMBLER__
#include <commonlib/bsd/compiler.h>
#include <stdint.h>
#include <stddef.h>

#if !defined(__i386__) || !defined(__BYTE_ORDER__) || \
	!defined(__ORDER_LITTLE_ENDIAN__) || \
	__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "This unintegrated leaf requires genuine little-endian IA32"
#endif

#include "native84_interval.h"

struct native84_table_pointer {
	uint16_t limit;
	uint32_t base;
} __packed;

struct native84_stack_pair {
	uint32_t esp;
	uint16_t ss;
} __packed;

struct native84_far_pointer {
	uint32_t offset;
	uint16_t selector;
} __packed;

struct native84_context_image {
	struct native84_table_pointer gdtr1, gdtr2, idtr_x, idtr_08;
	struct native84_stack_pair stack_y, stack_10;
	struct native84_far_pointer far_x, far_08;
	uint32_t entry, descriptor;
	uint8_t gdt1[NATIVE84_GDT_BYTES], gdt2[NATIVE84_GDT_BYTES];
	uint8_t idt_x[NATIVE84_IDT_BYTES], idt_08[NATIVE84_IDT_BYTES];
} __aligned(8);

struct native84_context_request {
	/* A complete stable caller-qualified copy, never a followed SGDT pointer. */
	const uint8_t *old_gdt_snapshot;
	uint32_t old_gdt_bytes;
	struct native84_interval parent;
	/* A container; it is not the separate CBMEM external-handle bank. */
	struct native84_interval context_container;
	struct native84_interval stack;
	uint32_t stack_pointer;
	const void *descriptor;
	uint32_t descriptor_bytes;
	/* Whole persistent bridge AND vector-text extent; no identity from numbers. */
	struct native84_interval shim_text;
	uint32_t bridge_x_entry, bridge_08_entry, fault_entry;
	struct native84_interval entry_text;
	uint32_t entry;
	const struct native84_interval *protected;
	uint32_t protected_count;
};

enum native84_context_result {
	NATIVE84_CONTEXT_VALID = 0,
	NATIVE84_CONTEXT_BAD_ARGUMENT,
	NATIVE84_CONTEXT_BAD_GEOMETRY,
	NATIVE84_CONTEXT_BAD_ALIAS,
	NATIVE84_CONTEXT_BAD_OLD_GDT,
	NATIVE84_CONTEXT_PROTECTED_OVERLAP,
};

_Static_assert(sizeof(void *) == 4 && sizeof(uintptr_t) == 4 &&
	sizeof(size_t) == 4 && sizeof(int) == 4, "actual IA32 ABI");
_Static_assert(sizeof(uint8_t) == 1 && sizeof(uint16_t) == 2 &&
	sizeof(uint32_t) == 4, "actual byte/word/DWORD widths");
_Static_assert(sizeof(struct native84_interval) == 8 &&
	_Alignof(struct native84_interval) == 4 &&
	sizeof(struct native84_context_request) == 84 &&
	_Alignof(struct native84_context_request) == 4, "fixed request geometry");
_Static_assert(sizeof(struct native84_table_pointer) == 6 &&
	_Alignof(struct native84_table_pointer) == 1 &&
	offsetof(struct native84_table_pointer, limit) == 0 &&
	offsetof(struct native84_table_pointer, base) == 2, "GDTR/IDTR6");
_Static_assert(sizeof(struct native84_stack_pair) == 6 &&
	_Alignof(struct native84_stack_pair) == 1 &&
	offsetof(struct native84_stack_pair, esp) == 0 &&
	offsetof(struct native84_stack_pair, ss) == 4, "LSS m16:32 pair6");
_Static_assert(sizeof(struct native84_far_pointer) == 6 &&
	_Alignof(struct native84_far_pointer) == 1 &&
	offsetof(struct native84_far_pointer, offset) == 0 &&
	offsetof(struct native84_far_pointer, selector) == 4, "far m16:32 pointer6");
_Static_assert(sizeof(struct native84_context_image) == NATIVE84_IMAGE_BYTES &&
	_Alignof(struct native84_context_image) == 8, "fixed context image ABI");
_Static_assert(NATIVE84_GDT_BYTES == NATIVE84_OLD_GDT_BYTES + 16 &&
	NATIVE84_SEL_X == NATIVE84_OLD_GDT_BYTES &&
	NATIVE84_SEL_Y == NATIVE84_OLD_GDT_BYTES + 8 &&
	NATIVE84_IDT_BYTES == 256 * 8, "nine old entries plus X/Y and full IDTs");
#define NATIVE84_ASSERT_OFFSET(member, constant) \
	_Static_assert(offsetof(struct native84_context_image, member) == (constant), \
		"shared C/assembler offset: " #member)
NATIVE84_ASSERT_OFFSET(gdtr1, NATIVE84_OFF_GDTR1);
NATIVE84_ASSERT_OFFSET(gdtr2, NATIVE84_OFF_GDTR2);
NATIVE84_ASSERT_OFFSET(idtr_x, NATIVE84_OFF_IDTR_X);
NATIVE84_ASSERT_OFFSET(idtr_08, NATIVE84_OFF_IDTR_08);
NATIVE84_ASSERT_OFFSET(stack_y, NATIVE84_OFF_STACK_Y);
NATIVE84_ASSERT_OFFSET(stack_10, NATIVE84_OFF_STACK_10);
NATIVE84_ASSERT_OFFSET(far_x, NATIVE84_OFF_FAR_X);
NATIVE84_ASSERT_OFFSET(far_08, NATIVE84_OFF_FAR_08);
NATIVE84_ASSERT_OFFSET(entry, NATIVE84_OFF_ENTRY);
NATIVE84_ASSERT_OFFSET(descriptor, NATIVE84_OFF_DESCRIPTOR);
NATIVE84_ASSERT_OFFSET(gdt1, NATIVE84_OFF_GDT1);
NATIVE84_ASSERT_OFFSET(gdt2, NATIVE84_OFF_GDT2);
NATIVE84_ASSERT_OFFSET(idt_x, NATIVE84_OFF_IDT_X);
NATIVE84_ASSERT_OFFSET(idt_08, NATIVE84_OFF_IDT_08);
#undef NATIVE84_ASSERT_OFFSET

/*
 * New source-only builder, unused. Caller qualifies all input objects, all
 * mappings, immutable snapshot/code identity and output writability BEFORE
 * this ordinary C prologue. A numeric VALID result is no context/cut capability.
 * The supplied protected spans include the actual builder text/ROdata, its
 * complete live C/caller frames, external handle bank and other excluded live
 * objects. Caller must establish image disjointness from those actual extents
 * before this C prologue; numbers here cannot retroactively guard its stores.
 * Every error leaves the complete image untouched. Success writes exactly its
 * 4328 bytes; nothing else, including the descriptor and whole stack, is written.
 * Selected image/descriptor/code/tables/stack lifetimes span the entire bridge.
 * Fresh SGDT limit47/snapshot72, CS10/data08 effective flat caches, incoming
 * IDT/handlers, IF0/DF0/CPL0/PG context, async/system-state and all-agent premises
 * are external. No SGDT pointer is dereferenced and no CPU state is changed here.
 */
enum native84_context_result native84_context_build(
	const struct native84_context_request *request,
	struct native84_context_image *image);

/*
 * ASM-only nonreturning entry, NOT a cdecl call to this symbol:
 * EAX=image, ESI=descriptor56. Caller has freshly rechecked the actual output
 * image/descriptor/source identity, current CPU/borrowed stack and lifetimes.
 * After the two-GDT sequence, an ordinary indirect cdecl entry(descriptor) CALL
 * is made with ESP16 aligned before CALL. An unexpected return only HALTs.
 * This is not stock c_start: its later arg at4ESP must still be cbmem_top.
 */
void native84_bridge_enter(void) __noreturn;
void native84_bridge_stage_x(void) __noreturn;
void native84_bridge_stage_08(void) __noreturn;
void native84_owned_fault(void) __noreturn;
#endif
#endif
