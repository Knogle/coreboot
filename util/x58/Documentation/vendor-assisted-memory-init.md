> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# Experimental vendor-assisted X58 memory initialization

Status: **historical B06V0 interface document; the default-off research path
has since produced full MINIT returns and bounded UC DRAM tests in B06V5/B06VD.
B06VG is the current built coupled-state experiment; it has not yet run on
hardware**. Updated: 2026-09-05.

The sections below document the original B06V0 manual boundary and should not
be read as the current experimental milestone. See the
[B06VG experiment](b06vg-coupled-minit-seabios.md), the
[bring-up log](bringup-log.md), and the [test matrix](test-matrix.md) for the
later guarded automatic path and its exact claim limits.

This path tests whether locally extracted MSI initialization code can bridge
the current pre-DRAM research gap. It is not a native X58 implementation, is
not equivalent to a documented `mrc.bin` ABI, and is not a redistributable
firmware dependency. The intended release target remains native, reviewable
DDR3/QPI initialization.

## Implemented B06V0 boundary

The normal B06V0 boot path is deliberately unchanged through CAR, ICH10R,
SMBus and the interactive COM1 monitor:

```text
reset -> Intel microcode -> CAR -> ICH10R/SMBus -> ROMMON at POST bc
                                                     |
                                                     +-- no automatic blob call
```

All vendor actions require `unlock VENDOR`, which is consumed by exactly one
operation. CSI and MINIT are separate commands. A returning CSI result must
be inspected and explicitly echoed back before MINIT can be armed. B06V0
never copies the MINIT workspace to 1 MiB, never accesses ordinary DRAM,
never leaves CAR, and never starts ramstage or a payload.

The public 4 MiB base ROM contains only coreboot and the Intel microcode
already present in the coreboot source tree. A deterministic local tool
accepts the user's hash-pinned MSI ROM and inserts four proprietary ranges at
their original XIP addresses. Those bytes and the composed images remain
under ignored `blobs-local/` and must not be committed or redistributed.

## Exact local ranges

Source ROM: `A7522IMS.8F0`, 4 MiB, SHA-256
`ee7912da81a3cf47b6bedef7dedfa56cbc31723f5cdd7f0d75a7f64e336c04a8`.

| Range | Raw 4 MiB interval | Runtime address | Size | SHA-256 | Runtime FNV-1a-32 |
|---|---:|---:|---:|---|---:|
| CSI wrapper | `0x3c04e2..0x3c0d19` | `0xfffc04e2` | `0x837` | `546f0c4d455ce250b3fc6047d3864b25d6fd16e7cf1c3bfaa6b61d69510af6c3` | `65b20e50` |
| CSI CMOS helper | `0x3c1554..0x3c1579` | `0xfffc1554` | `0x25` | `6641f8cc9bfb9b08df0f39b688cbeeab93a0c92bff0d47ee565e525dd3179d75` | `99aeb979` |
| `MINITDLL` | `0x3c1dc0..0x3da4c0` | `0xfffc1dc0` | `0x18700` | `54fb7d14c1d88c0a42ea0e7b51511735983686b79fdf9db64e5ace2fc651264a` | `6465d8f5` |
| `CSI_INITDLL` | `0x3e6de0..0x3ee3c0` | `0xfffe6de0` | `0x75e0` | `e7c42f1a3474fc007c6d0731dc94f753f1fb0367d1ddbf8a3437edeb1daeb150` | `77990db9` |

The older local CSI file of size `0x75c0`, SHA-256 `b1d14415...`, was a
normalized/repacked image with a `0x20` raw/RVA mismatch. It is rejected.
For the authoritative source ROM, raw and loaded layouts are byte-identical:
section raw pointers equal their RVAs and both PE images execute directly at
their preferred bases.

The offline extractor verifies the full ROM and every range with SHA-256,
checks PE metadata and the closed CSI-wrapper call graph, and refuses any
occupied destination range. At runtime the monitor rechecks each complete
range with pinned FNV-1a-32 plus PE/entry anchors. FNV-1a is only accidental
change detection; the source/composite SHA-256 values are the provenance
checks.

## Reconstructed call contracts

The selected CSI path calls the original wrapper at `0xfffc04e2`:

```c
ami_csi_wrapper(0, 0, experimental_seed_byte);
```

The wrapper creates and initializes a private `0x304`-byte state, then calls
`0xfffe7000(state, state)`. Only the low byte of its third wrapper argument is
statically used. On return it deliberately exposes a non-cdecl register
tuple:

```text
EAX = state[0x2fc]
EBX = state[0x301]
ECX = little-endian dword state[0x2f8]
EDI = address of transient wrapper state
```

The assembly trampoline therefore saves and restores the coreboot stack,
callee-saved registers and EFLAGS, captures EAX/EBX/ECX/EDX/EDI, and copies
the complete state before the private stack is reused. It enters with
interrupts disabled and direction flag clear. The experimental seed remains
unknown; zero is an experiment, not a proven cold-boot value.

MINIT is called directly rather than through the much larger AMI wrapper:

```c
uint32_t minit_entry(void *policy_e0, void *workspace_2bcc);
```

The `0xe0` policy and `0x2bcc` zeroed workspace live outside the vendor stack.
Workspace offsets `0x65` and `0x69` implement MINIT's internal non-local
error return. No external callback ABI was found. A return is only reported
as a candidate when all four observations hold:

```text
EAX == 0
workspace[1] != 1
workspace[2] == 0
policy[0x0a] == 0
```

Even that result does not prove trained or usable DRAM. B06V0 intentionally
performs no memory read/write and publishes no memory map.

## Fail-closed gates

Before a vendor entry can execute, the implementation requires:

- Intel E5645 leaf-1 signature `0x000206c2` and microcode revision `0x1f`;
- BSP state, enabled LAPIC with zero high half at `0xfee00000`, SSE2, CPL0
  protected mode, paging off, expected CR0/CR4 bits, trap/interrupt/direction/
  nested-task/VM flags clear, and exact flat selectors `CS=0x08`,
  `DS=ES=SS=0x10`;
- execution on the verified coreboot CAR stack;
- an exact, recomputed CAR layout, five intact canaries and a private vendor
  stack of at least `0x6000` bytes;
- complete-range runtime fingerprints for wrapper, helper, CSI and MINIT;
- exactly one responding DDR3 SPD address among `0x50..0x57`, specifically
  `0x54`, plus the BLS4G3D1609DS1S00. geometry, CRC and conservative decoded
  timings; all 256 twice-read bytes are FNV-hashed and the operator must echo
  that exact runtime fingerprint to `vprep`;
- exact single-socket SAD identity `8086:2d81`;
- PCIEXBAR high half zero and low half either zero before `vprep` or exactly
  `0xe0000001` afterward;
- X58 MMCONFIG identity `8086:3405`, host-bridge class and revision gate;
- the measured Slow-QPI state `ff:02.1:80 = 0x030f0f03` and ratio-6 memory
  clock state `ff:03.4:50/54 = 0x0a000006/0x00000006`.

`vprep SPD_FNV` is not read-only. Before mutation it rechecks CPU mode, CAR,
canaries, all four complete code ranges and the pre-PCIEXBAR platform state.
If the prechecked PCIEXBAR value is zero, it writes high `0` and low
`0xe0000001`, verifies readback, and only then reads X58 MMCONFIG. It refuses
every other initial value.

The final static link map places the guarded scratch objects as follows:

```text
runtime metadata             0xfff84f60
trampoline call state        0xfff84fe0
guard/CSI/policy/workspace   0xfff85040..0xfff88030
private vendor stack         0xfff88030..0xfff8fff0  (0x7fc0 bytes)
final guard                  0xfff8fff0..0xfff90000
```

Repeated runtime preparation is rejected, and every public buffer accessor
revalidates the layout and canaries before returning a pointer. CSI and MINIT
each have a physical-attempt latch: a call that reaches its XIP entry cannot
be retried in the same boot, even if its return stack or a canary is invalid.
The trampoline requires CSI to return at `stack_top-12` and MINIT at
`stack_top-8`.

## Known severe outcomes

- CSI contains a QPI retrain path which can reset or end in an HLT/self-loop.
  Code running on the same CPU cannot time it out; external cold reset or
  socketed-flash recovery is required.
- The CSI wrapper returns with port `0x70 = 0x8e`, leaving NMI disabled and
  the standard CMOS index at `0x0e`. The later explicitly armed policy decoder
  preserves that NMI-disabled state. Extended CMOS uses the separate
  `0x72/0x73` interface and its selector is restored after telemetry.
- `vaccept EAX EBX ECX CSI_FNV` proves only that the operator echoed the
  observed tuple and full-state digest. No known CSI success tuple exists
  yet.
- MINIT may hang, reset, or return a value whose low word is `0xe801`.
  B06V0 does not reproduce the AMI wrapper's automatic reset loop.
- The original wrapper first tests standard CMOS diagnostic byte `0x0e`.
  B06V4's selector-controlled telemetry observed `0x6c`, which selects the
  wrapper's diagnostic-invalid path:
  policy bytes `0x01/0x04`, word `0xbc`, and setup fields `0xc3..0xdd` remain
  zero. Extended CMOS `0x8e=0x6c` is independently bounded to `0x30` for
  policy byte `0x02`; B06V4 performs that bound in CAR without writing CMOS.
- The valid-diagnostic path remains inferred only for the reference host's
  `ALT_GP_SMI_EN[6:4]=1` profile. Other selector values fail closed. The
  optional original ROM-object input at policy offset `0x46` also remains
  unreconstructed. Installation still requires the operator-supplied complete
  policy FNV and a runtime-copy readback; later edits invalidate that copy.
- MINIT may modify `policy[0x0a]`; B06V0 therefore reports separate installed
  input-policy, current-policy and complete-workspace digests.
- Any generic ROMMON hardware write permanently dirties the vendor state for
  that boot and requires a cold reset before a later vendor operation.
- POST `d2` and `d4` mean only that CSI or MINIT returned. They do not mean
  success. A persistent `d1` or `d3` is compatible with a no-return path.

## Current proof sequence

The staged B06V2/B06V3/B06V4 hardware sequence has passed passive ROMMON
entry, `vprep`, the first CSI-produced reset, the exact warm-entry I801 tuple,
a second CSI return, complete CSI/policy/workspace captures, policy install
read-back, and a direct returning MINIT call. Static correlation proves that
`workspace[0x4f]` bit 2 made B3 skip phases B4-B8; the completion marker at
`workspace[0xe79]` stayed zero. B06V5 is the exact-digest experiment that
clears only the associated policy status and bit before another separately
armed call. See
[the B06V0 command and recovery procedure](b06v0-vendor-rommon.md) and
[the B06V5 build manifest](PUBLIC_EVIDENCE.md#omitted-artifact-reference-index).

## Assessment

The MSI code is isolatable and callable under narrow laboratory gates, but it
is not a standalone or documented MRC service. Hardware has proven CSI's
reset/return boundary and a MINIT B3 early return. Even a future full-path
return would only permit a later, separately reviewed memory-map and
destructive-memory-test step; it would not itself be a first boot or an
acceptable permanent vendor path.
