> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06V4 direct MINIT return and phase-B3 boundary

> **Later RTC-bank correction:** the MINIT/phase-B3 and policy-buffer results
> remain empirical facts. The source bytes formerly labeled extended CMOS were
> read while ICH10 U128E was clear and were aliased standard RTC fields; that
> label and any claimed retention semantics are invalid.

Status: **hardware-observed and statically correlated; DRAM remains
uninitialized and untested**.

## Fixed hardware configuration

```text
image: X58PROE-B06V4-POLICY-TELEMETRY-20260904
W25Q128 image SHA-256: c2cf7eed56db35916515fd7c759201730169ca9d839db42de25b8adf6736127a
board: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 000206c2, microcode 0000001f
DIMM: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
SPD topology: sole responder 0x54, ACKMAP/DDR3MAP 10/10, FNV fb66b530
QPI/memory ratio observation before call: 030f0f03 / ratio 6
UART: COM1 0x3f8, 115200 8N1
```

This is one hardware sequence, not a stability claim. No ordinary DRAM read or
write was issued by coreboot before or after the vendor calls.

## Selector-controlled policy inputs

B06V4 corrected two earlier selector-confused observations. The reliable
`vinputs` result was:

```text
CMOS_DIAG_0E=6c VALID=00
EXT81/82/88/89=19/f7/6f/cb
EXT8E=6c EFFECTIVE8E=30
EXTCA/F1/F5=62/91/e9
ALT_GP_SMI_EN_LOW=00 FIELD_6_4=00
CMOS_DATA_WRITES=00
```

Therefore standard CMOS `0x0e` and extended CMOS `0x8e` both happened to be
`0x6c`; they are still separate registers on separate index/data interfaces.
Extended `0xca=0x62` is another independent input. Documentation that calls
`0x62` the standard diagnostic byte is superseded by this capture.

## Returned CSI and reviewed policy candidate

The MSI CSI path again followed the two-pass shape: the first call reset the
machine; the configured-I801 continuation reached ROMMON; the second call
returned:

```text
EAX/EBX/ECX = 00000002/00000002/00000106
CSI state FNV = a3736e20
```

The tuple matches the previous B06V3 return. The state digest differs from
`c4eab5a4` by one captured byte, so the complete state was dumped and the new
digest was accepted rather than assumed equivalent.

Starting from generated policy FNV `36981039`, seven neutral, experimentally
reviewed offsets were changed to reproduce the one-DIMM candidate used for
the MINIT call:

```text
04=01  08=01  24=80  25=04  bc=85  d9=0a  db=05
candidate policy FNV = 2e0ccce9
policy[0a] = 02
little-endian policy[24..27] = 01060480
```

No semantic field names are assigned to these offsets. The complete
`0xe0`-byte policy was dumped, reconstructed without gaps, and verified at FNV
`2e0ccce9` before installation. The retained runtime copy matched that digest.

## Direct MINIT result

The separately armed direct call reached POST `d3`, returned through the
private CAR stack to POST `d4`, and reported:

```text
EAX=00000000 EBX=fff83cc8 ECX=000000bf EDX=00000080
EDI=fff83ce4 EFLAGS=00000096 VESP=fff8ffe8
workspace[01]=00 workspace[02]=00 policy[0a] after call=00
input policy FNV=2e0ccce9
current policy FNV=9fbe08df
workspace FNV=267b2858
```

The old B06V0 four-gate check labeled this a candidate. That label is now
known to be insufficient.

The complete workspace transcript contains all `0x2bcc` bytes with no gaps or
duplicates. Independent reconstruction produced:

```text
workspace FNV-1a-32: 267b2858
workspace binary SHA-256: 63f26e92dcd55675731d00a94b5ea0718a421b94010313bbbf2b065c47b0817d
workspace[0x4f]: 06
workspace[0xe79]: 00
```

The proprietary full-workspace transcript remains under `blobs-local/` and
is not redistributable. Its transcript SHA-256 is
`72b7dd3d71cf456a7b077cf5ab054da8622a088d23ac89ac941e131b23471a79`.

## Static control-flow correlation

Static analysis of the pinned MSI `MINITDLL` shows the main dispatcher calling
internal phases B0, B1, B2, and B3. B3 begins at `0xfffc8781`. At the caller
around `0xfffd8bf5`, a nonzero B3 result branches around phases B4 through B8.
The normal full-path end writes byte value 1 at workspace offset `0xe79`.

B3 returns 1 when either workspace byte 2 is nonzero or bit 2 of workspace
byte `0x4f` is set. The hardware workspace has byte 2 clear but
`workspace[0x4f]=0x06`, so bit 2 explains the exact early return. The policy
builder's status-derived bit 18 is copied to this workspace flag.

This correlation changes the interpretation of `EAX=0`: the outer entry point
returned normally, but the memory-init pipeline did not execute phases B4-B8.

## SPD decode versus hardware programming

MINIT did perform useful work before the B3 return. Its workspace records only
channel 2 as populated and decodes the SPD geometry as dual-rank x8, including
a channel-2 DIMM-organization value `0x02ac`. This independently confirms the
SMBus address, SPD bytes, and topology passed into the vendor code.

It did not make the corresponding hardware configuration usable. The
post-call snapshots show, among other values:

```text
FF:03.0 MC mapper 0x60 = 00000000
FF:03.4 completion/status 0xf8 = 00000000
channel 0/1/2 rank-present 0x7c = 00000000
channel mapper/TAD/DOD programming remains absent or at pre-training defaults
```

The public read-only captures are:

```text
171feb0227a7168a472b9e5661d628a4826d6d13d5999d1bf9e71b2de29dffb5  2026-09-04-b06v4-post-minit-uncore.raw
2063783db04922b55cbddbcc64d2487b77ddd0d0ac1ea4cad9a3fec277b2dc3b  2026-09-04-b06v4-post-minit-channels.raw
1b919ecfd7f1fd9a2ace492f1e8c561d76c7a382f3e1f283b56ad27e170cfe27  2026-09-04-b06v4-post-minit-smbus.raw
```

## Next isolated hypothesis

B06V5 keeps the exact returned CSI tuple and requires the full previously
returning policy digest `2e0ccce9`. Its separately armed `vpolicy cold` command
changes only:

```text
policy[0x0a]: 02 -> 00
policy flags bit 18: 01060480 -> 01020480
resulting complete policy FNV: 3c0f3a0b
```

The command rejects every other input digest, flag word, policy status, or CSI
tuple. After a returning MINIT call B06V5 reports `workspace[0x4f]`, the
`workspace[0xe79]` completion marker, and selected Uncore registers. The
strongest possible result from this image is only
`FULL_PATH_RETURN_DRAM_UNTESTED`; actual DRAM access and training validation
remain a later, separately reviewed step.
