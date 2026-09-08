> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VG coupled High-QPI MINIT/SeaBIOS experiment

Status: **BUILT TWICE / BYTE-REPRODUCIBLE / 133/133 SOURCE CONTRACTS /
CBFS AND LOCAL COMPOSITE VERIFIED / HARDWARE EXERCISED / FAIL-CLOSED BEFORE
HANDOFF / NO PAYLOAD**.

The exact public and local artifact hashes are recorded in the
[B06VG image manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

B06VG is a deliberately bounded successor to B06VF. It does not infer a field
meaning from CPU register A0 and it does not broaden the gate with a mask or a
numeric range. Instead, it recognizes two disjoint, fully coupled profiles.
The already established profile may attempt the inherited coreboot-to-SeaBIOS
transition. The newly observed profile may call MINIT once for measurement,
but must then return to the pre-DRAM CAR ROMMON.

Like its predecessors, B06VG is a locally vendor-assisted research bridge.
The flashable local image contains hash-pinned MSI CSI/MINIT code supplied by
the user. Those proprietary bytes remain under `blobs-local/`, are ignored by
the repository, and must not be redistributed. This is not native/open X58
memory or QPI initialization.

## Hypothesis

Four B06VF G3 captures completed CSI passes 1 and 2. Their pass-3 pre-A0
observations were:

```text
G3-02  00017a00  -> CSI returned 0/0/11; post-A0 00017a00; 9c 00a00502
G3-03  00017400  -> B06VF stopped before CSI
G3-04  00017400  -> B06VF stopped before CSI
G3-05  00017800  -> CSI returned 0/0/11; post-A0 00017800; 9c 00a00502
```

Both returned states used CSI byte/raw/canonical
`0c/8b38506a/908dabb6` and retained High-QPI status `070f0f03`. This is
evidence that the returned CPU A0 may be relational to the pre-call value,
not proof that a broad A0 field or range is understood.

B06VG therefore tests the narrow hypothesis that:

- the old exact `17000 -> 17000`, `9c=b00502`, CSI
  `08/03e3d24e` state remains the only payload-capable profile;
- a non-170 value from the finite observed set may be meaningful only if it
  survives CSI unchanged, ends at `9c=a00502`, and carries one exact
  byte/digest pair;
- one guarded MINIT return from that second profile can reveal whether its
  post-MINIT state is complete without treating it as usable DRAM.

## Exact profile classifier

The caller samples pre-CSI CPU `ff:02.1 + 0xa0` twice and requires both reads
to agree. The vendor-call layer independently samples it twice, compares it
with the caller's saved value, checks the rest of the exact High-QPI platform
gate, and performs one final A0 drift check before arming CSI.

Only these six literal pre-A0 values exist in the policy:

```text
00017000
00017400
00017600
00017800
00017a00
00017c00
```

After the pass-3 CSI return, exactly one of these profiles may match:

| Profile | Saved pre-A0 | Post-A0 | CPU 9c | CSI byte/raw FNV | Action |
| --- | --- | --- | --- | --- | --- |
| PRIMARY | `17000` | equal to saved `17000` | `b00502` | only `08/03e3d24e` | one guarded MINIT, then unchanged B06VF promotion gates |
| OBSERVATION | one of `17400/17600/17800/17a00/17c00` | exactly equal to saved value | `a00502` | `08/03e3d24e` or `0c/8b38506a` | one guarded MINIT, full telemetry, mandatory CAR-ROMMON terminal |

The byte and digest are a pair. `08/8b38506a` and `0c/03e3d24e` are
rejected. A0 drift, `17000/a00502`, non-170/`b00502`, unobserved values,
and every other combination are rejected before MINIT.

## One-way control-flow boundary

```text
reset
  -> guarded three-pass High-QPI CSI sequence
  -> stable pre-A0 capture and exact pass-3 return classifier
     |
     +-> REJECTED
     |     -> fail closed in CAR ROMMON; no MINIT
     |
     +-> OBSERVATION
     |     -> one persistent-guarded MINIT call
     |     -> complete CSI and 11212-byte workspace telemetry
     |     -> B06VG_OBSERVATION_TERMINAL
     |     -> CAR ROMMON; no ordinary DRAM access or handoff
     |
     +-> PRIMARY
           -> one persistent-guarded MINIT call
           -> unchanged B06VF exact post-MINIT/workspace gates
           -> complete tested-memory transition and v4 CBMEM handoff
           -> postcar -> ramstage -> coreboot tables
           -> PAM/shadow proof -> SELF load -> reduced SeaBIOS
```

The OBSERVATION branch returns before the sole call to the inherited promotion
function. It cannot set `AUTO_READY`, record a CBMEM handoff, execute the
ordinary DRAM tests, enter postcar/ramstage, or invoke the payload. Even a
MINIT return labelled `FULL_PATH_RETURN_DRAM_UNTESTED` remains telemetry,
not authorization.

Only PRIMARY can inherit B06VF's exact post-MINIT gates, destructive 0--640
KiB test, complete 16-MiB window tests, CBMEM/postcar transition, conservative
resource publication, PAM checks, 34-word C--F shadow transaction, and
SeaBIOS SELF loading.

## SeaBIOS payload boundary

The embedded payload is the same deliberately reduced SeaBIOS rel-1.17.0
configuration as B06VF:

```text
one PT_LOAD segment: 000f8800..000fffff
entry point:         000fecd6
debug console:       COM1 03f8, 115200 8N1
boot support:        disabled
VGA/storage/USB:     disabled
```

It is a serial execution probe, not a usable boot firmware. It can still
perform PCI configuration reads and legacy DMA/PIC/PIT/RTC writes during
POST. No VGA output is expected. If it completes, the intended final message
is:

```text
Boot support not compiled in.
```

That message would prove substantial payload execution, not PCI resource
assignment, working devices, an operating-system boot, or full-DIMM validity.

## Persistent guard preparation

CSI/MINIT experiments deliberately retain reset-loop state. Before flashing
B06VG, inspect the currently active CAR ROMMON. If `vinputs` reports
`CMOS_DIAG_0E=ec` or `ed`, clear it only through the guarded command:

```text
id
vinputs
unlock RESET
autoguard clear
vinputs
```

The final passive read-back must report:

```text
CMOS_DIAG_0E=2c VALID=01
```

Then remove AC power completely. The I801 phase signature is volatile and is
cleared by that real G3 interval. If B06VG is already installed with a retained
`ec/ed` marker, its fail-closed ROMMON can be used for the same sequence,
followed by another complete AC removal.

## Exact first-test configuration

```text
board:          MSI X58 Pro-E / MS-7522 revision 3.0
CPU:            Intel Xeon E5645
CPUID:          000206c2
microcode:      0000001f
DIMM:           BLS4G3D1609DS1S00
SPD topology:   sole responder at 0x54, full-256 FNV fb66b530
DDR policy:     ratio 6, no XMP or overclocking
initial QPI:    Slow; the guarded sequence performs the High-QPI transition
GPU:            unchanged single test GPU; no graphics output expected
UART:           COM1 03f8, 115200 8N1, no flow control
flash:          spare socketed W25Q128, complete 16-MiB image
start:          controlled true G3/AC-cold boot
```

Preserve the verified vendor/known-good recovery chip. Program the complete
16-MiB B06VG image and verify the programmer read-back against:

```text
2dcc944f579383c9d82eb941187a5da4acb7c0abefea4b72c4c9078895b8a01d
```

Start raw COM1 and POST-code capture before applying AC. Do not send ROMMON
input or issue a manual reset through the two expected CSI-produced reset
continuations. Stop after the first terminal result and preserve the raw log
before clearing any guard.

## Expected first-run outcomes

The recent B06VF observations make OBSERVATION the likely first outcome, but
the hardware state determines the profile.

For OBSERVATION, require:

```text
[QPI] B06VG ... PHASE=03
[QPI] B06VG PRE_A0 FIRST/SECOND=.../... ... STABLE=01
[QPI] B06VG COUPLED_PROFILE=OBSERVATION PRE_A0=... POST_A0/9C=.../00a00502
POST d3
POST d4                         only if MINIT returns
[RAMINIT] B06VG MINIT_OBSERVATION RESULT=...
[RAMINIT] B06VG_OBSERVATION_TERMINAL
[RAMINIT] AUTO_FALLBACK=B06VG_OBSERVATION_MINIT_TERMINAL
CAR ROMMON prompt
```

The exact spelling of intermediate telemetry should be taken from the raw
serial output; the invariant is that no B06VF handoff or SeaBIOS banner may
appear after an OBSERVATION.

For PRIMARY, the expected successful continuation includes the inherited
post-memory/payload suffix:

```text
[QPI] B06VG COUPLED_PROFILE=PRIMARY PRE_A0=00017000 POST_A0/9C=00017000/00b00502
POST d3, d4
[RAMINIT] B06VG_PRIMARY_AUTO_HANDOFF=READY
POST 21, 22, 23, 28, 29, 2a
SeaBIOS (version ...)
...
Boot support not compiled in.
```

Transient generic coreboot POST codes may overwrite some values quickly, so
the full serial log is authoritative. A rejected profile should name a
`B06VG_...` fallback and return to CAR before MINIT.

## Failure modes and recovery

The proprietary CSI or MINIT call may reset, halt, hang, or return an unknown
state. The persistent CMOS/I801 guard prevents an automatic retry loop; do not
clear it until the captured failure has been preserved. If serial disappears,
POST remains at a call boundary, or the board loops:

1. remove AC power completely;
2. recover with the externally verified vendor or retained known-good chip;
3. if CAR ROMMON is available, record `id`, `vinfo`, and `vinputs`;
4. clear `ec/ed` only with `unlock RESET` plus `autoguard clear`;
5. verify `2c/VALID=01`, then remove AC again before a retry.

Do not rely on GPU, storage, USB, NIC, TFTP, SSH, or the SeaBIOS payload for
recovery. See [flash recovery](flash-recovery.md).

## Build and validation evidence

- two clean, ccache-disabled builds with
  `SOURCE_DATE_EPOCH=1788602400` produced byte-identical coreboot ROM,
  generated config, and SeaBIOS ELF;
- the full repository Python suite passed 133/133 tests after the final source
  cleanup; B06VG's focused contract contributes 12 tests;
- predecessor B06VA--B06VG focused contracts passed 71/71;
- vendor-composer, ROMMON-script, and RAM-loader support tests passed 28/28;
- portable C harnesses passed 8 ROMMON-script and 5 RAM-loader scenarios;
- CBFS layout, SeaBIOS ELF entry/range, and all 30720 loaded segment bytes were
  checked;
- the local composite copied only the four hash-pinned vendor ranges, applied
  the reviewed seven-byte deterministic wrapper patch, and verified exact
  top placement in a 16-MiB W25Q128 image;
- the lower 12 MiB are all `ff`, and the upper 4 MiB compare byte-for-byte
  with the deterministic composite.

Those checks alone were not a B06VG hardware result. The later G3 evidence
below records what the exact image did on the board; it does not change the
build hashes or turn a manually executed ROMMON test into an automatic path.

## Hardware results: G3-01 through G3-05

Five G3-labelled attempts reached pass 3. G3-01 was observed live before a
complete capture had been started and therefore has no immutable raw-log hash.
G3-02 through G3-05 have retained serial evidence. The profile classifier
behaved as designed:

| Attempt | Pass-3 saved/post A0, CPU 9c, CSI byte/raw/canonical | Automatic result |
| --- | --- | --- |
| G3-01 | `17400->17400`, `b00502`, `0c/8b38506a/908dabb6` | crossed profile rejected before MINIT; live observation only, no retained raw log |
| G3-02 | `17a00->17a00`, `a00502`, `08/03e3d24e/908dabb6` | OBSERVATION admitted; MINIT returned EAX zero; mandatory observation terminal in CAR |
| G3-03 | `17400->17400`, `b00502`, `08/03e3d24e/908dabb6` | crossed profile rejected before MINIT; immutable capture retained |
| G3-04 | `17000->17000`, `b00502`, `08/03e3d24e/908dabb6` | PRIMARY admitted; MINIT returned; post-MINIT exact gate rejected; CAR retained |
| G3-05 | `17000->17000`, `b00502`, `08/03e3d24e/908dabb6` | PRIMARY admitted; MINIT returned; post-MINIT exact gate rejected; CAR retained |

The G3-02 automatic branch printed
`MINIT_OBSERVATION RESULT=FULL_PATH_RETURN_DRAM_UNTESTED`, dumped the complete
`0x304`-byte CSI and `0x2bcc`-byte workspace buffers, then printed
`B06VG_OBSERVATION_TERMINAL`. Its workspace is FNV-1a `64e3c821`, SHA-256
`43c9e94ea2e13a42762aeb01ee837022d7ebfb2b901405cefc6f5d254099191e`.
The explicit `DRAM_ACCESSES=00` and terminal control flow are the automatic
result; later manual ROMMON accesses are a separate experiment.

Both PRIMARY runs returned from CSI with EAX/EBX/ECX `0/0/11` and from MINIT
with EAX zero and the same ABI-clean return tuple. They retained the exact
post-MINIT High-QPI and one-channel endpoint:

```text
CPU ff:02.1 50/54/6c/80 = 160c0112/00000012/0040a0a8/070f0f03
CPU ff:02.1 94/9c/a0/a4 = 00010202/00b00502/00017000/00322808
IOH 00:0d.0 82c/840/854/85c/864 =
  004060a0/070f0f03/00010102/00000002/00322808
link CPU50/58, IOH_C8, SR0/SR1, SYRE_CC =
  86000000/00064555, 0616fc00, 00000000/00000000, 00000600
post-MINIT CPU ff:02.0 80/d0 = 0000fe91/00000501; pointer match = 01
post-MINIT IOH 00:14.1 9c = bf000000; MC 50/54 = 0a000006/00000006
MC map/common, channel-2 DOD/ranks/status =
  00024489/00001545, 000002ac/00000003/00000140
```

The two complete PRIMARY workspaces are retained under neutral internal labels
P and N. Those labels carry no inferred semantic meaning:

| Form | Attempt | raw FNV-1a | current B06VF canonical FNV | SHA-256 | bytes `2459..2460`; byte `2461` |
| --- | --- | --- | --- | --- | --- |
| P | G3-04 | `eb15c076` | `c313e060` | `37f2cf136cd7549f0558eed7e24cc68ddc89485887d48331bd01eb0e697e8681` | eight `ff`; `ff` |
| N | G3-05 | `5fb636d9` | `b3fafec0` | `dba44459fa45e5f6c3c16c7b2dd5908a59cba41cdd00e50e1cfb130b2eef219f` | eight `00`; `ff` |

Offline parsing found 701 contiguous 16-byte records covering exactly
`0000..2bcb`, with no gap, overlap, or conflicting duplicate, in each dump.
Neither form matches B06VG's inherited raw-pattern/canonical workspace gate;
both runs also reported runtime probe `platform-state`/code `0d`.

### Post-MINIT persistent-marker observation

All three MINIT returns printed
`POST_MINIT_GUARD I801_EXACT=00 CMOS0E=ec`. G3-04 and G3-05 then passively
read I801 offsets `402..406` and returned, identically:

```text
control:command:address:data0:data1 = 08:00:5d:01:a3
```

G3-02 did not perform that passive byte read. Offset `400` was deliberately
not read in the two measured runs because host status is read-to-clear.
This repeat demonstrates that the vendor MINIT path overwrites the pre-call
`08:64:9b:5c:a3` I801 marker. It does not mean the persistent guard can be
removed: CMOS `0e=ec` remained intact. A later implementation may re-arm an
I801 return marker only after an otherwise accepted MINIT return, with exact
read-back, before any possible promotion. B06VG itself did not do that and
therefore correctly remained fail-closed.

### Manual bounded DRAM evidence, not the automatic test path

After each terminal CAR prompt in G3-02, G3-04, and G3-05, an operator loaded
the same sealed 28-operation script (`PROGRAM_FNV=0a85f9f6`). It wrote unique
dwords to fourteen selected addresses spanning both intended test windows:

```text
01000000 01000004 01000ffc 01001000 013ffffc 017ff000 017ffffc
02000000 02000004 02000ffc 02001000 023ffffc 027ff000 027ffffc
```

All fourteen subsequent exact assertions passed. Reverse-order rollback then
read back every saved original and returned `ROLLBACK=ok`:

| Attempt/profile | run transaction FNV | rollback transaction FNV | Result |
| --- | --- | --- | --- |
| G3-02 / OBSERVATION | `4e9a06e3` | `636a402c` | `RUN=ok`, `ROLLED_BACK=01` |
| G3-04 / PRIMARY P | `16f13368` | `120afb90` | `RUN=ok`, `ROLLED_BACK=01` |
| G3-05 / PRIMARY N | `efec928d` | `9566b3b0` | `RUN=ok`, `ROLLED_BACK=01` |

This is repeated selected-address write/read/alias and rollback evidence.
G3-02 additionally read `IA32_MTRR_DEF_TYPE=0000000000000800`, establishing
the effective UC default for that run. G3-04 and G3-05 did not repeat that MSR
measurement, so cache-independent access is not independently claimed for
those two runs. This is not the automatic 0--640-KiB test, not either complete
8-MiB-window test, and not a memory soak. In every run the automatic path had
already failed closed in CAR. No CBMEM handoff, postcar, ramstage, coreboot
table construction, SELF load, SeaBIOS banner, PCI enumeration, VGA, storage,
or payload execution occurred.

### Immutable capture provenance

The `final.raw` file for each retained attempt is the authoritative full
session; the other files are earlier retained checkpoints of the same session.

```text
b45dc52b13930c43a6558710da5981f839aa1c3f7216ee9e77c99ba1ce6048c3  2026-09-05-b06vg-g3-02-minit-return.raw         (79502 bytes)
4d6333b4fd098997986f4609ca7b4d72237b2c1cf53deb6ca44452a1a2a84cfe  2026-09-05-b06vg-g3-02-minit-dram-complete.raw  (98514 bytes)
0864a1a0fa2cbbb1bc15025b9dfde3134642ec99b06ce04d6f3b985296de22c4  2026-09-05-b06vg-g3-02-final.raw                (99088 bytes)
f8efe15f7c084b0466a46932999acd6da751da59c8dd47a852a2cd531ffc2902  2026-09-05-b06vg-g3-03-cross-174-b-08.raw       (17864 bytes)
3eebb29f37708c750749b306cc1d9e53e7848082d5122e5d2241c6f731feb9f9  2026-09-05-b06vg-g3-04-primary-new-workspace.raw (63018 bytes)
50f410efbff9df6f85f4e6017cfd2f1d2d0aa1538cc2e5071ffdbd946f0d8544  2026-09-05-b06vg-g3-04-final.raw                (81419 bytes)
07f3df18dd9b7868ab545d548ca626fd5aac88f1ddf7808746afbfad58de96ae  2026-09-05-b06vg-g3-05-workspace.raw            (57905 bytes)
2821f5c4d3984608c98e2f0b3931042cc117b81413cc4d75a4c57b43aa98d404  2026-09-05-b06vg-g3-05-final.raw                (75943 bytes)
```

The manually cleared final `CMOS_DIAG_0E=2c VALID=01` records in G3-02,
G3-04, and G3-05 are recovery preparation after the evidence was captured;
they are not part of the automatic success path. Another retry still requires
a complete AC removal.
