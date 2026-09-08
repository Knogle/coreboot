# Public X58 research hand-off

The [mainboard overview](../../../Documentation/mainboard/msi/x58_pro_e.md)
is the current entry point for status, limitations, public build commands and
recovery. This directory and `../research/` preserve the historical English
research notes as redacted derivatives, not as a new set of hardware tests.
See [public evidence and omissions](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) before interpreting
captured identities, artifact links or claims of immutability in older text.

## Architecture, scope and recovery

- [Project operating instructions](../AGENTS.md)
- [Historical overview](../README.md)
- [Implementation plan](implementation-plan.md)
- [Board inventory](board-inventory.md)
- [Flash recovery](flash-recovery.md)
- [First-build architecture](first-build-architecture.md)
- [Bring-up history](bringup-log.md) and [test matrix](test-matrix.md)

## Reset, memory, QPI and diagnostics

- [Vendor-assisted memory initialization](vendor-assisted-memory-init.md)
- [Optional vendor ROMMON interface](b06v0-vendor-rommon.md)
- [Register scripting and rollback](b06v1-register-scripting.md)
- [Post-memory ROMMON](b06v6-ramstage-rommon.md)
- [RAM object-transfer protocol](ram-loader-protocol.md)
- [Deterministic High-QPI sequencing](b06v9-deterministic-highqpi.md)
- [High-QPI automatic handoff](b06ve-highqpi-auto-handoff.md)
- [Broader hard-gated profile](b06vl-broad-hard-gate-seabios.md)
- [Source-only serial-independent SPD admission](spd-compatibility.md)

## Platform, payload and OS observations

- [PCI, VGA and iPXE path](b06vm-auto-pci-vga-ipxe.md)
- [USB/SeaBIOS integration](b06wg-automatic-usb-seabios.md)
- [Reduced USB logging](b06wh-quiet-usb-seabios.md)
- [Vendor IRQ/ACPI comparison](b06wi-vendor-irq-acpi.md)
- [Native ACPI platform descriptions](b06wj-acpi-platform.md)
- [B06WK ACPI repair and remaining limits](b06wk-acpi-repair.md)
- [Linux initcall observations](../research/msi/b06wk-linux-initcall-hw-2026-09-08.md)
- [Linux latency hypotheses](../research/msi/b06wk-linux-latency-root-cause-analysis-2026-09-08.md)
- [Memtest observations and memory-map caveat](../research/msi/b06wk-memtest-hw-2026-09-08.md)

## Firmware archaeology

- [Firmware analysis methodology](firmware-analysis.md)
- [Vendor-state capture method](vendor-state-capture.md)
- [Cross-vendor shared sequences](../research/comparisons/shared-sequences.md)
- [Candidate memory-init interface](../research/comparisons/candidate-mrc-interface.md)
- [Uncore register correlation](../research/comparisons/uncore-register-correlation.md)
- [Intel PEI candidates](../research/intel/pei-candidates.md)
- [Intel/MSI ACPI comparison](../research/intel/acpi-x58-wj-comparison-2026-09-08.md)

The remaining stage-specific notes remain alongside these entry points;
they have not been collapsed into claims of universal compatibility.
Firmware binaries, raw captures and private/generated artifacts are not part
of this public hand-off. Their omitted references are catalogued in
[the evidence index](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).
