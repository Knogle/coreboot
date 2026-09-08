> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VB High-QPI MINIT observation

Status: **BUILT / STATIC / HARDWARE TESTED FOUR TIMES FROM G3; PASS 1 AND
PASS 2 SUCCEEDED, PASS 3 FAILED CLOSED BEFORE CSI/MINIT**.

B06VB is a one-shot vendor-assisted observation build. It is designed to
accept only the exact B06VA pass-3 CSI result already measured on hardware.
If that admission succeeds, it constructs the reviewed cold MINIT policy,
invokes MINIT once, prints all retained evidence, and terminates in CAR. It
does not claim working DRAM.

## Hardware result: four controlled G3 sequences

The original build-time status was `BUILT / STATIC / NOT RUN`. That status is
superseded by four real G3 captures from 2026-09-04 using the exact E5645,
microcode `0x1f`, sole-SPD-`0x54`, DDR-ratio-6 lab configuration. All four
sequences reproduced the intended first two phases:

1. pass 1 entered CSI and took the expected CSI-internal reset;
2. pass 2 returned and was accepted with `EAX/EBX/ECX=1/0/2a6`, raw CSI-state
   FNV `856a313b`, state bytes `01/01/08/01/00`, and the exact pass-2
   CPU/IOH/link tuple;
3. the caller issued the one intended IOH `SYRE` CPU-only reset and the next
   reset-vector entry selected phase 3 in operational High-QPI state;
4. phase 3 rejected the platform before calling CSI because CPU
   `ff:02.1:0xa0` was outside B06VB's original exact set
   `{00017000, 00017600}`.

The observed phase-3 boundary was:

| G3 capture | Pass-3 CPU `ff:02.1:0xa0` | Result |
|---|---:|---|
| run 1 | `00017a00` | exact-set mismatch, `CODE=0d`, CAR ROMMON |
| run 2 | `00017800` | exact-set mismatch, `CODE=0d`, CAR ROMMON |
| run 3 | `00017a00` | exact-set mismatch, `CODE=0d`, CAR ROMMON |
| run 4 | `00017a00` | exact-set mismatch, `CODE=0d`, CAR ROMMON |

The remaining High-QPI selector fields matched. Run 1 additionally recorded
the complete CPU/IOH/link tuple from ROMMON and repeated A0 reads, all of which
retained `00017a00`; run 2 repeated A0 reads at `00017800`. Together with the
earlier `00017000` and `00017600` observations, the exact observed High-QPI
A0 set is now:

```text
{ 00017000, 00017600, 00017800, 00017a00 }
```

Only bits 11:9 vary across that set, but neither their field width nor their
meaning is established. Treating A0 as reset- or training-variable PHY state
is an inference from the repeated otherwise-identical endpoints, not a named
register-field claim. These captures do not establish a deterministic DDR
ratio encoding.

The fail-closed behavior worked as designed: every run reported
`B06VB_HIGH_CSI_PROFILE STATUS=platform-state CODE=0d`, emitted the automatic
fallback, and retained the recovery ROMMON. No run made the third CSI call,
emitted POST `d3`, invoked MINIT, accessed ordinary DRAM, or reached postcar,
ramstage, graphics, or a payload. This is therefore four hardware executions
of the guarded QPI path, but zero MINIT attempts and zero DRAM-init results.

Immutable local-only captures, not for redistribution:

```text
489122279ba08c8dfa18f656783b7d8a76e6dda79a29bc8ab807bd0cc16bf617  2026-09-04-b06vb-g3-run1-runtime.raw
d0b648593524b0db1be3782c905da56042c25cdb8eaa1aec66df213529ccd1bc  2026-09-04-b06vb-g3-run2.raw
f761d2672fe44dfe329cc65ea2778b7a02ff91e92f6689eeb10d3a65fc7138c9  2026-09-04-b06vb-g3-run3.raw
d1aabbec74f1e8fa4f32e96a45aa5ea112504092c6082384c05779b5c512ef9e  2026-09-04-b06vb-g3-run4.raw
```

## Intended exact admission contract

The four B06VB hardware runs did not reach this contract because they stopped
at the preceding pass-3 platform preselector.

```text
EAX/EBX/ECX = 00000000/00000000/00000011
CSI state FNV-1a-32 = 8b38506a
retained I801 signature = 08:42:bd:7a:85
CMOS 0e = ec
policy base FNV = bc4268bf
seven-edit policy FNV = 3c0f3a0b
```

Immediately before POST `d3`, firmware atomically commits and verifies the
unique one-shot signature `08:64:9b:5c:a3`, while CMOS remains `ec`. It is
never cleared automatically and makes a reset continuation fail closed.

## Observable boundary

```text
d3  immediately before the MINIT call
d4  immediately after MINIT returns
da  terminal observation complete
20  safe fallback into CAR ROMMON
```

Before `d3`, serial includes `HIGH_CSI_OBSERVATION=ACCEPTED_FOR_ONE_MINIT_CALL`,
the policy hashes, and `B06VB_MINIT_IN_PROGRESS`. If MINIT returns, B06VB prints
the call result, policy/workspace hashes, post-call QPI fingerprints, the full
772-byte CSI state and full `0x2bcc`-byte workspace. It classifies the return
as `FULL_PATH_RETURN_DRAM_UNTESTED` or
`RETURNED_UNKNOWN_OR_INCOMPLETE_DRAM_UNTESTED`. Neither proves usable memory.

A stable `d3` without `d4` means only that MINIT was entered and did not
return. POST `d4` proves a return to the caller, not DDR training. B06VB never
accesses ordinary DRAM and cannot enter postcar, ramstage, PCI enumeration,
graphics initialization, or a payload.

## Original first-hardware procedure

The procedure below is retained as the build's original test plan. The four
completed runs stopped at the earlier pass-3 A0 gate and therefore never
reached its MINIT-specific `d3` boundary.

Capture COM1 at 115200 8N1 and the POST board from before power-on. If the
guard is not already at cold authorization `2c`, use `unlock RESET` followed
by `autoguard clear` in the preceding ROMMON and require `ec`/`ed -> 2c`.
Then perform a real G3 AC removal. Do not substitute a warm or CF9 reset, and
do not interrupt the expected QPI reset continuations.

After a return, preserve the automatic full-buffer output. Optional independent
read-only checks are:

```text
id
resetcause
vinputs
vinfo
vstate 0 e0
vstate e0 e0
vstate 1c0 e0
vstate 2a0 64
vpolicy dump 0 e0
vwork 0 80
vwork e40 80
```

Do not run `vminit`, enable writes, probe ordinary DRAM, or clear the guard
until evidence is archived. Proprietary MSI modules and composed images are
local-only and must not be redistributed.
