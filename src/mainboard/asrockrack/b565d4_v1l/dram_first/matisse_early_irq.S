/* SPDX-License-Identifier: GPL-2.0-only */
.section .text
.code32

.global b565d4_early51_hardware
.type b565d4_early51_hardware, @function
b565d4_early51_hardware:
	pushal
	incl b565d4_early51_hardware_count
	movl $0, 0xfee000b0
	popal
	iret
.size b565d4_early51_hardware, .-b565d4_early51_hardware

.global b565d4_early51_software
.type b565d4_early51_software, @function
b565d4_early51_software:
	pushal
	incl b565d4_early51_software_count
	popal
	iret
.size b565d4_early51_software, .-b565d4_early51_software

.global b565d4_early51_spurious
.type b565d4_early51_spurious, @function
b565d4_early51_spurious:
	incl b565d4_early51_spurious_count
	iret
.size b565d4_early51_spurious, .-b565d4_early51_spurious

.global b565d4_early51_unexpected
.type b565d4_early51_unexpected, @function
b565d4_early51_unexpected:
	cli
	cld
	movl $.Lunexpected, %esi
.Lcharacter:
	lodsb
	testb %al, %al
	jz .Lstop
	movb %al, %bl
	movl $100000, %ecx
	movw $0x2fd, %dx
.Lpoll:
	inb %dx, %al
	testb $0x20, %al
	jnz .Lsend
	loop .Lpoll
	jmp .Lstop
.Lsend:
	movw $0x2f8, %dx
	movb %bl, %al
	outb %al, %dx
	jmp .Lcharacter
.Lstop:
	hlt
	jmp .Lstop
.size b565d4_early51_unexpected, .-b565d4_early51_unexpected

.section .rodata
.Lunexpected:
	.asciz "\r\nMatisse EARLY51 unexpected vector; stopped\r\n"
