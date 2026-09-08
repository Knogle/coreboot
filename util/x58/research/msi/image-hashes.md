> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# MSI image provenance and hashes

Status: **verified-static** plus user-verified earlier provenance and a direct
read from the running target, updated 2026-08-04.

| Object | Size | SHA-256 |
|---|---:|---|
| Supplied `7522v8F.zip` | 1,338,294 | `83b7cc523ed79ab1b86b810261d33f7bb3a27575b7479b9fbe26ad2a037adf7b` |
| Archive member `7522v8F/A7522IMS.8F0` | 4,194,304 | `ee7912da81a3cf47b6bedef7dedfa56cbc31723f5cdd7f0d75a7f64e336c04a8` |
| Running target, AMI `V8.14B8`, two identical internal reads | 4,194,304 | `75232f915d44f2180d35c6b293d21694fae8c217c8f54bbd84a9a7c8819a2db1` |

The included release note identifies this as AMI BIOS 8.15, build/date
2011-03-19, for MS-7522.  The ROM itself contains `AMIBIOSC0800` and an
`AMIEBBLK` bootblock.

The bundled note labels the package `X58 Pro SLI (MS-7522)`.  The user reports
that `A7522IMS.8F0` was compared with the target X58 Pro-E's complete EEPROM
contents and is byte-identical.  It is therefore treated as the full 4 MiB chip
image for that compared board.  The provenance available here is a supplied
MSI-branded update archive, not a documented live MSI download URL.

The directly read 2026-08-04 target is not whole-image-identical to `8F0` and
reports AMI `V8.14B8`.  The earlier comparison must therefore refer to a
different firmware state or comparison instance and is not used as evidence
that the current live flash equals the archive member.  Two flashrom reads of
the current Winbond W25Q32.V were byte-identical.  Its `MINITDLL` headers and
data differ from `8F0`, while both 94,749-byte `.text` sections are exactly
equal at SHA-256
`c93135af56b85c28bb6f6da6589a2f5a7a35c6a79407b21745d369bc1aa292f4`.
The live proprietary image remains ignored under `blobs-local/`.

Static inspection finds no Intel Flash Descriptor signature and no identified
ME (`$FPT`/`$MN2`) or GbE region.  The reset vector is present at raw offset
`0x3ffff0`.  For the compared EEPROM, this establishes a descriptorless 4 MiB
legacy layout rather than an IFD image with omitted regions.

The live read identifies the chip as W25Q32.V through flashrom but does not
measure package voltage and does not replace an external write/verify/recovery
test.  Neither image should be generalized to every MS-7522 revision without
comparison.
