/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef ARCH_X86_EBDA_H
#define ARCH_X86_EBDA_H

#include <stdbool.h>

/* Optional board observation around generic BDA/EBDA initialization.
 * before=true precedes all initialization writes; before=false follows them.
 * Neither invocation occurs on S3 resume. The default implementation is empty.
 */
void mainboard_ebda_init(bool before);

#endif
