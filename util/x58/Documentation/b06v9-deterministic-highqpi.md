> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06V9 deterministic High-QPI CSI three-pass probe

Status: **two clean fixed-epoch builds are byte-identical; source, patch-tool,
local-composition, wrapper and W25Q128 placement checks pass; one controlled-G3
hardware sequence completed pass 1 and pass 2, reached and retained the exact
High-QPI endpoint, then stopped fail-closed before the pass-3 CSI call**.

B06V9 supersedes the retired B06V8 experiment. It remains a default-off,
terminal research branch for the exact E5645, one-DIMM, DDR-ratio-6 lab
configuration. It can invoke CSI in the existing guarded three-pass sequence,
but it never invokes MINIT and never enters postcar, ramstage or a payload.

## Corrected ICH10 prerequisite

The [Intel I/O Controller Hub 10 Family Datasheet](https://www.intel.com.br/content/dam/doc/datasheet/io-controller-hub-10-family-datasheet.pdf)
defines RCBA `+0x3400[2]` as Upper-128 enable for RTC RAM. Its reset value is
zero; with the bit clear, ports `0x72/0x73` alias the standard `0x70/0x71`
bank. The X58 port currently links only the narrow i82801jx early-BAR helper,
so the analogous write in the full upstream i82801jx bootblock did not run.

B06V9 performs one explicit, bounded prerequisite step after RCBA decode has
been established and read back:

1. emit POST `10` and read RCBA `+0x3400`;
2. accept only complete value `00000000` or `00000004`;
3. set only bit 2;
4. read back and require complete value `00000004`;
5. emit POST `11` and report `RTC_RC PRE=... POST=00000004 U128E=PASS`.

An unexpected value or failed readback is not overwritten. The automatic
path falls back at `B06V9_RTC_UPPER_BANK_GATE` before any vendor call. No RTC
data byte is read or written by this prerequisite.

The true upper-bank values measured on the target and vendor-booted reference
were different and are telemetry only:

```text
target:    80/81/82/88/89 = a0/84/c0/a0/06; 8e/ca/f1/f5 = 44/0c/26/bc
reference: 80/81/82/88/89 = 43/54/37/02/00; 8e/ca/f1/f5 = 30/00/48/c1
```

B06V9 does not copy, compare, or derive CSI policy from these bytes.

## Deterministic local wrapper profile

The repository contains no MSI wrapper or patched proprietary code. The tool
[`scripts/x58_b06v9_wrapper_patch.py`](../scripts/x58_b06v9_wrapper_patch.py)
operates on a locally composed, hash-pinned image and changes exactly six
regions/seven bytes:

| Runtime address | 4-MiB offset | Change | Result |
|---|---|---|---|
| `fffc0516` | `003c0516` | `00 -> 01` | initialize `state[07]=1` |
| `fffc0578` | `003c0578` | `01 -> 00` | pair `1c/1d=0/1` |
| `fffc066b` | `003c066b` | `01 -> 00` | pair `70/71=0/1` |
| `fffc083f` | `003c083f` | `01 -> 00` | pair `cd/ce=0/1` |
| `fffc0a0c` | `003c0a0c` | `01 -> 00` | pair `121/122=0/1` |
| `fffc0bc2..0bc3` | `003c0bc2..0bc3` | `74 09 -> 90 90` | bypass dynamic RTC parser |

The unmodified E5645 wrapper path already derives `state[06]=1`. Together the
profile produces the exact logical state observed on the successful live
High-QPI transition:

```text
state[06/07] = 1/1
state[1c/1d] = 0/1
state[70/71] = 0/1
state[cd/ce] = 0/1
state[121/122] = 0/1
```

Complete wrapper pins are:

```text
range:                   003c04e2..003c0d18 (0x837 bytes)
original SHA-256:        546f0c4d455ce250b3fc6047d3864b25d6fd16e7cf1c3bfaa6b61d69510af6c3
original FNV-1a32:       65b20e50
B06V9 SHA-256:           d38c093271f18cfa5f399421654fcab2e61fa07efca85323904dcb74b89e0fa9
B06V9 FNV-1a32:          98e2f3de
```

The runtime checks both the complete FNV and all six local instruction/data
signatures. A B06V8-patched, partially patched or otherwise mixed wrapper is
rejected.

## Retained three-pass safety model

B06V9 deliberately keeps the B06V8 automaton rather than changing reset logic
and wrapper policy in one experiment:

- cold authorization is standard CMOS `0x0e=0x2c`;
- in-progress/fail-lock values are `0xec/0xed`;
- exact I801 signatures identify pass 2, consumed and pass 3;
- each phase is consumed before a fallible operation or vendor call;
- pass 1 expects CSI's internal CPU-only reset;
- pass 2 requires the exact logical CSI return and endpoint tuple;
- the outer reset uses the exact MSI IOH SYRE edge, never CF9;
- pass 3 is observational and always terminates in CAR ROMMON;
- every manual vendor mutation and MINIT command remains locked.

The complete state predicates and recovery procedure remain documented in the
[retired B06V8 state-machine document](b06v8-highqpi-three-pass.md). Its RTC
input section is historical and must not be used; its three-pass/I801/SYRE
description remains the B06V9 control-flow contract.

## Expected POST and serial progress

Every reset-vector entry retains the earlier bootblock/ICH10/SPD markers and
adds:

```text
10  RTC configuration precheck begins
11  exact RC=00000004 readback accepted
12  deterministic wrapper profile and platform inputs accepted
20  fail-closed automatic fallback; CAR ROMMON follows
```

The intended three segments are:

```text
pass 1:  ... c5,10,11,c6,d7,02,12,03,04,d1 -> CSI internal reset
pass 2:  ... c5,10,11,c6,d7,02,12,03,05,d1,d2,ca,cb,fe -> IOH SYRE reset
pass 3:  ... c5,10,11,c6,d7,02,12,03,cf,d1
return:  d9, complete CSI/QPI telemetry, da,20,bc
```

Absence of `11` means the new ICH10 gate failed. A stop at `d1` or `cf` does
not prove success: CSI may have reset, halted or hung. High QPI is accepted
only from the existing exact CPU/IOH endpoint predicates printed after reset.

## 2026-09-04 hardware result

The first test used one controlled 15-second Shelly AC removal. Once AC was
restored the board remained in soft-off at about 0.9 W, so the operator had to
press the physical power button. The Shelly path therefore supplied a real G3
transition but did not autonomously restart this board.

The complete local-only 17,776-byte serial capture is:

```text
443eb6cbe3808e2ac63481c7057c1bef369e06dea544e78c10109c49a1b549de  blobs-local/msi-x58-pro-e/b06v9/2026-09-04-b06v9-ac-cold-three-pass-highqpi.raw
```

It records four reset-vector/romstage segments within that one AC-cold
sequence:

1. A first `COLD_DEFAULT` segment enabled U128E, passed the complete SPD gate,
   and stopped printing after `PCIEXBAR PRE ... LO=e0000001`. A fresh
   cold-like reset-vector entry followed before any CSI call. Its cause is
   unknown; the last emitted line is not proof that PCIEXBAR caused it.
2. The repeated `COLD_DEFAULT` segment passed the same gates and invoked CSI
   pass 1. The expected CSI-produced reset followed, leaving exact I801 phase
   signature `08:31:ce:68:97`.
3. Pass 2 returned `EAX/EBX/ECX=1/0/2a6`, CSI FNV `92e228e7`, exact
   `state[06/07]=1/1`, byte `2a6=0c`, and `00/01` in all four paired fields.
   The complete pass-2 CPU/IOH/link predicate accepted and the one-shot outer
   IOH SYRE edge produced the intended second reset.
4. Pass 3 entered with exact signature `08:53:ac:6b:94`, but
   `B06V8_HIGH_CSI_PROFILE` returned `platform-state CODE=0d`. The automatic
   path consumed the phase and fell back through POST `20` to terminal `bc`
   before reading SPD or invoking CSI a third time.

The retained terminal endpoint was:

```text
CPU ff:02.1 50/54/6c/80 = 160c0112/00000012/0040a0a0/070f0f03
CPU ff:02.1 94/9c/a0/a4 = 00010202/00000502/00017000/00322808
IOH 00:0d.0 82c/840/854/85c/864 = 004060a0/070f0f03/00010102/00000002/00322808
LINK CPU50/58 = 86000000/00064555; IOH_C8 = 0606fc00; SYRE_CC = 00000600
```

Sixteen consecutive groups of ROMMON reads kept CPU `A0=00017000`, CPU
`50=160c0112`, CPU `80=070f0f03`, and IOH `840=070f0f03` unchanged. The
vendor-booted High-QPI reference independently has CPU `A0=00017000` as well.
Source inspection showed that every other pass-3 platform field matched; the
sole rejection was the exact `A0=00017600` comparison in each of the two
duplicated High-QPI predicates. Thus the stop is a fail-closed false negative,
not evidence that the preceding High-QPI transition failed. The two observed
values are kept semantically neutral; no undocumented field meaning is
claimed.

This run proves one execution of both intended CSI/reset transitions and one
stable occurrence of the project's exact High-QPI endpoint. It does not meet
the ten-cold-boot rule. It did not execute pass-3 CSI, MINIT, ordinary DRAM,
postcar, ramstage, PCI enumeration, GPU initialization or a payload. At the
terminal monitor CMOS `0x0e` remained `ec` and the consumed I801 signature was
`08:42:bd:7a:85`; the guard must be cleared explicitly before another cold
sequence.

## Exact first-test configuration

```text
board: MSI X58 Pro-E / MS-7522 revision 3.0
CPU: Intel Xeon E5645, CPUID 000206c2, microcode 0000001f
DIMM: BLS4G3D1609DS1S00., sole SPD 0x54, FNV-1a fb66b530
memory ratio: 6
initial QPI: Slow
UART: COM1 03f8, 115200 8N1, no flow control
flash: socketed W25Q128.V..M, 16 MiB; 4 MiB image top-aligned
start: true AC-cold after exact guard preparation
```

The completed first B06V9 run retained this exact configuration. It was a
QPI/CSI experiment, not a graphics or first-payload boot attempt.

## Artifacts

Redistributable coreboot-only base:

```text
d954b0b14fcd1716594ca5aa49a1fff809d150f81b76a6c8e2e3fbcb753f97d8  msi-x58-pro-e-b06v9-coreboot-base-4MiB.rom
```

Local-only files containing MSI code; do not redistribute:

```text
f93e516eb5bc1c63c32ec87ec8364bd765a83fea07592fa009c39b2bb3127d13  msi-x58-pro-e-b06v9-unpatched-4MiB.rom
0b40713543a28ac0e85600394b17cbc2984d3b6ebcbd03c31b315e57b9c09730  msi-x58-pro-e-b06v9-deterministic-4MiB.rom
0eeebf94b91b2fe060329974a69878385d2a8cb0b01721beaa73fd4b510af032  msi-x58-pro-e-b06v9-deterministic-w25q128-16MiB.rom
```

The flashable lab artifact is the final 16-MiB file. Its lower 12 MiB are all
`ff` with SHA-256 `6747318cfda6f6bb9e77ee1c229d37b1799610bc1d41e5165cc40ccf2363a7c4`;
its upper 4 MiB exactly equal the deterministic composite. See the
[B06V9 build manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index)
for reproduction and validation details.
