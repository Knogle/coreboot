/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NATIVE132_CORE_LAUNCH_H
#define NATIVE132_CORE_LAUNCH_H

#define NATIVE132_RESET_BASE        0x09fffbf0
#define NATIVE132_RESET_BYTES       0x410
#define NATIVE132_MSR_OFFSET        0x180
#define NATIVE132_MSR_COUNT         36
#define NATIVE132_RECORDS_OFFSET    0x330
#define NATIVE132_LAUNCH_BIT_OFFSET 0x334
#define NATIVE132_GDT_OFFSET        0x3b0
#define NATIVE132_VECTOR_OFFSET     0x400
#define NATIVE132_ENTER_OFFSET      0x40c
#define NATIVE132_READY_OFFSET      0x40e
#define NATIVE132_RECORD_BYTES      48
#define NATIVE132_RECORD_SLOTS      16
#define NATIVE132_RECORD_DONE       0x10000001

#ifndef __ASSEMBLER__
#include <stddef.h>
/* Called once, after genuine Source73 SIPI/MSR preparation and flush, before INIT. */
void native132_release_before_init(const void *saved_msrs, size_t count);
/* Called only after genuine MP success AND the current CPU-device checks. */
void native132_restore_after_mp_success(void);
#endif
#endif
