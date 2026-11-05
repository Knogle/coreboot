/* SPDX-License-Identifier: GPL-2.0-only */
/* Separate future artifact, never a bootblock/stage replacement or run GO. */
#include <b565d4_memlayout.h>
OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(native84_bridge_enter)
PHDRS { bridge PT_LOAD FLAGS(5); }
SECTIONS
{
	. = B565D4_NATIVE_DRAM_BASE + 0x2000;
	_native84_bridge_start = .;
	.text : { KEEP(*(.text.native84_bridge)) } : bridge
	_native84_bridge_end = .;
	ASSERT(_native84_bridge_end <= B565D4_NATIVE_DRAM_BASE + 0x4000,
		"Persistent bridge/fault code exceeds its explicit EARLY slot")
	/DISCARD/ : { *(.note.GNU-stack) *(.comment) }
}
