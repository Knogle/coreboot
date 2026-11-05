/* SPDX-License-Identifier: GPL-2.0-only */
/* Qualify the actual CPU before any platform writes; entry EAX is ResetEAX. */

.section .init, "ax", @progbits
.code32
.global __wrap_bootblock_pre_c_entry
__wrap_bootblock_pre_c_entry:
	/* entry32.S preserves initial EAX in EBP; this is not interpreted as BIST.
	 * MM0/MM1/MM2 and EBP must remain untouched until the raw bridge stores it.
	 */
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
	andl $0xfffffff0, %eax
	cmpl $0x00a50f00, %eax
	jne .Lcpu_failed
#if CONFIG(B565D4_PRE_MSR_RAW_CONSOLE)
	jmp cezanne_raw_pre_c_entry
#else
	jmp __real_bootblock_pre_c_entry
#endif
.Lcpu_failed:
	movb $0xe2, %al
.Lhalt:
	outb %al, $0x80
	cli
	hlt
	jmp .Lhalt

.section .note.GNU-stack, "", @progbits
