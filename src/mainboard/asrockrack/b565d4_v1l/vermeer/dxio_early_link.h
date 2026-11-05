/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMD_MATISSE_DXIO_EARLY_LINK_H
#define AMD_MATISSE_DXIO_EARLY_LINK_H

#include <timer.h>
#include "dxio_init.h"
#include "dxio_result.h"

struct matisse_dxio_early_input {
	uint8_t *platform;
	size_t platform_bytes;
	uint32_t platform_physical;
	const struct matisse_dxio_result *result;
};

enum matisse_dxio_early_phase {
	MATISSE_DXIO_EARLY_NOT_STARTED,
	MATISSE_DXIO_EARLY_RUNNING,
	MATISSE_DXIO_EARLY_PENDING_CLEANUP,
	MATISSE_DXIO_EARLY_FINISHED,
	MATISSE_DXIO_EARLY_FAILED,
};

enum matisse_dxio_early_failure {
	MATISSE_DXIO_EARLY_NO_FAILURE,
	MATISSE_DXIO_EARLY_ADMISSION_FAILED,
	MATISSE_DXIO_EARLY_ACCESS_DENIED,
	MATISSE_DXIO_EARLY_PCI_FAILED,
	MATISSE_DXIO_EARLY_DMA_FAILED,
	MATISSE_DXIO_EARLY_REQUEST_FAILED,
	MATISSE_DXIO_EARLY_UNQUALIFIED_SWITCH,
};

/* Addresses are PCI configuration offsets: bus << 20 | devfn << 12 | reg.
 * The caller uses the actual common PCI access implementation, not a fixed
 * ECAM base. Callbacks are bounded and retain exclusive bus/SMU ownership.
 */
struct matisse_dxio_early_io {
	enum cb_err (*admit)(const struct matisse_dxio_early_input *input, void *context);
	bool (*may_access)(void *context);
	enum cb_err (*read)(uint32_t address, unsigned int bytes, uint32_t *value,
			    void *context);
	enum cb_err (*write)(uint32_t address, unsigned int bytes, uint32_t value,
			     void *context);
	enum cb_err (*prepare)(unsigned int use, void *context);
	enum cb_err (*consume)(void *context);
	void *context;
};

/* Initially zeroed persistent storage. A failing callback may have completed
 * an access. Keep the state and frame after failure; do not restart the event.
 */
struct matisse_dxio_early_state {
	uint32_t phase, failure, pci_attempted, request_attempted;
	uint32_t bdf, bus_before, downstream_id, engine_key;
	uint32_t last_address, last_value;
	int32_t last_status, cleanup_status;
	bool cleanup_needed, cleanup_attempted, frame_changed;
	struct smu_payload last_reply;
	struct stopwatch budget;
};

/* Native preparation for original event7 and 09c7ac8f, ordinary non-PLX case.
 * Single-root bus zero is the qualified original AM4 topology. Admission must
 * establish the stage before normal PCI bus allocation, matching owned DMA
 * frame/result, actual PCIe root-port capability layout and buffer lifetime.
 * prepare(14)/consume handle result DMA; prepare(3) handles the modified load.
 * may_access must check continued ownership and any enclosing program budget.
 * Requests use the supplied controller transport and its shared limits.
 *
 * Finish explicitly when polling returns, including after a failed event if
 * access remains admitted. It clears temporary secondary/subordinate buses as
 * in the original pre-allocation phase. Each started run and cleanup is once
 * only; a distinct later firmware event needs new caller-owned event state.
 * A PLX early topology is unqualified and returns an error with cleanup state.
 * All callbacks and a 120-second event budget are bounded. Success establishes
 * callback return, not PCI readback verification, training or physical boot.
 * Outside active Makefiles; no physical Matisse qualification.
 */
enum cb_err matisse_run_dxio_early_link(const struct matisse_dxio_early_input *input,
		struct matisse_dxio_early_state *state,
		const struct matisse_dxio_transport *transport,
		const struct matisse_dxio_early_io *io);

enum cb_err matisse_finish_dxio_early_link(struct matisse_dxio_early_state *state,
					const struct matisse_dxio_early_io *io);

#endif /* AMD_MATISSE_DXIO_EARLY_LINK_H */
