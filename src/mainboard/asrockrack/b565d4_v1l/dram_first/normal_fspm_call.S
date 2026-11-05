/* SPDX-License-Identifier: GPL-2.0-only */
/* IA32 cdecl: fn, UPD, HOB-slot, own original-flags output. */
.section .text.b565d4_fspm_call_boundary,"ax",@progbits
.code32
.globl b565d4_fspm_call_boundary
.hidden b565d4_fspm_call_boundary
.type b565d4_fspm_call_boundary,@function
b565d4_fspm_call_boundary:
	pushl %ebx
	pushl %esi
	movl 12(%esp), %ebx
	movl 16(%esp), %ecx
	movl 20(%esp), %edx
	movl 24(%esp), %esi
	pushl %edx
	pushl %ecx
	call *%ebx
	/* No C, stack cleanup, memory operation or flag-changing instruction
	 * can precede this exact original-flags capture and DF restoration. */
	pushfl
	popl %edx
	cld
	addl $8, %esp
	movl %edx, (%esi)
	popl %esi
	popl %ebx
	ret
.size b565d4_fspm_call_boundary, .-b565d4_fspm_call_boundary
.section .note.GNU-stack,"",@progbits
