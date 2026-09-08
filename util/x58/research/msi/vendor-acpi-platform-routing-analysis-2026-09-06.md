> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# MSI X58 Pro-E vendor ACPI and platform-routing analysis

Date: 2026-09-06
Scope: offline analysis only; no target access, firmware build, flash, or
coreboot-source change was performed.

## Result

The MSI AML gives a coherent board-specific APIC-routing policy for the ICH10R
devices at D26--D29, and those direct `_PRT` GSIs exactly match the nibbles in
the independently recorded MSI runtime `DnnIR` values.  This is strong enough
to justify isolated, exact-gated route trials for the USB functions:

```text
D26IR = 3250   -> INTA/B/C/D = GSI 16/21/18/19
D29IR = 0237   -> INTA/B/C/D = GSI 23/19/18/16
```

It is not evidence for enabling legacy PIRQ-to-8259 routing: MSI's runtime
PIRQA--H bytes were all `80` (disabled), and the B06VP values are also `80`.
The generic ICH10 path which writes all eight PIRQs and all PCI `INT_LINE`
bytes to IRQ11 therefore must not be used as the board policy.

The same corpus supports a native FADT using runtime-derived PMBASE and SCI 9,
one ICH10 IOAPIC at `fec00000`, an IRQ9 -> GSI9 level/high MADT override, and
the reset register `SystemIO cf9 = 06`.  It does **not** justify copying any
vendor table wholesale.  The MSI standard-table region is a parameterized
template with zero pointers and invalid checksums; the DSDT depends on
vendor-SMM services and writable BIOS globals at `ffffff00`; and its static
PMBASE, IOAPIC ID, processor list, and HPET ID do not describe B06VP's current
state.

The immediate safe order is: program only routes for present devices while
all destinations remain masked; prove one interrupt after QPI/DMI; establish
SCI with all event sources disabled; prove one PM-timer SCI; then generate
minimal native tables.  SMI, USB wake, S3/S4, RTC wake, and autonomous
power-after-G3 remain fail-closed.

## Evidence classes and limits

Tags used below are deliberate:

- **MSI-static**: bytes extracted from MSI `A7522IMS.8F0`.  This is the primary
  board-specific source, but it is a build-time template rather than a live
  ACPI dump.
- **MSI-live-secondary**: decoded values in
  `research/msi/platform-init-audit-2026-09-06.md` from an identical X58
  Pro-E running AMI V8.14B8.  The underlying vendor `inteltool`/`lspci` files
  are not retained in this repository, so these values are useful corroboration
  but are not independently re-decodable here.
- **B06VP-live**: immutable captures from the current coreboot target.  These
  describe the present coreboot state, not vendor policy.
- **Intel-comparative**: DX58SO firmware templates.  They may corroborate an
  ICH10/X58 mechanism, but never establish MSI board policy.
- **Inference**: a proposed implementation or experiment constrained by the
  preceding observations.

The decompiled line references below name deterministic `/tmp/*.dsl` outputs
created by the commands in the reproduction section.  No proprietary AML or
firmware bytes are added to the repository by this note.

## Reproduction

Tool:

```text
coreboot/util/crossgcc/xgcc/bin/iasl
ACPICA 20251212; coreboot toolchain v2026-08-30_fe3e0819
SHA-256 4b1e5067ef40ec0dbbc4f4717e4fcf1596b0a39e195096444d16bbe108a122b9
```

Commands run from the repository root:

```sh
python3 scripts/analyze_msi_acpi.py \
  blobs-local/msi-x58-pro-e/extracted/ACPITBL_SEG.bin \
  blobs-local/msi-x58-pro-e/extracted/amibody_10.rom

coreboot/util/crossgcc/xgcc/bin/iasl -d -p /tmp/msi_a7522_dsdt \
  blobs-local/msi-x58-pro-e/extracted/amibody_10.rom

coreboot/util/crossgcc/xgcc/bin/iasl -d -p /tmp/intel_dx58so2_fadt \
  blobs-local/intel-dx58so/extracted/Intel32/Intel32c/Main/Insyde/ACPI/00.raw
coreboot/util/crossgcc/xgcc/bin/iasl -d -p /tmp/intel_dx58so2_dsdt \
  blobs-local/intel-dx58so/extracted/Intel32/Intel32c/Main/Insyde/ACPI/02.raw
coreboot/util/crossgcc/xgcc/bin/iasl -d -p /tmp/intel_dx58so2_madt \
  blobs-local/intel-dx58so/extracted/Intel32/Intel32c/Main/Insyde/ACPI/03.raw
coreboot/util/crossgcc/xgcc/bin/iasl -d -p /tmp/intel_dx58so2_mcfg \
  blobs-local/intel-dx58so/extracted/Intel32/Intel32c/Main/Insyde/ACPI/05.raw
coreboot/util/crossgcc/xgcc/bin/iasl -d -p /tmp/intel_dx58so2_hpet \
  blobs-local/intel-dx58so/extracted/Intel32/Intel32c/Main/Insyde/ACPI/07.raw
coreboot/util/crossgcc/xgcc/bin/iasl -d -p /tmp/intel_dx58so2_ssdt_pm \
  blobs-local/intel-dx58so/extracted/Intel32/Intel32c/Main/Insyde/ACPI/11.raw

rg -n 'Name \(PR00|Name \(AR00|Method \(_PRT|Device \(HPET|Device \(SATA|Device \(SAT1|Device \(USB|OperationRegion \(PIX0|Method \(_PTS|Method \(_WAK|Name \(_S[01345]' \
  /tmp/msi_a7522_dsdt.dsl

sha256sum \
  blobs-local/msi-x58-pro-e/7522v8F/A7522IMS.8F0 \
  blobs-local/msi-x58-pro-e/extracted/ACPITBL_SEG.bin \
  blobs-local/msi-x58-pro-e/extracted/amibody_10.rom \
  /tmp/msi_a7522_dsdt.dsl
```

Primary MSI hashes:

| Artifact | SHA-256 |
|---|---|
| `blobs-local/msi-x58-pro-e/7522v8F/A7522IMS.8F0` | `ee7912da81a3cf47b6bedef7dedfa56cbc31723f5cdd7f0d75a7f64e336c04a8` |
| `blobs-local/msi-x58-pro-e/extracted/ACPITBL_SEG.bin` | `dc8ee36106d5eb10d19ff8bccba8936dea340fe343f9cd5210029bdfe490c6fd` |
| `blobs-local/msi-x58-pro-e/extracted/amibody_10.rom` | `f50bf11a566525a0a05ea52c7958f9f5a25667cc74fbfb8b333ac9b5305211cb` |
| `/tmp/msi_a7522_dsdt.dsl` | `ede8ff40cb708a47b44c79542861210fd787269aae873a415e7c9d0fbfae6865` |
| `scripts/analyze_msi_acpi.py` | `410322a1e3a377f85320b5466ab40468e555fb11eb5c7596ba6da008592b69f7` |

Selected Intel comparative hashes:

| Table | Raw path suffix | SHA-256 |
|---|---|---|
| FADT | `ACPI/00.raw` | `7444ed55f93b1f9070e0da604d4a4a828e06dda3370d43c88cba8a2f5cdba708` |
| DSDT | `ACPI/02.raw` | `b0877577dea5bb6ed23eb0d1816fc5bcf15b0e7497ed4b7a945fd3065fa0ed7b` |
| MADT | `ACPI/03.raw` | `94101d113cf21668e0c99d5ff5670dd44453d6e58e6da94b846a9c82b5c05ad4` |
| MCFG | `ACPI/05.raw` | `f16830a900b77f18d8d22176de88aa296b9eec639d0c94a17a41c7d897612ba8` |
| HPET | `ACPI/07.raw` | `dff434bcc799ffc501e8dc578f3cc251998c05ea161d4131c434b11c109b869b` |
| PM SSDT | `ACPI/11.raw` | `6c715f0815f0142d96f39960d8a886b9e86333dd0ef4db9d249a6e1aed95fe16` |

The local coreboot checkout was at `fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2`
with unrelated local changes.  The exact files used here have hashes recorded
at the end of this note.

## MSI standard-table template

`ACPITBL_SEG.bin` is 44,288 bytes.  The parser finds:

| Offset | Table | Decisive contents | Status |
|---:|---|---|---|
| `0000` | RSDT, 40 bytes | one zero pointer | checksum invalid |
| `0100` | XSDT, 44 bytes | one zero pointer | checksum invalid |
| `0200` | FADT rev 2, 132 bytes | populated policy below | checksum invalid |
| `0290` | FADT rev 3, 244 bytes | empty placeholder | checksum invalid |
| `0390` | MADT, 172 bytes | one IOAPIC and two overrides | checksum invalid |
| `0440` | MCFG, 60 bytes | `e0000000`, segment 0, buses 0--255 | checksum invalid |
| `a480` | FACS, 64 bytes | wake vectors and flags zero | structural object |
| `a4c0` | OEMB, 122 bytes | vendor-specific | checksum invalid |
| `aa6f` | HPET, 56 bytes | base `fed00000`, minimum tick 14318 | checksum invalid |

The separate DSDT is valid: revision 1, OEM `A7522/A7522800`, OEM revision
`00000800`, compiler `INTL 20051117`, length 28,290 bytes, valid checksum.
The invalid standard-table checksums and zero pointers prove those objects
were intended to be patched at runtime.  Values are evidence of policy and
layout, not directly installable ACPI tables.

### FADT policy

The populated MSI FADT says:

```text
SCI_INT             9
SMI_CMD             00b2
ACPI_ENABLE         e1
ACPI_DISABLE        1e
PSTATE_CNT          e2
PM1_EVT             0800, length 4
PM1_CNT             0804, length 2
PM2_CNT             0850, length 1
PM_TMR              0808, length 4
GPE0                 0820, length 16
IAPC_BOOT_ARCH       0003 (legacy devices and 8042)
FLAGS                000004a5
RESET_REG            SystemIO cf9, width 8
RESET_VALUE          06
```

Using `coreboot/src/include/acpi/acpi.h:1032-1064`, `0x4a5` is WBINVD,
C1, control-method sleep button, S4 RTC wake, and reset-register support.  It
does not set the 32-bit PM-timer or platform-clock flags.  B06VP has PMBASE
`0501`, not `0801`, so every block address must be derived from D31:F0 `40h`
(`coreboot/src/southbridge/intel/common/pmbase.c:16-35`).

Under `NO_SMM`, the native FADT must set `SMI_CMD`, `ACPI_ENABLE`,
`ACPI_DISABLE`, and `PSTATE_CNT` to zero.  Advertising the vendor B2 commands
would invite an OS to invoke handlers which do not exist.  Start with
`LEGACY_DEVICES` only; the current Fintek KBC primary-interface test returned
`AB=03`, so neither `8042` nor PS/2 AML is admissible yet.  Also omit the
sleep-button and S4-RTC-wake flags until those paths are proved.  The reset
register can be added only with a separate `cf9=06` reboot acceptance test.

`coreboot/src/southbridge/intel/i82801jx/fadt.c:7-34` gets the runtime base and
correct lengths, but its unconditional 8042, S4-RTC, sleep-button, C2-MP and
platform-clock policy is broader than either current evidence or the MSI
template.  It needs board policy rather than wholesale reuse.

### MADT, MCFG, and HPET

**MSI-static MADT:** LAPIC base `fee00000`, PCAT flag set, one IOAPIC ID 1 at
`fec00000` with GSI base 0, ISA IRQ0 -> GSI2 with conforming flags, and ISA
IRQ9 -> GSI9 with flags `000d` (active-high, level-triggered).  Its twelve
processor records (APIC IDs `80`--`8b`) are all disabled placeholders.

**B06VP-live:** the decoded ICH10 IOAPIC currently reports ID 0 and version
`00170020`, hence 24 redirection entries; the complete census found all entries
masked.  Therefore native MADT generation must read the actual ID and enumerate
only CPUs actually brought online.  It must not copy ID 1 or the placeholder
CPU list.  SCI9 level/high agrees with the coreboot ICH contract at
`common/pmbase.c:108-115`.  Keep the IRQ0 override deferred until PIT/ExtINT or
IOAPIC delivery is proved in the intended post-QPI state.

**MSI-static MCFG:** `e0000000`, segment 0, buses `00`--`ff`; the DSDT also
defines `PCIB=e0000000`, `PCIL=10000000` (`/tmp/msi_a7522_dsdt.dsl:73-74`).
Use the live PCIEXBAR-derived base and decoded bus range, not these literals,
when generating the native MCFG and reserving ECAM.

**MSI-static HPET:** `fed00000`, table number 0, minimum tick 14318, but event
timer block ID zero.  DSDT `HPET` is `PNP0103` and derives the aperture from
RCBA `HPTC` bits 1:0 and enable bit 7
(`/tmp/msi_a7522_dsdt.dsl:3792-3834`).  B06VP's isolated decode trial found
`HPTC=80` and `GCAP_ID=0429b17f:8086a301`: Intel vendor ID, revision 1, four
timers, 64-bit main counter, legacy-routing capability, and period 69,841,279
fs.  Its counter run/stop test passed with all timer interrupts disabled.

That proves decode and a polling timebase, not interrupt delivery.  A later
timer-0 one-shot crossed its comparator on advertised route 20 while LAPIC
IRR vector `51` stayed clear.  The result rejects only that early CAR routing
state; it does not diagnose the timer or APIC as faulty.  Reserve the aperture
once decoded, but withhold an HPET ACPI table from the first OS-boot table set
until one post-QPI/DMI IRQ delivery test succeeds.  Do not call
`common/hpet.c:13-23` in a quiescent decode stage because it also starts the
main counter.

## PCI interrupt routing

The MSI DSDT's `_PIC` stores the OS-selected interrupt model in `PICM`
(`/tmp/msi_a7522_dsdt.dsl:138-150`).  Root `_PRT` returns link-device package
`PR00` for PIC mode and direct-GSI package `AR00` for APIC mode
(`/tmp/msi_a7522_dsdt.dsl:331-852,853-1260,2619-2627`).

The direct routes relevant to ICH10 are:

| Device | MSI AML INTA/B/C/D | PIRQ nibble A/B/C/D | Corroborating MSI runtime `DnnIR` |
|---|---|---|---:|
| D31 | `18/19/18/-` | C/D/C/A | `0232` |
| D29 | `23/19/18/16` | H/D/C/A | `0237` |
| D28 | `17/16/18/19` | B/A/C/D | `3201` |
| D27 | `22/-/-/-` | G/-/-/- | `3216` (unused nibbles not established by AML) |
| D26 | `16/21/18/19` | A/F/C/D | `3250` |

The mapping is independently described by
`coreboot/src/southbridge/intel/common/rcba_pirq.c:16-45,68-93`: each
`DnnIR` nibble selects PIRQA--H, and APIC GSIs are `16 + pirq_index`.
Register offsets are in `common/rcba_pirq.h:15-26` and
`i82801jx/i82801jx.h:111-126`.

This correlation is exact for every AML-listed pin above.  D31 pin D and
D27 pins B--D are not present in MSI `AR00`, so the corresponding runtime word
nibbles must not be published unless an enabled function actually reports
that pin.  MSI's static `PR00/AR00` contains no D25 entry at all.  The
MSI-live-secondary value `D25IR=7654` is therefore insufficient on its own;
leave D25 unchanged until a D25 function and its `PCI_INTERRUPT_PIN` are
observed.

The legacy link devices LNKA--H use D31:F0 PCI config bytes `60h`--`63h` and
`68h`--`6bh` (`/tmp/msi_a7522_dsdt.dsl:7067-7079`).  Bit 7 is the disabled
state.  LNKA/C/D/E/F/G/H offer level/active-low/shared IRQs
`3,4,6,7,10,11,12,14,15`; LNKB offers IRQ5 only
(`/tmp/msi_a7522_dsdt.dsl:2587-2602,7089-7430`).  These resource choices
describe PIC compatibility and do not require enabling legacy routing for an
APIC-only first OS boot.

### Safe route implementation

For the first route mutation, assert the complete pre-state, including at
least LPC identity `D31:F0 00h=3a168086`, RCBA `f0h=fed1c001`, `OIC=03`, all
24 IOAPIC entries masked, PIRQ A--H=`80`, and D26IR/D29IR=`3210/3210`.
Re-read the eight USB functions' `PCI_INTERRUPT_PIN`; the captured
values were D26 F0/F1/F2/F7 = A/B/C/C and D29 F0/F1/F2/F7 = A/B/C/A.

Then perform only:

```text
RCBA16(D26IR) <- 3250
RCBA16(D29IR) <- 0237
```

Use full-word equality readback and stop on any mismatch.  Do not touch
`DnnIP`, the eight PIRQ route bytes, PCI `INT_LINE`, or IOAPIC redirection
entries in that image.  A subsequent image may generate `_PRT` entries only
for enabled devices with valid `PCI_INTERRUPT_PIN`, following the enumeration
discipline in `rcba_pirq.c:68-93`.

## Device scopes and policy boundary

The MSI DSDT declares these useful topology scopes:

- `PCI0` (`PNP0A08`, bus 0), `SBRG` D31:F0, and the D30 bridge `P0P1`;
- ICH10 PCIe roots `P0P4`--`P0P9` at D28:F0--F5, each with its own `_PRT`;
- X58/IOH port scopes `NPE1`--`NPEA` at D1--D10, each with `_PRT`;
- `P0P5` contains static `JMB0`/`JMB1` IDE-style child scopes;
- SATA at D31:F2 and `SAT1` at D31:F5;
- UHCI D29:F0--F3 and D26:F0--F2; EHCI D29:F7 and D26:F7;
- HPET below `SBRG`, plus conditional PS/2 and other legacy devices.

This is a vendor namespace inventory, not a mandate to expose all objects.
Only enumerate scopes for functions visible and enabled under the selected
coreboot policy.  In particular, do not instantiate the JMicron children,
unused IOH port scopes, or hidden D31:F5 solely because the template names
them.

The downstream APIC `_PRT` packages encode the conventional swizzle explicitly:

| Bridge scopes | Child INTA/B/C/D GSIs |
|---|---|
| P0P4, P0P8 | `16/17/18/19` |
| P0P5, P0P9 | `17/18/19/16` |
| P0P6 | `18/19/16/17` |
| P0P7 | `19/16/17/18` |
| NPE1--NPEA | `16/17/18/19` |

These are visible in MSI packages `AR04`--`AR09` and `AR11`--`AR1A`
(`/tmp/msi_a7522_dsdt.dsl:1367-2046,2047-2454`).  Emit a bridge `_PRT` only
for an enabled bridge, preserving the swizzle for its downstream devices.

### USB

MSI's USB wake map is concrete:

| PCI function | AML name | `_PRW` GPE |
|---|---|---:|
| D29:F0 | USB0 | `03` |
| D29:F1 | USB1 | `04` |
| D29:F2 | USB2 | `0c` |
| D29:F3 | USB5 | `20` |
| D29:F7 | EUSB | `0d` |
| D26:F0 | USB3 | `0e` |
| D26:F1 | USB4 | `05` |
| D26:F2 | USB6 | `20` |
| D26:F7 | USBE | `0d` |

Each `_PRW` calls `GPRW(gpe, 4)`, but `GPRW` dynamically clamps the deepest
wake state to the firmware-enabled sleep-state set
(`/tmp/msi_a7522_dsdt.dsl:225-249,5601-5897`).  UHCI `_PSW` writes only bits
1:0 of PCI config byte `c4` (`3` for wake enabled, `0` otherwise), using a
Preserve field.  UHCI `_S3D` returns D2 for selected OS identities and D3
otherwise.  EHCI has `_PRW` but no equivalent `_PSW` in this template.

The matching `_GPE` methods notify the USB device and `PWRB`; GPE `20` serves
both USB5 and USB6, and GPE `0d` serves both EHCI functions
(`/tmp/msi_a7522_dsdt.dsl:6993-7035`).  `_L09` similarly notifies all ICH10
and IOH PCIe port scopes (`7037-7056`).

These methods are suspend/wake policy, **not** normal USB interrupt setup and
not an EHCI/UHCI initialization sequence.  The minimal OS path should expose
the enumerated PCI controllers and the validated direct `_PRT` routes while
keeping `GPE0_EN`, USB legacy SMI, USB2 legacy SMI, and Intel USB2 SMI clear.
Omit `_PRW`, `_PSW`, and USB GPE methods until S3/S4 and board wake wiring are
independently tested.  The existing firmware USB-enumeration failure occurred
in polling and is not explained by disabled PCI IRQ routing.

### SATA

The MSI DSDT names D31:F2 `SATA` and D31:F5 `SAT1`, each with PCI-config
operation region `40h`--`5fh` and legacy IDE `_GTM`, `_STM`, and `_GTF`
methods (`/tmp/msi_a7522_dsdt.dsl:4149-4877,4879-5598`).  Neither scope is an
AHCI initialization recipe and neither has an `_STA` gate.

B06VP's isolated AHCI stages instead established D31:F2 as `8086:3a22`, class
`010601`, with `MAP=0060`, `PCS=a23f`, and `SCGC=00000193`; D31:F5 was hidden.
Continue the existing exact-gated `MAP`, port-enable, and selected `SCGC`
stages.  Do not copy the legacy SATA AML and do not publish D31:F5.  AHCI
needs no vendor IDE channel objects for a minimal native namespace.

## SCI, SMI, sleep, reset, and power loss

### Status versus enable registers

The B06VP PM census used live PMBASE `0500` and found this fail-closed state:

| Register | Captured value | Access class and required treatment |
|---|---:|---|
| `PM1_STS +00h` | `0801` | W1C status: `PRBTNOR_STS` bit 11 and `TMROF_STS` bit 0 |
| `PM1_EN +02h` | `0000` | ordinary enable; keep zero until one source is tested |
| `PM1_CNT +04h` | `00000000` | ordinary control; `SCI_EN` clear |
| `GPE0_STS +20h/+24h` | `6eff0000/00000000` | W1C status; historical GPIO events, not enables |
| `GPE0_EN +28h/+2ch` | `00000000/00000000` | ordinary enables; keep zero |
| `SMI_EN +30h` | `00000000` | ordinary enables; keep the full dword zero under `NO_SMM` |
| `SMI_STS +34h` | `00006100` | W1C summary: periodic, TCO, and PM1 status |
| `ALT_GP_SMI_EN +38h` | `0000` | ordinary enables; keep zero |
| `ALT_GP_SMI_STS +3ah` | `6eff` | W1C status; does not prove routing or active SMI |
| `GPIO_ROUT D31:F0+b8h` | `00000000` | all sixteen 2-bit GPI routes disabled; keep zero |

The W1C behavior is explicit in `common/pmutil.c:26-37,56-69,97-113,
191-204`; the distinct enable/control offsets and masks are in
`common/pmutil.h:36-55,61-104`.  Never clear a status register by writing
zero, never write back an unreviewed aggregate value, and never treat a set
status bit as proof that its interrupt is enabled.

Before SCI is enabled, clear only `PRBTNOR_STS` with
`outw(PMBASE + PM1_STS, 0800)`, while PM1/GPE/SMI enables and both IRQ9
delivery paths remain masked.  `i82801jx/lpc.c:262-268` explains why a pending
power-button-override status can hold SCI asserted.  Preserve the timer status
bit for the dedicated timer experiment.

### One-source SCI proof

The next SCI-capable experiment should be a separate exact-gated image:

1. Assert LPC identity and live PMBASE; assert PM1_EN=0, both GPE0_EN halves=0,
   SMI_EN=0, ALT_GP_SMI_EN=0, GPIO_ROUT=0, and low PM1_CNT=0.  Assert the
   ICH10 IOAPIC is the expected 24-entry instance and GSI9 is masked.
2. Mask legacy PIC IRQ9 as well as IOAPIC GSI9.  B06VP uses a LAPIC ExtINT
   path, so leaving the PIC source open defeats the IOAPIC mask.
3. W1C only `PRBTNOR_STS`; require it clear on readback.  Set only `SCI_EN`
   with a 16-bit PM1_CNT RMW; require all event enables still zero.  Do not set
   `SMI_EN.BIOS_RLS`—that bit asserts SCI rather than enabling ACPI mode.
4. For a later PM-timer test, install a bounded handler first; program GSI9
   fixed delivery, level/high polarity, BSP destination, still masked.  With
   `TMROF_EN=0`, W1C only `TMROF_STS`; set only `TMROF_EN`; unmask GSI9 last.
5. On the one expected interrupt, read PM1_STS, require TMROF, acknowledge
   only TMROF W1C, issue LAPIC EOI, and record the bounded count.
6. Cleanup in this order: mask GSI9, clear TMROF_EN, acknowledge only any
   observed TMROF status, EOI if necessary, and verify all other enables stayed
   zero.  A timeout or unexpected source terminates the trial and leaves the
   source masked.

SCI9's level/high override may be emitted after this proof.  Keep the IOAPIC
entry masked at firmware handoff; the OS owns final vector/destination setup.

### Why vendor sleep AML is not reusable

MSI `_PTS` calls `SIOS`, `SPTS`, and `NPTS`; `_WAK` calls `SIOW`, `SWAK`, and
`NWAK` (`/tmp/msi_a7522_dsdt.dsl:7744-7789,8289-8304`).  `SIOS` enters Fintek
configuration mode and programs logical device `0a` wake policy
(`3320-3338`).  More importantly, `SPTS` sets `PS1S`, `PS1E`, and `SLPS`;
the field layout places PS1E at PMBASE+30 bit 4 (`SLP_SMI_EN`) and PS1S at
PMBASE+34 bit 4 (`SLP_SMI_STS`, W1C)
(`/tmp/msi_a7522_dsdt.dsl:2740-2793`; `common/pmutil.h:87-101`).  The vendor
sleep transition intentionally hands off through SMM.

Consequently `NO_SMM` firmware must keep `GBL_SMI_EN`, `SLP_SMI_EN`, APMC,
TCO, periodic, and all legacy-USB SMI enables clear and must not install the
vendor `_PTS/_WAK` methods.  MSI conditionally publishes S1, S3 (`SLP_TYP=5`),
and S4 (`6`) through BIOS globals; S5 (`7`) is unconditional
(`/tmp/msi_a7522_dsdt.dsl:8242-8288`).  A minimal native `_S5` poweroff path
may be tested separately with SLP_SMI disabled.  S1/S3/S4 and wake methods
remain out of scope until native sleep/resume and board wake policy exist.

### Reset and power-after-G3

Both MSI-static and Intel-comparative FADTs advertise `cf9=06`; this is
useful corroboration for a board-native reset register.  It does not authorize
an incidental reset during routing or ACPI work.  Add the FADT reset capability
only after an intentional, recoverable reboot trial verifies the exact current
reset path and retained bring-up guards.

Coreboot names D31:F0 `GEN_PMCON_3` bit 0 `SLEEP_AFTER_POWER_FAIL` and documents
the inverse policy (`0` -> S0/on, `1` -> S5/off) in
`i82801jx/lpc.c:158-201`; bits 1 and 2 are RTC power-failed and battery-dead in
`common/pmutil.h:15-18`.  B06VP captured `GEN_PMCON_3=02`: bit 0 clear but
`RTC_POWER_FAILED` set.  Controlled 15-second and 60-second AC removals in
`Documentation/b06vh-primary-workspace-rearm-seabios.md` nevertheless returned
to S5 and required the physical button.  Thus bit 0 alone is not a working
power-restore implementation.  Preserve it, do not promise remote/automatic
power-on, and do not advertise S4 RTC wake while the RTC failure and the
remaining board power dependency are unresolved.

## Intel comparison: useful constraints, not MSI policy

The DX58SO templates corroborate the ICH10 mechanisms but demonstrate why
literal reuse is unsafe:

- FADT uses SCI9 and `cf9=06`, but PMBASE `0400` and B2 commands `a0/a1`, not
  MSI's `0800` and `e1/1e`.
- Its direct D26, D27, D28, and D29 routes agree with MSI.  D31 does not:
  Intel routes A/B/C/D to GSI `19/18/16/21`, whereas MSI lists
  `18/19/18/-`.  D31 is visibly board policy.
- Intel MADT has ICH10 IOAPIC ID8 at `fec00000`/GSI0 **and a second IOH
  IOAPIC** ID9 at `fec90000`/GSI24.  MSI MADT contains only the ICH10 instance;
  do not import the second IOAPIC.
- Intel MCFG has a zero base placeholder, confirming that template fields need
  runtime patching.
- Similar USB and SATA scopes corroborate chipset structure, but at least the
  D29:F3 USB wake GPE differs from MSI.  Use MSI AML for MSI wiring and omit
  wake AML until tested.

## Minimal staged implementation plan

1. **Quiescent resource stage.** Keep the already proven ICH10 IOAPIC decoded
   at `fec00000` with every entry masked.  Keep HPET decoded at `fed00000`,
   timer interrupts disabled, and the main counter stopped except inside the
   bounded polling-timer test.  Reserve both apertures and live ECAM/RCBA; no
   ACPI publication is required yet.
2. **USB direct-route stage.** Exact-gate and write only D26IR=`3250` and
   D29IR=`0237`; leave PIRQ route bytes `80`, PCI `INT_LINE`, DnnIP, and
   IOAPIC entries untouched.  Reconfirm all eight measured interrupt pins and
   full-word readback.  This is useful OS routing but is not a USB-enumeration
   fix.
3. **One device-interrupt proof.** After QPI/DMI completion, choose one known
   source, install its handler and EOI path, program one masked IOAPIC entry,
   clear only that source's documented W1C status, then unmask last with a
   timeout.  Restore mask/enables on every exit.  Repeat the HPET route-20
   experiment only here, not in early CAR.
4. **SCI-with-no-source stage.** With PIC IRQ9 and IOAPIC GSI9 masked and all
   enables zero, W1C only PRBTNOR_STS, then set only PM1_CNT.SCI_EN by 16-bit
   RMW.  Verify that no interrupt fires.
5. **One PM-timer SCI stage.** Follow the six-step bounded sequence above.
   Do not add GPE, power-button, TCO, USB wake, or SMI sources to this image.
6. **Minimal native tables.** Generate, never copy, FADT/MADT/MCFG and direct
   APIC `_PRT` entries from live resources and enumerated pins.  If the first
   port intentionally has no PIC-routing mode, omit link devices instead of
   advertising disabled or fictitious legacy routes.  Use SCI9; one actual IOAPIC;
   actual BSP/online CPUs; no B2 SMI command; no 8042, S3/S4, RTC wake, USB
   wake, or hidden D31:F5.  Add the HPET table only after delivery proof and
   fill its block ID from `GCAP_ID`.
7. **Later isolated milestones.** Test `cf9=06` reset, native `_S5`, PWRBTN
   SCI, selected PCI/USB wake, RTC recovery, and power-after-G3 separately.
   SMM-backed vendor sleep/USB-legacy behavior remains disabled unless a
   reviewed native SMM design is explicitly introduced.

Every mutation stage should preserve the existing exact platform/phase gates,
use selected-field or complete-register post-readback as appropriate, emit a
distinct terminal diagnostic on mismatch, and retain the socketed-flash
recovery path.

## Unresolved items

- No raw vendor-live ACPI dump or vendor `inteltool`/`lspci` capture is present
  locally; the V8.14B8 register comparison remains secondary evidence.
- The 8F0 static DSDT may be patched differently by other MSI BIOS revisions.
- D25 routing, D31 pin D, and D27 pins B--D lack matching MSI AML entries.
- IOH interrupt-controller needs beyond the one MSI MADT IOAPIC are not proved.
- PIT IRQ0 -> GSI2 and HPET interrupt delivery are not yet proved in the
  intended post-QPI/DMI state.
- USB normal IRQ delivery, USB wake, physical wake-source wiring, and all GPE
  polarities remain untested.
- The TCO block was not captured even though SMI_STS.TCO was set; TCO must stay
  disabled until its underlying status is decoded.
- `RTC_POWER_FAILED=1`, the failed Fintek keyboard-interface test, and the
  unexplained power-after-G3 dependency prevent RTC-wake, 8042, or automatic
  restore claims.
- Native S5, reset-table consumption by an OS, and all S1/S3/S4 transitions
  require dedicated hardware records.

## Evidence hashes

| Repository artifact | SHA-256 |
|---|---|
| `research/msi/platform-init-audit-2026-09-06.md` | `7fdf8a6b521b4bab87084fcbca1065767def5af86af066cfa5de61ccdcfa508d` |
| `research/msi/acpitbl-static-analysis-2026-09-06.md` | `bfdc17dae6dbaafe41b794b62d166f5386047c77d563b5161ce7471b64521e7a` |
| `research/msi/b06vp-pm-smi-gpio-routing-census-2026-09-06.md` | `6b008044e2e6fd2fac8b637d8933985d10b7d70796c9eb17c8fb069111ce56a9` |
| `research/msi/captures/2026-09-06-b06vp-pm-smi-gpio-census-run.raw` | `81d22eb61f6bd42757b3ee320d4874117991454e9678aac8bce20436148151e4` |
| `research/scripts/ich10-pm-smi-gpio-routing-census-b06vp.xrs` | `cd8890aee61bd20e64e5809d3ae8a6c53ed5b8bc388f9238222f7ee256bfdda6` |
| `research/msi/captures/2026-09-06-b06vp-car-ich10-routing-readonly.raw` | `d07c2b6d3a8f73458578cbf3e1b7d38a38449d8912f3139c3278a07c45bd4429` |
| matching routing metadata | `6f4fff1f19f9864557542a983d45257939843abc2cd135906842a2e7ac44cf0f` |
| USB pin-routing metadata | `02a984d5fc64b469b82fa811ca46644d7263e7bc836ff457f56eccbe2acfdce1` |
| USB pin-routing raw (named in metadata) | `b5d63502afbdfaf013d8ca6abac33691e65a4d6701300a8ce57cefd13756e7b0` |
| Fintek keyboard-interface raw | `b9350a8dffbd8425d08a81a35d51d40d7c208710f406df915c31194b0d9d3f1f` |
| `research/msi/b06vp-car-hpet-ioapic-delivery-2026-09-06.md` | `e4c725dc59893f548abbc45c0752a3e4dd4e78528143107ba3f84228334bc9e1` |
| HPET decode raw | `b89e8ed5741385424b00f1d1287df742d276d652e307de7ac042506034af434f` |
| HPET counter run/stop raw | `842837b01a06078d5159ddec953e5de94fa2be3cfec1a7e45280612d053ca234` |
| IOAPIC ID/version raw | `cf526730810ec1b4541181ff5570c42a49427218e9d6da6a7ca79bbfd61fec8e` |
| IOAPIC redirection 00--05 / 06--11 | `3bc9fc4498dbbdd3f8ef4cf770c5c82be1bea567d48dc46ab7aaae43d073dbcd` / `a290511ce92fa9a477e873079f53021a53f456ab5bbb56b0a4e8ee1b973ecd23` |
| IOAPIC redirection 12--17 / 18--23 | `e6df0992f5e3fa6d60d0dca1951412c1737b394210fcfc7ec844fb87bdbc0f3a` / `ad3daf89293c847bbc8725e03b4628dc06cca4fba55fc1d0bb0fc3b16920e200` |
| `Documentation/b06vh-primary-workspace-rearm-seabios.md` | `3266abd47eec2c234b6da5f9e8d8d11a2af0936320d8f33c142b29ed51835abf` |

Exact coreboot-source hashes used for register semantics:

```text
1666059f164ccc74f4936263df25b49d409e2b8442a29d8c80ec7097777ddd0d  coreboot/src/southbridge/intel/common/rcba_pirq.c
6343b13d8b2198d85bcc7f862c3b9de12d7ea0f65aa6b79b3f242ae0f30a90b2  coreboot/src/southbridge/intel/common/rcba_pirq.h
2dcfd65b1cf72cd69ee50e789d347e1051679b48e8bc78a764e0d1fb7e743ce6  coreboot/src/southbridge/intel/common/pmutil.h
1fab6d75cf904b5f732366911aaffc3b060f34e75ea3d56823820a8554b76b8d  coreboot/src/southbridge/intel/common/pmutil.c
78c1c96ea50b21b8db53a91fdf4c21c060da17b3d6d2bb85173090542d8f9fc5  coreboot/src/southbridge/intel/common/pmbase.c
e2020b4ce1070704b17d837540eb1171582e40ff4987f095c31645d4adbf8d90  coreboot/src/southbridge/intel/common/hpet.c
94d55ef8e954b1c08d7a76101085c9219e3444107b6507965c732428c086cbf6  coreboot/src/southbridge/intel/i82801jx/lpc.c
4cd86d9277f57a50e3df23526449f330963d9261895bea52bbfb19e41f46fae5  coreboot/src/southbridge/intel/i82801jx/i82801jx.h
98dcbe9569799a73d6f48bc59e643643f02fd65f94612107c8f4cd26509fd58e  coreboot/src/southbridge/intel/i82801jx/fadt.c
b8a3ef574979a90116feb5bbc2d49a01775f9c2b9bf8280db62ec0cd7899880c  coreboot/src/include/acpi/acpi.h
```
