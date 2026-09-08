/* SPDX-License-Identifier: GPL-2.0-only */

	.code32
	.section ".text.x58_rommon_irqprobe", "ax", @progbits

	.globl x58_rommon_irq0_entry
	.type x58_rommon_irq0_entry, @function
x58_rommon_irq0_entry:
	pushal
	movb $0x0b, %al		/* Select master PIC ISR. */
	outb %al, $0x20
	inb $0x20, %al
	movb %al, x58_irqprobe_pic_isr
	inb $0x21, %al		/* Prevent a second IRQ before EOI. */
	orb $0x01, %al
	outb %al, $0x21
	incl x58_irqprobe_hits
	movb $0x20, %al		/* ExtINT requires only the PIC EOI. */
	outb %al, $0x20
	popal
	iret
	.size x58_rommon_irq0_entry, .-x58_rommon_irq0_entry

	.globl x58_rommon_irq_fatal_entry
	.type x58_rommon_irq_fatal_entry, @function
x58_rommon_irq_fatal_entry:
	cli
	movb $0xff, %al
	outb %al, $0x21
	outb %al, $0xa1
	movl $0xfee00350, %edx
	movl $0x00010000, (%edx)
	movb $0xef, %al
	outb %al, $0x80
1:
	hlt
	jmp 1b
	.size x58_rommon_irq_fatal_entry, .-x58_rommon_irq_fatal_entry
