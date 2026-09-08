/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MAINBOARD_MSI_X58_PRO_E_VENDOR_INIT_INTERNAL_H
#define MAINBOARD_MSI_X58_PRO_E_VENDOR_INIT_INTERNAL_H

/* Keep these offsets synchronized with struct x58_vendor_call_state. */
#define VENDOR_CALL_ENTRY		0x00
#define VENDOR_CALL_ARG0		0x04
#define VENDOR_CALL_ARG1		0x08
#define VENDOR_CALL_ARG2		0x0c
#define VENDOR_CALL_ARGC		0x10
#define VENDOR_CALL_STACK_TOP		0x14
#define VENDOR_CALL_CALLER_ESP		0x18
#define VENDOR_CALL_CALLER_EFLAGS	0x1c
#define VENDOR_CALL_VENDOR_ESP		0x20
#define VENDOR_CALL_EAX			0x24
#define VENDOR_CALL_EBX			0x28
#define VENDOR_CALL_ECX			0x2c
#define VENDOR_CALL_EDX			0x30
#define VENDOR_CALL_EFLAGS		0x34
#define VENDOR_CALL_EDI			0x38
#define VENDOR_CALL_STATE_SIZE		0x3c

#ifndef __ASSEMBLER__

#include <types.h>

struct x58_vendor_call_state {
	uint32_t entry;
	uint32_t arg0;
	uint32_t arg1;
	uint32_t arg2;
	uint32_t argc;
	uint32_t stack_top;
	uint32_t caller_esp;
	uint32_t caller_eflags;
	uint32_t vendor_esp;
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t eflags;
	uint32_t edi;
};

extern struct x58_vendor_call_state x58_vendor_call_state;
void x58_vendor_call_trampoline(void);

#endif /* __ASSEMBLER__ */

#endif /* MAINBOARD_MSI_X58_PRO_E_VENDOR_INIT_INTERNAL_H */
