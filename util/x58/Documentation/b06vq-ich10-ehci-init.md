> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VQ isolated ICH10 EHCI initialization

## Purpose

B06VQ is the first post-SeaBIOS bring-up experiment after B06VP established
working BSP virtual-wire interrupts, visible Radeon output and a complete
RTL8168 DHCP/TFTP path.  It keeps the B06VP memory, QPI, X58 routing, PCIe,
GPU, SeaBIOS and iPXE path unchanged and tests one narrow USB prerequisite.

The B06VP trace proves that SeaBIOS found and started both ICH10 EHCI
controllers and all six UHCI companions.  It did not print a USB descriptor or
keyboard registration, however.  The same run saw `0xff` while probing the
i8042 and timed out there.  USB HID and PS/2 therefore remain independent,
unproved paths.

## Evidence for the one register change

Intel ICH10 Datasheet 319973, section 17.1.36, describes the EHCI instruction
register at PCI configuration offset `0xfc` (`EHCIIR2`).  BIOS must program:

```text
bit 29       = 1
bit 17       = 1
bits 3:2     = 10b
all others   = preserved
```

A read-only capture from the identical MSI vendor-booted reference machine on
2026-09-06 found the same final value on both controllers:

```text
00:1a.7 EHCIIR2 = 2002130a
00:1d.7 EHCIIR2 = 2002130a
```

The complete literal is not copied.  A reserved bit differs between the Intel
reset example and the observed vendor value, so B06VQ performs the documented
masked read-modify-write:

```text
target = (before & ~2002000c) | 20020008
```

For example, a pre-value of `20001706` yields `2002170a`; a pre-value of
`20001306` yields the vendor-observed `2002130a`.  Both preserve every bit
outside the documented required fields.

The implementation was factored out of the existing upstream ICH10 chip
driver into a reusable ramstage helper.  The full driver still invokes it once
and retains its previous behavior.  B06VQ selects only that helper, not the
full southbridge driver.

## Exact B06VQ behavior

Before IOU0 link start and generic PCI enumeration, B06VQ:

1. passes all inherited platform, static-topology and raw-root gates;
2. applies the inherited exact `IOHBUSNO=0100` route;
3. reads and prints `EHCIIR2` for `00:1a.7` and `00:1d.7`;
4. emits POST `44` and calls the common helper exactly once;
5. reads both registers back and requires the complete dword to equal the
   target derived from the corresponding pre-value;
6. emits POST `46` on success or halts at POST `47` on any discrepancy;
7. continues through the unchanged IOU0, PCI, Radeon and payload path.

After coreboot assigns resources and enables decodes, but before SeaBIOS is
entered, B06VQ emits a read-only snapshot of:

- global ICH10 `PPO`, `MAP`, `UPRWC` and GPIO-use registers;
- both EHCI command, BAR, D-state, `EHCIIR2`, legacy/ownership fields,
  capability header, operational registers and bounded port status;
- all six UHCI command, BAR, legacy-key and two port-status registers.

No controller MMIO is read unless its BAR lies inside the assigned window,
memory decoding is enabled and PMCSR reports D0.  No UHCI I/O register is read
unless its I/O BAR and command decode pass.  The port loop is bounded at six.
The telemetry contains no writes.

SeaBIOS remains responsible for host-controller reset, frame/DMA structures,
port power, companion routing, `CONFIGFLAG`, device enumeration and the USB
keyboard driver.

## Why the full ICH10 driver is still excluded

The existing full driver couples this small EHCI requirement to unrelated and
partly irreversible policy: six ICH10 PCIe root ports, function hiding and
locking, RCBA fields, watchdog handling, LPC/IOAPIC/HPET/PIRQ policy, SATA
resource assumptions and SMM-related configuration.  The current selective
X58 device model does not yet satisfy those assumptions.  Enabling it would
destroy the one-hypothesis boundary and could hide a useful failure behind a
different southbridge regression.

The vendor reference also gives no USB-specific reason to add further writes:

```text
EHCI CMD=0006, PMCSR=0000
UHCI CMD=0005, LEGKEY=2f00
PPO=0000, MAP=00000000, UPRWC=0000
GPIO_USE_SEL=19fdff01, GPIO_USE_SEL2=030300ff
```

The lower ownership/SMI enable fields were clear.  An active PiKVM device was
visible on EHCI2 port 5 in that runtime snapshot.  These observations justify
logging ownership and routing state, not copying it.

## First hardware test

Use the same E5645, sole SPD-`0x54` DIMM, HD 5450 and RTL8168 configuration as
B06VP.  Connect COM1 at 115200 8N1 before power-on.  For two separate boots,
attach before applying power:

1. a known low/full-speed USB keyboard to a rear USB 2.0 port;
2. a known high-speed USB 2.0 storage device to the same port.

Retain the complete serial trace.  The new decisive prefix is:

```text
[USB] EHCI2 00:1a.7 EHCIIR2 PRE=........ TARGET=........
[USB] EHCI1 00:1d.7 EHCIIR2 PRE=........ TARGET=........
[USB] EHCI2 00:1a.7 EHCIIR2 POST=........
[USB] EHCI1 00:1d.7 EHCIIR2 POST=........
[USB] B06VQ ICH10 EHCI BIOS-required fields exact gate PASS
```

Before SeaBIOS, `CONFIGFLAG=0` is normal.  A low/full-speed keyboard can appear
only in a UHCI companion's `PORTSC`, so an EHCI connect-status value of zero is
not by itself a failure.  Useful interpretations are:

- no connect bit on any controller: investigate physical rear-port mapping,
  VBUS and overcurrent/power routing next;
- connect state but no SeaBIOS descriptor: investigate reset/companion routing
  and DMA visibility;
- descriptors but no HID registration: investigate the SeaBIOS HID path;
- `USB keyboard initialized` and working menu input: B06VQ succeeds.

POST `47`, a stop after POST `44`, invalid BAR/D0/decode diagnostics, or an
unchanged absence of all USB devices is a valid negative result.  Recover with
the preserved B06VP or vendor chip.  Do not combine the first B06VQ run with
SATA, LPC, GPIO or interrupt-routing writes.

## Proof boundary

B06VQ treats the already reached RAM/QPI path as a frozen development
assumption, as requested.  That does not replace the project's eventual
ten-cold/ten-warm and external memory-test acceptance criteria.  Until real
B06VQ logs exist, the image proves only source/build correctness and the
read-only vendor comparison; it does not prove USB enumeration or keyboard
input.

See the [B06VP full hardware capture](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
and the [B06VP experiment](b06vp-lapic-extint.md).
