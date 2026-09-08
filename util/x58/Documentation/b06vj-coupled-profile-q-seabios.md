> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VJ coupled profile-Q SeaBIOS experiment

Status: **BUILT REPEATEDLY / BYTE-REPRODUCIBLE / STATIC CONTRACT VERIFIED /
LOCAL COMPOSITE VERIFIED / ONE HARDWARE EXECUTION / SAFE POST-MINIT CAR
FALLBACK / NO DRAM-BACKED STAGE OR PAYLOAD**.

B06VJ is the narrow successor to B06VI. It preserves the exact PRIMARY
A/B/P/N rows and the original profile O, then adds the complete
`B06VI-HW-G3-02` result as one new indivisible profile, `Q` (also called O2 in
the analysis notes). It does not broaden a mask, canonicalization range, CPU
endpoint, or CSI allowlist.

## Hypothesis

The controlled-G3 B06VI run reproduced the exact O-family CPU, CSI, policy,
QPI, IOH, and memory-controller endpoint and returned successfully from
MINIT. Its complete 0x2bcc-byte workspace was internally complete but differed
from the only O workspace that B06VI admitted. B06VJ tests the smallest useful
hypothesis: if that *entire same result* recurs, the already bounded UC memory,
CBMEM, postcar, ramstage, and reduced SeaBIOS path may be attempted.

The user's instruction to treat RAM as provisionally stable remains a
bring-up assumption. It is not final DDR3 training or stability proof.

## Exact profile Q

Profile Q is wire value 6. Every value below belongs to the same row:

```text
saved pre-CSI CPU A0:        00017c00
post-CSI/pre-MINIT CPU A0:   00017c00
post-MINIT CPU A0:           00017c00
post-CSI/post-MINIT CPU 9c:  00a00502
CSI byte 2a6:                0c
CSI raw FNV-1a:              8b38506a
CSI canonical FNV-1a:        908dabb6
workspace raw FNV-1a:        69b4c386
workspace canonical FNV-1a:  0b161f01
workspace full SHA-256:       6c29393a9bdde219319a4f15bf724c005843cc53e3ade6671568c75ceaaeb529
policy FNV-1a:                3c0f3a0b
MINIT EAX:                   00000000
MINIT-return I801 402..406:  08:00:5d:01:a3
CMOS diagnostic byte 0e:     ec
```

The complete workspace reconstruction had no gap, duplicate, or conflict.
Completion and neutral-marker bytes required by the new exact helper are:

```text
workspace[1]/[2]/[4f]/[e79] = 00/00/02/01
18d1..18d3                    = 43:18:00
23a2 / 2402                  = 06 / 00
2459..2461                   = 00:00:00:00:ff:ff:ff:ff:ff
26b6..26bb                   = 0c:78:37:0c:78:37
26c0..26c5                   = 0a:7a:39:0a:7a:39
```

The remaining mandatory endpoint includes QPI `070f0f03`, IOH stage
`bf000000`, MC50/54 `0a000006/00000006`, mapper/common-f8
`00024489/00001545`, and channel DOD/ranks/status
`000002ac/00000003/00000140`.

Compared with the older O workspace, Q differs in 58 of 11,212 bytes. Forty-
nine differences fall inside pre-existing canonicalization ranges and nine do
not. B06VJ therefore adds a separate exact row and marker helper; it does not
extend those ranges or pretend O and Q are one wildcard form.

## Six-row producer/consumer contract

`x58_b06vi_profile_for_tuple()` remains the sole truth table used by both the
romstage producer and ramstage consumer:

```text
PRIMARY-A  A0 17000/17000  9c b00502  CSI 08/03e3d24e/908dabb6  WORK 94299f43/a6f9c2e6  ID 1
PRIMARY-B  A0 17000/17000  9c b00502  CSI 08/03e3d24e/908dabb6  WORK b6346533/a6f9c2e6  ID 2
PRIMARY-P  A0 17000/17000  9c b00502  CSI 08/03e3d24e/908dabb6  WORK eb15c076/c313e060  ID 3
PRIMARY-N  A0 17000/17000  9c b00502  CSI 08/03e3d24e/908dabb6  WORK 5fb636d9/b3fafec0  ID 4
O          A0 17c00/17c00  9c a00502  CSI 0c/8b38506a/908dabb6  WORK 50f67315/92c70df4  ID 5
Q          A0 17c00/17c00  9c a00502  CSI 0c/8b38506a/908dabb6  WORK 69b4c386/0b161f01  ID 6
```

Romstage separately requires the post-CSI/pre-MINIT endpoint to equal the
selected row. The Q marker helper requires all 26 listed marker bytes in
addition to both workspace digests. Cartesian crosses, every single tuple-
field mutation, every single Q-marker mutation, and wrong profile IDs are
executable negative tests.

The inherited vendor probe can report `X58_VENDOR_ERR_PLATFORM_STATE` for
this endpoint family. B06VJ may provisionally admit that status only after an
exact O or exact Q digest-and-marker helper succeeds; every common module,
ABI/canary, policy, I801/CMOS, CPU/QPI/IOH, MC, SPD, and MTRR gate remains
mandatory. With the B06VJ option disabled, Q is rejected and the version-6
B06VI five-row contract remains intact.

## Version-7 handoff and guard ordering

The CBMEM record remains 160 bytes but uses wire version 7 so a B06VJ producer
cannot be confused with a B06VI consumer. It carries the selected profile ID,
all raw/canonical digests, and the coupled CPU endpoints; its final FNV covers
every preceding byte.

O and Q both keep MINIT's returned I801 tuple until all of the following pass:

```text
exact coupled result and default-UC MTRRs
complete 0..640-KiB destructive test and clear
transactional 14-address alias test and rollback
complete 8-MiB CBMEM-window test and clear
complete 8-MiB object-window test and clear
CBMEM creation and exact version-7 handoff read-back
```

Only then may the code restore and read back `08:64:9b:5c:a3`. The common
finalizer clears I801 and restores CMOS byte `0x0e` to `0x2c` before postcar.
Any earlier mismatch remains fail-closed and preserves the CAR recovery path.

## Expected exact-Q serial path

Useful milestones are:

```text
[QPI] B06VJ COUPLED_PROFILE=O-FAMILY ... A0=00017c00 ... CPU9C=00a00502
[RAMINIT] B06VJ COUPLED EXACT WORK_RAW=69b4c386 ...
[RAMINIT] B06VJ_COUPLED_AUTO_HANDOFF=READY PROFILE=06; O-family I801 rearm deferred until full v7 postmem
[RAMINIT] B06VJ O-family POSTMEM_REARM ... REARMED_EXACT=01 CMOS0E=ec AFTER_FULL_POSTMEM=01
[RAMINIT] B06VJ persistent guard finalized after v7 CBMEM readback
[HANDOFF] PROFILE=6 PRE_A0=00017c00 POST_A0=00017c00 POST_9C=00a00502
[PAYLOAD] B06VJ coupled profile v7 handoff accepted; leaving DRAM ROMMON callback
[PAYLOAD] entering SeaBIOS payload
SeaBIOS (version ...)
Boot support not compiled in.
```

The intended board-specific POST suffix is unchanged from B06VI. The complete
serial trace is authoritative because generic coreboot POST values can
overwrite transient board-specific codes.

## Payload and recovery boundary

The payload is deliberately serial-only and non-booting. It has no VGA,
option-ROM, storage, USB, network, TFTP, SSH, or operating-system boot support.
Reaching `Boot support not compiled in.` is the intended B06VJ result, not a
general first boot.

Use only the established E5645/CPUID-206c2, sole SPD-0x54 4-GiB DIMM with FNV
`fb66b530`, ratio 6, unchanged GPU/PSU, and socketed W25Q128 setup. Verify the
complete programmer read-back against the manifest. Although the retained
B06VI session has CMOS `0x0e=0x2c`, the idle I801 return tuple is still
present; a complete G3/AC removal is mandatory after flashing B06VJ. This
board restores into S5, so a physical power-button start may again be needed.

Arm COM1 at 115200 8N1 before starting. If CAR is unavailable, remove AC and
restore the externally verified known-good/vendor chip. Do not depend on
DRAM, SeaBIOS, GPU, storage, USB, NIC, TFTP, or SSH for recovery.

## Proprietary-byte boundary

The public base contains coreboot, microcode, and the reduced SeaBIOS payload,
but no MSI CSI/MINIT bytes. The flashable composite is created only from the
user's hash-pinned local firmware and stays below ignored `blobs-local/`.

## Hardware result — 2026-09-05

One retained B06VJ execution completed both intended CSI-produced reset
continuations. The user reported the new image active, but the initial AC/G3
action and programmer read-back were not independently captured. The serial
entry is therefore one execution of unconfirmed initial power type, not a
confirmed controlled-G3 repetition. A recurring preliminary phase-1 fragment
is retained in the log and is not counted as another execution.

Pass 3 selected the exact O-family CPU/CSI row and the guarded MINIT call
returned:

```text
saved/post/post-MINIT CPU A0: 00017c00
post-CSI/post-MINIT CPU 9c:   00a00502
CSI byte/raw/canonical:       0c / 8b38506a / 908dabb6
policy FNV-1a:                3c0f3a0b
MINIT EAX:                    00000000
MINIT-return I801 402..406:   08:00:5d:01:a3
QPI/IOH stage:                070f0f03 / bf000000
MC50/54:                      0a000006 / 00000006
mapper/common-f8:             00024489 / 00001545
channel DOD/ranks/status:     000002ac / 00000003 / 00000140
```

The complete returned workspace is a third exact raw form in this canonical
family:

```text
raw FNV-1a:       dd61cb51
canonical FNV-1a: 0b161f01
SHA-256:          370ca2366b44bfc66882f286686600851a60bf834ae5c79c249ba365ad845cef
```

Offline parsing reconstructed all `0x2bcc` bytes from 701 records with no gap,
duplicate, or conflict. Relative to B06VJ's admitted Q workspace
`69b4c386/0b161f01`, exactly 16 bytes changed, at offsets:

```text
1336 1338 1346 134c 1350 1358 1360 136a 136c
13c8 13ce 13f0 13fc 242d 24bd 252d
```

Every difference is inside an existing canonicalization range; there are zero
changes outside those ranges. The four common completion bytes remain
`00/00/02/01`, and the complete 26-byte Q-specific marker tuple is unchanged.
Thus B06VJ's exact endpoint, canonical digest, completion bytes and markers all
recurred, while the raw digest did not. The runtime line
`RAW_PATTERN_GATE=00` is a combined, short-circuited result after that raw
digest mismatch; it is not independent evidence that a marker differed.

B06VJ consequently refused the provisional `platform-state` exception and
stopped at `B06VJ_COUPLED_POST_MINIT_EXACT_GATE` with
`DRAM_ACCESSES=00`. It did not execute the low-memory or 8-MiB tests, create
CBMEM, enter postcar, relocate ramstage, start SeaBIOS, or initialize graphics.
The surviving monitor was still the coreboot CAR romstage ROMMON, not a
DRAM-backed coreboot stage. Module signatures, ABI/canary state, script state,
and `ROMMON_DIRTY=00` remained clean.

The immutable serial slice is
`research/msi/captures/2026-09-05-b06vj-hw-01-o-workspace-dd61cb51.raw`,
68,509 bytes / 1,285 lines, SHA-256
`088740f2e693d059487198b93d4e49f1a139085b330d79698efd90b51255dfbf`.
Its first opening `[` was cropped by the slice boundary, so it is not described
as byte-complete from the opening banner; the CSI, policy, and full workspace
records themselves parse completely.

After capture, a first batched UART command attempt fragmented into partial
commands and was rejected safely. Individually issuing `unlock RESET`,
`autoguard clear`, and `vinputs` then changed CMOS diagnostic byte `0x0e` from
`0xec` to `0x2c` and verified `VALID=01`. The 573-byte cleanup capture is
`research/msi/captures/2026-09-05-b06vj-hw-01-postcapture-guard-clear.raw`,
SHA-256
`9ee226a8d40563a5f45850c18a4f13223a98becba8f01feb176705f4e61c4d7d`.
No reset, Shelly request, AC cycle, or power action occurred during this run or
cleanup.

For future lab control, the current operator constraint is to address the
Shelly directly at `192.0.2.215` and use only `OFF -> 1 second -> ON`.
Power-on-after-AC-loss is not presently reliable, so the board may remain in
S5 and still require a physical power-button start. This constraint does not
retroactively establish the boot provenance of this B06VJ execution.
