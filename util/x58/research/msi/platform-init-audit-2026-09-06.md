> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# MSI X58 Pro-E platform-initialization audit (2026-09-06)

This record separates what the current B06VP/B06VQ path has proved from what
the original MSI firmware configures.  It is based on:

- the complete B06VP hardware capture
  `captures/2026-09-06-b06vp-hw-03-full-to-netboot-menu.raw`;
- read-only PCI, RCBA, PMBASE, GPIO, ACPI and DMI inspection of an identical
  MSI X58 Pro-E running AMI `V8.14B8`;
- static inspection of the independently supplied `A7522IMS.8F0` image; and
- Intel's *I/O Controller Hub 10 (ICH10) Family Datasheet*.

The live reference was already under Linux.  BAR addresses, PCI command bits,
MSI state and running-controller registers are therefore not copied as BIOS
initialization values.  Only documented mode/decode fields and independently
corroborated board policy are candidates for isolated experiments.  The live
BIOS version also differs from the static 8F0 corpus, so ACPI table bytes are
semantic references rather than transplantable data.

## Proven current boundary

B06VP has completed one end-to-end run through the vendor-assisted CSI/MINIT
path, coreboot postcar and ramstage, the Radeon HD 5450 physical VBIOS,
SeaBIOS, RTL8168 link/DHCP/TFTP and the downloaded netboot.xyz menu.  This is
strong evidence that the current one-DIMM development configuration provides
enough usable DRAM and PCIe for that path.  It is not the required ten-cold and
ten-warm validation milestone, and it does not prove general RAM/QPI support.

The same run showed these remaining platform gaps:

- SeaBIOS initialized two EHCI and six UHCI controllers but found no USB
  descriptor or keyboard.
- The i8042 status read returned `ff`; the current LPC decode does not enable
  the KBC range.
- D31:F2 and D31:F5 remained `8086:3a20` and `8086:3a26` legacy IDE devices;
  ATA register reads returned `ff`.
- SeaBIOS reported `rsdp=0`; no coreboot ACPI tables exist for this target.
- coreboot deliberately enumerates only a small devicetree allowlist.  The
  later SeaBIOS census of 68 functions is discovery, not proof that those
  functions were initialized.

## B06VP CAR census after a fail-closed boot

A later exact one-second AC cycle reached and returned from MINIT, printed the
post-MINIT I801 tuple, and then produced no further serial output for 180
seconds.  The next AC cycle correctly stopped on the persistent reset-loop
guard.  After clearing that guard, B06VP remained in CAR ROMMON, which allowed
a target-state-read-only southbridge census without depending on DRAM,
ramstage or SeaBIOS.

The identity command proved that the executing image was B06VP.  RCBA was
enabled at `fed1c001`; note that D31:F0 offset `50` read as zero and is not the
RCBA register.  The six fields programmed by the historical ICH10 common
driver's `BIOS must program` block were all still at their pre-init values:

```text
register       live value  selected field  common-driver target
CIR8  3430     00000000    bits 1:0 = 0    2
FD    3418     00000000    bit 0 = 0       1
CIR9  350c     00000020    bits 27:26 = 0  2
CIR7  2034     b2b477cc    bits 19:16 = 4  5
CIR13 0f20     b2b477cc    bits 19:16 = 4  5
CIR10 352c     0008c008    bits 17:16 = 0  3
```

`CIR6[7]` was already clear.  `CIR5[0]` was also clear and must remain clear;
the common driver intentionally omits that old write because of the ICH10
specification update.  These measurements prove that the successful B06VP
SeaBIOS/iPXE run did not depend on the six common-driver fields already being
set.  They do not prove the fields are optional for reliable USB, timers,
power management or a general platform boot.

Other decisive live values were:

```text
LPC:   CMD=0007 PMBASE=0501 ACPI_CNTL=80 GPIOBASE=0581 GPIO_CNTL=10
       SERIRQ=10 LPC_IO_DEC=0010 LPC_EN=2001 PMIR=00000000
SMBus: CMD=0001 BAR4=0401 HOSTC=01
EHCI:  00:1a.7 fc=20001706; 00:1d.7 fc=20001706
USB:   CG=00000000 PPO=0000 MAP=00000000 UPRWC=0000
PIRQ:  A-D=80/80/80/80 E-H=80/80/80/80
HPET:  HPTC=00000000
OIC:   00000000
```

Both USB over-current GPIO-use masks evaluated to zero in this state, so the
first USB diagnostic does not justify changing GPIO muxing.  `CG[20]` was
also zero, so there is no evidence for clearing the USB clock-gate bit.  The
PIRQ bytes have bit 7 set and therefore do not presently route legacy PIC
IRQs; that is compatible with the first polled SeaBIOS USB enumeration and is
not evidence for copying the vendor PIRQ literals.

The current B06VP routing differs substantially from the vendor runtime:

```text
        B06VP CAR                              vendor runtime
D31IR   3210                                   0232
D30IR   0000                                   0000
D29IR   3210                                   0237
D28IR   3210                                   3201
D27IR   3210                                   3216
D26IR   3210                                   3250
D25IR   3210                                   7654
OIC     00                                     03
```

That comparison is a routing-policy input, not a safe bulk-write table.  The
raw immutable evidence and explicit side-effect boundary are in
`captures/2026-09-06-b06vp-car-ich10-readonly-census.*` and
`captures/2026-09-06-b06vp-car-ich10-routing-readonly.*`.

### Controlled six-field write result

The exact measured PRE tuple was subsequently encoded as a sealed ROMMON
table. Host and ROMMON independently produced program FNV `84afcaf8` for 24
operations (`OPS=18` is hexadecimal). Before the first write the table
asserted the LPC ID, enabled RCBA, byte-wide `FDSW=00`, and all six complete
PRE dwords. It then executed only the six selected R/W field updates in the
historical common-driver order, with every mutation marked non-reversible and
followed by a complete-dword assertion.

All operations returned `done`. The six exact transitions were:

```text
CIR8   00000000 -> 00000002
FD     00000000 -> 00000001
CIR9   00000020 -> 08000020
CIR7   b2b477cc -> b2b577cc
CIR13  b2b477cc -> b2b577cc
CIR10  0008c008 -> 000bc008
```

The final gates reconfirmed `FDSW=00`, `RCBA=fed1c001` and LPC
`8086:3a16`. Runtime transaction FNV was `287284a2`, with `MUT=06`,
`NONREV=06`, `LAST=ok` and no rollback attempt. After archiving the complete
trace, only the ROMMON transaction record was discarded; direct reads proved
the register values remained exact. The board stayed responsive in CAR.

This proves safe write/readback behavior in this one retained B06VP CAR
session. It does not prove that the fields change USB behavior, that they
survive any reset, or that automatic ramstage integration is stable. GCS,
CIR5, FDSW, function-disable bits, RPFN/MAP and the rest of the full driver
were not touched. See
`captures/2026-09-06-b06vp-car-ich10-six-required-fields-live.*`.

## USB

The vendor reference has EHCI D26:F7 and D29:F7 register `fc=2002130a`.
Intel ICH10 section 17.1.36 requires bits 29 and 17 to be set and bits 3:2 to
equal `10b`; all other fields must be preserved.  B06VQ therefore uses the
masked pair `mask=2002000c`, `value=20020008`, rather than copying the complete
vendor dword.  This is B06VQ's only new write.

Reference observations useful only as diagnostics are:

```text
EHCI D26:F7/D29:F7: fc=2002130a, CAPLEN=20, HCIVER=0100, 6 ports
UHCI legacy register c0=2f00
RCBA PPO 3524=0000, MAP 35f0=00000000
PMBASE UPRWC=0000
GPIO_USE_SEL=19fdff01, GPIO_USE_SEL2=030300ff
```

B06VQ records those classes of state before SeaBIOS without changing port
power, companion routing, GPIO muxing, controller schedules or DMA.  A USB
milestone requires an actual descriptor, `USB keyboard initialized`, and
effective menu input.  The `fc` readback alone is not sufficient.

## SATA/AHCI

The vendor reference exposes only D31:F2 as `8086:3a22`, class `010601`, and
hides D31:F5.  Its stable PCI mode fields are:

```text
D31:F2 MAP  90=0060
D31:F2 PCS  92=a23f
D31:F2 SCGC 94=00000193
```

Intel ICH10 sections 14.1.30 through 14.1.32 give a narrow staged route:

1. `MAP[7:5]=011b` selects AHCI and maps all six ports to D31:F2.  Software
   may make this change during POST; the device ID and class change as a
   result.
2. `PCS[5:0]` must enable all supported ports before handing control to an
   AHCI-aware OS.  Presence bits 13:8 are read-only and must not be copied.
3. `SCGC[8:0]` must be programmed to `0x193`; all other clock-policy fields
   remain preserved.

These are deliberately separate experiments.  The existing broad
`i82801jx/sata.c` path is not yet suitable: it also writes CAP, PI, VSP,
PxCMD and a large unexplained SIDX/SDAT table.  The vendor runtime snapshot
has VSP bit 0 set while that driver clears it.  No such writes are justified
for the first AHCI tests.

## LPC, KBC and Fintek Super I/O

The relevant live reference state is:

```text
PMBASE=0801 ACPI_CNTL=80 GPIOBASE=0501 GPIOCNTL=10
SERIRQ= d0  LPC_IO_DEC=0000  LPC_EN=340e
GEN1=00fc0a01 GEN2=00140a01 GEN3=00fc4701
```

B06VQ instead intentionally retains its established early-console policy:

```text
PMBASE=0501 GPIOBASE=0581 SERIRQ=10 LPC_IO_DEC=0010 LPC_EN=2001
```

`LPC_EN` bit 10 is the documented KBC decode enable, so an isolated future
probe may change only `2001 -> 2401`.  The complete vendor value `340e` is not
safe to copy: it enables CNF1/CNF2, KBC, FDD, LPT and COMB while omitting the
current COMA policy.  Static analysis identifies a Fintek F71882F at 4e/4f
and confirms the 87/87 entry plus aa exit protocol, but the exact KBC logical
device and board pin mux have not yet been proved.  They must be captured
before any Super-I/O write sequence is added.

## Interrupts, IOAPIC and HPET

The reference exposes IOAPIC at `fec00000`, HPET at `fed00000`, and RCBA
`OIC=03`, `HPTC=00000080`.  MSI's generated ACPI routes include:

```text
D31 A/B/C -> GSI 18/19/18
D29 A/B/C/D -> GSI 23/19/18/16
D26 A/B/C/D -> GSI 16/21/18/19
D27 A -> GSI 22
D28 A/B/C/D -> GSI 17/16/18/19
```

The full historical ICH10 driver must not be enabled wholesale yet.  This
audit found three blockers.  The first two are corrected in the current local
working tree but remain build/regression evidence rather than target-hardware
proof; the third still requires board-specific policy:

- the original `D25IP` alias of `D26IP` at `0x3114` is now corrected to
  `0x3118`;
- the original 8-bit access to the 32-bit D31:F0 `PMIR` is now a named 32-bit
  read-modify-write;
- `lpc.c` programs every PIRQ and PCI `INT_LINE` to IRQ11, which does not
  represent the MSI routing policy.

Those generic correctness defects should be fixed and regression-built before
selective MSI-specific OIC/IOAPIC/PIRQ work is enabled.

## ACPI, SMBIOS and resource ownership

Vendor semantics useful for a clean implementation are:

```text
FADT: SCI 9, SMI_CMD b2, PM1_EVT 800, PM1_CNT 804,
      PM timer 808, GPE0 820, reset CF9 value 06
MADT: LAPIC fee00000, IOAPIC fec00000, IRQ0 -> GSI2,
      IRQ9 -> GSI9 with flags 000d
MCFG: e0000000, buses 00-ff
HPET: fed00000
```

The static 8F0 ACPI template and the live V8.14B8 tables differ in generated
fields such as IOAPIC ID.  No vendor AML/table blob should be copied.  Initial
coreboot tables must describe the actual current PMBASE/GPIOBASE, one BSP,
the actual memory map and only devices that have been initialized.  The FADT
8042 flag must remain clear until KBC decode plus Fintek setup work.

The present domain reports RAM and broad PCI windows but does not model LPC
subtractive I/O or reserve flash, RCBA, IOAPIC, HPET, LAPIC, PMBASE and
GPIOBASE as platform-owned resources.  A resource-only stage should address
this before adding ACPI.  SMBIOS Types 16/17/19 also require a real
`CBMEM_ID_MEMINFO` record; vendor claims of six slots, ECC and 192 GiB cannot
be reused for the current one-DIMM non-ECC experiment.

## Ordered isolated experiments

| Build | Sole new policy | Acceptance evidence |
|---|---|---|
| B06VQ | EHCI `fc` required fields + read-only USB telemetry | Descriptor, USB-keyboard registration and usable input |
| B06VQ-USBTRACE1 | deferred, bounded SeaBIOS USB transaction trace | exact first failure phase or descriptor/HID success; no dropped events |
| B06VQ-PLATRO1 | read-only PM/IRQ/HPET/resource census | complete strictly decode-gated telemetry; no hardware or resource writes |
| B06VQ-ICHBASE1 | only the six common-driver required fields | exact PRE/TARGET/POST readback; no function hiding, locks or broad driver |
| B06VR | SATA `MAP[7:5]=011b` | D31:F2 becomes `3a22/010601`, D31:F5 disappears |
| B06VS | SATA `PCS[5:0]=3f` | Exact enabled-bit readback and bounded port telemetry |
| B06VT | SATA `SCGC[8:0]=193` only if still required | Exact selected-field readback; SeaBIOS AHCI probe |
| B06VU | LPC KBC decode bit 10 | 60/64 decode no longer floats; no Super-I/O claim |
| B06VV | captured Fintek KBC logical-device/pin policy | i8042 self-test and real keyboard input |
| B06VW | fixed/subtractive platform resource ownership | no overlaps; resources visible in coreboot tables |
| B06VX | corrected OIC/IOAPIC/PIRQ routing | bounded IRQ delivery tests, no all-IRQ11 shortcut |
| B06VY | minimal native ACPI | SeaBIOS reports a nonzero RSDP; tables validate |
| B06VZ | SPD-backed `memory_info` | accurate SMBIOS Types 16/17/19 for the tested DIMM |

Every hardware-affecting stage remains default-off, retains serial/POST
diagnostics and recovery compatibility, logs PRE/TARGET/POST, verifies exact
selected-field readback, and does not select the complete ICH10 driver.
