> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WE: corrected X58 SAD gate before MCFG and SeaBIOS

Status: **BUILD-TESTED; 415/415 repository tests pass; two release builds are
byte-identical; TARGET HARDWARE UNTESTED**.

B06WE is a deliberately narrow successor to the hardware-executed B06WD
image.  B06WD reached DRAM-backed ramstage, completed PCI/resource setup,
prepared AHCI, initialized/finalized the selected devices and began ACPI table
construction.  It then failed closed while checking PCIEXBAR because that
callback read the absent `00:00.1` function.  The X58 CPU-side SAD function
used by the already-passed platform code is actually `ff:00.1`; direct ECAM
access at `e0000000` still returned the expected host ID `8086:3405`.

This is therefore a software wrong-BDF false negative.  It is not evidence
that RAM training, QPI, the X58 link, PCI enumeration or ECAM had failed.

## Exact functional delta

B06WE changes only the read-only MCFG publication gate:

```text
B06WD SAD BDF:       00:00.1 (absent; returned ffffffff)
B06WE SAD BDF:       ff:00.1
required SAD ID:     8086:2d81 (dword 2d818086)
required PCIEXBAR:   high 00000000, low e0000001
required ECAM host:  8086:3405 (dword 34058086 at e0000000)
coreboot PCI access: CF8/CFC remains unchanged
```

No PCI configuration, MMIO, USB, PS/2, SATA, PM or reset register is written
by this delta.  The complete B06WD memory/QPI, PCI, SATA, input, ACPI,
SeaBIOS, physical Radeon VBIOS and RTL8168 iPXE path is retained unchanged.
The old B06WD branch of the source remains frozen for regression comparison.

## POST and serial contract

| Code | Meaning |
|---|---|
| `8e` | B06WE has begun the corrected read-only SAD/PCIEXBAR/ECAM gate. |
| `8f` | SAD identity, PCIEXBAR and direct ECAM host identity all match. |
| `86` | Existing terminal ACPI fail-closed path; the serial reason remains authoritative. |

The decisive new success line is:

```text
[ACPI] B06WE SAD_BDF=ff:00.1 SAD=2d818086 PCIEXBAR=00000000:e0000001 HOST=34058086 COREBOOT_ACCESS=CF8_CFC
```

It must be followed by POST `8f`, successful MCFG/MADT/table completion and
the normal payload handoff.  The target outcome is then visible SeaBIOS
output, followed by its boot path and RTL8168 iPXE if no earlier boot device
wins.  Reaching `8f` alone proves only the corrected gate, not SeaBIOS entry.

COM1 remains at `0x3f8`, 115200 baud, 8N1.  SeaBIOS still contains its serial
console, so serial input is the preferred control path even while physical
keyboard support remains unresolved.

## Known input limitations retained from B06WD

B06WD proved that the ICH10R KBC decode opens (`LPC_EN 2001 -> 2401`) and
that port `0x64` changes from open-bus `ff` to `00`.  The actual primary
interface test nevertheless returned `03`, and keyboard reset returned ACK
`ff`; PS/2 input did not work.  The generic `pc_keyboard_init()` API returns
the auxiliary-device flag, not an explicit primary-keyboard success value,
so B06WD's later `PNP0303`/FADT-8042 admission is provisional and must not be
read as a successful keyboard test.

All six UHCI controllers and both EHCI controllers were structurally present,
but the B06WD electrical census reported no connected endpoint and asserted
over-current state.  B06WE intentionally makes no speculative GPIO,
over-current, PORTSC or external-power write.  USB keyboard input therefore
also remains unproved.

These input issues do not block the B06WE SeaBIOS experiment because the
serial console is independent of PS/2 and USB.

## First target run

Use the same fixed configuration as B06WD:

```text
board:    MSI X58 Pro-E / MS-7522
flash:    socketed W25Q128.V..M, with known-good recovery chip available
CPU:      Xeon E5645, CPUID 000206c2, microcode 0000001f
DIMM:     BLS4G3D1609DS1S00, sole responding SPD address 0x54
GPU:      AMD Radeon HD 5450 with its physical VBIOS
NIC:      RTL8168 10ec:8168
console:  COM1 0x3f8, 115200 8N1, armed before power-on
```

Retain the complete trace from reset through SeaBIOS/iPXE.  Record the boot
type, CMOS guard state, diagnostic-board POST code, attached input/storage
devices and whether serial input is accepted.  Do not infer a loop from a
ConsolePi replayed suffix; only a temporally coherent capture is milestone
evidence.

## Failure and recovery

A failure before POST `8e` belongs to the inherited path.  POST `86` after
`8e` means one of the three exact read-only checks differs; retain the full
line rather than bypassing the gate.  If the board is unresponsive, use the
established one-second Shelly AC interruption; press the physical power button
if restore leaves it in S5.  The socketed known-good coreboot/vendor chip is
the final recovery path.

## Acceptance boundary

B06WE is ready for a first hardware execution but has not yet been flashed or
run for this record.  One successful payload entry will establish that the
B06WD stop was solely the wrong-BDF check.  It will not by itself qualify
memory, PS/2, USB, SATA media, ACPI runtime or OS boot; the normal milestone
still requires ten cold boots and ten applicable warm resets.

See the [B06WD hardware analysis](../research/msi/b06wd-hw-01-2026-09-07.md)
and the [B06WE release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).
