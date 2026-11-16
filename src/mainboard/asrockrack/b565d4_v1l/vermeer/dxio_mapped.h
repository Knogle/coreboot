/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_DXIO_MAPPED_H
#define AMD_MATISSE_DXIO_MAPPED_H

#include "dxio_init.h"
#include "dxio_reconfigure.h"

struct matisse_dxio_mapped_input {
	uint8_t root_index, bus;
	const uint8_t *wrappers;
	size_t wrapper_count;
	const struct matisse_dxio_result_slot *slots;
	size_t slot_count;
	const struct matisse_dxio_port *ports;
	size_t port_count;
	const struct matisse_dxio_link_overrides *overrides;
	const struct matisse_dxio_link_controls *initial_controls;
	const struct matisse_dxio_reconfigure_policy *policy;
	uint8_t *platform;
	size_t platform_bytes;
	uint32_t platform_physical;
};

enum matisse_dxio_mapped_phase {
	MATISSE_DXIO_MAPPED_NOT_STARTED,
	MATISSE_DXIO_MAPPED_ADMITTING,
	MATISSE_DXIO_MAPPED_RUNNING,
	MATISSE_DXIO_MAPPED_IO_RETURNED,
	MATISSE_DXIO_MAPPED_FAILED,
};

enum matisse_dxio_mapped_failure {
	MATISSE_DXIO_MAPPED_NO_FAILURE,
	MATISSE_DXIO_MAPPED_ADMISSION_FAILED,
	MATISSE_DXIO_MAPPED_ACCESS_DENIED,
	MATISSE_DXIO_MAPPED_DMA_FAILED,
	MATISSE_DXIO_MAPPED_REQUEST_FAILED,
	MATISSE_DXIO_MAPPED_DECODE_FAILED,
	MATISSE_DXIO_MAPPED_CONTROLS_FAILED,
	MATISSE_DXIO_MAPPED_MAP_FAILED,
	MATISSE_DXIO_MAPPED_RECONFIGURE_FAILED,
};

/* Persistent caller storage. Keep partial writes and callback state after
 * failure. A fresh later event requires a freshly initialized workspace.
 */
struct matisse_dxio_mapped_state {
	uint32_t phase, failure, request_attempted, accesses, reads, writes;
	uint32_t last_address, last_value;
	int32_t last_status;
	struct smu_payload reply;
	struct matisse_dxio_result result;
	struct matisse_dxio_link_controls controls[MATISSE_NBIF_MAP_SLOTS];
	uint32_t map_before[MATISSE_NBIF_MAP_SLOTS];
	struct matisse_nbif_map_update map[MATISSE_NBIF_MAP_SLOTS];
	struct matisse_dxio_reconfigure_state reconfigure;
};

struct matisse_dxio_mapped_io {
	enum cb_err (*admit)(const struct matisse_dxio_mapped_input *input, void *context);
	bool (*may_access)(void *context);
	enum cb_err (*prepare)(unsigned int use, void *context);
	enum cb_err (*consume)(void *context);
	enum cb_err (*read)(uint32_t address, uint32_t *value, void *context);
	enum cb_err (*write)(uint32_t address, uint32_t value, void *context);
	void *context;
};

/* Native event15 preparation: owned result DMA, decoder, policy/control update,
 * eighteen-slot NBIF map and earlier port/wrapper reconfiguration. Admission
 * establishes the actual root/bus, board policy, stable input lifetimes, DMA
 * backing/cache handling and exclusive SMN ownership. The qualified AM4 path
 * uses bus zero; other buses require separate qualification.
 *
 * Each map read immediately precedes its write, preserving fresh upper bits.
 * All callbacks are bounded and share the caller's enclosing budget; this
 * component additionally checks120s and caps accesses at32768. The controller
 * transport shares request limits. No automatic retry, event ACK, result-buffer
 * free, training completion or physical Matisse boot is implied by success.
 * Outside active Makefiles; behavioral qualification is tracked separately.
 */
enum cb_err matisse_run_dxio_mapped(const struct matisse_dxio_mapped_input *input,
		struct matisse_dxio_mapped_state *state,
		const struct matisse_dxio_transport *transport,
		const struct matisse_dxio_mapped_io *io);

#endif /* AMD_MATISSE_DXIO_MAPPED_H */
