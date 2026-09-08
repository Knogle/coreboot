> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06VA exact High-QPI CPU-A0 allowlist probe

Status: **two clean fixed-epoch builds are byte-identical, the source
contract passes, and one live B06VA sequence reached and returned from the
third CSI call. The exact initial power/reset type and early serial trace of
that run were not captured, so the required AC-cold repetition matrix remains
open**.

B06VA is a deliberately narrow successor to B06V9. It changes one predicate
which prevented the third CSI observation call after B06V9 had already
completed both reset transitions and reached a stable High-QPI endpoint. It
remains a default-off, terminal CAR experiment for the exact E5645,
one-DIMM, DDR-ratio-6 lab configuration.

## Evidence and hypothesis

The controlled B06V9 sequence completed CSI pass 1, returned successfully from
pass 2, accepted the complete pass-2 CPU/IOH/link tuple, and completed the
outer IOH SYRE reset. On pass 3 every checked High-QPI field matched except
CPU PCI function `ff:02.1`, register `0xa0`:

```text
B06V9 retained pass-3 endpoint:        00017000
vendor-booted ratio-6 High-QPI host:   00017000
earlier accepted target observation:  00017600
```

The field meaning is not established. These values are therefore documented
only as exact observations; B06VA does not name unknown bits or infer a field
encoding.

The sole experiment is to accept this exact set:

```text
{ 00017000, 00017600 }
```

Values such as `00017200` and `00017400`, and every other value, remain
rejected. There is no masked comparison, range comparison, or write to
register `0xa0`.

## Scope of the change

The shared exact-set helper is used at both duplicate High-QPI decision
points:

- the vendor-call preselector's complete platform predicate;
- the romstage pass-3 complete CPU/IOH/link tuple predicate.

When the B06VA option is disabled, the B06V9 code still requires exactly
`00017600`. A clean B06V9 regression build remains byte-for-byte identical to
the published B06V9 base ROM:

```text
d954b0b14fcd1716594ca5aa49a1fff809d150f81b76a6c8e2e3fbcb753f97d8
```

Nothing else in the three-pass mechanism changes. B06VA retains:

- the B06V9 upper-RTC-bank prerequisite;
- the deterministic, hash-pinned B06V9 wrapper profile;
- the CMOS and I801 one-shot phase guards;
- CSI pass 1's internal reset expectation;
- the exact pass-2 return and endpoint checks;
- the exact outer IOH SYRE reset edge;
- the full SPD, PCIEXBAR, X58 and wrapper gates;
- terminal CAR ROMMON recovery after pass 3.

The preselector remains mandatory. B06VA does not bypass or replace it.

## New telemetry and POST code

On pass 3 B06VA reads CPU register `ff:02.1 + 0xa0` once for explicit
preselector telemetry:

```text
[QPI] PASS3_PRESELECT CPU_A0=00017000 EXACT_SET=00017000|00017600 MATCH=PASS
[VENDOR] B06VA_HIGH_CSI_PROFILE STATUS=ok CODE=00
```

The first line is diagnostic only. The vendor preselector independently
checks the complete platform tuple and returns a status. Only after that
complete selector succeeds does B06VA emit POST `13`. The normal SPD,
PCIEXBAR, wrapper, complete tuple, arm and call gates still follow; POST `13`
alone therefore does not prove that CSI pass 3 was called or returned.

A value outside the two-member set prints `MATCH=FAIL`; the preselector then
fails closed and control returns to the CAR ROMMON without a CSI call.

## Expected outcome and hard boundary

The intended first hardware result is that the previously measured
`CPU_A0=00017000` pass-3 state crosses the preselector and complete tuple
gates, invokes CSI exactly once, prints the complete return telemetry, and
terminates in CAR ROMMON. Pass 3 deliberately does not constrain or accept a
new CSI result as a memory-init handoff.

B06VA never invokes MINIT. It does not test ordinary DRAM, enter postcar or
ramstage, enumerate PCI devices, initialize a GPU, or start a payload. A
successful pass-3 CSI return would be a QPI/CSI research result only.

## First hardware result

The first live B06VA terminal session retained an ABI-clean CSI return and a
complete 772-byte state object. The automatic path had not accepted the
result and had not armed or called MINIT:

```text
CSI_ATTEMPTED/RETURNED/ACCEPTED = 01/01/00
MINIT_ARMED/ATTEMPTED/RETURNED  = 00/00/00
EAX/EBX/ECX                    = 00000000/00000000/00000011
EDX/EDI                        = fff8fcd8/fff8fcd8
VESP                           = fff8ffe4
CSI raw FNV-1a-32              = 8b38506a
canaries                       = valid
```

`CSI_RETURNED` is set only after the real wrapper returned, the dedicated
CAR stack balanced, both state pointers matched, and all CAR canaries passed.
The return tuple is an observation, not a decoded vendor success code.

The full state dump corrects an earlier visual transcription: byte `0x2ef`
is `00`; the nearby byte `0x2ed` is `01`. The compact state tuple is:

```text
state 06/07/2a6/2ef/301 = 01/01/0c/00/00
pairs 1c/1d 70/71 cd/ce 121/122 = 00/01 00/01 00/01 00/01
```

Relative to the reconstructed complete pass-2 state with FNV `92e228e7`,
pass 3 changed exactly eleven bytes:

```text
030 01->00  031 02->00  1db 00->01  206 10->12
275 00->01  285 0c->0e  2ef 01->00  2f8 a6->11
2f9 02->00  2fc 01->00  302 00->01
```

The wrapper returns `state[2fc]`, `state[301]`, and the dword at `state[2f8]`
as EAX, EBX, and ECX. The captured bytes therefore independently reproduce
the observed `0/0/11` result. Static inspection also shows that the terminal
class-3 path sets result bit `01`, and its positive two-endpoint aggregate
sets bit `10`; this explains `ECX=11` structurally without assigning it a
success meaning.

CSI made three visible, non-redundant QPI changes:

```text
CPU ff:02.1 6c: 0040a0a0 -> 0040a0a8
CPU ff:02.1 9c: 00000502 -> 00b00502
IOH 00:10.0 c8: 0606fc00 -> 0616fc00
```

The two CPU values are exactly the terminal ratio-6 High-QPI values on the
vendor-booted reference. More strongly, all 256 bytes of CPU PHY function
`ff:02.1` matched the reference snapshot byte for byte. The sampled second,
unused CPU PHY `ff:02.5` also matched. The IOH function is hidden after the
vendor POST, so its new `c8` bit 20 has no equivalent live reference and its
meaning remains unknown.

Both CPU and IOH physical-status values remained `070f0f03`. Ten consecutive
read-only sample groups retained the final CPU, IOH, link, and memory-clock
values without drift. The complete final tuple was:

```text
CPU 50/54/6c/80 = 160c0112/00000012/0040a0a8/070f0f03
CPU 94/9c/a0/a4 = 00010202/00b00502/00017000/00322808
CPU link 50/58  = 86000000/00064555
MC 50/54        = 0a000006/00000006
IOH 82c/840/854/85c/864 = 004060a0/070f0f03/00010102/00000002/00322808
IOH link c8     = 0616fc00
SR0/SR1/SYRE   = 00000000/00000000/00000600
```

The terminal CSI checkpoint was `00:14.1:9c=ea000000`. CPU link registers
`ff:02.0/02.4:80=0000fe91` contain the low part of the CSI local CAR-context
address (`state + 0x1b9 = fff8fe91`), exactly as the disassembly predicts;
they are not interpreted as QPI error status.

The retained one-shot state was CMOS `0e=ec` plus I801 signature
`08:42:bd:7a:85`. After all evidence was archived, `autoguard clear` changed
only the persistent authorization to `2c`; a true AC removal is still
required before another automatic attempt. A later interactive SPD read had
already replaced the volatile I801 residue, which is harmless because G3
resets the controller and any non-G3 restart fails closed.

Local-only evidence, not for redistribution:

```text
1f2352a383959c28f1cdeeb63a60388d8135d42aabba71da25b2ccb10c22566b  2026-09-04-b06va-pass3-terminal-state.raw
7c9bd9aa6f2f4e3bae1dfd90123da83185f4ba659f0fe1384b9248752b792501  2026-09-04-b06va-pass3-pci-scan.raw
09b74d13fc3ec1649dc46e8e1a87d5dcd75c829607a9a48f03ea8e2b1793a0c8  2026-09-04-b06va-autoguard-clear.raw
ed71cba07f00d61cef6cf4c132877bb323b84f32c15622d737998ea705e4d8a7  2026-09-04-b06va-pass3-csi-state.bin
```

This run proves a real, stable, non-no-op third CSI return and CPU-side
vendor-equivalent High-QPI PHY state. It does not prove a complete vendor
POST state, MINIT, usable DRAM, PCI enumeration, GPU output, ramstage, or a
payload. A repeat with serial capture armed before the first cold entry is
still required to make the `PHASE=03` marker chain formally complete and ten
AC-cold repetitions remain the milestone criterion.

The B06V9 hardware evidence and guard-recovery procedure are retained in
[the B06V9 document](b06v9-deterministic-highqpi.md). Hardware results must be
added only after a complete serial capture from a controlled run.

## Build identity and artifacts

```text
identity: X58PROE-B06VA-HIGHQPI-CPU-A0-ALLOWLIST-ROMMON-20260904
defconfig: configs/x58-pro-e-b06va.config
payload: none
terminal state: CAR ROMMON
UART: COM1 03f8, 115200 8N1, no flow control
```

Redistributable coreboot-only base:

```text
f23a19814c1c53115646c473c473b5c46c6f3b77d20fade73611911cf3bddce8  msi-x58-pro-e-b06va-coreboot-base-4MiB.rom
```

Local-only files containing MSI code; **do not commit or redistribute**:

```text
3ffda66334a54d41e1d32941fbb793490844727ae30887126683347ca79e998f  msi-x58-pro-e-b06va-unpatched-4MiB.rom
6214f4ef8c9a657059e4b46ed73253d69843bbd79ccdb98d8158463584af7248  msi-x58-pro-e-b06va-deterministic-4MiB.rom
fb7a425afe9e22c85691f4ffa71d88e37baa4b44f66e9b630615aa6d21512a77  msi-x58-pro-e-b06va-deterministic-w25q128-16MiB.rom
```

The flashable lab image is the final 16-MiB local file. Its lower 12 MiB are
erased `ff`, and its upper 4 MiB exactly equal the deterministic composite.
See the [B06VA build manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
for reproduction and validation details.
