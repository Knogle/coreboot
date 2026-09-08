> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](../../Documentation/PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../../Documentation/mainboard/msi/x58_pro_e.md).

# Vendor QPI and post-MINIT candidate inventory

> **2026-09-04 RTC-bank erratum:** B06V8's alleged extended-CMOS tuple was
> standard RTC clock/calendar data because ICH10 U128E was clear and ports
> `0x72/0x73` aliased `0x70/0x71`. Preserve the analysis below as historical
> control-flow archaeology, but do not use its tuple as a hardware policy.
> B06V9 directly establishes the observed successful CSI state with six
> hash-pinned patch regions and exact-gates U128E.

Date: 2026-09-04.  Status: **verified-static**, except where a result is
explicitly attributed to the immutable B06V6 hardware record.  This analysis
did not access hardware.  Proprietary objects named below remain local and
ignored; only hashes, offsets, interfaces, and independent observations belong
in the repository.

## Decision

The only justified executable vendor candidate for the next isolated
High-QPI experiment is the already exercised MSI CSI wrapper plus
`CSI_INITDLL`, with one hash-pinned branch-byte patch that makes the existing
diagnostic/retry guard take the wrapper's complete extended-CMOS policy path.
The observed invalid policy pair then selects ratio policy 1 through the
original fallback logic.  This combination has already staged both the CPU
and IOH endpoints and produced a successful Slow-to-High transition when
followed by the MSI IOH CPU-only reset sequence.

No additional Intel, Apple, or ASUS PEIM is a defensible direct-call candidate
for B06V8.  Those images are valuable disassembly and revision-diff oracles,
but their PEI services, private contexts, board policy, and reset contracts are
not reproduced.  Two small MSI routines after the CSI/MINIT calls are also not
appropriate B06V8 calls: one mutates board-specific I/O banks after detecting
an operational QPI PHY, while the other has an unusual return-address-as-data
contract and expects a larger handoff at physical 1 MiB.

The actionable B06V8 experiment is therefore a bounded three-pass CSI probe:
patch that one wrapper branch, retain phase outside the vendor state,
perform exactly one documented IOH `SYRE.CPURESET` after the confirmed pass-2
request, invoke CSI once in the resulting High-QPI state, capture everything,
and stop in CAR.  It must not call MINIT or any post-MINIT helper automatically.

## Authoritative MSI objects

The source is the complete 4 MiB image, mapped at `0xffc00000`:

```text
path:   blobs-local/msi-x58-pro-e/7522v8F/A7522IMS.8F0
size:   0x00400000
SHA256: ee7912da81a3cf47b6bedef7dedfa56cbc31723f5cdd7f0d75a7f64e336c04a8
```

The exact local extracted objects are:

| Object | Source/runtime interval | Size | Entry | Raw SHA-256 |
|---|---:|---:|---:|---|
| CSI wrapper | `0x3c04e2..0x3c0d19` / `0xfffc04e2..0xfffc0d19` | `0x837` | `0xfffc04e2` | `546f0c4d455ce250b3fc6047d3864b25d6fd16e7cf1c3bfaa6b61d69510af6c3` |
| wrapper helper | `0x3c1554..0x3c1579` / `0xfffc1554..0xfffc1579` | `0x25` | `0xfffc1554` | `6641f8cc9bfb9b08df0f39b688cbeeab93a0c92bff0d47ee565e525dd3179d75` |
| `CSI_INITDLL` | `0x3e6de0..0x3ee3c0` / base `0xfffe6de0` | `0x75e0` | `0xfffe7000` | `e7c42f1a3474fc007c6d0731dc94f753f1fb0367d1ddbf8a3437edeb1daeb150` |
| `MINITDLL` | `0x3c1dc0..0x3da4c0` / base `0xfffc1dc0` | `0x18700` | `0xfffc2000` | `54fb7d14c1d88c0a42ea0e7b51511735983686b79fdf9db64e5ace2fc651264a` |

Local files are under
`blobs-local/msi-x58-pro-e/b06v0/vendor-init/`.  The raw and loaded copies of
both PE32 images are byte-identical in this ROM because section raw offsets and
RVAs are congruent.  Both images have relocation directories but no imports or
exports; this is not evidence of a portable ABI.

The wrapper is 32-bit cdecl with three arguments.  Its first two inputs are
unused; the low byte of the third becomes state byte `+0x0b`.  It allocates a
`0x304`-byte state object and calls `fffe7000(&state, &state)`.  On return:

```text
EAX = zero-extended state[0x2fc]
EBX = zero-extended state[0x301]
ECX = dword state[0x2f8]
EDI = address of the transient state object
```

The direct MINIT entry is cdecl
`fffc2000(policy_0xe0, workspace)`.  The currently reconstructed successful
workspace size is `0x2bcc`; its private policy, non-local failure return, CSI
precondition, and phase-sensitive reset state make it an MSI-assisted research
component rather than an `mrc.bin`-style interface.

## Exact guarded-policy patch

An earlier experiment patched the wrapper's default caller-state selector:

```text
fffc0515: mov al,0
fffc0517: mov [edi+7],al
```

That earlier patch changed the immediate at runtime address `0xfffc0516`:

```text
wrapper offset:        0x34
4 MiB source offset:   0x3c0516
16 MiB top alignment:  0xfc0516
original -> patched:   00 -> 01
```

Its resulting private wrapper was pinned by:

```text
size:        0x837
SHA256:      77487de80d09b16acc9b7f1ce3876b3f791179b86d0f44e4340a79b98cdccde2
FNV-1a-32:   6ef22ffb
```

It is not the correct B06V8 patch.  It changes only a default that the complete
policy path overwrites, while the diagnostic-invalid path skips four material
per-link policy pairs.  B06V8 instead changes the conditional branch at:

```text
runtime instruction:    0xfffc0bc2: 74 09  je 0xfffc0bcd
wrapper offset:         0x6e0
4 MiB source offset:    0x3c0bc2
16 MiB top alignment:   0xfc0bc2
original -> patched:    74 -> eb
patched instruction:    0xfffc0bc2: eb 09  jmp 0xfffc0bcd
```

The composer must first verify the complete original wrapper hash and the
original two instruction bytes `74 09`.  The resulting local-only B06V8
wrapper is pinned by:

```text
size:        0x837
SHA256:      8ffd927db21a22614bf446264575216138ed012b49c90f135729bb38c153c9dc
FNV-1a-32:   986153c5
```

At `fffc0bb8..fffc0ca2`, the wrapper selects standard CMOS diagnostic index
`0x0e` through ports `0x70/0x71` and tests bits 7:6:

- in the unmodified wrapper, when those bits are nonzero, `fffc0bc4` sets
  temporary `SI=1` and jumps directly to `fffc0ca2`; extended CMOS is not read
  and the resulting caller state is `+0x06/+0x07=1/0`, with all four link
  policy pairs retaining their initialized `1/1`; the earlier immediate patch
  changed only that resulting `+0x07` to 1;
- the B06V8 `74 -> eb` patch makes either diagnostic result enter the original
  full policy path at `fffc0bcd`; it does not hard-code a ratio or any caller
  state byte;
- when those bits are zero, a valid complemented `EXT88/89` pair overwrites the
  selector with `EXT88 & 0x3f`; for example `67/98` selects untested value
  `0x27`;
- when that pair is invalid, the fallback is `(EXT80 & 0x0e) >> 1`; the live
  `EXT80=0xb3` produces selector 1.

The exact observed input tuple was `CMOS0E=ec`, `EXT80=b3`, and
`EXT81/82/88/89=19/d7/67/cb`.  `67/cb` is deliberately classified invalid by
the original code because `((cb ^ 67) + 1) & 0xff = ad`, not zero.  The
patched branch therefore derives, rather than forces:

```text
state[0x07] = (b3 & 0x0e) >> 1 = 1
state[0x1c/0x1d]   = 0/1
state[0x70/0x71]   = 0/1
state[0xcd/0xce]   = 0/1
state[0x121/0x122] = 0/1
state[0x06] = b3 & 1 = 1
```

The four pairs result because `EXT82=d7` has bit 0 set and the exact gated IOH
revision byte at `0xe0000008` is `0x13`: `fffc0c40..fffc0c95` preserves the
extended-CMOS bit in the second byte and clears the first byte.  Then
`fffc0c96..fffc0cb6` obtains `SI=1` from `EXT80[0]`; the E5645's CPUID is not
the one exceptional `06a0` value, so caller state `+0x06` becomes 1.

These four pairs are not inert padding.  CSI `fffe85d8` obtains the caller
state through `[context+0x13b]`.  At `fffe8633` plus a `0x54` record stride and
at `fffe869c`, it tests the second bytes (`+0x1d/+0x71` and
`+0xce/+0x122`); a zero calls `fffe82a6`, which clears unknown link-register
field bits 21:20 at physical offset `0xc4` or `0x48`, whereas a nonzero value
enters paired-endpoint logic that can set `0x00300000`.  At `fffe8802` plus the
same stride and at `fffe8866`, it tests the first bytes (`+0x1c/+0x70` and
`+0xcd/+0x121`); a zero calls `fffe8238`, which clears bit 18, whereas a
nonzero value enters the paired path that can set that bit.  Their public
semantic names remain unknown, but `1/1` from the old bypass is materially
different from the hardware-proven `0/1` tuple.

B06V8 must require the exact read-only extended-CMOS tuple before every call,
including `EXT82=d7`, and must not write `EXT80`, `EXT82`, `EXT88`, or `EXT89`.
No direct port-`0x72/0x73` access was found in the hash-pinned CSI DLL; this
parsing belongs to the outer wrapper.  Writing `EXT41/be` was fidelity to the
original MSI caller, not a demonstrated CSI/IOH dependency, and is not needed
when the coreboot state machine owns phase.  The old `+0x07` immediate patch
must not be combined with this branch patch.

Selector 1 is decoded by `fffe7d85` to internal ratio byte `0x12`, which is
then programmed through the discovered CPU and peer link structures.  Public
Nehalem register documentation identifies `0x12` as QPI PLL ratio 18.  On X58,
the IOH `FREQ` encoding used in this configuration is 4.8 GT/s; “High” here
means operational 4.8 GT/s rather than the E5645's maximum supported QPI rate.

## CSI reset and pass structure

The CSI entry begins at `0xfffe7000`.  Its relevant verified chronology is:

```text
fffe7021  construct/discover local context and touch IOH policy
fffe707e  call fffedcd7, the internal CPU-only-reset helper
fffe709d  call fffeab4f, the main ratio/peer-side branch
fffe715f  fold result class into caller result fields
fffe716a  final visible checkpoint before return handling
```

The helper at `fffedcd7` calls `fffe7241`; when a reset is still required it
stages a CPU-Uncore value, reads IOH `00:14.2:0xcc`, writes it with bit 10
clear, then writes it with bit 10 set and enters a permanent HLT loop at
`fffede5f`.  It does not use CF9 and it does not clear IOH scratch registers
`00:14.1:0x7c/0x80`.  There is no local `cli` instruction in the bounded helper,
so it relies on the caller's interrupt state before its HLT loop.

Function `fffe7241` is not a simple speed-status read.  It sums per-active-link
bytes at `0x44`-byte strides in two local groups and compares the aggregate to
topology-dependent expected values.  In `fffeab4f`, when the structure byte at
`[context+0x13b]+6` is nonzero and this aggregate predicate succeeds, CSI skips
`fffe7d85` and the larger paired programming path; it instead calls
`fffe8a7f` and `fffedb0f` and returns.  If the predicate fails, it executes the
selector decoder and the peer/program/trigger sequence.

This control flow is the strongest static reason to try one third CSI pass
after the link has become operational.  It suggests a terminal/resume path,
but does not prove that pass 3 will return, what tuple it returns, or that its
return means completion.

The observed pass contract for the one supported lab configuration is:

| Pass | Observed/expected behavior |
|---|---|
| 1 | CSI does not return; its internal `SYRE.CPURESET` path returns through the reset vector. |
| 2 | CSI returns `EAX=1`, `EBX=0`, `ECX=0x2a6`; caller state `+0x06=1`, `+0x07=1`, `+0x2ef=1`, `+0x301=0`; this requests the one outer CPU-only reset. |
| 3 | Unknown.  Call once in the proven operational-High state, capture the complete `0x304` state and register tuple, then halt in CAR regardless of return values. |

`ECX=0x2a6` is a composite result/reset-reason mask assembled by
`fffe72dd`, not an error program counter.  Two valid pass-2 histories produced
different raw state FNVs (`856a313b` and `92e228e7`), so B06V8 must gate the
logical fields and endpoint tuple, not one complete raw-state digest.

## Paired endpoint evidence and reset gate

The successful live record in
[b06v6-qpi-high-2026-09-04.md](b06v6-qpi-high-2026-09-04.md) proves that CSI
programs both sides before the outer reset.  The exact pass-2 pre-reset tuple
was:

| Endpoint | Offset | Value |
|---|---:|---:|
| CPU `ff:02.1` | `0x50` | `0x160c0110` |
| CPU `ff:02.1` | `0x54` | `0x00000012` |
| CPU `ff:02.1` | `0x6c` | `0x0040a0a0` |
| CPU `ff:02.1` | `0x80` | `0x030f0f03` |
| CPU `ff:02.1` | `0x94` | `0x00010202` |
| CPU `ff:02.1` | `0x9c` | `0x00000502` |
| CPU `ff:02.1` | `0xa0` | `0x00000c00` |
| CPU `ff:02.1` | `0xa4` | `0x00322808` |
| IOH QPI0 `00:0d.0` | `0x82c` | `0x004060a0` |
| IOH QPI0 `00:0d.0` | `0x840` | `0x030f0f00` |
| IOH QPI0 `00:0d.0` | `0x854` | `0x00010102` |
| IOH QPI0 `00:0d.0` | `0x85c` | `0x00000002` |
| IOH QPI0 `00:0d.0` | `0x864` | `0x00322808` |
| IOH reset `00:14.2` | `0xcc` | `0x00000600` immediately after pass 2 |

With PCIEXBAR `0xe0000000`, the five extended IOH addresses are
`e006882c/e0068840/e0068854/e006885c/e0068864`.  Offset `0x85c` has no name
in the public register set used here and must remain neutral.

The original outer caller's exact CPU-only reset body is
`fffc1330..fffc137e` (exclusive end), 78 bytes, SHA-256
`e3592722583aa27aed69d5945ffa95c82e94fae71d3ab5d9a566906f0c0dfe06`:

1. dword-read `00:14.2:0xcc`, clear bit 10, dword-write it;
2. dword-write zero to `00:14.1:0x7c` and `0x80`;
3. emit POST `0xfe`;
4. reread `00:14.2:0xcc`, set bit 10, dword-write it;
5. execute `cli`, then remain in an HLT loop if reset does not arrive.

There is no CF9 access, delay, completion poll, or `wbinvd` in this path.
Configuration cycles provide ordering; B06V8 should additionally finish its
serial message at TEMT before entering the exact sequence.  The immutable raw
log proves that pass 2 returned with `SYRE=0x00000600`; `0x00000200` was the
AC-cold value and the explicit clear value, not the pre-outer-reset value.
Therefore the pre-outer gate must require `0x00000600`, the caller must clear
bit 10 and require a `0x00000200` readback before touching `SR0/SR1`, and only
then may it set bit 10 to produce `0x00000600` again.  A post-reset live read
also remained `0x600`, even though the public datasheet describes the request
bit as self-clearing.

After the one successful edge, the operational tuple was:

```text
CPU ff:02.1 50/54/6c/80 = 160c0112/00000012/0040a0a0/070f0f03
CPU ff:02.1 94/9c/a0/a4 = 00010202/00000502/00017600/00322808
IOH 00:0d.0 82c/840     = 004060a0/070f0f03
IOH 00:0d.0 854/85c/864 = 00010102/00000002/00322808
CPU ff:02.0 58          = 00064555
IOH 00:10.0 c8          = 0606fc00
IOH 00:14.2 cc          = 00000600
```

For `PH_PIS`, the public operational predicate is:

```text
(value & 0x041f1f01) == 0x040f0f01
```

This tests operational-speed bit 26, RX/TX states `0xf`, and link-up bit 0;
it deliberately does not assign reserved bit 1.  Both CPU `ff:02.1:0x80` and
IOH `00:0d.0:0x840` must pass it.

CPU link-layer `ff:02.0:0x50` was historically `0x86000000`, while another
successful full-CSI/MINIT history stably reported `0x96000000`.  The sole
difference is bit 28.  The public CPU material used here establishes bit 31 as
QPILS but does not establish bit 28 as a speed selector; analogous later-server
material only suggests a retry/allocation role and is insufficient to name the
field on this CPU.  B06V8 should accept exactly `0x86` or `0x96` in the high
byte (or omit this field from the speed gate), never write bit 28, and derive
High status from the paired ratio/PLL/`PH_PIS` evidence instead.

The tuple immediately after the CPU-only reset is operational but not identical
to the terminal vendor-booted snapshot: the latter has CPU `0x6c=0040a0a8`,
`0x9c=00b00502`, and `0xa0=00017000`.  That difference is further evidence for
observing pass 3 before attempting High-QPI MINIT; it is not a register-replay
recipe.

## Persistent three-pass guard

Vendor CAR state cannot survive CPU reset.  IOH `SR0/SR1` are unsuitable for
the phase because the original outer caller deliberately zeroes them.  The
`EXT88/89` vendor cookie is unnecessary and must remain at the proven invalid
`67/cb` pair; standard diagnostic `CMOS0E=ec` remains the independent retry
guard while I801 holds the detailed phase.

The least invasive phase store is the five idle I801 host registers while
`START` remains clear.  Survival of a complete five-byte signature across
CSI's internal reset is proven.  Ordinary I801 state also survived the
successful outer reset, so survival of the new exact pass-3 signature is a
strong, explicit hardware hypothesis rather than an already repeated fact.
Proposed distinct complemented tuples are:

```text
pass-2 entry:  HSTCTL/HSTCMD/XMITADD/DAT0/DAT1 = 08/31/ce/68/97
consumed:                                      08/42/bd/7a/85
pass-3 entry:                                  08/53/ac/6b/94
```

The safe state machine is:

1. AC-cold entry requires the exact platform, CPU, SPD, strict composite
   Slow-QPI predicate, `CMOS0E=2c`, and exact read-only
   `EXT80/81/82/88/89=b3/19/d7/67/cb`.  The individual Slow-QPI values are
   evidence-backed but were not all jointly captured at both entries, so this
   gate remains a fail-closed hypothesis.  After enabling SMBus I/O decode but
   before the first SPD transaction, passively capture the five host registers
   and reject any tuple equal to a B06V8-owned pass-2, consumed, or pass-3
   signature.  Do not require an invented all-zero reset value; still require
   a true AC-off start.  Set only CMOS `0x0e=0xec`, install the pass-2
   signature, verify both, and call patched CSI pass 1.
2. Reentry requires `0xec` plus the exact pass-2 signature.  Preserve the
   captured tuple in CAR, replace the hardware tuple with `consumed`
   immediately after SMBus decode and before any fallible reporting or gate,
   then reassert `consumed` immediately before calling CSI pass 2.  Accept only
   the logical result and exact paired pre-reset tuple above.
3. Require the complete paired pass-2 tuple including `SYRE=600`.  Emit and
   flush the phase announcement, then install and verify the pass-3 signature
   immediately before clearing SYRE bit 10; verify the
   `SYRE=200` readback, clear and verify `SR0/SR1`, reread and require exact
   `SYRE=200`, emit POST `cb` then `fe`, set the bit again from that final
   read, and execute the one MSI CPU-only reset.  Do not use reset merely to
   obtain a clean monitor context.
4. Reentry requires `0xec`, the exact captured pass-3 signature, the unchanged
   read-only extended-CMOS tuple, and both High endpoint predicates.  Consume
   the hardware tuple immediately after decode and reassert `consumed` before
   calling CSI pass 3.  Dump return
   registers, the full `0x304` state, all endpoint fields, and halt in CAR.
   Do not interpret or retry automatically.

The second manual CPU-only reset attempted at already-active High did not
return because it was issued only to clear ROMMON context, without a fresh CSI
reset request and restaging.  The exact internal failure point is unknown.
This is sufficient reason to permit exactly one outer reset per cold sequence
and to consume every phase before its hazardous action.

## MSI routines after CSI/MINIT

The original orchestration region calls CSI, conditionally calls MINIT, invokes
the reset dispatcher, and only then reaches two additional helpers:

```text
fffc02cf  CSI wrapper
fffc02eb  fffc1610: persist return/phase in EXT88/89 and board state
fffc0313  MINIT wrapper
fffc033a  fffc12e5: reset dispatcher
fffc0342  fffc17f6: operational-QPI-dependent board I/O helper
fffc034a  fffc1380: handoff-region check
fffc0357  fffc13b5: physical-1-MiB handoff/scratch builder
```

The phase/cookie helper `fffc1610..fffc16a4` is 148 bytes with SHA-256
`c67c542d3075f42eabc7f12953ed5da3866e9a0b8b56e9327ee4ec56462caaa7`.
In the successful manual reproduction its caller persisted the returned phase
as valid `EXT88/89=67/98`; a subsequent wrapper invocation would therefore
decode selector `0x27`, not 1.  B06V8 intentionally does not call this helper:
it retains the invalid `67/cb` pair and stores its private one-shot phase only
in the separately gated diagnostic byte and I801 signature.

### `fffc17f6`: concrete but not yet safe to call

The exact function is `fffc17f6..fffc185e`, 104 bytes, SHA-256
`c5db8dacc26a8ae9f20177b6111ca1d82f918ece9485266cd84f30c0bd7d5e03`.
It calls:

- `fffc1466..fffc149c`, SHA-256
  `579f187e66f559062b1376fe9a0d4e781cacb57e294806e420b116ca2fb1aaec`;
- `fffc149c..fffc14f4`, SHA-256
  `d5a219e5e83984c29c4f08dc61c58454f46f08d1c0a0b690be87095e40d7bff7`.

It first checks an ICH/LPC and board-I/O condition.  It then tests bit 7 of
IOH physical-link control at `e006882c` and `e006982c`, reads IOH
`00:14.2:0xd0[1:0]`, and uses those bits to mutate bit positions 12 and 8 in
I/O-register banks beginning at ports `0x500/0x504/0x518/0x51c`.  The public
meaning of those board I/O registers is not established.

This helper is plausibly part of post-QPI clock/electrical board policy, but it
does not train QPI and has no value in the pass-3 observation build.  The next
safe investigation is a read-only capture/diff of those I/O banks across
vendor Slow/High boots, followed by a named clean-room implementation if a
required effect is demonstrated.  Calling the raw helper would hide its
board-policy dependency and mutate unexplained registers.

### `fffc13b5`: reject direct call

The exact function is `fffc13b5..fffc1466`, 177 bytes, SHA-256
`e64162454f4451943f7c30a1e12b28683ef90669bebdabf0fd57c3f3e66f49d6`.
It expects `ESI=0x00100000`, reads numerous private handoff fields, and writes
beyond `+0x2bcc`.  At `fffc1422` it pops the CALL return address into ESI and
uses the following 36 instruction bytes as encoded source data for writes to
IOH device `00:14.1` offsets `0xd0/0xd4/0xd8/0xb8/0xbc/0xc0`.  Its final RET
therefore consumes an older saved continuation rather than returning like a
normal cdecl function.

A direct coreboot call would interpret coreboot code bytes as policy and could
corrupt both scratch state and control flow.  It must not be tried as a blob.
Reverse-engineer its output format or reimplement the required handoff after
DRAM is established.

## Cross-vendor candidates

### Intel GUID `63C0690C...`

Intel GUID `63C0690C-5D9E-4EE3-840E-FAE0E76E291A` is a PEI module in desktop
X58 and server 5500/5520 firmware.  Server UI/PDB metadata verifies the name
`UnCoreInitPlatform`.  Representative exact bodies are:

| Firmware/local source | Body offset/path | Size | Preferred base + entry RVA | Raw SHA-256 | Relocation-normalized SHA-256 |
|---|---|---:|---|---|---|
| DX58SO2/OG 0920, 9FC copy | `blobs-local/intel-dx58so/extracted/Intel32/Intel32c/9FC23D82/63C0690C.efi` | `0x4c2c0` | `ffedd33c + 3540 = ffee087c` | `aca71404ccb5eb9df65997ee887914db3231503f0cfb0c0b3b5ca2b06135e4ae` | `80c78bfafc720154f57df830dff4257c3c92dc52e7b4d865371f9a3fb26d3ffe` |
| DX58SO2/OG 0920, 9A copy | `blobs-local/intel-dx58so/extracted/Intel32/Intel32c/9A4245CD/63C0690C.efi` | `0x4c280` | `ffe1933c + 3540 = ffe1c87c` | `2cb7f2ea1168afac4cf1c2d49079dfba98bdc9d9bf36d04aab2efe7e36a1d078` | same |
| DX58SO 5600 | `SO5600P.bio` body `0x6eab8` | `0x4a3a0` | `ffe196d0 + 32a0 = ffe1c970` | `57ea3155d67a0c79eef310c2c48e34b6216fc8b7485fe57fd1cb23042a03942a` | `f43339c7d0a936868eb0ea4a51366ce10f31ca2759e8e2dd3c162475b76bb4b4` |
| S55xx R0033 | `R0033.cap` body `0x201ac4` | `0x28f20` | `fffa0134 + 260 = fffa0394` | `fccbf6bd9fd65140c91478d268d206b483963f0d2cbf7a7fad9ea82643453cdd` | `d50af7f84b41a8d83a9d8f4c1a665bc821cf5bf23515749d00dd3d505319b1d0` |
| S55xx R0069 | `R0069.cap` body `0x201ccc` | `0x2b9e0` | `fffa0134 + 260 = fffa0394` | `91bef163e0bac948f40e594fd99672797cbb575c0a1d73204b00f3167f96bd5a` | `8f85783d4b27d00364ed1984184901333a19e814383764f0b63542d9b2fd6a24` |

The late desktop dependency expression requires base-memory-test,
platform-memory-size, read-only-variable, CPU-I/O, SMBus, and private
`159e7d1b-51cc-4cad-81ea-6b7d2ed39dc0` PPIs.  Its entry consumes PEI services
and a large private context.  Preferred addresses are not callable fixed XIP
contracts.  Rehosting it would require a substantial PEI compatibility layer
and still supply Intel-board rather than MSI policy.  It is therefore a
function-level reference, not a B06V8 blob.

### Apple GUID `D71C8BA4...`

Apple uses GUID `D71C8BA4-4AF2-4D0D-B1BA-F2409F0C20D3`; the firmware contains
no UI name, while the pinned UEFITool database externally labels it
`UncoreInitPeim`.

| Firmware/local source | Body offset | Type/size | Preferred entry expression | Raw SHA-256 | Canonical SHA-256 |
|---|---:|---:|---|---|---|
| MacPro4,1 `MP41_0081_07B_LOCKED.fd` | `0x1118c` | PE32 / `0x23060` | `ffc1118c + 260 = ffc113ec` | `841791c4fb65d14ab06798ac1e01151e27730c0f63b3014a16460c6de6d3cb28` | `2ed37447a73d531b0594fdbeba309e3acad1e5c6a09f0a8d959e262ffb55cb87` |
| MacPro5,1 `MP51_007F_03B_LOCKED.fd` | `0x0da6c` | TE / `0x26e10` | `ffc0da6c + 260 - (1b8-28) = ffc0db3c` | `89904e830275af330e8679f52a684ace11f9fc784f135f3f3ea1dd4c53c144c4` | `eeee1a762fa1596c5b97a2858bad0c6dd6b37d9373d934e858f136474bc6596c` |

These modules share 890 exact executable bytes with MSI MINIT, proving useful
common-code ancestry.  They still have private PEI policy and no reconstructed
coreboot ABI.  Their different boards and PE32-to-TE evolution make them poor
direct-call candidates and good semantic diff partners.

### ASUS Rampage III Formula

All four local 2 MiB AMIBIOS8 ROMs contain PE32 `MINITDLL` at raw offset
`0x1c1dc0`, preferred base `0xfffc1dc0`, entry RVA `0x220`
(`0xfffc1fe0`), and `CSI_INITDLL` at raw offset `0x1e6de0`, preferred base
`0xfffe6de0`, entry `0xfffe7000`.

| Release | MINIT size / raw SHA-256 | CSI size / raw SHA-256 |
|---|---|---|
| 0402 | `0x1e480` / `82eb20df6d8d09e7ff9370e4808f7c14a5f51d1776460f873c1ef63b53381092` | `0x7b60` / `b0f97e3f8ae49a01bc1d5f91fd0afe2e2dba97965d4d55c6bf94641bacc8e648` |
| 0701 | `0x1e460` / `626302ced5ff741ee83d1d9486446a8b4d8863952b3d1febe435aa0bb89b31a5` | same |
| 0702 | `0x1e4c0` / `9ca9a753c8f19a3e77afe20a633befa31b6f119142400dd7c6edf74be66cbf45` | same |
| 0903 | same as 0702 | same |

ASUS CSI is byte-identical across all four releases, while the target MSI CSI
is a different size and hash.  The 0701-to-0702 “Memory Recheck” change at
preferred `0xfffce122` is inside ASUS MINIT.  ASUS 0903 changes compatibility
outside both unchanged init DLLs, pointing to caller policy/tables.  This makes
the ASUS family an excellent policy-diff oracle, but weaker than the target
MSI CSI already proven on this board.  Substituting the ASUS DLL or caller in
B06V8 has no supporting evidence.

## Final candidate ranking

1. **Try now:** target MSI branch-patched wrapper plus unchanged target MSI CSI,
   exactly once per pass under the persistent three-pass guard.
2. **Try only after pass-3 evidence:** unchanged target MSI MINIT with a
   separately reviewed High-QPI policy/workspace experiment; still stop before
   ramstage until memory is tested.
3. **Observe/reimplement later:** MSI `fffc17f6` board-I/O effects, after
   read-only vendor Slow/High comparison proves what is required.
4. **Never direct-call:** MSI `fffc13b5` without reproducing its inline-data
   continuation and full handoff layout.
5. **Reference only:** Intel `63C0690C...`, Apple `D71C8BA4...`, and ASUS
   DLLs until their PEI/caller ABIs and board policies are independently
   reconstructed.

This ranking preserves the diagnostic path and tests one hardware hypothesis:
whether the target MSI CSI's terminal High-QPI pass completes after the single
correct IOH CPU-only reset.  It does not broaden the experiment into an
unreviewable mixture of vendor modules.
