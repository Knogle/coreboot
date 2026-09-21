/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef SOUTHBRIDGE_INTEL_I82801JX_PCIE_INIT_H
#define SOUTHBRIDGE_INTEL_I82801JX_PCIE_INIT_H

struct device;
struct southbridge_intel_i82801jx_config;

#define I82801JX_PCIE_PORT_COUNT 6
#define I82801JX_PCIE_XCAP 0x42
#define I82801JX_PCIE_LCAP 0x4c
#define I82801JX_PCIE_SLCAP 0x54
#define I82801JX_PCIE_SLCAP_POWER_SCALE_SHIFT 15
#define I82801JX_PCIE_RPDCGEN 0xe1
#define I82801JX_PCIE_DYNAMIC_CLOCKS 0x0f
#define I82801JX_PCIE_PECR2 0x300
#define I82801JX_PCIE_PECR2_REQUIRED (1U << 21)
#define I82801JX_PCIE_PEC1 0x324
#define I82801JX_PCIE_PEC1_REQUIRED 0x40

/* All calls require admitted ICH10 devices and working extended config access.
 * They do not establish ECAM, validate silicon identity, or install PCI drivers.
 */

/* Exactly two required fields, no slot/ASPM/error-mask locks or hiding.
 * Intel ICH10 Datasheet, sections 20.1.71 and 20.1.73.
 */
void i82801jx_pcie_required_fields(struct device *dev);

/* Pre-enumeration full reference policy; ports indexed by hardware function.
 * Includes R/WO slot fields and ASPM-capability locking. Caller must provide
 * the complete six-port graph and board wiring/power policy before calling.
 * Does NOT hide functions or write RCBA FD/FDSW/RPFN/MAP.
 */
void i82801jx_pcie_setup(struct device *const ports[I82801JX_PCIE_PORT_COUNT],
	const struct southbridge_intel_i82801jx_config *config);

/* Pre-enumeration setup after another initializer supplied the slot policy.
 * All six ports must remain enabled: this entry point never applies the
 * reference highest-disabled-port policy to undocumented PECR2 bits 17:16.
 * Capture and check all XCAP/SLCAP/LCAP values before any write, then apply
 * required fields and self-write the exact retained capabilities. This still
 * consumes any remaining write-once opportunity; it is not a lock-free probe.
 * Caller must snapshot/check results and must not retry after a failed write.
 */
void i82801jx_pcie_setup_retained(
	struct device *const ports[I82801JX_PCIE_PORT_COUNT]);

/* Post-allocation reference device init, including BME and R/WO error masks.
 * Caller owns resources, interrupt routing, and the hotplug policy. This is
 * not the lock-free required-fields helper above.
 */
void i82801jx_pcie_init_sequence(struct device *dev,
	const struct southbridge_intel_i82801jx_config *config);

/* Same post-allocation body without touching PCI_COMMAND, even transiently.
 * For a caller that already owns an audited per-port forwarding/BME policy.
 * In particular, an empty enabled root need not acquire BME. This does not
 * weaken any resource or command admission performed by that caller.
 */
void i82801jx_pcie_init_sequence_preserve_command(struct device *dev,
	const struct southbridge_intel_i82801jx_config *config);

#endif
