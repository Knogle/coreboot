> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WF: late interactive USB electrical lab

Status: **SOURCE- AND BUILD-TESTED; TARGET HARDWARE UNTESTED**.

B06WF is a default-off diagnostic successor to B06WE. It preserves the
complete B06WE memory/QPI, PCI/resource, SATA/AHCI, ACPI, physical Radeon
VBIOS, SeaBIOS and RTL8168 iPXE path, but stops at a new serial prompt before
ACPI/table construction and the payload. The operator can inspect the final
coreboot USB state and invoke only five narrowly defined experiments.

The lab is registered at `BS_WRITE_TABLES` / `BS_ON_ENTRY`. This is after
`dev_initialize()` and `dev_finalize()`, and before `write_tables()`. It is
therefore the earliest stable boundary at which the completed PCI/resource
state can be tested without allowing ACPI or SeaBIOS to alter USB ownership.
The older `BS_PRE_DEVICE` handoff monitor still validates and returns normally.

## Why this is interactive

B06WD repeated the same electrical result on every ICH10R USB port:

```text
EHCI PORTSC1..6: 00003030 on both controllers
UHCI PORTSC1..2: 0c80 on all six controllers
EHCI/UHCI CCS:   000
EHCI/UHCI OCA:   fff
EHCI/UHCI OCC:   fff
```

GPIO56 was observed as GPIO input/high (`IOSEL2=0f55fff0`, nominal
`LVL2=15ff00d3`). Reversible GPIO56 output/high and output/low experiments did
not change the EHCI all-port `00003030` state. GPIO56 is therefore retained as
a bounded negative-control experiment, not adopted as board policy.

A later masked live experiment reproduced a vendor-correlated GPIO57/H_PWRGD
sequence: make GPIO57 output while its latch is low, delay, then drive it high.
After the high transition, all ports on both EHCI controllers changed from
`00003030` to `00003020`: live OCA bit 4 cleared globally, sticky OCC bit 5
remained, and CCS remained clear in the halted sample. The companion UHCI
views changed from `0c80` to values such as `0880`, `0883`, and `0983`: OCA bit
10 cleared on all ports while real CCS/LSDA input state was allowed to vary.
This is strong causal evidence for GPIO57/H_PWRGD, but not yet evidence that a
keyboard works through SeaBIOS. B06WF therefore exposes the sequence as an
explicit one-shot command whose verified output/high result may persist into
the payload; it is not an automatic boot write.

Two separate companion-port probes also establish that the released state is
not merely cosmetic. On D26:F0 port 1, a low-speed attachment progressed
`0983 -> 0a82 -> 0983 -> 0987` across reset assertion, reset clear and port
enable. On D26:F2 port 2, a full-speed attachment progressed
`0c8a -> 088b -> 0a8a -> 088b -> 088f` across GPIO57 release and the same
reset/enable operations. Both scripts ended `RUN=ok`. The second script has
FNV `c5a0d015`, SHA-256
`c3b41ebb984811737e95d4cebf23019e6c6008b22ae0e63bed26fb8e3812c84c`,
and its capture is
[`2026-09-07-b06wd-usb-car-gpio57-uhci6-port2-reset-short-run.raw`](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).
These are predecessor CAR experiments, not B06WF or SeaBIOS enumeration.

Other native-input bits in `GP_LVL2` changed during the live sequence. Their
meaning is not established, so B06WF neither names them nor requires the full
level dword to remain constant. Every experiment still requires a distinct
one-shot unlock.

## Exact admission state

Before any write, B06WF requires the complete B06WD-derived tuple:

```text
LPC:       8086:3a16
RCBA:      fed1c001
PM:        00000501 / ACPI_CNTL 80
GPIO:      00000581 / GPIO_CNTL 10
FD/CG:     02000001 / 00000000
PPO/MAP:   0000 / 00000000
UPRWC:     0000
GPIO USE:  197e75ff / 030300ff
GPIO DIR2: 0f55fff0 (or only GPIO57 changed to output after its command)
GPIO LVL2: stable configured-output mask 0002000f must match 15ff00d3;
           GPIO57 is checked separately as low initially or high persistently;
           unrelated/native input bits are sampled but not asserted
EHCI:      both exact IDs/class/header, CMD 0002, valid MMIO BAR,
           D0, EHCIIR2 2002170a, legacy tuple
           00000001/c0040000/00000000 (SMI enables clear; observed status
           bits retained), capability tuple,
           USBCMD 00080000, USBSTS 00001004, USBINTR 0, CONFIGFLAG 0
UHCI:      all six exact IDs/class/header, CMD 0001, valid I/O BAR,
           LEGKEY 2f00, C8/CA 0, USBCMD 0, USBSTS 0020, USBINTR 0
```

GPIO56, GPIO57, ownership, and over-current acknowledgement additionally
require all EHCI ports to equal `00003030` and all UHCI ports to equal `0c80`
before their first write. Decode validation happens before the first RCBA, PM,
GPIO, EHCI-MMIO or UHCI-I/O access.

Any failed preflight, mutation readback or rollback latches a permanent lab
fault for this boot. `continue` then fails closed even if a later read looks
normal. A reset is required.

## Commands and bounded writes

At the `x58-usb>` prompt:

```text
usb status

unlock GPIO56-HIGH
usb gpio56 high

unlock GPIO56-LOW
usb gpio56 low

unlock GPIO57-PWRGD
usb gpio57 pwrgd

unlock USB-OWNER
usb owner

unlock USB-OCACK
usb ocack

lock
continue
unlock RESET
reset warm
reset full
```

The existing monitor's PCI, I/O, memory and MSR commands remain reads only.
Its RAM upload sink remains bounded, data-only and non-executable. B06WF adds
no generic PCI, MMIO, I/O or MSR writer.

### GPIO56 output/high and output/low

The high probe writes only `GP_IO_SEL2[24]` from input to output while the
level latch is already high. The low probe first makes the same verified
output/high transition, then clears only `GP_LVL2[24]`. Both log complete
EHCI/UHCI port bitmaps at 0, 10, 100 and 500 ms. Rollback restores the target
level latch first and the input direction second. It verifies the complete
direction/configuration dwords, configured output levels, and GPIO56 itself;
unrelated native inputs are allowed to change.

The low probe is electrically invasive. Its bounded 500-ms duration and
automatic high-before-input rollback reduce exposure but do not establish
that GPIO56 is safe. Use it only with socketed-flash recovery, no valuable USB
storage attached, and the fixed test configuration below.

### GPIO57/H_PWRGD persistent sequence

The GPIO57 command writes only bit 1 of the bank-2 MSB byte. From the exact
input/low and all-OCA prestate it performs:

```text
GP_IO_SEL2[25] = output, with GP_LVL2[25] still low
wait 65,536 calibrated microseconds
GP_LVL2[25] = high
sample both EHCI controllers and all six UHCI companions through 500 ms
```

The earlier XRS used the numeric operand `0x10000`, but that primitive executes
65,536 uncalibrated `pause` instructions. It is not a microsecond delay and the
UART runner's wall time is not a measurement of it. B06WF intentionally uses
`udelay(0x10000)`—65.536 ms of calibrated wall time—as a conservative, bounded
low phase; the two equal numeric constants have different units.

Success requires target-bit readback of GPIO57 as output/high and all twelve
EHCI plus all twelve companion-UHCI OCA indications clear. Sticky OCC/OCI and
dynamic CCS/LSDA are logged but do not fail the test. On success GPIO57 stays
output/high and `continue` explicitly accepts only that persistent deviation,
then verifies it and the clear OCA state again immediately before ACPI and the
payload. Any target-bit or OCA failure first attempts low-then-input rollback
and permanently latches the lab fault for that boot.

### EHCI CONFIGFLAG ownership probe

The owner probe writes only each EHCI operational `CONFIGFLAG`: `0 -> 1 -> 0`,
one controller at a time. It never writes an EHCI or UHCI `PORTSC`. It logs
both EHCI and all companion-UHCI views at 0 and 20 ms, verifies every
CONFIGFLAG write, restores both values in reverse order, and verifies exact
rollback.

This tests whether companion ownership publication alone exposes a connection
that was hidden while `CONFIGFLAG=0`. It does not reset, start or enumerate a
controller.

### Dedicated over-current latch acknowledgement

The OC test is deliberately last and non-reversible. It uses fixed W1C write
images from the exact admitted state, never a `PORTSC` read-modify-write:

```text
EHCI PORTSC: write 00003020, expect 00003010
UHCI PORTSC: write 0800,     expect 0480
```

These values preserve the already-set EHCI owner/power bits and acknowledge
only the sticky OCC/OCI history. Live OCA is read-only and is expected to
remain asserted in this separate baseline-state experiment. Hardware may
immediately reassert the sticky change bit; that is useful evidence but fails
the exact readback. Because W1C history cannot be reconstructed, invoking
`usb ocack` permanently blocks `continue` and requires a reset, whether its
readback passes or fails.

## Recommended first run

Use the fixed configuration:

```text
board:    MSI X58 Pro-E / MS-7522
flash:    socketed W25Q128.V..M, known-good recovery chip available
CPU:      Xeon E5645, CPUID 000206c2, microcode 0000001f
DIMM:     BLS4G3D1609DS1S00, sole responding SPD address 0x54
GPU:      AMD Radeon HD 5450 with physical VBIOS
NIC:      RTL8168 10ec:8168
console:  COM1 0x3f8, 115200 8N1, capture armed before power-on
USB:      no valuable storage; record exact keyboard/device and physical port
```

Run one hypothesis per boot:

1. Capture the untouched baseline with `usb status`, then `continue`. This
   proves that the late hook admits the real state and that B06WE still reaches
   ACPI/SeaBIOS without mutation.
2. On a fresh boot, run only GPIO56-high, confirm exact rollback, then
   `continue`.
3. On a fresh boot, run only GPIO56-low, confirm samples and target-masked
   rollback, then `continue` only if no fault latched.
4. On a fresh boot, run only GPIO57/H_PWRGD. Require both EHCI and UHCI OCA
   bitmaps to become `000`, then `continue` with GPIO57 persistently high. This
   is the primary SeaBIOS/keyboard experiment.
5. On a fresh boot, run only `usb owner`, confirm exact rollback, then
   `continue`.
6. On a fresh boot, run `usb ocack` last, retain the complete output and reset;
   do not attempt payload continuation.

For each boot retain reset-to-terminal serial, diagnostic-board POST code,
device attachment, port, video behavior, power-cycle type and any external
VBUS observation. Do not combine low, owner, and OC tests in the first evidence
run.

## POST contract and recovery

| Code | Meaning |
|---|---|
| `90` | late B06WF monitor entered |
| `91` | exact baseline admitted; prompt ready |
| `92` | a reversible mutation is beginning |
| `93` | exact reversible rollback completed |
| `94` | continuation gate passed; ACPI/table path resumes |
| `95` | fail-closed fault latched; reset required |
| `96` | non-reversible OC W1C acknowledgement beginning |
| `97` | GPIO57 is verified output/high and all EHCI/UHCI OCA bits are clear |

If serial remains available, use `unlock RESET` followed by `reset full`.
Otherwise use a one- or two-second Shelly mains interruption; the operator
explicitly approved increasing the off interval to two seconds when needed.
A physical power-button press may still be needed if the board remains in S5.
The socketed known-good firmware chip is the final recovery path.

The live GPIO57 result is evidence from the preceding B06WD/XRS research path;
the B06WF implementation itself remains target-hardware untested until its own
complete immutable log exists. Its next milestone is to carry the verified
GPIO57 state into SeaBIOS and determine whether UHCI enumeration and a keyboard
become functional.
