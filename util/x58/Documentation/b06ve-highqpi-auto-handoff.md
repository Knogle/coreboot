> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VE exact High-QPI automatic handoff and DRAM ROMMON

Status: **BUILT TWICE / STATIC CONTRACT VERIFIED / LOCAL COMPOSITE VERIFIED /
HARDWARE EXERCISED / EXACT MINIT RETURN OBSERVED / RAW-WORKSPACE GATE
REJECTED / CAR RECOVERY ONLY**.

B06VE promotes exactly one hardware-observed B06VD result into the existing
coreboot post-memory path.  Its target is deliberately limited: reproduce the
accepted three-pass High-QPI CSI sequence, call MINIT once, validate the exact
returned state, exercise two complete 8-MiB DRAM windows, leave cache-as-RAM,
and enter the DRAM-resident ROMMON.  It contains no payload and performs no
normal coreboot PCI enumeration or GPU initialization.

The hardware run described below reached and returned from MINIT with the
expected High-QPI/one-channel endpoint, but produced a second raw workspace
form.  B06VE's deliberately narrow raw-FNV gate rejected it.  Therefore B06VE
did **not** run its post-memory tests, leave CAR, enter postcar, or reach the
DRAM ROMMON.

This remains an experimental vendor-assisted bridge.  The local flash image
executes hash-pinned MSI CSI/MINIT code; the repository and redistributable
coreboot-only base contain no proprietary module.  B06VE is not a native/open
X58 memory initializer.

## Evidence promoted from B06VD

All inherited CPU, SPD, three-pass reset, wrapper/module, ABI, stack, pointer,
CAR-canary, I801, CMOS, and platform checks remain mandatory.  B06VE continues
only for the exact successful B06VD run-2 branch:

```text
CPU:                         CPUID 000206c2, microcode 0000001f
SPD:                         sole responder 0x54, FNV fb66b530
CSI state byte 0x2a6:        08
CSI raw/canonical FNV:       03e3d24e / 908dabb6
MINIT policy FNV:            3c0f3a0b before, current and direct
MINIT workspace FNV:         94299f43
workspace 01/02/4f/e79:      00/00/02/01
MINIT EAX:                   00000000
QPI status:                  070f0f03
post-MINIT IOH stage +0x9c:  bf000000
MC mapper/common:            00024489 / 00001545
channel-2 DOD/ranks/status:  000002ac / 00000003 / 00000140
```

That workspace value is the single raw form admitted by B06VE, not a claim
that every successful MINIT return is byte-identical.  The later B06VE
hardware run measured raw FNV `b6346533`; after offline canonicalization of
the now-observed dynamic bytes, both raw forms produce canonical FNV
`a6f9c2e6`.  B06VE itself still required raw `94299f43` and correctly failed
closed on the new observation.

The measured `2a6=0c`/raw-FNV `8b38506a` CSI variant is still recognized by
the inherited B06VD audit, but B06VE rejects it before CSI acceptance, policy
installation, or MINIT.  The pre-pass-3 A0 set remains the finite observed set
`{17000,17600,17800,17a00,17c00}`; the post-CSI and post-MINIT A0 requirement
remains exactly `17000`.  No masks, ranges, writes, or field semantics are
inferred from those observations.

The post-MINIT gate is separate from the pre-MINIT gate.  In particular, it
requires the measured IOH stage value `bf000000`; it does not incorrectly
reapply the pre-MINIT `ea000000` predicate after MINIT has changed the state.

## Memory and stage transition

After the complete exact-result gate, romstage records a version-3 handoff and
keeps the persistent `CMOS 0x0e=ec` plus I801 MINIT marker armed.  Post-memory
code then requires the observed default-UC MTRR frame and performs, in order:

1. a simultaneous reversible 14-address alias test across the future CBMEM
   and object windows;
2. an address-derived pattern, inverse pattern, and verified-clear pass over
   every aligned dword in `01000000..017fffff`;
3. the same complete test over `02000000..027fffff`;
4. CBMEM creation and digest-verified read-back of the version-3 handoff;
5. only then, exact clearing of the I801 phase marker and restoration of the
   High-QPI cold cookie to `CMOS 0x0e=2c`;
6. normal coreboot postcar transition and entry into the DRAM ROMMON.

POST `0x0a` can remain visible while the two complete 8-MiB regions are
written and read several times.  That interval alone is not evidence of a
hang.  These tests cover the 16 MiB used by the transition; they do not prove
the full 4-GiB DIMM, long-term retention, cache coherency, or cold-boot
reproducibility.

## Expected POST sequence

The first two reset-separated High-QPI segments remain the B06V9/B06VD
sequence:

```text
pass 1: ... 10,11,12,03,04,d1 -> expected CSI-internal reset
pass 2: ... 10,11,12,03,05,d1,d2,ca,cb,fe -> expected IOH SYRE reset
```

The expected successful third segment is:

```text
pass 3: ... 10,11,13,12,03,cf,d1,d9,d3,d4,08,09,0a,
        [two complete 8-MiB UC tests],0b,0c,0d,30,31,32,0e,0f
```

Important boundaries:

| Code | Meaning |
| --- | --- |
| `d3` / `d4` | MINIT entered / returned; neither alone proves usable DRAM |
| `08` | exact B06VE post-MINIT result accepted for handoff |
| `09` | post-memory validation entered |
| `0a` | exact pre-postcar MTRR state accepted; bulk UC tests running |
| `0b` | 14-address and both complete 8-MiB tests passed |
| `0c` | CBMEM handoff verified and persistent guard finalized |
| `0d,30,31,32` | postcar frame and generic postcar transition |
| `0e` / `0f` | DRAM ROMMON callback entered / prompt ready |
| `14..19` | fail-closed post-memory result/MTRR/DRAM/CBMEM/handoff stop |
| `1f` | persistent phase-guard stop |
| `20,bc` | automatic branch declined; recovery ROMMON retained in CAR |

Any post-memory stop before the final guard commit deliberately leaves the
persistent marker armed, preventing an unreviewed warm reset from repeating
MINIT.  B06VE clears it only after the complete DRAM and CBMEM read-backs.
Postcar loading and the DRAM-ROMMON's repeated CBMEM, UART, handoff, and MTRR
checks remain fallible after that commit.  Their explicit failure paths halt
rather than request a reset, but a later operator reset starts a fresh guarded
three-pass sequence.  Deferring the marker clear into ramstage would require a
second stage to mutate the early I801/CMOS protocol and was not added to this
first hardware experiment.

## Hardware result — 2026-09-05

Three raw hardware captures were retained.  They are three observations, not
three successful cold-boot repetitions:

1. `x58-b06ve-cold-20260905-1213.raw.log` stopped immediately on the retained
   persistent guard (`CMOS 0x0e=ec`) and entered the CAR recovery ROMMON.  No
   automatic CSI/MINIT call followed.
2. `x58-b06ve-cold-20260905-1215.raw.log` completed passes 1 and 2 and returned
   from pass-3 CSI, but the strict post-CSI platform endpoint did not match:
   CPU `9c=00a00502` and `A0=00017a00`.  It fell back before MINIT.
3. `x58-b06ve-cold-20260905-1217.raw.log` completed passes 1 and 2, returned
   from pass-3 CSI with state `08`, raw/canonical CSI FNVs
   `03e3d24e/908dabb6`, and reached the exact post-CSI endpoint.  MINIT
   returned with EAX zero, QPI `070f0f03`, IOH stage `bf000000`, mapper/common
   `00024489/00001545`, and channel-2 DOD/ranks/status
   `000002ac/00000003/00000140`.  Its workspace raw FNV was `b6346533`, not
   B06VE's only admitted raw FNV `94299f43`; the exact post-MINIT gate rejected
   it and retained the CAR recovery ROMMON.  Offline classification gives the
   new workspace canonical FNV `a6f9c2e6`.

The capture SHA-256 values are:

```text
a1a03b888c68c2b09b87e9a0047b63878c315c3d5fe7e8e60bc7475d55f03b88  x58-b06ve-cold-20260905-1213.raw.log
2626d2a77b460f4470ede0c152563aaf57adda89449ef3533bf3746f2c9c6524  x58-b06ve-cold-20260905-1215.raw.log
2ddb5bc2e71a9c87f31e65bf22f7291bb25dac9e79bbaa55e7f10549f5047b17  x58-b06ve-cold-20260905-1217.raw.log
6549cd0c2eb04bacb5dd2a15fb000c109cca8fcb622473da6934de8d11018b1c  x58-b06ve-run3-workspace.bin
```

No `08,09,0a` post-memory sequence followed run 3.  In particular, there is
no B06VE hardware evidence for the automatic 16-MiB tests, CBMEM, postcar,
ramstage, DRAM ROMMON, or a payload handoff.

From run 3's post-MINIT **CAR recovery ROMMON**, separate transactional scripts
then tested the B06VF PAM prerequisite.  PAM bytes `ff:00.1 + 0x40..0x46`
read `00/00/00/00/00/00/00`, accepted
`30/33/33/33/33/33/33`, and restored exactly to zero.  Reversible shadow tests
covered representative addresses in all sixteen 16-KiB half-windows from
`000c0000` through `000fffff` without an alias among the tested points.  A
final script kept distinct patterns at the future SeaBIOS load word
`000f8800` and aligned entry word `000fecd4` resident simultaneously, returned
`RUN=ok`, and then restored both words and all PAM bytes.  Its transaction FNV
changed from `68a06b96` before rollback to `6e389292` after verified rollback;
discard also succeeded.

These PAM/shadow observations validate only those reversible accesses in the
post-MINIT CAR state.  They do not turn the session into a DRAM-ROMMON run and
do not prove coreboot SELF loading or SeaBIOS execution.

## Future B06VE repetition procedure

Use only the configuration that produced B06VD run 2:

```text
board: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 000206c2, microcode 0000001f
DIMM: BLS4G3D1609DS1S00, sole SPD responder 0x54, FNV fb66b530
memory ratio: 6
initial QPI: Slow; automatic path promotes it to the measured High-QPI state
UART: COM1 03f8, 115200 8N1, no flow control
flash: socketed W25Q128.V..M, complete 16-MiB image
start: true G3/AC-cold after exact guard preparation
```

Start the serial capture before applying AC power.  Do not issue an
interactive reset merely to obtain output: the two expected resets belong to
the CSI/QPI state machine.  If the recovery ROMMON reports guard `ec` or
fail-lock `ed`, issue `unlock RESET`, then `autoguard clear`, require the
reported transition to `0x2c`, and remove AC power completely before retrying.

If a future B06VE repetition reaches its first `0x0f` prompt, run only these
passive commands and preserve their complete output:

```text
id
handoff
mtrr
timer
netprobe
obj status
```

`netprobe` is read-only.  B06VE has no TFTP implementation, NIC writes/DMA,
global PCI enumeration, graphics initialization, uploaded-object execution,
or payload.  The XRL1 serial loader can store and read back a bounded object
only after the first passive log has been reviewed.

## Recovery boundary

CSI/MINIT may reset, hang, halt, or fail to return.  Preserve the complete
serial stream, POST trace, reset provenance, image hash, and programmer
read-back when available.  For a nonresponsive board, remove AC and restore
the verified vendor image on the spare socketed chip.  One `0x0f` arrival will
be a major new milestone, but the project acceptance rule still requires ten
controlled cold boots and appropriate broader memory tests.
