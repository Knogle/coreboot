> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06V7 robust Slow-QPI automatic handoff

B06V7 is a separate, default-off successor to B06V6. It keeps the same exact
E5645, microcode, one-DIMM, ratio-6, cold-policy, Slow-QPI, reset-guard,
Uncore, MTRR, DRAM-window, postcar and ramstage gates. Its only behavioral
change is that two proven run-variable regions are canonicalized for hashing.
The raw vendor buffers are never modified.

This remains local vendor-assisted research, not native X58 memory
initialization. The redistributable source and base ROM contain no MSI module
bytes. The local composite and full-chip image do and must not be distributed.
B06V7 has been build-tested but has not run on hardware.

## Hardware evidence and hypothesis

Two otherwise accepted Slow-QPI CSI states have these fingerprints:

| Raw FNV-1a-32 | Byte `0x2a6` | Canonical FNV-1a-32 |
|---:|---:|---:|
| `a3736e20` | `08` | `8403ac98` |
| `c4eab5a4` | `0c` | `8403ac98` |

They are byte-identical after CSI byte `0x2a6` is replaced with zero only in
the hash stream. B06V7 therefore requires the byte itself to be exactly `08`
or `0c` and the canonical digest to be exactly `8403ac98`. The logical CSI
return tuple remains exactly `EAX/EBX/ECX=2/2/0x106`.

The separate High-QPI experiment is intentionally outside this build. Its
captured state has raw FNV `f68d1f86`, byte `0x2a6=08`, but canonical FNV
`cbddcf0e`; it fails the B06V7 Slow-QPI gate.

Two full-path Slow-QPI MINIT workspaces have raw FNVs `6d87f4e4` and
`1f354cea`. Zeroing only the following inclusive byte ranges in the hash
stream produces canonical FNV `7a34f363` for both:

```text
132c-136d  13be-13fd  1859-185c  18e7-18e9  1aa4-1aa7
23a6       2411-2434  24a1-24c4  251f-252e
```

These offsets have neutral meaning: current evidence establishes variation,
not register or data-structure semantics. Any byte outside the ranges still
changes the canonical digest and fails closed.

Direct comparison found one CSI difference, exactly at `0x2a6`. The two
workspaces differ at 50 byte positions, and every difference lies inside the
listed ranges; no outside byte differs.

## Unchanged gates

B06V7 still requires:

- CPUID `000206c2` and microcode revision `0000001f`;
- the sole SPD responder at `0x54` and complete SPD FNV `fb66b530`;
- the same two-pass CSI/I801/CMOS provenance and reset-loop guards;
- CSI seed zero and logical return tuple `2/2/106`;
- cold-policy FNV `3c0f3a0b`, status zero and flags `01020480`;
- MINIT status success, `EAX=0`, runtime-result self-consistency, balanced
  vendor stack, intact CAR canaries and completion workspace markers;
- mapper `00024489`, common `F8=00001545`, channel-2 DOD `000002ac`, ranks
  `00000003`, status `00000140`, and Slow-QPI status `030f0f03`;
- the B06V6 exact UC MTRR state and both complete 8-MiB DRAM-window tests.

Absolute MINIT `EBX`, `EDI` or `EFLAGS` values are not success gates. They are
caller/build state rather than a proven vendor result ABI. Runtime call-result
self-consistency and the exact returned vendor stack pointer remain gated.

## Telemetry and handoff

The second CSI pass prints raw FNV, raw byte `0x2a6`, and canonical FNV before
acceptance. After MINIT it prints raw and canonical CSI/workspace digests
again. A successful suffix includes:

```text
[RAMINIT] CSI RAW_FNV=........ BYTE2A6=.. CANON_FNV=8403ac98
[RAMINIT] FINAL CSI_RAW=........ CSI_BYTE2A6=.. CSI_CANON=8403ac98 \
          WORK_RAW=........ WORK_CANON=7a34f363
[RAMINIT] AUTO_HANDOFF=READY; robust Slow-QPI canonical state; returning to coreboot
```

The CBMEM handoff remains magic `X6HO` but uses version 2 and a 140-byte
structure. It carries both raw and canonical CSI/workspace digests. B06V6
continues to compile its original version-1, 132-byte handoff and exact raw
digest gates when B06V7 is disabled.

## POST sequence and recovery

The POST map is unchanged from B06V6. First pass is expected to end with
`02,03,04,d1` followed by the CSI-produced reset. A successful second pass is:

```text
02,03,05,d1,d2,06,07,d3,d4,08,09,0a,
[full 16-MiB UC window test],0b,0c,0d,30,31,32,0e,0f
```

Any canonical mismatch emits fallback POST `20` and enters the CAR recovery
ROMMON without another automatic vendor call. Guard ambiguity stops at `1f`.
For `1f`, issue `unlock RESET`, then `autoguard clear`, require the reported
CMOS transition `0xec->0x6c`, and remove AC power completely. A CF9 reset is
not an AC cycle. Any hang requires AC removal and the known-good socketed
recovery chip.

First hardware testing must use the exact E5645, one BLS4G3D1609DS1S00.
dual-rank DIMM as sole responder at SPD `0x54`, DDR ratio 6, Slow QPI, COM1
115200 8N1, and a fully externally verified spare W25Q128. Capture the entire
serial stream and POST trace; one success does not satisfy the ten-cold-boot
validation rule.

## Offline verifier

The vendor-free verifier operates on user-supplied binary snapshots:

```bash
python3 scripts/x58_b06v7_canonical.py \
  --csi local-csi-state.bin --workspace local-minit-workspace.bin \
  --require-gate
```

It reports SHA-256, raw FNV, canonical FNV, the CSI dynamic byte and the exact
gate result. Its unit tests are in `tests/test_x58_b06v7_canonical.py`.
