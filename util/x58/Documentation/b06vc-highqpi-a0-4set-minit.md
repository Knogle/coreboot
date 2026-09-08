> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VC exact-four-value High-QPI MINIT observation

Status: **FOUR CONTROLLED G3 RUNS / PASS 1 AND PASS 2 COMPLETE 4/4 /
PASS-3 CSI ABI RETURN 2/4 / STRICT FALLBACK BEFORE MINIT 4/4**.

B06VC is the narrow successor to the four real B06VB controlled-G3 runs. Those
runs reached the pass-3 preselector with CPU PCI `ff:02.1 + 0xa0` equal to
`00017a00`, `00017800`, `00017a00`, and `00017a00`; earlier observations had
produced `00017000` and `00017600`. Every other B06VB preselector field matched.
That evidence supported B06VC's test of the exact observed set
`{00017000,00017600,00017800,00017a00}`. It does not establish a field meaning,
range, mask, or DDR-ratio encoding for A0.

## Deliberately narrow change

B06VC changes only the read-only A0 decision immediately before pass 3. It
accepts the four exact values above. It does not mask A0, write A0, admit a
range, or alter the pass-1/pass-2 sequence. The post-CSI gate still requires
exactly `00017000`, and every other B06VB platform, result, digest, policy,
one-shot, and terminal guard remains unchanged.

Thus the experiment can progress only when the complete known platform state
matches. Any other pre-pass-3 A0 value fails closed before another vendor call;
any mismatch after a returned CSI call fails closed before MINIT or at the
retained terminal guard.

## Identity and reproducibility

```text
image ID: X58PROE-B06VC-HIGHQPI-A0-4SET-MINIT-ROMMON-20260904
config: configs/x58-pro-e-b06vc.config
config SHA-256: 6079d3d9981b12eb0860ff61523a87799cf8f01aeb5772aa416ab8832c7d5ada
fixed SOURCE_DATE_EPOCH: 1788523200
repository test suite: 87/87 passed
support-tool test suites: 28/28 passed
independent audit: no blocker reported
reproducibility: two fixed-epoch builds were byte-identical
```

The redistributable 4-MiB coreboot-only base has SHA-256
`398c631b6bf68b2bfb9c344c05b3adc477c6ea67e58093efadc2e5ec1c241b1d`.
The local compositions and their proprietary provenance are recorded only in
the [B06VC manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## 2026-09-05 controlled-G3 hardware results

The exact B06VC identity was observed from the flashed 16-MiB image whose
expected SHA-256 is
`6c1ac5710a8689d43609cfbf7e631dbd64b4f981bfd1b41bed247cfb1ce9c5af`.
There is no programmer read-back. Before the first controlled attempt, two
captured starts entered the intended persistent-loop fallback with CMOS
`0e=ec`. In ROMMON, `unlock RESET` followed by `autoguard clear` changed the
guard to `2c`; each recorded attempt then used a five-second G3 removal
through the Shelly at `192.0.2.215`.

All four G3 sequences completed the exact pass-1 path and its CSI-internal
reset. Pass 2 returned and was accepted 4/4 with:

```text
EAX/EBX/ECX = 00000001/00000000/000002a6
EDX/EDI     = fff8fcd8/fff8fcd8
EFLAGS/VESP = 00000086/fff8ffe4
CSI FNV     = 856a313b
state 06/07/2a6/2ef/301 = 01/01/08/01/00
```

The expected outer SYRE reset also completed 4/4. The pass-3 branches were:

| Run | Pre-CSI A0 | Preselector | Pass-3 CSI | Post-CSI result | Terminal boundary |
|---|---:|---|---|---|---|
| 1 | `00017c00` | fail, `CODE=0d` | not called | n/a | safe CAR fallback |
| 2 | `00017a00` | pass | ABI-clean return | `0/0/11`, FNV `03e3d24e` | strict post-observation gate |
| 3 | `00017c00` | fail, `CODE=0d` | not called | n/a | safe CAR fallback |
| 4 | `00017000` | pass | ABI-clean return | `0/0/11`, FNV `03e3d24e` | strict CSI-state gate |

Runs 1 and 3 therefore stopped before pass-3 CSI. Five direct read-only
confirmations after run 1 retained `00017c00`. Runs 2 and 4 returned from the
third CSI call with `EDX=EDI=fff8fcd8`, `EFLAGS=00000082`,
`VESP=fff8ffe4`, intact canaries, and self-consistent result tuples. ROMMON
reported `CSI_ATTEMPTED=01`, `CSI_RETURNED=01`, `CSI_ACCEPTED=00`, and
`MINIT_ATTEMPTED=00` in both returned-call runs; the post-call probe reported
platform-state `CODE=0d`.

Every other complete preselector input matched in all four runs:

```text
CPU ff:02.1 50/54/6c/80 = 160c0112/00000012/0040a0a0/070f0f03
CPU ff:02.1 94/9c/a4    = 00010202/00000502/00322808
CPU ff:02.0 50/58       = 86000000/00064555
MC  ff:03.4 50/54       = 0a000006/00000006
IOH 82c/840/854/85c/864 = 004060a0/070f0f03/00010102/00000002/00322808
IOH 00:10.0 c8          = 0606fc00
IOH SR0/SR1/SYRE        = 00000000/00000000/00000600
```

After pass-3 CSI, run 2 had CPU `6c=0040a0a8`, `9c=00a00502`, and
`A0=00017a00`; IOH matched apart from the expected returned `c8=0616fc00`.
The strict B06VB observation predicate rejected this state before MINIT.

Run 4 is the stronger discriminator. Its complete post-CSI platform tuple
matched, including CPU `6c=0040a0a8`, `9c=00b00502`, `A0=00017000`, IOH
`c8=0616fc00`, stage value `ea000000`, and exact MC fields. Its 772-byte CSI
state was nevertheless SHA-256
`0a10266b0e65c8888b001b386ce73460075e6cc810ed58cd00338a29ce13f199`, FNV
`03e3d24e`, with byte `0x2a6=08`, rather than the saved B06VA state's SHA-256
`ed71cba07f00d61cef6cf4c132877bb323b84f32c15622d737998ea705e4d8a7`, FNV
`8b38506a`, with byte `0x2a6=0c`. The run-2 and run-4 states are
byte-identical; comparison with the saved B06VA state found exactly that one
byte different. Canonicalizing byte `0x2a6` to zero yields FNV `908dabb6` for
all three states.

No POST `d3` or `d4` occurred. MINIT, ordinary DRAM access, postcar, ramstage,
graphics, and payload execution were never reached. These runs prove the
pass-1/pass-2/reset sequence 4/4, and an ABI-clean, non-no-op pass-3 CSI return
2/4. They do not prove accepted High-QPI completion, memory training, or the
required ten-run repetition milestone.

The immutable local raw captures are:

```text
320fb21908519be33b0817f345900ab4f59bed35e3591b2e47701989e0c22df7  2026-09-05-b06vc-g3-run1-a0-17c00.raw  (18087 bytes)
4be4b9e9c7151ff77cad9d165d68dd96849ed84937b9349192770d566cfc93f3  2026-09-05-b06vc-g3-run2-csi3-03e3d24e.raw  (17554 bytes)
ba18692b7d3eafa8a0c91329662fb8a83c3f738a3daebf468957208af5ea2cfb  2026-09-05-b06vc-g3-run3-a0-17c00.raw  (10049 bytes)
bfd2051fca64b5f4d58e5427ef003cbc174bfab6096f9556c78baa7c4d241938  2026-09-05-b06vc-g3-run4-csi3-03e3d24e-postexact.raw  (17554 bytes)
```

The run-1 capture also includes the two initial guarded starts and interactive
read-only confirmations. Across the earlier captures and these runs, the
exact observed pass-3 preselector A0 set is now
`{00017000,00017600,00017800,00017a00,00017c00}`. This supersedes the
four-value evidence used to construct B06VC, but does not establish A0's field
width, meaning, or a valid mask/range. Treating it as reset- or
training-variable remains an inference.

The narrowest evidence-derived successor hypothesis is to add exactly the
fifth preselector value `00017c00` and accept only CSI-state byte `0x2a6` in
the observed set `{08,0c}` under canonical FNV `908dabb6`, while retaining all
post-CSI platform gates exactly. This is a proposed experiment, not behavior
implemented by B06VC and not a claim about either field's semantics.

## Expected observable branches

Any successor hardware run is observational and may take any of these
branches:

- pass-3 CSI may reset, hang, or return;
- POST `d3` is reachable only if CSI returns and the exact post-CSI gate,
  result/digest checks, authorization, and policy construction all succeed;
- POST `d4` is reachable only if MINIT returns;
- a returned call prints the existing CSI/workspace telemetry and terminates
  in CAR.

Regardless of outcome, B06VC has no DRAM handoff, postcar, ramstage, graphics,
or payload path. Reaching `d3` would prove only entry into MINIT, while reaching
`d4` would prove only that MINIT returned; neither alone proves trained DRAM.

## Failure and recovery boundary

Unexpected values are rejected before MINIT or by an existing terminal guard.
The target uses a socketed W25Q128.V..M recovery flash. Preserve the verified
vendor image and restore it externally if the board no longer reaches the
diagnostic path. A single run is not milestone validation; preserve the full
serial stream, POST trace, exact boot/reset provenance, flashed-image hash, and
programmer read-back where available.
