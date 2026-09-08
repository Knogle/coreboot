> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# Intel X58/5520 early-init candidates

Status: **verified-static** unless explicitly marked.

## Cross-platform candidate `63C0690C...`

FFS GUID `63C0690C-5D9E-4EE3-840E-FAE0E76E291A` occurs in all four inspected
Intel desktop/server firmware lines:

| Firmware | Copies | Extracted PE size | Relocation-normalized SHA-256 | Static name evidence |
|---|---:|---:|---|---|
| DX58SO2/DX58OG 0920 | 4 | 312,000 | `80c78bfafc720154f57df830dff4257c3c92dc52e7b4d865371f9a3fb26d3ffe` | no UI/PDB name |
| DX58SO 5600 | 4 | 304,032 | `f43339c7d0a936868eb0ea4a51366ce10f31ca2759e8e2dd3c162475b76bb4b4` | no UI/PDB name |
| S55xx R0033 | 2 | 167,712 | `d50af7f84b41a8d83a9d8f4c1a665bc821cf5bf23515749d00dd3d505319b1d0` | UI `UnCoreInitPlatform`; PDB `UnCoreInitPlatform.pdb` |
| S55xx R0069 | 2 | 178,656 | `8f85783d4b27d00364ed1984184901333a19e814383764f0b63542d9b2fd6a24` | UI `UnCoreInitPlatform`; PDB `UnCoreInitPlatform.pdb` |

The repeated PE bodies are not raw-byte-identical: their preferred bases and
relocated addresses differ.  `scripts/pe_inventory.py` removes the preferred
base, timestamp/checksum, and supported relocation effects before computing
the canonical hash.  Every copy within one firmware release then agrees.  The
canonical hash is only a duplicate-detection aid, not an ABI claim.

The server UI sections and PDB paths make the role name
`UnCoreInitPlatform` **verified-static**.  Exactly how much DDR3 PHY training,
QPI setup, address-map construction, and board policy lives inside it remains
to be established function by function.

## DX58SO2/DX58OG 0920 behavior

The late desktop module:

- is a PE32/i386 EFI boot-service image;
- contains direct PCI-CF8/CFC, CPUID, RDMSR/WRMSR, port-I/O, memory-pattern,
  SMBus/variable, and POST-code code;
- emits POST code `0x17` on an entry path;
- references `PlatformInfo`, `MemoryConfig`, `MemoryConfig2`, `MemorySetup`,
  and `MemoryMap` variables;
- shares exact code blocks with MSI `MINITDLL`, the Intel S55xx PEIM, and the
  Apple Uncore PEIM (see
  [shared-sequences.md](../comparisons/shared-sequences.md)).

Its dependency expression requires all of the following before dispatch:

1. PEI base-memory-test PPI;
2. platform-memory-size PPI;
3. PEI read-only-variable PPI;
4. PEI CPU-I/O PPI;
5. PEI SMBus PPI;
6. PPI `159e7d1b-51cc-4cad-81ea-6b7d2ed39dc0`, whose meaning is not yet
   established.

The entry point consumes a PEI-services environment and has a substantial
private context.  It is therefore a platform PEIM/reference oracle, not an
isolated `mrc.bin`, a documented FSP API, or a normal coreboot blob candidate.
Rehosting it would require a meaningful PEI compatibility environment and
would still leave MSI-specific policy unresolved.

Other identified desktop PEIMs include:

| GUID prefix | Role |
|---|---|
| `0A5EA2E1` | PlatformInit |
| `27A5159D` | PCAT single-segment PCI configuration |
| `34C8C28F` | PEI variable services |
| `4BB346D2` | Monolithic status code |
| `643DF777` | PCH SMBus, ARP disabled |
| `8A78B107` | PEI CPU I/O |
| `86D70125` | DXE IPL |
| `8BCEDDD7` | S3 resume PEIM |
| `B4E0CDFC` | SMM access PEIM |
| `C779F6D8` | Capsule PEIM |

`PpisNeededByDxeCore.efi` has generic SPD failure/status strings, but its PE
type and surrounding code do not make it the memory-init implementation.
