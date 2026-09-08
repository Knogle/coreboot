> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06WJ runtime ACPI: reconstructed bytes and CTBL resource-flag defect

Read-only host analysis of the captured GRUB session, 8 September 2026.
No firmware source, ROM, vendor bytes, or hardware registers were changed by
this analysis. A Windows `0xA5` causal attribution is **not established**.

## Results

- All eleven requested native ACPI structures were reconstructed from explicit
  hexdump bytes, with no gaps or conflicting observations. All applicable
  checksums pass; FACS has no ACPI checksum field.
- Runtime DSDT is byte-identical to the pinned B06WJ release DSDT, not a stale,
  foreign, or silently edited table. Its SHA-256 is
  `63ba8a0d497cc6998bfffd35ea816d7bba5d85f811337ce0bfb21f7c854d7d03`.
- RSDT and XSDT contain the same six entries. FADT legacy and extended DSDT/FACS
  pointers agree and point to the reconstructed structures.
- DSDT and SSDT disassemble and compile with IASL 20251212 with zero errors,
  warnings, or remarks. DSDT recompilation is byte-identical. SSDT recompilation
  is **not** byte-identical: notably, a reserved resource flag is discarded.
- The runtime SSDT contains a demonstrable malformed flag in `CTBL._CRS`.
  The source is an unchanged common coreboot constant, not the new WJ DSDT.
  This supplies a concrete isolated experiment, but not yet a proven explanation
  for Windows rejecting the platform.

## Reproduce without hardware

From the repository root, choosing a new output directory:

```sh
python3 scripts/reconstruct_grub_acpi.py \
  research/msi/captures/b06wj-grub-rootdump-20260908-01.raw \
  research/msi/captures/b06wj-grub-fadt-20260908-01.raw \
  research/msi/captures/b06wj-grub-tables-20260908-01.raw \
  --output builds/experimental/b06wj-acpi-runtime-20260908
python3 -m unittest discover -s tests -p test_reconstruct_grub_acpi.py -v
```

The output directory already exists for the recorded run; the utility refuses
to overwrite it. Its `manifest.json` records every observed row, capture hashes,
table hashes and decoded fields, IASL commands/log hashes, and pointer checks.
Files named `*-recompiled.aml` are separate diagnostics, never replacements for
the recovered originals. The script accepts alternate explicit `--table
NAME=0xADDRESS` arguments; its default address list applies only to this capture.

The script strips ANSI CSI decoration and recognizes GRUB address/hex/ASCII
rows across CRLF/LFCR line endings. It rejects malformed rows, missing bytes,
conflicting overlapping observations, invalid signatures, unbounded lengths,
and failed checksums. It does not infer binary values from decoded prose.
There are 131 rows covering 2,029 distinct observed bytes. Nine synthetic tests
pass. A second run into `b06wj-acpi-runtime-20260908-repeat` reproduced all
eleven original table binaries byte-for-byte.

Tool script SHA-256:
`4640e14a883026f1cbc744c861d68f51a24486bacf186bf74941ab9cc537bdc7`.
IASL binary SHA-256:
`4b1e5067ef40ec0dbbc4f4717e4fcf1596b0a39e195096444d16bbe108a122b9`.
Original output manifest SHA-256:
`3dc14be5887e74ec095810aa9db5d448fc09fab4bc9153e110e078476842bcfc`.

| Capture suffix | Bytes | SHA-256 |
| --- | ---: | --- |
| `rootdump-20260908-01.raw` | 5028 | `61197745c751d62e4ef7efd0a41cad4c5cd36200198a376fe7cfe5dbd0f39ff1` |
| `fadt-20260908-01.raw` | 7347 | `f9f9e15da919b64e4d350373dcb1cc17b88b2a2a867d68efa938c4e18b3bcdee` |
| `tables-20260908-01.raw` | 10622 | `6a9f3f6e217ffdd2e49f2f8a6ab02f4d80f4d4c87abd99b73bb67631d90dade9` |

## Exact runtime table inventory

Addresses below are physical addresses for this observed run, not portable
addresses for another boot. Standard table OEM ID is `COREv4`, table ID
`COREBOOT`. DSDT has OEM revision `0x20260906`, creator `INTL`; other standard
tables have OEM revision zero and creator `CORE`. Creator revision is
`0x20251212`. The manifest and binaries preserve every original header byte.

| Table | Physical address | Length | Revision | Checksum byte | SHA-256 |
| --- | --- | ---: | ---: | --- | --- |
| RSDP | `0x01676000` | 36 | 2 | legacy `14`, extended `34` | `907f80e28e6ae513061f1682963adfc1ce1d594a8386416d167ab80803342344` |
| RSDT | `0x01676030` | 60 | 1 | `e5` | `90ffef325285b17962c32b4a28bfe8dcd640d20ddd48ad0b44a553c6c65254ab` |
| XSDT | `0x016760e0` | 84 | 1 | `c7` | `e743f8c4116a11b02e2cd08508ef202aa519690cbc6e379f4cfc1d60d94efcdb` |
| FACS | `0x01676240` | 64 | version 1 | n/a | `44341977e5b1ae7e1bee5be50d27b4881a24dab6c04801b34945c3e241c68cb9` |
| DSDT | `0x01676280` | 1076 | 2 | `f2` | `63ba8a0d497cc6998bfffd35ea816d7bba5d85f811337ce0bfb21f7c854d7d03` |
| FADT/FACP | `0x016766c0` | 276 | 6 | `1a` | `01d7f57ee17cbd10d30a0733424bef63ae63df3e0d3cbb6809bf63d66ba14393` |
| SSDT | `0x016767e0` | 113 | 2 | `aa` | `ddf9c8e8c8938f0ff49abb0613fe633ea9a0a27ff389b6fb2a460f782df6dd90` |
| MCFG | `0x01676860` | 60 | 1 | `05` | `9c0e34f1de2c6bbf017507ee460c8d4bf4c5be6f0c806787e01f80fc94bcebfe` |
| APIC | `0x016768a0` | 84 | 3 | `d8` | `75cc7f6d4febc8d045bbcd2fee989dddf601d9cd2b72091ac8190349f7804bf1` |
| SPCR | `0x01676900` | 88 | 4 | `94` | `0a464512c737928df0f395def558c0cb687cf26fdcb73767f63ec5ac1d765c44` |
| HPET | `0x01676960` | 56 | 1 | `9c` | `b1d07e290b94e38e70da4ca7e2620f9d648e8b7565c3a4e6224153ac19f350cc` |

FADT contains SCI IRQ 9, flags `0x75`, boot architecture `0x0001`, desktop
profile 1, PM1 event `0x500`/4 bytes, control `0x504`/2 bytes, PM timer
`0x508`/4 bytes, GPE0 `0x520`/16 bytes. Corresponding extended GAS addresses
agree and use SystemIO. SMI command/ACPI enable/disable are zero. C2/C3
latencies are 101/1001. Reset GAS and reset value are zero.

MADT reports one enabled BSP, processor ID/APIC ID 0; I/O APIC ID 1 at
`0xfec00000`, GSI base 0; IRQ 0 to GSI 2 flags `0x0005`; IRQ 9 to GSI 9 flags
`0x000d`; LAPIC at `0xfee00000`, MADT flags 1. MCFG describes segment 0, buses
0–255 at `0xe0000000`. HPET ID is `0x8086a301`, base `0xfed00000`, minimum
tick 128. These are table descriptions, not new hardware-read measurements.

## New defect: CTBL resource general flags set reserved bit 4

Runtime SSDT is a single root `CTBL` device: HID `BOOT0000`, UID zero,
`_STA = 0x0b`. Its only resource is cacheable, read-only memory at
`0x0169a000–0x016a1fff`, length `0x8000`. At SSDT offset `0x55`:

```text
87 17 00 00 1c 02 00 00 00 00 00 a0 69 01 ff 1f
6a 01 00 00 00 00 00 80 00 00 79 00
```

`87` is a DWORD address descriptor; `17 00` is its 23-byte payload length;
resource type is zero (memory). The general-flags byte is **`0x1c` at table
offset `0x59`, physical address `0x01676839`**. This sets bits 4, 3, and 2.
The official ACPI 6.6 DWORD descriptor definition requires bits 7–4 to be
zero. The reserved-bit violation is independent of the valid whole-table
checksum. [ACPI 6.6 §6.4.3.5.2, Table 6.46](https://uefi.org/specs/ACPI/6.6/06_Device_Configuration.html#dword-address-space-descriptor)

Local source path, unchanged relative to base commit
`fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2`:

1. `coreboot/src/acpi/acpi.c:344`: CTBL calls
   `acpigen_resource_consumer_mmio()`.
2. `coreboot/src/acpi/acpigen.c:2404`: the fixed minimum/maximum flags are
   combined with `ADDR_SPACE_GENERAL_FLAG_CONSUMER`.
3. `coreboot/src/include/acpi/acpigen.h:276`: that constant is `0x10`, yielding
   `0x08 | 0x04 | 0x10 = 0x1c`.

The B06WJ release patch has no changes to these three common files. The same
`0x10` value is also visible in the primary upstream main-branch file retrieved
on 8 September 2026; upstream presence does not make the encoding conformant.
[coreboot acpigen.h](https://github.com/coreboot/coreboot/blob/main/src/include/acpi/acpigen.h#L276)

Important nuance: ACPI 6.6 labels DWORD flag bit 0 ignored. Older ACPI 2.0
defines it as consumer/producer, and IASL still uses that interpretation.
Neither specification assigns this meaning to bit 4.
[ACPI 2.0 Table 6-26](https://uefi.org/sites/default/files/resources/ACPI_2.pdf)

IASL decodes the original descriptor as `ResourceProducer`, silently discards
reserved bit 4, and compiles general flags `0x0c`. It also shortens the AML
buffer-length integer and changes the creator header, so its 112-byte output
must not be treated as a byte-minimal corrective patch. A clean IASL result
alone did not detect this original encoding defect.

For a future isolated RAM-only experiment, two separable tests are possible:
remove only the CTBL SSDT from the root lists, or correct only its descriptor
and table checksum. For this **exact** original table, `0x59: 1c→0d` restores
the historical consumer encoding and requires `0x09: aa→b9`; clearing only
the reserved bit (`1c→0c`) instead requires checksum `aa→ba`. None of these
changes has been applied by this analysis. A future run must first reverify
the current table address, exact length/hash, bytes, and checksum; physical
addresses may change across boots. No success claim follows until Windows is
tested with one isolated change. Bugcheck parameters remain needed for precise
Windows attribution.
