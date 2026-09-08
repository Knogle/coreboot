> Public redacted historical record. Private lab addresses, access details
> and individual hardware identities have been removed or replaced with
> synthetic examples; replacement values are not measurements. Statements
> about immutable captures refer to private originals, not this derivative.
> See [evidence and omissions](PUBLIC_EVIDENCE.md) and the
> [current development guide](../../../Documentation/mainboard/msi/x58_pro_e.md).

# B06V0 vendor-assisted CAR ROMMON

Status: built and statically verified on 2026-09-01; **not flashed and not
executed on hardware**.

B06V0 is an intentionally terminal pre-DRAM laboratory image. It preserves
the B06J monitor and adds locally composed MSI CSI/MINIT code, strict platform
gates, a private guarded CAR stack, reset commands and inspectable state
buffers. Nothing invokes proprietary code automatically.

## Fixed first-test hardware

Use only this configuration:

```text
board: MSI X58 Pro-E / MS-7522 revision 3.0
flash: socketed Winbond W25Q128.V..M, 16 MiB, verified spare
CPU: Intel Xeon E5645, CPUID 000206c2, one socket
microcode required at runtime: 0000001f
DIMM: one Crucial BLS4G3D1609DS1S00., 4 GiB, 2Rx8, non-ECC
SPD topology: exact sole response at 0x54
memory policy: ratio 6
QPI: Slow Mode, observed PH_PIS 030f0f03
COM1: 0x3f8, 115200 8N1, no flow control
```

Do not begin a CSI or MINIT experiment remotely without a known external cold
power-cycle path and a verified socketed vendor-recovery chip. CSI may halt
the only running CPU permanently.

The locally composed full-chip image is:

```text
blobs-local/msi-x58-pro-e/b06v0/
  msi-x58-pro-e-b06v0-vendor-assisted-w25q128-16MiB.rom
SHA-256 ba4ec305f3b437e9325e5d557f5ec23d861f54d58ba1251f5610802980f1dc96
```

It is exactly 12 MiB of `0xff` followed by the verified 4 MiB composite. The
file is local/proprietary and must not be committed or redistributed. Program
and independently read back the complete 16 MiB chip; record the readback
hash before installing it.

## Stage 1: passive boot only

After a true AC removal, boot and capture the complete POST and serial trace.
The expected terminal monitor code is `bc`, followed by:

```text
B06V0 VENDOR-ASSISTED ROMMON in CAR; no DRAM; no automatic blob call.
```

Run only:

```text
id
resetcause
spd
vinfo
```

Required `spd` evidence is:

```text
SPD_TOPOLOGY ACKMAP=10 DDR3MAP=10 RESULT=00 EXACT_ONE_54=PASS
SPD54_TARGET_GATE=PASS FULL256_FNV1A=<eight hex digits>
```

The command reads the complete declared 256-byte SPD twice where required,
then performs one read-only type-byte transaction at each `0x50..0x57`.
Any other responder, type, timeout, CRC, geometry, part number or timing
rejects the vendor path. The monitor hashes all 256 bytes and requires the
operator to echo that observed fingerprint at `vprep`; it does not silently
equate the part number with byte-for-byte SPD identity. An SMBus timeout is
latched; remove AC power rather than retrying.

Before `vprep`, `vinfo` may report `pciexbar-state`; it must still show the
expected CPUID/microcode, CAR range, stack range, intact canaries and all
three visible full-range fields `WRAPPER`, `CSI` and `MINIT` as `01`.
`WRAPPER=01` covers both the separately pinned wrapper and helper ranges.
Stop and preserve the log after this stage. Do not infer anything about
CSI/MINIT from passive success.

## Stage 2: PCIEXBAR preparation

On a separate cold boot, repeat Stage 1, then run:

```text
unlock VENDOR
vprep FULL256_FNV1A
vinfo
```

Use the exact `FULL256_FNV1A` printed by the reviewed `spd` command. `vprep`
consumes the one-shot vendor arm. Before any PCIEXBAR write it revalidates the
32-bit CPU mode, CAR layout/canaries and all complete blob fingerprints. It
then accepts only SAD ID
`2d818086`, PCIEXBAR high `0`, and low `0` or the exact final value
`e0000001`. When low is zero it writes PCIEXBAR, verifies readback, then
checks X58 MMCONFIG ID/class/revision and the fixed Slow-QPI/ratio-6 state.
Success emits POST `d0` and `PCIEXBAR STATUS=ok`.

Stop after this step for the first B06V0 hardware evaluation. This is already
a useful result: it verifies the base image, pre-RAM microcode, complete local
blob placement, CAR guards, exact SPD topology and the X58 MMCONFIG gate.

## Stage 3: isolated CSI experiment

Only after reviewing Stage 1/2 logs, repeat them from a cold boot and choose an
explicit experimental seed. Zero is the initial neutral experiment, not a
known vendor value:

```text
unlock VENDOR
vcsi 00
```

Immediately before the call, serial reports the addresses and risks and POST
becomes `d1`. Three outcomes are possible:

- reset before return;
- permanent HLT/self-loop, leaving POST `d1`;
- return, producing POST `d2`, a register tuple, full-state FNV and canary
  status.

For a returned call, do not accept it immediately. Capture the complete state
in bounded chunks:

```text
vinfo
vstate 000 080
vstate 080 080
vstate 100 080
vstate 180 080
vstate 200 080
vstate 280 080
vstate 300 004
```

Record EAX/EBX/ECX/EDX/EDI/EFLAGS/VESP and `CSI_FNV`. The wrapper may have
left port `0x70=0x8e`, hence NMI disabled. The monitor intentionally does not
guess how to restore that state.

Only an explicitly reviewed result can be marked accepted, using the exact
three returned fields and state digest:

```text
unlock VENDOR
vaccept EAX EBX ECX CSI_FNV
```

This proves only that the operator echoed the observed result. There is no
known CSI success tuple yet. A mismatched value returns
`csi-result-mismatch` and consumes the ROMMON arm.

The trampoline also requires the exact cdecl return stack (`top-12` for CSI).
A canary or stack mismatch invalidates the result and the physical call may
not be retried in the same boot even though control returned.

## Stage 4: policy inspection and MINIT

Do not perform this stage merely because CSI returned. First archive and
analyze its complete state. `vpolicy reset` also requires the post-CSI
platform probe still to match the pinned Slow-QPI/ratio-6 state. If CSI changed
that tuple, stop and revise the hypothesis instead of bypassing the gate. The
locally reconstructed policy contains fields inferred from a three-DIMM
reference setup.

After explicit CSI acceptance:

```text
unlock VENDOR
vpolicy reset
vpolicy dump 00 e0
```

The generated template decodes current CMOS and labels itself `INFERRED`.
Fields `0x04` and `0xbc..0xbd` are not proven for the one-DIMM target. To
change one byte, each write requires a new arm:

```text
unlock VENDOR
vpolicy set OFFSET BYTE
```

After the complete `0xe0` bytes and FNV have been independently reviewed:

```text
unlock VENDOR
vpolicy install POLICY_FNV
```

The command requires the exact printed full-policy digest and verifies the
installed runtime copy by hashing it again. Any later `vpolicy set` or
`vpolicy reset` first invalidates the installed copy. Installation still does
not call MINIT. The final call is another distinct one-shot action:

```text
unlock VENDOR
vminit
```

POST `d3` means the call began. POST `d4` means only that it returned. A
return is labeled candidate only when EAX is zero, workspace byte 1 is not
one, workspace byte 2 is zero, and policy byte `0x0a` is zero. Even then,
B06V0 does not read DRAM or leave CAR. Dump the workspace with repeated
`vwork OFF COUNT` commands, where `COUNT` is at most `0xe0`, and preserve all
output. `vinfo` reports separate installed-policy, current-policy and complete
workspace FNV values because MINIT may modify policy byte `0x0a`. The return
stack must be exactly `top-8`. Low word `e801` is logged as a reset request
but B06V0 does not enter the AMI reset loop.

`vpolicy reset` keeps CMOS/NMI port `0x70` at the CSI-returned value `0x8e`
while decoding policy fields and logs that state. Any generic `WRITE` command
sets `ROMMON_DIRTY=01`, invalidates the local SPD/PCIEXBAR/policy gates and
blocks every later vendor mutation until a cold reset.

## Reset commands

The monitor provides separately armed CF9 requests:

```text
unlock RESET
reset init

unlock RESET
reset warm

unlock RESET
reset full
```

The exact sequences are `00->04`, `02->06`, and `0a->0e`; POST codes are
`cc`, `cd`, and `ce`. UART is drained, interrupts are disabled, and a failed
reset falls into HLT. These commands do not flush CAR and `full` is not an
AC-off cycle. They are unavailable if CSI/MINIT no longer returns control.
Use external power control in that case.

`lock` cancels all pending WRITE, RESET and VENDOR arms.

## B06V0 POST meanings

| POST | Meaning |
|---:|---|
| `bc` | CAR ROMMON prompt ready |
| `bd` | command dispatch |
| `be` | UART receive fault |
| `bf` | command, lock or argument error |
| `cc` | CF9 INIT request issued |
| `cd` | CF9 warm request issued |
| `ce` | CF9 full request issued |
| `d0` | PCIEXBAR/X58/platform probe passed |
| `d1` | CSI entered; may never return |
| `d2` | CSI returned; not necessarily successful |
| `d3` | MINIT entered; may never return |
| `d4` | MINIT returned; not necessarily successful |
| `df` | B06V0 fail-closed gate/status error |

Inherited SPD progress codes `54..62` and `a0..b7` remain visible during the
`spd` command. `d2`/`d4` must never be recorded as memory-init success by
themselves. A successful call return keeps `d2`/`d4` even if the following
read-only probe observes that CSI/MINIT changed a pre-call platform value;
the exact probe status remains in serial. Canary, pointer or return-stack
validation failures deliberately replace the return code with `df`.

## Per-run evidence

Every run must record the complete fields required by `AGENTS.md`, plus:

- flashed 16 MiB file hash and independent programmer-readback hash;
- true cold/warm/CF9 provenance and `resetcause` output;
- full POST sequence and complete serial-log SHA-256;
- `vinfo` before and after every state-changing step;
- exact SPD population, physical slot and full SPD fingerprint;
- CSI seed, tuple, state dump/hash and whether control returned;
- port `0x70`/NMI observation after a returned CSI call;
- complete policy bytes, CMOS/strap context, every edit and digest;
- MINIT tuple, all four candidate gates and complete workspace digest;
- recovery action and confirmation that no ordinary DRAM or SPI write was
  performed by the monitor.

Ten cold boots and ten relevant warm resets remain required before any
milestone is considered stable. No B06V0 hardware success is currently
claimed.
