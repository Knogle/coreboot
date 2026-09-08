> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# MSI vendor USB-power control-path forensics

Date: 2026-09-06

Status: read-only schematic, AML and vendor-code audit.  No target was
contacted, no register was written, no firmware source or build configuration
was changed, and no image was built or flashed for this work.

## Result and safety decision

The reviewed evidence does **not** identify a vendor-firmware write that
asserts or deasserts the MSI net `USB_MODE` on ICH10 GPIO56.  In particular,
the board GPIO-policy table in the extracted MSI bootblock has no entry for
GPIO56.  The vendor DSDT has no GPIO operation region and does not name
`USB_MODE`, `5VDRV1_EN`, UP7533, `H_SKTOCC#`, or `CHIP_PWGD`.

The board-level path that can be established from the available Rev2 drawing
is:

```text
ICH10 GP56/F16 ---------------------> USB_MODE
                                         |
                                         +--> EN/pin 4 of all six UP7533AM8

ICH10 SLP_S3# --+                     5VDRV1 -- R471 200K --+
                +--> U11 UP7501 -->                         +--> 5VDRV1_EN
ICH10 SLP_S5# --+                                GND--R470 56K--+
                                                                 |
                                                                 +--> S3#/pin 5
                                                                      of all six
                                                                      UP7533AM8

UP7533 VOUT -- 27K --+--> pair OC# --> native ICH10 OC input
              GND--51K--+
```

This is connectivity, not a reconstructed UP7533 or UP7501 truth table.  The
exact control polarity and thresholds are not established by an exact local
UP7533AM8 data sheet.  `5VDRV1_EN` is not shown as an ICH or Fintek GPIO; it is
a shared board-power node derived from the U11 gate-drive path and also fans
out to other board loads.  There is consequently no evidence-backed ROMMON
write for that net.

The only exact state-dependent vendor GPIO sequence found is for adjacent
**GPIO57**, which sheet 20 names `H_PWRGD`.  The bootblock policy is designed
to configure GPIO57 as a GPIO output initially low outside S3, another
bootblock entry can set it high after a delay, and the SMI sleep dispatcher
contains GPIO57-clear paths for S4 and S5.  All of those instructions address
bit 1 of I/O port `0x53b`.
GPIO56 would be bit 0 of that byte.  Therefore none of this GPIO57 evidence
may be relabelled as `USB_MODE` initialization.

The existing GPIO56-low trial remains unjustified.  The prior calibrated
experiment safely changed GPIO56 from input to output while preserving its
sampled high level; it did not establish the safety or meaning of driving the
opposite level.  Static vendor evidence now strengthens the case for measuring
vendor-time GPIO56 direction/level and the physical rails rather than guessing
a low polarity.

## Reproducible inputs

The proprietary modules remain only in the ignored local `blobs-local/`
corpus.  No vendor bytes are reproduced in this note beyond short instruction
and policy-field descriptions.

| Input | Bytes | SHA-256 |
|---|---:|---|
| MSI `A7522IMS.8F0` | 4194304 | `ee7912da81a3cf47b6bedef7dedfa56cbc31723f5cdd7f0d75a7f64e336c04a8` |
| extracted `amiboot.rom` | 262144 | `da8dc1dcd027e379e1e4d23cf46ca903570557e34f1b3b8e99f10a40f24ffd61` |
| extracted DSDT `amibody_10.rom` | 28290 | `f50bf11a566525a0a05ea52c7958f9f5a25667cc74fbfb8b333ac9b5305211cb` |
| extracted `POST_CSEG.bin` | 50534 | `517ac986b9b97120a70bcbf5b8e6098b6dd4a74db28b8b2ce3fd938e10973873` |
| extracted `SETSVR_CSEG.bin` | 47207 | `03e68824291c61d0a75ba371bfab5529bbc81588d20db924edf0a6e681ad39df` |
| extracted `SMI_BSPCSEG.bin` | 49697 | `935689b4adb46d830b89a2bfd15b441919ab9492eeb5c3921fd311e983c9f721` |
| combined `amibody_1b.rom` | 470008 | `be60656f0218231530ef923453275b4c36c8a91473d3cec2433eae73c9001c83` |
| public third-party MS-7522 Rev2 drawing | 1713041 | `a743a1082c7bf8f1be03ae922828f36b64cf854bf19d5e58535ba5f9c5b2a3d3` |
| Intel ICH10 Family Datasheet 319973-003 | 4369972 | `b7436502827a9ff0cb8c6921682764b33a825d9618dbadf88d28680936dff96b` |

The schematic file title is `7522-20_20081024_A`.  Its page title blocks say
MS-7522 Rev2.0 and `Sheet ... of 49`, but the PDF contains only 44 pages.  The
index on PDF page 1 assigns `GPIO Setting / PCI Routing / Power Map` to sheets
45--46 and `Reset & PWROK Map` to sheets 47--48.  Those decisive policy/map
sheets are absent from this public copy.  The target has separately been
reported as a later board revision, so the Rev2 drawing is a correlation
source, not proof of the target BOM.

GNU objdump 2.46.1 was used in 16-bit x86 mode.  The DSDT was disassembled
with the local coreboot-toolchain IASL 20251212.  Its annotated temporary ASL
output had SHA-256
`0458c1ac18251219a57feeaf43b85c67040d49cae4b497619d51af9067392b62`;
all AML offsets below refer to the original 28290-byte DSDT and are also
printed inline by that disassembler.

Relevant retained hardware inputs are:

| Capture | SHA-256 |
|---|---|
| [`2026-09-06-b06vp-hw-cold-02-gpio-usb-power-census.raw`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) | `7beeab735724fcc93caa7420adf6200b55f302d5dd335fd837fa572042e03e36` |
| [`2026-09-06-b06vp-gpio56-high-ehci2-short-run.raw`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) | `b0ed09cf7a2027a1da77666585020c3ec99058ad03631959b8921bd156a6770e` |
| [`2026-09-06-b06vp-gpio56-high-ehci2-short-rollback.raw`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) | `6b2daf3198b54f5a1de98ae7b5150add7a7172a629e8bbff215f11e2ad2a874b` |
| [`2026-09-06-b06vp-gpio56-high-ehci2-short-discard.raw`](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) | `975d4aafc6ca6934fd698af730646f316a75eabda2742b325355f5b95d175592` |

The earlier board-level analyses remain
[`ich10-usb-overcurrent-vbus-audit-2026-09-06.md`](ich10-usb-overcurrent-vbus-audit-2026-09-06.md)
and
[`usb-power-gpio56-high-followup-2026-09-06.md`](usb-power-gpio56-high-followup-2026-09-06.md).
This note adds bootblock, SMI and AML evidence and records one byte-addressing
correction without rewriting those older immutable observations.

## Exact Rev2 schematic connectivity

### `USB_MODE`

Sheet 20 connects ICH10 `GP56`, package pin F16, directly to `USB_MODE`.
The same node has R607 `X_10K/4` to 3VSB and R618 `10K/4` to ground.  Under
the drawing's `X_` convention R607 is an unstuffed option in the shown Rev2
BOM.  Sheet 34 carries `USB_MODE` to pin 4, labelled `EN`, of all six
UP7533AM8 dual-port USB power switches.

The previously measured B06VP state was:

```text
GPIOBASE      = 00000581
GPIO_CNTL     = 10
GPIO_USE_SEL2 = 030300ff
GP_IO_SEL2    = 0f55fff0  # bit 24 / GPIO56 input
GP_LVL2       = 15ff00d3  # bit 24 / GPIO56 sampled high
```

The calibrated test changed only `GP_IO_SEL2[24]` to output, yielding
`0e55fff0`, while the level stayed high.  All six sampled EHCI2 PORTSC values
remained `00003030`; exact rollback and transaction discard succeeded.  It
did not write `GP_LVL2`, and it gives no evidence for driving GPIO56 low.

### `5VDRV1_EN`

Sheet 38 shows U11, an exact `UP7501`, receiving ICH10 `SLP_S3#` on pin 5 and
`SLP_S5#` on pin 6.  Its 5VCC gate-drive output is the `5VDRV1` node.  That
node feeds R471 `200KST/4`; the other end is `5VDRV1_EN`, with R470
`56KST/4` from `5VDRV1_EN` to ground.  Sheet 34 carries `5VDRV1_EN` to pin 5,
labelled `S3#`, of all six UP7533 devices.

The net is not USB-only.  The same drawing also carries `5VDRV1_EN` to the
enable/reference circuit around U40 UP6264 on sheet 38 and to optional JMicron
rail circuitry on sheets 32 and 33.  Sheet 42 shows an optional
`X_NN-2N7002D` Q48 connection among `H_SKTOCC#`, `CHIP_PWGD`, and
`5VDRV1_EN`; the `X_` prefix marks Q48 as not stuffed in the shown Rev2 BOM.
These connections do not establish the Boolean function of the node on the
later target and must not be converted into a firmware polarity claim.

There is no identified ICH PCI, RCBA, ACPI-PM or Fintek register that directly
drives `5VDRV1_EN`.  Firmware controls the documented ICH sleep transition;
the ICH then drives its physical `SLP_S3#`/`SLP_S5#` outputs and the discrete
board circuit reacts.  That is not equivalent to a software-writable
`5VDRV1_EN` bit.

### Overcurrent feedback

Sheet 34 shows six switch/output groups:

| Switch | USB pair | output net | feedback net |
|---|---|---|---|
| U14 | 0/1 | `RUSB_VCC1` | `OC#8` |
| U17 | 2/3 | `RUSB_VCC2` | `OC#10` |
| U27 | 4/5 | `RUSB_VCC3` | `OC#4` |
| U19 | 6/7 | `RUSB_VCC4` | `OC#6` |
| U70 | 8/9 | `FUSB_VCC1` | `OC#2` |
| U68 | 10/11 | `FUSB_VCC2` | `OC#0` |

Each shown OC node is biased by 27 kOhm from VOUT and 51 kOhm to ground before
reaching the native ICH input path.  Sheet 21 adds 0.1-uF capacitors on the
shown even OC inputs.  This is feedback, not an enable register.  The B06VP
GPIO use-selects leave every documented OC/GPIO multiplexed pin in native OC
mode.  Intel documents no independent ICH10 per-port OC mapping/enable field;
PPO, UPRWC, MAP and CONFIGFLAG have other functions.

The observed `PORTSC=00003030` on every EHCI port and `PORTSC=0c80` on every
UHCI port therefore reports active and latched OC input, but does not by
itself distinguish a switch fault from VOUT being off.  The Rev2 divider can
pull OC low when VOUT is near zero.

## Bootblock GPIO policy table

The extracted `amiboot.rom` begins with `AMIEBBLK`.  File offsets
`0x328d7..0x32a4b` implement a fixed-base GPIO policy interpreter:

- `0x328d7`: load GPIO I/O base `0x500`;
- `0x328da`: table start is segment offset `0x297d`, corresponding to file
  offset `0x3297d`;
- `0x328dd`: exclusive end is segment offset `0x29cb`, corresponding to file
  offset `0x329cb`;
- `0x32a11..0x32a4b`: read/modify/write exactly one GPIO-indexed bit and
  return through the continuation held in DI.

The 78-byte interval `[0x3297d,0x329cb)` contains 39 two-byte records.  Each
record is `(GPIO number, policy flag byte)`.  The code tests that flag byte as
bits 8 and above of the little-endian word:

| Policy flag bit | word bit | register selected | established action |
|---:|---:|---|---|
| 0 | 8 | use-select (`+0x00` or `+0x30`) | set = GPIO mode; clear = native mode |
| 1 | 9 | direction (`+0x04` or `+0x34`) | set = input; clear = output |
| 2 | 10 | level (`+0x0c` or `+0x38`) | set = high; clear = low for an output |
| 3 | 11 | bank-1 input inversion (`+0x2c`) | only visited for a bank-1 input |
| 4 | 12 | bank-1 blink (`+0x18`) | only visited for a bank-1 output |

The ICH10 datasheet confirms that bank 2 covers GPIO32--GPIO63 and bit 0 in
each bank-2 dword corresponds to GPIO32.  The final twelve table records are:

```text
20 05  21 03  22 03  23 03  24 03  25 03
26 03  27 03  30 01  31 01  39 01  3a 00
```

Thus the table includes GPIO32--GPIO39, GPIO48, GPIO49, GPIO57 and GPIO58.
It does **not** include record `38 xx`, i.e. GPIO56.  This loop therefore does
not write `GPIO_USE_SEL2[24]`, `GP_IO_SEL2[24]`, or `GP_LVL2[24]`.

For record `39 01`, GPIO57 is selected as GPIO mode, direction output, level
low.  Before applying that record, offsets `0x32931..0x32944` perform a
GPIO57-only state gate:

```text
inb  0x805
and  0x1c
cmp  0x14
je   skip_GPIO57_record
```

With the vendor PMBASE at `0x800`, port `0x805` is the byte containing
`PM1_CNT.SLP_TYP[12:10]`.  In that byte `0x14` is code `101b`.  Intel defines
that code as S3, and the MSI DSDT's `_S3` package at AML offset `0x6dd3`
likewise returns `0x05`.  The bootblock therefore preserves GPIO57 policy on
the S3 path and applies the output-low policy otherwise.  This special case
is for GPIO57 only.

There is an in-file jump thunk at `0x300aa` to the policy interpreter and a
tail entry at `0x34b65` to that thunk.  These show static bootblock reachability,
but this audit does not claim an execution trace from reset through every
entry.

## Exact GPIO57 high/low paths: a negative control for GPIO56

### Bootblock high path

The bootblock has a separate entry `0x34890 -> 0x300e6 -> 0x35140`.  After a
fixed delay loop, offsets `0x35151..0x3515b` execute:

```text
mov  dx, 0x53b
in   al, dx
or   al, 0x02
out  dx, al
```

`0x53b` is the most-significant byte of the little-endian dword
`GP_LVL2` at GPIOBASE `0x500` + `0x38`.  Byte bit 1 is dword bit 25, hence
GPIO32+25 = GPIO57.  This entry raises GPIO57; it does not touch byte bit 0,
which would be dword bit 24 / GPIO56.  The presence of the entry is exact;
without a vendor execution trace its precise invocation time is not claimed.

### SMI S4/S5 low paths

`SMI_BSPCSEG.bin` has a five-way sleep-state dispatcher at file offset
`0x75f3`.  It indexes the word table at `0x75e9` with `(AX-1)*2`.  The table
contains entry offsets:

```text
00e7 00f4 00f5 0102 0109
```

The exposed module entries at file offsets `0x102` and `0x109`, corresponding
to internal states 4 and 5, call file offsets `0x7697` and `0x769e`.
Both paths reach the helper at `0x7881..0x7888`:

```text
mov  dx, 0x53b
in   al, dx
and  al, 0xfd
out  dx, al
```

This clears GPIO57.  The main SMI sleep path calls the dispatcher at file
offset `0x95f8`.  A second helper at `0x9eaf..0x9ec5`, called at `0xa0b8`,
performs the same GPIO57 clear before additional vendor work.  Again, neither
sequence changes GPIO56.

The schematic maps GPIO57/package AD23 to `H_PWRGD`, while GPIO56/package F16
is `USB_MODE`.  This exact one-bit distinction is why copying the GPIO57
boot/sleep lifecycle to GPIO56 would be an unsupported electrical guess.

The combined `amibody_1b.rom` contains matching embedded copies at file
offsets `0x488bb` and `0x4aeeb`, which provides an independent container-level
cross-check of the two SMI sequences.

## Little-endian correction to the earlier bounded CSEG notes

The complete 16-bit helper in `POST_CSEG.bin` occupies file offsets
`[0x5e1d,0x5e2e)` (end exclusive).  Its exact instruction offsets and bytes
are:

```text
0x5e1d  52           push  dx
0x5e1e  ba 38 05     mov   dx, 0x0538
0x5e21  ec           in    al, dx
0x5e22  24 70        and   al, 0x70
0x5e24  c0 e8 04     shr   al, 4
0x5e27  24 07        and   al, 0x07
0x5e29  0f b6 c0     movzx ax, al
0x5e2c  5a           pop   dx
0x5e2d  c3           ret
```

It therefore returns raw `GP_LVL2[6:4]` as `AX[2:0]`.

The corresponding helper in `SETSVR_CSEG.bin` occupies file offsets
`[0x6dec,0x6dff)` and differs by an explicit inversion:

```text
0x6dec  52           push  dx
0x6ded  ba 38 05     mov   dx, 0x0538
0x6df0  ec           in    al, dx
0x6df1  24 70        and   al, 0x70
0x6df3  c0 e8 04     shr   al, 4
0x6df6  f6 d0        not   al
0x6df8  24 07        and   al, 0x07
0x6dfa  0f b6 c0     movzx ax, al
0x6dfd  5a           pop   dx
0x6dfe  c3           ret
```

It returns the three-bit complement
`(~(GP_LVL2 >> 4)) & 0x07`.  The inversion changes the interpretation of the
three sampled pin levels, but not which GPIOs are sampled.

Earlier immutable notes described those selected bits as GPIO52--GPIO54.
That label is incorrect.  `0x538` is the **lowest-address byte** of the 32-bit
little-endian `GP_LVL2` register at GPIOBASE+`0x38`; byte bits 4--6 are dword
bits 4--6, which map to GPIO36--GPIO38.  GPIO52--GPIO54 would be dword bits
20--22 and reside in byte port `0x53a`, not `0x538`.

The concrete effect of the endian correction is therefore:

- `POST_CSEG.bin` samples GPIO36--GPIO38 with raw polarity;
- `SETSVR_CSEG.bin` samples the same GPIO36--GPIO38 and returns their
  three-bit complement;
- neither helper samples GPIO52--GPIO54, GPIO56/`USB_MODE`, or
  GPIO57/`H_PWRGD`; and
- both helpers are port reads plus arithmetic only, with no GPIO write.

Consequently, the correction has no adverse effect on either older USB-power
conclusion:

- the read still does not sample GPIO56; and
- it is a read/mask sequence, not a USB-power write.

The combined `amibody_1b.rom` contains the byte-identical helpers at
`[0x163d8,0x163e9)` and `[0x3ac33,0x3ac46)`.  Within those copies, the
`mov dx,0x0538` / `in al,dx` instructions are respectively at
`0x163d9` / `0x163dc` and `0x3ac34` / `0x3ac37`.  The masks start at
`0x163dd` and `0x3ac38`; only the latter copy has `not al`, at `0x3ac3d`.
This new note is the append-only correction; the older notes are intentionally
not edited.

For completeness, `POST_CSEG.bin` also has a generic GPIO writer at
`0xb8bc..0xb918`.  Its only direct near-call sites found in that module are
`0xb7c5` with ECX=62 and `0xb7d0` with ECX=63.  The direct absolute
`mov dx,0x53b` scan across the extracted modules finds only the GPIO57 paths
listed above.  This is a bounded static result, not proof that no indirect or
cross-module vendor path can ever pass GPIO56 to a generic helper.

## Vendor ACPI/DSDT result

The extracted `amibody_10.rom` is a checksum-valid DSDT with OEM table ID
`A7522800`.  Its relevant exact AML behavior is:

- `Name (GPBS, 0x0500)` at AML `0x01a9` supplies only the GPIO I/O-resource
  window built at `0x2346..0x237f`; there is no GPIO SystemIO OperationRegion
  based on `GPBS` and no GPIO field access;
- `NPTS` and `NWAK` at `0x15d5` and `0x15dc` are empty;
- `SIOS` at `0x1d33` programs Fintek logical device `0x0a` wake policy
  (`OPT0`, `OPT1`, `ACTR`), not a USB rail;
- the global `_PTS` at `0x6449` calls `PTS`; `PTS` at `0x6e00` calls `SIOS`,
  `SPTS`, and empty `NPTS`;
- `SPTS` at `0x16e8` changes the PS1 SMI enable/status fields and a software
  sleep-button flag, not an ICH GPIO;
- each UHCI ACPI device maps a two-bit `USBW` field to its own PCI-config byte
  `0xc4`; for USB0 the field begins at AML `0x4350` and `_PSW` at `0x437e`
  writes `3` at `0x4388` or zero at `0x4391`;
- the EHCI ACPI devices provide wake descriptions but no board-power GPIO
  method.

Those per-UHCI writes are wake-event policy.  They do not address GPIOBASE,
`USB_MODE`, `5VDRV1_EN`, VBUS or the UP7533 devices.  No matching Super-I/O
control path appears in the schematic: `USB_MODE` goes directly to ICH GP56,
and `5VDRV1_EN` belongs to the discrete sleep/rail circuit.

## What is and is not established

Established:

1. `USB_MODE` is the Rev2 net from ICH GPIO56 to all six UP7533 pin-4 inputs.
2. `5VDRV1_EN` is a shared discrete rail-derived net to all six UP7533 pin-5
   inputs, not an identified firmware GPIO.
3. OC is native feedback; the captured controller state is consistent with
   the OC inputs being low.
4. The audited bootblock GPIO-policy table explicitly omits GPIO56.
5. Vendor boot/sleep code does manipulate GPIO57/H_PWRGD, with a specific S3
   preservation gate and S4/S5 clear paths.
6. The vendor DSDT does not program the two USB power-control nets.
7. The `0x538 & 0x70` reads select GPIO36--GPIO38, not GPIO52--GPIO54 and not
   GPIO56.

Not established:

1. the exact UP7533AM8 `EN`/`S3#` truth table;
2. the target Rev3 stuffing and connectivity versus the Rev2 drawing;
3. vendor-time `GP_IO_SEL2[24]` or `GP_LVL2[24]`;
4. voltage on `USB_MODE`, `5VDRV1_EN`, switch inputs, switch outputs or OC#
   while running vendor firmware versus coreboot;
5. whether a non-audited indirect vendor caller changes GPIO56; or
6. a safe GPIO56-low polarity.

## Next evidence-only step

Before any opposite-level GPIO experiment, collect the following on the same
physical board and connector in vendor and current-firmware states:

```text
GPIOBASE / GPIO_CNTL
GPIO_USE_SEL2, GP_IO_SEL2, GP_LVL2  (decode GPIO56 explicitly)
physical USB_MODE voltage
physical 5VDRV1 and 5VDRV1_EN voltages
physical 5VCC, 5VSB and one UP7533 VOUT/VBUS voltage
the corresponding OC# voltage
EHCI and UHCI PORTSC samples
```

For a state comparison, retain separate cold-S0, S3-resume and S5-to-S0
captures.  Read-only register snapshots alone cannot reveal the analog
`5VDRV1_EN` or VBUS voltages.  A GPIO56-low write remains out of scope until
the target revision, exact switch behavior, vendor GPIO56 state and physical
voltages jointly support it.
