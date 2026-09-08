> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# MSI X58 Pro-E vendor ACPI BIOS-NVS decode

Status: read-only decode of one running vendor-firmware reference capture. The
raw proprietary tables and RAM remain under ignored `blobs-local/`; this file
contains only offsets, values and resource correlations. Nothing here is a
portable AML/NVS image or a justification to copy runtime state.

## Provenance and address relationship

The running MSI DSDT has SHA-256
`99f49927294b2217ec1e6de06df3c8b16c6be7508b45277a7771cca71b380e8f` and
contains:

```text
OperationRegion (BIOS, SystemMemory, 0xBF78E064, 0xFF)
```

The live FACS is at `BF78E000`, length 64 bytes, SHA-256
`44341977e5b1ae7e1bee5be50d27b4881a24dab6c04801b34945c3e241c68cb9`.
Thus the AML region starts exactly at `FACS + 0x64`, not at the
`FFFFFF00` placeholder found in the unpatched firmware DSDT. The 255-byte
read has SHA-256
`d9bc49365e3029e1fe08304c6aacd6e7ce657ff7d6f349010716e88fc0c6d28d`.

## Named-field decode

All multibyte values below are little-endian and offsets are relative to
`BF78E064`.

| Field | Offset/width | Live value | Correlation or boundary |
|---|---:|---:|---|
| `SS1..SS4` | `00`, four bits | byte `09` | `SS1` and `SS4` set; vendor sleep policy only |
| `IOST` | `01`/16 | `0000` | runtime firmware global; meaning not inferred here |
| `TOPM` | `03`/32 | `c0000000` | top of low usable address space |
| `ROMS` | `07`/32 | `ffc00000` | start of vendor ROM reservation |
| `MG1B/MG1L` | `0b/0f`, 32 each | `000d0000/00010000` | PCI window `D0000-DFFFF` |
| `MG2B/MG2L` | `13/17`, 32 each | `c0000000/20000000` | PCI window `C0000000-DFFFFFFF` |
| `DMAX` | `1c`/8 | `8b` | name retained; semantics not inferred |
| `HPTA` | `1d`/32 | `000e4f60` | name retained; it is not the live HPET MMIO base |
| `CPB0..CPB3` | `21..30`, 32 each | all zero | runtime CPU-policy globals |
| `ASSB/AOTB/AAXB` | `31/32/33` | all zero | vendor sleep/wake state |
| `SMIF/DTSE/DTS1/DTS2/MPEN/TPMF` | `37..3c` | all zero | vendor SMI/thermal/TPM policy |
| `MG3B/MG3L` | `3d/41`, 32 each | `f0000000/0ed90000` | PCI window `F0000000-FED8FFFF` |
| `MH1B/MH1L` | `45/49`, 32 each | `fed90000/00000000` | motherboard-reserved tail begins at `FED90000`; AML derives its length separately |
| `OSTP/DIOH/VGAR/B0SE/B0SU` | `4d..51` | all zero | runtime policy flags |
| `B0IB/B0IL` | `52/54`, 16 each | `0000/0000` | final two named fields |

The named field layout ends at offset `0x56`. Bytes after that point in the
255-byte read belong to adjacent runtime/NVS data (including an `SSDT`
signature later in the sample); they are not part of these named DSDT fields.

## Resource-map correlation

The three nonzero `MG` pairs reproduce the running reference kernel's
reported PCI root resources exactly:

```text
000d0000-000dffff
c0000000-dfffffff
f0000000-fed8ffff
```

The same kernel reports VGA/option-ROM use within the legacy holes and PCI
Bus 0 owning `A0000-BFFFF`. This explains how the vendor's dynamic `PCI0._CRS`
is instantiated. It does not make these values appropriate for the B06WJ
target: the reference has a different RAM size and PCI population, and its
AML methods also rely on original SMI/SMM and sleep services.

The clean-room port should derive static/current resources from its own E820
and PCI allocation and describe them directly. It must not import the patched
vendor DSDT, the FACS-adjacent region address, sleep flags, SMI methods or an
entire NVS byte image.

## Reproduction

With the private capture present, the table can be disassembled without
changing it:

```sh
coreboot/util/crossgcc/xgcc/bin/iasl -d \
  blobs-local/msi-x58-pro-e/x58-acpi-a5-reference-20260908-01/acpi-tables/DSDT
```

Decode only the named offsets above from
`blobs-local/msi-x58-pro-e/x58-acpi-nvs-reference-20260908-01/bios-nvs-ff.bin`.
Do not treat the free-running/runtime fields as a stable configuration image.
