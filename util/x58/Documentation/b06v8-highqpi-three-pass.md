> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06V8 terminal High-QPI CSI three-pass probe

> **2026-09-04 hardware erratum — retired; do not flash**
>
> B06V8 ran once and reached its fail-closed CAR ROMMON without invoking CSI.
> Live inspection found ICH10 `RCBA+0x3400=0`: upper-128 RTC decode was
> disabled, so ports `0x72/0x73` aliased `0x70/0x71`. Consequently the values
> previously labeled `EXT80/81/82/88/89=b3/19/d7/67/cb` were standard RTC
> seconds/seconds-alarm/minutes/month/year, not upper-bank CMOS. The associated
> exact-input gate is time-dependent and the B06V8 cold-rearm table wrote RTC
> calendar data. Do not run that table or use B06V8 for another hardware test.
>
> This erratum does not invalidate the separately observed Slow-to-High QPI
> transition or its endpoint-register evidence. It invalidates only the claimed
> source and deterministic retention of the wrapper-input tuple. B06V9
> supersedes B06V8 with an exact upper-bank-decode gate and a deterministic,
> hash-pinned wrapper-state profile.

Status: **retired after one safe hardware fallback; no B06V8 CSI call occurred**.
The 2026-09-04 live record proves the two-reset transition and return to the
reset vector, but no third CSI invocation occurred in that record.  Therefore
it is not evidence that this image reaches pass 3, that pass 3 returns, or that
High-QPI memory initialization succeeds.

B06V8 is a default-off research branch selected by
`CONFIG_X58_PRO_E_B06V8_HIGH_QPI_3PASS`.  B06V7 remains the known buildable
robust Slow-QPI automatic RAM-init path when this option is disabled.  B06V8
never calls MINIT, never records a DRAM handoff, and never enters postcar,
ramstage, or a payload.  Its only terminal environment is the CAR ROMMON.

## Narrow hardware hypothesis

The experiment tests one question: after two CSI invocations have staged both
ends of QPI link 0 and one exact MSI IOH CPU-only reset has made that link
operational at 4.8 GT/s, does one final CSI invocation take the static
resume/terminal branch and return useful state?

The supported test configuration is deliberately exact:

```text
board: MSI X58 Pro-E
CPU: Intel Xeon E5645, CPUID 000206c2
microcode: 0000001f
DIMM: BLS4G3D1609DS1S00., sole SPD responder 0x54, FNV-1a fb66b530
memory ratio: 6
initial QPI: Slow
UART: COM1 03f8, 115200 8N1, no flow control
flash: socketed W25Q128.V..M, 16 MiB, 4 MiB image top-aligned
boot type for first test: true AC-cold
```

No other CPU, DIMM, topology, ratio, or QPI policy is accepted.  The offset
field names that are not established in public documentation remain neutral.

## Local-only wrapper patch

No patched wrapper or other proprietary MSI byte is stored in the public
tree.  The deterministic tool
[`scripts/x58_b06v8_wrapper_patch.py`](../scripts/x58_b06v8_wrapper_patch.py)
operates only on a locally composed, user-supplied 4 MiB image.  It verifies
the complete original wrapper and complete input image, changes exactly one
byte, verifies the resulting wrapper, and writes a separate local file.

```text
wrapper runtime:          fffc04e2..fffc0d18
wrapper 4-MiB offset:     003c04e2
wrapper length:           00000837
original wrapper SHA-256: 546f0c4d455ce250b3fc6047d3864b25d6fd16e7cf1c3bfaa6b61d69510af6c3
original wrapper FNV-1a:  65b20e50

patch runtime address:    fffc0bc2
patch wrapper offset:     000006e0
patch 4-MiB offset:       003c0bc2
patch W25Q128 offset:     00fc0bc2
opcode:                   74 -> eb
instruction:              JE fffc0bcd -> JMP fffc0bcd

patched wrapper SHA-256:  8ffd927db21a22614bf446264575216138ed012b49c90f135729bb38c153c9dc
patched wrapper FNV-1a:   986153c5
```

The patch does not force selector byte `state[7]` directly.  It forces the
complete wrapper input parser to execute even while standard CMOS `0x0e=0xec`
is the persistent no-loop guard.  Before every CSI call B06V8 reads, prints,
and requires the exact retained inputs:

```text
EXT80/81/82/88/89 = b3/19/d7/67/cb
X58 class/revision dword  = 06000013
```

`EXT88/89=67/cb` is not a valid complemented pair, so the wrapper falls back
to `(EXT80 & 0x0e) >> 1 = 1`.  `EXT80[0]=1` produces `state[6]=1` on the
E5645.  `EXT82=d7` and IOH revision `0x13` produce the four exact pairs:

```text
state[1c/1d]   = 0/1
state[70/71]   = 0/1
state[cd/ce]   = 0/1
state[121/122] = 0/1
```

Those fields affect CSI's paired-link programming; they are not padding.  The
old experimental `state[7]` immediate patch must never be combined with the
branch patch.  B06V8 performs no extended-CMOS data write.

## Reset-safe three-pass automaton

Standard CMOS `0x0e` is changed from the live High-QPI authorization `0x2c` to
`0xec` before the first call and retained until an operator explicitly clears
it.  Three unique
five-byte I801 tuples distinguish pass 2, consumed/in-progress, and pass 3.
All tuples keep `START` clear and use complemented command/address and data
pairs:

```text
pass 2:              08:31:ce:68:97
consumed/in-progress:08:42:bd:7a:85
pass 3:              08:53:ac:6b:94
```

The pass-2 tuple is based on demonstrated I801 retention over CSI's internal
CPU-only reset.  Retention of the pass-3 tuple over the outer IOH reset is a
hardware inference and is the principal automaton risk.  Loss or ambiguity of
either CMOS or I801 state falls back to ROMMON without another automatic CSI
call.  A returned pass-1 call and every pass-2/pass-3 call are marked consumed
before any retry could occur.

On configured reentry, an exact pass-2 or pass-3 signature is copied into the
CAR entry snapshot and immediately replaced by the consumed tuple, before
fallible UART reporting, SPD access, PCIEXBAR/QPI gates, or wrapper arming.
The captured snapshot still selects the intended phase; consumed is reasserted
immediately before the one vendor call.  Thus a failure anywhere in the
pre-call path cannot make a later warm reset retry that phase.

On `COLD_DEFAULT` entry B06V8 first establishes the ordinary SMBus I/O decode,
then passively captures and prints the five idle host registers before any SPD
transaction.  A tuple equal to any B06V8-owned pass-2, consumed, or pass-3
signature is rejected.  Other values are telemetry rather than assumed reset
defaults.  The first experiment still requires a true AC-off cold start.

### Pass 1

From exact AC-cold entry, B06V8 verifies CAR, CPU, microcode, SPD, ratio,
PCIEXBAR, X58 identity, CMOS inputs, and a strict composite Slow-QPI endpoint
predicate:

```text
CPU ff:02.1 50/54/6c/80 = 160c0110/00000010/0000a020/030f0f03
CPU ff:02.1 94/9c/a0/a4 = 00000102/00000502/00000c00/001d2c03
IOH 00:0d.0 82c/840     = 00006020/030f0f03
IOH 00:0d.0 854/85c/864 = 00000102/00000002/00322808
IOH 00:14.1 SR0/SR1     = 00000000/00000000
IOH 00:14.2 SYRE.cc     = 00000200 on pass 1, 00000600 on pass 2
```

The individual values above come from immutable B06V6 observations, but the
complete tuple was not captured jointly at both AC-cold and pass-2 entry.
Consequently this is an intentionally fail-closed hardware hypothesis, not a
claim that both complete entry tuples have already been demonstrated.

It writes the pass-2 signature, invokes CSI once, and expects CSI's internal
`IOH.SYRE.CPURESET` path not to return.  Any ordinary return is consumed and
falls back.

### Pass 2

After the reset vector re-enters with the exact pass-2 signature and CMOS
guard, B06V8 consumes the signature and calls CSI once.  Acceptance requires:

```text
EAX/EBX/ECX = 1/0/000002a6
state[06/07/2ef/301] = 1/1/1/0
state pairs = 0/1 at 1c/1d, 70/71, cd/ce, and 121/122
runtime canaries, wrapper/CSI signatures, stack return and call-result copy exact
```

The raw `0x304`-byte CSI FNV is printed but deliberately not gated because two
valid live histories differed.  The complete paired pre-reset tuple is then
required:

```text
CPU ff:02.1 50/54/6c/80 = 160c0110/00000012/0040a0a0/030f0f03
CPU ff:02.1 94/9c/a0/a4 = 00010202/00000502/00000c00/00322808
IOH 00:0d.0 82c/840     = 004060a0/030f0f00
IOH 00:0d.0 854/85c/864 = 00010102/00000002/00322808
IOH 00:14.1 SR0/SR1     = 00000000/00000000
IOH 00:14.2 SYRE.cc     = 00000600
```

Only then is the pass-2 acceptance announcement emitted and UART drained.
The pass-3 signature is committed immediately before the single outer MSI
sequence, which is dword-wide and bounded:

```text
require 00:14.2:cc == 00000600
write   00:14.2:cc = 00000200; require readback 00000200
write   00:14.1:7c = 00000000
write   00:14.1:80 = 00000000; require both zero
reread  00:14.2:cc; require exact 00000200
POST cb
POST fe
write   00:14.2:cc = 00000600
CLI; HLT forever if reset does not occur
```

There is no CF9 reset, no extra SYRE retry, and no extended-CMOS phase write.
The final config write normally resets the CPU before a readback can retire.

### Pass 3

Pass 3 requires the exact retained pass-3 signature, CMOS `0x0e=0xec`, safe
PCIEXBAR decode, and the observed operational-High tuple.  CPU link offset
`0x50` may be `0x86000000` or the separately observed `0x96000000`; bit 28 is
not treated as a speed proof.  CPU link `0x58=0x00064555`, IOH link
`0xc8=0x0606fc00`, the ratio registers, both physical endpoint tuples, SR0/1,
SYRE, and both public PH_PIS operational masks must still match.

The pass is marked consumed before CSI.  If CSI returns, B06V8 prints the
complete `0x304`-byte state, call/runtime status, full endpoint tuple, and then
enters CAR ROMMON.  No return registers or digest are assigned success
semantics.  If CSI internally resets instead, the consumed signature prevents
an automatic fourth call.

## POST expectations

```text
02  B06V8 automatic path entered
03  platform/SPD/PCIEXBAR/CMOS gates accepted
04  pass 1 armed
05  pass 2 armed
ca  pass-2 return and complete tuple accepted
cb  caller reset sequence committed
fe  exact MSI outer IOH SYRE request about to be asserted
cf  pass 3 armed
d9  pass 3 returned
da  pass-3 telemetry complete; terminal CAR path
1f  persistent guard/reset-sequence fail-stop
20  automatic path rejected; recovery ROMMON
d1  CSI call entered
d2  pass-1/pass-2 CSI call returned
```

Expected progress for a successful probe is reset-vector/serial re-entry after
pass 1, again after POST `fe`, then POST `cf`; POST `d9/da` occurs only if the
unknown third call returns.  A stop at `cf` can mean CSI reset, HLT, or hang and
must not be called success without the next immutable log.

## Failure and recovery

Any failed exact gate enters CAR ROMMON without another automatic vendor call;
the persistent guard deliberately remains `0xec`.  A failure after a phase
commit instead enters a terminal fail-stop and attempts to change CMOS to the
independent `0xed` fail-lock.  If that second write itself fails, the terminal
message requires AC removal before any reboot because the old I801 signature
could still look valid.  After an AC-off recovery boot, re-arm with:

```text
rommon> unlock RESET
rommon[R]> autoguard clear
```

The command consumes one reset authorization, accepts exact `0xec` or the
B06V8-only `0xed` fail-lock, restores the isolated `0x2c` authorization with
readback, and requires removal of AC power again before another test.  Do not
issue CF9 or a second manual SYRE reset.  The socketed flash,
external programmer, verified vendor backup, and true AC removal are mandatory
recovery prerequisites.

Each hardware run must record the standard project metadata plus all three
serial segments, POST trace, whether each reset returned, final I801/CMOS
phase, and whether recovery was required.  Preserve failed logs rather than
overwriting them.

## Build and local composition

The public base contains no vendor module.  See
[`builds/experimental/msi-x58-pro-e-b06v8-manifest.md`](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
for pinned image hashes and commands.  The intended sequence is:

1. build the public base from `configs/x58-pro-e-b06v8.config`;
2. compose the user's verified MSI ranges with `msi_vendor_blobs.py`;
3. run `x58_b06v8_wrapper_patch.py patch` with the complete unpatched input
   SHA-256;
4. verify the patched wrapper;
5. top-align that patched 4 MiB image in a 16 MiB W25Q128 image;
6. independently compare the lower 12 MiB to erased `0xff` and the upper
   4 MiB to the patched composite.

Static derivation and the evidence boundary are detailed in
[`research/msi/vendor-qpi-blob-inventory-2026-09-04.md`](../research/msi/vendor-qpi-blob-inventory-2026-09-04.md).
