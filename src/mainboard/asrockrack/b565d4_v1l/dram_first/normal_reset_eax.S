/* SPDX-License-Identifier: GPL-2.0-only */
/* The common NONCAR runtime has initialized owned BSS before this boundary. */
.section .init, "ax", @progbits
.code32
.global __wrap_bootblock_c_entry
.type __wrap_bootblock_c_entry, @function
__wrap_bootblock_c_entry:
	movd %mm0, %eax
	movl %eax, b565d4_normal_reset_eax
	/* Preserve ESP and the caller's original uint64_t timestamp arguments. */
	jmp b565d4_pre_soc_c_entry
.size __wrap_bootblock_c_entry, .-__wrap_bootblock_c_entry
.section .note.GNU-stack, "", @progbits
