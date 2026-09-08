/* SPDX-License-Identifier: GPL-2.0-only */

#include "i82801jx.h"

/* Intel ICH10 Family Datasheet 319973-003, FD section 10.1.77. */
void i82801jx_disable_sata2(void)
{
	RCBA32_OR(RCBA_FD, FD_SAD2);
}
