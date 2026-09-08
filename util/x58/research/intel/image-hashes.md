> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# Intel desktop-X58 image provenance and hashes

Status: **verified-static**, 2026-08-04.

## DX58SO2 / DX58OG 0920

| Object | Size | SHA-256 |
|---|---:|---|
| Supplied `sox5820j-86a-0920-bi.zip` | 2,871,264 | `d8cb1c8cb2ad0027c85f1a51ee9d436cdaad7f74d50ec17e363fd223863fce74` |
| Archive member `SO0920P.bio` | 3,991,700 | `4e6b895898db07e78d8627ae6fb9c81df8df9ad950f24c1617ad4bb7ac5ce52c` |
| Archive member `SO0920P.itk` | 657,424 | `59b9a4c14ae2b1a5206366fa3229673a44d950601935d83fd017027f5d78d30c` |

The internal BIOS identifier is
`SOX5820J.86A.0920.2013.0729.0042`.  `SOX5820J` is the common Intel
DX58SO2/DX58OG firmware family.  The earlier classification as an original
DX58SO image was wrong; the original DX58SO uses `SOX5810J`.

## Original DX58SO 5600

| Object | Size | SHA-256 |
|---|---:|---|
| Mirrored `SOX5810J.86A.5600.BI.ZIP` | 2,740,806 | `347801ebc6801beb540e2677f7852d00aa786122be8c234ea4577612489970d7` |
| Archive member `SO5600P.bio` | 3,849,620 | `d26337570c7a8efde9cce5002310a4f96583e667f063a691e4d582f55c0f866b` |

The 5600 archive is a public mirror of retired Intel `downloadmirror`
content, not a live Intel download.  Intel's
[INTEL-SA-00019](https://www.intel.com/content/www/us/en/security-center/advisory/intel-sa-00019.html)
provides official product/version provenance; the exact mirror URL and local
path are recorded in [firmware-corpus.json](../firmware-corpus.json).

Both `.BIO` files are update capsules, not raw flash-chip dumps.  They are
Intel-generic and Intel-board-policy references; neither establishes MSI board
policy or a reusable module ABI.  Public availability does not grant
redistribution rights, so the archives, capsules, and extracted modules remain
under ignored `blobs-local/` storage.
