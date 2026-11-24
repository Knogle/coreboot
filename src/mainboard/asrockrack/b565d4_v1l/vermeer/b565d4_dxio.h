/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef B565D4_MATISSE_DXIO_H
#define B565D4_MATISSE_DXIO_H

#include "b565d4_policy.h"
#include "b565d4_link_policy.h"
#include <amdblocks/matisse_dxio_dma.h>
#include <amdblocks/matisse_dxio_events.h>

struct b565d4_matisse_dxio_input {
	enum b565d4_matisse_slot_mode slot_mode;
	uint8_t pspp_policy;
	uint8_t max_cpu_link_speed;
	uint8_t cpu_slot_speed;
	uint8_t cpu_gpp_speed;
	bool force_reset_id;
	bool acs;
	/* Original wrapper selector0x17: ASRock PCD2e6=0; F20 equivalent323=1. */
	bool reference_link_feature;
	/* F20 e3/2f9 map to existing ASRock165/231 wrapper controls. */
	bool reference_capability_features;
	uint8_t *platform;
	size_t platform_capacity;
	uint32_t platform_physical;
	void *ancillary;
	uint32_t ancillary_physical, ancillary_dwords;
};

enum b565d4_matisse_dxio_phase {
	B565D4_MATISSE_DXIO_NOT_STARTED,
	B565D4_MATISSE_DXIO_ADMITTING,
	B565D4_MATISSE_DXIO_PREPARING,
	B565D4_MATISSE_DXIO_RUNNING,
	B565D4_MATISSE_DXIO_FINISHED,
	B565D4_MATISSE_DXIO_FAILED,
};

/* Persistent, initially zeroed caller storage, outside the early stack.
 * Pointers below refer to this object or admitted external DMA backing.
 * Keep it and both buffers after failure; no implicit restart or release.
 */
struct b565d4_matisse_dxio_state {
	uint32_t phase;
	size_t port_count, platform_bytes;
	struct matisse_dxio_port ports[B565D4_MATISSE_ACTIVE_PORTS];
	struct b565d4_matisse_link_policy link;
	struct matisse_dxio_dma_input backing;
	struct matisse_dxio_mapped_input mapping;
	struct matisse_dxio_events_input event_input;
	struct matisse_dxio_dma_state dma;
	struct matisse_dxio_events_state events;
	struct matisse_dxio_init_state controller;
};

struct b565d4_matisse_dxio_io {
	enum cb_err (*admit)(const struct b565d4_matisse_dxio_input *input,
			    const struct b565d4_matisse_dxio_state *state, void *context);
	bool (*may_access)(void *context);
	enum cb_err (*smn_read)(uint32_t address, uint32_t *value, void *context);
	enum cb_err (*smn_write)(uint32_t address, uint32_t value, void *context);
	enum cb_err (*pci_read)(uint32_t address, unsigned int bytes, uint32_t *value,
			       void *context);
	enum cb_err (*pci_write)(uint32_t address, unsigned int bytes, uint32_t value,
				void *context);
	enum cb_err (*reset)(unsigned int group, unsigned int level, void *context);
	void (*report)(const struct matisse_dxio_poll_result *event, unsigned int phase,
		       void *context);
	void *context;
};

/* Experimental desktop board composition. Admission precedes
 * platform stores. It must establish the actual Matisse single-root initial
 * phase, complete board/reset policy, reserved unencrypted DRAM backing and
 * CPU mapping, PSP/SMM/firmware exclusions, exclusive SMN/SMU and pre-allocation
 * PCI access for the whole call. Geometry alone never establishes ownership.
 * External input, I/O and context remain stable; callbacks are bounded.
 *
 * Builds the native three/six-port frame and connects the qualified DMA/cache,
 * event owner and common-SMU controller, including both phase cleanup hooks.
 * Ancillary bytes are already populated and admitted by the caller; zero
 * ancillary input deliberately requests none. Compiled initial/link defaults
 * are explicit starting policy, not later OEM/NVRAM policy. The complete call
 * has a 120-second budget, in addition to component/request limits.
 *
 * CB_SUCCESS means both logical phases completed, with the controller's
 * conditional final result DMA. Its legacy zero-version path remains intact.
 * Actual physical cache/DMA, board wiring, training, enumeration, USB
 * initialization and a Matisse coreboot boot remain separate requirements.
 */
enum cb_err b565d4_matisse_run_dxio(const struct b565d4_matisse_dxio_input *input,
		struct b565d4_matisse_dxio_state *state,
		const struct b565d4_matisse_dxio_io *io);

#endif /* B565D4_MATISSE_DXIO_H */
