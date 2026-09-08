> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# MSI 8F0 ACPI template and DSDT analysis

## Scope and reproducibility

This is a structural analysis of two modules extracted locally from the
user-supplied `A7522IMS.8F0` image.  Neither proprietary input nor a decompiled
copy is stored in the public tree.

```text
dc8ee36106d5eb10d19ff8bccba8936dea340fe343f9cd5210029bdfe490c6fd  ACPITBL_SEG.bin (44288 bytes)
f50bf11a566525a0a05ea52c7958f9f5a25667cc74fbfb8b333ac9b5305211cb  amibody_10.rom (28290 bytes)
```

The machine-readable inventory can be reproduced with:

```bash
python3 scripts/analyze_msi_acpi.py ACPITBL_SEG.bin amibody_10.rom
iasl -d amibody_10.rom
```

The decompilation used the coreboot toolchain's ACPICA `iasl` 20251212.  The
new inventory tool is read-only, dependency-free and covered by synthetic
unit tests.  It does not extract or redistribute either input.

## Static table layout

`ACPITBL_SEG.bin` contains the following table templates:

| offset | signature | length | relevant static state |
|---:|:---:|---:|---|
| `0000` | RSDT | `0028` | one zero table pointer |
| `0100` | XSDT | `002c` | one zero table pointer |
| `0200` | FACP rev 2 | `0084` | populated legacy PM policy, zero FACS/DSDT pointers |
| `0290` | FACP rev 3 | `00f4` | empty alternate template |
| `0390` | APIC | `00ac` | LAPIC/IOAPIC/override skeleton |
| `0440` | MCFG | `003c` | one segment covering buses `00..ff` |
| `a480` | FACS | `0040` | zero wake vectors and locks, version 1 |
| `a4c0` | OEMB | `007a` | AMI-specific payload, not interpreted |
| `aa6f` | HPET | `0038` | HPET address and minimum tick |

All standard checksums except that of the separate DSDT are invalid in this
static module.  This is expected: pointers, APIC enable flags and other bytes
are runtime inputs, and the loader later regenerates the checksums.  The zero
RSDT/XSDT entries and invalid checksums are evidence that this module is a
template, not an ACPI image that can be copied verbatim into coreboot.

The separate `amibody_10.rom` is a complete, checksum-valid DSDT with OEM ID
`A7522`, table ID `A7522800`, revision 1 and OEM revision `0x800`.  Static
analysis of the loader shows that AML is supplied separately and copied into
the reserved runtime area; it is not embedded inside `ACPITBL_SEG.bin`.

## FADT semantics

The populated revision-2 FADT describes:

```text
SCI interrupt                 9
SMI command port              00b2
ACPI enable / disable         e1 / 1e
P-state control               e2
PM1 event / control           0800 / 0804
PM2 control                   0850
PM timer                      0808
GPE0                          0820
lengths                       4 / 2 / 1 / 4 / 16
IAPC boot architecture        0003 (legacy devices + 8042 advertised)
flags                         000004a5
reset register                SystemIO 0cf9, width 8
reset value                   06
```

These values agree semantically with ICH10's PMBASE-relative register map and
with the vendor-booted reference's PMBASE `0801`.  They do **not** justify
moving the current early PMBASE from `0501` in the same experiment: that
would disturb already proved UART/SMBus/SeaBIOS behavior.  PMBASE migration,
PM register initialization and ACPI table publication must remain separate,
ordered changes.

The `8042` FADT flag is vendor policy, not proof that the current coreboot path
has a functioning keyboard controller.  It may be enabled only after LPC KBC
decode and the Fintek KBC logical device have been measured and proved.

## MADT, MCFG and HPET semantics

The static MADT uses LAPIC base `fee00000`, PCAT-compatible flag 1, IOAPIC ID 1
at `fec00000` with GSI base 0, and two interrupt-source overrides:

```text
ISA IRQ 0 -> GSI 2, flags 0000
ISA IRQ 9 -> GSI 9, flags 000d
```

It reserves twelve local-APIC records with processor IDs 1 through 12 and
APIC IDs `80..8b`, all disabled in the template.  The vendor loader patches
the active CPU set.  Coreboot must instead derive LAPIC entries from the CPUs
it actually starts; copying twelve disabled vendor entries is not useful.

The MCFG records PCIEXBAR `e0000000`, segment 0, buses `00..ff`.  That base
matches the current ECAM observation, but an eventual MCFG must be emitted
from the base and bus span actually programmed by the X58 path.

The HPET template points to `fed00000` and records a minimum clock tick of
14318.  Its event-timer-block ID is zero in the static template, so coreboot
should publish HPET only after enabling RCBA HPTC, validating the live HPET ID
register and reserving its MMIO window.

## DSDT findings relevant to bring-up

The DSDT provides board topology and policy clues, but also contains extensive
AMI runtime dependencies.  The useful, independently implementable facts are:

- `PCI0` is `PNP0A08`; `_PIC` selects link-device routing versus direct GSI
  routing.
- D31:F0 PCI config offsets `60..63` and `68..6b` are the eight PIRQ route
  bytes `PIRA..PIRH`.
- The direct APIC routing package maps ICH10 devices as follows:

  | device | INTA | INTB | INTC | INTD |
  |---|---:|---:|---:|---:|
  | D31 | 18 | 19 | 18 | — |
  | D29 | 23 | 19 | 18 | 16 |
  | D26 | 16 | 21 | 18 | 19 |
  | D27 | 22 | — | — | — |
  | D28 | 17 | 16 | 18 | 19 |

- UHCI functions are D29:F0/F1/F2/F3 and D26:F0/F1/F2; the two EHCI
  functions are D29:F7 and D26:F7.  The UHCI `_PSW` methods alter two wake
  bits at PCI offset `c4`; their `_PRW` objects describe GPE wake policy.
  Neither construct is necessary for pre-OS USB enumeration.
- The PS/2 keyboard is `PNP0303` at ports `60/64`, IRQ 1, and is visible only
  when the firmware I/O-status bitmap has bit 10 set.  The PS/2 mouse is
  `PNP0F03`, IRQ 12, controlled by bit 12.  This matches the independent
  finding that LPC_EN bit 10 is the first missing KBC-decode prerequisite,
  but it does not establish the Fintek programming sequence.
- The SATA AML covers both ICH10 SATA functions and legacy timing interfaces.
  It does not initialize the AHCI controller; that remains firmware/SeaBIOS
  work before an OS can use the ACPI namespace.
- PCI root resource windows are patched from runtime global values.  Literal
  zero/vendor resource-template addresses must not be copied into coreboot.

The DSDT also contains SMI, sleep/wake, TPM, PCIe wake and Super-I/O methods
that depend on AMIBIOS global state.  Reusing the AML wholesale would import
unknown SMI and NVS contracts, so it is explicitly excluded from the normal
coreboot path.

## Reconstructed loader behavior

The code following the static templates performs four relevant operations:

1. obtains the separate AML body and copies it into a reserved area of at most
   `0xa000` bytes;
2. patches FADT, RSDT and XSDT pointers for the selected ACPI revision;
3. appends constructed table addresses to the root table and recalculates each
   checksum; and
4. copies the table area and the FACS/OEMB block into runtime ACPI memory.

The checksum helper and pointer-appending code explain the zero pointers and
bad template checksums without requiring any assumption that the extraction
was corrupt.

## Coreboot implementation boundary

The safe implementation order inferred from this analysis is:

1. finish device initialization and resource allocation;
2. establish correct PIRQ/IOAPIC routing and prove timer/device interrupts;
3. enable and validate HPET independently;
4. construct dynamic memory and PCI resource reporting;
5. emit a minimal native DSDT plus FADT/MADT/MCFG/HPET using live values; and
6. advertise `8042` only if the isolated KBC path has passed.

No table from this corpus should be inserted as a blob.  The module is most
valuable as a semantic and topology reference for native, reviewable ACPI.
