/* SPDX-License-Identifier: GPL-2.0-only */
#include "native84_context.h"

static int span(uint32_t begin, uint32_t bytes, struct native84_interval *out)
{
	if (!begin || !bytes || bytes > UINT32_MAX - begin)
		return 0;
	out->begin = begin;
	out->end = begin + bytes;
	return 1;
}

static int interval(struct native84_interval range)
{
	return range.begin && range.begin < range.end;
}

static int contains(struct native84_interval outer, struct native84_interval inner)
{
	return inner.begin >= outer.begin && inner.end <= outer.end;
}

static int overlaps(struct native84_interval a, struct native84_interval b)
{
	return a.begin < b.end && b.begin < a.end;
}

static int in_text(uint32_t entry, struct native84_interval text)
{
	return entry >= text.begin && entry < text.end;
}

static int old_flat(const uint8_t *d, int code)
{
	/* Admit old data92/93 only; old code remains accessed, DPL0, present,
	 * base0/4GiB D32/L0. GDT1 data08 is normalized in the OWN copy.
	 * Preserve AVL; readable conforming old code9f is admitted like Source14.
	 */
	if (d[0] != 0xff || d[1] != 0xff || d[2] || d[3] || d[4] || d[7] ||
	    (d[6] & 0xef) != 0xcf)
		return 0;
	return code ? (d[5] == 0x9b || d[5] == 0x9f) :
		(d[5] == 0x92 || d[5] == 0x93);
}

static void flat(uint8_t *d, uint8_t access)
{
	d[0] = 0xff; d[1] = 0xff;
	d[2] = 0; d[3] = 0; d[4] = 0;
	d[5] = access; d[6] = 0xcf; d[7] = 0;
}

static void gate(uint8_t *g, uint32_t entry, uint16_t selector)
{
	g[0] = (uint8_t)entry;
	g[1] = (uint8_t)(entry >> 8);
	g[2] = (uint8_t)selector;
	g[3] = (uint8_t)(selector >> 8);
	g[4] = 0;
	g[5] = 0x8e; /* Present DPL0 legacy32 interrupt gate, no task switch. */
	g[6] = (uint8_t)(entry >> 16);
	g[7] = (uint8_t)(entry >> 24);
}

enum native84_context_result native84_context_build(
	const struct native84_context_request *request,
	struct native84_context_image *image)
{
	struct native84_context_request r;
	struct native84_interval input, output, old, descriptor, list;
	uint8_t old_bytes[NATIVE84_OLD_GDT_BYTES];
	uint32_t base = (uintptr_t)image;

	if (!request || !image || ((uintptr_t)request & 3) || (base & 7) ||
	    !span((uintptr_t)request, sizeof(*request), &input) ||
	    !span(base, sizeof(*image), &output))
		return NATIVE84_CONTEXT_BAD_ARGUMENT;
	if (overlaps(input, output))
		return NATIVE84_CONTEXT_BAD_ALIAS;
	r = *request;
	if (r.old_gdt_bytes != NATIVE84_OLD_GDT_BYTES ||
	    !span((uintptr_t)r.old_gdt_snapshot, r.old_gdt_bytes, &old) ||
	    r.descriptor_bytes != 56 || ((uintptr_t)r.descriptor & 3) ||
	    !span((uintptr_t)r.descriptor, r.descriptor_bytes, &descriptor) ||
	    !r.protected || ((uintptr_t)r.protected & 3) ||
	    !r.protected_count || r.protected_count > NATIVE84_MAX_PROTECTED ||
	    !span((uintptr_t)r.protected, r.protected_count * sizeof(*r.protected), &list))
		return NATIVE84_CONTEXT_BAD_ARGUMENT;
	if (!interval(r.parent) || !interval(r.context_container) ||
	    !interval(r.stack) || !interval(r.shim_text) || !interval(r.entry_text) ||
	    !contains(r.parent, r.context_container) ||
	    !contains(r.context_container, output) || !contains(r.parent, r.stack) ||
	    !contains(r.parent, descriptor) || !contains(r.parent, r.shim_text) ||
	    !contains(r.parent, r.entry_text) ||
	    (r.stack_pointer & 15) || r.stack_pointer < r.stack.begin ||
	    r.stack_pointer > r.stack.end || r.stack_pointer - r.stack.begin < 64 ||
	    !in_text(r.bridge_x_entry, r.shim_text) ||
	    !in_text(r.bridge_08_entry, r.shim_text) ||
	    !in_text(r.fault_entry, r.shim_text) || !in_text(r.entry, r.entry_text) ||
	    r.bridge_x_entry == r.bridge_08_entry ||
	    r.bridge_x_entry == r.fault_entry || r.bridge_08_entry == r.fault_entry)
		return NATIVE84_CONTEXT_BAD_GEOMETRY;
	if (overlaps(output, old) || overlaps(output, descriptor) ||
	    overlaps(output, list) || overlaps(output, r.stack) ||
	    overlaps(output, r.shim_text) || overlaps(output, r.entry_text) ||
	    overlaps(r.stack, input) || overlaps(r.stack, old) ||
	    overlaps(r.stack, descriptor) || overlaps(r.stack, list) ||
	    overlaps(r.stack, r.shim_text) || overlaps(r.stack, r.entry_text) ||
	    overlaps(descriptor, r.shim_text) || overlaps(descriptor, r.entry_text) ||
	    overlaps(r.shim_text, r.entry_text))
		return NATIVE84_CONTEXT_BAD_ALIAS;
	for (uint32_t i = 0; i < r.protected_count; i++) {
		struct native84_interval p = r.protected[i];

		if (!interval(p))
			return NATIVE84_CONTEXT_BAD_ARGUMENT;
		if (overlaps(output, p) || overlaps(r.stack, p))
			return NATIVE84_CONTEXT_PROTECTED_OVERLAP;
	}
	/* The ONLY foreign table read is the exact caller-qualified72B snapshot. */
	for (uint32_t i = 0; i < NATIVE84_OLD_GDT_BYTES; i++)
		old_bytes[i] = r.old_gdt_snapshot[i];
	if (!old_flat(old_bytes + 8, 0) || !old_flat(old_bytes + 16, 1))
		return NATIVE84_CONTEXT_BAD_OLD_GDT;

	/* Commit begins. No remaining error path or dereference of input pointers.
	 * No store precedes this point; every image byte is initialized exactly.
	 */
	image->gdtr1.limit = NATIVE84_GDT_BYTES - 1;
	image->gdtr1.base = base + NATIVE84_OFF_GDT1;
	image->gdtr2.limit = NATIVE84_GDT_BYTES - 1;
	image->gdtr2.base = base + NATIVE84_OFF_GDT2;
	image->idtr_x.limit = NATIVE84_IDT_BYTES - 1;
	image->idtr_x.base = base + NATIVE84_OFF_IDT_X;
	image->idtr_08.limit = NATIVE84_IDT_BYTES - 1;
	image->idtr_08.base = base + NATIVE84_OFF_IDT_08;
	image->stack_y.esp = r.stack_pointer;
	image->stack_y.ss = NATIVE84_SEL_Y;
	image->stack_10.esp = r.stack_pointer;
	image->stack_10.ss = NATIVE84_SEL_DATA;
	image->far_x.offset = r.bridge_x_entry;
	image->far_x.selector = NATIVE84_SEL_X;
	image->far_08.offset = r.bridge_08_entry;
	image->far_08.selector = NATIVE84_SEL_CODE;
	image->entry = r.entry;
	image->descriptor = (uintptr_t)r.descriptor;
	for (uint32_t i = 0; i < NATIVE84_OLD_GDT_BYTES; i++) {
		image->gdt1[i] = old_bytes[i];
		image->gdt2[i] = old_bytes[i];
	}
	/* Normalize ONLY data08 Accessed in the OWN copy; never old snapshot. */
	image->gdt1[0x08 + 5] |= 1;
	flat(image->gdt1 + NATIVE84_SEL_X, 0x9b);
	flat(image->gdt1 + NATIVE84_SEL_Y, 0x93);
	flat(image->gdt2 + NATIVE84_SEL_X, 0x9b);
	flat(image->gdt2 + NATIVE84_SEL_Y, 0x93);
	flat(image->gdt2 + NATIVE84_SEL_CODE, 0x9b);
	flat(image->gdt2 + NATIVE84_SEL_DATA, 0x93);
	for (uint32_t i = 0; i < 256; i++) {
		gate(image->idt_x + i * 8, r.fault_entry, NATIVE84_SEL_X);
		gate(image->idt_08 + i * 8, r.fault_entry, NATIVE84_SEL_CODE);
	}
	return NATIVE84_CONTEXT_VALID;
}
