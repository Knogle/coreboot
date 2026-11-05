/* SPDX-License-Identifier: GPL-2.0-only */

/* The initial RAM medium intentionally has no current option table. Read
 * options only after the verified CBFS medium is available. Extended setup
 * migrates its schema on the BSP; the simple layout uses the option fallback
 * when its checksum is invalid. Preserve existing CMOS bytes here.
 */
void __wrap_sanitize_cmos(void);
void __wrap_sanitize_cmos(void)
{
}
