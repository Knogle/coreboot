> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WI: exact-gated vendor-correlated IRQ and ACPI experiment

Status: **SOURCE/BUILD/ARTIFACT PASS; B06WI-HW-01 ROUTING AND USB BOOT
HANDOFF PASS; WINDOWS AGAIN STOPS AT ACPI_BIOS_ERROR 0xA5**.

Update 2026-09-07: the [first captured hardware run](../research/msi/b06wi-jetflash-hw-2026-09-07.md)
reached the new IRQ READY marker, native ACPI table construction, SeaBIOS and
the selected JetFlash boot sector. The operator then supplied a Windows
restart-screen image showing `ACPI_BIOS_ERROR (0xA5)` again. No bugcheck
parameters are visible, so the exact ACPI subtype remains unknown. The
historical design/test expectations below remain preserved: this experiment
proved its routing/USB path, but did not fix the observed Windows stop.

B06WI is a default-off successor to B06WH. It preserves B06WH's measured
RAM/QPI path, sparse fast-postmem checks, automatic GPIO57 USB-power sequence,
quieter SeaBIOS configuration, USB HID/mass-storage support, physical Radeon
VBIOS execution and RTL8168 iPXE option ROM. Its new behavior is deliberately
limited to one exact-gated ICH10 interrupt-routing transaction and the matching
clean-room ACPI description.

The build identity is
`X58PROE-B06WI-VENDOR-IRQ-ACPI-20260907`. Its execution is now recorded in
B06WI-HW-01; its Windows PE attempt still stops with A5.

## Hypothesis and evidence boundary

B06WH reached graphical Windows PE and then displayed
`ACPI_BIOS_ERROR (0xA5)`. The four bugcheck parameters were not captured, so
the exact ACPI subtype and causal defect remain unknown. A source and table
audit nevertheless found a set of internally inconsistent interfaces:

- the DSDT had no root or downstream PCI `_PRT` packages;
- the single BSP Local APIC and ICH10 IOAPIC were both advertised as ID 0;
- the MADT omitted the conventional IRQ0-to-GSI2 override;
- the revision-6 FADT published no GPE0 block; and
- the DSDT/FADT advertised an i8042 although the bounded KBC test returned
  interface value `03` and reset ACK `ff`.

Two read-only vendor-firmware snapshots are bit-identical for RCBA interrupt
policy, LPC routing, GPIO configuration, ELCR and PIC masks; only the
free-running PM timer differs. A separate read-only B06WH CAR transcript gives
the exact predecessor values used by the new gate. B06WI tests the narrow
hypothesis that hardware routing and the ACPI IRQ namespace must describe one
coherent topology before Windows can continue. It does not assert that any
single missing table field caused `0xA5`; in particular, a zero GPE block is
formally optional and is not treated as a proven primary defect.

The detailed read-only comparison is preserved in
[the vendor live-state analysis](../research/msi/vendor-live-acpi-irq-gpio-2026-09-07.md).

## Execution point and exact admission gate

The B06WI transaction runs in ramstage after the inherited B06VY IOAPIC
canonicalization and B06WC quiet-ACPI-mode setup, but before the raw root
preflight and PCI scan. It refuses to write unless the complete tuple matches:

```text
LPC identity:       8086:3a16
RCBA:               fed1c001
OIC:                03
GPIO_ROUT:          00000000
PIRQA..PIRQH:       80 80 80 80 80 80 80 80
IOAPIC selector:    00000000
IOAPIC ID/version:  00000000 / 00170020
IOAPIC RTE 0..23:   low 00010000, high 00000000 (all masked)
D31/D30/D29/D28/D27/D26 IP:
                    03243200 00000000 10004321
                    00214321 00000001 30000321
D31/D30/D29/D28/D27/D26 IR:
                    3210 0000 3210 3210 3210 3210
```

The gate also requires exact PCI identities and `PCI_INTERRUPT_PIN` values for
the admitted D3, D26, D28, D29 and D31 functions. D26:F2 must initially report
pin C; the postcondition requires pin D. Any absent function, stale retained
state, unexpected selector, enabled PIRQ or noncanonical redirection entry
stops before mutation.

## Exact hardware delta

Only the following fields are changed and read back:

| Register | Prestate | Mask/selected field | Target |
|---|---:|---:|---:|
| `D26IP` | `30000321` | `00000f00` | `30000421` |
| `D31IR` | `3210` | `f0ff` | `0232` |
| `D29IR` | `3210` | `f0ff` | `0237` |
| `D28IR` | `3210` | `00ff` | `3201` |
| `D27IR` | `3210` | `000f` | `3216` |
| `D26IR` | `3210` | `00f0` | `3250` |
| IOAPIC ID register | `00000000` | ID field | `01000000` |

The D26IP change selects pin D for D26:F2 and is admitted only with the
matching PCI pin tuple. The IR targets describe these direct GSIs:

```text
D31 A/B/C   -> 18/19/18
D29 A/B/C/D -> 23/19/18/16
D28 A/B/C/D -> 17/16/18/19
D27 A       -> 22
D26 A/B/C/D -> 16/21/18/19
```

The hardware IOAPIC ID changes from 0 to 1 so it no longer collides with the
only Local APIC published by this BSP-only build. Vendor ID 6 is not copied:
the vendor MADT describes twelve Local APIC IDs, while B06WI intentionally
builds with one CPU and therefore selects the smallest unique ID.

`D29IP=10004321` is admitted unchanged. The vendor's `10000321` differs only
in the field for absent D29:F3, so B06WI does not rewrite that inactive field.
All 24 IOAPIC redirection entries stay masked and destination-zero throughout.

## Matching ACPI delta

B06WI publishes a native ACPI view matched to the hardware transaction:

- MADT: one enabled BSP Local APIC ID 0, IOAPIC ID 1 at `fec00000`, GSI base
  0, IRQ0-to-GSI2 edge/high override, and the inherited SCI
  IRQ9-to-GSI9 level/high override;
- FADT: GPE0 at the current decoded `PMBASE+0x20`, therefore `0x0520`, length
  `0x10`; `fill_fadt_extended_pm_io()` also fills `X_GPE0_BLK`; all GPE,
  PM1, SMI and GPIO-route enables remain zero;
- DSDT root `_PRT`: D3 A/B/C/D to 16/17/18/19, D31 A/B/C to 18/19/18,
  D29 A/B/C/D to 23/19/18/16, D28 A/B/C/D to 17/16/18/19, D27 A to 22,
  and D26 A/B/C/D to 16/21/18/19;
- downstream `_PRT`: IOH bridge `NPE3` at `_ADR 0x00030000` and ICH10 root
  port `P0P8` at `_ADR 0x001c0004`, each routing child INTA/B/C/D to
  16/17/18/19 for the HD 5450 and RTL8168 paths; and
- no `PNP0303` node and no FADT 8042 flag in B06WI. USB keyboard support is
  retained; only the unproved PS/2 advertisement is suppressed.

These are direct-GSI packages, not ACPI link-device mutation methods. B06WI
does not enable an interrupt source while constructing the tables.

## Deliberately not copied from the vendor image

B06WI is not a vendor-runtime-state transplant. It deliberately excludes:

- the proprietary DSDT `OperationRegion (BIOS, SystemMemory, 0xffffff00,
  0xff)`, its writable firmware globals, and all vendor AML that depends on
  them;
- SMM/SMI services, `SMI_CMD`, ACPI enable/disable commands, `_PTS`, `_WAK`,
  S3/S4, USB wake, reset and sleep policy;
- the vendor's dynamic twelve-Local-APIC MADT and IOAPIC ID 6;
- vendor PIRQ low nibbles `8a/85/8e/8b/80/8f/80/83`; B06WI keeps all eight
  bytes exactly `80`, so every legacy PIRQ remains disabled;
- PCI `INT_LINE` writes and all IOAPIC redirection-entry writes;
- the vendor D29:F3 pin field for a function absent from the admitted target;
- bulk GPIO `USE_SEL`, `IO_SEL`, `LVL`, `BLINK`, `INV`, second-bank or
  `GPIO_ROUT` writes.

The vendor GPIO `LVL` words mix output latches with live input pins. Even two
matching snapshots cannot make full-word writes safe, so no dynamic GPIO
level is copied. B06WI preserves only the already hardware-proven, pin-scoped
B06WG GPIO57 sequence inherited unchanged from B06WH.

No proprietary firmware, AML, CSI/MINIT module or AMD option ROM is added to
the public image. Local composites still derive only from the user's
hash-pinned MSI input and remain ignored and non-redistributable.

## POST and serial contract

| Code | Meaning |
|---|---|
| `a6` | Exact B06WI route preflight started; no B06WI write has occurred. |
| `a7` | The selected RCBA fields and IOAPIC ID were written and individually read back. |
| `a8` | Full postcondition passed; PCI scan may continue. |
| `a9` | Terminal B06WI failure; serial reports the reason and rollback result. |

The expected success lines are:

```text
[IRQ] B06WI-IRQ-ACPI1 PRE ID=3a168086 RCBA=fed1c001 OIC=03 GPIO_ROUT=00000000
[IRQ] B06WI-IRQ-ACPI1 READY D31IR=0232 D29IR=0237 D28IR=3201 D27IR=3216 D26IR=3250 D26F2_PIN=D IOAPIC_ID=1 RTE_MASKED=24 PIRQ_DISABLED=8 PIRQ_WRITE=0 GPIO_WRITE=0 INT_LINE_WRITE=0 RTE_WRITE=0
```

Later ACPI output must identify IOAPIC 1 and `IRQ0_OVERRIDE=1`; the FADT line
must show `GPE0=0520/10`, a nonzero extended GPE address, and `I8042=0`. After
POST `a8`, the inherited B06WH PCI, automatic USB, SeaBIOS, display and local
boot path should continue without a ROMMON stop.

A failure line has this form:

```text
[IRQ] B06WI-IRQ-ACPI1 FAIL reason=<gate> MUTATED=<0|1> ROLLBACK=<0|1> PIRQ_WRITE=0 GPIO_WRITE=0 INT_LINE_WRITE=0 RTE_WRITE=0
```

POST `86` remains the inherited terminal ACPI-table gate. A later Windows
`0xA5` must be photographed with all four parameters; its recurrence would
disprove neither table construction nor USB operation, but it would show that
this combined hypothesis did not remove the rejected condition.

COM1 remains `0x3f8`, 115200 baud, 8N1 and must be captured before power-on.

## Failure, rollback and recovery

Preflight failures reach `a9` before any B06WI target write. If a selected
write or postcondition fails, the code restores the admitted RCBA fields in
reverse order and returns the IOAPIC ID to 0, verifies the rollback, restores
IOREGSEL to index 0 and then halts at `a9`. `ROLLBACK=1` proves only those
selected fields were restored; it is not a global platform rollback.

There is no automatic retry or bypass. Retain the complete serial reason and
do not weaken an exact gate merely to continue. If the board is unresponsive,
use the established one-second Shelly AC interruption. If AC restore leaves
the board in S5, press the physical power button. The socketed known-good
flash chip and external programmer remain the final recovery path.

## Fixed first-test configuration

```text
board:      MSI X58 Pro-E / MS-7522; exact PCB revision to restamp
flash:      socketed W25Q128.V..M, 16 MiB; known-good chip available
CPU:        Intel Xeon E5645, CPUID 000206c2, stepping 2
microcode:  embedded fixed-test revision 0000001f
DIMM:       BLS4G3D1609DS1S00, 4096 MiB, dual-rank x8
slot:       sole responding SPD address 0x54; physical label to restamp
GPU:        AMD Radeon HD 5450 1002:68f9 with physical VBIOS
NIC:        RTL8168 10ec:8168
input:      known-good USB-2.0 keyboard
medium:     same Transcend 128GB USB device / Hiren's Windows PE test
console:    COM1 0x3f8, 115200 8N1, armed before power-on
recovery:   socketed known-good chip and one-second AC interruption
```

The first run must retain reset provenance, every `a6`--`a8` line, ACPI table
logs, SeaBIOS USB discovery and a photograph/video of any graphical stop. One
successful continuation is a first result, not stability; the normal target
remains ten cold boots, ten applicable warm resets and independent full-memory
validation.

## Build and artifact record

Build with:

```bash
./scripts/build_x58_b06wi.sh
```

The current build produced these files:

```text
68ceaabd643aa16ac436b4de33a91b0f87ace3b5b6bd0bab7ffeb7c64a70ee86  builds/experimental/msi-x58-pro-e-b06wi-coreboot-base-4MiB.rom
8c729dfb55bf3757ee6bee2129cd68fa1b349d2df3538011d5348e5dd7b2be38  blobs-local/msi-x58-pro-e/b06wi/msi-x58-pro-e-b06wi-unpatched-4MiB.rom
132d6987696e4fb9800877611cad25d2e019a3eeabbab8cf2fde06c612572bf6  blobs-local/msi-x58-pro-e/b06wi/msi-x58-pro-e-b06wi-deterministic-4MiB.rom
3d9e6e03e2ed5a59dcdb95b34e2f0cd82e4becee9aa188b81efe2597374c6b04  blobs-local/msi-x58-pro-e/b06wi/msi-x58-pro-e-b06wi-deterministic-w25q128-16MiB.rom
8b21a133a1a795bcd19f7945c23306d37b348fedc8299527e3ce3d66c906ee80  embedded RTL8168 iPXE ROM
```

Each 4-MiB image is 4,194,304 bytes; the W25Q128 image is 16,777,216 bytes.
The focused B06WI source-contract suite passed 8/8 at construction time. This
is a build/static-artifact result only: no programmer read-back, POST trace,
serial log, SeaBIOS entry or Windows result exists for B06WI yet.
