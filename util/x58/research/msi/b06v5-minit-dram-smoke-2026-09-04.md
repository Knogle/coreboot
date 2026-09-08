> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06V5 full MINIT path and UC-DRAM smoke tests — 2026-09-04

Status: **hardware-observed once after a CF9 full reset; vendor-assisted DRAM
access works in this fixed configuration, but AC-cold stability, native
coreboot raminit, ramstage and payload execution remain unproven**.

## Test record

```text
test ID: B06V5-HW-MINIT-DRAM-01
image ID: X58PROE-B06V5-COLD-MINIT-GATE-ROMMON-20260904
build commit: coreboot fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2 plus local experimental X58/LGA1366 changes
ROM hash (intended local W25Q128 16 MiB): e2a24406b3007eae52fcb76653284873e25315047e9faa2100ce14c5763574cf; programmer readback not captured
flash chip: socketed Winbond W25Q128.V..M, 16 MiB
board revision: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 000206c2
CPU stepping: 2
microcode revision: 0000001f
DIMM model: Crucial BLS4G3D1609DS1S00., 4 GiB, dual-rank x8, non-ECC
DIMM slot: sole responder at SPD 0x54; ACKMAP/DDR3MAP 10/10
GPU: unchanged; not exercised
PSU: not re-recorded
boot type: CF9 full-reset continuation followed by the expected CSI-produced reset; not an independently verified AC-cold boot
POST trace: d1/reset; CONFIGURED_AFTER_RESET; d1/d2 around returning CSI; d3/d4 around returning MINIT; interactive bc ROMMON afterward
serial log: local-only blobs-local/msi-x58-pro-e/b06v5/2026-09-04-b06v5-full-live-through-walking-zero.raw, SHA-256 673330d8ce196f9d29582ededa60b126dc4f971aa74d6c0a5ee88513f4ec6db2
result: PASS ONCE for the full MINIT dispatcher path and the listed UC-DRAM smoke/address/walking-one/walking-zero tests
recovery required: none; every modified DRAM dword was restored and independently checked
notes: no SPI write, post-CAR transition, ramstage or payload; one run is not a stability claim
```

The full transcript remains under ignored `blobs-local/` because it contains
vendor-derived diagnostic data. This report records only hashes, interfaces
and independently observed results; it does not include or redistribute the
proprietary modules or their captured buffers.

An earlier private capture ending after the A22--A31 address test has SHA-256
`e83cd9272b5cb9fdb69191e9c32def9e2734490c6fc27563001632b81da092dd`;
a later capture through walking one has SHA-256
`254c8db98a005297fe1055b5e612ecd5c8d9484583b267782ac3faaba15e19b8`.
The test-record hash above is the final superset through walking zero.

## Observed identity defect

Both bootblock executions printed
`X58PROE-B06V4-POLICY-TELEMETRY-20260904`, while the following romstage and
ROMMON identities correctly identified B06V5. Source inspection explains the
discrepancy: the `b06j_build_id` preprocessor chain tests
`CONFIG_X58_PRO_E_B06V4_POLICY_TELEMETRY` before the B06V5 option, and B06V5
inherits/enables the V4 prerequisite. This is an identification-only defect;
the B06V5 cold-policy command, hashes and returned MINIT telemetry prove which
romstage ran. The next image must test the most-specific release option first.

## CSI and cold-policy gates

The first CSI call reset the machine. The second call returned the already
observed register tuple `EAX/EBX/ECX=2/2/0x106`. The complete `0x304`-byte CSI
state was reconstructed without gaps:

```text
CSI state FNV-1a-32: a3736e20
CSI state SHA-256: 0a4b87c2d11daa798dd6ea91842a03a7789a0ac21dd4f5ce44b842b17de4937b
```

It is byte-identical to the B06V4 ratio-6 capture. The reviewed B06V4 policy
candidate had FNV `2e0ccce9`. B06V5's separately armed cold transform changed
only policy byte `0x0a` from `02` to `00` and byte `0x26` from `06` to `02`,
which clears flags bit 18. The complete result was dumped and then installed
with matching readback:

```text
cold policy FNV-1a-32: 3c0f3a0b
cold policy SHA-256: 6de724da5a46e5602e7b7c22d4f190101a22425ae343b3b7ab8c270cabc8ec29
```

## Full MINIT return

The separately armed MINIT call returned normally through the private CAR
stack:

```text
EAX=00000000  WS01=00  WS02=00  POLICY0A=00
WS4F=02       COMPLETE_E79=01
MC_MAP60=00024489  MC_F8=00001545
channel-2 DOD=000002ac  channel-2 rank-present=00000003
```

Static correlation places the `workspace[0xe79]=1` store at the normal
dispatcher tail after phases B4 through B8. The B06V4 run had stopped after
B3 with `workspace[0x4f]=0x06` and left this marker clear. B06V5 instead
returned with bit 2 clear in `workspace[0x4f]`, set the completion marker and
programmed the mapper, memory rules and channel-2 state. This proves that the
full dispatcher path returned for this input; it does not prove that each
algorithm is portable or suitable for native coreboot.

The complete `0x2bcc`-byte workspace was reconstructed from 701 non-duplicate
records covering offsets `0x0000..0x2bcb`:

```text
workspace FNV-1a-32: 6d87f4e4
workspace binary SHA-256: e45f3069048a15772a96f5b339b5471421d7d27f38c30a8bb6b8e44e3b74f8c9
private workspace-transcript SHA-256: 2e094b8ca776048eaca6b736cabdbe12ccbef794d479afa2e34f63c2d6b7029a
```

Only 353 of 11,212 bytes differ from the B06V4 B3-return workspace. Of those,
348 changed from zero to nonzero, one changed to zero and four changed between
nonzero values. The structured additions are consistent with outputs of the
newly reached B4--B8 path.

## Resulting memory state

Read-only PCI observations after MINIT included:

```text
SAD rules: 00000bc3 00000fc0 000013c3 000013c0 ...
TAD channel selectors: 22222222 / 00000000 / 22222222 / 00000000
IOH TOLM:  bc000000
IOH TOHM:  00000001:3c000000
MC mapper: 00024489
MC control:00000400
channel 2: DOD 000002ac, ranks 00000003, status 00000140
QPI status:030f0f03
```

The rules select only channel 2 and describe a low-memory region below the
PCI hole plus a remapped high region. Resource reporting has not yet been
implemented, so these values are observations rather than a coreboot memory
map exposed to ramstage.

`IA32_MTRR_DEF_TYPE=0x800` enabled MTRRs with default type UC. The only active
variable pairs covered the CAR window at `0xfff80000` and the flash window at
`0xfffc0000`; the next pair was disabled. Therefore the ordinary-memory
addresses below were not satisfied by a WB L3/CAR mapping.

## Direct UC-DRAM tests

Reads succeeded at `0x00100000`, `0x01000000`, `0x10000000`, `0x40000000`,
`0x80000000` and `0xb0000000`; the first two were also reread with the same
values. A reversible data-pattern test at `0x01000000` wrote and read back
`00000000`, `ffffffff`, `55aa55aa` and `aa55aa55`, then restored and verified
the original dword `e82c939b`.

A separate simultaneous ten-address test used distinct patterns at addresses
spanning selected address bits A2 through A30:

```text
01000000 01000004 01000008 01000010 01000100
01010000 01100000 01800000 11000000 41000000
```

All ten patterns read back together. All ten original dwords were then
restored and independently read back correctly.

## Transactional A22--A31 test

The ROMMON script engine then exercised one anchor plus ten addresses whose
XOR differences from `0x03000000` select exactly one address bit from A22
through A31:

```text
anchor: 03000000
A22..A31: 03400000 03800000 02000000 01000000 07000000
          0b000000 13000000 23000000 43000000 83000000
```

The sealed 32-operation table wrote 11 distinct patterns, asserted that the
anchor survived after every target mutation, and finally asserted every
target and the anchor. Its hardware result was:

```text
OPS=20 (hex)  PROGRAM_FNV=1447f71f
RUN=ok        MUT=0b (hex)  TXN_FNV=f127d461
ROLLBACK=ok   RB_RESULT=ok  ROLLED_BACK=01
post-rollback TXN_FNV=cd1d74f6
```

Rollback replayed all 11 saved dwords in reverse order and verified each
readback. The transaction was then discarded. Independent ordinary reads
again matched all 11 pre-test values. QPI status remained `030f0f03`, memory
status remained `00001545/00000140`, and all six sampled RAS/ECC counters at
`ff:03.2:80..94` remained zero.

## One-dword D0--D31 walking-one test

Two further sealed tables exercised all 32 data bits at UC address
`0x01000000`. Each table contained 16 reversible write/full-mask-assert pairs:
the first wrote `1 << 0` through `1 << 15`, and the second wrote `1 << 16`
through `1 << 31`. Every assertion observed exactly the single requested bit.

```text
D0--D15:  PROGRAM_FNV=08863aba  RUN=ok  MUT=10 (hex)
          TXN_FNV=39cfb15a
          ROLLBACK=ok  RB_RESULT=ok  ROLLED_BACK=01  post-RB TXN_FNV=6543677b

D16--D31: PROGRAM_FNV=356d86da  RUN=ok  MUT=10 (hex)
          TXN_FNV=6f1cacba
          ROLLBACK=ok  RB_RESULT=ok  ROLLED_BACK=01  post-RB TXN_FNV=97d3ab0b
```

Both transactions were discarded after rollback. A final independent read
returned the original `e82c939b`. QPI remained `030f0f03`, memory status
remained `00001545/00000140`, and the same six RAS/ECC counters remained zero.
This is a 32-bit walking-one test at one address; it is not a multi-address
walking-data or full-memory test.

## One-dword D0--D31 walking-zero test

Two final sealed tables exercised all 32 data bits at the same UC address
`0x01000000`. Each contained 16 reversible write/full-mask-assert pairs. The
first cleared one bit at a time from D0 through D15 against an all-ones
background; the second did the same for D16 through D31. Every assertion
observed the exact 32-bit walking-zero value.

```text
D0--D15:  PROGRAM_FNV=b7db281a  RUN=ok  MUT=10 (hex)
          TXN_FNV=7096d1a8
          ROLLBACK=ok  RB_RESULT=ok  ROLLED_BACK=01  post-RB TXN_FNV=98a98539

D16--D31: PROGRAM_FNV=2b2469ea  RUN=ok  MUT=10 (hex)
          TXN_FNV=450cb168
          ROLLBACK=ok  RB_RESULT=ok  ROLLED_BACK=01  post-RB TXN_FNV=33528d31
```

Both transactions were discarded after rollback. A final independent read
again returned the original `e82c939b`. QPI remained `030f0f03`, memory
status remained `00001545/00000140`, and all six RAS/ECC counters remained
zero. This remains a 32-bit data-pattern test at one address, not a bulk or
multi-address walking-data test.

## Evidence boundary and next validation

This is strong evidence for real, cache-independent DDR reads and writes,
one-dword D0--D31 walking-one and walking-zero behavior, and non-aliasing at
the specifically tested address bits in one vendor-assisted session. It is
not a complete memory test: no multi-address walking-data suite, full 256-MiB
region, long soak, AC-cold repetition, post-CAR transition or ramstage ran.
Native open DDR3/QPI initialization is also not claimed; this experiment used
locally supplied MSI code behind the explicit experimental gate.

Before promoting the result to a stable memory milestone, retain this fixed
CPU/DIMM configuration and require ten AC-cold repetitions plus broader
cache-independent memory testing. The next firmware step should separately
make the currently terminal ROMMON path return, establish a conservative
CBMEM/post-CAR stack below known reserved memory, preserve the vendor outputs
needed after CAR teardown, and enter a minimal diagnostic ramstage.
