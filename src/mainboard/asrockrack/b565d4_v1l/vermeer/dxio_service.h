/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef MATISSE_DXIO_SERVICE_H
#define MATISSE_DXIO_SERVICE_H

#include <amdblocks/smu.h>

/* Experimental desktop backend component.
 *
 * The caller owns the single AM4 I/O die's SMN/SMU access and any physical
 * buffers described by the arguments. Argument zero becomes the DXIO opcode.
 * On CB_SUCCESS all six words contain the raw firmware return, which remains
 * separate from the outer mailbox completion. No automatic retry is made.
 */
enum cb_err matisse_dxio_request(uint32_t opcode, struct smu_payload *arguments);

/* Original PcieSetSpeed request for a bound returned-platform engine index.
 * Outer transport completion does not establish the requested link speed.
 */
enum cb_err matisse_dxio_set_link_speed(unsigned int engine_index, unsigned int speed,
				      struct smu_payload *reply);

/* Original DXIO service status decoding; the raw return remains available. */
uint8_t matisse_dxio_status(uint32_t raw);

#endif /* MATISSE_DXIO_SERVICE_H */
