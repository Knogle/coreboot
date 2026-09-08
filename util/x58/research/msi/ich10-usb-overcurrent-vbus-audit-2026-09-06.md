> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# ICH10 USB overcurrent and MSI USB-power audit

Date: 2026-09-06

Status: register-level analysis of the B06VP hardware captures, public Intel
documentation, a public third-party MS-7522 Rev2.0 schematic, and locally
supplied MSI CSEG modules.  This note contains no vendor executable bytes and
records no target write.

## Result

The decisive B06VP observation is not merely "no USB connect": every exposed
ICH10 USB port reports an **active-low overcurrent input asserted**.

```text
EHCI ports 0..11: PORTSC = 00003030
UHCI ports 0..11: PORTSC = 0c80
```

The normal reset values are `00003000` for EHCI and `0080` for UHCI.  The
extra EHCI `0030` and UHCI `0c00` are the active and sticky-change
overcurrent bits.  Intel states that an active EHCI overcurrent condition
automatically disables the port.  This state is sufficient to explain why
SeaBIOS sees all controllers but no descriptor or keyboard.

The most plausible board-level explanation is missing USB VBUS, not twelve
independent downstream shorts.  The public MS-7522 Rev2.0 schematic shows
each dual-port power-switch `OC#` signal biased from its USB output: a 27-kOhm
resistor from VOUT to `OC#` and 51-kOhm from `OC#` to ground.  With VOUT near
5 V the divider produces about 3.27 V; with the output off it pulls `OC#`
low.  On that circuit, the ICH cannot distinguish "VBUS output is off" from
the switch asserting its active-low fault output by looking at PORTSC alone.

This is a high-value hypothesis, not yet a board-revision proof.  The
available schematic is a third-party copy labelled **MS-7522 Rev2.0**, while
the target X58 Pro-E is believed to be a later v3.x board.  The exact target
revision and BOM have not been matched, and a reliable public UP7533AM8
datasheet was not found.  No GPIO should be driven from this schematic alone.

## Reproducible inputs

### Public documentation

- Intel, *I/O Controller Hub 10 (ICH10) Family Datasheet*, document
  `319973-003`, October 2008, [official PDF](https://www.intel.sg/content/dam/doc/datasheet/io-controller-hub-10-family-datasheet.pdf),
  local SHA-256
  `b7436502827a9ff0cb8c6921682764b33a825d9618dbadf88d28680936dff96b`.
- Public third-party [MS-7522 schematic](https://manualmachine.com/msi/ms7522/24412054-schematics/),
  title block `MS-7522 Rev2.0 / 7522-20_20081024_A`, local SHA-256
  `a743a1082c7bf8f1be03ae922828f36b64cf854bf19d5e58535ba5f9c5b2a3d3`.

The local PDF and schematic copy are research inputs only and are not added
to version control.

### Hardware captures

- [EHCI1 reset/port census](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [EHCI2 reset/port census](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [D29 UHCI reset/port census](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [D26 UHCI reset/port census](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
- [GPIO/USB-power census](../../Documentation/PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)

All temporary BAR and command-decode changes in those ROMMON tests rolled
back cleanly.  No host-controller operational register was written.

### Local MSI modules

The following user-supplied files remain under the ignored `blobs-local/`
tree.  Only identities, offsets and semantics are recorded here:

| Module | Size | SHA-256 |
|---|---:|---|
| `RUN_CSEG.bin` | 65536 | `dbabc6b1385be15fc6f83deb14bbe39c48166362539626e8d4a87bf52e43a291` |
| `POST2_CSEG.bin` | 9552 | `ae30d9f4d29aac8175d0d6892ac6f2fc3cb93bd5949065d756c37f8fd227410b` |
| `POST_CSEG.bin` | 50534 | `517ac986b9b97120a70bcbf5b8e6098b6dd4a74db28b8b2ce3fd938e10973873` |

GNU objdump 2.46.1 was used in 16-bit Intel syntax.

## Exact PORTSC decoding

### EHCI

ICH10 EDS section 17.2.2.9, printed pages 645--648, defines EHCI PORTSC with
reset value `00003000`:

| Bit | Name | Access | B06VP |
|---:|---|---|---:|
| 13 | Port Owner | R/W | 1, companion UHCI owns the port while CONFIGFLAG is 0 |
| 12 | Port Power | RO, always reads 1 on ICH10 | 1 |
| 5 | Overcurrent Change | R/WC | 1 |
| 4 | Overcurrent Active | RO | 1, external OC input currently low |
| 0 | Current Connect Status | RO | 0 |

Consequently `00003030 = reset 00003000 + OCC + OCA`.  Bit 12 is hardwired
to one and is **not evidence that 5-V VBUS is physically present**.  Section
17.2.2.8 gives CONFIGFLAG reset value zero, which routes the ports to their
UHCI companions and explains Port Owner at reset.

OCC can be acknowledged by writing one to bit 5.  OCA bit 4 cannot be cleared
by software; it follows the pin and returns to zero only after the condition
is removed.  Intel explicitly says OCA=1 causes automatic port disable.

### UHCI

ICH10 EDS section 16.2.7, printed pages 609--610, defines UHCI PORTSC with
reset value `0080`:

| Bit | Name | Access | B06VP |
|---:|---|---|---:|
| 11 | Overcurrent Indicator/change | R/WC | 1 |
| 10 | Overcurrent Active | RO | 1, external OC input currently low |
| 7 | Reserved | RO, always reads 1 | 1 |
| 0 | Current Connect Status | RO | 0 |

Thus `0c80 = reset 0080 + OCI + OCA`.  Again, clearing bit 11 can remove only
the history bit; it cannot change active bit 10.

EDS section 5.19.8.1, printed page 206, states that `OC[11:0]#` are routed
directly to both EHCI and UHCI and an event is recorded in both families.
The matching EHCI/UHCI result is therefore one physical-input observation,
not two independent controller-software failures.

## OC pins, muxing and the absence of an OC-map register

EDS section 2.10, printed page 57, maps the pins as follows:

| USB OC input | Multiplexed GPIO | GPIO-use bit |
|---|---:|---:|
| OC0# | GPIO59 | `GPIO_USE_SEL2[27]` |
| OC1#..OC4# | GPIO40..GPIO43 | `GPIO_USE_SEL2[8:11]` |
| OC5#..OC7# | GPIO29..GPIO31 | `GPIO_USE_SEL[29:31]` |
| OC8#..OC11# | GPIO44..GPIO47 | `GPIO_USE_SEL2[12:15]` |

For each mux bit, zero selects the native OC function and one selects GPIO.
The B06VP values are:

```text
GPIO_USE_SEL  = 197e75ff
GPIO_USE_SEL2 = 030300ff
```

Every OC-related bit listed above is zero.  The vendor runtime has different
unrelated bank-1 policy (`GPIO_USE_SEL=19fdff01`) but the same conclusion for
all OC mux bits, and its bank-2 value is also `030300ff`.  The OC signals are
already in native mode; changing these mux bits is not a supported fix.  EDS
table 3-3 additionally requires external pull-ups for the relevant
suspend-well inputs.

No independent per-port OC enable or OC mapping register is documented for
ICH10.  The EDS describes direct pin routing.  The nearby USB registers have
different purposes:

- `RCBA+0x3524` PPO, reset `0000`, electrically disconnects selected ports
  only when a corresponding bit is one.  B06VP and vendor both read zero.
- `PMBASE+0x3c` UPRWC controls writes to per-port chipset registers.  A zero
  read does not power ports off and should not be changed for this test.
- `RCBA+0x35f0` MAP, reset zero, only remaps UHCI controller 6 between
  D26:F2 and D29:F3 and requires matching HCSPARAMS.  B06VP's zero agrees
  with D26:F2 and the observed six-plus-six topology.
- `RCBA+0x3418` FD function-disable bits are all clear for the USB functions.
- `RCBA+0x341c` CG bit 20 is the static EHCI clock-gate control.  It is clear,
  so the EHCI clocks are not statically gated.
- EHCI CONFIGFLAG controls UHCI/EHCI data-pair ownership, not VBUS or OC.

Both EHCI controllers and all six UHCIs are in D0 and accepted temporary
decode.  Those facts rule out PCI power state, function disable and static
clock gating as the first-order explanation.

## Board-level USB power correlation

The public Rev2.0 schematic gives the following useful, but revision-limited,
topology.

### Six dual-port power switches

Sheet 34, `USB POWER`, shows six UP7533AM8 devices, each supplying two USB
ports.  Pins 1 and 2 receive 5VCC and 5VSB, pins 7 and 8 are VOUT, pin 4 is
`EN=USB_MODE`, pin 5 is `S3#=5VDRV1_EN`, and pin 6 is `OC#`.

| Switch | Port pair | VBUS net | OC net |
|---|---|---|---|
| U14 | 0/1 | RUSB_VCC1 | OC#8 |
| U17 | 2/3 | RUSB_VCC2 | OC#10 |
| U27 | 4/5 | RUSB_VCC3 | OC#4 |
| U19 | 6/7 | RUSB_VCC4 | OC#6 |
| U70 | 8/9 | FUSB_VCC1 | OC#2 |
| U68 | 10/11 | FUSB_VCC2 | OC#0 |

Each shown OC node has the 27-kOhm-to-VOUT / 51-kOhm-to-ground network.  Sheet
21 shows only the even-numbered OC nets entering the ICH in this revision and
adds a 0.1-uF capacitor to ground on each.  How the unused/paired inputs
correspond to every PORTSC instance remains unresolved and must not be filled
in by assumption.

### Common controls

Sheet 20 connects ICH10 GPIO56, package pin F16, to `USB_MODE`.  Sheet 42
derives `5VDRV1_EN` through discrete circuitry involving `H_SKTOCC#` and
`CHIP_PWGD`; it is not shown as a simple Super-I/O USB-enable output.

For the Rev2.0 BOM, the USB_MODE node appears to have an unstuffed 10-kOhm
pull-up (R607) to 3VSB and a stuffed 10-kOhm pull-down (R618).  The exact
UP7533 polarity is not established from a reliable datasheet, so even that
observation is not authorization to drive it.

B06VP measured:

```text
GPIO_USE_SEL2 = 030300ff  -> GPIO56 is the unmultiplexed GPIO
GP_IO_SEL2    = 0f55fff0  -> bit 24/GPIO56 is input
GP_LVL2       = 15ff00d3  -> bit 24/GPIO56 currently reads high
GPIO_CNTL     = 10        -> GPIO enabled, GLE lockdown clear
```

EDS sections 13.10.10 and 13.10.11 define bit 24 as GPIO56, input when
GP_IO_SEL2[24]=1, and make GP_LVL2[24] a valid pin-level read in GPIO mode.
The input/high observation conflicts with a simple interpretation of the
Rev2.0 pull-down BOM.  It may reflect a target revision/BOM difference,
another driver on the net, or incomplete interpretation of the switch.  It
is another reason not to force GPIO56 yet.

The vendor snapshot presently records only GPIO use-select, not
GP_IO_SEL2/GP_LVL2.  The vendor uses GPIOBASE `0x500`, so the missing decisive
read-only comparison is I/O `0x534` and `0x538`, especially bit 24.  The
reference host at `192.0.2.203` timed out during this audit, so no new value
is claimed.

## Vendor-CSEG evidence

The targeted MSI USB path does not reveal a hidden controller start sequence
that should be copied wholesale:

- `RUN_CSEG` `0x6953..0x69a1` iterates D29:F7 and D26:F7, assigns a temporary
  EHCI BAR, enables memory decode, obtains the operational-register base from
  CAPLENGTH, sets USBCMD.HCRESET, polls it clear, then removes decode and the
  BAR.  It does not write PORTSC, CONFIGFLAG, a power GPIO, PPO or MAP.  The
  vendor poll is unbounded and must not be copied without a timeout.  SeaBIOS
  already performs host-controller reset later.
- `POST2_CSEG` `0x383..0x4b2` selects the enabled USB-function topology from
  setup and updates RCBA FD/MAP policy.  `0x4b3..0x547` temporarily decodes
  each EHCI and adjusts HCSPARAMS to match that topology.  The current target
  already exposes all six UHCIs, two six-port EHCIs and coherent HCSPARAMS.
- The table loop at POST2 entry `0x77..0x97` accesses hard-coded I/O base
  `0x800`, which matches the vendor PMBASE, not its GPIOBASE `0x500`; it is
  ACPI/PM status-enable policy and is not a GPIO56 write.
- In `POST_CSEG`, `0xb8bc..0xb918` is a generic GPIO-mode/direction/level
  helper.  The direct callers found at `0xb7c5` and `0xb7d0` pass GPIO62 and
  GPIO63, not GPIO56.  `POST_CSEG` `0x5e1d..0x5e2d` reads I/O `0x538`, but
  masks only bits 4--6 (GPIO52--GPIO54).  No direct GPIO56 write was found in
  these reviewed call paths.

This is deliberately a bounded claim.  AMIBIOS setup dispatch and cross-module
calls make the absence of a literal write in these paths weaker than a live
vendor direction/level capture.  Static evidence does not justify saying the
vendor never programs GPIO56.

EHCI PCI offset `0xfc` (EHCIIR2) remains one documented missing initialization
item.  EDS section 17.1.36 requires bit 29, bit 17 and field 3:2=`10b`;
B06VQ's masked RMW implements only those fields.  The vendor final value on
both controllers is `2002130a`.  This write should remain because it is a
publicly documented BIOS requirement, but the EDS does not describe it as an
OC map or VBUS control and it must not be presented as the proven power fix.

No SMI ownership conflict is visible: the captured EHCI legacy/ownership
enable fields are clear.  SMI ownership is therefore useful telemetry but is
not the leading hypothesis.

## Safest next automatic image/test

Do **not** add a new USB register write yet.  The smallest informative next
run already exists as B06VQ-USBTRACE1; B06VX carries the same EHCIIR2 helper
and deferred SeaBIOS USB journal on top of the current AHCI work.  If strict
one-hypothesis isolation is desired, flash B06VQ-USBTRACE1.  If continuing the
linear current branch, use B06VX unchanged for USB and treat its SATA changes
as a separate proof boundary.

Before power-on, attach one known-good, directly connected USB-2.0 keyboard;
repeat separately with a USB-2.0 storage device.  The run should:

1. apply only the existing documented EHCIIR2 masked RMW;
2. log FD, CG, PPO, MAP, UPRWC, GPIO use/direction/level and GLE;
3. decode every EHCI/UHCI PORTSC as `CCS/OCA/OCC` at pre-payload time;
4. continue into SeaBIOS even if OCA remains asserted so USBTRACE records the
   precise controller/port failure;
5. retain the full serial log; and
6. measure physical VBUS at the tested connector, preferably before and after
   SeaBIOS starts.

Expected discriminator:

| Observation | Interpretation |
|---|---|
| VBUS near 0 V, all OCA=1 | board USB-power path is the primary fault |
| VBUS near 5 V, all OCA=1 | investigate OC bias/wiring, switch fault or schematic mismatch |
| OCA clears after EHCIIR2, CCS appears | EHCIIR2 was a prerequisite; let SeaBIOS enumerate |
| OCA=0 but no CCS | test physical port mapping/device and controller reset/routing next |
| CCS/descriptor appears | proceed to HID/storage driver evidence in USBTRACE |

Clearing OCC/OCI is not part of this first test.  It destroys useful history
and cannot clear OCA.  A later diagnostic may acknowledge only EHCI bit 5 and
UHCI bit 11, but mixed R/W and W1C PORTSC semantics make a naive read-modify-
write unsafe: it can accidentally clear CSC/PEDC or modify reset, suspend,
enable and ownership fields.  Never write an observed full PORTSC value back,
and never claim to write the read-only OCA bit.

## Gate before any GPIO56 experiment

A later, separately named experiment such as `B06VQ-USBOC1` may drive
USB_MODE only after all of these gates are met:

1. target PCB revision and the USB-power sheet are matched;
2. exact UP7533AM8 EN/S3# semantics are established or measured;
3. vendor-boot GPIOBASE, GPIO_CNTL, GPIO_USE_SEL2, GP_IO_SEL2 and GP_LVL2 are
   captured, with GPIO56 explicitly decoded;
4. VBUS, USB_MODE, 5VDRV1_EN and at least one OC# net are measured on vendor
   firmware and current coreboot;
5. FD=0, CG bit20=0, PPO=0, MAP=0, both EHCIs/all UHCIs present and D0, and
   EHCIIR2 exact readback all pass;
6. GLE is clear and the image retains serial/POST recovery diagnostics; and
7. the write is an isolated board-policy option, not part of generic ICH10.

Until those gates are satisfied, copying GPIO tables, forcing PPO/UPRWC,
changing OC pins to GPIO, or forcing GPIO56 is materially less safe and less
diagnostic than the read-only/VBUS comparison.
