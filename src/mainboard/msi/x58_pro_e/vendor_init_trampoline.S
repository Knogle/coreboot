/* SPDX-License-Identifier: GPL-2.0-only */

#include "vendor_init_internal.h"

	.code32
	.text
	.section ".text.x58_vendor_call_trampoline", "ax", @progbits
	.globl x58_vendor_call_trampoline
	.type x58_vendor_call_trampoline, @function
x58_vendor_call_trampoline:
	/* Preserve the complete 32-bit cdecl callee-saved set on coreboot's stack. */
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	%esp, x58_vendor_call_state + VENDOR_CALL_CALLER_ESP
	pushfl
	popl	x58_vendor_call_state + VENDOR_CALL_CALLER_EFLAGS

	/* Switch to the separately guarded vendor stack. */
	movl	x58_vendor_call_state + VENDOR_CALL_STACK_TOP, %esp
	cli
	cld

	/* cdecl arguments are pushed right to left. */
	cmpl	$3, x58_vendor_call_state + VENDOR_CALL_ARGC
	jne	1f
	pushl	x58_vendor_call_state + VENDOR_CALL_ARG2
1:
	pushl	x58_vendor_call_state + VENDOR_CALL_ARG1
	pushl	x58_vendor_call_state + VENDOR_CALL_ARG0
	call	*x58_vendor_call_state + VENDOR_CALL_ENTRY

	/* Capture returned flags, then keep the temporary stack interrupt-free. */
	pushfl
	cli
	popl	x58_vendor_call_state + VENDOR_CALL_EFLAGS

	/* Capture outputs before using any general-purpose register as scratch. */
	movl	%esp, x58_vendor_call_state + VENDOR_CALL_VENDOR_ESP
	movl	%eax, x58_vendor_call_state + VENDOR_CALL_EAX
	movl	%ebx, x58_vendor_call_state + VENDOR_CALL_EBX
	movl	%ecx, x58_vendor_call_state + VENDOR_CALL_ECX
	movl	%edx, x58_vendor_call_state + VENDOR_CALL_EDX
	movl	%edi, x58_vendor_call_state + VENDOR_CALL_EDI

	/* Vendor code may violate cdecl.  Restore from memory, not vendor registers. */
	movl	x58_vendor_call_state + VENDOR_CALL_CALLER_ESP, %esp
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	pushl	x58_vendor_call_state + VENDOR_CALL_CALLER_EFLAGS
	popfl
	ret
	.size x58_vendor_call_trampoline, . - x58_vendor_call_trampoline
