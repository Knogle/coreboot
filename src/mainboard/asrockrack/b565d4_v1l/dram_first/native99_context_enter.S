/* SPDX-License-Identifier: GPL-2.0-only */
#include "native84_context.h"
#if !defined(__i386__)
#error "Native95 returning context preparation requires genuine IA32"
#endif

.code32
.section .text.native95_context_prepare, "ax", @progbits
.global native95_context_prepare
.type native95_context_prepare, @function
native95_context_prepare:
	/* Explicit asmlinkage/cdecl: image is4(%esp). The C caller admitted
	 * current CS08/data10 flat mappings and its existing EARLYRAM stack.
	 * EAX is caller-saved; no callee-saved register or ESP is changed.
	 */
	cli
	cld
	movl 4(%esp), %eax
	lgdt NATIVE84_OFF_GDTR2(%eax)
	lidt NATIVE84_OFF_IDTR_08(%eax)
	/* Keep the same flat CS08 cache and data10 stack mapping. No far jump,
	 * copied stage08 execution or LSS onto the intermediate stack.
	 */
	movl $0x10, %eax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	xorl %eax, %eax
	movw %ax, %fs
	movw %ax, %gs
	ret
.size native95_context_prepare, .-native95_context_prepare
.section .note.GNU-stack, "", @progbits
