> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VQ-USBTRACE1 deferred SeaBIOS USB trace

## Purpose and isolation

`B06VQ-USBTRACE1` is a diagnostic derivative of B06VQ, not the next SATA
release and not a replacement for B06VR/B06VS/B06VT. It answers the unresolved
question left by the B06VP hardware log: at which exact controller, port or
USB-control-transfer step does SeaBIOS stop before registering a keyboard?

The coreboot hardware policy is B06VQ exactly. The variant deliberately does
not select the later AHCI MAP, PCS or SATA-clock experiments. Its only
coreboot-side addition is bounded read-only telemetry immediately before the
payload. USB controller reset, DMA schedules, port reset, companion ownership,
descriptors and HID setup remain SeaBIOS responsibilities.

Build identity:

```text
X58PROE-B06VQ-USBTRACE1-20260906
X58 Pro-E B06VQ USBTRACE1
```

## Deferred journal

The patch against SeaBIOS `b52ca86e094d19b58e2304417787e96b940e39c6`
creates a deterministic local commit:

```text
5497f43189374647b3b0f282aa497e71c891f3c5
patch SHA-256 f222796ea8029444106ca009045b1528b9a325fa5b84706ca252739d353c232f
```

It records at most 96 fixed 16-byte entries (1536 bytes) in an append-only RAM
journal while USB initialization runs. The timing-sensitive paths perform no
serial output. After `usb_setup()` has finished, SeaBIOS prints one bounded
block:

```text
[USBTRACE] BEGIN count=N dropped=D
[USBTRACE] 0 EVT=.. TYPE=.. BDF=.. PORT=.. RET=.. A=........ B=........
...
[USBTRACE] END
```

This is low-perturbation deferred tracing, not mathematically timing-neutral.
The extra RAM stores and selected status reads still exist. It is intentionally
not a cyclic ring: if 96 entries are exceeded, early evidence is retained and
`dropped` increases. A trace is conclusive only when `dropped=0`, indices are
contiguous, the reported count matches and `END` is present.

The event families are:

| Events | Boundary |
| --- | --- |
| `01`, `02` | complete USB setup begin/end |
| `10`--`17` | EHCI capabilities, HC reset/run, ports and companion handoff |
| `20`--`25` | UHCI HC reset/run and companion ports |
| `30`--`32` | device detection, reset and USB address |
| `33`--`36` | first descriptor, configuration header/body and SetConfiguration |
| `37`--`39` | class-driver choice, HID boot protocol and interrupt pipe |
| `3f` | disconnect/abort observation |

The compact `A` and `B` values are event-specific register or descriptor
snapshots. Preserve the raw block; the checked-in decoder supplies stable
event names without discarding the original values:

```bash
python3 scripts/decode_seabios_usbtrace.py --strict serial.log
python3 scripts/decode_seabios_usbtrace.py --strict --json serial.log
```

## Pre-payload USB gate snapshot

The derivative expands B06VQ's read-only snapshot with the fields needed to
separate a disabled/routed-away controller from a SeaBIOS software failure:

- RCBA Function Disable (`0x3418`) USB mask, Clock Gating (`0x341c`) bit 20,
  Port Power Override (`0x3524`) low 12 bits and MAP (`0x35f0`) bit 0;
- both GPIO native-function overcurrent-routing masks;
- each EHCI HCCPARAMS, configuration `0x61`, legacy ownership `0x68`, selected
  SMI fields at `0x6c/0x70`, and configuration `0x84`;
- each UHCI legacy/configuration words at `0xc0`, `0xc8` and `0xca`.

There is no new write in this snapshot and no new halt gate. The fields are
reported under `[USB-GATE]`; unknown bits retain neutral names.

## Emulator evidence

The exact source patch was exercised on QEMU i440fx with an explicit EHCI
controller and USB keyboard. One complete 24-event journal with `dropped=0`
advanced through HC reset/run, connected port reset, address assignment,
descriptor/configuration transfers, SetConfiguration, HID boot protocol,
interrupt-pipe creation and driver registration. SeaBIOS then printed
`USB keyboard initialized`. This validates instrumentation and decoding only;
QEMU is not evidence that ICH10 routing or a physical MSI port works.

See the immutable summary in
[`research/msi/seabios-usbtrace-qemu-validation-2026-09-06.md`](../research/msi/seabios-usbtrace-qemu-validation-2026-09-06.md).

## First target experiment

Use the fixed E5645, sole SPD-`0x54` DIMM and HD 5450 configuration. Attach a
known USB-2.0 keyboard directly to a rear board port before power-on. Capture
COM1 at 115200 8N1 from reset through the entire SeaBIOS trace. Do not use a
hub for the first run. Test a high-speed USB stick only in a separate run.

Interpret the last successful event as the boundary:

- no connect bit anywhere: physical port/VBUS/overcurrent/native routing;
- EHCI hands ownership to a companion but no UHCI connect: companion routing;
- device reset succeeds but `set_address` fails: control-pipe/DMA/timing;
- address succeeds but descriptor 8 fails: EP0 transfer or device timing;
- configuration succeeds but no `driver_probe`: class/interface parsing;
- HID protocol/pipe succeeds plus `USB keyboard initialized`: firmware-side
  enumeration milestone passed; menu input must still be tested directly.

The SeaBIOS CBFS policy also gives devices 1000 ms attachment time. An invalid
or incomplete journal is still useful raw evidence but must not be reported as
a successful trace.

## Failure and recovery boundary

The flashable W25Q128 image remains vendor-assisted locally and preserves the
existing serial diagnostic path. Recovery uses the socketed known-good B06VP,
B06VQ or vendor chip. No target write or reset was performed while producing
this release. RAM/QPI are treated as the requested development assumption;
their formal cold/warm repetition and full-memory validation remain open.
