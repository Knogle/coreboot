> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VI coupled profile-O SeaBIOS experiment

Status: **BUILT TWICE / BYTE-REPRODUCIBLE / STATIC CONTRACT VERIFIED /
LOCAL COMPOSITE VERIFIED / TWO HARDWARE EXECUTIONS / ONE CONTROLLED G3 /
EXACT O + MINIT RETURNED / NEW WORKSPACE REJECTED BEFORE DRAM**.

B06VI is the smallest follow-up to B06VH that can turn the exact
`B06VH-HW-G3-04` observation into an automatic post-memory and reduced-SeaBIOS
attempt. It does not broaden the old OBSERVATION class. It adds one named,
indivisible profile, `O`, while preserving the previous PRIMARY A/B/P/N path.

## Hypothesis

The G3-04 return is sufficiently coherent to run the already bounded memory
checks and attempt postcar, ramstage, and the reduced serial-only SeaBIOS
payload when—and only when—the complete observed tuple repeats exactly.

This is deliberately an experimental bring-up assumption. For B06VI, the
exact gates plus the inherited bounded tests are provisionally treated as
enough for an automatic payload attempt. They are **not** a claim that DDR3 is
stable. Final stability still requires repeated cold and warm boots and an
extended independent memory test.

## Exact evidence profile

Profile O is one row; none of these values is an independent allowlist:

```text
profile ID:                  O (wire value 5)
saved pre-CSI CPU A0:        00017c00
post-CSI/pre-MINIT CPU A0:   00017c00
post-MINIT CPU A0:           00017c00
post-MINIT CPU 9c:           00a00502
CSI byte 2a6:                0c
CSI raw FNV-1a:              8b38506a
CSI canonical FNV-1a:        908dabb6  (only byte 2a6 canonicalized to zero)
workspace raw FNV-1a:        50f67315
workspace B06VF canonical:   92c70df4
workspace full SHA-256:      7f2d1547de4a7b9c854f518727e60b4cf22ad7c2ba408273105069521081f42f
MINIT EAX:                   00000000
MINIT-return I801 402..406:  08:00:5d:01:a3
CMOS 0e:                     ec
```

The full workspace dump was reconstructed offline as 0x2bcc contiguous bytes.
Its raw FNV, canonical FNV, full SHA-256, and the following marker bytes all
agree with G3-04:

```text
18d1..18d3 = 44:18:00
23a2       = 06
2402       = 00
2459..2460 = ff:ff:ff:ff:ff:ff:ff:ff
2461       = ff
26b6..26bb = 0c:7a:38:0c:7a:38
26c0..26c5 = 0c:7a:38:0c:7a:38
```

Common completion bytes remain exact (`workspace[1:2]=00:00`, `4f=02`,
`e79=01`). The final CPU/IOH/QPI endpoints, MC mapper/status values, default-UC
MTRRs, and ABI/canary checks are the same exact values already required by
B06VH except for the profile-coupled A0/9c fields above.

The manual G3-04 transactional 14-address test used program digest
`0a85f9f6`; pattern readback and rollback both passed. That single bounded run
supports attempting the automatic path, but is not long-form RAM validation.

## One coupled truth table

`x58_b06vi_profile_for_tuple()` is the sole admission table shared by the
romstage producer and CBMEM-handoff consumer. Its five complete rows are:

```text
PRIMARY-A  17000/b00502  CSI 08/03e3d24e/908dabb6  WORK 94299f43/a6f9c2e6
PRIMARY-B  17000/b00502  CSI 08/03e3d24e/908dabb6  WORK b6346533/a6f9c2e6
PRIMARY-P  17000/b00502  CSI 08/03e3d24e/908dabb6  WORK eb15c076/c313e060
PRIMARY-N  17000/b00502  CSI 08/03e3d24e/908dabb6  WORK 5fb636d9/b3fafec0
O          17c00/a00502  CSI 0c/8b38506a/908dabb6  WORK 50f67315/92c70df4
```

The A0 column means saved and post-MINIT A0. Romstage additionally requires
the post-CSI/pre-MINIT A0 and 9c to equal that selected row. Crossed CSI,
workspace, A0, 9c, canonical, raw, marker, or profile-ID combinations fail
closed.

The existing vendor-side B06VE post-MINIT helper remains intentionally
unchanged and therefore reports `X58_VENDOR_ERR_PLATFORM_STATE` for O. B06VI
may provisionally admit that status only after the O-specific raw digest,
O-specific canonical digest, and exact workspace pattern are all known. That
exception alone causes neither promotion nor a write: the independent complete
coupled tuple, all endpoint fields, ABI state, module digests, I801/CMOS state,
and memory-controller values must still pass before a result is recorded.

## Version-6 handoff

The version-6 CBMEM record extends the prior result with:

```text
profile_id
saved_pre_a0
post_minit_cpu_a0
post_minit_cpu_9c
```

It retains the existing raw/canonical CSI and workspace digests. The complete
wire structure is 160 bytes and its final FNV covers every preceding byte.
Ramstage re-evaluates the same coupled truth table and rejects a profile ID
that does not match the carried tuple.

## Guard timing

The inherited PRIMARY path is deliberately unchanged: after every exact
PRIMARY return gate, it restores and reads back `08:64:9b:5c:a3` before the
post-memory tests.

O uses stricter timing:

```text
MINIT returns 08:00:5d:01:a3 with CMOS 0e=ec
  -> exact O/profile/endpoint/module/ABI gates
  -> exact default-UC MTRR gate
  -> complete 0..640-KiB destructive test and clear
  -> transactional 14-address alias test and rollback
  -> complete 8-MiB CBMEM and 8-MiB object-window tests and clear
  -> create and read back the exact version-6 CBMEM handoff
  -> verify 08:00:5d:01:a3 still survives
  -> restore/read back 08:64:9b:5c:a3
  -> common finalizer clears I801 and restores CMOS 0e=2c
  -> postcar -> ramstage -> reduced SeaBIOS entry probe
```

Any failure before finalization leaves the persistent guard armed and stops or
returns to the CAR recovery ROMMON. O cannot rearm the I801 marker merely from
a matching workspace or a broad OBSERVATION classification.

## Expected serial markers

On an exact O run, useful milestones include:

```text
[QPI] B06VI COUPLED_PROFILE=O PRE_A0=00017c00 POST_A0/9C=00017c00/00a00502
[RAMINIT] POST_MINIT_I801_RAW=08:00:5d:01:a3 ... REARM_AFTER_FULL_POSTMEM=01
[RAMINIT] B06VI_COUPLED_AUTO_HANDOFF=READY PROFILE=05; O I801 rearm deferred until full v6 postmem; provisional payload attempt
[RAMINIT] B06VI O POSTMEM_REARM ... REARMED_EXACT=01 CMOS0E=ec AFTER_FULL_POSTMEM=01
[RAMINIT] B06VI persistent guard finalized after v6 CBMEM readback
[PAYLOAD] B06VI coupled profile v6 handoff accepted; leaving DRAM ROMMON callback
[PAYLOAD] entering SeaBIOS payload
SeaBIOS (version ...)
Boot support not compiled in.
```

The reduced SeaBIOS configuration remains non-booting and serial-only. It has
no graphics, storage, PCI BIOS, option-ROM, USB, or operating-system boot
claim. CAR recovery and the existing fail-closed POST codes remain available.

## First hardware record requirements

Record the exact ROM hash, programmer verify, CPU/stepping, DIMM and slot, GPU,
board revision, PSU, boot type, complete serial log, POST trace, outcome, and
whether recovery was required. A successful first payload entry should be
followed by the repository's normal multi-boot and extended-memory validation;
those later tests are validation work, not an added prerequisite inside B06VI.

## Proprietary-byte boundary

The repository contains only open source, hashes, offsets, metadata, and
locally applicable tooling. B06VI does not commit the MSI image or extracted
CSI/MINIT ranges. A flashable composite may be produced only from the user's
locally supplied, hash-pinned firmware and remains under ignored
`blobs-local/` storage.

## First hardware execution

`B06VI-HW-01` exercised both intended CSI reset continuations and safely
stopped before MINIT on a crossed phase-3 row:

```text
saved/post-CSI A0: 00017000/00017000
post-CSI CPU 9c:   00b00502
CSI byte/raw/canon: 0c/8b38506a/908dabb6
QPI status:        070f0f03
fallback:          B06VG_COUPLED_HIGH_CSI_GATE
```

The `17000/b00502` endpoint is valid only with PRIMARY CSI
`08/03e3d24e`; the observed `0c/8b38506a` CSI form is valid only in the exact
O row with endpoint `17c00/a00502`. B06VI therefore behaved as intended and
made no policy, MINIT, ordinary-DRAM, CBMEM, postcar, ramstage, or payload
attempt. CAR ROMMON remained responsive and reported valid module signatures
and canaries, zero policy/workspace digests, and `ROMMON_DIRTY=00`.

The immutable 21,547-byte capture is
`research/msi/captures/2026-09-05-b06vi-hw-01-cross-170-0c.raw`, SHA-256
`82f986fce343e443d71dd7d27ae13b0bd1e2348c1caf0cf088841dcdb152b77e`.
The run was labelled `COLD_DEFAULT`, but its initial physical power and
programmer-read-back provenance were not independently recorded, so it does
not increment the controlled-G3 count. A following controlled 60-second AC
removal left the board in S5 and awaits a physical power-button start for the
next execution.

## Second hardware execution: controlled G3, exact O, new workspace

`B06VI-HW-G3-02` began after the already recorded Shelly-controlled
60-second AC removal, AC restore into S5, and a physical power-button start.
It therefore increments the B06VI controlled-G3 count to 1/10. As on several
earlier images, the serial stream contains one unexplained preliminary
`COLD_DEFAULT` phase-1 start that ended after the SPD gate; it is not counted
as a separate execution.

The following complete sequence performed both intended reset continuations,
then reproduced the exact O pre-MINIT profile:

```text
saved/post-CSI A0:       00017c00/00017c00
post-CSI CPU 9c:         00a00502
CSI byte/raw/canonical:  0c/8b38506a/908dabb6
policy FNV before/after: 3c0f3a0b/3c0f3a0b
MINIT return registers:  EAX=00000000 EBX=fff839d8 ECX=000000bf
                         EDX=00000080 EDI=fff83e40 EFLAGS=00000096
MINIT-return I801:       08:00:5d:01:a3
CMOS guard:              ec
QPI/IOH stage:           070f0f03 / bf000000
MC50/54/map/f8:          0a000006/00000006/00024489/00001545
channel DOD/ranks/state: 000002ac/00000003/00000140
```

The proprietary MINIT function returned, but its 0x2bcc-byte workspace was a
new exact form rather than the one O row admitted by B06VI:

```text
workspace raw FNV-1a:        69b4c386
workspace canonical FNV-1a:  0b161f01
workspace full SHA-256:       6c29393a9bdde219319a4f15bf724c005843cc53e3ade6671568c75ceaaeb529
completion bytes 1/2/4f/e79:  00/00/02/01
18d1..18d3:                   43:18:00
23a2 / 2402:                  06 / 00
2459..2461:                   00:00:00:00:ff:ff:ff:ff:ff
26b6..26bb:                   0c:78:37:0c:78:37
26c0..26c5:                   0a:7a:39:0a:7a:39
```

Offline reconstruction proved complete coverage `0x0000..0x2bcb` with no
duplicate or missing bytes. Relative to O, 58 bytes differ: 49 fall inside
the already documented digest-canonicalization ranges, while nine are outside
them (`18d1` and eight bytes in `26b7..26c5`). This is not merely the visible
`2459..245c` marker variation. The exact CSI buffer is nevertheless
byte-identical to O, and all captured CPU/QPI/IOH/MC endpoints remain in the
same O endpoint family.

B06VI correctly reported `RAW_PATTERN_GATE=00`,
`PROBE_STATUS_ADMITTED=00`, and stopped at
`B06VI_COUPLED_POST_MINIT_EXACT_GATE`. It explicitly reported
`DRAM_ACCESSES=00`; no post-memory test, CBMEM, postcar, ramstage, or SeaBIOS
path ran. The subsequent full read-only ROMMON capture left
`ROMMON_DIRTY=00`. Thus this execution proves an exact O-family CSI/CPU
endpoint and a real returned MINIT call, but does not itself add a DRAM-access
or payload result.

The immutable 69,300-byte serial capture is
`research/msi/captures/2026-09-05-b06vi-g3-02-controlled60s-o-workspace-69b4c386.raw`,
SHA-256
`69cfca6197da1f5d5db379c69b4adf5b25a3246b30c50de085297efe7cd222ff`.
It includes the original boot prefix, complete CSI/policy/workspace dumps,
I801 tuple, and passive endpoint snapshot. After that capture, the dedicated
guard command changed CMOS `0xec -> 0x2c` and read it back. The separate
514-byte cleanup transcript has SHA-256
`dc2bccab9a3a0eed77358cdb0541ed99862a367124924ee6b646a8c99b13c1be`;
the volatile I801 return tuple was intentionally left unchanged for the next
mandatory G3 removal.
