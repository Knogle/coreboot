/* SPDX-License-Identifier: GPL-2.0-only */
#include "native84_context.h"
#if !defined(__i386__)
#error "Genuine IA32 bridge only; no alternate-bitness fallback"
#endif

.code32
.section .text.native84_bridge, "ax", @progbits
.global native84_bridge_enter
.global native84_bridge_stage_x
.global native84_bridge_stage_08
.global native84_owned_fault
.type native84_bridge_enter, @function
native84_bridge_enter:
	/* Fresh caller-admitted image/code/context/lifetimes, not a C function ABI.
	 * EAX=image, ESI=descriptor56; no unsafe SGDT pointer discovery here.
	 */
	cli
	cld
	movl %eax, %edi
	cmpl NATIVE84_OFF_DESCRIPTOR(%edi), %esi
	jne native84_owned_fault
	lgdt NATIVE84_OFF_GDTR1(%edi)
	/* Incoming IDT still targets identical old entries in GDT1. */
	lidt NATIVE84_OFF_IDTR_X(%edi)
	ljmp *NATIVE84_OFF_FAR_X(%edi)
.size native84_bridge_enter, .-native84_bridge_enter

.type native84_bridge_stage_x, @function
native84_bridge_stage_x:
	/* IDT-X resolves code48 in both tables. No old08/10 data cache survives. */
	movw $NATIVE84_SEL_Y, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	lss NATIVE84_OFF_STACK_Y(%edi), %esp
	lgdt NATIVE84_OFF_GDTR2(%edi)
	lidt NATIVE84_OFF_IDTR_08(%edi)
	ljmp *NATIVE84_OFF_FAR_08(%edi)
.size native84_bridge_stage_x, .-native84_bridge_stage_x

.type native84_bridge_stage_08, @function
native84_bridge_stage_08:
	movw $NATIVE84_SEL_DATA, %ax
	movw %ax, %ds
	movw %ax, %es
	xorl %eax, %eax
	movw %ax, %fs
	movw %ax, %gs
	lss NATIVE84_OFF_STACK_10(%edi), %esp
	/* New cdecl entry(descriptor), NOT stock c_start(cbmem_top).
	 * Top16-aligned -> reserve12/push4 -> CALL, C entry ESP=top-20.
	 * Whole future C frame and CPU async-entry headroom remain unqualified.
	 */
	subl $12, %esp
	pushl %esi
	call *NATIVE84_OFF_ENTRY(%edi)
	jmp native84_owned_fault
.size native84_bridge_stage_08, .-native84_bridge_stage_08

.type native84_owned_fault, @function
native84_owned_fault:
	/* No IRET, stack interpretation, C/IO/helper or OEM rollback. */
	cli
1:
	hlt
	jmp 1b
.size native84_owned_fault, .-native84_owned_fault
.section .note.GNU-stack, "", @progbits
