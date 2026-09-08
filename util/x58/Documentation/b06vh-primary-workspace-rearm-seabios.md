> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VH exact PRIMARY workspace/rearm SeaBIOS experiment

Status: **BUILT TWICE / BYTE-REPRODUCIBLE / STATIC CONTRACT VERIFIED /
LOCAL COMPOSITE VERIFIED / NOT HARDWARE TESTED**.

Superseding hardware status (2026-09-05): **HARDWARE EXERCISED ONCE /
SAFE PRE-MINIT FALLBACK / INITIAL POWER PROVENANCE UNCONFIRMED**. The original
status line above is retained as the immutable pre-run build record. Run
`B06VH-HW-G3-01` is documented at the end of this file.

The exact build and local-composite hashes are recorded in the
[B06VH image manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

B06VH is a default-off, deliberately narrow successor to B06VG. It does two
things only on B06VG's exact PRIMARY path:

1. it extends the post-MINIT workspace truth table from the two inherited
   A/B forms to four exact A/B/P/N raw/canonical/pattern combinations; and
2. it treats the measured post-MINIT I801 tuple as a return-state gate, then
   recreates the original persistent I801 marker only after every other
   PRIMARY gate has passed.

The B06VG OBSERVATION path remains terminal in the pre-DRAM CAR ROMMON and
cannot perform the re-arm, record a handoff, access ordinary DRAM
automatically, enter ramstage, or start SeaBIOS.

This remains a locally vendor-assisted research image. The user-supplied MSI
CSI/MINIT ranges stay under ignored `blobs-local/` storage and are not
redistributable project artifacts. B06VH is not native/open X58 memory or QPI
initialization, and the embedded SeaBIOS is a serial execution probe rather
than a boot-capable payload.

## Hardware evidence and hypothesis

Two retained B06VG PRIMARY runs, G3-04 and G3-05, returned from MINIT with the
same exact High-QPI, one-channel platform endpoint but with two previously
unaccepted complete workspaces. They are named P and N only as neutral local
identifiers:

```text
G3-04 / P: raw/canonical FNV eb15c076/c313e060
G3-05 / N: raw/canonical FNV 5fb636d9/b3fafec0
```

Both runs then passively read I801 offsets `0x402..0x406`, deliberately not
reading the read-to-clear host-status byte at `0x400`, and twice observed:

```text
08:00:5d:01:a3
```

The pre-call persistent marker had been `08:64:9b:5c:a3`, while CMOS byte
`0x0e` remained `0xec`. Thus the two PRIMARY measurements prove that MINIT
changed the volatile I801 tuple in those runs; they do not establish field
semantics or universal behavior. B06VH validates the returned tuple afresh on
every attempted promotion. In particular, the earlier A/B workspace captures
are not assumed to have the return tuple: an A or B run can advance only if
the live B06VH read also equals `08:00:5d:01:a3`.

Separate manual CAR-ROMMON scripts in B06VG G3-04 and G3-05 passed a
14-address simultaneous write/assert test over selected points in the two
future DRAM windows and verified reverse-order rollback. That is supporting
selected-address evidence, not evidence that B06VH's automatic low-memory,
full-window, CBMEM, postcar, ramstage, or payload path has run.

B06VH tests this bounded hypothesis:

- P and N are valid additional PRIMARY return forms only when their complete
  raw/canonical digests and observed marker/residual bytes agree;
- A, B, P, and N may use the existing post-memory path only after every exact
  CPU, SPD, CSI, policy, workspace, IMC, QPI, IOH, ABI, CMOS, and post-MINIT
  I801 gate passes;
- recreating `08:64:9b:5c:a3` after those gates preserves the existing
  reset-loop protection until the destructive memory tests and version-5
  CBMEM handoff read-back complete;
- if any predicate or the re-arm read-back differs, the image fails closed
  without a payload transition.

No B06VH hardware execution has yet tested this hypothesis.

## Exact admission contract

### CSI profile

B06VH inherits B06VG's two disjoint CSI profiles. Only this exact PRIMARY
profile can reach B06VH promotion:

```text
saved pass-3 pre-A0:  00017000
post-CSI CPU A0:      00017000
post-CSI CPU 9c:      00b00502
CSI byte 0x2a6:       08
CSI raw FNV-1a:       03e3d24e
CSI canonical FNV:    908dabb6
```

The saved A0 is sampled twice, independently rechecked immediately before
CSI, and required to remain unchanged after the call. Crossed or broadened
profiles fail closed. The non-170 B06VG OBSERVATION profile can still execute
one guarded MINIT call for telemetry, but it always returns through
`B06VG_OBSERVATION_MINIT_TERMINAL` before the PRIMARY promotion function.

### Workspace truth table

The executable truth table is exactly four disjoint raw/canonical pairs. The
SHA-256 values are offline provenance for the full `0x2bcc`-byte captures;
firmware gates on FNVs and exact bytes, not on a runtime SHA implementation.

| Form | Raw FNV-1a | Canonical FNV-1a | Full workspace SHA-256 | Required marker/pattern |
| --- | --- | --- | --- | --- |
| A | `94299f43` | `a6f9c2e6` | `843dbc4e45b734ba18dd2f36e183886d75d22e48a2a6f3a02d44965bfbe6833a` | `2459..245c=00:00:00:00`, `2461=fe` |
| B | `b6346533` | `a6f9c2e6` | `6549cd0c2eb04bacb5dd2a15fb000c109cca8fcb622473da6934de8d11018b1c` | `2459..245c=ff:ff:ff:ff`, `2461=ff` |
| P | `eb15c076` | `c313e060` | `37f2cf136cd7549f0558eed7e24cc68ddc89485887d48331bd01eb0e697e8681` | exact P tuple below |
| N | `5fb636d9` | `b3fafec0` | `dba44459fa45e5f6c3c16c7b2dd5908a59cba41cdd00e50e1cfb130b2eef219f` | exact N tuple below |

P is additionally coupled to:

```text
18d1..18d3  45 18 00
23a2        07
2402        00
2459..2460  ff ff ff ff ff ff ff ff
2461        ff
26b6..26bb  0e 7a 37 0e 7a 37
26c0..26c5  0c 7a 38 0c 7a 38
```

N is additionally coupled to:

```text
18d1..18d3  44 1a 02
23a2        06
2402        40
2459..2460  00 00 00 00 00 00 00 00
2461        ff
26b6..26bb  0a 78 38 0a 78 38
26c0..26c5  0c 7a 38 0c 7a 38
```

Crossing any raw and canonical digest, changing a coupled byte, or supplying
the B06VG OBSERVATION-C workspace is rejected. C remains telemetry only:

```text
raw/canonical FNV: 64e3c821/e20c406b
full SHA-256:      43c9e94ea2e13a42762aeb01ee837022d7ebfb2b901405cefc6f5d254099191e
```

The producer and CBMEM/ramstage consumer both recheck the same four-pair
digest truth table. The P/N byte checks intentionally use neutral offsets;
no semantic names have been invented.

### Remaining exact PRIMARY gates

Promotion still requires all inherited exact predicates, including:

```text
CPUID / microcode:               000206c2 / 0000001f
sole full-SPD FNV:               fb66b530
PCIEXBAR / host bridge:          e0000001:00000000 / 34058086 / 06000013
QPI status:                      070f0f03
memory-clock 50/54:              0a000006 / 00000006
MINIT policy FNV:                3c0f3a0b
MINIT EAX:                       00000000
MC mapper/common F8:             00024489 / 00001545
channel-2 DOD/ranks/status:      000002ac / 00000003 / 00000140
post-MINIT IOH 00:14.1 + 0x9c:  bf000000
```

The complete vendor-call preparation, signatures, CAR canaries, call/return
flags, policy consistency, CSI state, workspace completion bytes, QPI tuple,
CPU/IOH stage fingerprints, and ABI self-consistency must also match. A
runtime-probe result of exact `platform-state`/code `0x0d` is admitted only
when the complete B06VH workspace pattern and digest pair are themselves
exact; this narrow exception covers the B06VG P/N measurements and does not
authorize another workspace.

## I801/CMOS persistence protocol

The order is deliberately one-way:

```text
before MINIT
  require phase state and CMOS guard
  write/read back I801 08:64:9b:5c:a3
  require CMOS0E=ec
  POST d3
  call MINIT exactly once
  POST d4 if it returns

immediately after MINIT
  capture I801 402..406
  require 08:00:5d:01:a3
  require CMOS0E=ec
  never auto-clear

OBSERVATION
  dump telemetry and terminate in CAR
  no re-arm and no handoff

PRIMARY
  evaluate every exact return/platform/workspace gate
  only after all gates pass, write 08:64:9b:5c:a3
  capture a fresh read-back
  require exact match and CMOS0E=ec
  record the version-5 result
  enter automatic post-memory validation

post-memory
  verify result and effective UC MTRR state
  complete every destructive memory read-back
  create and validate the version-5 CBMEM handoff
  only then clear I801 and restore the cold authorization cookie
```

The expected B06VH return and re-arm lines are:

```text
[RAMINIT] POST_MINIT_I801_RAW=08:00:5d:01:a3 EXPECT_RETURN_08:00:5d:01:a3_MATCH=01 CMOS0E=ec AUTO_CLEAR=00 REARM_AFTER_PRIMARY_GATE=01
[RAMINIT] B06VH POST_MINIT_REARM I801=08:64:9b:5c:a3 REARMED_EXACT=01 CMOS0E=ec
[RAMINIT] B06VH_PRIMARY_AUTO_HANDOFF=READY; exact four-pair workspace gate; I801 rearmed; persistent guard retained through v5 postmem
```

A failed re-arm write or fresh read-back invokes the phase-guard stop, writes
the independent CMOS `0xed` fail-lock where possible, emits POST `0x1f`, and
halts. It cannot record a handoff.

## Automatic post-memory and payload boundary

After POST `0x08`, B06VH inherits the deliberately bounded B06VF transition:

- exact recorded-result and effective-UC MTRR checks;
- complete destructive test and clear of `0x00000000..0x0009ffff`;
- one transaction containing all 14 selected addresses across
  `0x01000000..0x017fffff` and `0x02000000..0x027fffff`;
- complete destructive test and clear of each 8-MiB window;
- creation and read-back validation of the 144-byte, version-5 CBMEM handoff;
- persistent-guard finalization only after that read-back;
- postcar and ramstage handoff revalidation;
- conservative resources only for 0--640 KiB, 16--24 MiB, and 32--40 MiB;
- exact SAD ID, PAM `30/33/33/33/33/33/33`, and reversible 34-word
  C0000--FFFFF shadow checks before SELF loading.

There is no general PCI scan or resource assignment. GPU, storage, USB, NIC,
TFTP, SSH, ACPI/SMBIOS completeness, and an operating-system boot remain out
of scope for this image.

The embedded payload is the unchanged reduced SeaBIOS rel-1.17.0 probe:

```text
ELF32/i386 PT_LOAD: 000f8800..000fffff, RWE
entry point:        000fecd6
loaded bytes:       00007800
debug console:      COM1 03f8, 115200 8N1
boot support:       disabled
VGA/storage/USB:    disabled
```

The DRAM ROMMON callback validates the version-5 handoff and returns directly
to the payload path; it is not an interactive pause in this SeaBIOS build.
The intended final SeaBIOS message is:

```text
Boot support not compiled in.
```

That message would prove substantial serial payload execution only. SeaBIOS
may still perform PCI configuration reads and legacy PIC/PIT/DMA/RTC writes
during POST despite the reduced configuration.

## Exact first hardware configuration

```text
board:          MSI X58 Pro-E / MS-7522 revision 3.0
CPU:            Intel Xeon E5645
CPUID:          000206c2
microcode:      0000001f
DIMM:           BLS4G3D1609DS1S00, 4096 MiB, dual rank
SPD topology:   sole responder at 0x54, full-256 FNV fb66b530
DDR policy:     ratio 6; no XMP or overclocking
initial QPI:    Slow; guarded sequence performs the High-QPI transition
GPU:            unchanged single test GPU; no graphics output expected
UART:           COM1 03f8, 115200 8N1, no flow control
flash:          spare socketed W25Q128.V..M, complete 16-MiB image
start:          controlled true G3/AC-cold boot
```

Before the first run:

1. preserve the externally verified vendor/known-good recovery chip;
2. from a retained known-image CAR ROMMON, inspect `vinputs`; if CMOS `0xec`
   or `0xed` is present, run `unlock RESET`, `autoguard clear`, and `vinputs`;
3. require the passive read-back `CMOS_DIAG_0E=2c VALID=01`;
4. remove AC power completely;
5. program the complete B06VH 16-MiB image and verify the programmer read-back
   against
   `97a69436bead7f8b4732a9e7f2fb6e6b694a9012581a4c2239d070adf055a452`;
6. start raw COM1 and POST capture before applying AC;
7. send no ROMMON input and do not issue a manual reset during the two
   expected CSI reset continuations.

Stop after the first terminal outcome and preserve the raw capture before
clearing a guard. One successful run is not the required ten-cold-boot
milestone.

## Expected trace and acceptance

The first successful PRIMARY continuation must include this ordered
subsequence; complete serial output is authoritative because generic coreboot
POST codes can replace transient board codes quickly:

```text
X58PROE-B06VH-PRIMARY-WORKSPACE-REARM-SEABIOS-20260905
[QPI] PHASE=03
[QPI] B06VG COUPLED_PROFILE=PRIMARY ...
POST d3
POST d4
[RAMINIT] POST_MINIT_I801_RAW=08:00:5d:01:a3 ... MATCH=01 ...
[RAMINIT] B06VH PRIMARY WORK_CANON=<one exact A/B/P/N canonical FNV> ...
[RAMINIT] B06VH PRIMARY EXACT WORK_RAW=<matching exact raw FNV> ...
[RAMINIT] B06VH POST_MINIT_REARM I801=08:64:9b:5c:a3 REARMED_EXACT=01 CMOS0E=ec
[RAMINIT] B06VH_PRIMARY_AUTO_HANDOFF=READY ...
POST 08
POST 09
POST 0a
POST 0b
[RAMINIT] UC alias+full test PASS ...
[RAMINIT] B06VH persistent guard finalized after v5 CBMEM readback
POST 0c
POST 0e
POST 0f
POST 21
[PAYLOAD] B06VH PRIMARY v5 handoff accepted; leaving DRAM ROMMON callback
POST 22
POST 23
POST 28
POST 29
POST 2a
SeaBIOS (version ...)
Boot support not compiled in.
```

Immediate experiment acceptance requires the complete ordered serial trace,
an exact admitted workspace pair/pattern, both I801 lines above, every
automatic memory-test pass, valid v5 handoff, and the final SeaBIOS message.
It does not by itself close the broader DDR/QPI, PCI-enumeration, or SeaBIOS
boot milestones. A separately recorded POST-card trace, programmed-chip
read-back hash, power provenance, and the full AGENTS.md hardware metadata
must accompany the result.

An OBSERVATION result remains a valid fail-closed outcome only if it ends with
the inherited B06VG terminal and no B06VH re-arm/handoff/payload markers:

```text
[RAMINIT] B06VG_OBSERVATION_TERMINAL
[RAMINIT] AUTO_FALLBACK=B06VG_OBSERVATION_MINIT_TERMINAL
CAR ROMMON prompt
```

## Failure modes and recovery

| Boundary | Expected failure behavior | Payload possible? |
| --- | --- | --- |
| pre-CSI/profile/ordinary exact return gate | POST `0x20`, named `AUTO_FALLBACK`, CAR ROMMON | no |
| B06VG OBSERVATION | mandatory telemetry terminal and POST `0x20`, CAR ROMMON | no |
| vendor CSI/MINIT reset, halt, or hang | call-boundary trace may stop; persistent guard prevents blind retry | no |
| B06VH I801 re-arm write/read-back | POST `0x1f`, phase fail-lock `0xed` where possible, halt | no |
| result/MTRR/memory/CBMEM/handoff | terminal POST `0x14..0x19` | no |
| ramstage handoff | terminal POST `0x1a` | no |
| payload handoff/SAD/PAM/shadow | terminal POST `0x24..0x27` | no |

For a CAR fallback, first preserve `id`, `vinfo`, `vinputs`, and the complete
serial log. Clear a retained `0xec/0xed` only with:

```text
unlock RESET
autoguard clear
vinputs
```

Require `CMOS_DIAG_0E=2c VALID=01`, then remove AC completely before another
attempt. For a phase-guard halt, vendor-call hang, post-memory halt, or lost
serial path, remove AC and recover with the externally verified spare
vendor/known-good chip; use its CAR ROMMON to clear the guard if needed, then
remove AC again before retrying. Do not rely on GPU, storage, USB, NIC, TFTP,
SSH, or SeaBIOS for recovery.

## Static/build validation

The following checks are complete; none is a hardware claim:

- two complete `make clean` builds with `CCACHE_DISABLE=1` and
  `SOURCE_DATE_EPOCH=1788602400` produced byte-identical base ROMs, generated
  `.config` files, and SeaBIOS source ELFs;
- the final repository Python suite passed 149/149 tests;
- the focused B06VH source contract passed 10/10 tests, and the combined
  B06VE/B06VF/B06VG/B06VH contracts passed 45/45;
- warning-clean portable C ROMMON-script and RAM-loader harnesses passed;
- CBFS enumeration, ELF class/machine/load range/entry, all 30,720 loaded
  SeaBIOS bytes, microcode object, vendor-range composition, seven-byte
  wrapper patch, full-chip placement, erased lower 12 MiB, and exact top
  4-MiB comparison were verified.

The hardware status remains **NOT HARDWARE TESTED** until an immutable B06VH
serial/POST capture exists.

## 2026-09-05 hardware update: crossed PRIMARY/CSI state

The sentence immediately above records the pre-run status and is superseded
by this hardware record. One immutable B06VH capture now exists:

```text
db83af5cc0aa58ef5e95d532ba02bc6daafc583a34dffc59f8445ef222f7d361  research/msi/captures/2026-09-05-b06vh-g3-01-cross-170-b-0c-final.raw  (18184 bytes)
```

The retained sequence starts with `ENTRY=COLD_DEFAULT` and contains both
intended CSI-produced reset continuations through phases 1, 2, and 3. The
initial user power action and complete G3/AC-off duration were not captured
independently, so this is not counted as a confirmed cold-boot repetition.
There is also no full-chip programmer read-back for this run; the hardware
identity is established by the repeated B06VH build ID in the serial trace,
not by a post-flash SHA-256 measurement.

Pass 2 returned the required CSI ABI tuple `EAX/EBX/ECX=1/0/2a6`, reached its
accepted endpoint, and issued the intended IOH SYRE reset. Pass 3 then
returned ABI-clean and non-no-op with:

```text
pre/post CPU A0:         00017000 / 00017000
post-CSI CPU 9c:         00b00502
CSI byte/raw/canonical:  0c / 8b38506a / 908dabb6
QPI status:              070f0f03
MC 50/54:                0a000006 / 00000006
CSI return EAX/EBX/ECX:  0 / 0 / 11
```

This is a deliberately rejected crossed combination: A0 `17000` and CPU 9c
`b00502` are the PRIMARY endpoint, but B06VH admits that endpoint only with
CSI `08/03e3d24e/908dabb6`, not `0c/8b38506a/908dabb6`. The exact classifier
therefore emitted:

```text
[RAMINIT] AUTO_FALLBACK=B06VG_COUPLED_HIGH_CSI_GATE; no further automatic vendor call this boot; entering ROMMON
B06VH PRIMARY/rearm path stopped; recovery ROMMON remains in CAR.
```

This safe fallback occurred before MINIT. Consequently the run performed no
post-MINIT I801 validation or re-arm, no automatic ordinary-DRAM access, no
CBMEM/postcar/ramstage transition, and no SeaBIOS execution. Passive follow-up
commands showed High-QPI remained at `070f0f03`, CMOS byte `0x0e` remained
`0xec`, and:

```text
MINIT_ATTEMPTED=00 MINIT_RETURNED=00 WORK_FNV=00000000 ROMMON_DIRTY=00
```

The mistyped `vinp` command returned only the command error. It preceded the
successful passive `vinputs` and `vinfo` commands, and `ROMMON_DIRTY=00`
confirms that no interactive register write occurred. The result proves the
B06VH image executes on the target, completes both CSI reset continuations,
retains High-QPI, and rejects this crossed state at the intended pre-MINIT
boundary. It does **not** exercise the new A/B/P/N workspace admission,
post-MINIT I801 return/re-arm protocol, DRAM tests, or payload path.

## 2026-09-05 hardware update: controlled G3 repetition

This second record supersedes the preceding hardware count: B06VH now has two
retained executions, of which one is a confirmed controlled G3 attempt. The
new immutable capture is:

```text
918e0db571dd6ab5a93e093238b0475b753300193dd9a9e6b24b512b179c9b6d  research/msi/captures/2026-09-05-b06vh-g3-02-controlled15s-cross-170-b-0c-final.raw  (20613 bytes)
```

Before the attempt, the retained guard was passively verified at CMOS
`0x0e=0x2c`. The Shelly at `192.0.2.215` removed AC for 15 seconds using its
local restoration timer. Its output returned, but the board remained in S5 at
approximately 0.8 W and required a physical power-button press. COM1 capture
was armed throughout. This establishes one controlled G3 attempt for B06VH,
although it does not establish automatic power restoration.

The capture starts with power-return serial garbage and unusually contains
two `COLD_DEFAULT`/phase-1 starts. The first completed the SPD54 target gate
and then ended silently before PCIEXBAR preflight or CSI arm. No cause is
assigned to that preliminary restart. The second start completed phase 1,
the intended CSI-internal reset, phase 2 with accepted
`EAX/EBX/ECX=1/0/2a6`, the intended outer IOH SYRE reset, and phase 3.
Consequently only the second start is the complete three-phase sequence; the
preliminary start is recorded separately and is not counted as an additional
cold boot or intended CSI continuation.

The complete sequence reproduced G3-01's exact pass-3 crossed state:

```text
pre/post CPU A0:         00017000 / 00017000
post-CSI CPU 9c:         00b00502
CSI byte/raw/canonical:  0c / 8b38506a / 908dabb6
QPI status:              070f0f03
MC 50/54:                0a000006 / 00000006
CSI return EAX/EBX/ECX:  0 / 0 / 11
```

B06VH again rejected the crossed CPU/CSI profile at
`B06VG_COUPLED_HIGH_CSI_GATE` before MINIT and retained the CAR ROMMON.
Passive `vinputs`/`vinfo` in the immutable capture reported CMOS `0e=ec`,
`MINIT_ATTEMPTED=00`, `MINIT_RETURNED=00`, `WORK_FNV=00000000`, and
`ROMMON_DIRTY=00`. A later live passive byte read of ICH10 D31F0 offset
`0xa4`, outside this immutable capture, returned `0x02`: bit 0 was already
clear even though AC restoration had left the board in S5. This observation
does not by itself establish the remaining power-on policy dependency.

After evidence collection the persistent guard was deliberately cleared back
to `0x2c` for the next G3 attempt; that mutation is not part of this capture.
G3-02 performed no MINIT, I801 return validation/re-arm, ordinary-DRAM access,
CBMEM, postcar, ramstage, or payload execution. Current count is therefore
one confirmed controlled G3 attempt out of ten, with two safe pre-MINIT
crossed-profile fallbacks across all retained B06VH executions.

## 2026-09-05 hardware update: second controlled G3 repetition

`B06VH-HW-G3-03` brings the controlled-G3 count to 2/10 and the total retained
B06VH execution count to three:

```text
a27d6dee458f2846a6912cf31f8d3318cf34198690dab18ce708ecc76a48c104  research/msi/captures/2026-09-05-b06vh-g3-03-controlled15s-cross-174-b-0c-final.raw  (20614 bytes)
```

The pre-start guard was verified at CMOS `0x0e=0x2c`. The preceding Shelly
cycle removed AC for 15 seconds; after AC returned the board remained in S5,
and the user pressed the physical power button while COM1 capture was already
armed. As in G3-02, the capture contains an unexplained preliminary
`COLD_DEFAULT`/phase-1 start that ended silently after
`SPD54_TARGET_GATE=PASS`, before PCIEXBAR preflight or CSI arm. A second
`COLD_DEFAULT` start then completed phases 1, 2, and 3 plus both intended
CSI-produced reset continuations. The preliminary start is recorded but is
not counted as a separate G3 attempt or intended continuation.

Unlike G3-01 and G3-02, the saved and returned CPU A0 value was `00017400`;
the remaining crossed endpoint was unchanged:

```text
saved/pre/post CPU A0:   00017400 / 00017400 / 00017400
post-CSI CPU 9c:         00b00502
CSI byte/raw/canonical:  0c / 8b38506a / 908dabb6
QPI status:              070f0f03
MC 50/54:                0a000006 / 00000006
CSI return EAX/EBX/ECX:  0 / 0 / 11
```

The exact B06VG/B06VH coupled-profile predicate again rejected the combination
at `B06VG_COUPLED_HIGH_CSI_GATE` before MINIT and preserved the CAR ROMMON.
Passive `vinputs`/`vinfo` recorded CMOS `0e=ec`,
`MINIT_ATTEMPTED=00`, `MINIT_RETURNED=00`, `WORK_FNV=00000000`, and
`ROMMON_DIRTY=00`.

G3-03 therefore proves a second controlled-G3 safe fallback and a second
literal A0 form for this `b00502/0c/8b38506a` crossed endpoint. It does not
authorize widening the PRIMARY selector and provides no execution evidence
for MINIT, I801 return/re-arm, A/B/P/N workspaces, DRAM, CBMEM, postcar,
ramstage, or SeaBIOS.
