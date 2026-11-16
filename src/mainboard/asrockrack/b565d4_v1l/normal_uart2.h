/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef B565D4_NORMAL_UART2_H
#define B565D4_NORMAL_UART2_H

#include <stdbool.h>
#include <stdint.h>

#if ENV_RAMSTAGE
struct b565d4_uart2_requal {
	uint32_t boundary, helper_step, attempted, result;
	uint32_t lpc_id, lpc44_before, lpc48_before, lpc44_after, lpc48_after;
	uint32_t lsr_before, lcr_before, lsr_after, lcr_after, dlab_readback;
	uint32_t ast_revision, scu84_after, scu90_after, cf8_untouched;
};

bool b565d4_uart2_ack38_requalify(struct b565d4_uart2_requal *report);
#endif

bool b565d4_uart2_setup(void);


#endif
