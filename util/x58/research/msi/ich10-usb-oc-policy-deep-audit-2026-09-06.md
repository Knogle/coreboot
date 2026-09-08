> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# ICH10R USB overcurrent/power-policy deep audit

Date: 2026-09-06

Status: read-only audit of retained B06VP captures, the unexecuted B06VQ
implementation, the pinned SeaBIOS path, Intel ICH10 documentation, and local
MSI vendor modules.  No target was contacted and no firmware source or image
was changed.

## Executive result

The B06VP evidence shows one coherent external fault state across both ICH10
USB controller families:

```text
EHCI 00:1a.7 ports 1..6 = 00003030
EHCI 00:1d.7 ports 1..6 = 00003030
UHCI 00:1a.0/.1/.2 ports 1..2 = 0c80
UHCI 00:1d.0/.1/.2 ports 1..2 = 0c80
```

For EHCI, `0x3030` is the normal ICH10 owner/power baseline (`0x3000`) plus
Overcurrent Change bit 5 and Overcurrent Active bit 4.  CCS bit 0 and Port
Enable bit 2 are clear.  Port Power bit 12 is hardwired to one on ICH10 and is
not evidence that physical VBUS exists.  Port Owner bit 13 is consistent with
CONFIGFLAG zero and companion-UHCI ownership at the time of the CAR census.

For UHCI, `0x0c80` is reserved/read-as-one bit 7 plus Overcurrent Active bit
10 and the sticky overcurrent indicator/change bit 11.  CCS bit 0 and Port
Enable bit 2 are clear.  The reset census also observed `USBCMD=0000` and
`USBSTS=0020`, i.e. a stopped/halted controller before SeaBIOS starts it.

ICH10 routes the physical `OC[11:0]#` inputs into both EHCI and UHCI status.
The matching values are therefore two software views of the same external
input condition.  They do not prove twelve independent shorts and do not
identify the upstream cause.  Intel states that active EHCI overcurrent
disables the port, which is sufficient to prevent the CCS/descriptor path seen
by SeaBIOS.

The leading board-level explanation remains absent VBUS or an unestablished
USB-switch state.  The available Rev2 schematic biases each shown OC node from
the corresponding switch output, so an unpowered output can also present a
low OC input.  This is a correlation, not proof for the later target PCB.

## Evidence identities and limits

### Hardware captures

| Evidence | SHA-256 |
|---|---|
| B06VP full SeaBIOS/iPXE log | `6659101eb3abd58bdca89f90d5dc2a8ed8824d9b07ebcccdcb4a86888d779e18` |
| B06VP EHCI D29:F7 census | `477d77e31f5b30795768334f753cc1161c5087cd7727b63051ff7b2016a0f09d` |
| B06VP EHCI D26:F7 census | `db132f36f69f6a7733c017fdc956d49dcc9990433c25233c5b029c0ab84761f6` |
| B06VP UHCI D29 census | `96fdc6ebc48878f35b27779ed000278f6ae8ff1ec7b4181683d5b1570240a628` |
| B06VP UHCI D26 census | `5ceed79262b628db2aab3c3b0ad12328bfff9b01b8d614b7c9a595b9dd58727b` |
| B06VP GPIO/USB-power census | `7beeab735724fcc93caa7420adf6200b55f302d5dd335fd837fa572042e03e36` |
| B06VP GPIO56 high-preserving run | `b0ed09cf7a2027a1da77666585020c3ec99058ad03631959b8921bd156a6770e` |

The EHCI scripts temporarily assigned BAR0 and enabled MEM decode only to read
capability/operational state; the UHCI scripts similarly assigned BAR4 and IO
decode.  The transactions were reversible and retained rollback evidence.
They did not write PORTSC or other host-controller operational registers.

There is **no B06VQ target capture** in this repository.  B06VQ and
B06VQ-USBTRACE1 are built artifacts whose source contract was tested, not
hardware results.  Any statement that B06VQ changed `0x3030`, produced a USB
descriptor, or fixed the keyboard would be unsupported.  The only valid
B06VP/B06VQ comparison today is:

| Property | B06VP hardware | B06VQ status |
|---|---|---|
| EHCIIR2 before selective init | `20001706` in both CAR censuses | source derives target from runtime pre-value |
| EHCIIR2 BIOS-required fields | not added by B06VP | masked RMW `(pre & ~2002000c) | 20020008` |
| EHCI/UHCI PORTSC | `3030` / `0c80` | not measured |
| SeaBIOS controllers reached | two EHCI and six UHCI | expected by unchanged payload path; not measured |
| USB descriptor/HID | none in retained B06VP log | not measured |

The local vendor-runtime values quoted below are preserved in project
documentation, but the original raw vendor capture is not present in the
repository.  They are secondary recorded evidence rather than independently
re-decodable inputs:

```text
00:1a.7 EHCIIR2 = 2002130a
00:1d.7 EHCIIR2 = 2002130a
GPIOBASE         = 00000500
GPIO_USE_SEL     = 19fdff01
GPIO_USE_SEL2    = 030300ff
```

### Local source snapshot

| File | SHA-256 at audit time |
|---|---|
| `configs/x58-pro-e-b06vp.config` | `03653f390b7e7d1d03b02718d3d40d13d4386d47f163fcac0f5569394a664209` |
| `configs/x58-pro-e-b06vq.config` | `dddfff5b999b0d7cdfc361339c0e32602832defbdd206b29955a10958baac522` |
| `coreboot/src/mainboard/msi/x58_pro_e/b06vn_pci.c` | `e6fb952fb969aac0e453d5d5c3a108e1765ac4ee182cd6538486a4396edb473f` |
| `coreboot/src/southbridge/intel/i82801jx/ehci_init.c` | `38b3ab4757327fbdddbad96d459dc6c98676cc6954f3ab4644b5312deb5e4f8b` |
| SeaBIOS `src/hw/usb-ehci.c` | `36c80c57f3a9d458318d2261f714f4bb9716c8beeb3ce82c35b26d5a5feed3d0` |
| SeaBIOS `src/hw/usb-uhci.c` | `f2e175a9c866a96df2dd882affeeffa87a3c707e353529ed1520d6458a90a323` |
| SeaBIOS `src/hw/pcidevice.c` | `5328c83279199c5046bbae4a630e19125a92168d1f8ef2492ed27696eaf3b8d8` |

The source tree is shared and under active development.  These hashes bound
this audit; they do not claim that later files with the same names are
identical.

### Public reference material

- Intel ICH10 Family Datasheet 319973-003, local PDF SHA-256
  `b7436502827a9ff0cb8c6921682764b33a825d9618dbadf88d28680936dff96b`.
- Public third-party MS-7522 Rev2.0 schematic, local PDF SHA-256
  `a743a1082c7bf8f1be03ae922828f36b64cf854bf19d5e58535ba5f9c5b2a3d3`.

The public schematic lacks its indexed sheets 45--48 and is not the reported
later target revision.  It may establish named-net correlation, but not target
stuffing, polarity, or permission to drive a pin.

## Exact B06VP controller observations

Both EHCI censuses observed:

```text
PCI command before temporary decode = 0000
BAR0 before temporary decode        = 00000000
PMCSR                                = 0000 (D0)
EHCIIR2                              = 20001706
CAPLENGTH                            = 20
HCIVERSION                           = 0100
HCSPARAMS                            = 00103206 (six ports)
HCCPARAMS                            = 00016871
USBCMD                               = 00080000
USBSTS                               = 00001004
USBINTR                              = 00000000
CONFIGFLAG                           = 00000000
PORTSC1..6                           = 00003030
```

`USBSTS=0x1004` contains halted plus port-change-detect state.  It is
consistent with the stopped pre-SeaBIOS controller and the port condition; it
does not establish a DMA, interrupt-routing, or schedule failure.

Each UHCI group exposed three correct Intel device IDs.  Before temporary
decode, PCI command was `0000` and BAR4 read `00000001`.  With temporary IO
decode, all six controllers read:

```text
USBCMD   = 0000
USBSTS   = 0020 (HC halted)
PORTSC1  = 0c80
PORTSC2  = 0c80
```

The stopped/halted values are expected before payload ownership.  The
electrically significant result is OCA asserted with CCS clear on every port.
The B06VP SeaBIOS log later proves that this pre-payload state did not prevent
software controller setup: it printed both EHCI and all six UHCI init lines,
allocated schedules, then freed every controller after finding no device.
There is no descriptor request, `USB keyboard initialized`, or HID pipe.

Initial discovery is polled.  Missing PIRQ/IOAPIC/ACPI routing therefore does
not explain failure to observe CCS or reach the first descriptor transaction.

## Exact register ownership in the selective coreboot path

### Writes made before SeaBIOS

The generic PCI resource path assigns:

- EHCI D26:F7 and D29:F7 BAR0 at PCI `0x10`, then enables PCI command MEM;
- each UHCI D26:F0/F1/F2 and D29:F0/F1/F2 BAR4 at PCI `0x20`, then enables
  PCI command IO; and
- ordinary PCI enumeration fields such as cache-line/latency/IRQ line where
  generic code requires them.

The selective USB-specific helper changes only EHCI PCI config `0xfc`
(`EHCIIR2`) on D26:F7 and D29:F7.  Intel section 17.1.36 requires bit 29, bit
17, and bits 3:2=`10b`.  The implementation preserves all other bits:

```text
target = (before & ~0x2002000c) | 0x20020008
```

For the B06VP-observed pre-value `20001706`, the derived target is
`2002170a`; it is not a blind copy of the vendor `2002130a` literal.  The
helper does not touch PORTSC, CONFIGFLAG, an OC mapping, or VBUS power.

At the coreboot-to-SeaBIOS boundary, the selective path deliberately leaves
PCI bus mastering clear.  SeaBIOS assumes ownership and enables it.

### Read-only telemetry

The current B06VQ telemetry reads but does not modify:

- RCBA `0x3418` FD and its USB function-disable bits;
- RCBA `0x341c` CG bit 20;
- RCBA `0x3524` PPO low twelve bits;
- RCBA `0x35f0` MAP bit 0;
- PMBASE `0x3c` UPRWC;
- GPIOBASE `0x00` GPIO_USE_SEL and `0x30` GPIO_USE_SEL2;
- EHCI PCI `0x04`, `0x10`, `0x54`, `0x61`, `0x68`, `0x6c`, `0x70`, `0x84`,
  and `0xfc`;
- EHCI capability CAPLENGTH/HCIVERSION/HCSPARAMS/HCCPARAMS and operational
  USBCMD/USBSTS/USBINTR/CONFIGFLAG/PORTSC;
- UHCI PCI `0x04`, `0x20`, `0xc0`, `0xc8`, and `0xca`; and
- UHCI USBCMD/USBSTS/USBINTR/PORTSC1/PORTSC2.

The current function logs only the two GPIO use-select dwords.  It does not
currently log GPIO direction or level.  Those are present in the separate
B06VP ROMMON census, not in a B06VQ target trace.

The captured/global facts relevant to OC are:

- `PPO=0`: no selected port is electrically disconnected by PPO;
- `MAP=0`: controller 6 remains at D26:F2, matching the observed topology;
- all USB FD bits clear: no USB PCI function is disabled;
- `CG[20]=0`: the documented static EHCI clock gate is not asserted; and
- every OC-multiplexed pin remains selected for native OC input.

ICH10 documents no separate per-port OC-enable or OC-map register.  PPO,
UPRWC, MAP, CONFIGFLAG and EHCIIR2 have distinct documented functions and must
not be relabelled as VBUS controls.

## Exact SeaBIOS mutations

For each EHCI, the pinned SeaBIOS path:

1. verifies/enables BAR0 MEM decode and sets PCI command Bus Master;
2. writes CTRLDSSEGMENT zero if 64-bit addressing capability is advertised;
3. writes USBCMD.HCRESET while clearing asynchronous/periodic schedule enable,
   then polls reset clear with a 250-ms timeout;
4. writes USBINTR zero;
5. writes PERIODICLISTBASE and ASYNCLISTADDR;
6. writes USBCMD RUN+ASE+PSE;
7. writes CONFIGFLAG one, routing high-speed ownership to EHCI;
8. waits 20 ms and reads all six PORTSC values; and
9. only if CCS is set, writes reset/owner/enable fields needed for that port.

ICH10 PORT_POWER reads one, so the generic `if clear, set PORT_POWER` branch
does not establish external 5-V power.  With CCS clear, SeaBIOS does not enter
the port-reset or descriptor path.  It stops a controller with no devices and
frees its schedules.

For each UHCI, SeaBIOS:

1. verifies/enables BAR4 IO decode and sets PCI command Bus Master;
2. writes PCI USBLEGSUP `0xc0` with the defined R/WC mask;
3. writes USBCMD.HCRESET, then USBINTR zero and USBCMD zero;
4. writes SOFMOD, FLBASEADD and FRNUM;
5. writes USBCMD Run/Stop + Configure Flag + 64-byte Max Packet; and
6. after all EHCIs finish, reads both PORTSC registers and only resets a port
   if CCS is set.

SeaBIOS does not program RCBA PPO/MAP/FD/CG, PMBASE UPRWC, GPIO use/direction/
level, or EHCIIR2.  It also does not presently claim EHCI legacy ownership by
writing PCI `0x68/0x6c/0x70`; the source retains a TODO comment.  This is not
the leading fault because initial port discovery is polled and the B06VP
captures already show CCS clear/OCA active before payload execution.

## Vendor policy found in reviewed modules

The local vendor modules have these identities:

| Module | Size | SHA-256 |
|---|---:|---|
| `RUN_CSEG.bin` | 65536 | `dbabc6b1385be15fc6f83deb14bbe39c48166362539626e8d4a87bf52e43a291` |
| `POST2_CSEG.bin` | 9552 | `ae30d9f4d29aac8175d0d6892ac6f2fc3cb93bd5949065d756c37f8fd227410b` |
| `POST_CSEG.bin` | 50534 | `517ac986b9b97120a70bcbf5b8e6098b6dd4a74db28b8b2ce3fd938e10973873` |
| `amiboot.rom` | 262144 | `da8dc1dcd027e379e1e4d23cf46ca903570557e34f1b3b8e99f10a40f24ffd61` |
| `SMI_BSPCSEG.bin` | 49697 | `935689b4adb46d830b89a2bfd15b441919ab9492eeb5c3921fd311e983c9f721` |

Bounded disassembly results:

- `RUN_CSEG.bin` `0x6953..0x69a1` iterates D29:F7 and D26:F7, assigns a
  temporary EHCI BAR, enables PCI MEM decode, derives the operational base
  from CAPLENGTH, sets USBCMD.HCRESET, polls it clear, and removes decode/BAR.
  It does not write PORTSC, CONFIGFLAG, PPO, MAP, or a power GPIO.  Its poll is
  unbounded and is not suitable for direct reuse.
- `POST2_CSEG.bin` `0x383..0x4b2` derives USB function enable/topology policy
  from setup and updates RCBA FD/MAP.  `0x4b3..0x547` temporarily decodes each
  EHCI and adjusts HCSPARAMS to that topology.  The B06VP hardware already
  exposes two coherent six-port EHCIs and all six UHCIs.
- The `POST2_CSEG.bin` table path `0x77..0x97` uses fixed IO base `0x800`, the
  vendor PMBASE, not GPIOBASE `0x500`; it is PM status/enable policy and is not
  a GPIO56 operation.
- `POST_CSEG.bin` `0xb8bc..0xb918` is a generic GPIO use/direction/level
  helper.  Its direct near callers at `0xb7c5` and `0xb7d0` pass GPIO62 and
  GPIO63.  This bounded call-site result does not prove that no indirect or
  cross-module caller exists.
- The full bootblock GPIO interpreter lies at `amiboot.rom`
  `0x328d7..0x32a4b`, using the 39-record table `[0x3297d,0x329cb)`.  The table
  has a GPIO57 record but no GPIO56 record, so this audited table does not
  program GPIO56 use, direction, or level.
- The bootblock path `amiboot.rom` `0x35151..0x3515b` sets port `0x53b` bit 1,
  i.e. GP_LVL2[25]/GPIO57.  SMI helper `SMI_BSPCSEG.bin`
  `0x7881..0x7888` clears the same bit on its reviewed sleep paths.  The
  schematic names GPIO57 `H_PWRGD`, not `USB_MODE`; it cannot be copied to
  GPIO56.
- `POST_CSEG.bin` helper `[0x5e1d,0x5e2e)` reads port `0x538 & 0x70` and
  returns raw GP_LVL2[6:4], which is GPIO36--GPIO38.  It does **not** read
  GPIO52--GPIO54 or GPIO56.  The little-endian byte correction is material to
  naming but does not alter the USB conclusion.

The reviewed vendor paths establish early host reset, topology policy and a
separate GPIO57 power-good lifecycle.  They do not reveal a documented
GPIO56/USB_MODE write or a software-controlled `5VDRV1_EN` register.

## Board power-chain correlation

In the Rev2 schematic only:

- GPIO56/package F16 is net `USB_MODE` and reaches pin 4 (`EN`) of six
  UP7533AM8 dual-port switches;
- each switch also receives `5VDRV1_EN` on pin 5 (`S3#`), 5VCC/5VSB on pins
  1/2, and reports OC# on pin 6;
- U11 UP7501 accepts ICH SLP_S3#/SLP_S5# and produces 5VDRV1; R471/R470 derive
  the named `5VDRV1_EN` node from it; and
- the shown OC nodes use 27 kOhm from VOUT and 51 kOhm to ground, so VOUT near
  zero can pull the observed OC input low.

No reliable UP7533AM8 truth table or target-revision BOM is available.  These
net labels do not establish EN/S3# polarity.  `5VDRV1_EN` is not shown as an
ICH/SIO GPIO and must not be invented as one.

The B06VP GPIO census measured GPIO56 selected as GPIO, configured input, and
sampling high:

```text
GPIOBASE      = 00000581
GPIO_CNTL     = 10
GPIO_USE_SEL2 = 030300ff
GP_IO_SEL2    = 0f55fff0  (bit 24 / GPIO56 = input)
GP_LVL2       = 15ff00d3  (bit 24 / GPIO56 = high)
```

The calibrated high-preserving trial changed only GP_IO_SEL2[24] from input
to output while retaining the existing high level.  All EHCI2 PORTSC remained
`00003030`; rollback and discard succeeded.  It disproves only the hypothesis
that output direction plus the already-high level was sufficient.  It says
nothing safe about the opposite level.

## Ranked next hypotheses

### 1. Highest confidence and safest: observe B06VQ without another USB write

Hypothesis: the documented EHCIIR2 RMW is necessary platform hygiene but will
not remove an externally asserted OCA condition.

Use the already isolated B06VQ-USBTRACE1 image, one directly attached known-
good USB 2.0 device, and one complete serial capture.  Verify exact EHCIIR2
derived readback, then record all pre-SeaBIOS PORTSC and the deferred SeaBIOS
trace.  Simultaneously measure VBUS at that connector if possible.

Discriminators:

| Result | Meaning |
|---|---|
| EHCIIR2 passes; `3030/0c80` persists; VBUS ~0 V | board power chain remains primary |
| EHCIIR2 passes; OCA clears; CCS/descriptor appears | EHCIIR2 was a prerequisite; repeat before claiming causality |
| VBUS ~5 V while every OCA remains active | investigate OC bias/wiring or revision mismatch, not a VBUS-off assumption |
| OCA clear but CCS clear | verify physical port/device mapping next |

Do not acknowledge OCC/OCI before this capture.  The change bits preserve
history, while OCA is read-only and follows the pin.  A naive PORTSC
read-modify-write is unsafe because the registers combine R/W, read-only and
write-one-to-clear fields.

### 2. Read-only vendor/current electrical correlation

Capture vendor and current firmware on the same board/connector at cold S0,
then separately across S3 resume and S5-to-S0:

```text
GPIOBASE, GPIO_CNTL
GPIO_USE_SEL2, GP_IO_SEL2, GP_LVL2
EHCI and UHCI PORTSC
physical USB_MODE, 5VDRV1, 5VDRV1_EN, 5VCC, 5VSB, VOUT/VBUS and OC#
```

The missing vendor direction/level snapshot and physical voltages are more
diagnostic than another speculative register write.

### 3. Deferred and not presently justified: GPIO56 opposite-level trial

GPIO56-low is not safely justified.  It requires a matched target schematic/
BOM, exact switch truth table or measurements, vendor GPIO56 state, and a
recoverable isolated transaction.  No write to GPIO56, OC muxes, PPO, UPRWC,
MAP, FD, CG, PORTSC, or an invented `5VDRV1_EN` register follows from the
current evidence.

## Bottom line

Controller discovery, BAR decode, D0 state, topology and SeaBIOS host startup
are already proved on B06VP.  The failure boundary precedes descriptors: every
port has CCS clear and the common external OC input asserted.  B06VQ adds one
correct EHCIIR2 BIOS-required-field operation, but has no hardware result yet.
The next safe experiment is therefore observation and voltage correlation,
not broader ICH10 initialization or a guessed board-power write.
