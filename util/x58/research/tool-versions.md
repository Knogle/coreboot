> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# Static-analysis tool record

Recorded 2026-08-04.  Firmware and extracted artifacts remain local under
ignored `blobs-local/` or temporary storage and are not repository content.

| Tool | Version/revision | Use |
|---|---|---|
| Python | 3.14.6 | inventory, canonicalization, comparison, corpus verification, tests |
| 7-Zip | 26.02 | archive/capsule and Apple-package extraction |
| GNU objdump | binutils 2.46.1 | PE headers and i386 disassembly |
| Rizin | 0.7.4 | cross-references and early control-flow inspection |
| binwalk | 2.3.4-17.fc44 | signature/microcode survey |
| `bios_extract` | Git `0a7bc1d71735ef97b00dfec0fd54a02fcc5d1bb0` | AMIBIOS8 module extraction and SLAB decomposition |
| UEFITool/UEFIExtract | Git `dac91b26733ca21cb204e41614c1b8c81cc50860`; UEFIExtract NE alpha 76 | Intel/Apple capsule/FV/FFS extraction and reports; GUID-name database |
| upstream coreboot | local Git `78d314b7c3abd383c661f4f9d1533eb0fb4e6031`; previously fetched `origin/main` `c6c871909a1200f83ed5e324e6c1ad0f8c1591f9` | reuse/gap audit and current `inteltool` capability check |
| Linux `i7core_edac.c` | upstream `master` retrieved 2026-08-04; file SHA-256 `459a2dbeae81c35bbd9d9aba9eb95aa80a438d9a61e57572a96b67007acd75bc` | Bloomfield/Westmere PCI-ID and documented-register basis for the read-only snapshot tool |
| Intel Xeon 5500 datasheet Volume 2 | document 321322; PDF SHA-256 `3e0691020033944375986aff6159a18ce9d14f4b0338b4256d3f03eb159e4342` | CPU Uncore device/function and register/field map |
| Intel Xeon 5600 datasheet Volume 2 | supplement document 323370; PDF SHA-256 `3a0748c6617ddee45a5faf0661d81db59357940ddeb0258054f8fc14d86c1cb8` | Westmere-EP register additions; explicitly supplements document 321322 |
| Intel X58 datasheet | document 320838; PDF SHA-256 `8ba958f3a77685577bed0a66563adb97248babfad87aa243b109070cced1d2ff` | IOH and IOH-side QPI register map |
| flashrom on target | v0.9.9-r1955 | two read-only W25Q32.V captures; no flash-content write |
| GCC | 16.1.1 | building host-side extraction tools |
| CMake | 4.3.0 | building UEFIExtract |

## Checked-in analysis tools

| Script | Contract |
|---|---|
| `firmware_inventory.py` | non-extracting raw-image hash, microcode, FV, and embedded-PE survey |
| `verify_firmware_corpus.py` | verifies local archive/payload size and SHA-256 against `firmware-corpus.json`; never downloads |
| `uefi_report_matrix.py` | parses pinned UEFIExtract text reports and compares only real `File`-row GUIDs |
| `run_uefi_report.py` | runs UEFIExtract while preserving diagnostics and hash-binding the tool, input, and report in a private JSON sidecar |
| `pe_inventory.py` | inventories PE32/PE32+/TE bodies and computes relocation-normalized canonical hashes |
| `find_shared_sequences.py` | finds maximal, byte-verified, nontrivial exact matches with bounded repeated seeds |
| `dump_nehalem_uncore.py` | read-only sysfs snapshot of enumerated Bloomfield/Gulftown Uncore PCI functions |
| `compare_uncore_dumps.py` | dword-level diff of two Uncore JSON snapshots |
| `analyze_minit_pci.py` | recovers literal BDF/register arguments at the pinned MSI `MINITDLL` MMCONFIG helpers; dynamic operands remain unresolved |
| `analyze_csi_pci.py` | recovers hash-bound MSI `CSI_INITDLL` MMCONFIG calls with conservative straight-line literal propagation; dynamic buses/values remain explicit |
| `read_mmconfig.py` | read-only access to an explicit BDF through an already configured PCIEXBAR; never scans |
| `probe_x58_hidden_qpi.py` | masked, restoring `DEVHIDE1` experiment for documented IOH QPI devices 16/17 |
| `read_spd_i2c.py` | SMBus read-byte-data SPD capture and conservative DDR3 decode; no EEPROM data writes |

The canonical PE/TE hash clears timestamp/checksum/preferred-base metadata and
normalizes supported HIGHLOW/DIR64 relocations.  Unsupported relocation types
are reported rather than silently ignored.  Equal canonical hashes group
rebased copies; they do not prove compatible entry parameters or policies.

`uefi_report_matrix.py` intentionally counts GUIDs only on UEFIExtract `File`
rows.  Counting every GUID-looking token would incorrectly mix firmware-volume,
section-definition, and file identities.  `run_uefi_report.py` preserves the
exact UEFIExtract diagnostics in a private sidecar; their checked-in hash/count
summary is [uefi-report-provenance.json](uefi-report-provenance.json).  A
successful parse does not establish a DEPEX meaning or module role.

## Reproduction outline

1. Verify available private inputs with
   `python3 scripts/verify_firmware_corpus.py --allow-missing`.
2. Run `scripts/firmware_inventory.py` over raw/member images.
3. Run `bios_extract` on AMIBIOS8 images and `ami_slab` on extracted SLAB
   bodies.  Keep every derivative under ignored local storage.
4. Run `scripts/run_uefi_report.py UEFIEXTRACT IMAGE --tool-commit REV` for
   Intel/Apple inputs, retain its private sidecar, and compare reports with
   `scripts/uefi_report_matrix.py`.
5. Extract only candidate GUID bodies locally and inventory them with
   `scripts/pe_inventory.py` before comparing copies.
6. Use PE optional-header preferred bases for bootblock disassembly; verify
   direct callers against raw bootblock addresses before assigning semantics.
7. Compare extracted executable sections with
   `scripts/find_shared_sequences.py --seed-size 64`; inspect instructions and
   hardware accesses before giving a match a functional name.
8. On real target hardware, follow
   [vendor-state-capture.md](../Documentation/vendor-state-capture.md); do not
   substitute unsupported `inteltool -S` output for X58 Uncore state.
9. Run `python3 -m unittest discover -s tests -v` after tool changes.

The code-to-register correlation and official-document hashes are summarized
in [uncore-register-correlation.md](comparisons/uncore-register-correlation.md).
The hardware values and live-code identity proof are recorded in
[live-uncore-2026-08-04.md](msi/live-uncore-2026-08-04.md).

Tool output alone does not establish module purpose.  Checked-in CSV/JSON and
Markdown record the interpretation, provenance, and evidence level.
