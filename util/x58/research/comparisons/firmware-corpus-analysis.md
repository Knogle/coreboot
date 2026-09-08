> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# X58/5500/5520 firmware corpus analysis

Date: 2026-08-04.  Status: **static analysis only**.  No image in this corpus
was flashed or executed by this project.

## Corpus and provenance

The local private corpus now contains eleven firmware releases.  Exact archive
and payload sizes, SHA-256 values, source URLs, dates with explicit meanings,
and repository-handling notes are machine-readable in
[firmware-corpus.json](../firmware-corpus.json).  These handling notes are not
legal conclusions about vendor license terms.

| Family | Releases | Why it is useful | Source quality |
|---|---|---|---|
| MSI MS-7522 / target X58 Pro-E | A7522IMS.8F0 | exact compared-EEPROM bootblock and policy evidence | user-supplied MSI-branded package labeled X58 Pro SLI; user reports equality with target EEPROM |
| Intel DX58SO2/DX58OG | 0920 (`SOX5820J`) | late desktop Intel X58 PEI reference | user-supplied archive; internal ID verified |
| Intel DX58SO | 5600 (`SOX5810J`) | true original-DX58SO diff partner | public mirror of retired Intel download content |
| Intel S55xx | R0033, R0069 | early/late 5500/5520 Uncore, QPI, and MRC endpoints | public mirrors; Intel pages corroborate product/version, not mirror hashes |
| ASUS Rampage III Formula | 0402, 0701, 0702, 0903 | MSI-near AMIBIOS8 revision diff | active official ASUS CDN |
| Apple Mac Pro | EFI 1.4 (4,1), EFI 1.5 (5,1) | independent Nehalem/Westmere OEM PEI line | active official Apple CDN |

The ASUS releases and notes are available on the
[official support page](https://www.asus.com/us/supportonly/rampage%20iii%20formula/helpdesk_bios/).
The Apple packages came from the official
[MacPro4,1 EFI 1.4](https://support.apple.com/en-euro/106656) and
[MacPro5,1 EFI 1.5](https://support.apple.com/en-us/106455) pages.  Intel no
longer serves the tested S55xx packages directly; official product/version
provenance remains in
[INTEL-SA-00026](https://www.intel.com/content/www/us/en/security-center/advisory/intel-sa-00026.html),
[INTEL-SA-00038](https://www.intel.com/content/www/us/en/security-center/advisory/intel-sa-00038.html),
and
[INTEL-SA-00045](https://www.intel.com/content/www/us/en/security-center/advisory/intel-sa-00045.html).
The manifest explicitly distinguishes those official references from the
third-party archive mirrors that supplied the bytes.

Embedded release metadata identifies S55xx R0033 as QPI RC 1.03 / MRC
1.05RC1 and R0069 as QPI RC 1.85 / MRC 2.21.  These are unusually useful
endpoints for version-aligned semantic diffing.

## PI/UEFI structural matrix

Reports were generated with pinned UEFIExtract NE alpha 76 and parsed by
`scripts/uefi_report_matrix.py`.  Counts describe parsed report nodes, not
source-code modules, and duplicate update branches remain visible.
Input/report/tool hashes and diagnostic summaries are recorded in
[uefi-report-provenance.json](../uefi-report-provenance.json); exact diagnostic
streams remain in ignored private sidecars.

| Firmware | FV nodes | Unique file GUIDs | PE32 sections | TE sections |
|---|---:|---:|---:|---:|
| DX58SO2/DX58OG 0920 | 17 | 196 | 335 | 38 |
| DX58SO 5600 | 17 | 193 | 325 | 42 |
| S55xx R0033 | 9 | 197 | 171 | 1 |
| S55xx R0069 | 10 | 208 | 176 | 1 |
| MacPro4,1 EFI 1.4 | 9 | 220 | 191 | 1 |
| MacPro5,1 EFI 1.5 | 10 | 234 | 141 | 60 |

The most informative same-family comparisons are:

| Pair | Shared unique GUIDs | Left-only | Right-only | Jaccard |
|---|---:|---:|---:|---:|
| DX58SO2/DX58OG 0920 ↔ DX58SO 5600 | 191 | 5 | 2 | 0.964646 |
| S55xx R0033 ↔ R0069 | 197 | 0 | 11 | 0.947115 |
| MacPro4,1 ↔ MacPro5,1 | 216 | 4 | 18 | 0.907563 |

Cross-vendor overlap is still substantial: DX58SO2/DX58OG 0920 shares 81
unique file GUIDs with S55xx R0033 and 67 with MacPro4,1; S55xx R0033 shares
61 with MacPro4,1.  A shared GUID is a structural anchor, not proof that file
bodies or policies are identical.

## Main finding: two independent Uncore GUID lines

Intel GUID `63C0690C-5D9E-4EE3-840E-FAE0E76E291A` occurs in DX58SO,
DX58SO2/DX58OG, and both S55xx endpoints.  The server firmware supplies UI and
PDB names `UnCoreInitPlatform`, converting the earlier broad role inference
into verified static identification.  Sizes and canonical hashes are recorded
in [pei-candidates.md](../intel/pei-candidates.md).

Apple instead uses GUID `D71C8BA4-4AF2-4D0D-B1BA-F2409F0C20D3`.  The images
contain no UI name for that file; the pinned UEFITool GUID database supplies
the external label `UncoreInitPeim`.  Its PE32-to-TE evolution from MacPro4,1
to MacPro5,1 is an independent Nehalem/Westmere diff line, not evidence of
module identity with Intel GUID `63C0690C...`.

Both lines share long exact executable regions with MSI `MINITDLL`.  The
strongest matches are 888 bytes for both S55xx versions and 890 bytes for both
Apple versions, compared with 404 bytes for each Intel desktop image.  See
[shared-sequences.md](shared-sequences.md) for offsets and limits on the
interpretation.

## ASUS revision result

All four Rampage III Formula bootblocks contain `MINITDLL` and `CSI_INITDLL`.
`CSI_INITDLL` is byte-identical in 0402/0701/0702/0903.  `MINITDLL` changes in
0402→0701 and 0701→0702, then is byte-identical in 0702 and 0903.

The 0701→0702 change contains a compact branch change at preferred address
`0xfffce122`: a private bit previously gated by a `0x2c00` value becomes
unconditional.  Because ASUS labels 0702 “Memory Recheck”, this is a strong
candidate feature anchor, while the private field meanings remain unknown.
The 0903 “memory compatibility” changes must be outside the two embedded init
PE bodies, so the next diff should focus on bootblock callers, SLAB policy, and
tables rather than re-disassembling unchanged DLLs.

## Reproduction

Verify whichever private inputs are present without downloading or modifying
them:

```bash
python3 scripts/verify_firmware_corpus.py --allow-missing
```

Generate local UEFI reports with a private provenance/diagnostic sidecar, then
compare their structures:

```bash
python3 scripts/run_uefi_report.py /path/to/UEFIExtract \
  blobs-local/intel-dx58so/SO0920P.bio \
  --tool-commit dac91b26733ca21cb204e41614c1b8c81cc50860

python3 scripts/uefi_report_matrix.py \
  dx58so2og=blobs-local/intel-dx58so/SO0920P.bio.report.txt \
  dx58so=blobs-local/intel-dx58so/5600/extracted/SO5600P.bio.report.txt \
  s55xx_r0033=blobs-local/intel-s55xx/r0033/extracted/R0033.cap.report.txt \
  s55xx_r0069=blobs-local/intel-s55xx/r0069/extracted/S55xx_BIOS69_EFI_BIOS_Only/R0069.cap.report.txt \
  macpro41=blobs-local/apple-macpro/efi-1.4/MP41_0081_07B_LOCKED.fd.report.txt \
  macpro51=blobs-local/apple-macpro/efi-1.5/MP51_007F_03B_LOCKED.fd.report.txt \
  --candidate 63C0690C-5D9E-4EE3-840E-FAE0E76E291A \
  --candidate D71C8BA4-4AF2-4D0D-B1BA-F2409F0C20D3
```

Run `scripts/pe_inventory.py` on locally extracted PE/TE bodies before using
their canonical hashes for duplicate grouping.  Compare executable sections
with `scripts/find_shared_sequences.py --seed-size 64`; inspect every reported
anchor in a disassembler before assigning semantics.

## Legal and engineering boundary

Nothing under `blobs-local/` or any extracted PE/TE body is repository
content.  A public download URL does not grant redistribution or transplant
rights.  The checked-in deliverables are hashes, source metadata, parsers,
diff tools, and independently written observations.  Matching code ancestry
does not make any vendor module a documented, relocatable, or board-neutral
MRC interface.
