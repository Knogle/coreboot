/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef X58_MANUAL_RECOVERY_H
#define X58_MANUAL_RECOVERY_H

#include <stdbool.h>

/* Call only from a valid BSP ramstage stack. The caller must establish a
 * safe continuation boundary BEFORE entering this console. Recheck must be
 * read-only: it must not replay initialization, acknowledge status or repair
 * hardware. False keeps the checkpoint stopped; true permits this caller to
 * resume once. Never use for memory corruption, machine checks or partial
 * silicon/flash writes. No return from die(), longjmp or saved-PC patching.
 *
 * id is a static single command token; reason remains owned by the caller.
 * No current fatal site is made resumable merely by adding this interface.
 */
void x58_recovery_checkpoint(const char *id, const char *reason,
	bool (*recheck)(void *context), void *context);

#endif
