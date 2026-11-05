/* SPDX-License-Identifier: GPL-2.0-only */
/* Isolated Cezanne diagnostic: entered after the strict vendor/family guard. */

.section .init, "ax", @progbits
.code32
.global cezanne_raw_pre_c_entry
cezanne_raw_pre_c_entry:
	cli
	cld
	movb $0xc7, %al
	outb %al, $0x80

	/* No RDMSR/WRMSR and no change to CR0/MTRRs. entry16 left caches disabled. */
	movl $_eearlyram_stack, %esp
	andl $0xfffffff0, %esp

	/* Linker assertions place all runtime regions inside qualified PSP DRAM. */
	movl $_bss, %edi
	movl $_ebss, %ecx
	subl %edi, %ecx
	jc .Lbad_runtime
	cmpl $0x10000, %ecx
	ja .Lbad_runtime
	xorl %eax, %eax
	rep stosb

	movl $_data, %edi
	movl $_edata, %ecx
	subl %edi, %ecx
	jc .Lbad_runtime
	cmpl $0x10000, %ecx
	ja .Lbad_runtime
	movl $_data_load, %esi
	rep movsb

	/* Saved by stock entry16 before POST/CPUID; no BIST interpretation. */
	movl %ebp, raw_reset_eax

	/* MM0-MM2 remain untouched. C10 does not consume the basetime argument. */
	subl $8, %esp
	pushl $0
	pushl $0
	movb $0xc8, %al
	outb %al, $0x80
	call __wrap_bootblock_c_entry

	movb $0xe9, %al
	jmp .Lstop
.Lbad_runtime:
	movb $0xea, %al
.Lstop:
	outb %al, $0x80
	cli
	hlt
	jmp .Lstop

.section .note.GNU-stack, "", @progbits
