/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef B565D4_MATISSE_POLICY_H
#define B565D4_MATISSE_POLICY_H

#include <amdblocks/matisse_dxio_engines.h>

enum b565d4_matisse_slot_mode {
	B565D4_MATISSE_PCIE_X16,
	B565D4_MATISSE_PCIE_FOUR_X4,
};

/* Desktop board policy. The original Matisse OEM
 * selection and CPM provider supply three active PCIe descriptors. The split
 * mode activates their three dormant x4 descriptors and narrows the first
 * port to lanes28..31. This is an explicit native choice, not a claim that
 * vendor setup/NVRAM selected it. Preserve original active descriptor order,
 * reset IDs, device/function policy and lane orientation.
 *
 * Returns three/six ports, or zero without output stores for invalid mode,
 * missing output or insufficient capacity. Output contains only native
 * consumed fields, no vendor pointers, firmware calls or physical I/O.
 * Caller supplies PSPP/reset policy, ancillary data and complete training.
 */
size_t b565d4_matisse_build_ports(enum b565d4_matisse_slot_mode mode,
				struct matisse_dxio_port *ports, size_t capacity);

/* Apply the explicit CPU-port generation before serializing the DXIO frame.
 * Zero preserves the OEM auto policy; the PCH descriptor remains unchanged.
 */
void b565d4_matisse_set_cpu_speed(struct matisse_dxio_port *ports, size_t count,
				unsigned int speed);

/* Independent CPU-slot limits, Gen1..3 or zero for the build policy. The
 * build ceiling remains authoritative; zero ceiling retains OEM Auto.
 * PCH and unknown indices return zero. Invalid setter inputs make no stores.
 * The same selector feeds the initial frame and returned-engine service.
 */
unsigned int b565d4_matisse_cpu_slot_speed(unsigned int policy, size_t count,
		unsigned int ceiling, unsigned int gfx_speed, unsigned int gpp_speed);
bool b565d4_matisse_set_slot_speeds(struct matisse_dxio_port *ports, size_t count,
		unsigned int ceiling, unsigned int gfx_speed, unsigned int gpp_speed);

#endif /* B565D4_MATISSE_POLICY_H */
