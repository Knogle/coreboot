> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# Intel desktop X58 ACPI templates versus the B06WJ release

Status: **local static analysis and repeatable extraction only**. B06WJ again
shows Windows `ACPI_BIOS_ERROR (0xA5)` according to the operator. No bugcheck
parameters identify its subtype. Nothing here executes vendor code, programs
hardware, changes a ROM or establishes a Windows fix.

## Inputs and reproducible provenance

The comparison uses two independent Intel desktop firmware releases, not
Ironlake and not just files with a plausible module name:

| Input | Size | SHA-256 |
|---|---:|---|
| DX58SO2/DX58OG `SO0920P.bio` | 3,991,700 | `4e6b895898db07e78d8627ae6fb9c81df8df9ad950f24c1617ad4bb7ac5ce52c` |
| DX58SO `SO5600P.bio` | 3,849,620 | `d26337570c7a8efde9cce5002310a4f96583e667f063a691e4d582f55c0f866b` |
| B06WJ DSDT extracted from release ROM | 1,076 | `63ba8a0d497cc6998bfffd35ea816d7bba5d85f811337ce0bfb21f7c854d7d03` |

The WJ baseline is the immutable
`builds/experimental/b06wj-release-20260907/dsdt-from-rom.aml` and associated
`dsdt.dsl`, not a subsequently edited source tree. That DSL has SHA-256
`ca21db13d6a409f7f13801b68d4f89a05d340029f920c5b6120be1fb2a1e5df1`.

The original Intel UEFIExtract reports identify ACPI freeform FFS GUID
`7E374E25-8E01-4FEE-87F2-390C23C606CD` within compressed DXE firmware volumes.
The new [extraction tool](../../scripts/extract_intel_x58_acpi.py) checks the
original compressed-section header/CRC32 against those reports, decodes the
Intel type-2 LZMA stream, identifies a matching FFS header rather than a driver
GUID constant, and extracts all 13 raw sections without changing their bytes.
Their offsets below are explicitly **decompressed offsets**, not addresses
inside the compressed `.bio`.

| Firmware | Original-image compressed-section offsets | ACPI FFS offset in decompressed stream | ACPI FFS size / SHA-256 |
|---|---|---|---|
| 0920 | `0x12b4f0`, `0x2fbe38` | `0x90124` | `0x130ba` / `f82b4028e8f55d44d2bfe5e0cf6d116604149961f78087b68345ff1a2426917f` |
| 5600 | `0x120488`, `0x2e0770` | `0x8744c` | `0x1791e` / `e7b6ba1acce567f95f13983cc16d8acbca627de9912ca05ccb74d4ab752fc541` |

Both copies within each firmware have identical FFS and raw-section hashes.
An independent second extraction reproduced all 26 raw sections per firmware;
all 13 first-copy 0920 sections also match the older extractor corpus exactly.
Across the two different releases, **only raw sections 02 (DSDT) and 11
(CPU-power SSDT) differ**. This makes the shared fixed-table templates a useful
Intel-generic reference, not an MSI policy source.

| Table | Raw index | 0920 body offset within FFS | 5600 body offset within FFS | Size 0920 / 5600 |
|---|---:|---|---|---:|
| FADT | 00 | `0x1c` | `0x1c` | 244 / 244 |
| FACS | 01 | `0x114` | `0x114` | 64 / 64 |
| DSDT | 02 | `0x158` | `0x158` | 16,622 / 17,954 |
| MADT | 03 | `0x424c` | `0x4780` | 312 / 312 |
| MCFG | 05 | `0x43cc` | `0x4900` | 60 / 60 |
| HPET | 07 | `0x44b8` | `0x49ec` | 56 / 56 |
| CPU-power SSDT | 11 | `0x48b8` | `0x4dec` | 59,092 / 76,292 |

Selected table hashes:

- Shared FADT: `7444ed55f93b1f9070e0da604d4a4a828e06dda3370d43c88cba8a2f5cdba708`.
- Shared MADT: `94101d113cf21668e0c99d5ff5670dd44453d6e58e6da94b846a9c82b5c05ad4`.
- Shared HPET: `dff434bcc799ffc501e8dc578f3cc251998c05ea161d4131c434b11c109b869b`.
- 0920 DSDT: `b0877577dea5bb6ed23eb0d1816fc5bcf15b0e7497ed4b7a945fd3065fa0ed7b`.
- 5600 DSDT: `f138ca8d6174e728a536513c863b28bdc5a870d95473073d7ad815e643785774`.
- 0920 CPU SSDT: `6c715f0815f0142d96f39960d8a886b9e86333dd0ef4db9d249a6e1aed95fe16`.
- 5600 CPU SSDT: `8740391611cfa6601d6dbb096573947dd65dc4c1b370e86c5866852c15640315`.

## Fixed-table contract: useful values, unsafe runtime templates

Both Intel FADTs are revision 3 and contain SCI 9, PM1 event/control/timer
at `400/404/408`, GPE0 `420` length 16, flags `0x4a5`, SMI port `b2` with
enable/disable commands `a0/a1`, and reset register `cf9`, value `06`.
Their OEM fields and DSDT/FACS pointers are zero and checksums invalid. These
are templates requiring firmware publication, not tables suitable for direct
installation. The MSI live reference uses PMBASE `800`; WJ correctly describes
its own observed PMBASE `500`. Copying Intel's `400` would be wrong.

Both Intel MADT templates describe 16 disabled Local APIC entries and two
IOAPICs: ID 8 at `fec00000`/GSI 0 and ID 9 at `fec90000`/GSI 24. IRQ0 maps
to GSI2; SCI9 maps to GSI9 with flags `0x0d`, agreeing in semantics with WJ's
SCI override. Their per-CPU NMI entries and disabled CPU flags require runtime
selection. The MSI live capture instead shows one IOAPIC at `fec00000`.
Importing Intel's second IOAPIC or its CPU map is unjustified.

Both Intel MCFG templates have base zero and buses 0--63, and their HPET
templates have block ID zero, base `fed00000`, minimum tick 1. Checksums are
invalid. WJ's actual ECAM range and measured HPET ID are stronger evidence
than these unpatched placeholders. Their reset/SMI/power flags likewise
presuppose Intel's platform services, absent from the NO_SMM experiment.

## Namespace comparison against the exact WJ release

Both Intel DSDTs and their CPU SSDTs disassemble using IASL 20251212 with
external-table resolution. Full vendor DSL remains private under
`blobs-local/intel-dx58so/acpi-analysis-20260908/`.

| Interface | Both Intel desktop DSDTs | Pinned WJ DSDT | Interpretation |
|---|---|---|---|
| System states | Root `_S0={0,0,0,0}`; `_S5` has SLP_TYPa 7, SLP_TYPb 0; also S3/S4 | No `_S0` or `_S5` | Concrete shared omission; inspect minimal S0/S5 contract separately, without advertising unsupported suspend |
| VGA aperture | PCI0 root `_CRS` explicitly produces `a0000..bffff`, length `20000` | Root MMIO producer only `c0000000..dfffffff` | Concrete resource-hierarchy difference for the active legacy-VBIOS GPU; possible OS-facing experiment, not a proven A5 cause |
| CPU | 16 legacy `Processor` objects under `_PR`, IDs 0..15, PBLK `410`, length 6 | One `ACPI0007` device `CP00`, UID 0 | Different namespace model; WJ describes its single advertised BSP. The legacy Intel model is not evidence that ACPI0007 is invalid |
| CPU power | Large SSDT supplies `_PDC`, `_OSC`, `_CST`, `_PCT`, `_PSS` | No CPU-power methods | Deliberate missing power-management support, not an established prerequisite for boot |
| Interrupt model | `_PIC` selects link-device/PIC versus direct-GSI/APIC `_PRT` packages | `_PIC` exists; root `_PRT` unconditionally returns the 20-entry direct-GSI RPRT, two child routes exist | WJ does **not** return an empty `_PRT` before `_PIC(1)`; no such defect exists in the release |
| Motherboard resources | PNP0C02 and LPC device resource descriptions | MRES/PNP0C02 already reserves PM/GPIO/SMBus/UART/KBC/ELCR, ECAM/IOAPIC/RCBA/LAPIC/ROM; WJ adds PIC/PIT/RTC | Generic motherboard reservations are **not missing**; comparisons must identify an exact range or hierarchy mismatch |
| PCI capabilities | PCI0 `_OSC` negotiates and masks native control | No PCI0 `_OSC` | Additional platform interface, but absence alone does not identify this A5 |

The two strongest small **comparison-derived hypotheses** are therefore the
root S0/S5 descriptions and the missing legacy VGA root aperture. Neither is
a decoded Windows diagnosis. Test changes separately or label a grouped build
explicitly; do not undo the established RAM/QPI/USB path merely to reproduce
its earlier milestones. The four Windows bugcheck parameters remain the
direct way to distinguish namespace, resource, table, or interrupt rejection.

## Why even the valid Intel DSDT is not transplantable

Valid DSDT checksums do not imply that its address and resource fields are
ready for an OS. Both contain `OperationRegion (PSYS, SystemMemory,
0x30584946, 0x100)`: the little-endian bytes spell `FIX0`. Fields provide
platform/CPU/TPM/power policy. PCI root resource descriptors contain `FIX1`,
`FIX2`, `FIX3` and `FIX4` markers in fields that must become actual bus and
memory windows. This is direct static evidence of relocation/policy patching,
analogous to MSI's runtime BIOS-data region. This analysis does not reconstruct
the entire runtime patcher or its ABI.

The FFS also stores Intel-specific CPU/board policy, and its SMI and sleep
methods depend on the original services. Neither a copied DSDT nor copied EFI
ACPI DXE module can replace native WJ tables without reproducing those
dependencies. The appropriate reuse here is documented interface semantics
and measured ranges, not vendor module execution.

## Reproduction

The supplied `--output` directory must be new and beneath `blobs-local`.
Existing inputs, reports, outputs and WJ artifacts are never overwritten.

```sh
python3 scripts/extract_intel_x58_acpi.py \
  blobs-local/intel-dx58so/SO0920P.bio \
  --report blobs-local/intel-dx58so/SO0920P.bio.report.txt \
  --output blobs-local/intel-dx58so/acpi-repeat-0920
python3 scripts/extract_intel_x58_acpi.py \
  blobs-local/intel-dx58so/5600/extracted/SO5600P.bio \
  --report blobs-local/intel-dx58so/5600/extracted/SO5600P.bio.report.txt \
  --output blobs-local/intel-dx58so/acpi-repeat-5600
coreboot/util/crossgcc/xgcc/bin/iasl \
  -e blobs-local/intel-dx58so/acpi-repeat-0920/copy0-11.raw \
  -p blobs-local/intel-dx58so/acpi-repeat-0920/dsdt \
  -d blobs-local/intel-dx58so/acpi-repeat-0920/copy0-02.raw
```

Retained full manifests are `acpi-analysis-20260908/final0920/manifest.json`
and `final5600/manifest.json` under the same private Intel directory. They
record every raw hash, table checksum/decoded fields, original and decompressed
offset, report/source hashes, script hash and Python version. Tool script
SHA-256 at this analysis is
`b68fbbdcabc3a975a475e6aad07ca234fd2ba415139beff07dafd7ab6f4e6c07`.
IASL 20251212 binary SHA-256 is
`4b1e5067ef40ec0dbbc4f4717e4fcf1596b0a39e195096444d16bbe108a122b9`.
The old reports retain their original UEFIExtract provenance sidecars; the
new extraction requires no currently installed UEFIExtract executable.
