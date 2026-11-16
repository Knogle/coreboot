/* SPDX-License-Identifier: GPL-2.0-only */
/* UNBUILT Source74 proposal. This blob is copied, never entered by the BSP.
 * No stack, call, low30000 read, UART, SMM, VM_CR or GIF operation.
 */
#include "native132_core_launch.h"

.section .rodata.native132_reset_image,"a",@progbits
.balign 16
.global native132_reset_image
.global native132_reset_image_end
native132_reset_image:
.code16
	cli
	cld
	/* CS-relative even before DS or any MTRR is established. AX receives
	 * unique old entry count; retain initial CS in EBP's upper half.
	 */
	movw $1, %ax
	lock xaddw %ax, %cs:0xfffc
	movzwl %ax, %ebp
	movw %cs, %ax
	movzwl %ax, %eax
	shll $16, %eax
	orl %eax, %ebp
	lgdtl %cs:0xfff4
	/* Same cache-disabled/flat IA32 start as the genuine SIPI path.
	 * No WBINVD here: do not invalidate a sibling's shared caches.
	 */
	movl $0x60000011, %eax
	movl %eax, %cr0
	ljmpl $0x08, $(NATIVE132_RESET_BASE + (.Lflat - native132_reset_image))
.Lflat:
.code32
	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	xorl %eax, %eax
	movw %ax, %fs
	movw %ax, %gs
	movl %eax, %cr3
	movl %eax, %cr4
	/* The copied table is in this same reset window. SYS exposure first,
	 * TOM/TOM2/IORR/fixed/variable/DEF, exact SYS last: no new MSR values.
	 */
	movl $(NATIVE132_RESET_BASE + NATIVE132_MSR_OFFSET), %esi
	movl $NATIVE132_MSR_COUNT, %ebx
.Lmsr:
	movl 0(%esi), %ecx
	movl 4(%esi), %eax
	movl 8(%esi), %edx
	wrmsr
	addl $12, %esi
	decl %ebx
	jnz .Lmsr
	movl %cr0, %eax
	andl $0x9fffffff, %eax
	orl $0x10000, %eax
	movl %eax, %cr0
	movl $0x600, %eax
	movl %eax, %cr4
	/* Only now access separately owned RAMstage BSS. No APIC-ID indexing:
	 * record slots use the atomic entry ordinal, not guessed APIC IDs.
	 */
	movl %ebp, %eax
	shrl $16, %eax
	andl $0xffff, %ebp
	cmpl $NATIVE132_RECORD_SLOTS, %ebp
	jae .Lpark
	imull $NATIVE132_RECORD_BYTES, %ebp, %edi
	addl NATIVE132_RESET_BASE + NATIVE132_RECORDS_OFFSET, %edi
	movl %eax, 8(%edi)
	movl %ebp, 44(%edi)
	movl NATIVE132_RESET_BASE + NATIVE132_LAUNCH_BIT_OFFSET, %eax
	movl %eax, 4(%edi)
	movl $1, %eax
	cpuid
	movl %eax, 12(%edi)
	movl %ebx, 16(%edi)
	movl $0x1b, %ecx
	rdmsr
	/* Enable this AP's inherited LAPIC base without changing its other bits.
	 * The existing record stores the fresh post-WRMSR readback before park.
	 */
	orl $(1 << 11), %eax
	wrmsr
	rdmsr
	movl %eax, 28(%edi)
	movl %edx, 32(%edi)
	movl %cr0, %eax
	movl %eax, 36(%edi)
	movl %cr4, %eax
	movl %eax, 40(%edi)
	movl $0x80000000, %eax
	cpuid
	cmpl $0x8000001e, %eax
	jb .Lrecord_done
	movl $0x8000001e, %eax
	cpuid
	movl %eax, 20(%edi)
	movl %ebx, 24(%edi)
.Lrecord_done:
	movl $NATIVE132_RECORD_DONE, 0(%edi)
	mfence
	lock incw NATIVE132_RESET_BASE + NATIVE132_READY_OFFSET
.Lpark:
	cli
	hlt
	jmp .Lpark

/* .org rejects an oversized stub in a later explicitly selected build. */
.org NATIVE132_MSR_OFFSET
	.space NATIVE132_MSR_COUNT * 12, 0
.org NATIVE132_RECORDS_OFFSET
	.long 0 /* BSP patches own record-array address, before any release. */
	.long 0 /* BSP patches selected ordinal launch bit before each message. */
.org NATIVE132_GDT_OFFSET
	.quad 0x0000000000000000
	.quad 0x00cf9b000000ffff /* code08, standard32 flat */
	.quad 0x00cf93000000ffff /* data10 */
	.quad 0x00af9b000000ffff /* unused64 descriptor, matches standard32B GDT */
.org NATIVE132_VECTOR_OFFSET
	.byte 0x90, 0xe9, 0xfc, 0xfb /* reset NOP; near16 jump back0x400 */
	.word 31
	.long NATIVE132_RESET_BASE + NATIVE132_GDT_OFFSET
	.word 0
	.word 0 /* reset+c: entry count */
	.word 0 /* reset+e: mirrored/recorded, about to CLI/HLT */
.org NATIVE132_RESET_BYTES
native132_reset_image_end:
.section .note.GNU-stack,"",@progbits
