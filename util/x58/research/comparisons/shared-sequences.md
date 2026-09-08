> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# Cross-firmware shared-sequence comparison

Status: **verified-static** for byte matches; semantic attribution is marked
separately.  Comparisons use executable sections extracted locally and
`scripts/find_shared_sequences.py --seed-size 64`.

The exact executable-section inputs used for the offsets below are bound by
size and SHA-256:

| Input | Size | SHA-256 |
|---|---:|---|
| MSI `MINITDLL` `.text` | 94,752 | `9e4ffbb6d43e3536cfaa6a14db1f7a9705b189e840894124b0a15cd644e2a59b` |
| DX58SO2/DX58OG 0920 `63C...` `.text` | 310,656 | `c14ef99e107c9bb9c9061d99f7c4b523543cbea8d074b28238f9d4e2906ec753` |
| DX58SO 5600 `63C...` `.text` | 302,592 | `b88d9749b8aee5bab7191d5ab0ddd8e944f1f8beeee49f73cb9a657b492ac635` |
| S55xx R0033 `63C...` `.text` | 146,880 | `02bcceccbaca8a102c5e2ece82983bd94ae3491cf766eda1004868c3c6f70357` |
| S55xx R0069 `63C...` `.text` | 160,096 | `d7056bb95e5c48df91dc0524904372bd400cfccc79989c1ba774d6b817f67488` |
| MacPro4,1 `D71C...` `.text` | 127,392 | `0c71037eafa91b283f89a05d16a475147a55a0ce6df67503db84f5d8a1641a9d` |
| MacPro5,1 `D71C...` `.text` | 145,376 | `11606194a9f63354a5b575613f0622dfd685f4a120e2d465c8db2cf8f77a79eb` |

## MSI `MINITDLL` versus Intel/Apple Uncore PEIMs

| Comparison target | Longest exact match | MSI `.text` offset | Target `.text` offset |
|---|---:|---:|---:|
| Intel DX58SO2/DX58OG 0920 `63C0690C...` | 404 bytes | `0x0abc` | `0x101db` |
| Intel DX58SO 5600 `63C0690C...` | 404 bytes | `0x0abc` | `0x0eb7b` |
| Intel S55xx R0033 `UnCoreInitPlatform` | 888 bytes | `0x0a76` | `0x08196` |
| Intel S55xx R0069 `UnCoreInitPlatform` | 888 bytes | `0x0a76` | `0x08bf1` |
| Apple MacPro4,1 GUID `D71C8BA4...` | 890 bytes | `0x0a74` | `0x05869` |
| Apple MacPro5,1 GUID `D71C8BA4...` | 890 bytes | `0x0a74` | `0x0661e` |

The corresponding MSI region contains `movntdq`, memory-pattern evolution,
and verification code.  The larger server/Apple matches are strong evidence
of common memory-initialization source ancestry; Intel authorship remains an
inference from the surrounding corpus.  They do not establish identical outer control flow, policy
structures, calling conventions, electrical assumptions, or board suitability.

The two 404/381-byte desktop matches reported in the original baseline remain
valid.  MSI `CSI_INITDLL` and the late Intel desktop candidate also share a
61-byte library-like fragment; that fragment is too short to support any claim
of a portable common CSI ABI.

## Apple independent implementation line

Apple MacPro4,1 and MacPro5,1 contain PEIM GUID
`D71C8BA4-4AF2-4D0D-B1BA-F2409F0C20D3` twice each.  The images contain no UI
name for the file; the pinned UEFITool GUID database supplies the external
label `UncoreInitPeim`.  The MacPro4,1 body is PE32; MacPro5,1 uses TE.
Relocation-normalized hashes are:

| Firmware | Body size | Canonical SHA-256 |
|---|---:|---|
| MacPro4,1 EFI 1.4 | 143,456 | `2ed37447a73d531b0594fdbeba309e3acad1e5c6a09f0a8d959e262ffb55cb87` |
| MacPro5,1 EFI 1.5 | 159,248 | `eeee1a762fa1596c5b97a2858bad0c6dd6b37d9373d934e858f136474bc6596c` |

The name is corroborating metadata, not proof that every routine concerns RAM
or QPI.  Function-level naming still requires access/disassembly evidence.

## ASUS Rampage III Formula revision diff

The ASUS AMIBIOS8 bootblocks contain the same `MINITDLL`/`CSI_INITDLL` naming
and PE layout as the MSI family, making them a structurally close comparison.

| Release | `MINITDLL` raw SHA-256 | `CSI_INITDLL` raw SHA-256 |
|---|---|---|
| 0402 | `82eb20df6d8d09e7ff9370e4808f7c14a5f51d1776460f873c1ef63b53381092` | `b0f97e3f8ae49a01bc1d5f91fd0afe2e2dba97965d4d55c6bf94641bacc8e648` |
| 0701 | `626302ced5ff741ee83d1d9486446a8b4d8863952b3d1febe435aa0bb89b31a5` | same |
| 0702 | `9ca9a753c8f19a3e77afe20a633befa31b6f119142400dd7c6edf74be66cbf45` | same |
| 0903 | same as 0702 | same |

ASUS describes 0702 as adding “Memory Recheck”.  At preferred address
`0xfffce122`, 0701 sets bit 2 of private byte `[ebx+0xeb0]` only when private
word `[edi+0x1f]` equals `0x2c00`; 0702 sets that bit unconditionally and
removes the `0x2c00` branch.  This is a high-value **inference** anchor for the
feature, but the two private fields are not yet named.

ASUS describes 0903 as improving memory compatibility, yet its two embedded
init PE bodies are byte-identical to 0702.  The corresponding change therefore
resides outside these bodies: likely caller policy, tables, or another module.
That sharply narrows the next revision diff.

## Practical use

Long matches locate candidate routines in Ghidra; revision diffs locate policy
branches.  A sequence should be called an Intel-generic primitive only after
its hardware accesses or externally visible behavior agree.  Exact bytes and
canonical hashes are analysis anchors, never evidence of a reusable ABI.
