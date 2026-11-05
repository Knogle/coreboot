/* SPDX-License-Identifier: GPL-2.0-only */
/* Check the actual CPU before the common AMD pre-C code changes any MSRs. */
#include <b565d4_cpu_ids.h>

.section .init, "ax", @progbits
.code32
.global __wrap_bootblock_pre_c_entry
__wrap_bootblock_pre_c_entry:
	/* entry32.S preserves initial EAX in EBP and MM0. Keep MM0-MM2. */
	xorl %eax, %eax
	cpuid
	cmpl $1, %eax
	jb .Lcpu_failed
	cmpl $0x68747541, %ebx /* Auth */
	jne .Lcpu_failed
	cmpl $0x69746e65, %edx /* enti */
	jne .Lcpu_failed
	cmpl $0x444d4163, %ecx /* cAMD */
	jne .Lcpu_failed
	movl $1, %eax
	cpuid
	andl $B565D4_CPUID_MODEL_MASK, %eax
#if CONFIG(B565D4_MATISSE)
	cmpl $B565D4_CPUID_MATISSE_A0, %eax
	je .Lcpu_accepted
	cmpl $B565D4_CPUID_MATISSE_B0, %eax
#elif CONFIG(B565D4_VERMEER)
	cmpl $B565D4_CPUID_VERMEER_A0, %eax
	je .Lcpu_accepted
	cmpl $B565D4_CPUID_VERMEER_B0, %eax
#else
	cmpl $B565D4_CPUID_CEZANNE, %eax
#endif
	jne .Lcpu_failed
.Lcpu_accepted:
	jmp __real_bootblock_pre_c_entry
.Lbist_failed:
	movb $0xe1, %al
	jmp .Lhalt
.Lcpu_failed:
	movb $0xe2, %al
.Lhalt:
	outb %al, $0x80
	cli
	hlt
	jmp .Lhalt

.section .note.GNU-stack, "", @progbits
