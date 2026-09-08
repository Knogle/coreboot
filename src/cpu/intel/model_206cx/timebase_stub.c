/* SPDX-License-Identifier: GPL-2.0-only */

#include <cpu/x86/tsc.h>
#include <delay.h>

/*
 * B00 never enters ramstage.  Keep the mandatory link interface explicit and
 * report no timebase instead of guessing a ratio before CPU policy exists.
 */
void init_timer(void)
{
}

unsigned long tsc_freq_mhz(void)
{
	return 0;
}
