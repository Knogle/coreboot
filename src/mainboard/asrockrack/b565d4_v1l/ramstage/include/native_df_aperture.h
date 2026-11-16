/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef NATIVE_DF_APERTURE_H
#define NATIVE_DF_APERTURE_H

/* Once after fresh owned APOB capture and the selected DXIO trainer, before
 * generic PCI enumeration/resource discovery. Failure is terminal.
 */
void native107_df_posted_aperture(void);

/* Entry2 ordinary PCI I/O route, immediately after the MMIO preparation. */
void native137_df_io_aperture(void);

#endif
