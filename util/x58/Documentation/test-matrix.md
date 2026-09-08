> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# Test matrix

B00 through B04 each have one user-reported successful hardware execution to
their intended terminal (`0xdf` for B00--B02, `0xde` for B03, and `0xdd` for
B04).  B05 has one user-reported execution ending at its exact `DEV_ERR`
terminal `0xfa`, not its `0xdc` success gate.  B06A has one user-reported
execution ending at `0x45`, its exact single-DDR3-responder-at-`0x54` success
terminal.  B06B has one user-reported execution ending at exact `0x64`, its
fixed-`0x54` base-SPD/CRC success terminal.  B06C has one user-reported
execution ending at exact header rejection `0x76`, not success `0x75`.
One B06H execution reached its encoded `0x93` terminal and produced a complete
COM1 transmit log.  B06I remains a reproducible static artifact.  B06J has
now completed an interactive hardware session proving UART receive, SPD
telemetry, bounded volatile PCI/PHY writes, RCOMP, both-rank ZQCL, and live
training-result mutation, but not a DDR training pass.  Its permanent
SerialICE stream has also completed externally driven common/channel PHY
profile trials, restored 65-point RD sweeps with both the rejected guessed
MSI bank-4 value and the corrected Intel no-bank-4 sequence, plus bounded
write-level/write-DQ-DQS probes.  None passed; SerialICE-QEMU itself remains
untested. B06V0 is built and locally composed but has no hardware execution;
its passive ROMMON, PCIEXBAR, CSI and MINIT milestones are intentionally
tracked separately. B06V1 adds sealed transactional register tables and is
host-tested/built only; its passive boot, read/assert table, and later
reversible-write milestone are also separate.
On B06V5, one CF9-full-reset continuation completed the MSI MINIT B4--B8 path
and returned to ROMMON with a programmed one-channel map. Cache-independent
DRAM reads/writes, a restored ten-address test, and a transactional A22--A31
test with verified reverse-order rollback all passed. Two further sealed
table pairs passed one-dword D0--D31 walking one and walking zero, restoring
the original after each half. This is one vendor-assisted session, not a
validated AC-cold boot, native raminit, ramstage, or a stability result.
Their repetition requirements remain open. B06V6 is built and locally
composed, but its automatic CSI/MINIT, full-window DRAM test, postcar,
ramstage, DRAM ROMMON and object-transfer path have no hardware execution yet.
B06V7 is a separate build-tested Slow-QPI variant that preserves the V6 path
but canonicalizes only the hardware-proven CSI byte and MINIT-workspace
ranges. Its local composite has no hardware execution yet.
B06V9 has one controlled-G3 hardware sequence. It completed CSI pass 1 and
its expected reset, returned and accepted the exact High-QPI endpoint on pass
2, and completed the outer IOH SYRE reset. Pass 3 stopped fail-closed before
another CSI call because its selector accepted only CPU `A0=00017600`, while
the retained endpoint was stable at `00017000`, the value also observed on the
vendor-booted High-QPI reference. One additional cold-like restart before
pass 1 remains unexplained. No MINIT, DRAM, ramstage or payload path ran.
B06VA is the build-tested successor. It changes only the duplicated read-only
CPU-A0 predicate to the exact observed set `{00017000,00017600}`, retains the
full High-QPI selector and all other gates, and remains terminal after the
observational third CSI call. One live sequence returned from pass 3, with
initial boot type and early phase trace still uncaptured. Four later B06VB G3
runs completed pass 1/pass 2, then failed closed before pass-3 CSI because A0
was `17a00/17800/17a00/17a00`, outside its two-value set; no MINIT ran. Four
controlled B06VC G3 runs completed pass 1/pass 2 and both reset
transitions 4/4. Pre-pass-3 A0 was `17c00/17a00/17c00/17000`. Runs 1/3 failed
closed before CSI; runs 2/4 returned ABI-clean from pass-3 CSI with `0/0/11`,
raw FNV `03e3d24e`, and state byte `0x2a6=08`. Run 4's complete post-CSI
platform tuple matched, but the state differed from saved B06VA only at
`0x2a6` (`08` versus `0c`), so the strict gate stopped before MINIT. Both
variants canonicalize to FNV `908dabb6`. No MINIT ran. Neither the five A0
observations nor the two CSI-state samples establish field semantics.
B06VD has two controlled-G3 hardware sequences. The first completed the
three-pass CSI path but failed closed on the unchanged exact post-CSI CPU
endpoint before MINIT. The second matched the exact High-QPI endpoint, accepted
CSI state byte `0x2a6=08` with raw/canonical FNVs
`03e3d24e`/`908dabb6`, and returned once from the authorized MINIT call with
workspace FNV `94299f43`. Subsequent ROMMON tests, under an effective UC MTRR
default, passed and rolled back a simultaneous 14-address alias table and both
halves of a D0--D31 walking-one table at one dword. Final key QPI/MC registers
were unchanged and six sampled RAS/ECC dwords were zero. This is one
vendor-assisted full-path return plus targeted volatile-memory evidence, not
the required cold-boot repetition, bulk-memory validation, native raminit, or a
ramstage/payload boot.
B06VE now has three retained hardware captures, but not three successful cold
repetitions. One stopped immediately on retained guard `CMOS0E=ec`. A second
completed CSI passes 1/2 and returned from pass 3, then failed the strict
pre-MINIT platform endpoint. The third reached the exact pass-3 CSI and
post-MINIT High-QPI/one-channel endpoint: CSI raw/canonical FNVs
`03e3d24e/908dabb6`, MINIT EAX zero, QPI `070f0f03`, IOH stage `bf000000`, and
the expected MC/channel tuple. Its workspace raw FNV was the newly observed
`b6346533`, while B06VE admitted only `94299f43`, so it failed closed before
POST `08`, all full-window tests, CBMEM, postcar, or DRAM ROMMON and retained
the CAR recovery monitor. Offline canonicalization maps both exact raw forms
to `a6f9c2e6`. From that post-MINIT CAR monitor, separate reversible scripts
proved the selected PAM bytes and representative C--F shadow points, including
exact future SeaBIOS words `f8800` and `fecd4`, with verified rollback. This is
PAM prerequisite evidence, not a B06VE DRAM handoff or payload execution.
B06VF admits only the two coupled raw
workspace forms plus canonical FNV `a6f9c2e6`, adds a complete 640-KiB
conventional-memory test, conservative resource tables, full 34-word C--F
shadow preflight, and a deliberately reduced SeaBIOS serial entry probe. It
has four retained G3-labelled hardware captures. All four completed CSI passes
1/2; two stopped before pass-3 CSI on newly observed A0 `17400`, while two
returned with preserved non-170 A0, CPU 9c `a00502`, and CSI
`0c/8b38506a`. Its stricter predecessor endpoint rejected both before MINIT.
No B06VF DRAM or payload path ran. B06VG is the built/static successor: it
keeps the exact `17000/b00502/08/03e3d24e` PRIMARY payload path, while an
exact non-170/A0-preserved/`a00502` OBSERVATION may perform one guarded MINIT
and must then terminate in CAR. Five G3-labelled B06VG attempts have now
reached pass 3: one live-only crossed state and one retained crossed state
were rejected before MINIT, one retained OBSERVATION returned from MINIT and
terminated as designed, and two retained PRIMARY runs returned from MINIT but
failed the inherited exact post-MINIT gate. The PRIMARY workspaces were new
forms P/N with raw/canonical FNVs `eb15c076/c313e060` and
`5fb636d9/b3fafec0`. Both also showed `I801_EXACT=00`; passive post-MINIT reads
in each run repeated `08:00:5d:01:a3`, demonstrating that the blob overwrote
the pre-call I801 marker while CMOS `ec` persisted. Separate manual CAR-ROMMON
scripts in the OBSERVATION run and both PRIMARY runs passed and rolled back a
14-address test across the two intended DRAM windows. Those manual tests are
not B06VG's automatic full-window tests. No B06VG attempt entered CBMEM,
postcar, ramstage, or SeaBIOS, so neither image yet satisfies the broader
`SB0` boot milestone.
B06VH is the built/static successor and now has one retained hardware
sequence. It promotes only B06VG PRIMARY, extends the exact workspace truth
table to the disjoint A/B/P/N raw/canonical/pattern forms, and rejects
OBSERVATION C. The capture began `COLD_DEFAULT` and retained both intended CSI
reset continuations, but the initial user power action and full G3 duration
were not independently captured. Pass 3 reached High-QPI `070f0f03` with A0
`17000`, CPU 9c `b00502`, and CSI `0c/8b38506a/908dabb6`. This crossed the
PRIMARY CPU endpoint with the other CSI profile, so the exact coupled gate
rejected it safely before MINIT. Passive follow-up showed CMOS `ec`, both
MINIT attempted/returned flags zero, and `ROMMON_DIRTY=0`. The A/B/P/N,
post-MINIT I801/re-arm, DRAM, CBMEM, postcar, ramstage, and SeaBIOS paths
therefore remain untested, and `SB0` is not advanced.
Superseding B06VH update: G3-02 is the first confirmed controlled G3 attempt.
The guard was verified at `2c`, AC was removed for 15 seconds by the Shelly's
local timer, the board remained in S5 after power restoration, and the user
started it with the physical button while serial remained armed. After an
unexplained preliminary phase-1 start stopped immediately after the SPD54
gate, a second start completed both intended CSI continuations and exactly
repeated the crossed `17000/b00502/0c/8b38506a/908dabb6` High-QPI fallback.
No MINIT ran. B06VH therefore has one confirmed controlled G3 attempt out of
ten, two total retained safe crossed-profile fallbacks, and no progress on
the post-MINIT or `SB0` milestones.
Second superseding B06VH update: controlled G3-03 repeated the preliminary
post-SPD phase-1 stop and then completed the full three-phase sequence. Its
crossed endpoint differed only in stable saved/pre/post A0 `17400`; CPU 9c,
CSI and QPI remained `b00502`, `0c/8b38506a/908dabb6`, and `070f0f03`.
The same exact gate rejected it before MINIT. Current B06VH count is 2/10
confirmed controlled G3 attempts, three retained safe pre-MINIT fallbacks,
and no post-MINIT or `SB0` progress.
Third superseding B06VH update: controlled G3-04 reached the distinct inherited
OBSERVATION path. After the recurring preliminary post-SPD start and a complete
three-phase sequence, pass 3 retained A0 `17c00`, CPU 9c `a00502`, CSI
`0c/8b38506a/908dabb6`, and High-QPI `070f0f03`. The exact classifier
authorized one diagnostic MINIT call, which returned EAX zero with post-MINIT
I801 `08:00:5d:01:a3`, workspace FNV `50f67315`, unchanged policy
`3c0f3a0b`, and explicit `DRAM_ACCESSES=0`. The mandatory OBSERVATION terminal
then returned to CAR without re-arm or ordinary DRAM. This corrects the prior
summary that every B06VH non-PRIMARY result was rejected before MINIT. Current
count is 3/10 controlled G3 attempts plus one earlier run of incomplete power
provenance; PRIMARY re-arm, DRAM and `SB0` remain untested.
Separate G3-04 manual follow-up: with effective UC default, the exact
mapper/common/channel/QPI/IOH endpoint was present and stable anchor reads at
16 and 32 MiB preceded a sealed 14-address write/assert transaction. All 14
assertions passed; one reverse-order rollback restored every recorded value,
and independent anchor reads confirmed restoration. QPI/common/channel status
was unchanged and six sampled RAS/ECC dwords remained zero. The final generic
runtime summary's `SPD_GATE=FAIL`/`PCIEXBAR_STEP=PENDING` is retained as a
mutable probe-state caveat after `ROMMON_DIRTY=1`, not used instead of the
direct gates and transactional evidence. This supports a narrowly gated O
profile in a later build, but is neither an automatic DRAM test nor a
full-memory, stability, training, ramstage, or payload result.
Another G3-04 manual follow-up exercised walking-one D0--D31 at the single
dword address `0x01000000`, split into two 16-write/16-assert transactions.
Both halves passed and rolled back in one attempt; the final anchor and the
QPI/common/channel endpoint were unchanged, and all six sampled RAS/ECC
dwords remained zero. A deliberately or accidentally wrong discard digest
was rejected before the exact digest succeeded, also exercising the monitor's
transaction binding. This remains a one-address data-bit test, not an
address-line, full-window, retention, stability, full-DIMM, or completed
training result.
B06VJ now has one retained hardware execution of unconfirmed initial power
provenance. It reproduced exact O-family A0/9c, CSI, policy and the expected
High-QPI/IOH/MC endpoint, then returned from guarded MINIT with EAX zero. The
workspace raw digest was a new `dd61cb51`, while its canonical digest remained
`0b161f01` and all 26 Q-specific marker bytes were unchanged. Offline byte
comparison against the admitted Q workspace found exactly 16 changes, all
inside the existing canonicalization ranges and none outside. Since B06VJ did
not enumerate the new raw digest, it rejected at the post-MINIT exact gate with
`DRAM_ACCESSES=00` and retained a clean CAR ROMMON. No post-memory test, CBMEM,
postcar, DRAM-backed ramstage or SeaBIOS ran; `SB0` remains unadvanced.
B06VK was subsequently exercised in seven retained hardware runs. One
OBSERVATION row returned from guarded MINIT but produced workspace
`0395518f/95abbb4b`, outside Q's canonical class. Five runs exposed repeated
cross-combinations of individually known CPU and CSI states and were rejected
before MINIT; the remaining run observed stable pre-CSI A0 `17200` and stopped
at the old A0 gate. No run performed an ordinary DRAM access or reached CBMEM,
postcar, ramstage, or SeaBIOS. B06VK's original six-row contract and build
record remain unchanged; the hardware campaign instead motivates a separate,
explicitly broader successor.
B06VL is that separately versioned, default-off successor. It admits the
finite seven-A0 by two-CPU-9c by two-CSI cross-product to one guarded MINIT
call, then treats both workspace digests as telemetry. All non-workspace
return predicates remain exact, and a new version-9/profile-7/bit-7 handoff can
exist only after the inherited complete UC low-memory, alias, two 8-MiB,
CBMEM, and handoff-readback sequence. B06VL-HW-02 subsequently passed this
complete path once and entered the deliberately reduced SeaBIOS payload. This
is not yet a confirmed-G3 repetition, full-DIMM, downstream-GPU, storage,
graphics, or operating-system boot result.
For B01, the verified machine code makes exact `8086:3a16` acceptance a
prerequisite for `df`; for B02 it also requires the narrow LPC, Fintek, and internal UART path
to pass.  The intended B03 machine code reaches `de` only after the CBFS/XIP
romstage path and its bounded UART/TEMT gate.  Full traces were not separately
reported.  `STATIC` means only that the relevant firmware evidence has been
inspected.  The user also reports
that the top-aligned factory image booted from the W25Q128 spare; a complete
external read-back/recovery log is still pending.

| ID | Milestone | Fixed configuration | Cold boots | Warm resets | Required evidence | Status |
|---|---|---|---:|---:|---|---|
| R0 | External recovery | exact spare flash | 0 confirmed cold; 1 vendor boot, type unknown | n/a | erase/write/read-back hash and recovery log | PARTIAL: VENDOR BOOT REPORTED; PROVENANCE PENDING |
| B00 | Reset/CAR/POST only; deliberate halt | E5645 stepping 2; no RAM dependency | 0 confirmed cold / 10; 1 terminal report, type unknown | 0 / 10 | `01,10,21,21,22,28,29,2b,2c,2f,c0,df`; ROM hash | TERMINAL REPORTED; COLD/REPETITION PENDING |
| B01 | Read-only ICH10R LPC ID probe; deliberate halt | same as B00 | 0 confirmed cold / 10; 1 terminal report, type unknown | 0 / 10 | `c0,c1,d0,df` success or stable `e1/e2`; ROM hash | TERMINAL REPORTED; COLD/REPETITION PENDING |
| B02 | Reset/CAR/narrow LPC/Fintek/UART; deliberate halt | E5645 stepping 2; no RAM dependency | 0 confirmed cold / 10; 1 terminal report, type unknown | 0 / 10 | `c0,c1,d0,c2,d1,d2,d3,d4,df`; internal UART loopback, serial build ID if wiring permits, ROM hash | TERMINAL REPORTED; COLD/REPETITION PENDING |
| RS0/B03 | CBFS XIP romstage entry; halt in CAR | same as B02 | 0 confirmed cold / 10; 1 terminal report, type unknown | 0 / 10 | `...d4,c3,d5,d6,de`; romstage banner if wiring permits; no post-CAR | TERMINAL REPORTED; COLD/REPETITION PENDING |
| ICH0/B04 | Existing ICH10R BAR helper plus SMBus BAR/enable-register programming; halt in CAR | same as B03; first test after full AC removal | 0 confirmed cold / 10; 1 terminal report, type unknown | 0 / 10, only after cold-state audit | terminal `dd` from verified hash stable for 60 s; guarded BAR/enable read-backs and invariants; serial PRE/POST if wired; no window access or SMBus transaction | TERMINAL REPORTED; COLD/STABILITY/REPETITION PENDING |
| SMB0/B05 | Exactly one I801 SPD memory-type byte-data read: `0x50:0x02` must return `0x0b` | E5645 CPUID `0x206c2`; retain a DIMM population known under vendor firmware to answer at `0x50`; true AC-off start | 0 confirmed cold / 10; 1 terminal report, type unknown | 0 / 10, only after state audit | verified full-image hash; complete trace through `c7,c8,c9,d8,dc`, stable `dc` for 60 s; serial status/data/poll count if wired | EXACT `FA` REPORTED ONCE; `DC`/COLD/STABILITY/REPETITION PENDING |
| SPD-DIAG/B06A | Record/set SMBCLK control, verify idle lines, then issue one bounded type-byte read at each `0x50..0x57` | exact E5645 signature; record the otherwise unchanged B05 DIMM population before flashing; true AC-off start | 0 confirmed cold / 10; 1 terminal report, type unknown | 0 / 10, only after state audit | raw 4-MiB SHA-256 `83100b39...06e930e`; terminal address result; complete POST trace and UART pin/status/poll/bitmap report if wired | EXACT `45` REPORTED ONCE; COLD/STABILITY/REPETITION PENDING |
| SPD-BASE/B06B | Read fixed `0x54` offsets `0x00..0x7f` exactly once and validate the DDR3 base-section CRC | exact E5645 signature; unchanged B06A responder configuration; true AC-off start | 0 confirmed cold / 10; 1 terminal report, type unknown | 0 / 10 | all 128 raw bytes; byte 2 `0x0b`; coverage 117 or 126 from byte 0 bit 7; CRC-16 `0x1021`, initial zero; little-endian bytes 126/127 match; exact failing offset/status | EXACT `64` REPORTED ONCE; COLD/STABILITY/REPETITION PENDING |
| SPD-POLICY/B06C | Fixed `0x54`: revalidate base CRC, require 176-used/256-total, double-read `0x80..0xaf`, decode, and derive a non-programmed DDR3-800 candidate | exact E5645 signature; unchanged B06B responder configuration; true AC-off start | 0 confirmed cold / 10; 1 terminal report, type unknown | 0 / 10 | raw SHA-256 `c9ef26c4...20cf35a3`; terminal `75`; complete trace; UART header/bytes/decoder tuple; exact failure | EXACT `76` REPORTED ONCE; HEADER ASSUMPTION REJECTED; COLD/REPETITION PENDING |
| SPD-HEADER/B06H | Repeat fixed-`0x54` base/CRC and encode byte-0 used/total fields without another SMBus command | exact E5645 signature; unchanged B06C population; true AC-off start | 0 confirmed cold / 10; 1 run, type unknown | 0 / 10 | full-image SHA-256 `cd5fca5b...2f23476`; trace through `62,7a`; terminal `0x80 | (B0 & 0x7f)`; UART raw B0 if received | EXACT `93` AND FULL SERIAL LOG REPORTED ONCE; COLD/REPETITION PENDING |
| SPD-FULL/B06I | Fixed `0x54`: base once, `0x80..0xff` twice, exact comparison, fingerprint, decoder, and non-programmed DDR3-800 candidate | exact E5645 signature and B06H DIMM population; true AC-off start | 0 / 10 | 0 / 10 | full-image SHA-256 `50cf4147...dbafd`; complete `62,a0..b7` trace; all 256 bytes and fingerprints; exact failure | BUILT/STATIC; NOT RUN |
| ROMMON/B06J | Interactive CAR monitor with locked human writes, on-demand B06I SPD, and SerialICE-v1.5 stream | exact E5645; COM1 115200 8N1; socketed recovery chip | boot type not independently confirmed; 1 continuing interactive session | 0 / 10 | `bc` prompt; `id/help`; full SPD; controlled PCI/PHY writes; external SerialICE controller; QPI stayed L0 | PARTIAL PASS — UART RX, monitor, SerialICE stream and restored PHY trials proven; no DRAM pass |
| PHY-ROMMON/B06K | B06J plus bounded X58 scan-chain read/write and fixed channel-2/rank-0 RD/RCVEN profiles | exact E5645; one BLS4G3D1609DS1S00. at SPD 0x54; full AC removal | 0 / 10 | 0 / 10 | full-image SHA-256 `4dd07478...9f7246b`; passive `phyr`; one armed `rdtry 0`; eight lane pairs; QPI remains L0 | BUILT/STATIC; B06K BINARY NOT RUN |
| EXACT-RD/B06L | B06K monitor plus opt-in, incomplete early reconstruction of MRS/ZQCL/coarse-RD | exact E5645; same single dual-rank DIMM at SPD 0x54; proven channel-2 base state; Slow-QPI `030f0f03`; full AC removal | 0 / 10 | 0 / 10 | full-image SHA-256 `ba1f2b01...35f71c4`; static comparison with corrected vendor order | BUILT TWICE/STATIC; NOT RUN; SUPERSEDED |
| BASEINIT-RD/B06M | B06L plus armed reproduction of the live-proven one-channel ratio-6 base state | exact E5645; same single dual-rank DIMM at SPD 0x54; reset-state gates; Slow-QPI `030f0f03`; full AC removal | 0 / 10 | 0 / 10 | full-image SHA-256 `56b8263b...4256b8`; complete BASE phase log; exact readbacks; isolated `rdexact 2`; QPI unchanged | BUILT TWICE/STATIC; NOT RUN |
| TIMED-MRS/B06N | B06M plus bounded UART-TEMT completion after each reset phase and each of eight MRS commands | exact E5645; same single dual-rank DIMM at SPD 0x54; reset-state gates; Slow-QPI `030f0f03`; full AC removal | 0 / 10 | 0 / 10 | full-image SHA-256 `1c1f60b9...b7e895c`; `MRS_GAPS=........`; complete BASE phase log; exact readbacks; QPI unchanged | BUILT THREE TIMES/STATIC; NOT RUN; DO NOT FLASH IN CURRENT STATE |
| VENDOR-PASSIVE/B06V0-P | Boot local composite to CAR ROMMON; execute only `id`, `resetcause`, `spd`, `vinfo` | exact E5645, microcode `1f`, one BLS4G3D1609DS1S00. and sole SPD `0x54`, ratio 6, Slow QPI, socketed recovery | 0 / 10 | 0 / 10 | programmer readback of pinned local 16 MiB SHA-256 `ba4ec305...f1dc96`; POST `bc`; SPD maps `10/10` and full-256 FNV; four code fingerprints; CAR layout/canaries; full serial log | BUILT TWICE/STATIC; LOCAL COMPOSITE VERIFIED; NOT RUN |
| VENDOR-MMCONFIG/B06V0-X | Separately armed `vprep SPD_FNV`; no vendor entry call | same exact B06V0-P configuration after a fresh cold boot and reviewed full-SPD digest | 1 / 10 using B06V2 | 0 / 10 | complete B06V0-P evidence; echoed SPD FNV; pre-mutation blob/CAR gate; PCIEXBAR PRE/POST; SAD/X58 identity; POST `d0`; `vinfo`; no `d1` | PASS ONCE — `fb66b530`, both status codes `00`, existing `e0000001` retained, `PCIEXBAR_STEP=PASS` |
| VENDOR-CSI/B06V0-C | Isolated original MSI CSI-wrapper call and complete state capture | same exact configuration; reviewed B06V0-P/X logs; external cold-reset path active | 1 / 10 using B06V3 | 1 CSI-produced reset/continuation | seed; POST ending `d1` or return `d2`; tuple; full `0x304` state/digest; canaries/high-water; port `70`/NMI; reset/no-return provenance | PASS ONCE AT ABI BOUNDARY — first `vcsi 00` reset; B06V3 accepted exact retained I801 tuple; second call returned `EAX/EBX/ECX=2/2/106`, FNV `c4eab5a4`, high-water `454`, intact guards; semantic CSI success not claimed |
| VENDOR-MINIT/B06V0-M | Direct MINIT call only after separately reviewed/accepted CSI state and hash-confirmed complete policy | same exact configuration; exact CSI capture; all `0xe0` corrected diagnostic-invalid policy bytes reviewed; installed digest read back | 1 / 10 using B06V4 | n/a | POST `d3`/possible `d4`; exact return stack; tuple; input/current policy FNV; complete `0x2bcc` workspace/digest; proof of no ordinary DRAM access | PARTIAL/NEGATIVE — returned `EAX=0`, gates `00/00/00`, workspace FNV `267b2858`, but `WS4F=06` and `COMPLETE_E79=00` prove a B3 early return; DRAM untrained |
| SCRIPT-PASSIVE/B06V1-P | Boot the local B06V1 composite to CAR ROMMON; execute only `id`, `resetcause`, `spd`, `vinfo`, `script status` | exact B06V0-P E5645/microcode/DIMM/Slow-QPI configuration and socketed recovery | 0 confirmed cold / 10; 1 run, type unknown | 0 / 10 | independently read-back full-chip hash; B06V1 ID; POST `bc`; prior SPD/blob/CAR gates; `OPS=00`, `SEALED=00`, no automatic access | PARTIAL PASS — ID, full SPD, CAR/runtime and empty script state observed; CSI guard exposed wrong compiled `e_lfanew`; flash readback/cold provenance pending |
| SCRIPT-READ/B06V1-R | Load, seal, run, trace, and discard only `b06v1-passive-state.xrs` | fresh cold B06V1-P pass; observed Slow-QPI/ratio-6 tuple must exactly match the script assertions | 0 confirmed cold / 10; 1 run, type unknown | 0 / 10 | both `PROGRAM_FNV=f9482511`; seven PRE/POST `done` records; `MUT=00`; exact `TXN_FNV`; complete trace before armed discard; final QPI tuple unchanged | PASS ONCE — seven operations done, `TXN_FNV=d7462413`, wrong-digest and unarmed discard rejected, exact armed discard passed |
| SCRIPT-ROLLBACK/B06V1-X | One isolated reversible write/mask followed by explicit reverse-order, read-back-verified rollback | only after B06V1-P/R logs; register and reversibility must have a documented hardware-specific justification | 0 confirmed cold / 10; 2 runs, type unknown | 0 / 10 | source `.xrs`; program/transaction digests; old/new/rollback values; `RB_TRIES`; `ROLLED_BACK=01`; independent post-state comparison | PASS ONCE EACH — 16550 SCR manual reverse-order rollback (`f432ec4d` to `d33d5e16`) and assert-failure auto rollback (`5112d646`) both restored `00`; repetition pending |
| CSI-GATE/B06V2-P | Boot corrected local B06V2 composite and perform only passive identity/SPD/vendor/script status checks | exact B06V1 hardware configuration; external write/verify; true AC-off start | 1 confirmed cold / 10 plus 1 run of unknown type | 0 / 10 | full-chip hash `e77f087a...66c4ea`; B06V2 serial ID; SPD gate; `SIG WRAPPER/CSI/MINIT=01/01/01`; `PROBE STATUS=ok CODE=00`; no dirty state or vendor call | PASS — repeated same-boot SPD/runtime gates plus one user-confirmed cold pass before successful `vprep`; programmer read-back pending |
| CSI-WARM/B06V3 | Reach ROMMON after the reset produced by `vcsi 00`, then repeat the passive gates and the separately armed CSI call | exact B06V2 configuration; externally verified B06V3 image; cold first pass must log `COLD_DEFAULT`; recovery path active | 1 / 10 | 1 CSI-produced reset/continuation | corrected B06V3 identity; exact pre-reset tuple; first `d1`; reset; `CONFIGURED_AFTER_RESET`; passive SPD/QPI state; second CSI return/reset boundary | PASS ONCE — exact retained I801 path reached ROMMON; second CSI returned with accepted `c4eab5a4` state; MINIT/DRAM not attempted |
| POLICY/B06V4 | Report both CMOS banks and `ALT_GP_SMI_EN`; generate the MSI diagnostic-invalid policy after accepted CSI state | exact B06V3 configuration; standard diagnostic `0e`, extended inputs and complete CSI result captured; no prior generic write | 1 reported cold / 10 | 1 CSI reset/continuation | B06V4 ID/hash; `vinputs`; complete `0xe0` policy dump/FNV; no CMOS data writes; reviewed candidate and installed digest | PASS ONCE — selector-controlled `0e=6c`, `8e=6c`, `ca=62`; candidate FNV `2e0ccce9` installed and read back; repetition pending |
| COLD-MINIT/B06V5 | Clear only policy status byte `0x0a` and status-derived flags bit 18 after the exact B06V4 candidate; observe the B4--B8 return boundary | exact B06V4 hardware; accepted CSI tuple `2/2/106`; candidate FNV `2e0ccce9`; cold-policy FNV `3c0f3a0b` | 0 confirmed AC-cold / 10; 1 CF9-full continuation | 1 CSI-produced reset/continuation | full-chip SHA-256 `e2a24406...63574cf`; POST `d3/d4`; `WS4F=02`; `COMPLETE_E79=01`; workspace FNV/SHA; MC mapper/status and channel-2 DOD/ranks | PASS ONCE — full dispatcher returned; workspace FNV `6d87f4e4`, MC mapper `24489`, status `1545`; AC-COLD/REPETITION PENDING |
| VENDOR-DRAM/B06V5-D | Cache-independent data/address smoke tests after the B06V5 vendor-assisted full-path return | same exact E5645 and one dual-rank 4-GiB DIMM at SPD `0x54`; MTRR default UC; all targets below TOLM | 0 confirmed AC-cold / 10; 1 CF9-full session | n/a | six distributed reads plus two repeats; four data patterns; ten simultaneous selected A2--A30 addresses; sealed A22--A31 table `1447f71f`; one-dword D0--D31 walking one (`08863aba`, `356d86da`) and walking zero (`b7db281a`, `2b2469ea`); exact rollback digests; restored originals; unchanged QPI/status and zero sampled RAS/ECC counters | PASS ONCE — address script and all four walking-data tables returned `RUN=ok`, exact rollback and discard; original restored; BULK/MULTI-ADDRESS-WALK/SOAK/AC-COLD/RAMSTAGE PENDING |
| AUTO-RAMSTAGE/B06V6 | Automatically replay only the exact B06V5 CSI/policy/MINIT result, test two full 8-MiB DRAM windows, leave CAR, load ramstage and enter DRAM ROMMON | exact E5645/microcode `1f`; sole known DIMM/SPD `0x54` FNV `fb66b530`; ratio 6; Slow QPI; spare socketed W25Q128 | 0 / 10 | 0 / 10; one CSI-produced reset expected per cold sequence | local full-chip SHA-256 `1898bdd5...ab970f`; exact first-pass reset and guarded second pass; `08,09,0a,0b,0c,0d,30,31,32,0e,0f`; full serial handoff/MTRR log; passive timer/NIC snapshot; optional small XRL1 upload/read-back | BUILT TWICE/STATIC; LOCAL COMPOSITE VERIFIED; NOT RUN |
| AUTO-ROBUST-SLOWQPI/B06V7 | Preserve B06V6 while accepting only the two proven Slow-QPI CSI byte-`2a6` variants and canonicalized successful MINIT workspaces | exact B06V6 E5645/microcode/SPD/ratio-6/Slow-QPI configuration; CSI byte `08` or `0c`; canonical CSI/workspace FNVs `8403ac98`/`7a34f363`; spare socketed W25Q128 | 0 / 10 | 0 / 10; one CSI-produced reset expected per cold sequence | local full-chip SHA-256 `91af11e8...83a815e`; raw+canonical serial telemetry; version-2 handoff; unchanged POST suffix `08,09,0a,0b,0c,0d,30,31,32,0e,0f`; full DRAM/MTRR log | BUILT/STATIC; V6 REGRESSION BUILT; LOCAL COMPOSITE VERIFIED; NOT RUN |
| HIGH-QPI-3PASS/B06V8 | Retired three-pass CSI probe with alleged exact upper-CMOS input gate | exact E5645/SPD/ratio-6 configuration | 0 confirmed AC-cold / 10; 1 manually powered run | 0 / 10 | B06V8 identity; RC/U128E; exact fallback reason; proof that no vendor call occurred | SAFE FALLBACK ONCE — CMOS0E authorization gate rejected; subsequent ROMMON inspection found RC=0, proving 72/73 aliased standard RTC; no CSI/MINIT; RETIRED |
| HIGH-QPI-DETERMINISTIC/B06V9 | Three-pass CSI probe with exact U128E enable and deterministic wrapper state | exact E5645/microcode `1f`/SPD `fb66b530`/ratio-6/Slow-QPI configuration; socketed W25Q128 | 1 controlled G3 sequence / 10 | 2 intended reset continuations observed once; 0 / 10 repetitions; plus 1 unexplained cold-like restart | local full-chip SHA-256 `0eeebf94...10af032`; raw log SHA-256 `443eb6cb...b549de`; RC PRE 0/4 and POST 4; pass-2 return `1/0/2a6`; exact High-QPI endpoint; terminal CAR ROMMON | PARTIAL PASS ONCE — pass 1 reset and pass 2 return/SYRE reset succeeded; pass 3 false-negative on CPU A0 `17000`; no pass-3 CSI/MINIT/DRAM |
| HIGH-QPI-A0-SET/B06VA | Repeat B06V9 and permit pass-3 selection for only the two observed CPU-A0 values | exact B06V9 E5645/microcode/SPD/ratio-6 configuration; A0 exactly `17000` or `17600`; socketed W25Q128 | 0 confirmed AC-cold / 10; 1 live sequence with initial type not captured | two reset continuations practically implied but early trace not captured; 0 / 10 formal repetitions | local full-chip SHA-256 `fb7a425a...1512a77`; terminal raw logs `1f2352a3...566b`, `7c9bd9aa...2501`; result `0/0/11`; CSI FNV `8b38506a`; complete CPU PHY vendor match; ten stable read groups; no MINIT | PASS ONCE WITH TRACE CAVEAT — real ABI-clean, non-no-op pass-3 CSI return and stable High-QPI; phase assignment practically unique but early `PHASE=03` markers missing; NO MINIT/DRAM |
| HIGH-QPI-MINIT-OBSERVE/B06VB | Admit the exact B06VA pass-3 observation and invoke MINIT once under a persistent reset-loop guard | exact E5645/microcode/SPD/ratio-6 configuration; CSI `0/0/11`, FNV `8b38506a`; socketed W25Q128 | 4 controlled G3 attempts / 10; 0 reached CSI3/MINIT | each completed the two intended reset continuations; 0 / 10 complete MINIT observations | base SHA-256 `2dc6aaa7...8b7254`; local full-chip `00a86646...7c22cc`; full serial; pass-3 A0 sequence `17a00/17800/17a00/17a00` | PASS1/PASS2 4/4; SAFE PRE-CSI3 FALLBACK 4/4 — only A0 missed exact two-value gate; NO CSI3/MINIT/DRAM |
| HIGH-QPI-A0-4SET-MINIT/B06VC | Retry the B06VB observation while accepting only the exact four observed pre-pass-3 A0 values | exact B06VB E5645/microcode/SPD/ratio-6 configuration; pre-pass-3 A0 exactly `17000/17600/17800/17a00`; post-CSI A0 still exact `17000`; socketed W25Q128 | 4 controlled G3 / 10; 2 reached CSI3, 0 reached MINIT | two intended CSI-produced continuations completed 4/4; 0 / 10 complete repetitions | full-chip `6c1ac571...e9c5af` (no read-back); four raw logs; pre-A0 `17c/17a/17c/170`; CSI3 return 2/4, `0/0/11`, raw FNV `03e3d24e`, canonical FNV `908dabb6` | PASS1/PASS2 4/4; SAFE PRE-CSI3 FALLBACK 2/4; ABI-CLEAN CSI3 RETURN THEN STRICT FALLBACK 2/4; NO `d3/d4`/MINIT/DRAM |
| HIGH-QPI-CSI2A6-MINIT/B06VD | Retry B06VC with the exact fifth pre-A0 value and only the two paired CSI-byte-2a6 state variants | same fixed E5645/microcode/SPD/ratio-6 configuration; pre-A0 exact five-value set; `08/03e3d24e` or `0c/8b38506a`; canonical FNV `908dabb6`; strict post-A0 `17000` | 2 controlled G3 / 10; 1 reached and returned from MINIT | two intended CSI-produced continuations completed 2/2 per G3 sequence; 0 / 10 full repetitions | full-chip `a2686f64...40806` (no programmer read-back); run-2 raw `baf59642...907f7e`; CSI SHA/FNV `0a10266b...13f199`/`03e3d24e`; workspace SHA/FNV `843dbc4e...be6833a`/`94299f43`; 14-address and D0--D31 reversible UC tests; final stable endpoint and six zero sampled RAS/ECC dwords | PASS ONCE — exact CSI and High-QPI endpoint admitted; MINIT full path returned; targeted UC DRAM tests passed with exact rollback; TERMINAL CAR; BULK/REPETITION/RAMSTAGE/PAYLOAD PENDING |
| HIGH-QPI-AUTO-RAMSTAGE/B06VE | Promote only the exact B06VD run-2 state through complete 16-MiB transition tests, CBMEM, postcar, and DRAM ROMMON | exact B06VD E5645/microcode/SPD/ratio-6 setup; CSI `08/03e3d24e/908dabb6`; workspace raw `94299f43`; QPI `070f0f03`; post-MINIT IOH stage `bf000000`; socketed W25Q128 | 3 retained start captures / 10; 2 entered pass 1, 1 returned from MINIT; independent cold provenance pending | two intended CSI reset continuations completed in each of the 2 full sequences; 0 / 10 formal repetitions | full-chip `71ddac31...33195f`; run-3 raw log `2ddb5bc2...f5047b17`; exact endpoint but workspace raw `b6346533`; expected post-memory `08,09,0a...0f` absent; CAR recovery prompt retained | PARTIAL PASS — EXACT CSI/MINIT ENDPOINT ONCE; NEW WORKSPACE REJECTED BEFORE POST-MEMORY; NO POSTCAR/DRAM-ROMMON; PAM/SHADOW PREFLIGHT PASSED SEPARATELY IN CAR |
| SEABIOS-ENTRY/B06VF | Admit only two coupled/canonical High-QPI workspaces, prove the bounded memory/resource/shadow path, and enter a reduced serial-only SeaBIOS probe | exact B06VE E5645/microcode/SPD/ratio-6 setup; workspace `94299f43` with `00:00:00:00/fe` or `b6346533` with `ff:ff:ff:ff/ff`; canonical FNV `a6f9c2e6`; exact High-QPI endpoint; socketed W25Q128 | 4 retained G3 captures / 10; G3-05 controlled long-off | two intended CSI reset continuations observed 4/4; 0 / 10 repetitions | local full-chip `7dfec8e9...b65317a`; G3-05 raw `352d63d2...80bd5b3`; pass-3 `0/0/11`, CSI `0c/8b38506a/908dabb6`, A0 `17800->17800`, CPU9c `a00502`; no MINIT | SAFE PRE-MINIT FALLBACK 4/4 — two pre-CSI on A0 `17400`, two post-CSI coupled endpoint rejections; NO DRAM/POSTCAR/RAMSTAGE/PAYLOAD |
| COUPLED-MINIT-SEABIOS/B06VG | Preserve the exact B06VF PRIMARY payload path while observing the newly proven non-170 coupled endpoint with exactly one guarded MINIT | same E5645/microcode `1f`/sole SPD54 `fb66b530`/ratio-6 setup; pre-A0 one of six literal values; exact preserved A0/CPU9c/CSI profile; socketed W25Q128 | 5 G3-labelled attempts / 10; G3-02..05 retained, G3-01 live-only; independent power provenance pending | two intended CSI reset continuations reached pass 3 in 5/5 by observation, fully retained in 4/5; 0 / 10 repetitions | full-chip `2dcc944f...5b8a01d`; G3-04/G3-05 finals `50f410ef...0d8544`/`2821f5c4...8d404`; P/N workspace raw/canonical `eb15c076/c313e060`, `5fb636d9/b3fafec0`; repeated I801 `08:00:5d:01:a3`; manual 14-address tests passed with rollback | PARTIAL PASS — exact classifier outcomes; MINIT returned in 1 OBSERVATION and 2 PRIMARY runs; all automatic paths fail-closed in CAR; NO AUTOMATIC DRAM/CBMEM/POSTCAR/RAMSTAGE/PAYLOAD |
| PRIMARY-REARM-SEABIOS/B06VH | Promote only exact B06VG PRIMARY after one of four coupled A/B/P/N workspace forms; require measured post-MINIT I801 and re-arm persistence only after all gates; then attempt the bounded v5 post-memory/SeaBIOS path | same E5645/microcode `1f`/sole SPD54 `fb66b530`/ratio-6 setup; PRIMARY `17000/b00502`, CSI `08/03e3d24e/908dabb6`; workspace A/B/P/N exact pattern; socketed W25Q128 | 0 confirmed cold / 10; 1 `COLD_DEFAULT`-labelled sequence with incomplete initial power provenance | two intended CSI-produced reset continuations observed once; 0 / 10 formal repetitions | full-chip artifact `97a69436...55a452` without programmer read-back; raw capture `db83af5c...7d361`; pass-3 High-QPI with crossed `17000/b00502/0c/8b38506a/908dabb6`; passive MINIT flags and ROMMON dirty state zero | SAFE PRE-MINIT FALLBACK ONCE — image and both CSI resets exercised; crossed profile rejected as designed; NO MINIT/I801 REARM/DRAM/CBMEM/POSTCAR/RAMSTAGE/PAYLOAD |
| PRIMARY-REARM-SEABIOS/B06VH-G3-02 | Superseding current B06VH count; same exact promotion experiment | same fixed B06VH target; guard `2c`; Shelly-controlled 15-second G3 removal; physical start from S5; serial armed throughout | 1 confirmed controlled G3 / 10; 2 retained executions total; one unexplained preliminary phase-1 restart within G3-02 is not counted separately | two intended CSI-produced reset continuations completed in G3-02; 0 / 10 complete promotion repetitions | raw capture `918e0db5...79c9b6d` (20613 bytes); repeated High-QPI crossed `17000/b00502/0c/8b38506a/908dabb6`; passive CMOS `ec`, MINIT flags/workspace/dirty all zero; no programmer read-back | SAFE PRE-MINIT FALLBACK REPRODUCED — CONTROLLED G3 1/10; TWO TOTAL CROSSED FALLBACKS; NO MINIT/I801 REARM/DRAM/CBMEM/POSTCAR/RAMSTAGE/PAYLOAD |
| PRIMARY-REARM-SEABIOS/B06VH-G3-03 | Superseding current B06VH count; same exact promotion experiment | same fixed B06VH target; guard `2c`; preceding Shelly-controlled 15-second G3 removal; physical start from S5; serial armed | 2 confirmed controlled G3 / 10; 3 retained executions total; G3-03 preliminary post-SPD restart not counted separately | two intended CSI-produced reset continuations completed in G3-03; 0 / 10 complete promotion repetitions | raw capture `a27d6dee...8c104` (20614 bytes); High-QPI crossed `17400/b00502/0c/8b38506a/908dabb6`; passive CMOS `ec`, MINIT flags/workspace/dirty all zero; no programmer read-back | SAFE PRE-MINIT FALLBACK REPRODUCED — CONTROLLED G3 2/10; THREE TOTAL CROSSED FALLBACKS; NO MINIT/I801 REARM/DRAM/CBMEM/POSTCAR/RAMSTAGE/PAYLOAD |
| PRIMARY-REARM-SEABIOS/B06VH-G3-04 | Superseding current B06VH count; exercise inherited OBSERVATION diagnostic MINIT terminal without PRIMARY promotion | same fixed B06VH target; guard `2c`; Shelly-controlled 60-second G3 removal; physical start from S5; serial armed | 3 confirmed controlled G3 / 10 plus 1 earlier run with incomplete power provenance; recurring preliminary post-SPD restart not counted separately | two intended CSI-produced reset continuations completed in G3-04; 1 OBSERVATION MINIT return; 0 / 10 PRIMARY promotions | raw capture `996117eb...538621` (74807 bytes); `17c00/a00502/0c/8b38506a/908dabb6`; MINIT EAX 0; post-I801 `08:00:5d:01:a3`; workspace `50f67315`; policy `3c0f3a0b`; `DRAM_ACCESSES=0` | OBSERVATION MINIT RETURN ONCE — TERMINAL CAR BY DESIGN; NO I801 REARM/ORDINARY-DRAM TEST/CBMEM/POSTCAR/RAMSTAGE/PAYLOAD; DRAM NOT CLAIMED TRAINED |
| PRIMARY-REARM-SEABIOS/B06VH-G3-04-UC14 | Separate manual selected-address UC experiment after G3-04 OBSERVATION terminal | same retained CAR session; MTRR default `0000000000000800`; exact mapper/common/channel/QPI/IOH endpoint | no additional boot count | one 14-write/14-assert transaction; one reverse rollback | raw capture `66da964b...6764` (27470 bytes); program `0a85f9f6`; run txn `fc7d3978`; rollback txn `468d79b8`; `RB_TRIES=1`, `ROLLED_BACK=1`; anchors restored; endpoint stable; six sampled RAS/ECC dwords zero | SELECTED-ADDRESS UC TEST AND ROLLBACK PASS — SUPPORTS FUTURE EXACT O-PROFILE AUTOMATIC TRIAL; NO FULL-WINDOW/FULL-DIMM/STABILITY/TRAINING/RAMSTAGE/PAYLOAD CLAIM |
| PRIMARY-REARM-SEABIOS/B06VH-G3-04-WALK1 | Separate manual one-address walking-one D0--D31 test after G3-04 OBSERVATION terminal | same retained UC CAR session; address `0x01000000`; exact endpoint retained | no additional boot count | D0--D15 and D16--D31 each: 16 write/assert pairs plus one reverse rollback | raw capture `c72f0800...ee3a4` (40659 bytes); programs `08863aba`/`356d86da`; run txns `62282405`/`568a2f65`; rollback txns `9ec9d1e2`/`29046832`; both `ROLLED_BACK=1`; wrong discard `8f2a4be6` safely rejected; anchor restored | ONE-ADDRESS WALKING-ONE D0--D31 AND ROLLBACK PASS — NO ADDRESS-LINE/FULL-WINDOW/RETENTION/STABILITY/FULL-DIMM/TRAINING/RAMSTAGE/PAYLOAD CLAIM |
| COUPLED-PROFILE-O-SEABIOS/B06VI | Preserve B06VH PRIMARY and promote only the complete G3-04 O row through bounded post-memory checks, v6 handoff, postcar, ramstage, and reduced serial SeaBIOS | E5645/CPUID `206c2`/microcode `1f`; sole SPD54 `fb66b530`; ratio 6; exact O `17c00/a00502`, CSI `0c/8b38506a/908dabb6`, workspace `50f67315/92c70df4`; socketed W25Q128 | 0 / 10 | 0 / 10; two CSI-produced reset continuations expected per sequence | base `745ea874...004752`; local full-chip `738b5a67...f28d8`; 157/157 static tests; 8/8 VI contract; two byte-identical clean builds; W25Q128 lower 12 MiB erased and upper 4 MiB exact; no programmer read-back | BUILT TWICE / STATIC CONTRACT AND LOCAL COMPOSITE VERIFIED / NOT RUN — RAM provisionally assumed stable for this bounded attempt; no final stability, graphics, storage, network, OS-boot, or general first-boot claim |
| COUPLED-PROFILE-O-SEABIOS/B06VI-HW-01 | First B06VI hardware execution; admit only an indivisible PRIMARY or O row | same fixed E5645/microcode/SPD/ratio-6 target; user-reported active build; initial G3 and programmer-read-back provenance not independently captured | 0 confirmed controlled G3 / 10; 1 COLD_DEFAULT-labelled execution plus one unexplained preliminary phase-1 restart | both intended CSI reset continuations completed once; 0 / 10 exact-profile promotions | raw capture `82f986fc...52b77e` (21547 bytes); phase-3 `17000/b00502` crossed with CSI `0c/8b38506a/908dabb6`; QPI `070f0f03`; passive MINIT flags/workspace/dirty all zero | SAFE PRE-MINIT FALLBACK ONCE — CROSSED ROW REJECTED AS DESIGNED; NO MINIT/DRAM/CBMEM/POSTCAR/RAMSTAGE/PAYLOAD; subsequent controlled 60-second G3 stopped in S5 pending physical power-button start |
| COUPLED-PROFILE-O-SEABIOS/B06VI-HW-G3-02 | Controlled-G3 continuation; require exact O and the one admitted O workspace before any DRAM access | same fixed E5645/microcode/SPD/ratio-6 target; Shelly 60-second AC removal, restore into S5, physical start; exact O `17c00/a00502` and CSI `0c/8b38506a/908dabb6` | 1 confirmed controlled G3 / 10; 2 B06VI executions total; recurring preliminary phase-1 restart not counted separately | both CSI-produced reset continuations completed; exact O selected; MINIT returned once; 0 / 10 post-memory promotions | raw capture `69cfca61...d222ff` (69300 bytes); policy `3c0f3a0b`; return I801 `08:00:5d:01:a3`; new workspace raw/canonical/SHA `69b4c386/0b161f01/6c29393a...eb529`; exact QPI/IOH/MC endpoint; full read-only dump ended `ROMMON_DIRTY=00` | PARTIAL PASS — EXACT O AND REAL GUARDED MINIT RETURN; NEW COMPLETE WORKSPACE REJECTED AS DESIGNED AT POST-MINIT EXACT GATE; `DRAM_ACCESSES=00`; NO CBMEM/POSTCAR/RAMSTAGE/PAYLOAD; guard later cleared `ec->2c` |
| COUPLED-PROFILE-Q-SEABIOS/B06VJ | Preserve B06VI's five exact rows and add only the complete controlled-G3 Q/O2 workspace as profile ID 6; attempt bounded post-memory checks and reduced SeaBIOS only after the complete tuple recurs | same fixed E5645/microcode/SPD/ratio-6 target; exact O-family CPU/CSI endpoint; Q workspace `69b4c386/0b161f01` plus 26 exact markers; socketed W25Q128 | 0 / 10 | 0 / 10; two CSI-produced reset continuations expected per sequence | base `246b1419...a5f9eaa`; local full-chip `f1b28231...7805c`; 163/163 full and 14/14 focused tests; repeated byte-identical clean builds; B06VI regression build; payload bytes and W25Q128 top placement verified; no programmer read-back | BUILT REPEATEDLY / STATIC CONTRACT, PAYLOAD, AND LOCAL COMPOSITE VERIFIED / NOT RUN — RAM remains only provisionally assumed stable; no graphics, storage, network, OS-boot, or general first-boot claim |
| COUPLED-PROFILE-Q-SEABIOS/B06VJ-HW-01 | First B06VJ hardware execution; require exact Q raw/canonical/marker tuple before any ordinary DRAM access | same fixed E5645/microcode/SPD/ratio-6 target; exact O-family `17c00/a00502`, CSI `0c/8b38506a/908dabb6`; expected local W25Q128 artifact, no programmer read-back | 0 confirmed controlled G3 / 10; 1 retained execution of unconfirmed initial power type; recurring preliminary phase-1 restart not counted separately | both intended CSI-produced reset continuations completed once; exact O-family selected; MINIT returned once; 0 / 10 post-memory promotions | raw slice `088740f2...dfbf` (68509 bytes; opening `[` cropped); policy `3c0f3a0b`; return I801 `08:00:5d:01:a3`; workspace raw/canonical/SHA `dd61cb51/0b161f01/370ca236...845cef`; exact 26-byte Q markers unchanged; 16 byte changes versus Q, all canonicalized | PARTIAL PASS — EXACT O-FAMILY AND GUARDED MINIT RETURN; NON-ENUMERATED RAW WORKSPACE REJECTED AT COUPLED EXACT GATE; `DRAM_ACCESSES=00`; CLEAN CAR ROMMON; NO POSTMEM/CBMEM/POSTCAR/RAMSTAGE/SEABIOS; guard later cleared `ec->2c` |
| Q-CANONICAL-CLASS-SEABIOS/B06VK | Preserve all six coupled rows; for Q/ID 6 only, retain raw workspace FNV as telemetry and require exact canonical FNV plus all markers and remaining gates before bounded UC memory tests and a possible v8 handoff | fixed E5645/CPUID `206c2`/microcode `1f`; sole SPD54 `fb66b530`; ratio 6; exact Q `17c00/a00502`, CSI `0c/8b38506a/908dabb6`, workspace canonical `0b161f01` plus 26 markers; O/PRIMARY raw-exact; socketed W25Q128 | build record only; hardware counts in the next row | build record only; hardware counts in the next row | base `594ff8ed...1e53d`; local full-chip `51db5981...d3864`; handoff v8; 170/170 full and 21/21 focused tests; B06VI/VJ regressions; repeated byte-identical clean builds; payload and top placement verified; no programmer read-back | BUILT REPEATEDLY / STATIC CONTRACT, PAYLOAD, REGRESSIONS, AND LOCAL COMPOSITE VERIFIED — subsequently run seven times; see B06VK-HW-01..07; NO CBMEM/POSTCAR/RAMSTAGE/SEABIOS CLAIM |
| Q-CANONICAL-CLASS-SEABIOS/B06VK-HW-01..07 | Exercise B06VK's coupled classifier and Q canonical-class gate without weakening the flashed image | same fixed E5645/microcode/SPD/ratio-6 target; expected local W25Q128 artifact; no programmer read-back | 3 controlled G3 starts plus 4 one-second relay interruptions / 10 | two CSI-produced continuations reached pass 3 in HW-01..05 and HW-07; HW-06 stopped before pass-3 CSI; one MINIT return; 0 promotions | seven immutable raw captures; HW-01 `17a00/a00502`, CSI `08/03e3d24e`, workspace `0395518f/95abbb4b`; HW-02..04 `17600/b00502`, CSI `08`; HW-05/07 `17000/b00502`, CSI `0c`; HW-06 pre-CSI `17200`; exact hashes in the bring-up log | SAFE CAR TERMINAL 7/7 — ONE OBSERVATION MINIT RETURN, FIVE CROSSED-ROW PRE-MINIT REJECTIONS, ONE PRE-CSI A0 REJECTION; `DRAM_ACCESSES=00`; NO CBMEM/POSTCAR/RAMSTAGE/SEABIOS |
| BROAD-HARD-GATE-SEABIOS/B06VL | Admit seven literal A0 values × two observed CPU-9c values × two exact CSI forms to one guarded MINIT call; promote only an independently hard-gated complete return through real UC DRAM tests and a v9 handoff | fixed E5645/CPUID `206c2`/microcode `1f`; sole SPD54 `fb66b530`; ratio 6; exact CSI `08/03e3d24e` or `0c/8b38506a`, canonical `908dabb6`; socketed W25Q128; workspace hashes telemetry-only | build record only; hardware count in next row | build record only; two CSI-produced continuations expected per sequence | base `dd956741...7a58`; local full-chip `9f8e1085...f5e7`; handoff v9/profile 7/required bit 7; 178/178 full and 29/29 focused tests; three byte-identical clean builds; composite, wrapper, payload ELF and top placement verified; no programmer read-back | BUILT REPEATEDLY / STATIC CONTRACT, PAYLOAD, REGRESSIONS, AND LOCAL COMPOSITE VERIFIED — subsequently executed once; see B06VL-HW-02 |
| BROAD-HARD-GATE-SEABIOS/B06VL-HW-02 | Execute the complete hard-gated B06VL path through destructive UC tests, v9 handoff, postcar, ramstage and the reduced SeaBIOS terminal | same fixed E5645/microcode/SPD/ratio-6 target; expected local W25Q128 artifact; no programmer read-back | 0 confirmed electrical G3 / 10; 1 Shelly-controlled one-second interruption | two intended CSI/IOH continuation resets completed; 1 complete payload path / 10 | readable log `61bcab5a...9a93`; A0/9c `17c00/a00502`; CSI `08/03e3d24e/908dabb6`; MINIT EAX 0; lowmem, alias, both 8-MiB windows PASS; handoff `7cc03894`; real postcar/ramstage; SeaBIOS 1.17.0; 65 bus-0 PCI functions; terminal reached | PASS ONCE — REAL COREBOOT POSTCAR + RAMSTAGE + TABLES + REDUCED SEABIOS; NO CONFIRMED G3/REPETITION/FULL-DIMM/DOWNSTREAM GPU/STORAGE/GRAPHICS/OS BOOT |
| AUTO-PCI-VGA-IPXE/B06VM | Continue automatically after the exact B06VL v9 handoff; enumerate and allocate only the fixed GPU/NIC/USB/SATA topology; execute a physical VGA ROM and offer SeaBIOS local/iPXE boot | E5645/CPUID `206c2`/microcode `1f`; sole BLS4G3D1609DS1S00 at SPD54; actual AMD HD 5450 differed from the fixed `10de:0f00` policy; exact `10ec:8168` rev02; socketed W25Q128; COM1 115200 | 0 confirmed electrical G3 / 10; one retained one-second relay run after explicit guard clear | inherited CSI resets, MINIT, destructive windows, CBMEM, postcar and ramstage passed once; 0 payload entries | base `26d2cb75...1869`; local full-chip `641063b6...f6b`; `00:03.0->[01]` but `01:00.0=ffff:ffff`; `00:1c.4->[02]` and RTL8168 present; terminal POST38; immutable captures/hashes in B06VM analysis | PARTIAL PASS — REAL RAMSTAGE AND SELECTIVE DOWNSTREAM PCI; NIC ENUMERATED; GPU LINK/ENDPOINT ABSENT; SEABIOS NOT ENTERED; NO VGA/STORAGE/IPXE/OS BOOT |
| IOU0-HD5450-PHYSVBIOS/B06VN | Preserve B06VM but start only X58 IOU0/`00:03.0` with documented x16+Start value; measure bounded link state; accept any runtime-observed AMD VGA and let SeaBIOS run the card's physical VBIOS | same fixed E5645/microcode/SPD54 setup; AMD Radeon HD 5450 with onboard VBIOS; RTL8168 rev02; socketed W25Q128; COM1 115200 | build record only; hardware counts in following rows | build record only | base `6cdce01c...268a`; local full-chip `5f1c945a...5e45`; two byte-identical clean builds; 202/202 release tests, later 209/209 repository tests; exact single `movw 0x000c -> e0018190`; CBFS no AMD ROM; independent audit pass; no programmer read-back | BUILT / REPRODUCIBLE / INDEPENDENT AUDIT PASS — subsequently reached boot-enabled SeaBIOS and iPXE twice; see B06VN-HW-02/HW-04 |
| IOU0-HD5450-PHYSVBIOS/B06VN-HW-02 | Measure IOU0 and continue through boot-enabled SeaBIOS even if the AMD endpoint remains absent | same fixed target; expected full-chip artifact without programmer read-back; guard explicitly cleared; one-second Shelly interruption; electrical G3 not independently proven | 0 confirmed electrical G3 / 10; 1 complete payload execution plus preliminary/diagnostic retries | two intended CSI resets completed; MINIT/full UC windows/CBMEM/postcar/ramstage/SeaBIOS/iPXE entry 1 / 10 | `17800/a00502`; root `00:03.0` Gen1 x16 `DLLA=1 LT=0`, start write skipped; GPU unreadable; RTL8168 `02:00.0`; SeaBIOS found 98 functions including 32 bus-2 X58 aliases; capture ended after first logged INT16; hashes in B06VN analysis | MAJOR PARTIAL PASS ONCE — BOOT-ENABLED SEABIOS AND IPXE OPTION ROM EXECUTED; PHYSICAL PEG LINK ACTIVE; GPU/VBIOS, NETWORK LINK/DHCP/TFTP/STORAGE/OS BOOT NOT PROVEN; IOHBUSNO ROUTING DEFECT EXPOSED |
| IOU0-HD5450-PHYSVBIOS/B06VN-HW-03/HW-04 | Hardened exclusive repeat: verify fail-closed A0 handling, then independently reproduce the complete payload path without HW-02's large replay prefix | same fixed target; expected full-chip artifact without programmer read-back; one-second Shelly interruptions; operator-reported guard clear between attempts; electrical G3 not independently proven | 0 confirmed electrical G3 / 10; 2 attempts: one safe fallback and one payload execution | HW-03 remained pre-MINIT on `16e00`; HW-04 completed both CSI resets, MINIT/full UC windows/CBMEM/postcar/ramstage/SeaBIOS/iPXE; B06VN payload total now 2 / 10 | HW-04 raw `538b9294...e5096d2`, 68088 bytes/1180.928 s, aggregate rate envelope passed; `17800/a00502`; root03 Gen1 x16; GPU unreadable; 98 PCI functions/32 aliases; capture ended after first logged INT16; immediate Ctrl-B/Enter silence is an unretained operator observation | HW-03 SAFE FAIL-CLOSED; HW-04 MAJOR PARTIAL PASS — PAYLOAD RESULT INDEPENDENTLY REPEATED WITHOUT THE LARGE REPLAY PREFIX; PRE-CALL STATE STILL NONDETERMINISTIC; VGA/NETWORK/STORAGE/OS BOOT PENDING |
| IOHBUSNO-ROUTE-PROBE/B06VO | Preserve B06VN; repair only X58 IOHBUSNO to exact vendor-observed bus-0-valid state before IOU0 start/PCI scan; compare CF8 and direct-PCIEXBAR endpoint visibility at bounded 0/1/10/100-ms snapshots | same fixed E5645/microcode/SPD54 target; AMD Radeon HD 5450 physical VBIOS; RTL8168 rev02; socketed W25Q128; COM1 115200 | build record only; hardware counts in following rows | build record only | base `5bb953c3...0b3c8`; local full-chip `01427d62...426c`; two byte-identical clean builds; 216/216 full and 60/60 focused tests; exact conditional `movw 0100 -> e00a010a`; B06VN regression hash exact; CBFS/composite/top-placement/checkpatch pass; no programmer read-back | BUILT / BYTE-REPRODUCIBLE / STATIC CONTRACT AND LOCAL COMPOSITE VERIFIED — SUBSEQUENTLY PASSED IOH ROUTING, RADEON, PHYSICAL VBIOS AND DISPLAY TWICE; SEE HW-02/HW-04 |
| IOHBUSNO-ROUTE-PROBE/B06VO-HW-02 | Test whether exact IOHBUSNO bus-0-valid routing removes X58 aliases and exposes the already-linked HD 5450, then let SeaBIOS execute its physical VBIOS | same fixed E5645/microcode/SPD54/HD5450/RTL8168 target; expected local full-chip artifact without programmer read-back; one-second Shelly interruption; electrical G3 not independently proven | 0 confirmed electrical G3 / 10; 1 complete B06VO execution / 10 | both CSI resets, MINIT, complete UC windows, CBMEM, postcar, ramstage, PCI, SeaBIOS, physical VBIOS and iPXE option-ROM entry 1 / 10 | raw `f03ada25...9459`, 66204 bytes/1155.823 s; A0/9c `17600/b00502`; IOHBUSNO `0000->0100`; sampled aliases immediately `ffffffff`; root03 Gen1 x16; four CF8/ECAM reads `68f91002`; 68 SeaBIOS functions with no known bus-2 aliases and two Radeon functions; physical 59904-byte VBIOS entered/returned; display operator-confirmed; capture marker first INT16 | MAJOR PASS ONCE — IOH ROUTING, PEG ENDPOINT, VGA RESOURCE/FORWARDING, PHYSICAL VBIOS AND DISPLAY SUCCEEDED; SEABIOS/IPXE REACHED; LONGER POST-INT16 CAPTURE, REPETITION, NETWORK, STORAGE AND OS BOOT PENDING |
| IOHBUSNO-ROUTE-PROBE/B06VO-HW-04 | Independently repeat the complete B06VO display path and retain a long trace beyond iPXE's first INT16 boundary | same fixed E5645/microcode/SPD54/HD5450/RTL8168 target; expected local full-chip artifact without programmer read-back; exact one-second Shelly interruption; electrical G3 not independently proven | 0 confirmed electrical G3 / 10; B06VO complete executions total 2 / 10 | both CSI resets, MINIT, complete UC windows, CBMEM, postcar, ramstage, PCI, SeaBIOS, physical VBIOS and iPXE option-ROM entry repeated; one retained run had no progress after first INT16 for >=120 s | raw `295f10c5...04ae`, 67230 bytes; zero quiet-gate discard; A0/9c `17c00/a00502`; IOHBUSNO `0000->0100`; aliases absent; all Radeon reads `68f91002`; 68 functions; physical VBIOS returned; supplied photo shows SeaBIOS/iPXE/Ctrl-B prompt; RLE `2449d215...891`; deliberate SIGINT provenance; zero-byte Ctrl-B probe | MAJOR REPEAT PASS THROUGH DISPLAY / ONE PERSISTENT POST-VBIOS UART-SILENCE OBSERVATION — MISSING LAPIC LINT0 EXTINT IS LEADING INFERENCE; NETWORK, STORAGE AND OS BOOT PENDING |
| BSP-LAPIC-EXTINT/B06VP | Preserve the vendor-assisted B06VO path and its bounded RAM tests; during the sole CPU-cluster init validate BSP xAPIC state, log TPR/SVR/LVT0/LVT1, invoke upstream virtual-wire setup once, require exact selected-field readback, then test real payload timer progress | same fixed E5645/microcode/SPD54/HD5450/RTL8168 target; SMP off; PCAT 8259 and SeaBIOS hardware IRQ on; prompted iPXE retained; socketed W25Q128; COM1 115200 | build record plus 1 complete type-unconfirmed hardware run / 10 | two intended CSI continuations, MINIT, RAM/CBMEM/postcar/ramstage/SeaBIOS/iPXE and netboot.xyz path 1 / 10 | base `2671fd32...d551`; local full-chip `e7211196...71b`; build audits pass; HW-03 raw `6659101e...e18`, 1,344,022 bytes; LAPIC post `SVR=10f LVT0=700 LVT1=400`; RTL8168 link, DHCP `192.0.2.196`, TFTP 388848-byte NBP, netboot.xyz v3 menu; no USB descriptor, i8042 `ff` timeout | MAJOR PASS ONCE — LAPIC GATE, TIMER PROGRESS, VGA, ETHERNET, DHCP, TFTP AND NETBOOT MENU; USB/PS2, SATA/STORAGE, OS BOOT AND 10-RUN STABILITY PENDING |
| PIC-ELCR-TCO-CAR/B06VP-LIVE | Measure the retained pre-QPI 8259/ELCR/TCO state; prove only `TCO1_CNT.TCO_TMR_HLT` reversibly under exact gates and restore it automatically | retained B06VP CAR ROMMON; no additional boot; same fixed target; COM1 115200 | no additional boot count | three sealed scripts: two read-only and one reversible write/rollback | PIC IMR `00/00`; ELCR `00/00`; GCS `00200404`; TCO status `0008/0002`; running RLD `4/2/4`; halted RLD `2/2/2`; rollback exact to `TCO1_CNT=0000`; six hashed captures | LIVE CONTROLLED PASS — ACTIVE WATCHDOG AND HALT EFFECT PROVED; EXACT CLEANUP PASSED; PIC/ELCR BASELINE IS NOT PAYLOAD-SAFE; PERSISTENT TCO HALT AND 8259 INIT REMAIN SEPARATE BUILDS |
| PIC-PIT-CAR/B06VP-LIVE | Apply the standard dual-8259 vectors/masks plus IRQ9 level-trigger policy, then prove PIT0 reaches master-PIC IRR0 while IRQ0 remains masked and restore the PIC state | retained B06VP pre-QPI CAR ROMMON; no additional boot; COM1 115200; LAPIC software-disabled/LINT0 masked; IF remains clear | no additional boot count | four sealed scripts; exact 8259/ELCR setup, read-only APIC census, bounded PIT one-shot and PIC cleanup | IMR `00/00 -> fb/ff`; ELCR `00/00 -> 00/02`; PIT mode0/count0800; master IRR0 `0 -> 1` on first poll; cleanup IRR0=0 and masks exact; eleven hashed captures | LIVE CONTROLLED PASS — PIT0->8259 IRQ0 DELIVERY PROVED; CPU EXTINT ACCEPTANCE NOT CLAIMED; COMPILED TEMPORARY-IDT HANDLER REQUIRED FOR FINAL PROBE |
| ICH10-EHCI-INIT/B06VQ | Preserve B06VP; before PCI scan apply only Intel ICH10 EHCIIR2 required fields to both EHCI functions with masked RMW and exact full-dword readback; log bounded read-only EHCI/UHCI/ownership/port/power-routing state before SeaBIOS | same fixed target; one pre-attached rear-port USB keyboard, then a high-speed USB stick on a separate run; full ICH10 driver disabled | two clean deterministic builds and composite/top-placement audits / 0 hardware runs / 10 | 0 / 10 | public base `01503b95...754`; local full-chip `9c764459...b27`; 234/234 tests, 8/8 focused and 5/5 normal ICH10 board builds pass; vendor both EHCI `EHCIIR2=2002130a`; source mask/value `2002000c`/`20020008`; POST 44/46 or terminal 47 | BUILD/SOURCE/BINARY/REFERENCE AUDITS PASS; HARDWARE USB ENUMERATION PENDING |
| SEABIOS-USBTRACE/B06VQ-USBTRACE1 | Preserve exactly B06VQ and exclude R/S/T; add only read-only pre-payload USB-gate telemetry plus a deferred, bounded SeaBIOS controller/port/control-transfer journal | same fixed target; directly attached known USB-2.0 keyboard first, high-speed stick in separate run; COM1 armed before reset | two clean deterministic builds, one exact B06VQ regression and QEMU trace validation / 0 hardware runs / 10 | 0 / 10 | public base `5e230059...327`; local full-chip `94ddb540...874`; exact SeaBIOS commit `5497f431`; 277/277 tests; QEMU 24/24 contiguous events, `dropped=0`, descriptor/config/HID/driver path and `USB keyboard initialized`; decoder 5/5 | BUILD/SOURCE/BINARY/ISOLATION/QEMU-INSTRUMENTATION PASS; TARGET USB BOUNDARY PENDING |
| PLATFORM-CENSUS/B06VQ-PLATRO1 | Preserve B06VQ and unmodified SeaBIOS; add one non-fatal read-only post-resource/pre-payload census of LPC/PM/PIRQ/RCBA/HPET gates and existing resources; no IOAPIC MMIO, resource creation or ACPI | same fixed target; COM1 armed before reset; retain complete `[PLATRO]` block and following SeaBIOS entry | two clean deterministic builds / 0 hardware runs / 10 | 0 / 10 | public base `09187e91...57b8`; local full-chip `fa6ddf9f...a4f2`; 9/9 focused plus complete repository suite; CBFS/composite/top placement passed | BUILD/SOURCE/BINARY/READ-ONLY ISOLATION PASS; TARGET CENSUS PENDING |
| ICH10-SIX-FIELD-BASE/B06VQ-ICHBASE1 | Preserve B06VQ; before IOH routing/EHCI/link/scan require exact LPC/RCBA/FDSW and complete PRE or TARGET tuple, then apply only six Intel BIOS-required RCBA fields and require complete target readback; GCS and all adjacent platform policy untouched | same fixed target; first useful run must present exact PRE and log `write=1`; COM1 armed before reset; full historical ICH10 driver disabled | two clean deterministic builds / 0 automatic-image hardware runs / 10; six-field primitive passed once separately in B06VP CAR | 0 / 10 | public base `533e4fdc...96b`; local full-chip `a58c7705...e69a`; 11/11 focused and 297/297 repository tests; pinned clean SeaBIOS/iPXE, CBFS/composite/wrapper/top placement passed; live 24/24 CAR script plus direct readback established exact tuples | BUILD/SOURCE/BINARY/REPRODUCIBILITY PASS; LIVE MANUAL PRIMITIVE PASS ONCE; AUTOMATIC ICHBASE1 TARGET PATH PENDING |
| ICH10-AHCI-MAP/B06VR | Preserve B06VQ; admit only exact quiescent dual-IDE reset or retained AHCI identity; change only MAP[7:5] to `011b`, clear BAR5 for the programming-interface transition, and require F2 `8086:3a22/010601`, F5 absent, MAP `0060`, BAR5 zero and decode off before PCI enumeration | same fixed target; first run without SATA device; full ICH10 driver disabled | two clean deterministic builds and composite/top-placement audits / 0 hardware runs / 10 | 0 / 10 | public base `1c9c2a1...f7c`; local full-chip `55c340b...913`; POST 48/4a or terminal 4b; PCS, clocks and AHCI MMIO untouched | BUILD/SOURCE/BINARY AUDITS PASS; HARDWARE MODE TRANSITION PENDING |
| ICH10-AHCI-PORTS/B06VS | Preserve B06VR; after complete resource allocation require exact AHCI identity/decode and exact 2-KiB assigned/stored ABAR, then set only PCS[5:0] to `3f` with an 8-bit masked write; preserve ORM, reject reserved bits, ignore asynchronous presence for admission | same fixed target; first run without SATA device; capture COM1 before power-on; clocks and AHCI MMIO untouched | two clean deterministic builds and composite/top-placement audits / 0 hardware runs / 10 | 0 / 10 | public base `57299ad...3ec`; local full-chip `09a6255...244`; 257/257 tests and 25/25 Q/R/S contracts pass; POST 4c/4e or terminal 4f; five bounded read-only presence samples | BUILD/SOURCE/BINARY/INDEPENDENT AUDITS PASS; HARDWARE PCS TEST PENDING |
| ICH10-SATA-CLOCK/B06VT | Preserve B06VS; in the same audited post-allocation hook admit SCLKCG only as exact reset `00000000` or retained target `00000193`, then select only Field 1 bits 8:0 as `0x193`; require complete-dword and inherited PCS/resource readback | same fixed target; first run without SATA device; capture COM1 before power-on; PCD/reserved fields zero; no AHCI MMIO/reset/OOB | two clean deterministic builds and composite/top-placement audits / 0 hardware runs / 10 | 0 / 10 | public base `1df6854...8ad`; local full-chip `64260f7...9bb`; 265/265 tests and 33/33 Q/R/S/T contracts pass; helper machine code AND `fffffe00`, OR `193`; POST 50/52 or terminal 53 | BUILD/SOURCE/BINARY AUDITS PASS; HARDWARE SCLKCG TEST PENDING; VALIDATE Q/R/S FIRST |
| ICH10-IOAPIC-MASK/B06VY | Preserve the inherited SeaBIOS/iPXE/USB-trace path; admit exact LPC/RCBA/OIC and IOAPIC identity, require all 24 entries initially masked, then write and verify deterministic masked destination-zero entries | same fixed target; COM1 armed before power-on; no routing, IRQ enable, MRE lock, ACPI or broad ICH10 driver | two byte-identical builds / 0 hardware runs / 10 | 0 / 10 | public base `de602a3d...14c0`; local full-chip `6203a53c...28f`; 337/337 tests; CBFS/composite/top placement pass; POST 70/71/72/73 or terminal 74 | BUILD/SOURCE/BINARY AUDITS PASS; TARGET HARDWARE RUN REQUIRED BEFORE B06VZ |
| FIXED-PLATFORM-RESOURCES/B06VZ | Preserve B06VY hardware behavior; add exact fixed SMBus/PM/GPIO/IOAPIC/RCBA/LAPIC/ROM domain reservations and audit indices 0..14 plus non-overlap without changing allocator apertures | same fixed target; B06VY hardware PASS is prerequisite; no new register write, HPET, ACPI, SMBIOS, IRQ or USB policy | two byte-identical builds / 0 hardware runs / 10 | 0 / 10 | public base `4c909b00...3f6c`; local full-chip `b87cf38e...930`; 344/344 tests; exact resource and W25Q128 placement audits pass | BUILD-ONLY PASS; DO NOT FLASH BEFORE RECORDED B06VY HARDWARE PASS |
| QUIESCENT-HPET-DECODE/B06WA | Preserve B06VZ; admit only complete HPTC `0` or `80`, select `fed00000` decode when needed, prove exact Intel HPET identity and quiescent state, and reserve fixed resource 15 | same fixed target; B06VZ hardware PASS is prerequisite; HPET MMIO read-only; no timer start, route, IOAPIC write, ACPI or IRQ policy | two byte-identical builds / 0 hardware runs / 10 | 0 / 10 | public base `53766368...0e1`; local full-chip `025829c2...80d`; 354/354 tests; 14/14 focused; exact CBFS/composite/top-placement audits pass | BUILD-ONLY PASS; DO NOT FLASH BEFORE RECORDED B06VZ HARDWARE PASS |
| EARLY-TCO-HALT/B06WB | Preserve B06WA; before later ramstage mutations require exact LPC/RCBA/PM decode and `TCO1_CNT` complete value `0000` or `0800`, then set only `TCO_TMR_HLT` when needed and preserve all status/history/timer/reset policy | same fixed target; recorded B06VY, B06VZ and B06WA hardware PASSes are prerequisites; COM1 armed before power-on | two byte-identical builds / 0 hardware runs / 10 | 0 / 10 | public base `1e6d0b4e...20cc`; local full-chip `9f7cd3d9...5bf4`; 7/7 focused and 361/361 all tests; independent source/disassembly, CBFS, composite and W25Q128 placement audits pass | BUILD-ONLY PASS; DO NOT FLASH BEFORE RECORDED VY/VZ/WA HARDWARE PASSES |
| INTEGRATED-PLATFORM/B06WC | Aggressively combine the exact TCO, standard PIC/ELCR, six-field ICH10/AHCI, deterministic IOAPIC, quiescent HPET, quiet ACPI-mode/table and read-only USB-admission work, then continue through the inherited physical-VBIOS SeaBIOS/iPXE path; retain an explicitly armed PIT/ExtINT ROMMON probe | fixed E5645/CPUID `206c2`/microcode `1f`, sole SPD54 BLS4G3D1609DS1S00, HD 5450 physical VBIOS, RTL8168, socketed W25Q128, COM1 115200; predecessor qualification intentionally waived for this experimental convergence image | two byte-identical builds; 0 confirmed electrical G3 / 10; 1 one-second Shelly hardware execution | 1 run reached trained DDR3, High-QPI, MINIT return, UC RAM tests, CBMEM, postcar, ramstage and PCI allocation; 0 payload entries / 10 | public base `133638bf...930ef`; local full-chip `c7482e03...4831`; HW-01 raw `ba733344...a04`; terminal POST `63` with hardware CMD `0000`, MAP `0060`, ABAR `cfcff000` size `800`, PCS/SCLKCG zero and software policy `0003`; no first PCS-write POST `5e` | MAJOR PARTIAL PASS ONCE — FAIL-CLOSED BEFORE SATA WRITE; HARDWARE STATE/RESOURCES VALID, SOFTWARE-ONLY IO-DECODE POLICY FIX REQUIRED |
| SEABIOS-INPUT/B06WD | Preserve B06WC; require its exact observed SATA tuple and clear only `PCI_COMMAND_IO` in coreboot's in-memory policy before unchanged PCS/SCLK/AHCI stages; exact-gate Fintek LDN5 and add only ICH10R KBC decode; run bounded coreboot PS/2 probe and retain SeaBIOS PS/2 plus UHCI/EHCI USB keyboard paths | same fixed E5645/SPD54/HD5450/RTL8168 target; one known-good PS/2 keyboard and one known-good USB-2.0 keyboard in separate runs; COM1 115200 armed before power-on | 0 confirmed electrical G3 / 10; 2 hardware executions, including one controlled one-second mains interruption | 0 / 10 formal warm resets; both executions reached late RAM tests, CBMEM, postcar, ramstage, PCI/resources, SATA correction, PCS/SCLK/AHCI, device init/finalize and FADT/SSDT start; 0 payload entries | two byte-identical builds and 410/410 tests; public base `dcbaf409...b005`; local composite `8a695a85...e12`; W25Q128 `9c2bd669...0533f`; HW-01 continuation `2a857267...041e`; HW-02 prefix/final `b4ffc764...ffcb`/`5789a719...19b5`; both runs show SATA policy `0003->0002`, AHCI READY, KBC interface `0x3`/ACK `ff`, USB `OC_NO_CONNECT` with all 12 live OCA bits asserted, and terminal wrong-BDF ACPI failure | MAJOR PARTIAL PASS TWICE — SATA/AHCI POLICY REPEATED; NO PS/2/USB INPUT; COMMON USB OC#/VBUS PATH IS LEADING HYPOTHESIS; ACPI WRONG-BDF FALSE-NEGATIVE BEFORE MCFG/SEABIOS |
| USB-GPIO57-CAR/B06WD-LIVE | In retained B06WD CAR ROMMON, use one exact-gated transaction per controller view to compare the GPIO56 high-preserving negative control with vendor-correlated GPIO57/H_PWRGD output-low/delay/high; then exercise one low-speed and one full-speed D26 companion port in SeaBIOS-style reset/clear/enable order | fixed E5645/SPD54/HD5450 target with attached USB devices; temporary exact BAR/decode gates; socketed recovery; no valuable USB storage; mandatory one-second AC interruption after every invasive GPIO57 test | multiple isolated one-second AC-recovery cycles; 0 formal electrical-G3 qualifications / 10 | 0 / 10 warm-reset or payload runs; four successful GPIO57 controller-view transactions, successful D26:F0/P1 and D26:F2/P2 reset transactions, plus one prior GPIO56 negative-control transaction | exact capture inventory in `research/msi/b06wd-usb-gpio57-hw-2026-09-07.md`; GPIO56 left EHCI2 `3030`; GPIO57 changed EHCI1/2 all ports `3030->3020`, D29 all `0880`, D26 `0983/0880/0883`; low-speed `0983->0a82->0983->0987`; full-speed `0c8a->088b->0a8a->088b->088f`; both `RUN=ok`; second script FNV `c5a0d015`, SHA `c3b41ebb...c84c` | CONTROLLED ELECTRICAL/ROOT-PORT PASS — GPIO57 CAUSALLY CLEARS LIVE OCA AND LOW-/FULL-SPEED D26 PORTS ACCEPT RESET/ENABLE; NO DESCRIPTOR, USB ADDRESS, HID, ACCEPTED KEY, CALIBRATED TIMING, SEABIOS ENUMERATION OR STABILITY CLAIM |
| ACPI-SAD-BDF/B06WE | Preserve the hardware-executed B06WD path and correct only the read-only MCFG publication gate from absent `00:00.1` to measured SAD `ff:00.1`; require exact SAD ID, PCIEXBAR and direct ECAM host ID before ACPI/SeaBIOS continuation | same fixed E5645/SPD54/HD5450/RTL8168 target; COM1 115200 armed before power-on; PS/2/USB not required because serial payload input is retained | 0 / 10 | 0 / 10 | two byte-identical clean-commit builds; 415/415 tests; public base `2097ae97...af53`; local composite `3219f86e...cc6`; W25Q128 `d9b401a7...8ada`; embedded revision clean `a2eb375438c8`; exact linked CF8 selectors `80ff0100/150/154`; expected POST `8e->8f` | BUILT / STATIC AND ARTIFACT VERIFIED / NOT RUN — single functional delta; NO ACPI COMPLETION/SEABIOS/PAYLOAD CLAIM; PS/2 AND USB REMAIN UNRESOLVED |
| LATE-USB-LAB/B06WF | Preserve B06WE; stop after PCI resources/device finalization and before ACPI/payload; expose only one-shot exact-gated GPIO56 high/low, GPIO57/H_PWRGD low-delay-high, EHCI CONFIGFLAG ownership and fixed W1C OC-latch experiments; allow only verified GPIO57 output/high to persist into SeaBIOS | same fixed E5645/SPD54/HD5450/RTL8168 target; preattached USB-2.0 keyboard; COM1 115200; socketed recovery chip; one hypothesis per boot | 0 / 10 B06WF target runs; preceding live B06WD/XRS GPIO57 experiment passed once | 0 / 10 B06WF payload continuations | two byte-identical builds; 427/427 full and 12/12 focused tests; public base `ffe200f4...d709`; local composite `a36c54d1...c9fa`; W25Q128 `a017b3db...39a`; live predecessor evidence: EHCI `3030->3020` on all ports, UHCI OCA cleared on all ports with values `0880/0883/0983`; GPIO57 assertions target-mask only | BUILT / STATIC, FAIL-CLOSED CONTRACT AND ARTIFACT VERIFIED / B06WF NOT RUN — PRIMARY NEXT TEST IS `unlock GPIO57-PWRGD`, `usb gpio57 pwrgd`, REQUIRE EHCI/UHCI OCA=000, THEN `continue`; NO B06WF KEYBOARD OR PAYLOAD CLAIM |
| AUTO-USB-SEABIOS/B06WG | Preserve B06WE with B06WF disabled; retain exact RAM-result/MTRR gates, transactional alias smoke and deterministic one-pass window clearing plus nine sparse reads while explicitly omitting only B06WG's exhaustive pattern/invert/full-read passes; after PCI resource/device init automatically require the fixed B06WD controller tuple and all live OCA active, drive only GPIO57 output-low, wait calibrated 65.536 ms, drive high, sample through 500 ms, require every live OCA clear, then revalidate output/high and OCA immediately before payload; SeaBIOS level 8 owns all HC reset/enumeration/HID | same fixed E5645/SPD54/HD5450/RTL8168 target; USB-2.0 keyboard and Transcend 128GB USB storage; COM1 115200 armed; socketed chip and established one-second AC recovery | 0 independently instrumented G3 / 10; 1 one-second-Shelly reset-to-payload pass; one later automatic reset correctly stopped at retained-state CAR guard | 1 / 10 payload/keyboard runs; operator-confirmed accepted USB input; one local Hiren's load reached graphical BSOD, STOP code unknown | two byte-identical builds; 447/447 full and 10/10 focused tests; W25Q128 `ba5c6d24...ac7c`; clean trace `caa59c5e...022fcc`; partial local-USB trace `edc89381...05d4`; automatic OCA gates, SeaBIOS EHCI/UHCI, USB HID/MSC, VGA, RTL8168 DHCP/TFTP and external local-USB loader proven once; B06WG sparse RAM remains explicitly unqualified | HARDWARE MAJOR PARTIAL PASS — AUTOMATIC COREBOOT/SEABIOS, USB KEYBOARD AND MSC WORK; LOCAL WINDOWS PE REACHES BSOD; REPETITION, COMPLETE HIGH-RAM VALIDATION, BUGCHECK CAUSE AND OS BOOT PENDING |
| QUIET-USB-SEABIOS/B06WH | Preserve the complete B06WG platform, sparse fast-postmem, GPIO57/OCA and payload-feature path; use the identical automatic USB source; change only SeaBIOS global debug level 8→6 while retaining the bounded level-1 USB trace; give every executable stage and artifact a distinct B06WH identity | same fixed E5645/SPD54/HD5450/RTL8168, keyboard and Transcend test medium; COM1 115200 armed; video capture for a possible Windows STOP code; socketed recovery chip and one-second AC recovery | 0 confirmed electrical G3 / 10; 1 B06WH hardware execution with incomplete reset provenance | 1 / 10 B06WH payload/USB/local-Windows-PE runs; graphical Windows reached, OS boot not completed | two clean fixed-epoch builds byte-identical; public base `7b079fe0...f49513`; W25Q128 `b5f894d4...024c6b`; retained HW-02 serial `f1246d78...3f9` covers late RAMINIT through complete ramstage/ACPI, SeaBIOS, USB HID/MSC and the post-bugcheck automatic reset; operator screen identified `ACPI_BIOS_ERROR (0xA5)`, parameters unavailable; audit finds no PCI `_PRT`, LAPIC0/IOAPIC0 ID collision, no IRQ0-to-GSI2 override, no GPE0 block and an unproved i8042 advertisement | HARDWARE MAJOR PARTIAL PASS ONCE — QUIETER SEABIOS, USB KEYBOARD/MSC, DISPLAY AND LOCAL WINDOWS EXECUTION PROVED; WINDOWS STOPS AT ACPI_BIOS_ERROR 0xA5; EXACT A5 SUBTYPE AND CAUSAL DEFECT REMAIN UNPROVED |
| VENDOR-IRQ-ACPI/B06WI | Preserve B06WH; before PCI scan exact-gate the post-B06VY/B06WC route tuple, change only D26:F2 pin selection plus the five active DxxIR fields and IOAPIC ID 0→1, then publish matching direct-GSI root/bridge `_PRT`, IRQ0→GSI2, disabled GPE0 and no unproved i8042; keep PIRQ/RTE/INT_LINE/GPIO untouched | same fixed E5645/SPD54/HD5450/RTL8168, USB keyboard and Transcend/Hiren's medium; COM1 115200; socketed known-good chip and one-second AC recovery; no Vendor AML/SMM/dynamic MADT/GPIO image | 1 / 10 routing/table/USB-handoff pass, B06WI-HW-01; operator screenshot confirms graphical Windows A5 again, not an OS boot | 0 / 10 warm-reset tests; no additional RAM/QPI qualification requested | focused source contract 8/8; W25Q128 `3d9e6e03...c6b04`; raw `d80b43fd...a00e`; IRQ READY, native FADT/MADT/MCFG, JetFlash entry 2 to 0000:7c00 captured; subsequent screen ACPI_BIOS_ERROR 0xA5, four parameters unavailable | HARDWARE ROUTE/ACPI-CONSTRUCTION AND USB HANDOFF PASS; WINDOWS A5 PERSISTS; EXACT ACPI SUBTYPE/CAUSE UNKNOWN |
| ACPI-PLATFORM/B06WJ | Inherit WI hardware path; add matching CP00 UID0, LPC PIC/PIT/RTC, low-I/O resources and HPET namespace/table; no new hardware programming or RAM/QPI qualification | fixed WI E5645/SPD54/HD5450/JetFlash setup; COM1 and screen capture; known-good chip/one-second AC recovery | 1 controlled one-second mains restart to JetFlash; electrical G3 not independently measured | 0 confirmed OS passes; operator subsequently confirms recurring ACPI error | two byte-identical builds; 469/469 host tests; IASL 0 errors/warnings; W25Q128 `9ad2108d...6bd06`; DSDT 1076 bytes; boot snapshot `eca57b17...621f9`, noisy pre-reset prefix excluded; new HPET/CP00/LPC marker, ACPI done, SeaBIOS and menu entry 2 to 0000:7c00 | HARDWARE ACPI-CONSTRUCTION/SEABIOS/JETFLASH HANDOFF PASS ONCE; WINDOWS A5 RECURS, SUBTYPE UNKNOWN |
| ACPI-RAMLAB/B06WJ | Correct captured CTBL descriptor flags/checksum and add 44 vendor-referenced IOH root routes only in RAM; preserve flash and accepted RAM/QPI/USB baseline | same E5645/SPD54/HD5450; JetFlash Windows setup NTFS CCCOMA_X64FRE_EN-GB_DV9; late BIOS-phase GRUB PXE diagnostic | 2 target-only one-second AC cycles for diagnostic entry; none issued after RAM corrections | 1 RAM mutation/readback and JetFlash MBR handoff; operator reports A5 again; 0 confirmed OS passes | all 11 final ACPI structures captured/checksummed, candidate DSDT hash `ebb760c9...8bbf`, corrected SSDT `08522e71...8033`; FADT first in both roots, E820 hook reserves ACPI/EBDA; boot raw `af7161b9...00d9c`; later single-space TX confirmed, no serial response | TABLE CORRECTIONS VERIFIED BEFORE HANDOFF; OPERATOR-CONFIRMED A5 RECURS; KERNEL TABLE SELECTION/SUBTYPE UNKNOWN; NOT AN ISOLATED-CAUSE A/B TEST |
| ACPI-REPAIR/B06WK | Native common CTBL consumer flag; WK-gated 44 IOH routes, VGA producer windows and fixed-button FADT description; no new register programming | fixed E5645/SPD54/HD5450; COM1 115200; Intenso Ultra Line 8.01 plus JetFlash/Crucial present; preserved WJ/vendor recovery | 7 hardware sessions: operator HW01, one-second HW02, HW03/04/05 with1/1/2-second Shelly interruptions, HW06 operator cold start, HW07 requested2-second interruption; no electrical G3 proof; two byte-identical clean builds | 0 confirmed completed desktop/session passes; HW07 native kernel and initramfs userspace proven; no added RAM/QPI qualification | 515/515 host and7/7 native C release tests; HW03–05 pre-payload UHCI mismatch; HW06 recovered USB but stale diagnostic edit; HW07 sole-ESC helper, actual serial args, RTC return0, /init/eudev, SATA/USB/HID/RTL8168, ISO9660, KVM at964s; CEDAR firmware unavailable; high RAM allocated; final622646-byte raw archived | NATIVE LINUX SERIAL AND USERSPACE PASS; CAPTURE CLOSED/TARGET UNCHANGED; GPU FIRMWARE, SLOWNESS, UNCLASSIFIED MCE AND HIGH-RAM CACHE COVERAGE OPEN; WINDOWS A5 NOT RETESTED |
| INTENSO-MEMTEST/B06WK-HW08 | Boot operator-reprepared Intenso without Linux edits or RAM/QPI retuning | target E5649 per HW07 brand string; SPD54 4GiB2Rx8; HD5450; intended unchanged WK | 1 requested2-second Shelly interruption; no measuredG3 | 0 tester starts/passes | 25/25 capture-helper tests; freshWK through ramstage; fiveUHCI0018/0024 mismatch; final raw4129633bytes,zeroTX,hashes in bring-up log | BLOCKED AT USB PREFLIGHT BEFORE SEABIOS; INTENSO/MEMTEST NOT TESTED; FULLER COLD RECOVERY NEEDED |
| INTENSO-MEMTEST/B06WK-HW09 | After operator cold start, boot named Intenso without additional inputs or retuning | inherited E5649/SPD54/HD5450; actualIntensoAluLine5.00,3.75GiB | 1 operator-reported cold start; off duration unmeasured; early boot missed | 0 confirmed Memtest passes | freshWKramstage,USB-PAYLOAD ADMIT,SeaBIOS,ESC+2,Intenso0000:7c00 and GRUB welcome; final878352-byte raw/hash verified; later operator explicitly reports tester did not run | USB/HANDOFF RECOVERY PASS; GRUB EXECUTES; MEMTEST DID NOT RUN; NO MEMORY-TEST RESULT; EXACT TESTER/STOPPING POINT UNKNOWN |
| SPD-SLOT/B06D | Establish the physical slot/address map | one DIMM moved one physical slot at a time only after B06C is stable | 0 / 10 | 0 / 10 | address-to-silkscreen map and missing-SPD handling | NOT RUN |
| SPD-SERIAL/PENDING | Admit the exact existing256-byte DDR3 profile regardless of bytes122–125, preserving raw provenance and all other gates | E5649/206c2/ucode1f; soleSPD54 BLS4G3D1609DS1S00.4GiB2Rx8, replacement serial01020305; HD5450/socketed recovery | 0 / 10; no new firmware image built | 0 / 10 | source/configuration and host-only regression tests; profileFNV5194e521; optional v10/164-byte handoff; see Documentation/spd-compatibility.md | SOURCE ONLY; HARDWARE/TRAINING/MEMTEST PENDING; ORIGINAL WK ROM UNCHANGED |
| QR0/B07 | Read-only CPU/X58 QPI snapshot | same fixed hardware | 0 / 10 | 0 / 10 | device IDs, link status, reset cause; no retraining | NOT RUN |
| Q0/B08 | Controlled CPU-X58 QPI init | conservative link | 0 / 10 | 0 / 10 | status/speed/width and bounded failure/reset count | NOT RUN |
| M0/B09 | One-rank DDR3-800 | one approved channel-0/A0 DIMM | 0 / 10 | 0 / 10 | per-training-phase/lane log and 256 MiB test | NOT RUN |
| P0 | PCI enumeration | one GPU and SATA/USB | 0 confirmed electrical G3 / 10; two B06VO and one B06VP complete payload runs | 3 selective trees / 10 | B06VP repeated X58/ICH10R, Radeon VGA/audio and RTL8168 enumeration and completed network boot-menu entry; USB/SATA controller functions enumerate but no USB/storage endpoint is proved | PARTIAL PASS THREE TIMES — FIXED SELECTIVE TREE AND GPU/NIC ENDPOINTS; 10-RUN STABILITY, GENERAL ENUMERATION, USB/STORAGE ENDPOINTS PENDING |
| SB0 | SeaBIOS payload | same fixed hardware | 0 confirmed G3 / 10; one reduced B06VL entry, two boot-enabled B06VN entries, two B06VO VGA entries and one B06VP network entry | 1 reduced plus 5 boot-enabled payload entries / 10; display 3 / 10; network/menu 1 / 10 | B06VP entered SeaBIOS, returned from the physical Radeon VBIOS, executed iPXE, brought RTL8168 link up, completed DHCP/TFTP and entered netboot.xyz; USB/i8042 input absent in that trace | MAJOR PARTIAL PASS WITH VGA AND NETWORK — SEABIOS, PHYSICAL VBIOS, DISPLAY, IPXE, DHCP/TFTP AND NETBOOT MENU REACHED; USB/PS2, SATA/STORAGE, OS BOOT AND STABILITY PENDING |
| U0 | EDK2 shell | GPT test device | 0 / 10 | 0 / 10 | UEFI memory map/shell/device enumeration | NOT RUN |
| O0 | 64-bit OS loader | same as U0 | 0 / 10 | 0 / 10 | loader and longer external memory test | NOT RUN |

For the intended B04 artifact, terminal `dd` proves only the PCI
configuration-space BAR/enable
register writes and their explicit invariants.  It does not prove RCBA MMIO,
PM/GPIO I/O, SMBus bus
traffic, SPD access, GPIO pin policy, watchdog handling, or complete ICH10R
initialization.  A retained warm-reset state may deliberately produce `f3`;
the reported run's boot type and terminal stability remain unknown.

B05 is read-only only with respect to the SPD EEPROM data payload: it writes
I801 host registers and transmits an SMBus command byte.  `c8` marks the last
safe boundary immediately before the single START.  Terminal `dc` proves one
successful byte-data transaction and value `0x0b`, not a scan, full SPD dump,
CRC, physical slot mapping, or memory initialization.  `fa` is exact
`DEV_ERR`; it may mean that no device acknowledged `0x50`, including a DIMM
population whose occupied slot maps elsewhere.  The one reported B05 run
ended at exact `fa`; its DIMM population, boot type, complete trace, serial
output, stability, and programmer read-back were not reported.  At `f9` B05 deliberately
does not issue KILL or release the host; remove AC power completely before
recovery or another attempt.

Exploratory vendor-firmware evidence from 2026-08-04 and the targeted
2026-08-31 runtime recheck do not satisfy a cell:
one already-running three-DIMM configuration yielded three identical Uncore
snapshots and valid SPDs at `0x50/0x52/0x54`.  Boot type was not controlled,
and no cold/warm repetition or one-DIMM slot isolation was performed.  The
runtime recheck also observed `SMBus_PIN_CTL=0x07` and the same responding
addresses, but it does not establish pre-DRAM reset state.  See
[the live Uncore report](../research/msi/live-uncore-2026-08-04.md) and
[the I801 reference](../research/msi/live-i801-2026-08-31.md).

B06A is a bounded diagnostic exception prompted by `fa`, not completion of
the SPD milestone.  It writes `SMBus_PIN_CTL=0x04` once, verifies bits 2:0
read back as `0x07`, and performs at most eight STARTs: one type-byte command
per standard SPD address with no retry.  Among transaction errors, only exact
`DEV_ERR` is cleared so that the next address can be attempted.  A timeout is
not recovered and requires complete AC removal.
The B06B base-SPD/CRC gate succeeded once at terminal `64`.  B06C then reached
`76`: its fixed 176-used/256-total assumption was rejected before any upper
read or decoder call.  B06H measures the relevant byte-0 fields while keeping
the proven 128-byte transaction boundary.  Physical-slot mapping is deferred
to B06D.  IMC, QPI, and DDR training remain later work.

The B06A POST contract is build-specific:

```text
41..48  exactly one DDR3 responder at 50..57, respectively
49      multiple DDR3 responders
4a      no responder; all eight commands returned exact DEV_ERR
4b      at least one successful response with a type other than 0b
4c..4e  clock, data, or both pin conditions invalid
4f      initial host state or command read-back invalid
50      transaction timeout; no recovery or host release attempted
51/52   exact BUS_ERR / other transaction failure
53      good scan result, but final UART/TEMT failure
54..56  pin phase entered, PIN_CTL=04 written, pins observed idle high
57      CPUID leaf-1 EAX was not exactly 000206c2
80..87  immediately before START at 50..57
88..8f  exact DEV_ERR at 50..57
90..97  successful DDR3 type 0b at 50..57
98..9f  successful other type at 50..57
```

For the three-address runtime reference (`0x50/0x52/0x54`), `49` was the
comparison hypothesis.  The real B06A test instead reached `45`: exactly one
DDR3 response at `0x54`, with exact `DEV_ERR` at the other seven addresses.
The test DIMM population, full trace, boot type, serial reception, stability,
and programmer read-back were not reported.
The complete raw-image SHA-256 required for the first B06A test is
`83100b39b15997576f0e7a599a0e8ab5c1e78fdf1cff6f204926620e206e930e`.

The B06B POST contract is also build-specific:

```text
4c..4e  clock, data, or both pin conditions invalid
4f      initial host state or command read-back invalid
50      transaction timeout; no recovery or host release attempted
51/52   exact BUS_ERR / FAILED, combined, or unexpected result
53      valid SPD/CRC, but final UART/TEMT failure
54..56  pin phase entered, PIN_CTL=04 written, pins observed idle high
57      CPUID leaf-1 EAX was not exactly 000206c2
58      fixed 0x54 base-SPD read begins
59      offset 02 returned DDR3 type 0b
5a..61  successive 16-byte blocks 00..0f through 70..7f completed
62      revision-selected base-section CRC matches bytes 126/127
63      complete UART report reached TEMT
64      intentional success halt
65      exact DEV_ERR during the fixed-address dump
66      offset 02 did not return DDR3 type 0b
67      base-section CRC mismatch
```

The later B06H serial measurement for `0x54` is header `93 13 0b 02`, CRC
coverage bytes `0..116` (117 bytes), stored/calculated CRC `0xec4f`, and
first-128-byte SHA-256
`4c13dd162c0545dc692a1f5ba93b56722c6daf7555e4751f6af3546847a4bbc7`.
The expected successful B06B suffix after inherited B04 code `d7` is
`54,55,56,58,59,5a,5b,5c,5d,5e,5f,60,61,62,63,64`.
Only terminal `64` was reported; the full trace, concrete reference bytes,
external UART reception, cold/warm provenance, stability, and read-back hash
remain unverified.

The B06C-specific continuation after inherited code `62` is:

```text
68       header declares 176 used bytes in a 256-byte DDR3 EEPROM
69       first read of offsets 80..af begins
6a..6c  first-read blocks 80..8f through a0..af completed
6d       verification read of offsets 80..af begins
6e..70  verification blocks completed and matched so far
71       all 48 unprotected upper bytes matched between both reads
72       coreboot spd_decode_ddr3() returned SPD_STATUS_OK
73       narrow 4-GiB 2R x8 DDR3-800 JEDEC-cycle candidate derived, not programmed
74       complete UART report reached TEMT
75       intentional B06C success halt
76       unsupported byte-0 used/total declaration
77       upper-byte verification mismatch
78       decoder or MTB-divisor guard failed
79       narrow topology/timing candidate rejected
```

Terminal `75` does not prove a physical slot, reference-module identity,
complete 256-byte SPD, XMP, usable DRAM, or any X58/QPI/IMC programming.
Offsets `0xb0..0xff` are structurally outside B06C.  Its upper-48 and used-176
CRC-16 values are diagnostic fingerprints only, not stored JEDEC integrity
fields.  The reported timing tuple is not an X58 register encoding; observed
X58 fields add controller-specific offsets/relations and remain a later gate.

The one reported B06C run ended at `76`.  That necessarily passed the path
through `62`, issued the I801 release write, and stopped before `68` or any
offset `0x80` command.  B06C alone proved only that byte 0 did not match the
accepted 176-used/256-total declaration; B06H subsequently measured the exact
value as `0x93`.

The B06H continuation after inherited code `62` is:

```text
7a       header telemetry armed; no additional SMBus command follows
80..ff  terminal = 0x80 | (SPD[0] & 0x7f)
```

In this build-specific encoding, `91`, `92`, and `93` mean 128, 176, or 256
bytes used respectively, each with 256 bytes total.  Raw byte-0 bit 7 selects
the already-passed CRC coverage.  The reported UART dump measured raw `0x93`.
Any other encoded value remains evidence for a fail-closed follow-up.

The B06I continuation after inherited code `62` is:

```text
a0       header accepted as 256 used / 256 total
a1       first upper-128 pass starts
a2..a9  first-pass blocks complete
aa       second upper-128 pass starts
ab..b2  second-pass blocks complete and match so far
b3       all upper bytes matched; I801 host released
b4       coreboot DDR3 decoder returned SPD_STATUS_OK
b5       narrow DDR3-800 cycle candidate derived but not programmed
b6       complete UART report reached TEMT
b7       intentional B06I success halt
b8..bb  header, mismatch, decode, or policy failure
```

B06J does not execute that path automatically.  Its normal entry ends at
`bc`; a human `spd` command exposes the B06I progress codes and then returns
to `bc`.  `bd` marks human command dispatch, `be` a UART monitor error, `bf` a
command/lock/protocol error, and `c7` entry into permanent SerialICE mode.

B06V0 retains those meanings and adds `cc/cd/ce` for armed CF9
INIT/warm/full requests, `d0` for the completed PCIEXBAR/X58 platform gate,
`d1/d2` for CSI call/return, `d3/d4` for MINIT call/return and `df` for a
vendor-path rejection. `d2` and `d4` are return markers only. CSI can reset or
remain permanently in HLT at `d1`; MINIT can do the same at `d3`. The first
test is limited to B06V0-P, and the first state-changing test is limited to
B06V0-X. See the dedicated command procedure before advancing cells.

B06V1 retains all B06V0 codes and adds `e5` for a flushed script-operation
PRE marker, `e6/e7` for a returned failed/successful run, `e8` for rollback
PRE, and `e9/ea` for a returned successful/failed manual rollback. A returned
ROMMON command then restores ordinary ready code `bc`. `rev` is only an
operator declaration; these codes cannot prove that a side effect was
reversible. The first B06V1 test is limited to B06V1-P, followed on a separate
cold boot by the read/assert-only B06V1-R procedure.

## Expansion matrix — locked until M0 is stable

| Variable | Initial value | Later values | Status |
|---|---|---|---|
| CPU | E5645 / CPUID `0x206c2` stepping 2 | other Westmere-EP/Gulftown, then Bloomfield | LOCKED |
| channels | one | two, then three | LOCKED |
| DIMMs | one | same-channel second slot, then more | LOCKED |
| ranks | one supported topology | dual/mixed ranks | LOCKED |
| speed | lowest safe supported JEDEC | higher JEDEC speeds | LOCKED |
| resume | cold/warm reset only | S3 | LOCKED |
| storage | AHCI | JMicron, RAID | LOCKED |
| payload | SeaBIOS diagnostic | EDK2, OS boot | LOCKED |
