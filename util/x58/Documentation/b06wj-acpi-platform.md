> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WJ: ACPI processor and platform-description experiment

Status: **TWO BYTE-IDENTICAL BUILDS; 469 HOST TESTS PASS; ONE HARDWARE
ACPI/SEABIOS/JETFLASH HANDOFF PASS; WINDOWS OUTCOME PENDING**.
Build identity: `X58PROE-B06WJ-ACPI-PLATFORM-20260907`.

The [first hardware run](../research/msi/b06wj-jetflash-hw-2026-09-08.md)
records the expected new marker and actual JetFlash selection. This does not
yet establish a resolved Windows A5 or an OS boot.

## Why this build

B06WI-HW-01 completed the new IRQ transaction, FADT/MADT/MCFG construction,
SeaBIOS and JetFlash handoff, but the operator's subsequent screenshot again
showed `ACPI_BIOS_ERROR (0xA5)`. The four parameters remain unknown. The IRQ
changes alone therefore did not resolve Windows' rejection. This is not a
reason to repeat RAM/QPI or the working USB bring-up sequence.

B06WJ closes concrete gaps in the native OS-facing description. It does not
claim to know the exact A5 cause or to be a complete vendor ACPI replacement.
The new option `X58_PRO_E_B06WJ_ACPI_PLATFORM` defaults off and depends on
B06WI. Disabling it preserves the byte-identical 800-byte WI DSDT.

## Changes

- `\_SB.CP00`, `ACPI0007`, integer UID 0, matching the existing single BSP
  MADT processor UID 0 / APIC ID 0. No additional processor, P-state or C-state
  package is advertised.
- `\_SB.PCI0.LPCB` at PCI `00:1f.0`, with fixed PIC, PIT and RTC devices.
  Their ports/ISA IRQs match the vendor-live DSDT and existing open ICH10 ASL:
  PIC `20–21,a0–a1`/IRQ2; PIT `40–43`/IRQ0; RTC `70–71`/IRQ8.
  MADT retains IRQ0-to-GSI2, distinct from the PIT's ISA IRQ number.
- PCI root I/O producers `0000–0cf7` and `0d00–ffff`, with CF8–CFF still a
  consumer. This includes the new low-I/O LPC children without changing
  coreboot's PCI allocation lower bound `1000`.
- Fixed motherboard reservations for actually used Fintek configuration
  `4e–4f`, polling COM1 `3f8–3ff`, KBC ports `60/64`, POST `80`, and ELCR
  `4d0–4d1`. This does not advertise working PS/2 input or an IRQ4 handler.
- `\_SB.HPET`, `PNP0103`, reserving `fed00000/400`. It is outside the PCI
  subtree and replaces the old duplicate HPET reservation in MRES.
  The common coreboot `acpi_write_hpet()` publishes its table using the
  already decoded device's ID and the existing ICH10 minimum-tick policy
  `0x80`. The new writer does not enable the counter or interrupts.

The CPU mapping follows the [ACPI processor declaration rule](https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/08_Processor_Configuration_and_Control/declaring-processors.html).
Board resource evidence is in the
[vendor-live comparison](../research/msi/vendor-live-acpi-irq-gpio-2026-09-07.md)
and [B06WI hardware run](../research/msi/b06wi-jetflash-hw-2026-09-07.md).
These sources motivate the experiment; they do not establish its Windows
result.

## Deliberately unchanged

RAM/QPI initialization and their existing local CSI/MINIT wrapper, sparse
post-memory path, hardware IRQ transaction, GPIO57/USB sequence, SATA/PCIe,
Radeon physical VBIOS and SeaBIOS/iPXE payloads remain inherited. There are no
new hardware writes or test/reset loops. COM1, early POST diagnostics and CAR
recovery remain available. Normal boot still goes automatically to SeaBIOS.

SMM/SMI services, S3/S4, ACPI wake, AP/SMP startup, full PIRQ/link-device
policy, general port/topology support and EDK2 remain outside this change.
The inherited B06WI `_PRT` subset is not enlarged. No proprietary vendor AML
or its `0xffffff00` BIOS data region is imported.

## First test and recovery

Use the fixed E5645 stepping 2, microcode 1f, one BLS4G3D1609DS1S00 DIMM at
SPD54, HD5450 and existing keyboard/JetFlash configuration. Flash only the
full W25Q128 image named in the [manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index),
verify it externally, and retain the known-good socketed chip.

Expected serial: B06WJ identity, inherited B06WI IRQ READY/a8, followed by
`[ACPI] B06WJ-PLATFORM1 HPET ... CPU_NAMESPACE=CP00/UID0 ... HW_WRITE=0`,
then SeaBIOS and JetFlash. No new diagnostic stop is intentionally added.
The next success criterion is progress past the former Windows A5 point,
not another memory qualification pass.

Possible failure: Windows may still reject another table/resource or IRQ
interface; the new timer declaration also lets Windows use hardware it did
not previously see. Capture the screen and UART. If the system resets into
the retained-state CAR guard, use the existing explicit reset unlock/guard
clear before a one-second Shelly interruption. If boot cannot be recovered
remotely, swap back the known-good chip. Do not claim a fixed A5 until the
hardware run demonstrates it.
