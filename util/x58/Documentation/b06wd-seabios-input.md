> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WD SeaBIOS input successor

Status: **BUILD-TESTED; HARDWARE PARTIAL PASS ONCE; FAIL-CLOSED IN ACPI BEFORE
SEABIOS; PS/2 NOT FUNCTIONAL IN THE RETAINED RUN**.

B06WD stays on the `x58-pro-e/seabios` line.  The independent EDK2 line starts
from the same B06WC checkpoint but is not part of this experiment.

## Build verification

Two clean release-wrapper executions produced byte-identical B06WD outputs at
coreboot commit `a66879da91968b2fad3fc63fbaf1678651c952d9`.  The complete
suite passed 410/410 tests.  The verified artifact hashes are:

```text
public 4-MiB coreboot base:
dcbaf409103db4e1b032d32b54d36d4d62198cbc78e9e7534de38aad1687b005
builds/experimental/msi-x58-pro-e-b06wd-coreboot-base-4MiB.rom

local deterministic 4-MiB composite:
8a695a85afafb452cbd2339508fcf8abd633cb68128c20c97a19abce364f9e12
blobs-local/msi-x58-pro-e/b06wd/msi-x58-pro-e-b06wd-deterministic-4MiB.rom

local deterministic 16-MiB W25Q128 image:
9c2bd669f34d913d41e9902320e753e13a24ba3d936f1bd0a6e7f1b23e50533f
blobs-local/msi-x58-pro-e/b06wd/msi-x58-pro-e-b06wd-deterministic-w25q128-16MiB.rom
```

Only the public base belongs in the public release area.  The local composite
and full-chip image depend on user-supplied proprietary material and remain
under the ignored `blobs-local/` tree.  See the
[B06WD release manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
for the complete provenance and CBFS inventory.

## Hardware result — B06WD-HW-01 (2026-09-07)

B06WD was executed once on the fixed E5645/SPD54/HD5450/RTL8168 target.  The
authoritative retained continuation identifies commit
`a66879da91968b2fad3fc63fbaf1678651c952d9` and build ID
`X58PROE-B06WD-SEABIOS-INPUT-20260906`.  It begins during the final RAM tests,
then records CBMEM, postcar, ramstage, PCI enumeration/resource allocation,
device initialization/finalization and the start of ACPI construction.

The SATA software-policy hypothesis passed once:

```text
[SATA] B06WD POLICY_CMD 0003->0002 HARDWARE_CMD=0000 HARDWARE_WRITE=0
[SATA] B06VV corrected route + PCS + SCLKCG READY
[AHCI] B06VW READY; dynamic ABAR decoded MEM-only, AE=1, PI=3f; SeaBIOS owns HBA reset, BME and every port operation
```

The KBC decode opened exactly as intended, but the primary-keyboard protocol
failed and no input success may be claimed:

```text
[INPUT] B06WD KBC_DECODE 2001->2401 PORT64=ff->00 WRITE_MASK=0400
Keyboard Interface test failed: 0x3
Keyboard reset failed ACK: 0xff
```

USB admission was structurally complete and read-only, but its electrical
summary was `CLASS=OC_NO_CONNECT`; there is no connected/enabled USB endpoint,
SeaBIOS USB trace or accepted key action in this run.

The run progressed through device finalization and added FADT/FACP and SSDT
evidence, then stopped fail-closed before MCFG and payload entry:

```text
[ACPI] B06WC-ACPI1 MCFG_GATE PCIEXBAR=ffffffff:ffffffff HOST=34058086 COREBOOT_ACCESS=CF8_CFC
[ACPI] B06WC-ACPI1 FAIL reason=PCIEXBAR
```

Inspection of the exact tested source shows that this ACPI-only gate reads the
SAD PCIEXBAR registers from the wrong BDF, `00:00.1`; the already-used X58 SAD
BDF is `ff:00.1`.  The exact ECAM host ID remained readable, so this is a
software false-negative rather than evidence of a lost PCIEXBAR or QPI/IOH
failure.  The next minimal change is to retain the gate at the correct BDF.

Capture provenance matters.  The first prestart artifact retained only two
unidentifying bytes.  The next 699,291-byte artifact is replay-contaminated
(49,492 repetitions of one line) and supplies no coherent milestone claim.
The authoritative continuation is
`research/msi/captures/2026-09-07-b06wd-hw-03-ramstage-acpi-fail.raw`, 38,495
bytes, SHA-256
`2a8572670d58041475571a7707bc2996fcaff83a148c36eecf21c6d59054041e`.
It has no reset-to-training prefix, so exact boot type and early-phase
telemetry remain unclaimed.  Full evidence and all capture hashes are in the
[B06WD-HW-01 record](../research/msi/b06wd-hw-01-2026-09-07.md).

## Hypothesis

B06WC-HW-01 reached trained DDR3, High-QPI, postcar, ramstage, PCI discovery and
resource allocation, then stopped safely at POST `63`.  Its hardware state was
the expected quiescent AHCI state, but coreboot's in-memory command policy was
`0003` because the function still owns legacy I/O BARs as well as its AHCI
ABAR.  B06VV requires the future enable policy to be memory-only (`0002`).

B06WD therefore tests two bounded hypotheses:

1. clearing only `PCI_COMMAND_IO` in the in-memory SATA device policy allows
   the unchanged PCS/SCLK/AHCI stages to run while the hardware command
   register remains `0000`;
2. the already measured Fintek keyboard logical device becomes usable when
   only the documented ICH10R `LPC_EN.KBC` bit is added.  SeaBIOS can then use
   PS/2 input, while its inherited UHCI/EHCI USB-keyboard path remains available
   as the independent alternative.

Neither hypothesis asserts that a physical keyboard works before the target
log proves it.

## Exact delta from B06WC

### SATA software policy

After resource allocation, B06WD admits only the complete B06WC-HW-01 tuple:

```text
D31:F2              8086:3a22, class 010601, normal header
hardware CMD        0000
MAP                 0060
assigned ABAR       exact 2-KiB resource matching BAR5
PCS                 0000
SCLKCG              00000000
D31:F5              absent
coreboot policy CMD 0003
```

It executes exactly:

```c
sata->command &= ~PCI_COMMAND_IO;
```

This changes the software policy from `0003` to `0002`.  The correction
contains no PCI-configuration write and requires the hardware command register,
PCS and SCLKCG to remain unchanged before entering the existing B06VV/B06VW
code.

### PS/2 controller

Late in ramstage, after the final AHCI and USB-controller census, B06WD requires:

```text
ICH10R LPC ID        8086:3a16
LPC_IO_DEC           0010
LPC_EN               2001
port 64 before       ff
Fintek ID/vendor     4105/3419 (F71882FG/F71883FG shared-ID family)
Fintek revision      00
LDN 05 enable        01
LDN 05 base          0060
LDN 05 IRQs          01/0c
LDN 05 mode          83
```

The Fintek functional configuration values are read only; the code changes
only the index used to select LDN5, then restores the previously selected
logical-device number before leaving configuration mode.  B06WD then adds only
`KBC_LPC_EN=0400`, producing `LPC_EN 2001 -> 2401`, verifies the full readback
and requires port `0x64` to stop returning open-bus `ff`.

Coreboot next calls its bounded `pc_keyboard_init(NO_AUX_DEVICE)` path.  The
controller/interface tests and keyboard reset/ACK/BAT/scancode operations are
reported on COM1.  A missing physical keyboard is non-fatal, so reaching POST
`8c` proves that the exact controller admission and bounded probe returned; it
does **not** by itself prove keyboard input.

Only after that admission does ACPI publish `PNP0303`, I/O ports `0x60/0x64`,
IRQ1 and the FADT 8042 flag.  No PS/2 mouse is advertised.

### SeaBIOS and USB

SeaBIOS remains the pinned diagnostic payload and keeps ATA/AHCI, physical VGA
option-ROM, RTL8168 iPXE, hardware IRQ, PS/2, UHCI, EHCI, USB hub, mass-storage
and USB-keyboard support.  B06WD adds a 1000-ms
`etc/ps2-keyboard-spinup` value and retains the 1000-ms deferred USB trace.

B06WD adds no USB GPIO, over-current, PORTSC or controller write.  Consequently
USB success still depends on the inherited ICH10R path observing a connected,
powered endpoint.

## POST contract

| Code | Meaning |
|---|---|
| `87` | Exact B06WC SATA hardware/resource/policy prestate passed; immediately before the software-only policy clear. |
| `88` | Policy is exactly `0002`; hardware CMD is still `0000`, and PCS/SCLKCG are unchanged. |
| `89` | Terminal SATA-policy failure: unexpected prestate, repeated correction, post-check mismatch, or PCS reached without the correction. |
| `8a` | Exact LPC/Fintek prestate passed; immediately before the sole `LPC_EN 2001 -> 2401` write. |
| `8b` | KBC decode and full readback passed; port `0x64` is no longer `ff`. |
| `8c` | Coreboot's bounded primary-keyboard probe returned and input/ACPI admission is ready.  Physical keyboard success is a separate log requirement. |
| `8d` | Terminal input failure: repeated preparation, unexpected LPC/Fintek state, decode/readback failure, later decode mutation, or premature FADT publication. |

After `88`, the inherited PCS/SCLK/AHCI POST sequence remains authoritative.
After transient `8c`, B06WC USB admission, resource-enable completion and the
normal SeaBIOS path continue; `8c` is not intended as a terminal success code.

## Expected serial evidence

The SATA correction must contain all three lines and no hardware-command write:

```text
[SATA] B06VV B06WD_POLICY_PRE ... CMD=0000 ... PCS=0000 SCLKCG=00000000 ... POLICY_CMD=0003 ...
[SATA] B06VV B06WD_POLICY_POST ... CMD=0000 ... PCS=0000 SCLKCG=00000000 ... POLICY_CMD=0002 ...
[SATA] B06WD POLICY_CMD 0003->0002 HARDWARE_CMD=0000 HARDWARE_WRITE=0
```

The input boundary should begin with:

```text
[INPUT] B06WD PRE LPC_ID=3a168086 IO_DEC=0010 LPC_EN=2001 KBC_STS=ff FINTEK=4105:3419 REV=00 LDN5=01 IO=0060 IRQ=01/0c MODE=83 ...
[INPUT] B06WD KBC_DECODE 2001->2401 PORT64=ff->XX WRITE_MASK=0400
Keyboard init...
```

`XX` must not be `ff`.  For actual PS/2 success, require both coreboot and
SeaBIOS evidence, not only POST `8c`:

```text
PS/2 keyboard initialized on primary channel
init ps2port
PS2 keyboard initialized
```

The measured controller may instead repeat the earlier interface diagnostic:

```text
Keyboard Interface test failed: 0x3
```

Coreboot deliberately continues after that interface result and attempts the
physical keyboard protocol.  SeaBIOS performs its own test and can still reject
the port; record its exact response.

For USB success, retain the complete `[USB]` admission block and deferred
`[USBTRACE] BEGIN ... END` journal.  The decisive payload line is:

```text
USB keyboard initialized
```

Finally prove input separately by using the corresponding keyboard to enter
the SeaBIOS boot menu and recording a successful navigation/selection.  A
blinking cursor alone is not input evidence.

## First target tests

Use the fixed bring-up configuration:

```text
board:       MSI X58 Pro-E / MS-7522
flash:       socketed W25Q128.V..M plus known-good recovery chip
CPU:         Xeon E5645, CPUID 000206c2, microcode 0000001f
DIMM:        sole BLS4G3D1609DS1S00 responder at SPD 0x54
GPU:         AMD Radeon HD 5450 with physical VBIOS
NIC:         RTL8168 10ec:8168
console:     COM1 0x3f8, 115200 8N1, capture armed before power-on
```

Run two separately attributable boots:

1. attach one known-good PS/2 keyboard before power-on and remove USB input
   devices;
2. attach one known-good USB-2.0 keyboard directly to a rear motherboard port
   before power-on and remove the PS/2 keyboard.

Retain the complete serial trace from reset through SeaBIOS and the keyboard
action.  Record the exact physical connector, boot type, guard state, displayed
POST trace and whether the key action was accepted.  A one-second Shelly relay
interruption may be used as established, but it is not independent proof of
electrical G3.

Do not invoke the destructive `irqprobe pit` command during these input runs.

## Failure and recovery

- POST `89` is a fail-closed software-policy result.  It occurs before the
  inherited PCS stage is allowed to continue.
- POST `8d` before `8a` means no B06WD KBC-decode write occurred.  A failure
  after `8a` may leave only `LPC_EN.KBC` enabled for that power session.
- Coreboot/SeaBIOS keyboard timeouts are bounded and are not by themselves a
  platform hang.  Continue capturing until a later milestone or a stable
  terminal state is clear.
- On a terminal failure, perform the established one-second AC interruption
  and use the physical power button if AC restore leaves the board in S5.
- If the board no longer executes the diagnostic image, power it down and use
  the socketed known-good coreboot/vendor recovery chip.  Do not weaken an exact
  gate merely from the POST code; retain and inspect its complete COM1 reason.

## Acceptance boundary

This image is now **hardware-executed once but not payload-accepted**.  The one
run proves the bounded SATA software-policy correction and subsequent AHCI
setup for the fixed configuration.  It does not prove a physical keyboard:
PS/2 protocol failed, and USB reported no connected endpoint before the ACPI
stop.  The normal milestone remains ten cold boots and ten applicable warm
resets.  B06WD does not claim native memory initialization, general DIMM
support, complete platform ACPI, PS/2 mouse, USB power-policy reconstruction,
SATA-device operation, SeaBIOS entry or OS boot.
