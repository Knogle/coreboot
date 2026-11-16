/* SPDX-License-Identifier: GPL-2.0-only */
.section .text.matisse_irq_probe,"ax",@progbits
.code32

.macro matisse_irq_handler number
.global b565d4_matisse_irq_handler\number
.type b565d4_matisse_irq_handler\number, @function
b565d4_matisse_irq_handler\number:
	pushl %eax
	incl b565d4_matisse_irq_count + 4 * \number
	xorl %eax, %eax
	movl %eax, 0xfee000b0
	popl %eax
	iret
.size b565d4_matisse_irq_handler\number, .-b565d4_matisse_irq_handler\number
.endm

matisse_irq_handler 0
matisse_irq_handler 1
matisse_irq_handler 2
matisse_irq_handler 3
