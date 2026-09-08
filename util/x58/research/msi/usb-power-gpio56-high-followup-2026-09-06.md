> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# USB power follow-up after the calibrated GPIO56-high trial

Date: 2026-09-06

Status: schematic, register and bounded vendor-code analysis.  The only live
result discussed here is the already archived B06VP GPIO56-high experiment and
the already archived read-only censuses.  This follow-up performed no target
access, changed no firmware source and produced no image.

## Decision

Do **not** run a GPIO56-low experiment yet, and do not write `5VDRV1_EN` by
any means.

The calibrated experiment proved that changing only
`GP_IO_SEL2[24]` from input to output while preserving the already sampled
high level is electrically tolerated for the observation window.  It did not
alter any of the six EHCI2 port samples:

```text
before direction change: PORTSC[0..5] = 00003030
after  direction change: PORTSC[0..5] = 00003030
```

All three reversible mutations -- temporary EHCI2 BAR, EHCI2 memory decode,
and GPIO56 direction -- rolled back with exact readback, and the transaction
was then digest-gated and discarded.  Therefore:

- an actively held high `USB_MODE` is insufficient to remove the observed
  overcurrent-active state;
- the test does not prove physical VBUS is absent;
- it does not establish UP7533AM8 `EN` polarity or the meaning of its `S3#`
  input; and
- it does not make the opposite pin level safe.

Driving low is qualitatively different from the completed test.  The input
sample was high despite the Rev2 drawing's stuffed pull-down.  On the actual
Rev3 target that can mean a different BOM, an external pull-up, or an active
driver.  Turning the ICH pin into a low output could therefore contend with a
source that the high-preserving trial deliberately did not oppose.

The next discriminator is a vendor/coreboot voltage comparison, not another
register write.

## Reproducible evidence

### Primary and board-reference documents

- Intel, *I/O Controller Hub 10 (ICH10) Family Datasheet*, document
  `319973-003`, October 2008,
  [official PDF](https://www.intel.sg/content/dam/doc/datasheet/io-controller-hub-10-family-datasheet.pdf),
  local SHA-256
  `b7436502827a9ff0cb8c6921682764b33a825d9618dbadf88d28680936dff96b`.
- Third-party public copy of the MSI drawing,
  [MS-7522 schematic](https://manualmachine.com/msi/ms7522/24412054-schematics/),
  title block `MS-7522 Rev2.0 / 7522-20_20081024_A`, local SHA-256
  `a743a1082c7bf8f1be03ae922828f36b64cf854bf19d5e58535ba5f9c5b2a3d3`.
- Archival F71882 data sheet, content revision `V0.24P`, November 2006,
  local SHA-256
  `3c04b57b1f164d0674981199cfefc1e435b1fd2fd514a487c875169e702c1a89`.
  This is not an official-current download and is used only to name the
  configuration-register fields already represented in coreboot's
  `util/superiotool/fintek.c` table.

The PDFs are local research inputs and are not added to the repository.  The
target has been recorded in the established inventory as MS-7522 Rev3.0; the
latest individual capture did not physically re-read the silk screen.  The
Rev2 drawing must therefore remain a correlation source, not a Rev3 BOM.

The current uPI product archive no longer exposes an exact UP7533AM8 page or
data sheet.  uPI's official page for the later, non-identical
[uP7537](https://www.upi-semi.com/upisemi/products/ic/power-switch-ic/power-switch/up7537/)
describes a dual-input USB high-side multiplexer and explicitly calls its EN
active-high.  That is useful family context only; it is **not** evidence that
UP7533 has the same truth table.  uPI's official
[uP7501 page](https://www.upi-semi.com/upisemi/products/ic/power-switch-ic/power-distribution/up7501/)
does corroborate the separate Rev2 `5VDUAL` controller's role in selecting
5VCC/5VSB across ACPI states.

### Calibrated hardware transaction

| Artifact | SHA-256 |
|---|---|
| [`ich10-usb-mode-gpio56-high-ehci2-short-probe-b06vp.xrs`](../scripts/ich10-usb-mode-gpio56-high-ehci2-short-probe-b06vp.xrs) | `81be0d102138b7d4c3c4dd87fb58fbea2fb448f5b6d31fda5e57726bb216da50` |
| [metadata](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) | `611c908fa07ef69f9a79991504891ae2d24e47c78698273dfb38e43d6e4e5ba9` |
| [run transcript](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) | `b0ed09cf7a2027a1da77666585020c3ec99058ad03631959b8921bd156a6770e` |
| [rollback transcript](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) | `6b2daf3198b54f5a1de98ae7b5150add7a7172a629e8bbff215f11e2ad2a874b` |
| [discard transcript](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) | `975d4aafc6ca6934fd698af730646f316a75eabda2742b325355f5b95d175592` |

The sealed program had FNV-1a identity `492f81ca`.  Its successful transaction
identity was `fee81911`; after exact rollback it was `9b9cf8ad`.  There were
three reversible and zero non-reversible mutations.  The delay was the
previously calibrated bounded value `00010000`.

Exact preconditions included:

```text
EHCI2          00:1a.7 = 8086:3a3c
COMMAND                   0000
BAR0                      00000000
PM state                  D0
EHCIIR2                   20001706
LPC            00:1f.0 = 8086:3a16
GPIOBASE                  0581
GPIO_CNTL                 10
GPIO_USE_SEL2             030300ff
GP_IO_SEL2                0f55fff0  (GPIO56 input)
GP_LVL2                   15ff00d3  (GPIO56 sampled high)
```

The experiment changed `GP_IO_SEL2` only to `0e55fff0`; `GP_LVL2[24]`
remained high.  It never wrote an EHCI operational register or an output-level
register.

## Rev2 schematic chain

### The six USB switches

Sheet 34 shows six UP7533AM8 devices.  Each supplies two USB ports and shares
the same two control nets:

| Pin | Rev2 label | Connection shown |
|---:|---|---|
| 1 | `5VCC` | 5VCC input |
| 2 | `5VSB` | 5VSB input |
| 3 | `GND` | ground |
| 4 | `EN` | common `USB_MODE` net |
| 5 | `S3#` | common `5VDRV1_EN` net |
| 6 | `OC#` | pair-specific OC net to the ICH path |
| 7, 8 | `VOUT1`, `VOUT2` | USB VBUS outputs |

The existing audit established that every shown OC node has 27 kOhm to VOUT
and 51 kOhm to ground.  Consequently an unpowered output can itself produce
a low OC input at the ICH.  The repeated `00003030` samples remain consistent
with missing VBUS, but do not measure it.

### `USB_MODE`

Sheet 20 connects ICH10 GPIO56/package F16 directly to `USB_MODE`.  The Rev2
option network is:

```text
3VSB -- R607 X_10K --+
                      +-- USB_MODE -- R618 10K -- GND
ICH10 GPIO56/F16 -----+
```

`X_` is the drawing's not-stuffed option convention, so Rev2 depicts the
pull-up as absent and the pull-down as fitted.  The target's input-high sample
does not match a simple, otherwise-undriven instance of this network.  That
disagreement is a measurement clue, not permission to force the signal.

### `5VDRV1_EN` is a shared board-power net

The important correction to a GPIO-only theory is that `5VDRV1_EN` is not
shown as an ICH or Super-I/O GPIO.  It fans out to several unrelated loads:

```text
                           +--> six UP7533 pin-5 S3# inputs       (sheet 34)
5VDRV1 -- R471 200K --+----+--> U40 UP6264 reference EN          (sheet 38)
                      |    +--> U71 JMB363 1.8-V regulator EN     (sheet 32)
GND ---- R470  56K ---+    +--> U56 JMB322 1.2-V regulator EN     (sheet 33)
                      |
H_SKTOCC# ----+       |
              +-- Q48 +    Q48 is marked X_NN-2N7002D / DNP
CHIP_PWGD ----+            in the Rev2 drawing                    (sheet 42)
```

This is a connectivity diagram, not a Boolean equation.  The exact UP7533,
UP6264 and Q48 thresholds and polarities have not been established.

Specific observations are:

- Sheet 38 derives `5VDRV1` from the UP7501 5VDUAL gate-driver circuit.
  R471 and R470 form a passive bias from that driver net to ground at
  `5VDRV1_EN`; it is not a normal firmware-controlled GPIO level.
- The same sheet connects `5VDRV1_EN` to U40 EN.  U40 produces the named
  `1_5VREF`, `0_9VREF` and `1_2VREF` outputs.  Sheet 39 takes `1_5VREF` to
  the DDR controller's `DDR3_1_5VREF` input and `0_9VREF` to the northbridge
  controller's `NB_1_1VREF` input.
- Sheets 32 and 33 connect the net to the enable pins of optional UP7706
  rails labelled JMB363 1.8 V and JMB322 1.2 V.  The vendor-boot inventory
  observed the populated JMB363 as `197b:2363`, which proves vendor-time
  operation of that device, not the current coreboot-time value of this net.
- Working DDR/QPI and payload execution are circumstantial evidence against
  the entire Rev2 reference path simply being at ground, if the Rev3 target
  retained it.  They do not replace a voltage measurement and do not reveal
  how UP7533 interprets pin 5.

### The optional `H_SKTOCC#` / `CHIP_PWGD` block

Sheet 5 brings `H_SKTOCC#` from CPU-socket pin AG36.  Sheet 42 pulls that net
to 5VSB with R509 and draws it, together with `CHIP_PWGD`, into the two halves
of Q48.  Q48 is labelled `X_NN-2N7002D`; under the schematic's `X_`
convention it is not stuffed in the shown Rev2 configuration.  No truth table
should be inferred for an unpopulated optional network, and the Rev3 board
must be visually checked before even relying on its absence.

Sheet 20 connects `CHIP_PWGD` to ICH10 pin F22, named `PWROK`, and shows a
10-kOhm pull-down R555.  Intel defines PWROK as an **input** indicating stable
platform rails.  Continued execution implies the ICH is seeing the required
asserted power-good condition; there is no identified read-only GPIO bit that
reports the analog voltage at the Q48 footprint.  `H_SKTOCC#` likewise has no
identified software-readable status in the audited ICH/Fintek blocks.

The result is important: there is no justified ROMMON write that can
"initialize" `5VDRV1_EN`.  On Rev2 it is an external shared power-sequencing
node, and its optional discrete contributors are not installed in the drawn
BOM.

## Bounded vendor-code result

The vendor-module result remains negative but useful:

- `RUN_CSEG` `0x6953..0x69a1` resets both EHCIs through temporary BARs and
  then removes decode.  It does not write PORTSC, CONFIGFLAG, PPO, MAP, a USB
  power GPIO, or the external rails.
- `POST2_CSEG` `0x383..0x4b2` selects USB-function topology through FD/MAP;
  `0x4b3..0x547` adjusts EHCI HCSPARAMS for that topology.  The current six
  UHCI plus two six-port EHCI topology is already coherent.
- `POST_CSEG` has a generic GPIO helper at `0xb8bc..0xb918`.  Its direct
  callers identified in the bounded review pass GPIO62 and GPIO63.  The
  read at `0x5e1d..0x5e2d` samples only GPIO52..GPIO54.  No direct GPIO56
  write was found in these paths.

This does not prove that no vendor module ever changes GPIO56.  The decisive
vendor evidence still missing is a live read of its GPIOBASE-relative
`GP_IO_SEL2` and `GP_LVL2`.  The archived vendor state gives:

```text
PMBASE       = 0801
GPIOBASE     = 0501
GPIO_CNTL    = 10
GPIO_USE_SEL = 19fdff01
USE_SEL2     = 030300ff
```

It does not give I/O `0534` or `0538`.  Static code plus the schematic instead
supports a narrower interpretation: vendor firmware may rely on board power
sequencing and passive/default USB-switch controls rather than a missing EHCI
start command.  That remains an inference until the live vendor levels are
captured.

## Exact read-only census

The purpose of the census is to make vendor/coreboot comparisons reproducible
without changing an enable, acknowledging a W1C status bit, or sampling an
address derived from an unchecked base.  Reads of W1C status registers are
safe; writing the observed value back is not.

### Already captured B06VP GPIO/PM state

The 32-operation program
[`ich10-pm-smi-gpio-routing-census-b06vp.xrs`](../scripts/ich10-pm-smi-gpio-routing-census-b06vp.xrs)
(SHA-256
`cd8890aee61bd20e64e5809d3ae8a6c53ed5b8bc388f9238222f7ee256bfdda6`)
ran after the GPIO56-high transaction had been rolled back.  Its
[metadata](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
has SHA-256
`8c0ddae17a22925fd4d690183f4a83a44994179f5b88eb20868a607bf0e04549`;
the [run transcript](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
has SHA-256
`81d22eb61f6bd42757b3ee320d4874117991454e9678aac8bce20436148151e4`.
It recorded zero mutations and was discarded after trace capture.

Exact decode gates were:

```text
00:1f.0 ID       3a168086
PMBASE           00000501
ACPI_CNTL        80
GPIOBASE         00000581
GPIO_CNTL        10
```

Observed configuration and status were:

```text
PCI GPIO_ROUT        00000000
PCI GEN_PMCON_1      0200
PCI GEN_PMCON_2      05
PCI GEN_PMCON_3      02
PCI GEN_PMCON_LOCK   0000
PCI PMIR             00000000

PM1_STS              0801       PM1_EN             0000
PM1_CNT              00000000   PM1_TMR sample     00d3fbe9
GPE0_STS low/high    6eff0000 / 00000000
GPE0_EN  low/high    00000000 / 00000000
SMI_EN               00000000   SMI_STS            00006100
ALT_GP_SMI_EN        0000       ALT_GP_SMI_STS     6eff
GPE_CNTL             00

GPIO_USE_SEL         197e75ff   GP_IO_SEL          e0ea6fff
GP_LVL               02fceeff   GPO_BLINK          00040000
GPI_INV              00000000
GPIO_USE_SEL2        030300ff   GP_IO_SEL2         0f55fff0
GP_LVL2              15ff00d3
```

Pending status was deliberately not cleared.  None of PM1, GPE0, SMI or
alternate-GPI SMI sources was enabled, and `PM1_CNT.SCI_EN` was clear.  This
does not expose `5VDRV1_EN`, `H_SKTOCC#`, or the physical PWROK voltage.

### Exact ICH10 PCI/PM/GPIO address set

For another B06VP-state capture, assert all six decode values before the
first derived I/O read:

| Space | Address | Width | Required/name |
|---|---:|:---:|---|
| PCI 00:1f.0 | `00` | dword | ID exactly `3a168086` |
| PCI 00:1f.0 | `40` | dword | PMBASE exactly `00000501` |
| PCI 00:1f.0 | `44` | byte | ACPI decode exactly `80` |
| PCI 00:1f.0 | `48` | dword | GPIOBASE exactly `00000581` |
| PCI 00:1f.0 | `4c` | byte | GPIO decode exactly `10` |
| PCI 00:1f.0 | `f0` | dword | RCBA exactly `fed1c001` |
| PCI 00:1f.0 | `80` | word | LPC I/O decode exactly `0010` |
| PCI 00:1f.0 | `82` | word | LPC enables exactly `2001` |

Then read these documented registers at their native widths:

| Block | Exact addresses | Widths |
|---|---|---|
| LPC PCI PM policy | `00:1f.0+a0` GEN_PMCON_1; `+a2` GEN_PMCON_2; `+a4` GEN_PMCON_3; `+a6` GEN_PMCON_LOCK; `+ac` PMIR; `+b8` GPIO_ROUT | word, byte, byte, word, dword, dword |
| PM events/control | `0500` PM1_STS; `0502` PM1_EN; `0504` PM1_CNT; `0508` PM1_TMR | word, word, dword, dword |
| GPE | `0520`, `0524` GPE0_STS; `0528`, `052c` GPE0_EN | four dwords |
| SMI | `0530` SMI_EN; `0534` SMI_STS; `0538` ALT_GP_SMI_EN; `053a` ALT_GP_SMI_STS | dword, dword, word, word |
| USB/legacy PM | `053c` UPRWC; `0542` GPE_CNTL; `0544` DEVACT_STS | word, byte, word |
| GPIO bank 1 | `0580` USE; `0584` direction; `058c` level; `0598` blink; `059c` serial blink; `05a0` serial-blink command/status; `05a4` serial-blink data; `05ac` input inversion | eight dwords |
| GPIO bank 2 | `05b0` USE2; `05b4` direction2; `05b8` level2 | three dwords |

`PM1_TMR` is expected to move.  That sample must not be treated as
configuration drift.  `PM1_STS`, GPE status, SMI status, alternate-GPI status,
UPRWC status and DEVACT_STS must only be read in this census.

### Exact RCBA address set

After the exact `fed1c001` RCBA gate, the following is sufficient to cover
the known USB topology, disable, clock, timer/interrupt-decode and previously
required initialization fields without an RCBA write:

| Physical address | Width | Register |
|---:|:---:|---|
| `fed1f1ff` | byte | consumer OIC |
| `fed1f400` | dword | RC |
| `fed1f404` | dword | HPTC |
| `fed1f410` | dword | GCS |
| `fed1f414` | byte | BUC |
| `fed1f418` | dword | FD |
| `fed1f41c` | dword | CG |
| `fed1f420` | byte | FDSW/function-disable SUS-well register |
| `fed1f430` | dword | CIR8 |
| `fed1f50c` | dword | CIR9 |
| `fed1e034` | dword | CIR7 |
| `fed1cf20` | dword | CIR13 |
| `fed1f524` | word | PPO |
| `fed1f52c` | dword | CIR10 |
| `fed1f5f0` | dword | USB MAP |

The earlier B06VP census observed, before later isolated platform work:

```text
OIC=00 HPTC=00000000 FD=00000000 CG=00000000 FDSW=00
PPO=0000 MAP=00000000
CIR8=00000000 CIR9=00000020 CIR7=b2b477cc
CIR13=b2b477cc CIR10=0008c008
```

These are historical B06VP values, not expectations for B06VY or a successor.
In particular, a later image may intentionally have OIC decode enabled.

### Exact Fintek census and the selector-write caveat

A Super-I/O configuration census cannot be literally write-free: entering
configuration mode and choosing an index/LDN necessarily writes selector
ports `4e/4f`.  The configuration **data** registers can remain read-only.
Treat every selector operation as non-reversible, require LPC ID/IO decode
first, restore the known selected LDN, and always leave configuration mode
with `iow 4e aa b`, including from the host executor's failure path.

The already archived
[`fintek-kbc-readonly-b06vp.xrs`](../scripts/fintek-kbc-readonly-b06vp.xrs)
(SHA-256
`b9f9193612f4fcb1513961fa04d955b02737de9b2019f316268f9c1fa3314a4e`)
identified `DID=4105`, `VID=3419`, global register `25=00`, and KBC LDN5
as `30/60/61/70/72/f0 = 01/00/60/01/0c/83`.  Its
[metadata](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
and [raw trace](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index) have
SHA-256 values
`cc3a4ec8e2c8d90330ef5326f269f19ceabfaa340b7410974e5981e0569fcf0e`
and
`41dc3b8b6c3900d6f27d1b1026ffbaa37bc384959cfcf88173fec982c40090cd`.

For a complete power/GPIO comparison, use three separately sealed selector
programs so a failure always has a short exit path:

| Program | LDN | Data indexes to read |
|---|---:|---|
| global identity/mux | none | `20 21 23 24 25 26 27 28 29 2a 2b 2c 2d` |
| Fintek GPIO | `06` | `70 e0 e1 e2 e3 d0 d1 d2 d3 c0 c1 c2 c3 b0 b1 b2 b3 f0 f1 f2 f3` |
| PME/ACPI | `0a` | `30 f0 f1 f4 f5` |

For each index, write only the index to `4e` and read its value from `4f`.
Before selecting LDN6 or LDN0a, select index `07`, assert the current value is
the established `01`, perform the reads, write `01` back to index `07`, and
exit with `AA`.  Never write a value to any listed configuration data index.
The GPIO groups are four-tuples of output-enable, output-data, pin-status and
drive-enable registers.  This census can identify a changed Fintek output but
cannot map it to `5VDRV1_EN` without a matching Rev3 schematic or continuity
measurement.

## What registers cannot answer

The defined census can settle all of the following without another output
mutation:

- whether GPIO56 has returned to input/high after the trial;
- whether GPIO56 is routed as GPIO rather than a native function;
- whether an OC pin was accidentally changed to GPIO mode;
- whether FD, CG, PPO, MAP or UPRWC differs between boots;
- whether a PM/SMI policy is unexpectedly owning the path; and
- whether a Fintek GPIO/multifunction state differs between vendor and
  coreboot.

It cannot directly sample:

- the voltage at `USB_MODE` beyond the ICH GPIO input threshold;
- `5VDRV1_EN`;
- UP7533 pin-1 5VCC or pin-2 5VSB at the device;
- USB VOUT/VBUS;
- the analog OC# voltage;
- Q48 population; or
- the voltage at `H_SKTOCC#` or `CHIP_PWGD`.

No undocumented register should be assigned one of those meanings merely
because its value correlates with a boot.

## Required physical comparison

With the same PSU, CPU, DIMM and unloaded USB connector, record these points
once under the vendor firmware and once at the current coreboot ROMMON:

| Point | Safe observation target |
|---|---|
| target PCB | exact revision; UP7533 marking; Q48/R607/R618 population |
| `USB_MODE` | voltage at R618 or UP7533 pin 4 |
| `5VDRV1_EN` | voltage at U40 EN or one UP7533 pin 5 |
| switch inputs | UP7533 pin-1 5VCC and pin-2 5VSB |
| switch output | one VOUT and the corresponding connector VBUS |
| fault feedback | the matching UP7533 OC# node and ICH-facing side |

Use a high-impedance meter or scope and a fixed ground point; do not probe
dense pins freehand while also issuing ROMMON writes.  An unloaded connector
first separates missing source/enable from a genuine downstream overcurrent.

The decisive comparisons are:

| Vendor versus coreboot result | Consequence |
|---|---|
| same controls and input rails, vendor VOUT only | investigate target-revision switch/BOM behavior and exact UP7533 semantics |
| `USB_MODE` differs and vendor GP_IO_SEL2/GP_LVL2 confirms the same difference | an isolated vendor-matching GPIO test becomes evidence-based |
| `5VDRV1_EN` differs | investigate the external shared power sequence; do not drive it from ROMMON |
| input 5-V rail missing at the switch | repair/initialize the upstream rail path before USB-controller work |
| VOUT near 5 V but OC# remains low | investigate OC divider, switch fault, mapping, or Rev2/Rev3 mismatch |
| VOUT near 0 V and OC# low | the present PORTSC state follows the board-level power path as hypothesized |

## GPIO56-low release gate

A low trial is a no-go until all of these are true:

1. The target silk-screen revision and the populated UP7533/Q48/R607/R618
   parts have been photographed or visually recorded.
2. Vendor `GPIOBASE`, `GPIO_CNTL`, `USE_SEL2`, `GP_IO_SEL2` and `GP_LVL2` are
   captured, with bit 24 explicitly decoded.
3. Vendor and coreboot voltages for `USB_MODE`, `5VDRV1_EN`, both UP7533
   inputs, VOUT/VBUS and one OC# are recorded.
4. Either an exact UP7533 data sheet establishes the input truth table, or a
   vendor-state measurement supplies the level to reproduce.
5. Any trial drives only the vendor-proven level, in a separate board-policy
   option, with exact preconditions, a bounded observation window, rollback
   and a one-second AC-recovery plan.

If the vendor state is output-low with valid VBUS, a low trial may then be
justifiable.  If vendor is input/high or output/high, the completed high trial
already rules out GPIO56 direction alone and low should not be tried.

`5VDRV1_EN` remains measurement-only even after those gates: it is shared with
reference and auxiliary rails and is not shown as a firmware GPIO.

## Net conclusion

The B06VP run is a successful negative result.  It excludes one narrow
hypothesis -- that merely taking ownership of GPIO56 while preserving high is
enough -- and it proves the transactional machinery restores the platform
exactly.  The Rev2 schematic then moves the leading uncertainty away from
EHCI software and toward a two-input, board-level power switch whose second
input is a shared externally biased net.

The most informative next work is zero-write: complete the exact census on
vendor and coreboot, inspect the Rev3 BOM, and measure the six named voltages.
Until that evidence exists, GPIO56-low is neither safe nor diagnostically
well-founded.
